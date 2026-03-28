#ifndef PHYSICS_SIM_H
#define PHYSICS_SIM_H

#include <iostream>
#include <cmath>
#include <random>

class FridgeSim {
public:
    double t_air = 2.0;
    double t_food = 2.0;
    double ambient_temp = 25.0;
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

    void setParams(double ambient, double food_mass_ratio) {
        ambient_temp = ambient;
        c_food = 2000.0 * food_mass_ratio;
    }

    std::mt19937 gen{42};

    void update(double dt) {
        int steps = 20;
        double sub_dt = dt / steps;
        for(int i=0; i<steps; ++i) {
            double r_env = door_open ? r_door_open : r_insulation;
            double q_env = (ambient_temp - t_air) / r_env;
            double q_food = (t_food - t_air) / r_coupling;
            double q_cool = compressor_on ? -cooling_power : 0.0;

            t_air += ((q_env + q_food + q_cool) / c_air) * sub_dt;
            t_food += ((-q_food) / c_food) * sub_dt;
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
