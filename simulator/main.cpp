#include "Arduino.h"
#include "PhysicsSim.h"
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
        // Standard pull-up: LOW when door open (switch closed)
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

// Forward declarations for functions in .ino
float readTemp();
float logEst(float rate);
void addSample(float s);
float calcMean();
void adjustWeight(float error);
void resetLight();
void resetEstimate();
void fitCorr();
void correctArray(float corr);
float estimateTempLoss();
float estimateTime();
void updateDisplay();

#define Wire_h
#define Adafruit_GFX_h
#define Adafruit_SSD1306_h

#ifdef USE_ESTIMATOR_2
#include "../estimator2_esp32.ino"
#else
#include "../estimator_esp32.ino"
#endif

int main() {
    setup();
    #ifdef USE_ESTIMATOR_2
    std::cout << "--- Testing estimator2_esp32.ino ---" << std::endl;
    #else
    std::cout << "--- Testing estimator_esp32.ino ---" << std::endl;
    #endif

    for (int cycle = 0; cycle < 3; ++cycle) {
        std::cout << "Cycle " << cycle << " started." << std::endl;

        // 1. Cool down
        fridge.compressor_on = true;
        while (fridge.current_temp > 2.0) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
        }
        fridge.compressor_on = false;

        // 2. Open door 20s
        fridge.door_open = true;
        for (int i = 0; i < 20; ++i) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();
        }
        fridge.door_open = false;

        // 3. Wait for threshold
        #ifdef USE_ESTIMATOR_2
        float target = temp_target;
        #else
        float target = temp_threshold;
        #endif

        while (temp < target) {
            current_millis += 1000;
            fridge.update(1.0);
            loop();

            if ((current_millis / 1000) % 300 == 0) {
                #ifdef USE_ESTIMATOR_2
                float est = time_est;
                #else
                float est = temp_time;
                #endif
                std::cout << "Time: " << current_millis / 1000.0 << "s, Temp: " << temp
                          << "C, Est Remaining: " << est << "s" << std::endl;
            }
        }
        std::cout << "Threshold reached at " << current_millis / 1000.0 << "s" << std::endl;
        loop(); // Trigger learning
    }
    return 0;
}
