// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

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
#define WINDOW_SIZE 300
float temp_window[WINDOW_SIZE];
unsigned long time_window[WINDOW_SIZE];
int window_idx = 0;
bool window_full = false;

// Adaptive learning parameters
float avg_rate_history = 0.00018;
float cycle_rate_acc = 0;
int cycle_rate_count = 0;

// Median Filter
#define MEDIAN_SIZE 5
float median_buffer[MEDIAN_SIZE];
int median_idx = 0;

float getMedian(float newVal) {
  median_buffer[median_idx] = newVal;
  median_idx = (median_idx + 1) % MEDIAN_SIZE;
  float sorted[MEDIAN_SIZE];
  for(int i=0; i<MEDIAN_SIZE; i++) sorted[i] = median_buffer[i];
  for(int i=0; i<MEDIAN_SIZE-1; i++) {
    for(int j=0; j<MEDIAN_SIZE-i-1; j++) {
      if(sorted[j] > sorted[j+1]) {
        float temp = sorted[j];
        sorted[j] = sorted[j+1];
        sorted[j+1] = temp;
      }
    }
  }
  return sorted[MEDIAN_SIZE/2];
}

// State variables
float temp;
float filtered_temp = -999;
float calc_air_temp = -999;
float food_temp_est = -999;
float temp_rate = 0;
float smoothed_rate = 0;
float temp_threshold = 4.0;
float temp_time = 3600;

// Internal state for logging
float debug_active_rate = 0;
float debug_confidence = 0;
float debug_alpha = 0;

bool light;
bool light_prev;
unsigned long light_start;
unsigned long light_cycle_total = 0;
unsigned long last_door_closed = 0;
bool comp;
bool comp_prev;

float readTemp();
void updateDisplay();

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(COMP_PIN, INPUT);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { for(;;); }
  display.clearDisplay();
  display.display();
  for(int i=0; i<WINDOW_SIZE; i++) { temp_window[i] = 0; time_window[i] = 0; }
}

float calculateRate() {
  int count = window_full ? WINDOW_SIZE : window_idx;
  if (count < 100) return 0;
  double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
  int oldest = window_full ? window_idx : 0;
  double start_time = (double)time_window[oldest] / 1000.0;
  for (int i = 0; i < count; i++) {
    int idx = (oldest + i) % WINDOW_SIZE;
    double x = (double)time_window[idx] / 1000.0 - start_time;
    double y = temp_window[idx];
    sum_x += x; sum_y += y; sum_xy += x * y; sum_x2 += x * x;
  }
  double denom = (count * sum_x2 - sum_x * sum_x);
  if (abs(denom) < 0.00001) return 0;
  return (float)((count * sum_xy - sum_x * sum_y) / denom);
}

unsigned long last_millis = 0;
void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 1.0 && last_millis > 0) return;

  float raw_temp = readTemp();
  temp = getMedian(raw_temp);
  if (filtered_temp < -100) {
      filtered_temp = temp;
      calc_air_temp = temp;
      food_temp_est = temp;
      for(int i=0; i<MEDIAN_SIZE; i++) median_buffer[i] = temp;
  }
  filtered_temp = filtered_temp * 0.8 + temp * 0.2;
  // Heavily smoothed air temp for the log formula to prevent jumpiness
  calc_air_temp = calc_air_temp * 0.98 + temp * 0.02;

  float food_alpha = 0.00005;
  food_temp_est = food_temp_est * (1.0 - food_alpha) + temp * food_alpha;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light != light_prev) {
    if (light) light_start = millis();
    else { light_cycle_total += (millis() - light_start); last_door_closed = millis(); }
    light_prev = light;
  }
  comp = digitalRead(COMP_PIN);
  if (comp && !comp_prev) {
      if (cycle_rate_count > 100) {
          float mean_cycle_rate = cycle_rate_acc / (float)cycle_rate_count;
          avg_rate_history = avg_rate_history * 0.7 + mean_cycle_rate * 0.3;
      }
      light_cycle_total = 0;
      window_idx = 0; window_full = false;
      cycle_rate_acc = 0; cycle_rate_count = 0;
      smoothed_rate = 0;
  }
  comp_prev = comp;

  bool door_recently_closed = (millis() - last_door_closed < 300000);
  bool stable = (!light && !door_recently_closed);

  if (stable && !comp) {
    temp_window[window_idx] = food_temp_est;
    time_window[window_idx] = current_m;
    window_idx++;
    if (window_idx >= WINDOW_SIZE) { window_idx = 0; window_full = true; }

    float new_rate = calculateRate();
    if (new_rate > 1e-9) {
        if (smoothed_rate == 0) smoothed_rate = new_rate;
        else smoothed_rate = smoothed_rate * 0.98 + new_rate * 0.02;
        cycle_rate_acc += smoothed_rate;
        cycle_rate_count++;
    }
  }

  // STABLE LOGARITHMIC ESTIMATION
  int sample_count = window_full ? WINDOW_SIZE : window_idx;
  debug_confidence = (float)sample_count / (float)WINDOW_SIZE;
  debug_active_rate = (smoothed_rate > 1e-9) ? ((avg_rate_history * (1.0 - debug_confidence)) + (smoothed_rate * debug_confidence)) : avg_rate_history;
  
  float door_impact = 1.0 + (float)light_cycle_total / 10000.0;
  debug_alpha = debug_active_rate / (calc_air_temp - food_temp_est + 0.1);

  // Dynamic alpha bounding
  float min_alpha = 0.00002;
  float max_alpha = 0.0005;
  if (debug_alpha < min_alpha) debug_alpha = min_alpha;
  if (debug_alpha > max_alpha) debug_alpha = max_alpha;

  float raw_time;
  if (food_temp_est < temp_threshold && calc_air_temp > temp_threshold + 0.5) {
      float ratio = (calc_air_temp - temp_threshold) / (calc_air_temp - food_temp_est);
      raw_time = - (1.0 / (debug_alpha * door_impact)) * log(ratio);
  } else if (food_temp_est >= temp_threshold) {
      raw_time = 0;
  } else {
      raw_time = (temp_threshold - food_temp_est) / (debug_active_rate * door_impact);
  }

  // Final Smoothing on output
  temp_time = temp_time * 0.9 + raw_time * 0.1;

  if (temp_time < 0) temp_time = 0;
  if (temp_time > 25000) temp_time = 25000;

  last_millis = current_m;
  updateDisplay();
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
  display.print("Rate: "); display.print(debug_active_rate * 1000000.0, 1);
  display.setCursor(0,30);
  display.print("Est: "); display.print(temp_time, 0); display.print(" s");
  display.display();
}
