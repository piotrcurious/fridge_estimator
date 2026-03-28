import subprocess
import pandas as pd
import numpy as np
import os
import json

def run_sim(version, ambient, mass, output_file):
    print(f"Running {version} (Ambient: {ambient}C, Mass: {mass}x)...")

    cmd = ["g++", "-O3", "simulator/main.cpp", "-o", "simulator/sim", "-Isimulator/"]
    if version == "V2":
        cmd.append("-DUSE_V2")

    # Compile
    subprocess.run(cmd, check=True, capture_output=True)

    # Run
    subprocess.run(["./simulator/sim", "--out", output_file, "--ambient", str(ambient), "--mass", str(mass)],
                   check=True, capture_output=True)

def analyze(csv_file):
    df = pd.read_csv(csv_file)

    # Calculate true remaining time
    df['cycle'] = (df['compressor'].diff() != 0).cumsum()
    warming_cycles = df[df['compressor'] == 0]

    maes = []

    for cycle_id in warming_cycles['cycle'].unique():
        cycle_df = warming_cycles[warming_cycles['cycle'] == cycle_id].copy()
        if cycle_df.empty: continue
        target = cycle_df['target'].iloc[0]

        # Find time where it first hits target
        hit_points = cycle_df[cycle_df['true_food'] >= target]
        if hit_points.empty:
            continue

        t_hit = hit_points['time'].iloc[0]
        cycle_df['true_remaining'] = t_hit - cycle_df['time']
        cycle_df.loc[cycle_df['true_remaining'] < 0, 'true_remaining'] = 0

        # We only evaluate when it's warming and we haven't hit target yet
        # AND when we are at least 5 mins into the cycle
        eval_points = cycle_df[(cycle_df['true_remaining'] > 0) & (cycle_df['time'] > cycle_df['time'].min() + 300)]

        if len(eval_points) > 0:
            mae = np.mean(np.abs(eval_points['est'] - eval_points['true_remaining']))
            maes.append(mae)

    avg_mae = np.mean(maes) if maes else 9999
    jumpiness = np.mean(np.abs(df['est'].diff().dropna()))

    return avg_mae, jumpiness

def main():
    scenarios = [
        {"name": "Baseline", "ambient": 25, "mass": 1.0},
        {"name": "Hot",      "ambient": 35, "mass": 1.0},
        {"name": "Cold",     "ambient": 15, "mass": 1.0},
        {"name": "Heavy",    "ambient": 25, "mass": 2.0},
        {"name": "Light",    "ambient": 25, "mass": 0.5},
    ]

    results = []

    for version in ["V1", "V2"]:
        for sc in scenarios:
            filename = f"results_{version}_{sc['name']}.csv"
            run_sim(version, sc['ambient'], sc['mass'], filename)
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
    df_results.to_csv("extensive_results.csv", index=False)
    print("\nResults saved to extensive_results.csv")

if __name__ == "__main__":
    main()
