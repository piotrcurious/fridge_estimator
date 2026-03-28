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

// Adaptive learning parameters
float log_corr = 1.0;
#define NONLIN_CORR 0.15

// State variables
float temp;
float temp_prev;
float temp_rate = 0;
float temp_threshold = 4.0;
float temp_time = 0;

bool light;
bool light_prev;
unsigned long light_start;
unsigned long light_total = 0;
unsigned long last_door_closed = 0;

bool comp;

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
}

unsigned long last_millis = 0;
float filtered_temp = -999;

void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 1.0 && last_millis > 0) return;

  temp = readTemp();
  if (filtered_temp < -100) filtered_temp = temp;
  filtered_temp = filtered_temp * 0.95 + temp * 0.05;

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

  if (last_millis > 0) {
    float instant_rate = (filtered_temp - temp_prev) / dt;
    comp = digitalRead(COMP_PIN);
    bool stable = (!light && (millis() - last_door_closed > 45000));

    if (stable && !comp) {
      temp_rate = temp_rate * 0.99 + instant_rate * 0.01;
    } else if (comp) {
      temp_rate = temp_rate * 0.99;
    }

    temp_prev = filtered_temp;
  } else {
    temp_prev = filtered_temp;
  }
  last_millis = current_m;
  
  // Estimation
  float base_time = 86400.0;
  if (temp_rate > 0.0001) {
    base_time = (temp_threshold - filtered_temp) / temp_rate;
  }
  
  float light_factor = 1.0 + (float)light_total / 1000.0 * NONLIN_CORR;
  temp_time = (base_time / light_factor) * log_corr;
  
  if (temp_time < 0) temp_time = 0;
  if (temp_time > 3600) temp_time = 3600;

  // Learning
  static float last_est_capture = 0;
  static unsigned long capture_time = 0;
  static bool captured_this_cycle = false;
  
  if (filtered_temp > 2.5 && filtered_temp < 3.0 && !captured_this_cycle && temp_rate > 0) {
    last_est_capture = temp_time;
    capture_time = millis();
    captured_this_cycle = true;
  }

  static unsigned long last_threshold_time = 0;
  if (filtered_temp >= temp_threshold && (millis() - last_threshold_time > 120000)) {
    if (last_threshold_time > 0 && captured_this_cycle) {
      float actual_remaining = (float)(millis() - capture_time) / 1000.0;
      if (actual_remaining > 10.0 && last_est_capture > 10.0) {
        float error_ratio = actual_remaining / last_est_capture;
        log_corr = log_corr * (0.8 + 0.2 * error_ratio);
        log_corr = constrain(log_corr, 0.5, 3.0);
      }
    }
    last_threshold_time = millis();
    light_total = 0;
    captured_this_cycle = false;
  }
  
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
  display.print("Temp: "); display.print(filtered_temp, 2);
  display.setCursor(0,30);
  display.print("Est: "); display.print(temp_time, 0); display.print(" s");
  display.display();
}
