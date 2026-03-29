import subprocess
import pandas as pd
import numpy as np
import os
import json

def run_sim(version, ambient, mass, duration, swing, output_file):
    print(f"Running {version} (Amb: {ambient}C, Swing: {swing}C, Mass: {mass}x, Dur: {duration}s)...")

    cmd = ["g++", "-O3", "simulator/main.cpp", "-o", "simulator/sim", "-Isimulator/"]
    if version == "V2":
        cmd.append("-DUSE_V2")

    subprocess.run(cmd, check=True, capture_output=True)

    subprocess.run(["./simulator/sim", "--out", output_file, "--ambient", str(ambient), "--mass", str(mass), "--duration", str(duration), "--swing", str(swing)],
                   check=True, capture_output=True)

def analyze(csv_file, scenario):
    df = pd.read_csv(csv_file)

    r_ins = 40.0
    r_coup = 10.0
    mass_ratio = scenario['mass']
    c_food = 2000.0 * mass_ratio
    alpha_true = 1.0 / ((r_ins + r_coup) * c_food)

    def calc_true_rem(row):
        t_f = row['true_food']
        # For true remaining time, we use the CURRENT ambient temperature
        # In the simulator, getAmbient() = base + swing * sin(...)
        # We need the time to hit 4C if cooling stopped NOW.
        # T(t_rem) = T_amb - (T_amb - T_food) * exp(-alpha * t_rem) = 4

        # We need to compute the actual ambient at this specific time
        t_curr = row['time']
        t_amb = scenario['ambient'] + scenario['swing'] * np.sin(2.0 * np.pi * t_curr / 86400.0)

        if t_f >= 4.0: return 0.0
        if t_amb <= 4.1: return 43200.0 # Never hits 4C

        ratio = (t_amb - 4.0) / (t_amb - t_f)
        if ratio <= 0: return 43200.0
        return - (1.0 / alpha_true) * np.log(ratio)

    df['true_remaining'] = df.apply(calc_true_rem, axis=1)

    # Eval points: Compressor OFF, Door CLOSED, after 1 hour stabilization
    eval_points = df[(df['compressor'] == 0) & (df['door'] == 0) & (df['time'] > 3600)].copy()

    if len(eval_points) > 0:
        mae = np.mean(np.abs(eval_points['est'] - eval_points['true_remaining']))
    else:
        mae = 9999

    jumpiness = np.mean(np.abs(df['est'].diff().dropna()))

    return mae, jumpiness

def main():
    scenarios = [
        {"name": "Baseline", "ambient": 25, "mass": 1.0, "swing": 5.0},
        {"name": "Hot",      "ambient": 35, "mass": 1.0, "swing": 5.0},
        {"name": "Cold",     "ambient": 15, "mass": 1.0, "swing": 3.0},
        {"name": "Heavy",    "ambient": 25, "mass": 2.0, "swing": 5.0},
        {"name": "Light",    "ambient": 25, "mass": 0.5, "swing": 5.0},
    ]

    results = []
    duration = 86400 # 24 hours

    for version in ["V1", "V2"]:
        for sc in scenarios:
            filename = f"results_{version}_{sc['name']}_24h_complex.csv"
            run_sim(version, sc['ambient'], sc['mass'], duration, sc['swing'], filename)
            mae, jump = analyze(filename, sc)
            results.append({
                "Version": version,
                "Scenario": sc['name'],
                "MAE": mae,
                "Jumpiness": jump,
                "CSV": filename
            })
            print(f"{version} {sc['name']}: MAE={mae:.1f}, Jumpiness={jump:.2f}")

    df_results = pd.DataFrame(results)
    df_results.to_csv("extensive_results_24h_complex.csv", index=False)
    print("\nResults saved to extensive_results_24h_complex.csv")

if __name__ == "__main__":
    main()
