import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def calculate_true_remaining(df):
    df['cycle'] = (df['compressor'].diff() != 0).cumsum()
    warming_cycles = df[df['compressor'] == 0]
    df['true_remaining'] = 0.0
    for cycle_id in warming_cycles['cycle'].unique():
        cycle_df = warming_cycles[warming_cycles['cycle'] == cycle_id].copy()
        target = cycle_df['target'].iloc[0]
        hit_points = cycle_df[cycle_df['true_food'] >= target]
        if not hit_points.empty:
            t_hit = hit_points['time'].iloc[0]
            df.loc[df['cycle'] == cycle_id, 'true_remaining'] = t_hit - df['time']
    df.loc[df['true_remaining'] < 0, 'true_remaining'] = 0
    return df

def plot_scenario(v1_csv, v2_csv, scenario_name):
    df1 = calculate_true_remaining(pd.read_csv(v1_csv))
    df2 = calculate_true_remaining(pd.read_csv(v2_csv))

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 12), sharex=True)

    # Ax1: Temperatures and True Remaining
    ax1.plot(df1['time'], df1['true_food'], label='True Food Temp (C)', color='green', linewidth=2)
    ax1.plot(df1['time'], df1['true_air'], label='True Air Temp (C)', color='gray', linestyle='--', alpha=0.3)
    ax1.axhline(df1['target'].iloc[0], color='black', linestyle=':', label='Target')
    ax1.set_ylabel('Temp (C)')
    ax1.legend(loc='upper left')
    ax1.grid(True, alpha=0.3)

    ax1b = ax1.twinx()
    ax1b.plot(df1['time'], df1['true_remaining'], label='True Remaining (s)', color='skyblue', linestyle='--')
    ax1b.plot(df1['time'], df1['est'], label='V1 Est (s)', color='tab:blue')
    ax1b.plot(df2['time'], df2['est'], label='V2 Est (s)', color='tab:orange')
    ax1b.set_ylabel('Remaining Time (s)')
    ax1b.legend(loc='upper right')
    ax1b.set_ylim(0, 15000)

    # Ax2: Internal States (Alpha and Confidence for V1)
    ax2.plot(df1['time'], df1['alpha'], label='V1 Adaptive Alpha', color='purple')
    ax2.set_ylabel('Alpha', color='purple')
    ax2.tick_params(axis='y', labelcolor='purple')

    ax2b = ax2.twinx()
    ax2b.plot(df1['time'], df1['confidence'], label='V1 Confidence', color='brown', alpha=0.5)
    ax2b.set_ylabel('Confidence', color='brown')
    ax2b.set_ylim(0, 1.1)

    plt.title(f"Detailed Analysis: {scenario_name}")
    plt.savefig(f"analysis_{scenario_name.lower()}.png")
    plt.close()

def plot_mae_comparison(results_csv):
    df = pd.read_csv(results_csv)
    # Pivot for plotting
    pivot_df = df.pivot(index='Scenario', columns='Version', values='MAE')

    pivot_df.plot(kind='bar', figsize=(10, 6))
    plt.title('MAE Comparison across Scenarios')
    plt.ylabel('Mean Absolute Error (s)')
    plt.xticks(rotation=45)
    plt.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    plt.savefig('mae_comparison.png')
    plt.close()

def plot_residuals(results_csv):
    df_results = pd.read_csv(results_csv)
    plt.figure(figsize=(12, 8))

    for _, row in df_results[df_results['Version'] == 'V1'].iterrows():
        df = calculate_true_remaining(pd.read_csv(row['CSV']))
        # Filter for evaluation points
        eval_df = df[(df['true_remaining'] > 0) & (df['time'] > df['time'].min() + 300)]
        plt.plot(eval_df['time'], eval_df['est'] - eval_df['true_remaining'], label=row['Scenario'])

    plt.axhline(0, color='black', linestyle='--')
    plt.title('V1 Estimation Error (Residuals) over Time')
    plt.xlabel('Time (s)')
    plt.ylabel('Error (Est - True) [s]')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig('v1_residuals.png')
    plt.close()

# Plot Baseline
plot_scenario('results_V1_Baseline.csv', 'results_V2_Baseline.csv', 'Baseline')
# Plot Heavy
plot_scenario('results_V1_Heavy.csv', 'results_V2_Heavy.csv', 'Heavy')
# Plot MAE comparison
plot_mae_comparison('extensive_results.csv')
# Plot Residuals
plot_residuals('extensive_results.csv')
