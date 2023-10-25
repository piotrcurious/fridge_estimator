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
#define LOG_BASE 2.7182818284590452353602874713527 // Base of natural logarithm
#define LOG_CORR 0.9 // Logarithmic correction factor for estimation
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
float sample_error; // Error between the estimated time and the mean of the samples

// Define variables for compressor detection and reset
bool comp; // Current compressor status, true if on, false if off

// Define variables for additional correction based on fitting of temperature drop 
float temp_quarter; // Temperature at quarter of the estimated time 
float temp_drop; // Temperature drop from current to quarter 
float fit_slope; // Slope of the linear fit of temperature drop 
float fit_intercept; // Intercept of the linear fit of temperature drop 
float fit_error; // Error between the actual and fitted temperature drop 
float fit_corr; // Additional correction factor based on fitting error 

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

void loop() {
  
  temp = readTemp(); // Read current temperature from sensor
  
  temp_rate = (temp - temp_prev) / (millis() / 1000.0); // Calculate rate of temperature change
  
  log_est = logEst(temp_rate); // Calculate logarithmic estimate based on extrapolation
  
  light = digitalRead(LIGHT_PIN); // Read current light status from sensor
  
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
  
  nonlin_est = temp_threshold * light_factor; // Calculate nonlinear estimate based on temperature loss
  
  est_weight = LOG_CORR / (LOG_CORR + NONLIN_CORR); // Calculate weighting factor for combining estimates
  
  temp_time = est_weight * log_est + (1 - est_weight) * nonlin_est; // Calculate estimated time by weighted average of estimates
  
  if (temp >= temp_threshold) { // Check if temperature has reached threshold
    addSample(millis() / 1000.0); // Add the actual time to the sample array
    sample_mean = calcMean(); // Calculate the mean of the samples in the array
    sample_error = temp_time - sample_mean; // Calculate the error between the estimated time and the mean of the samples
    adjustWeight(sample_error); // Adjust the weighting factor based on the error
    resetLight(); // Reset the light duration variables
    fitCorr(); // Apply additional correction based on fitting of temperature drop 
    correctArray(fit_corr); // Correct the array elements based on additional correction factor 
  }
  
  comp = digitalRead(COMP_PIN); // Read current compressor status from sensor
  
  if (comp) { // Check if compressor is on
    resetEstimate(); // Reset the current estimate to zero
  }
  
  display.setCursor(48,10); // Set cursor position for current temperature value
  display.print(temp, 1); // Print current temperature value with one decimal place
  
  display.setCursor(48,20); // Set cursor position for temperature threshold value
  display.print(temp_threshold, 1); // Print temperature threshold value with one decimal place
  
  display.setCursor(48,30); // Set cursor position for estimated time value
  display.print(temp_time, 0); // Print estimated time value with no decimal place
  
  display.display(); // Display the buffer to the screen
  
  temp_prev = temp; // Update previous temperature
  
}

// Function to read temperature from sensor and convert to degrees C
float readTemp() {
  int adc = analogRead(TEMP_PIN); // Read analog value from sensor
  float vout = (float)adc / 4095.0 * VCC; // Convert analog value to voltage
  float r = R0 * vout / (VCC - vout); // Calculate resistance of thermistor
  float t = B / log(r / R0) - T1; // Calculate temperature of thermistor
  return t; // Return temperature in degrees C
}

// Function to calculate logarithmic estimate based on extrapolation of current temperature over time
float logEst(float rate) {
  float t = (temp_threshold - temp) / rate; // Calculate time by linear extrapolation
  float l = pow(LOG_BASE, t * LOG_CORR); // Apply logarithmic correction factor
  return l; // Return logarithmic estimate in seconds
}

// Function to add a sample to the array and shift the older samples if necessary
void addSample(float s) {
  if (sample_count < MAX_SAMPLES) { // Check if array is not full
    sample_array[sample_count] = s; // Add sample to the end of array
    sample_count++; // Increment sample count
  }
  else { // Array is full
    for (int i = 0; i < MAX_SAMPLES - 1; i++) { // Loop through array except last element
      sample_array[i] = sample_array[i+1]; // Shift element to the left by one position
    }
    sample_array[MAX_SAMPLES - 1] = s; // Add sample to the last position of array
    // Sample count remains unchanged
  }
}

// Function to calculate the mean of the samples in the array
float calcMean() {
  float sum = 0.0; // Initialize sum to zero
  for (int i = 0; i < sample_count; i++) { // Loop through array elements
    sum += sample_array[i]; // Add element to sum
  }
  
return sum / sample_count; // Return mean of the samples in seconds
}

// Function to adjust the weighting factor based on the error between the estimated time and the mean of the samples
void adjustWeight(float error) {
  if (error > 0) { // Check if estimated time is too high
    est_weight -= error / temp_time; // Decrease weighting factor by error ratio
    est_weight = constrain(est_weight, 0, 1); // Constrain weighting factor between 0 and 1
  }
  else if (error < 0) { // Check if estimated time is too low
    est_weight -= error / temp_time; // Increase weighting factor by error ratio
    est_weight = constrain(est_weight, 0, 1); // Constrain weighting factor between 0 and 1
  }
}

// Function to reset the light duration variables 
void resetLight() {
  light_start = 0; // Reset start time of light duration 
  light_end = 0; // Reset end time of light duration 
  light_total = 0; // Reset total light duration 
}

// Function to reset the current estimate to zero 
void resetEstimate() {
  temp_time = 0; // Reset estimated time to zero 
}

// Function to apply additional correction based on fitting of temperature drop 
void fitCorr() {
  
temp_quarter = temp - temp_rate * temp_time /4.0; // Calculate temperature at quarter of the estimated time 
temp_drop = temp - temp_quarter; // Calculate temperature drop from current to quarter 
fit_slope = temp_drop / (temp_time /4.0); // Calculate slope of the linear fit of temperature drop 
fit_intercept = temp - fit_slope * temp_time; // Calculate intercept of the linear fit of temperature drop 
fit_error = temp_threshold - (fit_slope * temp_time + fit_intercept); // Calculate error between the actual and fitted temperature drop 
fit_corr = fit_error / temp_time; // Calculate additional correction factor based on fitting error 

}

// Function to correct the array elements based on additional correction factor 
void correctArray(float corr) {
  
for (int i = 0; i < sample_count; i++) { // Loop through array elements 
sample_array[i] += corr * sample_array[i]; // Correct element by adding correction factor times element 
}

}
