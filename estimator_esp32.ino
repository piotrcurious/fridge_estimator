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
float ambient_est = 22.0;
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
unsigned long last_door_closed = 0;
bool comp;
bool comp_prev;
unsigned long last_comp_off = 0;

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
  if (count < 60) return 0;
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
  filtered_temp = filtered_temp * 0.95 + temp * 0.05;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light) {
      if (filtered_temp > ambient_est) {
          ambient_est = ambient_est * 0.95 + filtered_temp * 0.05;
      }
  }
  if (!light && light_prev) last_door_closed = millis();
  light_prev = light;

  comp = digitalRead(COMP_PIN);
  if (!comp && comp_prev) last_comp_off = millis();
  comp_prev = comp;

  bool stable = (!light && (millis() - last_door_closed > 900000) && !comp && (millis() - last_comp_off > 900000));

  if (stable) {
    temp_window[window_idx] = filtered_temp;
    time_window[window_idx] = current_m;
    window_idx++;
    if (window_idx >= WINDOW_SIZE) { window_idx = 0; window_full = true; }

    float air_rate = calculateRate();
    if (air_rate > 1.0e-8) {
        float alpha_trust = 0.0005;
        float amb_trust = 0.0005;

        float drive = ambient_est - filtered_temp;
        if (drive > 0.5) {
            float instant_alpha = air_rate / drive;
            if (instant_alpha > 1e-7 && instant_alpha < 5e-4) {
                alpha_sys = alpha_sys * (1.0 - alpha_trust) + instant_alpha * alpha_trust;
            }
        }

        float instant_amb = filtered_temp + air_rate / alpha_sys;
        if (instant_amb > 5.0 && instant_amb < 45.0) {
            ambient_est = ambient_est * (1.0 - amb_trust) + instant_amb * amb_trust;
        }
    }
  } else if (comp || light) {
      window_idx = 0; window_full = false;
  }

  // Food Temp Proxy: accounts for lag in the warming phase
  food_temp_est = filtered_temp - 0.25 * (ambient_est - filtered_temp);
  
  float raw_time = 0;
  if (food_temp_est < temp_threshold) {
      if (ambient_est > temp_threshold + 0.2) {
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

  temp_time = temp_time * 0.998 + raw_time * 0.002;
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
  display.println("Phys-Adaptive Final");
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
