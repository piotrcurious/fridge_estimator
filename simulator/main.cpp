#include "Arduino.h"
#include "PhysicsSim.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

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

// Prototypes
void setup();
void loop();

#ifndef Wire_h
#define Wire_h
#endif
#ifndef Adafruit_GFX_h
#define Adafruit_GFX_h
#endif
#ifndef Adafruit_SSD1306_h
#define Adafruit_SSD1306_h
#endif

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
       << target << ","
       << debug_active_rate << ","
       << debug_confidence << ","
       << debug_alpha << std::endl;
}

int main(int argc, char** argv) {
    double ambient = 25.0;
    double mass_ratio = 1.0;
    double swing = 0.0;
    std::string out_path = "";
    double sim_duration = 86400.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ambient" && i + 1 < argc) {
            ambient = std::stod(argv[++i]);
        } else if (arg == "--mass" && i + 1 < argc) {
            mass_ratio = std::stod(argv[++i]);
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--duration" && i + 1 < argc) {
            sim_duration = std::stod(argv[++i]);
        } else if (arg == "--swing" && i + 1 < argc) {
            swing = std::stod(argv[++i]);
        }
    }

    fridge.setParams(ambient, mass_ratio, swing);
    setup();

    std::ostream* out = &std::cout;
    std::ofstream file_out;
    if (!out_path.empty()) {
        file_out.open(out_path);
        out = &file_out;
    }

    *out << "time,true_air,est,door,compressor,filtered_temp,food_proxy,true_food,target,active_rate,confidence,alpha" << std::endl;

    std::mt19937 event_gen(1337);
    std::uniform_real_distribution<double> door_duration(5, 60);

    double next_door_event = 3600.0;

    for (double t = 0; t < sim_duration; t += 1.0) {
        // More aggressive hysteresis for visible cycles
        if (fridge.t_air > 5.5) fridge.compressor_on = true;
        if (fridge.t_air < 1.0) fridge.compressor_on = false;

        if (t >= next_door_event) {
            fridge.door_open = true;
            double duration = door_duration(event_gen);
            for(int i=0; i<(int)duration; ++i) {
                current_millis += 1000;
                fridge.update(1.0);
                loop();
                log_csv(*out);
                t += 1.0;
            }
            fridge.door_open = false;
            std::uniform_real_distribution<double> next_event_dist(1800, 14400);
            next_door_event = t + next_event_dist(event_gen);
        }

        current_millis += 1000;
        fridge.update(1.0);
        loop();
        log_csv(*out);
    }

    return 0;
}
