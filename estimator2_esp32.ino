// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define TEMP_PIN 34
#define LIGHT_PIN 2
#define COMP_PIN 4

// Constants for temperature sensor
#define VCC 3.3
#define R0 10000
#define B 3950
#define T0 298.15

// Sliding window for rate estimation
#define WINDOW_SIZE 60
float temp_window[WINDOW_SIZE];
unsigned long time_window[WINDOW_SIZE];
int window_idx = 0;
bool window_full = false;

float temp;
float filtered_temp = -999;
float food_temp_est = -999;
float temp_prev;
float temp_rate = 0;
float temp_target = 4.0;

bool light;
bool light_prev;
unsigned long light_start;
unsigned long door_closed_time = 0;
float door_penalty_accum = 0;

bool comp;
bool comp_prev;

float time_est = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(COMP_PIN, INPUT);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.display();
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

  float food_alpha = light ? 0.005 : 0.02;
  food_temp_est = food_temp_est * (1.0 - food_alpha) + temp * food_alpha;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light != light_prev) {
    if (light) {
      light_start = millis();
    } else {
      unsigned long duration = millis() - light_start;
      door_penalty_accum += 0.09 * sqrt((float)duration / 1000.0);
      door_closed_time = millis();
    }
    light_prev = light;
  }

  comp = digitalRead(COMP_PIN);
  if (comp && !comp_prev) {
    door_penalty_accum = 0;
  }
  comp_prev = comp;

  // Update sliding window
  bool door_recently_closed = (millis() - door_closed_time < 45000);
  bool stable = (!light && !door_recently_closed);

  if (stable && !comp) {
    temp_window[window_idx] = filtered_temp;
    time_window[window_idx] = current_m;
    window_idx++;
    if (window_idx >= WINDOW_SIZE) {
      window_idx = 0;
      window_full = true;
    }

    int count = window_full ? WINDOW_SIZE : window_idx;
    if (count > 10) {
      temp_rate = calculateRate();
    }
  } else {
    window_idx = 0;
    window_full = false;
    if (comp) {
        temp_rate = 0;
    }
  }
  
  last_millis = current_m;

  // Estimation
  if (temp_rate > 0.0001) {
    float decay = exp(-(float)(millis() - door_closed_time) / 300000.0);
    float active_penalty = door_penalty_accum * decay;

    float effective_dist = (temp_target - food_temp_est) - active_penalty;
    if (effective_dist < 0) effective_dist = 0;
    
    time_est = effective_dist / temp_rate;
  } else {
    if (comp) time_est = 3600;
  }

  if (time_est < 0) time_est = 0;
  if (time_est > 3600) time_est = 3600;

  updateDisplay();
}

float readTemp() {
  int adc = analogRead(TEMP_PIN);
  if (adc >= 4095) return 200.0;
  float vout = adc * VCC / 4095.0;
  float r = R0 * vout / (VCC - vout);
  return 1.0 / (1.0 / 298.15 + log(r / R0) / B) - 273.15;
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("F-Estimator V2 (Phys)");
  display.setCursor(0,15);
  display.print("T: "); display.print(filtered_temp, 1);
  display.setCursor(0,30);
  display.print("Est: "); display.print(time_est, 0); display.print(" s");
  display.display();
}
