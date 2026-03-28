import pandas as pd
import matplotlib.pyplot as plt
import os

def plot_data(csv_path, output_path, title):
    if not os.path.exists(csv_path):
        print(f"File {csv_path} not found.")
        return
    # Use lowercase as found in the CSV
    df = pd.read_csv(csv_path)

    fig, ax1 = plt.subplots(figsize=(12, 6))

    color = 'tab:red'
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Temperature (C)', color=color)
    ax1.plot(df['time'], df['temp'], color=color, label='Actual Temp')
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.grid(True, linestyle='--', alpha=0.6)

    ax2 = ax1.twinx()
    color = 'tab:blue'
    ax2.set_ylabel('Est. Time Remaining (s)', color=color)
    # Filter out very large values for better plotting
    df['est_plot'] = df['est'].apply(lambda x: x if x < 2000 else 2000)
    ax2.plot(df['time'], df['est_plot'], color=color, alpha=0.7, label='Time Est')
    ax2.tick_params(axis='y', labelcolor=color)

    if 'door' in df.columns:
        # Plot door open events as small marks at the bottom
        door_open = df[df['door'] == 1]
        ax1.scatter(door_open['time'], [0]*len(door_open), color='orange', marker='s', s=10, label='Door Open', zorder=5)

    plt.title(title)
    fig.tight_layout()
    plt.savefig(output_path)
    print(f"Saved plot to {output_path}")

if __name__ == "__main__":
    plot_data('simulator/data_v1_rev.csv', 'graph_v1_rev.png', 'Estimator 1 (Improved Smoothing & Learning)')
    plot_data('simulator/data_v2_rev.csv', 'graph_v2_rev.png', 'Estimator 2 (Improved Smoothing & Physics)')
