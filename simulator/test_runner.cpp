#include "Arduino.h"
#include "PhysicsSim.h"
#include <chrono>
#include <thread>
#include <iostream>

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
        return fridge.door_open ? HIGH : LOW;
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

// Wrap the INO to handle the system headers
#define Wire_h
#define Adafruit_GFX_h
#define Adafruit_SSD1306_h
#include "../estimator2_esp32.ino"

int main() {
    setup();
    std::cout << "Starting Simulation for estimator2_esp32.ino..." << std::endl;
    fridge.current_temp = -5.0; // Start cold
    for (int i = 0; i < 40000; ++i) {
        current_millis += 100; // 100ms per loop
        fridge.update(0.1);    // 0.1s update

        // Scenario:
        if (i == 100) {
            fridge.door_open = true;
            std::cout << "--- Door Opened ---" << std::endl;
        }
        if (i == 150) {
            fridge.door_open = false;
            std::cout << "--- Door Closed ---" << std::endl;
        }

        loop();

        if (i % 2000 == 0) {
            std::cout << "Time: " << current_millis / 1000.0
                      << "s, Temp: " << temp
                      << "C, Est Time: " << time_est << "s, Rate: " << temp_rate << std::endl;
        }
        if (temp >= temp_target) {
             std::cout << "TARGET REACHED AT " << current_millis / 1000.0 << "s" << std::endl;
             // Final display update
             loop();
             std::cout << "Final Est Time at threshold: " << time_est << "s" << std::endl;
             break;
        }
    }
    return 0;
}
