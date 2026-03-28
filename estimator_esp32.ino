// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define OLED display size and address
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_ADDR   0x3C  // OLED display I2C address

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Define pins for temperature sensor, light sensor and compressor sensor
#define TEMP_PIN 34 // Analog pin for temperature sensor
#define LIGHT_PIN 2 // Digital pin for light sensor
#define COMP_PIN 4 // Digital pin for compressor sensor

// Define constants for temperature conversion and estimation
#define VCC 3.3 // Supply voltage for temperature sensor, in volts
#define R0 10000 // Resistance of thermistor at 25 degrees C, in ohms
#define B 3950 // Beta coefficient of thermistor, in K
#define T0 298.15 // Reference temperature, in K
#define T1 273.15 // Offset temperature, in K
float log_corr = 1.0; // Adaptive correction factor for estimation
#define NONLIN_CORR 0.1 // Nonlinear correction factor for estimation

// Define variables for temperature measurement and display
float temp; // Current temperature, in degrees C
float temp_prev; // Previous temperature, in degrees C
float temp_rate; // Rate of temperature change, in degrees C per second
float temp_threshold; // Temperature threshold for estimation, in degrees C
float temp_time; // Estimated time until temperature reaches threshold, in seconds

// Define variables for light measurement and estimation
bool light; // Current light status, true if on, false if off
bool light_prev; // Previous light status, true if on, false if off
unsigned long light_start; // Start time of light duration, in milliseconds
unsigned long light_end; // End time of light duration, in milliseconds
unsigned long light_total; // Total light duration, in milliseconds
float light_factor; // Nonlinear factor for estimation based on light duration

// Define variables for learning algorithm and estimation
float log_est; // Logarithmic estimate based on extrapolation of current temperature over time
float nonlin_est; // Nonlinear estimate based on temperature loss occurring when the fridge is opened by user
float est_weight; // Weighting factor for combining logarithmic and nonlinear estimates

// Define variables for reinforcement learning and refinement
#define MAX_SAMPLES 10 // Maximum number of samples to store in the array
int sample_count; // Current number of samples stored in the array
float sample_array[MAX_SAMPLES]; // Array to store the samples of actual time until temperature reaches threshold
float sample_mean; // Mean of the samples in the array

// Define variables for compressor detection and reset
bool comp; // Current compressor status, true if on, false if off

