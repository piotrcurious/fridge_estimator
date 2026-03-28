// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR   0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define TEMP_PIN 34
#define LIGHT_PIN 2
#define COMP_PIN 4

// Constants for temperature conversion
#define VCC 3.3
#define R0 10000
#define B 3950
#define T0 298.15
#define T1 273.15

// Sliding window for rate estimation
#define WINDOW_SIZE 60
float temp_window[WINDOW_SIZE];
unsigned long time_window[WINDOW_SIZE];
int window_idx = 0;
bool window_full = false;

// Adaptive learning parameters
float log_corr = 1.0;
#define NONLIN_CORR 0.15

// State variables
float temp;
float filtered_temp = -999;
float food_temp_est = -999;
float temp_rate = 0;
float temp_threshold = 4.0;
float temp_time = 0;

bool light;
bool light_prev;
unsigned long light_start;
unsigned long light_total = 0;
unsigned long last_door_closed = 0;

bool comp;

// Prototypes for mock environment
float readTemp();
void updateDisplay();
bool last_threshold_time_valid(unsigned long t);

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(COMP_PIN, INPUT);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();

  for(int i=0; i<WINDOW_SIZE; i++) {
    temp_window[i] = 0;
    time_window[i] = 0;
  }
}

float calculateRate() {
  if (!window_full && window_idx < 2) return 0;
  int count = window_full ? WINDOW_SIZE : window_idx;

  float sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
  float start_time = (float)time_window[0] / 1000.0;

  for (int i = 0; i < count; i++) {
    float x = (float)time_window[i] / 1000.0 - start_time;
    float y = temp_window[i];
    sum_x += x;
    sum_y += y;
    sum_xy += x * y;
    sum_x2 += x * x;
  }

  float denom = (count * sum_x2 - sum_x * sum_x);
  if (abs(denom) < 0.0001) return 0;

  return (count * sum_xy - sum_x * sum_y) / denom;
}

unsigned long last_millis = 0;

void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 1.0 && last_millis > 0) return;

  temp = readTemp();
  if (filtered_temp < -100) {
      filtered_temp = temp;
      food_temp_est = temp;
  }
  filtered_temp = filtered_temp * 0.9 + temp * 0.1;

  // Food temp tracks very slowly to ignore air spikes
  float food_alpha = light ? 0.005 : 0.02;
  food_temp_est = food_temp_est * (1.0 - food_alpha) + temp * food_alpha;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light != light_prev) {
    if (light) {
      light_start = millis();
    } else {
      light_total += (millis() - light_start);
      last_door_closed = millis();
    }
    light_prev = light;
  }

  comp = digitalRead(COMP_PIN);

  // Update sliding window
  bool door_recently_closed = (millis() - last_door_closed < 45000);
  bool stable = (!light && !door_recently_closed);

  if (stable && !comp) {
    temp_window[window_idx] = filtered_temp;
    time_window[window_idx] = current_m;
    window_idx++;
    if (window_idx >= WINDOW_SIZE) {
      window_idx = 0;
      window_full = true;
    }

    // Only update rate if we have enough new data to overcome the "reset"
    int count = window_full ? WINDOW_SIZE : window_idx;
    if (count > 10) {
      temp_rate = calculateRate();
    }
  } else {
    // If door open or compressor on, we reset window
    // But we don't necessarily zero the rate immediately if it was warming
    window_idx = 0;
    window_full = false;

    if (comp) {
        temp_rate = 0;
    } else if (light) {
        // Door is open, air is spiking. The last temp_rate is likely still valid for the food.
        // We hold it.
    }
  }

  last_millis = current_m;
  
  // Estimation
  if (temp_rate > 0.00005) {
    float base_time = (temp_threshold - food_temp_est) / temp_rate;
    float door_penalty_sec = sqrt((float)light_total / 1000.0) * 150.0;
    temp_time = (base_time - door_penalty_sec) * log_corr;
  } else {
    // During cooling or very stable warming, we hold or stay at max
    if (comp) temp_time = 3600;
  }
  
  if (temp_time < 0) temp_time = 0;
  if (temp_time > 3600) temp_time = 3600;

  // Adaptive Learning
  static float last_est_capture = 0;
  static unsigned long capture_time = 0;
  static bool captured_this_cycle = false;
  
  // Capture when we are 0.7 degrees away - middle of the warming curve
  if (filtered_temp > (temp_threshold - 0.8) && filtered_temp < (temp_threshold - 0.6) && !captured_this_cycle && !comp && stable) {
    last_est_capture = temp_time;
    capture_time = millis();
    captured_this_cycle = true;
  }

  static unsigned long last_threshold_event = 0;
  if (filtered_temp >= temp_threshold && (millis() - last_threshold_event > 120000)) {
    if (last_threshold_time_valid(last_threshold_event) && captured_this_cycle) {
      float actual_remaining = (float)(millis() - capture_time) / 1000.0;
      if (actual_remaining > 30.0 && last_est_capture > 30.0) {
        float error_ratio = actual_remaining / last_est_capture;
        // Stronger correction if error is large
        float weight = (abs(error_ratio - 1.0) > 0.3) ? 0.2 : 0.1;
        log_corr = log_corr * ((1.0 - weight) + weight * error_ratio);
        log_corr = constrain(log_corr, 0.4, 3.0);
      }
    }
    last_threshold_event = millis();
    light_total = 0;
    captured_this_cycle = false;
  }
  
  updateDisplay();
}

bool last_threshold_time_valid(unsigned long t) {
  return t > 0;
}

float readTemp() {
  int adc = analogRead(TEMP_PIN);
  if (adc >= 4095) return 200.0;
  float vout = (float)adc / 4095.0 * VCC;
  float r = R0 * vout / (VCC - vout);
  return 1.0 / (1.0 / T0 + log(r / R0) / B) - T1;
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("F-Estimator V1 (Adap)");
  display.setCursor(0,15);
  display.print("T: "); display.print(filtered_temp, 2);
  display.print(" R: "); display.print(temp_rate*3600, 2);
  display.setCursor(0,30);
  display.print("Est: "); display.print(temp_time, 0); display.print(" s");
  display.display();
}
