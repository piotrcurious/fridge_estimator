// Libraries for OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define OLED display size and address
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Define pins for temperature sensor, light sensor, and compressor
#define TEMP_PIN 34 // Analog pin for temperature sensor
#define LIGHT_PIN 2 // Digital pin for light sensor (with pull-up resistor)
#define COMP_PIN 4 // Digital pin for compressor

// Define constants for temperature sensor
#define VCC 3.3 // Supply voltage in volts
#define R0 10000 // Resistance of thermistor at 25 degrees C in ohms
#define B 3950 // Beta coefficient of thermistor in K
#define T0 298.15 // Reference temperature in K

// Define variables for temperature measurement
float temp; // Current temperature in degrees C
float temp_prev; // Previous temperature in degrees C
float temp_rate; // Rate of change of temperature in degrees C per second
float temp_target = 4; // Target temperature in degrees C
float temp_threshold = 0.5; // Temperature threshold for compressor activation in degrees C

// Define variables for light measurement
bool light; // Current light status (true = on, false = off)
bool light_prev; // Previous light status
unsigned long light_start; // Start time of light on in milliseconds
unsigned long light_end; // End time of light off in milliseconds
unsigned long light_duration; // Duration of light on in milliseconds

// Define variables for compressor measurement
bool comp; // Current compressor status (true = on, false = off)
bool comp_prev; // Previous compressor status
unsigned long comp_start; // Start time of compressor on in milliseconds
unsigned long comp_end; // End time of compressor off in milliseconds
unsigned long comp_duration; // Duration of compressor on in milliseconds

// Define variables for estimation algorithm
float temp_est; // Estimated temperature in degrees C
float temp_loss; // Estimated temperature loss due to opening the fridge in degrees C
float time_est; // Estimated time until target temperature is reached in seconds

// Define variables for display update
unsigned long display_interval = 1000; // Interval for updating the display in milliseconds
unsigned long display_last; // Last time the display was updated in milliseconds

void setup() {
  Serial.begin(115200); // Initialize serial communication
  
  pinMode(LIGHT_PIN, INPUT_PULLUP); // Set light pin as input with pull-up resistor
  pinMode(COMP_PIN, INPUT); // Set compressor pin as input
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { // Initialize display with I2C address
    Serial.println("SSD1306 allocation failed"); // Display error message if initialization fails
    for(;;); // Loop forever
  }
  
  display.clearDisplay(); // Clear the display buffer
  display.setTextSize(1); // Set text size to 1
  display.setTextColor(WHITE); // Set text color to white
  
  display.setCursor(0,0); // Set cursor position to top-left corner
  display.println("Fridge Temperature"); // Print title on display
  
  display.display(); // Show the display buffer on the screen
  
}

void loop() {
  
  temp_prev = temp; // Store previous temperature value
  
  temp = readTemp(); // Read current temperature value
  
  temp_rate = (temp - temp_prev) / (millis() / 1000.0); // Calculate rate of change of temperature
  
  Serial.print("Temperature: "); 
  Serial.print(temp);
  Serial.println(" C");
  
  light_prev = light; // Store previous light status
  
  light = digitalRead(LIGHT_PIN) == LOW; // Read current light status (inverted logic)
  
  if (light && !light_prev) { // If light is turned on
    
    light_start = millis(); // Record start time of light on
    
    Serial.println("Light on");
    
    temp_loss = estimateTempLoss(); // Estimate temperature loss due to opening the fridge
    
    Serial.print("Estimated temperature loss: ");
    Serial.print(temp_loss);
    Serial.println(" C");
    
    temp_est = temp + temp_loss; // Adjust estimated temperature with temperature loss
    
    time_est = estimateTime(); // Estimate time until target temperature is reached
    
    Serial.print("Estimated time: ");
    Serial.print(time_est);
    Serial.println(" s");
    
    updateDisplay(); // Update the display with new values
    
  }
  
  if (!light && light_prev) { // If light is turned off
    
    light_end = millis(); // Record end time of light off
    
    light_duration = light_end - light_start; // Calculate duration of light on
    
    Serial.print("Light off, duration: ");
    Serial.print(light_duration);
    Serial.println(" ms");
    
  }
  
  comp_prev = comp; // Store previous compressor status
  
  comp = digitalRead(COMP_PIN) == HIGH; // Read current compressor status
  
  if (comp && !comp_prev) { // If compressor is turned on
    
    comp_start = millis(); // Record start time of compressor on
    
    Serial.println("Compressor on");
    
  }
  
  if (!comp && comp_prev) { // If compressor is turned off
    
    comp_end = millis(); // Record end time of compressor off
    
    comp_duration = comp_end - comp_start; // Calculate duration of compressor on
    
    Serial.print("Compressor off, duration: ");
    Serial.print(comp_duration);
    Serial.println(" ms");
    
    temp_est = temp; // Reset estimated temperature to current temperature
    
    time_est = estimateTime(); // Estimate time until target temperature is reached
    
    Serial.print("Estimated time: ");
    Serial.print(time_est);
    Serial.println(" s");
    
    updateDisplay(); // Update the display with new values
    
  }
  
  if (millis() - display_last > display_interval) { // If display update interval has passed
    
    updateDisplay(); // Update the display with current values
    
  }
  
}

// Function to read temperature from sensor
float readTemp() {
  
  int adc = analogRead(TEMP_PIN); // Read analog value from sensor
  float vout = adc * VCC / 4095.0; // Convert analog value to voltage
  float r = R0 * VCC / vout - R0; // Calculate resistance of thermistor
  float t = 1.0 / (1.0 / T0 + log(r / R0) / B) - 273.15; // Calculate temperature in degrees C
  
  return t; // Return temperature value
  
}

// Function to estimate temperature loss due to opening the fridge
float estimateTempLoss() {
  
  float loss = 0; // Initialize loss variable
  
  // Use a simple linear function to estimate loss based on light duration
  // This can be improved with more data and a better model
  loss = 0.01 * light_duration / 1000.0;
  
  return loss; // Return loss value
  
}

// Function to estimate time until target temperature is reached
float estimateTime() {
  
  float time = 0; // Initialize time variable
  
  // Use a simple logarithmic function to estimate time based on current and estimated temperature
  // This can be improved with more data and a better model
  time = -10 * log(temp_est - temp_target);
  
  return time; // Return time value
  
}

// Function to update the display with current and estimated values
void updateDisplay() {
  
  display.clearDisplay(); // Clear the display buffer
  
  display.setCursor(0,0); // Set cursor position to top-left corner
  display.println("Fridge Temperature"); // Print title on display
  
  display.setCursor(0,10); // Set cursor position to next line
  display.print("Current: "); 
  display.print(temp,1); 
  display.println(" C"); // Print current temperature on display
  
  display.setCursor(0,20); // Set cursor position to next line
  display.print("Estimated: "); 
  display.print(temp_est,1); 
  display.println(" C"); // Print estimated temperature on display
  
  display.setCursor(0,30); // Set cursor position to next line
  display.print("Target: "); 
  display.print(temp_target,1); 
  display.println(" C"); // Print target temperature on display
  
  display.setCursor(0,40); // Set cursor position to next line
  display.print("Time: "); 
  display.print(time_est,1); 
  display.println(" s"); // Print estimated time on display
  
  display.display(); // Show the display buffer on the screen
  
}