void setup() {
  Serial.begin(115200); // Initialize serial communication
  
  pinMode(LIGHT_PIN, INPUT_PULLUP); // Set light pin as input with pull-up resistor
  
  pinMode(COMP_PIN, INPUT); // Set compressor pin as input
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { // Initialize OLED display with I2C address 
    Serial.println(F("SSD1306 allocation failed")); // Display error message if initialization fails
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay(); // Clear the display buffer
  
  display.setTextSize(1); // Set text size to 1
  display.setTextColor(SSD1306_WHITE); // Set text color to white
  
  display.setCursor(0,0); // Set cursor position to top-left corner
  display.println("Fridge Temperature"); // Print title
  
  display.setCursor(0,10); // Set cursor position below title
  display.print("Current: "); // Print label for current temperature
  
  display.setCursor(64,10); // Set cursor position to the right of label
  display.print("C"); // Print unit for current temperature
  
  display.setCursor(0,20); // Set cursor position below current temperature
  display.print("Threshold: "); // Print label for temperature threshold
  
  display.setCursor(64,20); // Set cursor position to the right of label
  display.print("C"); // Print unit for temperature threshold
  
  display.setCursor(0,30); // Set cursor position below temperature threshold
  display.print("Time: "); // Print label for estimated time
  
  display.setCursor(64,30); // Set cursor position to the right of label
  display.print("s"); // Print unit for estimated time
  
  display.display(); // Display the buffer to the screen
  
  temp_threshold = 4.0; // Set initial temperature threshold to 4 degrees C
  
  sample_count = 0; // Initialize sample count to zero
}

unsigned long last_millis = 0;
void loop() {
  unsigned long current_m = millis();
  float dt = (float)(current_m - last_millis) / 1000.0;
  if (dt < 1.0 && last_millis > 0) return; // Run every 1s
  
  temp = readTemp(); // Read current temperature from sensor
  
  static float filtered_temp = 0;
  if (filtered_temp == 0) filtered_temp = temp;
  filtered_temp = filtered_temp * 0.9 + temp * 0.1;

  if (last_millis > 0) {
    // Smooth the rate calculation using the filtered temperature
    float instant_rate = (filtered_temp - temp_prev) / dt;
    // EMA to handle noisy sensor data - slightly faster than before
    temp_rate = temp_rate * 0.98 + instant_rate * 0.02;
    temp_prev = filtered_temp;
  } else {
    temp_prev = filtered_temp;
    temp_rate = 0;
  }
  last_millis = current_m;
  
  log_est = logEst(temp_rate); // Calculate logarithmic estimate based on extrapolation
  
  light = digitalRead(LIGHT_PIN) == LOW; // Read current light status (LOW = ON)
  
  if (light != light_prev) { // Check if light status has changed
    if (light) { // Check if light is on
      light_start = millis(); // Record start time of light duration
    }
    else { // Light is off
      light_end = millis(); // Record end time of light duration
      light_total += (light_end - light_start); // Update total light duration
    }
    light_prev = light; // Update previous light status
  }
  
  light_factor = 1.0 + (float)light_total / 1000.0 * NONLIN_CORR; // Calculate nonlinear factor based on light duration
  
  nonlin_est = log_est / light_factor; // Calculate nonlinear estimate based on door openings
  
  est_weight = 0.5; // Default equal weighting
  
  temp_time = est_weight * log_est + (1 - est_weight) * nonlin_est; // Calculate estimated time by weighted average of estimates
  
  static float last_est_before_threshold = 0;
  static unsigned long est_capture_time = 0;
  // Capture a stable estimate when we are about 50% through the warming cycle
  if (temp > (temp_threshold + 2.0) / 2.0 && temp < temp_threshold - 0.5 && temp_rate > 0.0001) {
    last_est_before_threshold = temp_time;
    est_capture_time = millis();
  }

  static unsigned long last_threshold_reached = 0;
  if (temp >= temp_threshold && (last_threshold_reached == 0 || millis() - last_threshold_reached > 300000)) { // 5 min cooldown
    if (last_threshold_reached > 0) {
      float cycle_duration = (float)(millis() - last_threshold_reached) / 1000.0;
      addSample(cycle_duration);
      sample_mean = calcMean();

      if (last_est_before_threshold > 0 && est_capture_time > last_threshold_reached) {
        float actual_remaining = cycle_duration - (float)(est_capture_time - last_threshold_reached) / 1000.0;
        if (actual_remaining > 30.0) {
          float error_ratio = actual_remaining / last_est_before_threshold;
          log_corr = log_corr * (0.9 + 0.1 * error_ratio); // Slightly more aggressive learning
          log_corr = constrain(log_corr, 0.5, 2.0);
          Serial.print("Actual remaining: "); Serial.println(actual_remaining);
          Serial.print("Last estimate: "); Serial.println(last_est_before_threshold);
          Serial.print("Adjusted log_corr to: "); Serial.println(log_corr);
        }
      }
      last_est_before_threshold = 0;
    }
    last_threshold_reached = millis();
    resetLight();
  }
  
  comp = digitalRead(COMP_PIN);
  
  if (comp) {
    resetEstimate();
  }
  
  display.setCursor(48,10);
  display.print(temp, 1);
  
  display.setCursor(48,20);
  display.print(temp_threshold, 1);
  
  display.setCursor(48,30);
  display.print(temp_time, 0);
  
  display.display();
}

float readTemp() {
  int adc = analogRead(TEMP_PIN);
  if (adc >= 4095) return 200.0;
  float vout = (float)adc / 4095.0 * VCC;
  float r = R0 * vout / (VCC - vout);
  float t = 1.0 / (1.0 / T0 + log(r / R0) / B) - T1;
  return t;
}

float logEst(float rate) {
  if (rate <= 0.00001) return 86400.0;
  float t = (temp_threshold - temp) / rate;
  if (t < 0) return 0;
  if (t > 86400.0) t = 86400.0;
  return t * log_corr;
}

void addSample(float s) {
  if (sample_count < MAX_SAMPLES) {
    sample_array[sample_count] = s;
    sample_count++;
  }
  else {
    for (int i = 0; i < MAX_SAMPLES - 1; i++) {
      sample_array[i] = sample_array[i+1];
    }
    sample_array[MAX_SAMPLES - 1] = s;
  }
}

float calcMean() {
  float sum = 0.0;
  if (sample_count == 0) return 0;
  for (int i = 0; i < sample_count; i++) {
    sum += sample_array[i];
  }
  return sum / sample_count;
}

void resetLight() {
  light_start = 0;
  light_end = 0;
  light_total = 0;
}

void resetEstimate() {
  temp_time = 0;
}
