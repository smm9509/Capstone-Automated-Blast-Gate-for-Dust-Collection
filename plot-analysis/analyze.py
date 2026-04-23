import glob
import os
import tempfile

import numpy as np
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

data_dir = os.path.join(os.path.dirname(__file__), "data")
csv_files = glob.glob(os.path.join(data_dir, "*.csv"))

# wiper    = potentiometer reading (actual gate position)
# setpoint = commanded value sent to gate controller
# now_ns   = monotonic time in nanoseconds
# phase    = fractional position within the square wave period (0-1)
columns = ["wiper", "setpoint", "now_ns", "phase", "timestamp"]

dfs = []
for f in csv_files:
    df = pd.read_csv(f, header=None, names=columns)
    df["version"] = os.path.basename(f).removeprefix("monolog_").removesuffix(".csv")
    dfs.append(df)

data = pd.concat(dfs, ignore_index=True)
data["timestamp"] = pd.to_datetime(data["timestamp"])

# Drop IDLE sentinel rows written when the gate controller goes idle
data = data[data["wiper"] != "IDLE"].copy()
data["wiper"] = pd.to_numeric(data["wiper"], errors="coerce")
data["setpoint"] = pd.to_numeric(data["setpoint"], errors="coerce")
data["now_ns"] = pd.to_numeric(data["now_ns"], errors="coerce")

print(data[:5])
print(data.shape)

# --- Step response extraction ---

WINDOW_S = 5.0   # seconds to collect on each side of each step
MAX_PTS = 900    # downsample each segment for file size
MAX_SEGS = 20    # cap overlaid transients per version

COLORS = [
    "#e41a1c", "#377eb8", "#4daf4a", "#984ea3", "#ff7f00",
]

versions = sorted(data["version"].unique())
fig = make_subplots(
    rows=1, cols=2,
    subplot_titles=("Rising Step Response", "Falling Step Response"),
    shared_yaxes=True,
)

for i, version in enumerate(versions):
    color = COLORS[i % len(COLORS)]
    grp = (
        data[data["version"] == version]
        .dropna(subset=["setpoint", "wiper", "now_ns"])
        .sort_values("now_ns")
        .reset_index(drop=True)
    )

    sp = grp["setpoint"].values
    ns = grp["now_ns"].values
    wiper = grp["wiper"].values

    changed = np.concatenate(([False], sp[1:] != sp[:-1]))
    step_idxs = np.where(changed)[0]

    rise_t, rise_w = [], []
    fall_t, fall_w = [], []
    rise_count = fall_count = 0

    for idx in step_idxs:
        if idx == 0:
            continue
        sp_before, sp_after, t_step = sp[idx - 1], sp[idx], ns[idx]
        is_rising = sp_after > sp_before
        if is_rising and rise_count >= MAX_SEGS:
            continue
        if not is_rising and fall_count >= MAX_SEGS:
            continue

        mask = (ns >= t_step - WINDOW_S * 1e9) & (ns <= t_step + WINDOW_S * 1e9)
        t_rel = (ns[mask] - t_step) / 1e9
        w = wiper[mask]

        # Downsample to MAX_PTS
        if len(t_rel) > MAX_PTS:
            idx_ds = np.linspace(0, len(t_rel) - 1, MAX_PTS, dtype=int)
            t_rel, w = t_rel[idx_ds], w[idx_ds]

        # Append segment with a NaN break so traces stay separate
        if is_rising:
            rise_t.extend(t_rel.tolist() + [None])
            rise_w.extend(w.tolist() + [None])
            rise_count += 1
        else:
            fall_t.extend(t_rel.tolist() + [None])
            fall_w.extend(w.tolist() + [None])
            fall_count += 1

    common = dict(
        name=version,
        line=dict(color=color, width=1),
        opacity=0.6,
        legendgroup=version,
    )
    if rise_t:
        fig.add_trace(
            go.Scatter(x=rise_t, y=rise_w, **common),
            row=1, col=1,
        )
    if fall_t:
        fig.add_trace(
            go.Scatter(x=fall_t, y=fall_w, **common, showlegend=not rise_t),
            row=1, col=2,
        )

# Step marker line
for col in (1, 2):
    fig.add_vline(x=0, line=dict(color="black", width=1, dash="dash"), row=1, col=col)

fig.update_xaxes(title_text="Time since step (s)", range=[-0.1, 5.0])
fig.update_yaxes(title_text="Wiper position", col=1)
fig.update_layout(
    title="Blast Gate Step Responses by Version",
    height=500,
    width=1200,
    hovermode="x unified",
)

out = os.path.join(tempfile.mkdtemp(), "step_responses.html")
fig.write_html(out, include_plotlyjs="cdn")
url = f"file://{out}"
# OSC 8 hyperlink — clickable in most modern terminals
print(f"\033]8;;{url}\033\\Open step_responses.html\033]8;;\033\\")
print(f"  ({url})")
