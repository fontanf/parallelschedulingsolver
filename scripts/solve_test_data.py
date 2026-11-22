"""Compute reference optimal values of test instances.

For each instance of the given instance lists, write "<instance path> <value>"
to the output file if the optimal value is found. Two methods:

* '--algorithm <name>': run an algorithm of the solver (as in
  shopschedulingsolver) and keep the instances on which it proves the
  optimality of its solution (output "Optimal": true). Algorithms prove
  optimality in the special cases in which they are exact, e.g.
  'assignment-total-flow-time' for R || sum C_j or 'greedy-earliest-due-date'
  for P | p_j = p | L_max, so this works on large instances. The value is the
  objective of the instance.

* '--criterion <name>': run the 'brute-force' algorithm of the solver on a
  copy of the instance whose objective is the criterion ('objective': the
  objective of the instance; 'number-of-on-time-jobs': the throughput with
  all the weights equal to 1). Instances with more than
  '--maximum-number-of-candidates' candidates are skipped. Use it to test the
  algorithms that prove optimality themselves, and for criteria that differ
  from the objective of the instance. See
  include/parallelschedulingsolver/algorithms/brute_force.hpp for the
  enumeration.

The solver must be installed in 'install/'.

Examples:
python3 scripts/solve_test_data.py --criterion total-flow-time --instances data/test_tft_single.txt data/test_tft_identical.txt --output test/algorithms/assignment_total_flow_time_test.txt
python3 scripts/solve_test_data.py --algorithm assignment-total-flow-time --instances data/test_tft_unrelated.txt --output tft_unrelated.txt
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile


CRITERIA = [
        "objective",
        "makespan",
        "total-flow-time",
        "total-tardiness",
        "maximum-lateness",
        "number-of-on-time-jobs",
        "throughput"]

# Objective of the instance copy solved for a criterion.
CRITERION_OBJECTIVES = {
        "makespan": "Makespan",
        "total-flow-time": "Total flow time",
        "total-tardiness": "Total tardiness",
        "maximum-lateness": "Maximum lateness",
        "number-of-on-time-jobs": "Throughput",
        "throughput": "Throughput"}


def solve_with_algorithm(instance_full_path, algorithm, time_limit, arguments=()):
    """Value of the solution of the algorithm if it proves its optimality,
    None otherwise. No time limit if time_limit is None."""
    main = os.path.join("install", "bin", "parallelschedulingsolver")
    with tempfile.TemporaryDirectory() as tmp_dir:
        json_output_path = os.path.join(tmp_dir, "output.json")
        command = [
                main,
                "--verbosity-level", "0",
                "--input", instance_full_path,
                "--format", "json",
                "--algorithm", algorithm,
                "--output", json_output_path] + list(arguments)
        if time_limit is not None:
            command += ["--time-limit", str(time_limit)]
        print(" ".join(command), file=sys.stderr)
        subprocess.run(command, check=True)
        with open(json_output_path, "r") as json_output_file:
            json_data = json.load(json_output_file)
    if not bool(json_data["Output"]["Optimal"]):
        return None
    return int(json_data["Output"]["Value"])


def solve_brute_force(instance_full_path, criterion, maximum_number_of_candidates):
    """Optimal value of the criterion, None if the instance is too large."""
    with open(instance_full_path, 'r') as instance_file:
        instance = json.load(instance_file)
    if criterion != "objective":
        instance["objective"] = CRITERION_OBJECTIVES[criterion]
    if criterion == "number-of-on-time-jobs":
        for job in instance["jobs"]:
            job["weight"] = 1
    with tempfile.TemporaryDirectory() as tmp_dir:
        instance_copy_path = os.path.join(tmp_dir, "instance.json")
        with open(instance_copy_path, 'w') as instance_copy_file:
            json.dump(instance, instance_copy_file)
        return solve_with_algorithm(
                instance_copy_path,
                "brute-force",
                None,
                ["--maximum-number-of-candidates",
                 str(maximum_number_of_candidates)])


parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
method = parser.add_mutually_exclusive_group(required=True)
method.add_argument(
        "--algorithm",
        type=str,
        help="algorithm of the solver to run")
method.add_argument(
        "--criterion",
        type=str,
        choices=CRITERIA,
        help="criterion to optimize by brute force; 'objective': the objective of each instance file")
parser.add_argument(
        "--instances",
        type=str,
        nargs='+',
        required=True,
        help="files containing lists of instance paths, relative to 'data'")
parser.add_argument(
        "--output",
        type=str,
        required=True,
        help="output test file")
parser.add_argument(
        "--maximum-number-of-candidates",
        type=int,
        default=1000000,
        help="brute force: maximum number of candidates (sequences or assignments) enumerated per instance")
parser.add_argument(
        "--time-limit",
        type=float,
        default=1,
        help="algorithm: time limit in seconds")
parser.add_argument(
        "--keep-if-solved-by",
        type=str,
        help="only keep the instances on which this algorithm proves the optimality of the reference value within '--time-limit' (used to build the test lists of slow exact algorithms, and of the brute force); fail if it proves a different value")
parser.add_argument(
        "--keep-if-solved-with",
        type=str,
        action="append",
        help="with '--keep-if-solved-by': arguments of the algorithm, e.g. '--normal-instants 0'; repeat it to require each set of arguments")
args = parser.parse_args()


def keep_if_solved(instance_full_path):
    """With '--keep-if-solved-by': the value proved by the algorithm with each
    set of arguments, or False if it doesn't prove optimality with one of
    them (the instance is skipped). Without: None."""
    if args.keep_if_solved_by is None:
        return None
    values = set()
    for arguments in (args.keep_if_solved_with or [""]):
        algorithm_value = solve_with_algorithm(
                instance_full_path,
                args.keep_if_solved_by,
                args.time_limit,
                arguments.split())
        if algorithm_value is None:
            print(f"{instance_full_path}: skipped (not solved by {args.keep_if_solved_by} {arguments})", file=sys.stderr)
            return False
        values.add(algorithm_value)
    if len(values) > 1:
        sys.exit(f"{instance_full_path}: {args.keep_if_solved_by}: different values {sorted(values)}")
    return values.pop()

with open(args.output, 'w') as output_file:
    for instances_path in args.instances:
        with open(instances_path, 'r') as instances_file:
            for line in instances_file:
                instance_base_path = line.strip()
                if not instance_base_path:
                    continue
                instance_full_path = os.path.join("data", instance_base_path)
                if args.algorithm is not None:
                    # Check the other algorithm first: it is the fast one
                    # (e.g. brute force on small instances).
                    kept_value = keep_if_solved(instance_full_path)
                    if kept_value is False:
                        continue
                    value = solve_with_algorithm(
                            instance_full_path,
                            args.algorithm,
                            args.time_limit)
                    if value is None:
                        print(f"{instance_base_path}: skipped (not proved optimal)", file=sys.stderr)
                        continue
                    if kept_value is not None and kept_value != value:
                        sys.exit(f"{instance_base_path}: {args.keep_if_solved_by}: value {kept_value} differs from reference {value}")
                else:
                    value = solve_brute_force(
                            instance_full_path,
                            args.criterion,
                            args.maximum_number_of_candidates)
                    if value is None:
                        print(f"{instance_base_path}: skipped (too large)", file=sys.stderr)
                        continue
                    kept_value = keep_if_solved(instance_full_path)
                    if kept_value is False:
                        continue
                    if kept_value is not None and kept_value != value:
                        sys.exit(f"{instance_base_path}: {args.keep_if_solved_by}: value {kept_value} differs from reference {value}")
                print(f"{instance_base_path} {value}")
                output_file.write(f"{instance_base_path} {value}\n")
