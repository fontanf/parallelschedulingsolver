"""Analyze the results of scripts/run_benchmarks.py.

Run with:
    streamlit run scripts/process_benchmarks.py

For the selected benchmark, it shows, for each instance, the reference values
of the CSV file and the solution value and the gap to the best known solution
value of each run (each directory of benchmark_results/<benchmark>/), and,
for each group of instances, the average gap and the number of best known
solution values reached.

With several runs of the same algorithm (e.g. with different seeds), the
"Best run" and "Average" columns aggregate them, as in the tables of the
papers.
"""

import csv
import json
import os

import pandas as pd
import streamlit as st

from run_benchmarks import BENCHMARKS


st.set_page_config(layout='wide')
st.title("ParallelSchedulingSolver benchmarks analyzer")

bksv_field = "Best known solution value"
bkb_field = "Best known bound"

# For each benchmark: the columns of the CSV file defining the groups of the
# summary.
GROUP_COLUMNS = {
    "identical_tt_tanaka2008": ["Number of jobs", "Number of machines"],
    "single_twt_setup_times_cicirello2003": ["Tau", "R", "Eta"],
    "unrelated_makespan_setup_times_vallada2011": ["Size", "Number of jobs", "Number of machines"],
}


def gap(value, reference):
    """Gap in percent; with a reference of 0, the gap is relative to 1."""
    return (value - reference) / max(reference, 1) * 100


def show_dataframe(df):
    st.dataframe(
            df,
            width='stretch',
            height=min(len(df.index) + 1, 40) * 35 + 3)


benchmarks = [
    f
    for f in sorted(os.listdir("benchmark_results"))
    if os.path.isdir(os.path.join("benchmark_results", f))
    and f in BENCHMARKS]

benchmark = st.selectbox("Benchmark", benchmarks)
if benchmark is None:
    st.stop()

benchmark_directory = os.path.join("benchmark_results", benchmark)
output_directories = sorted(
    f
    for f in os.listdir(benchmark_directory)
    if os.path.isdir(os.path.join(benchmark_directory, f)))
output_directories = st.multiselect(
        "Runs",
        output_directories,
        default=output_directories)

datacsv_path = os.path.join("data", BENCHMARKS[benchmark]["csv"])
dataset = BENCHMARKS[benchmark].get("dataset")

rows = []
with open(datacsv_path, newline='') as csvfile:
    for row in csv.DictReader(csvfile):
        if dataset is not None and row["Dataset"] != dataset:
            continue
        out = dict(row)
        bks = int(row[bksv_field])
        bkb = int(row[bkb_field]) if row[bkb_field] != "" else None
        values = []
        for output_directory in output_directories:
            json_path = os.path.join(
                    benchmark_directory,
                    output_directory,
                    row["Path"] + "_output.json")
            value = None
            time = None
            if os.path.exists(json_path):
                with open(json_path) as json_file:
                    output = json.load(json_file)["Output"]
                # Feasible solution: all the jobs are scheduled.
                if output["Solution"]["NumberOfJobs"] == int(row["Number of jobs"]):
                    value = int(output["Value"])
                    time = output["Time"]
            out[output_directory + " / Solution value"] = value
            out[output_directory + " / Gap (%)"] = (
                    gap(value, bks) if value is not None else None)
            out[output_directory + " / Time (s)"] = time
            if value is not None:
                values.append(value)
        if values:
            out["Best run / Solution value"] = min(values)
            out["Best run / Gap (%)"] = gap(min(values), bks)
            out["Average / Gap (%)"] = sum(gap(v, bks) for v in values) / len(values)
            out["Number of runs reaching the BKS"] = sum(v <= bks for v in values)
            out["Number of runs reaching the bound"] = (
                    sum(v <= bkb for v in values) if bkb is not None else None)
        rows.append(out)

df = pd.DataFrame(rows)

st.header("Summary")
group_columns = GROUP_COLUMNS.get(benchmark, [])
aggregations = {"Path": "count"}
for column in df.columns:
    if column.endswith(" / Gap (%)") or column.endswith(" / Time (s)"):
        aggregations[column] = "mean"
    if column.startswith("Number of runs"):
        aggregations[column] = "sum"
summary = (
        df.groupby(group_columns).agg(aggregations)
        if group_columns
        else df.agg(aggregations).to_frame().T)
summary = summary.rename(columns={"Path": "Number of instances"})
total = df.agg(aggregations).to_frame().T.rename(
        columns={"Path": "Number of instances"})
total.index = ["Total"]
if group_columns:
    summary.index = summary.index.map(
            lambda key: " / ".join(str(k) for k in (key if isinstance(key, tuple) else (key,))))
    summary = pd.concat([summary, total])
show_dataframe(summary.round(3))

st.header("Instances")
show_dataframe(df.round(3))
