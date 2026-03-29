import pandas as pd
import matplotlib.pyplot as plt

def plot(csv, title, out):
    df = pd.read_csv(csv)
    df['Time_to_4C'] = 0
    df['cycle'] = (df['compressor'].diff() != 0).cumsum()
    warming_cycles = df[df['compressor'] == 0]
    for cycle_id in warming_cycles['cycle'].unique():
        cycle_df = warming_cycles[warming_cycles['cycle'] == cycle_id].copy()
        hit_points = cycle_df[cycle_df['true_food'] >= 4.0]
        if not hit_points.empty:
            t_hit = hit_points['time'].iloc[0]
            df.loc[df['cycle'] == cycle_id, 'Time_to_4C'] = t_hit - df['time']

    df.loc[df['Time_to_4C'] < 0, 'Time_to_4C'] = 0

    fig, ax1 = plt.subplots(figsize=(12, 6))
    ax1.plot(df['time'], df['true_air'], label='True Air Temp (C)', color='gray', linestyle='--', alpha=0.5)
    ax1.plot(df['time'], df['filtered_temp'], label='Filtered Air (C)', color='red', alpha=0.6)
    ax1.plot(df['time'], df['true_food'], label='True Food (C)', color='green')
    ax1.axhline(4.0, color='black', linestyle=':', label='Target Threshold', alpha=0.5)
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Temperature (C)')
    ax1.legend(loc='upper left')
    ax1.grid(True, linestyle='--', alpha=0.3)

    ax2 = ax1.twinx()
    ax2.plot(df['time'], df['est'], label='Est. Remaining (s)', color='tab:blue', alpha=0.8)
    ax2.plot(df['time'], df['Time_to_4C'], label='True Remaining (s)', color='skyblue', linestyle='--', alpha=0.8)
    ax2.set_ylabel('Time Remaining (s)', color='tab:blue')
    ax2.tick_params(axis='y', labelcolor='tab:blue')
    ax2.set_ylim(0, 4000)
    ax2.legend(loc='upper right')

    plt.title(title)
    plt.savefig(out)
    plt.close()

plot('final_estimator_esp32.csv', 'V1 Adaptive - Final Iteration', 'v1_final.png')
plot('final_estimator2_esp32.csv', 'V2 Physics - Final Iteration', 'v2_final.png')
