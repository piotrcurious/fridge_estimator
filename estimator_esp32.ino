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
bool ambient_acquired = false;
float alpha_sys = 1.0e-5;
bool alpha_acquired = false;
float food_temp_est = -999;
float temp_threshold = 4.0;
float temp_time = 3600;

// Filters
#define MEDIAN_SIZE 11
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
unsigned long last_door_closed = 0;
bool comp;
bool comp_prev;
unsigned long last_comp_off = 0;
unsigned long last_comp_on = 0;
float comp_start_temp = 0;

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
  for(int i=0; i<MEDIAN_SIZE; i++) median_buffer[i] = 2.0;
}

float calculateRate(int samples) {
  int count = window_full ? WINDOW_SIZE : window_idx;
  if (count < samples) return 0;
  double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
  int start_idx = (window_idx - samples + WINDOW_SIZE) % WINDOW_SIZE;
  double t0 = (double)time_window[start_idx] / 1000.0;
  for (int i = 0; i < samples; i++) {
    int idx = (start_idx + i) % WINDOW_SIZE;
    double x = (double)time_window[idx] / 1000.0 - t0;
    double y = temp_window[idx];
    sum_x += x; sum_y += y; sum_xy += x * y; sum_x2 += x * x;
  }
  double denom = (samples * sum_x2 - sum_x * sum_x);
  if (abs(denom) < 0.00001) return 0;
  return (float)((samples * sum_xy - sum_x * sum_y) / denom);
}

unsigned long last_millis = 0;
void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 0.9 && last_millis > 0) return;

  float raw_temp = readTemp();
  float temp = getMedian(raw_temp);
  if (filtered_temp < -100) {
      filtered_temp = temp;
      food_temp_est = temp;
  }
  filtered_temp = filtered_temp * 0.9 + temp * 0.1;

  // Food Observer
  float alpha_coup = alpha_sys * 1.5;
  if (comp) alpha_coup *= 2.0;
  food_temp_est += (filtered_temp - food_temp_est) * alpha_coup * dt;

  light = digitalRead(LIGHT_PIN) == LOW;
  comp = digitalRead(COMP_PIN);

  // Data logging
  temp_window[window_idx] = filtered_temp;
  time_window[window_idx] = current_m;
  window_idx++; if (window_idx >= WINDOW_SIZE) { window_idx = 0; window_full = true; }

  // Estimation
  if (light) {
      float rate = calculateRate(15);
      if (rate > 0.01) {
          float amb_proj = filtered_temp + rate * 300.0;
          if (amb_proj > 10.0 && amb_proj < 50.0) {
              float k = ambient_acquired ? 0.01 : 0.4;
              ambient_est = ambient_est * (1.0 - k) + amb_proj * k;
              ambient_acquired = true;
          }
      }
  } else if (!comp) {
      // Dynamic Stability Detection
      // We look at the 100-sample rate to see if the air temp is truly warming (stable)
      float rate_long = calculateRate(100);
      float rate_short = calculateRate(30);

      // If the short-term and long-term rates are consistent and positive, we are in a stable warming phase
      bool stable_warming = (rate_long > 1e-6 && abs(rate_long - rate_short) < 0.1 * rate_long);
      bool recovery = (millis() - last_door_closed < 300000 || millis() - last_comp_off < 300000);

      if (stable_warming && !recovery) {
          float delta_T = ambient_est - filtered_temp;
          if (delta_T > 2.0) {
              float alpha_inst = rate_long / delta_T;
              if (alpha_inst > 1e-7 && alpha_inst < 1e-4) {
                  float k = alpha_acquired ? 0.005 : 0.1;
                  alpha_sys = alpha_sys * (1.0 - k) + alpha_inst * k;
                  alpha_acquired = true;
              }
          }
      }
  }

  if (!light && light_prev) last_door_closed = millis();
  if (!comp && comp_prev) last_comp_off = millis();
  light_prev = light;
  comp_prev = comp;

  // Prediction
  float raw_time = 0;
  if (food_temp_est < temp_threshold) {
      float safe_amb = (ambient_est < temp_threshold + 0.5) ? temp_threshold + 0.5 : ambient_est;
      float ratio = (safe_amb - temp_threshold) / (safe_amb - food_temp_est);
      if (ratio > 0.01 && ratio < 0.99) {
          raw_time = - (1.0 / alpha_sys) * log(ratio);
      } else {
          raw_time = (temp_threshold - food_temp_est) / (alpha_sys * (safe_amb - food_temp_est) + 1e-10);
      }
  }

  float k = 0.05;
  if (light || comp || (millis() - last_door_closed < 300000) || (millis() - last_comp_off < 300000)) k = 0.002;
  temp_time = temp_time * (1.0 - k) + raw_time * k;

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
  display.println("Phys-Adaptive v19");
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
