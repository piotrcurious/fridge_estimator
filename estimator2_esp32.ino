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

float temp;
float filtered_temp = -999;
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

unsigned long last_millis = 0;
float rate_ema = 0;

void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 1.0 && last_millis > 0) return;

  temp = readTemp();
  if (filtered_temp < -100) filtered_temp = temp;
  filtered_temp = filtered_temp * 0.92 + temp * 0.08;

  light = digitalRead(LIGHT_PIN) == LOW;
  if (light != light_prev) {
    if (light) {
      light_start = millis();
    } else {
      unsigned long duration = millis() - light_start;
      door_penalty_accum += 0.085 * sqrt((float)duration / 1000.0);
      door_closed_time = millis();
    }
    light_prev = light;
  }

  comp = digitalRead(COMP_PIN);
  if (comp && !comp_prev) {
    door_penalty_accum = 0;
  }
  comp_prev = comp;

  if (last_millis > 0) {
    float instant_rate = (filtered_temp - temp_prev) / dt;

    // Stability logic: door closed for 60s, and no compressor
    bool stable = (!light && (millis() - door_closed_time > 60000));

    if (stable && !comp) {
      rate_ema = rate_ema * 0.994 + instant_rate * 0.006;
      temp_rate = rate_ema;
    } else if (comp) {
      rate_ema = rate_ema * 0.94;
      temp_rate = 0;
    }
    temp_prev = filtered_temp;
  } else {
    temp_prev = filtered_temp;
    temp_rate = 0;
  }
  last_millis = current_m;
  
  // Estimation: (Distance to target / Rate) - Factor for accumulated door heat
  if (temp_rate > 0.0001) {
    float effective_dist = (temp_target - filtered_temp) - door_penalty_accum;
    if (effective_dist < 0) effective_dist = 0;
    
    time_est = effective_dist / temp_rate;
  } else {
    time_est = 3600;
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
  // Steinhart-Hart B-parameter equation
  float t = 1.0 / (1.0 / 298.15 + log(r / R0) / B) - 273.15;
  return t;
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
