import pandas as pd
import numpy as np

def analyze_cooling_phases(csv_file):
    df = pd.read_csv(csv_file)
    df['comp_change'] = df['compressor'].diff()
    starts = df[df['comp_change'] == 1].index
    ends = df[df['comp_change'] == -1].index

    results = []
    for i in range(min(len(starts), len(ends))):
        cycle = df.loc[starts[i]:ends[i]]
        if len(cycle) < 20: continue

        # Initial slope (first 5 seconds)
        s0 = (cycle['true_air'].iloc[5] - cycle['true_air'].iloc[0]) / 5.0
        # Terminal slope (last 5 seconds)
        sT = (cycle['true_air'].iloc[-1] - cycle['true_air'].iloc[-6]) / 5.0

        results.append((s0, sT))
    return results

print("Baseline:", analyze_cooling_phases('results_V1_Baseline.csv'))
print("Heavy:   ", analyze_cooling_phases('results_V1_Heavy.csv'))
print("Light:   ", analyze_cooling_phases('results_V1_Light.csv'))
