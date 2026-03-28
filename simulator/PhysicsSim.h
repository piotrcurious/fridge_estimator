#ifndef PHYSICS_SIM_H
#define PHYSICS_SIM_H

#include <iostream>
#include <cmath>

class FridgeSim {
public:
    double current_temp = 2.0; // Initial temperature inside fridge (Celsius)
    double ambient_temp = 25.0; // Room temperature (Celsius)
    bool door_open = false;
    bool compressor_on = false;

    // Thermal constants (simplified)
    double k_leak = 0.0001; // Rate of temperature rise (leakage) when closed
    double k_door = 0.01;   // Rate of temperature rise when door is open
    double k_cool = 0.002;  // Rate of temperature drop when compressor is on

    void update(double dt) {
        if (door_open) {
            current_temp += k_door * (ambient_temp - current_temp) * dt;
        } else {
            current_temp += k_leak * (ambient_temp - current_temp) * dt;
        }

        if (compressor_on) {
            current_temp -= k_cool * (current_temp - (-10.0)) * dt; // Cooling toward -10C
        }
    }

    int getAnalogTemp() {
        // According to estimator_esp32.ino:
        // float vout = (float)adc / 4095.0 * VCC;
        // float r = R0 * vout / (VCC - vout);
        // float t = B / log(r / R0) - T1;

        // This means log(r / R0) = B / (t + T1)
        // r / R0 = exp(B / (t + T1))

        // Let's assume the user made a typo and it should be:
        // float t = 1.0 / (1.0/T0 + log(r/R0)/B) - T1
        // But let's look at estimator2_esp32.ino for inspiration:
        // float t = 1.0 / (1.0 / T0 + log(r / R0) / B) - 273.15;

        // Let's use what estimator2_esp32.ino has for the simulator as it looks more correct
        double T1 = 273.15;
        double B = 3950;
        double R0 = 10000;
        double T0 = 298.15; // 25 C

        // Standard thermistor equation: 1/T = 1/T0 + 1/B * ln(R/R0)
        // 1/T - 1/T0 = 1/B * ln(R/R0)
        // B * (1/T - 1/T0) = ln(R/R0)
        // R = R0 * exp(B * (1/(t+T1) - 1/T0))

        double r = R0 * std::exp(B * (1.0 / (current_temp + T1) - 1.0 / T0));

        // vout = VCC * r / (R0 + r)  <-- using the divider from the .ino
        // adc = 4095 * r / (R0 + r)
        double adc = 4095.0 * r / (R0 + r);
        return (int)adc;
    }
};

extern FridgeSim fridge;

#endif
