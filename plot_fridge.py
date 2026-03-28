import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_csv(csv_file, title, output_file):
    df = pd.read_csv(csv_file)

    fig, ax1 = plt.subplots(figsize=(12, 6))

    # Plot Temperature
    ax1.plot(df['time'], df['temp'], color='tab:red', label='Air Temp (C)', alpha=0.8)
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Temperature (C)', color='tab:red')
    ax1.tick_params(axis='y', labelcolor='tab:red')
    ax1.grid(True, linestyle='--', alpha=0.6)

    # Plot Estimate on secondary axis
    ax2 = ax1.twinx()
    ax2.plot(df['time'], df['est'], color='tab:blue', label='Est. Remaining (s)', alpha=0.7)
    ax2.set_ylabel('Est. Time Remaining (s)', color='tab:blue')
    ax2.tick_params(axis='y', labelcolor='tab:blue')
    ax2.set_ylim(-10, 2100)

    # Shade Door Open periods
    door_open = df[df['door'] == 1]
    if not door_open.empty:
        # Simplistic shading: just scatter at y=0
        ax1.scatter(door_open['time'], [0]*len(door_open), color='orange', label='Door Open', marker='s', s=10)

    plt.title(title)
    fig.tight_layout()
    plt.savefig(output_file)
    print(f"Saved plot to {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        # Default for the two files
        plot_csv('simulator/data_v1_rev2.csv', 'Estimator 1 (Adaptive) - Rev 2', 'graph_v1_rev2.png')
        plot_csv('simulator/data_v2_rev2.csv', 'Estimator 2 (Physics) - Rev 2', 'graph_v2_rev2.png')
    else:
        plot_csv(sys.argv[1], sys.argv[2], sys.argv[3])
