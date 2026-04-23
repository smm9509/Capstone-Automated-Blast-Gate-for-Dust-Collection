import pandas as pd
import glob
import os

data_dir = os.path.join(os.path.dirname(__file__), "data")
csv_files = glob.glob(os.path.join(data_dir, "*.csv"))

columns = ["pos", "response_digits", "now", "angle_turns", "timestamp"]

dfs = []
for f in csv_files:
    df = pd.read_csv(f, header=None, names=columns)
    df["source"] = os.path.basename(f)
    dfs.append(df)

data = pd.concat(dfs, ignore_index=True)
data["timestamp"] = pd.to_datetime(data["timestamp"])

print(data.head())
print(data.shape)
