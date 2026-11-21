"""Run an algorithm on the instances of a benchmark.

The instances of a benchmark are listed in a CSV file of the data directory
(data/data_<problem>.csv), with their format, time limit and reference values.
For each instance, the output and the solution are written in
benchmark_results/<benchmark>/<name>/, where <name> defaults to the date and
the algorithm. The results are analyzed with scripts/process_benchmarks.py.

Example:
    python3 scripts/run_benchmarks.py identical_tt_tanaka2008 local-search --options "--seed 0"
"""

import argparse
import csv
import datetime
import os
import sys


# For each benchmark: the CSV file listing its instances and, optionally, the
# value of the "Dataset" column of its instances.
BENCHMARKS = {
    "identical_tt_tanaka2008": {
        "csv": "data_identical_tt.csv",
        "dataset": "tanaka2008",
    },
    "single_twt_setup_times_cicirello2003": {
        "csv": "data_single_twt_setup_times.csv",
        "dataset": "cicirello2003",
    },
    "unrelated_makespan_setup_times_vallada2011": {
        "csv": "data_unrelated_makespan_setup_times.csv",
        "dataset": "vallada2011",
    },
}


def run_command(command):
    print(command)
    status = os.system(command)
    if status != 0:
        sys.exit(1)
    print()


main = os.path.join(
        "install",
        "bin",
        "parallelschedulingsolver")


if __name__ == "__main__":

    parser = argparse.ArgumentParser(
            prog='run_benchmarks',
            usage='%(prog)s [options]')
    parser.add_argument(
            'benchmark',
            help='benchmark to run',
            choices=sorted(BENCHMARKS.keys()))
    parser.add_argument('algorithm', help='algorithm')
    parser.add_argument(
            '--directory',
            help='benchmark directory',
            default="benchmark_results")
    parser.add_argument('--name', help='output directory', default=None)
    parser.add_argument('--options', help='options', default="")
    parser.add_argument(
            '--time-limit-factor',
            help='factor applied to the time limits of the CSV file',
            type=float,
            default=1.0)
    args = parser.parse_args()

    benchmark = BENCHMARKS[args.benchmark]
    output_directory = os.path.join(args.directory, args.benchmark)
    if args.name:
        output_directory = os.path.join(output_directory, args.name)
    else:
        date = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        output_directory = os.path.join(
                output_directory,
                date + "_" + args.algorithm)

    datacsv_path = os.path.join("data", benchmark["csv"])
    data_dir = os.path.dirname(os.path.realpath(datacsv_path))
    with open(datacsv_path, newline='') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:

            if "dataset" in benchmark \
                    and row["Dataset"] != benchmark["dataset"]:
                continue

            instance_path = os.path.join(
                    data_dir,
                    row["Path"])

            time_limit = float(row["Time limit"]) * args.time_limit_factor

            json_output_path = os.path.join(
                    output_directory,
                    row["Path"] + "_output.json")
            if not os.path.exists(os.path.dirname(json_output_path)):
                os.makedirs(os.path.dirname(json_output_path))

            certificate_path = os.path.join(
                    output_directory,
                    row["Path"] + "_solution.json")

            command = (
                    f"{main}"
                    f"  --verbosity-level 1"
                    f"  --input \"{instance_path}\""
                    f"  --format \"{row['Format']}\""
                    f"  --algorithm {args.algorithm}"
                    f"  --time-limit {time_limit}"
                    f"  {args.options}"
                    f"  --output \"{json_output_path}\""
                    f"  --certificate \"{certificate_path}\"")
            run_command(command)
