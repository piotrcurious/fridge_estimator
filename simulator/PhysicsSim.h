#ifndef PHYSICS_SIM_H
#define PHYSICS_SIM_H

#include <iostream>
#include <cmath>
#include <random>

class FridgeSim {
public:
    double t_air = 2.0;    // Air temperature (Celsius)
    double t_food = 2.0;   // Food/Thermal mass temperature (Celsius)
    double ambient_temp = 25.0; // Room temperature (Celsius)
    bool door_open = false;
    bool compressor_on = false;

    // Thermal constants - more realistic for a small fridge
    double c_air = 300.0;       // Air + light internals (J/K)
    double c_food = 10000.0;    // Food thermal mass (J/K) - ~2.5kg of water
    double r_insulation = 50.0; // Well insulated (K/W)
    double r_coupling = 10.0;   // Coupling between air and food (K/W)
    double r_door_open = 2.0;   // Air exchange when door is open (K/W)
    double cooling_power = 60.0; // Cooling power in Watts (applied to air)

    // Noise parameters
    double noise_stddev = 0.2; // Realistic sensor noise
    std::mt19937 gen{42};

    void update(double dt) {
        // Multiple sub-steps for stability if dt is large
        int steps = 10;
        double sub_dt = dt / steps;
        for(int i=0; i<steps; ++i) {
            // 1. Heat flow from ambient to air
            double r_env = door_open ? r_door_open : r_insulation;
            double q_env = (ambient_temp - t_air) / r_env;

            // 2. Heat flow from food to air (coupling)
            double q_food = (t_food - t_air) / r_coupling;

            // 3. Cooling power
            double q_cool = compressor_on ? -cooling_power : 0.0;

            // Total heat flow to air
            double q_air_total = q_env + q_food + q_cool;

            // Update air temperature
            t_air += (q_air_total / c_air) * sub_dt;

            // Update food temperature
            t_food += (-q_food / c_food) * sub_dt;
        }
    }

    int getAnalogTemp() {
        std::normal_distribution<double> d{0, noise_stddev};
        double noisy_temp = t_air + d(gen);

        double T1 = 273.15;
        double B = 3950;
        double R0 = 10000;
        double T0 = 298.15;

        double r = R0 * std::exp(B * (1.0 / (noisy_temp + T1) - 1.0 / T0));
        double adc = 4095.0 * r / (R0 + r);

        if (adc > 4094) adc = 4094;
        if (adc < 0) adc = 0;
        return (int)adc;
    }
};

extern FridgeSim fridge;

#endif
