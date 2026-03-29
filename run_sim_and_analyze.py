import subprocess
import pandas as pd
import numpy as np
import os

def run_sim(version, output_file):
    print(f"Building and running {version}...")

    cmd = ["g++", "-O3", "simulator/main.cpp", "-o", "simulator/sim", "-Isimulator/"]
    if version == "estimator2_esp32":
        cmd.append("-DUSE_V2")

    # Compile
    subprocess.run(cmd, check=True)

    # Run
    with open(output_file, "w") as f:
        subprocess.run(["./simulator/sim"], stdout=f, check=True)

def analyze(csv_file):
    df = pd.read_csv(csv_file)

    # Calculate true remaining time by finding the first point where true_food >= target in future
    # Group by warming cycles (compressor=0)
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
    versions = ["estimator_esp32", "estimator2_esp32"]
    results = {}

    for v in versions:
        out = f"final_{v}.csv"
        run_sim(v, out)
        mae, jump = analyze(out)
        results[v] = {"MAE": mae, "Jumpiness": jump}
        print(f"{v}: MAE={mae:.2f}, Jumpiness={jump:.2f}")

if __name__ == "__main__":
    main()
