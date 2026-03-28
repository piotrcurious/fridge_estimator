#ifndef PHYSICS_SIM_H
#define PHYSICS_SIM_H

#include <iostream>
#include <cmath>
#include <random>

class FridgeSim {
public:
    double current_temp = 2.0; // Initial temperature inside fridge (Celsius)
    double ambient_temp = 25.0; // Room temperature (Celsius)
    bool door_open = false;
    bool compressor_on = false;

    // Thermal constants (simplified)
    double thermal_mass = 1000.0; // Thermal mass in Joules per degree
    double r_insulation = 10.0;   // Thermal resistance of insulation (deg/W)
    double r_door_open = 0.5;      // Thermal resistance when door is open (deg/W)
    double cooling_power = 50.0;  // Cooling power in Watts when compressor is on

    // Noise parameters
    double noise_stddev = 0.1; // Standard deviation for ADC noise in Celsius equivalent
    std::mt19937 gen{42}; // Fixed seed for reproducibility

    void update(double dt) {
        double heat_flow;
        if (door_open) {
            heat_flow = (ambient_temp - current_temp) / r_door_open;
        } else {
            heat_flow = (ambient_temp - current_temp) / r_insulation;
        }

        if (compressor_on) {
            heat_flow -= cooling_power;
        }

        current_temp += (heat_flow / thermal_mass) * dt;
    }

    int getAnalogTemp() {
        // Add random Gaussian noise to simulate sensor noise
        std::normal_distribution<double> d{0, noise_stddev};
        double noisy_temp = current_temp + d(gen);

        // Map noisy_temp back to ADC value using the expected .ino logic
        // Standard Steinhart-Hart equation: 1/T = 1/T0 + 1/B * ln(R/R0)
        // 1/(t+273.15) - 1/T0 = ln(R/R0) / B
        // R = R0 * exp(B * (1/(t+273.15) - 1/T0))

        double T1 = 273.15;
        double B = 3950;
        double R0 = 10000;
        double T0 = 298.15; // 25 C

        double r = R0 * std::exp(B * (1.0 / (noisy_temp + T1) - 1.0 / T0));

        // vout = VCC * r / (R0 + r)
        // adc = 4095 * r / (R0 + r)
        double adc = 4095.0 * r / (R0 + r);
        if (adc > 4094) adc = 4094;
        if (adc < 0) adc = 0;
        return (int)adc;
    }
};

extern FridgeSim fridge;

#endif
