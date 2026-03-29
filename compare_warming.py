import pandas as pd
import numpy as np

def get_warming_slope(csv_file):
    df = pd.read_csv(csv_file)
    # Get the first warming cycle after a compressor turn-off
    df['comp_change'] = df['compressor'].diff()
    ends = df[df['comp_change'] == -1].index
    if len(ends) == 0: return None

    # Take a 100s window starting 60s after compressor off (to avoid transients)
    start_idx = ends[0] + 60
    end_idx = start_idx + 100
    if end_idx >= len(df): return None

    cycle = df.iloc[start_idx:end_idx]
    slope = (cycle['true_air'].iloc[-1] - cycle['true_air'].iloc[0]) / 100.0
    return slope

print(f"Baseline: {get_warming_slope('results_V1_Baseline.csv')}")
print(f"Heavy:    {get_warming_slope('results_V1_Heavy.csv')}")
print(f"Light:    {get_warming_slope('results_V1_Light.csv')}")
print(f"Hot:      {get_warming_slope('results_V1_Hot.csv')}")
print(f"Cold:     {get_warming_slope('results_V1_Cold.csv')}")
