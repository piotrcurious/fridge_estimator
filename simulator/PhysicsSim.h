#ifndef PHYSICS_SIM_H
#define PHYSICS_SIM_H

#include <iostream>
#include <cmath>
#include <random>

class FridgeSim {
public:
    double t_air = 2.0;
    double t_food = 2.0;
    double base_ambient = 25.0;
    double ambient_swing = 0.0; // Sinusoidal variation
    double diurnal_period = 86400.0;
    bool door_open = false;
    bool compressor_on = false;

    // Parameters for a typical fridge
    double c_air = 200.0;
    double c_food = 2000.0;
    double r_insulation = 40.0;
    double r_coupling = 10.0;
    double r_door_open = 1.5;
    double cooling_power = 70.0;

    double noise_stddev = 0.5;
    double current_time = 0.0;

    void setParams(double ambient, double mass_ratio, double swing = 0.0) {
        base_ambient = ambient;
        c_food = 2000.0 * mass_ratio;
        ambient_swing = swing;
    }

    std::mt19937 gen{42};

    double getAmbient() const {
        return base_ambient + ambient_swing * std::sin(2.0 * M_PI * current_time / diurnal_period);
    }

    void update(double dt) {
        int steps = 20;
        double sub_dt = dt / steps;
        for(int i=0; i<steps; ++i) {
            double r_env = door_open ? r_door_open : r_insulation;
            double q_env = (getAmbient() - t_air) / r_env;
            double q_food = (t_food - t_air) / r_coupling;
            double q_cool = compressor_on ? -cooling_power : 0.0;

            t_air += ((q_env + q_food + q_cool) / c_air) * sub_dt;
            t_food += ((-q_food) / c_food) * sub_dt;
            current_time += sub_dt;
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

#endif
