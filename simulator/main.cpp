#include "Arduino.h"
#include "PhysicsSim.h"
#include <iostream>
#include <fstream>
#include <iomanip>

MockSerial Serial;
MockWire Wire;
FridgeSim fridge;

unsigned long current_millis = 0;

unsigned long millis() {
    return current_millis;
}

void pinMode(int pin, int mode) {}

int digitalRead(int pin) {
    if (pin == 2) { // LIGHT_PIN
        return fridge.door_open ? LOW : HIGH;
    }
    if (pin == 4) { // COMP_PIN
        return fridge.compressor_on ? HIGH : LOW;
    }
    return LOW;
}

int analogRead(int pin) {
    if (pin == 34) { // TEMP_PIN
        return fridge.getAnalogTemp();
    }
    return 0;
}

void delay(unsigned long ms) {
    current_millis += ms;
}

// Prototypes for functions that might be used in .ino
void setup();
void loop();

// Define needed headers for the .ino to satisfy include guards
#ifndef Wire_h
#define Wire_h
#endif
#ifndef Adafruit_GFX_h
#define Adafruit_GFX_h
#endif
#ifndef Adafruit_SSD1306_h
#define Adafruit_SSD1306_h
#endif

// We need to provide dummy headers so #include <Wire.h> works if they are searched in simulator directory
// or we just rely on -I simulator/ and provide those files as empty.
// Let's create dummy files.

#ifdef USE_V2
#include "../estimator2_esp32.ino"
#else
#include "../estimator_esp32.ino"
#endif

void log_csv(std::ostream& os) {
    #ifdef USE_V2
    float est = time_est;
    float target = temp_target;
    #else
    float est = temp_time;
    float target = temp_threshold;
    #endif

    os << current_millis/1000.0 << ","
       << fridge.t_air << ","
       << est << ","
       << (fridge.door_open?1:0) << ","
       << (fridge.compressor_on?1:0) << ","
       << filtered_temp << ","
       << food_temp_est << ","
       << fridge.t_food << ","
       << target << std::endl;
}

int main(int argc, char** argv) {
    setup();

    std::ostream* out = &std::cout;
    std::ofstream file_out;
    if (argc > 1) {
        file_out.open(argv[1]);
        out = &file_out;
    }

    #ifdef USE_V2
    float target = temp_target;
    #else
    float target = temp_threshold;
    #endif

    std::cerr << "Starting Simulation. Target: " << target << "C" << std::endl;
    *out << "time,true_air,est,door,compressor,filtered_temp,food_proxy,true_food,target" << std::endl;

    for (int cycle = 0; cycle < 5; ++cycle) {
        // 1. Cool down
        fridge.compressor_on = true;
        while (fridge.t_air > 2.0) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
            log_csv(*out);
        }
        fridge.compressor_on = false;

        // Stabilization
        for (int i = 0; i < 60; ++i) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
            log_csv(*out);
        }

        // 2. Open door
        fridge.door_open = true;
        int open_time = 20 + (cycle * 10);
        for (int i = 0; i < open_time; ++i) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
            log_csv(*out);
        }
        fridge.door_open = false;

        // 3. Wait for threshold
        unsigned long start_wait = current_millis;
        // Wait until true food temp hits target + some margin
        while (fridge.t_food < target + 0.3 && (current_millis - start_wait < 14400000)) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
            log_csv(*out);
        }
    }

    std::cerr << "Simulation finished." << std::endl;
    return 0;
}
