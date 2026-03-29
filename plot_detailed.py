import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

def calculate_true_remaining(df, scenario):
    r_ins = 40.0
    r_coup = 10.0
    mass_ratio = scenario['mass']
    c_food = 2000.0 * mass_ratio
    alpha_true = 1.0 / ((r_ins + r_coup) * c_food)

    def calc_true_rem(row):
        t_f = row['true_food']
        t_curr = row['time']
        t_amb = scenario['ambient'] + scenario['swing'] * np.sin(2.0 * np.pi * t_curr / 86400.0)
        if t_f >= 4.0: return 0.0
        if t_amb <= 4.1: return 43200.0
        ratio = (t_amb - 4.0) / (t_amb - t_f)
        if ratio <= 0: return 43200.0
        return - (1.0 / alpha_true) * np.log(ratio)

    df['true_remaining'] = df.apply(calc_true_rem, axis=1)
    return df

def plot_scenario(v1_csv, v2_csv, scenario_name, scenario_params):
    df1 = calculate_true_remaining(pd.read_csv(v1_csv), scenario_params)
    df2 = calculate_true_remaining(pd.read_csv(v2_csv), scenario_params)

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(15, 12), sharex=True)

    # Ax1: Temperatures and True Remaining
    ax1.plot(df1['time'], df1['true_food'], label='True Food Temp (C)', color='green', linewidth=2)
    ax1.plot(df1['time'], df1['true_air'], label='True Air Temp (C)', color='gray', linestyle='--', alpha=0.3)

    # Plot Ambient
    t_amb = scenario_params['ambient'] + scenario_params['swing'] * np.sin(2.0 * np.pi * df1['time'] / 86400.0)
    ax1.plot(df1['time'], t_amb, label='Ambient (Diurnal Swing)', color='red', linestyle=':', alpha=0.5)

    ax1.axhline(4.0, color='black', linestyle=':', label='Target')
    ax1.set_ylabel('Temp (C)')
    ax1.legend(loc='upper left')
    ax1.grid(True, alpha=0.3)

    ax1b = ax1.twinx()
    ax1b.plot(df1['time'], df1['true_remaining'], label='True Remaining (s)', color='skyblue', linestyle='--')
    ax1b.plot(df1['time'], df1['est'], label='V1 Est (s)', color='tab:blue')
    ax1b.plot(df2['time'], df2['est'], label='V2 Est (s)', color='tab:orange')
    ax1b.set_ylabel('Remaining Time (s)')
    ax1b.legend(loc='upper right')
    ax1b.set_ylim(0, 30000)

    # Ax2: Internal States
    ax2.plot(df1['time'], df1['alpha'], label='V1 Alpha', color='purple')
    ax2.set_ylabel('Alpha')
    ax2.legend(loc='upper left')

    ax2b = ax2.twinx()
    ax2b.plot(df1['time'], df1['active_rate'], label='V1 Amb Est', color='red', alpha=0.6)
    ax2b.plot(df1['time'], df1['food_proxy'], label='V1 Food Est', color='green', linestyle=':', alpha=0.7)
    ax2b.set_ylabel('Estimator State (C)')
    ax2b.legend(loc='upper right')

    plt.title(f"Detailed 24h Diurnal Analysis: {scenario_name}")
    plt.savefig(f"analysis_{scenario_name.lower()}_24h.png")
    plt.close()

def plot_mae_comparison(results_csv):
    df = pd.read_csv(results_csv)
    pivot_df = df.pivot(index='Scenario', columns='Version', values='MAE')
    pivot_df.plot(kind='bar', figsize=(10, 6))
    plt.title('MAE Comparison: 24h Complex Diurnal Scenarios')
    plt.ylabel('Mean Absolute Error (s)')
    plt.xticks(rotation=45)
    plt.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    plt.savefig('mae_comparison_24h.png')
    plt.close()

# Plotting scripts
scenarios = {
    "Baseline": {"ambient": 25, "mass": 1.0, "swing": 5.0},
    "Heavy": {"ambient": 25, "mass": 2.0, "swing": 5.0},
    "Cold": {"ambient": 15, "mass": 1.0, "swing": 3.0}
}

for name, params in scenarios.items():
    plot_scenario(f'results_V1_{name}_24h_complex.csv', f'results_V2_{name}_24h_complex.csv', name, params)

plot_mae_comparison('extensive_results_24h_complex.csv')
