# Fridge Temperature Estimator

This project provides ESP32-based Arduino code to estimate the time remaining until a fridge reaches a critical temperature threshold (e.g., 4°C). It includes two variants of the estimation logic and a comprehensive C++ simulation environment for testing and validation.

## Estimator Variants

### 1. Adaptive Learning Variant (`estimator_esp32.ino`)
Uses a linear extrapolation of the temperature rate-of-change combined with an adaptive correction factor.
- **Filtering:** Employs dual-stage filtering (Median/EMA) to handle noisy thermistor data.
- **Adaptive Learning:** After each warming cycle, it compares the predicted time with the actual elapsed time to adjust a `log_corr` scaling factor.
- **Door Compensation:** Applies a linear factor based on total door-open duration.

### 2. Physical Model Variant (`estimator2_esp32.ino`)
Uses a physics-inspired model to estimate internal temperature shifts.
- **Non-linear Door Loss:** Models air exchange during door openings using a square-root duration model (representing the rapid initial exchange of air).
- **Continuous Estimation:** Provides a real-time countdown of time-to-threshold based on the smoothed temperature rate.

## Simulation Environment

A custom C++ simulator is provided in the `simulator/` directory to test the logic without hardware.

### Physics Simulation Features
- **Thermal Mass & Insulation:** Models the heat capacity of the fridge contents and heat leakage through walls.
- **Sensor Noise:** Simulates realistic analog-to-digital converter (ADC) noise.
- **Door Events:** Randomly simulates door openings and their thermal impact.
- **Compressor Cycle:** Simulates the cooling phase when the compressor is active.

### Running the Tests
To build and run the simulation:
```bash
cd simulator
make
./test_v1  # Runs Estimator 1
./test_v2  # Runs Estimator 2
```
This generates `data_v1_rev.csv` and `data_v2_rev.csv`.

### Visualization
The `plot_fridge.py` script generates performance graphs:
```bash
python3 plot_fridge.py
```

## Performance Results

### Estimator 1: Adaptive Learning
![Estimator 1 Results](graph_v1_rev.png)
*Figure 1: Temperature (red) and Time Estimate (blue). The estimate converges as the learning algorithm adjusts the correction factor over multiple cycles.*

### Estimator 2: Physical Model
![Estimator 2 Results](graph_v2_rev.png)
*Figure 2: Performance using the physics-based door loss model. Note the rapid response to door-open events (orange marks).*

## Hardware Requirements
- ESP32 Development Board
- NTC Thermistor (10k) with 10k resistor divider on GPIO 34
- Digital light sensor (Door status) on GPIO 2
- Digital compressor sensor on GPIO 4
- SSD1306 OLED Display (I2C)
