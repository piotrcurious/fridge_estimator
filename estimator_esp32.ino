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

#define VCC 3.3
#define R0 10000
#define B 3950
#define T0 298.15
#define T1 273.15

#define WINDOW_SIZE 300
float temp_window[WINDOW_SIZE];
unsigned long time_window[WINDOW_SIZE];
int window_idx = 0;
bool window_full = false;

// --- Adaptive Parameters ---
float ambient_est = 25.0;
float alpha_sys = 1.0e-5;
float food_temp_est = 2.0;
float temp_threshold = 4.0;
float temp_time = 3600;

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

float filtered_temp = -999;
float debug_active_rate = 0;
float debug_confidence = 0;
float debug_alpha = 0;

bool light;
bool light_prev;
unsigned long door_open_start = 0;
unsigned long last_door_closed = 0;
bool comp;
bool comp_prev;
unsigned long last_comp_off = 0;
unsigned long last_comp_on = 0;

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

float calculateRate(int min_samples) {
  int count = window_full ? WINDOW_SIZE : window_idx;
  if (count < min_samples) return 0;
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
  float temp = getMedian(raw_temp);
  if (filtered_temp < -100) {
      filtered_temp = temp;
      food_temp_est = temp;
      for(int i=0; i<MEDIAN_SIZE; i++) median_buffer[i] = temp;
  }
  filtered_temp = filtered_temp * 0.9 + temp * 0.1;

  // Integrated Food Observer
  float alpha_coup = alpha_sys * 5.0;
  if (comp) alpha_coup *= 2.0;
  food_temp_est += (filtered_temp - food_temp_est) * alpha_coup * dt;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light && !light_prev) {
      door_open_start = millis();
      window_idx = 0; window_full = false;
  }

  if (light) {
      temp_window[window_idx] = filtered_temp;
      time_window[window_idx] = current_m;
      window_idx++; if (window_idx >= WINDOW_SIZE) { window_idx = 0; window_full = true; }

      if (millis() - door_open_start > 5000) {
          float door_rate = calculateRate(10);
          if (door_rate > 0.001) {
              float amb_proj = filtered_temp + door_rate / 0.0033;
              if (amb_proj > 10.0 && amb_proj < 45.0) {
                  ambient_est = ambient_est * 0.95 + amb_proj * 0.05;
              }
          }
      }
      if (filtered_temp > ambient_est) ambient_est = filtered_temp;
  }

  if (!light && light_prev) {
      last_door_closed = millis();
      window_idx = 0; window_full = false;
  }
  light_prev = light;

  comp = digitalRead(COMP_PIN);
  if (comp && !comp_prev) {
      last_comp_on = millis();
      window_idx = 0; window_full = false;
  }
  if (!comp && comp_prev) {
      last_comp_off = millis();
      window_idx = 0; window_full = false;
  }
  comp_prev = comp;

  // Stable Warming Phase:
  // Post-door recovery: 6 mins, Post-compressor recovery: 10 mins
  // This ensures air/food coupling has stabilized before we measure alpha.
  bool post_door_recovery = (millis() - last_door_closed < 360000);
  bool post_comp_recovery = (millis() - last_comp_off < 600000);
  bool stable_warm = (!light && !post_door_recovery && !comp && !post_comp_recovery);

  if (stable_warm) {
    temp_window[window_idx] = filtered_temp;
    time_window[window_idx] = current_m;
    window_idx++;
    if (window_idx >= WINDOW_SIZE) { window_idx = 0; window_full = true; }

    float rate = calculateRate(60);
    if (rate > 1.0e-5) {
        float delta_T = ambient_est - filtered_temp;
        if (delta_T > 2.0) {
            float alpha_instant = rate / delta_T;
            if (alpha_instant > 1.0e-6 && alpha_instant < 5.0e-5) {
                // Slower, more stable learning
                alpha_sys = alpha_sys * 0.99 + alpha_instant * 0.01;
            }
        }
    }
  }

  // Prediction Logic
  float raw_time = 0;
  if (food_temp_est < temp_threshold) {
      if (ambient_est > temp_threshold + 0.1) {
          float ratio = (ambient_est - temp_threshold) / (ambient_est - food_temp_est);
          if (ratio > 0.001 && ratio < 1.0) {
              raw_time = - (1.0 / alpha_sys) * log(ratio);
          } else {
              raw_time = (temp_threshold - food_temp_est) / (alpha_sys * (ambient_est - food_temp_est) + 1e-10);
          }
      } else {
          raw_time = 43200;
      }
  }

  temp_time = temp_time * 0.99 + raw_time * 0.01;
  if (temp_time < 0) temp_time = 0;
  if (temp_time > 43200) temp_time = 43200;

  debug_active_rate = ambient_est;
  debug_confidence = food_temp_est;
  debug_alpha = alpha_sys;

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
  display.println("Phys-Adaptive v10");
  display.setCursor(0,12);
  display.print("Asys: "); display.print(alpha_sys * 1e6, 2);
  display.setCursor(0,24);
  display.print("Amb: "); display.print(ambient_est, 1);
  display.setCursor(0,36);
  display.print("Food: "); display.print(food_temp_est, 1);
  display.setCursor(0,48);
  display.print("Rem: "); display.print(temp_time, 0); display.print(" s");
  display.display();
}
