import glob
import os

import numpy as np
import pandas as pd

data_dir = os.path.join(os.path.dirname(__file__), "data")
csv_files = glob.glob(os.path.join(data_dir, "*.csv"))

columns = ["wiper", "setpoint", "now_ns", "phase", "timestamp"]

dfs = []
for f in csv_files:
    df = pd.read_csv(f, header=None, names=columns)
    df["version"] = os.path.basename(f).removeprefix("monolog_").removesuffix(".csv")
    dfs.append(df)

data = pd.concat(dfs, ignore_index=True)

data = data[data["wiper"] != "IDLE"].copy()
data["wiper"] = pd.to_numeric(data["wiper"], errors="coerce")
data["now_ns"] = pd.to_numeric(data["now_ns"], errors="coerce")

WIN_LOW = 200  # bottom of counting window
WIN_HIGH = 300  # top of counting window
MAX_TRAVERSE_S = 5.0

total_opens = 0
total_closes = 0

versions = sorted(data["version"].unique())

for version in versions:
    grp = (
        data[data["version"] == version]
        .dropna(subset=["wiper", "now_ns"])
        .sort_values("now_ns")
        .reset_index(drop=True)
    )

    w = grp["wiper"].values
    ns = grp["now_ns"].values

    opens = 0
    closes = 0

    # State machine: track entry into window from each side
    # entry_time_ns = when wiper first entered the window on the current pass
    # entry_side = "low" (came from below WIN_LOW) or "high" (came from above WIN_HIGH)
    entry_time_ns = None
    entry_side = None

    for i in range(1, len(w)):
        prev, curr = w[i - 1], w[i]

        in_window_prev = WIN_LOW <= prev <= WIN_HIGH
        in_window_curr = WIN_LOW <= curr <= WIN_HIGH

        # Entering window from below
        if not in_window_prev and prev < WIN_LOW and in_window_curr:
            entry_time_ns = ns[i]
            entry_side = "low"

        # Entering window from above
        elif not in_window_prev and prev > WIN_HIGH and in_window_curr:
            entry_time_ns = ns[i]
            entry_side = "high"

        # Exiting window through the top (open)
        elif in_window_prev and not in_window_curr and curr > WIN_HIGH:
            if entry_side == "low" and entry_time_ns is not None:
                elapsed_s = (ns[i] - entry_time_ns) / 1e9
                if elapsed_s < MAX_TRAVERSE_S and elapsed_s > 0:
                    opens += 1
            entry_time_ns = None
            entry_side = None

        # Exiting window through the bottom (close)
        elif in_window_prev and not in_window_curr and curr < WIN_LOW:
            if entry_side == "high" and entry_time_ns is not None:
                elapsed_s = (ns[i] - entry_time_ns) / 1e9
                if elapsed_s < MAX_TRAVERSE_S:
                    closes += 1
            entry_time_ns = None
            entry_side = None

        # Left window without completing traversal (turned back) — reset
        elif in_window_prev and not in_window_curr:
            entry_time_ns = None
            entry_side = None

    total_opens += opens
    total_closes += closes
    print(f"  {version:30s}  opens={opens:5d}  closes={closes:5d}")

print()
print(
    f"  {'TOTAL':30s}  opens={total_opens:5d}  closes={total_closes:5d}  combined={total_opens + total_closes:5d}"
)
