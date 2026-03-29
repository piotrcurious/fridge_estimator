import subprocess
import pandas as pd
import numpy as np
import os
import json

def run_sim(version, ambient, mass, duration, output_file):
    print(f"Running {version} (Ambient: {ambient}C, Mass: {mass}x, Duration: {duration}s)...")

    cmd = ["g++", "-O3", "simulator/main.cpp", "-o", "simulator/sim", "-Isimulator/"]
    if version == "V2":
        cmd.append("-DUSE_V2")

    # Compile
    subprocess.run(cmd, check=True, capture_output=True)

    # Run
    subprocess.run(["./simulator/sim", "--out", output_file, "--ambient", str(ambient), "--mass", str(mass), "--duration", str(duration)],
                   check=True, capture_output=True)

def analyze(csv_file):
    df = pd.read_csv(csv_file)

    # Calculate true remaining time to hit 4C target for food
    target = 4.0

    # We find all points where true_food hits 4C
    # Then for each point we calculate how long until then.
    # This is a bit complex for a stochastic 24h run since it might hit 4C multiple times if the fridge warms up.
    # Actually the simulator I just wrote doesn't force a warming cycle. It's a bang-bang controller.
    # So the fridge stays cold.

    # Let's define the "Remaining Time" task:
    # If the power went out right now, how long until food hits 4C?

    # To compute this, we need a separate simulation or a model.
    # Alternatively, we can let the simulator periodically "turn off" the fridge to measure true time.
    # For now, let's use the ground truth physics to calculate true_remaining:
    # T_food(t) = T_amb - (T_amb - T_food_0) * exp(-alpha_true * t)
    # 4 = T_amb - (T_amb - T_food_0) * exp(-alpha_true * t_rem)
    # exp(-alpha_true * t_rem) = (T_amb - 4) / (T_amb - T_food_0)
    # -alpha_true * t_rem = ln((T_amb - 4) / (T_amb - T_food_0))
    # t_rem = - (1 / alpha_true) * ln((T_amb - 4) / (T_amb - T_food_0))

    # We need the true alpha_sys for the simulator.
    # FridgeSim: t_food_dot = - (t_food - t_air) / (r_coupling * c_food)
    # This is not a simple Newton's Law if air is also moving.
    # But if we assume air equilibrates with ambient quickly:
    # t_food_dot = - (t_food - t_amb) / ((r_insulation + r_coupling) * c_food)
    # alpha_true = 1 / ((r_insulation + r_coupling) * c_food)

    r_ins = 40.0
    r_coup = 10.0

    # Read mass ratio from filename or pass it
    mass_ratio = 1.0
    if "Heavy" in csv_file: mass_ratio = 2.0
    if "Light" in csv_file: mass_ratio = 0.5

    c_food = 2000.0 * mass_ratio
    alpha_true = 1.0 / ((r_ins + r_coup) * c_food)

    ambient = 25.0
    if "Hot" in csv_file: ambient = 35.0
    if "Cold" in csv_file: ambient = 15.0

    def calc_true_rem(row):
        t_f = row['true_food']
        if t_f >= 4.0: return 0.0
        ratio = (ambient - 4.0) / (ambient - t_f)
        if ratio <= 0: return 43200.0
        return - (1.0 / alpha_true) * np.log(ratio)

    df['true_remaining'] = df.apply(calc_true_rem, axis=1)

    # We only evaluate when compressor is OFF and door is CLOSED
    eval_points = df[(df['compressor'] == 0) & (df['door'] == 0) & (df['time'] > 3600)]

    if len(eval_points) > 0:
        mae = np.mean(np.abs(eval_points['est'] - eval_points['true_remaining']))
    else:
        mae = 9999

    jumpiness = np.mean(np.abs(df['est'].diff().dropna()))

    return mae, jumpiness

def main():
    scenarios = [
        {"name": "Baseline", "ambient": 25, "mass": 1.0},
        {"name": "Hot",      "ambient": 35, "mass": 1.0},
        {"name": "Cold",     "ambient": 15, "mass": 1.0},
        {"name": "Heavy",    "ambient": 25, "mass": 2.0},
        {"name": "Light",    "ambient": 25, "mass": 0.5},
    ]

    results = []
    duration = 86400 # 24 hours

    for version in ["V1", "V2"]:
        for sc in scenarios:
            filename = f"results_{version}_{sc['name']}_24h.csv"
            run_sim(version, sc['ambient'], sc['mass'], duration, filename)
            mae, jump = analyze(filename)
            results.append({
                "Version": version,
                "Scenario": sc['name'],
                "MAE": mae,
                "Jumpiness": jump,
                "CSV": filename
            })
            print(f"{version} {sc['name']}: MAE={mae:.1f}, Jumpiness={jump:.2f}")

    df_results = pd.DataFrame(results)
    df_results.to_csv("extensive_results_24h.csv", index=False)
    print("\nResults saved to extensive_results_24h.csv")

if __name__ == "__main__":
    main()
