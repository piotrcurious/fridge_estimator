// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

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
float ambient_est = 25.0;
float learned_alpha = 0.00005;
float temp_target = 4.0;

bool light;
bool light_prev;
unsigned long light_start;
unsigned long door_closed_time = 0;
float door_penalty_accum = 0;
bool comp;
bool comp_prev;
float time_est = 3600;

float cooling_start_temp = 0;
unsigned long cooling_start_time = 0;

// Internal state for logging
float debug_active_rate = 0;
float debug_confidence = 0;
float debug_alpha = 0;

float readTemp();
void updateDisplay();

void setup() {
  Serial.begin(115200);
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(COMP_PIN, INPUT);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { for(;;); }
  display.clearDisplay();
  display.display();
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
  calc_air_temp = calc_air_temp * 0.98 + temp * 0.02;

  food_temp_est = food_temp_est + (filtered_temp - food_temp_est) * learned_alpha * dt;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light != light_prev) {
    if (light) light_start = millis();
    else {
      unsigned long duration = millis() - light_start;
      door_penalty_accum += 0.1 * ((float)duration / 1000.0) * (25.0 - filtered_temp) / 25.0;
      door_closed_time = millis();
    }
    light_prev = light;
  }

  comp = digitalRead(COMP_PIN);
  if (comp && !comp_prev) {
      cooling_start_temp = filtered_temp;
      cooling_start_time = millis();
      door_penalty_accum = 0;
  }
  if (!comp && comp_prev) {
      unsigned long dur = millis() - cooling_start_time;
      if (dur > 60000) {
          float cooling_slope = (filtered_temp - cooling_start_temp) / (dur / 1000.0);
          float base_slope = -0.005;
          float ratio = cooling_slope / base_slope;
          if (ratio < 0.2) ratio = 0.2;
          if (ratio > 5.0) ratio = 5.0;
          learned_alpha = 0.00005 * ratio;
      }
  }
  comp_prev = comp;

  // PHYS-LAG MODEL
  float decay = exp(-(float)(millis() - door_closed_time) / 1200000.0);
  float active_penalty = door_penalty_accum * decay;
  float effective_food = food_temp_est + active_penalty;

  // Estimate Ambient from warming rate in stable state
  if (!comp && !light && (millis() - door_closed_time > 600000)) {
     // Approx air rate from filtered temp change
     static float last_f_temp = 0;
     static unsigned long last_f_time = 0;
     if (last_f_time > 0) {
         float rate = (filtered_temp - last_f_temp) / ((millis() - last_f_time) / 1000.0);
         if (rate > 0) {
             float inst_amb = filtered_temp + rate * 1200.0;
             ambient_est = ambient_est * 0.999 + inst_amb * 0.001;
         }
     }
     last_f_temp = filtered_temp;
     last_f_time = millis();
  }

  float raw_time;
  float drive_temp = (ambient_est > calc_air_temp) ? ambient_est : calc_air_temp + 2.0;

  if (effective_food < temp_target && drive_temp > temp_target + 0.2) {
      float ratio = (drive_temp - temp_target) / (drive_temp - effective_food);
      if (ratio <= 0) raw_time = (temp_target - effective_food) / (learned_alpha * (drive_temp - effective_food) + 1e-9);
      else raw_time = - (1.0 / learned_alpha) * log(ratio);
  } else if (effective_food >= temp_target) {
      raw_time = 0;
  } else {
      raw_time = (temp_target - effective_food) / (learned_alpha * (drive_temp - effective_food) + 1e-9);
  }

  time_est = time_est * 0.95 + raw_time * 0.05;

  debug_alpha = learned_alpha;
  debug_confidence = ambient_est; // Reusing for logging
  debug_active_rate = effective_food;

  if (time_est < 0) time_est = 0;
  if (time_est > 30000) time_est = 30000;

  last_millis = current_m;
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
  display.println("F-Estimator V2 (Lag)");
  display.setCursor(0,15);
  display.print("Food: "); display.print(debug_active_rate, 1);
  display.setCursor(0,30);
  display.print("Amb: "); display.print(ambient_est, 1);
  display.setCursor(0,45);
  display.print("Est: "); display.print(time_est, 0); display.print(" s");
  display.display();
}
