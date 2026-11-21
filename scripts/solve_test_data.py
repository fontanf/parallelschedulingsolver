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

* '--criterion <name>': brute force; instances whose enumeration would exceed
  '--maximum-size' are skipped. Use it to test the algorithms that prove
  optimality themselves, and for criteria that differ from the objective of
  the instance.

  Single machine: all job sequences are enumerated (jobs are started as early
  as possible, which is dominant for the supported regular criteria).

  With sequence-dependent setup times, the sequences of all the machines are
  enumerated, as all the job orders split into one sequence per machine.
  The setup of a job is performed right before it and may be performed before
  its release date.

  Parallel machines: all job-to-machine assignments are enumerated, and the
  jobs of each machine are sequenced with a rule that is optimal for a fixed
  assignment:
  * makespan: earliest release date first
  * total-flow-time: weighted shortest processing time first (requires all
    release dates to be 0)
  * maximum-lateness: earliest due date first (requires all release dates to
    be 0)

Examples:
python3 scripts/solve_test_data.py --criterion total-flow-time --instances data/test_tft_single.txt data/test_tft_parallel.txt --output test/algorithms/assignment_total_flow_time_test.txt
python3 scripts/solve_test_data.py --algorithm assignment-total-flow-time --instances data/test_tft_parallel.txt --output tft_parallel_optimal.txt
"""

import argparse
import itertools
import json
import math
import os
import subprocess
import sys
import tempfile


# Criterion of the objective of an instance file.
OBJECTIVE_CRITERIA = {
        "Makespan": "makespan",
        "Total flow time": "total-flow-time",
        "Total tardiness": "total-tardiness",
        "Maximum lateness": "maximum-lateness",
        "Throughput": "throughput"}

CRITERIA = [
        "objective",
        "makespan",
        "total-flow-time",
        "total-tardiness",
        "maximum-lateness",
        "number-of-on-time-jobs",
        "throughput"]

MAXIMIZE = {"number-of-on-time-jobs", "throughput"}


def setup_time(setup_times, machine_id, job_id_1, job_id_2):
    """Setup time when job_id_2 follows job_id_1 (-1 for the first job)."""
    if setup_times is None:
        return 0
    machine = setup_times[machine_id]
    if job_id_1 == -1:
        return machine.get("initial_setup_times", [0] * len(machine["setup_times"]))[job_id_2]
    return machine["setup_times"][job_id_1][job_id_2]


def sequence_values(jobs, sequence, machine_id=0, setup_times=None):
    """Compute the value of each criterion for a job sequence on a machine."""
    current_time = 0
    values = {criterion: 0 for criterion in CRITERIA}
    values["maximum-lateness"] = None
    previous_job_id = -1
    for job_id in sequence:
        job = jobs[job_id]
        current_time = (
                max(current_time
                    + setup_time(setup_times, machine_id, previous_job_id, job_id),
                    job["release_date"])
                + job["processing_times"][machine_id])
        previous_job_id = job_id
        values["makespan"] = current_time
        values["total-flow-time"] += (
                job["weight"] * (current_time - job["release_date"]))
        due_date = job.get("due_date", -1)
        if due_date < 0:
            continue
        values["total-tardiness"] += (
                job["weight"] * max(0, current_time - due_date))
        lateness = current_time - due_date
        if values["maximum-lateness"] is None or lateness > values["maximum-lateness"]:
            values["maximum-lateness"] = lateness
        if current_time <= due_date:
            values["number-of-on-time-jobs"] += 1
            values["throughput"] += job["weight"]
    return values


def better(criterion, value_1, value_2):
    if value_2 is None:
        return True
    if criterion in MAXIMIZE:
        return value_1 > value_2
    return value_1 < value_2


def solve_single_machine(jobs, criterion, maximum_size, setup_times=None):
    if math.factorial(len(jobs)) > maximum_size:
        return None
    best = None
    for sequence in itertools.permutations(range(len(jobs))):
        value = sequence_values(jobs, sequence, 0, setup_times)[criterion]
        if better(criterion, value, best):
            best = value
    return best


def combine(criterion, machine_values):
    """Value of a solution from the values of its machines."""
    if criterion == "makespan":
        return max(machine_values)
    if criterion == "maximum-lateness":
        machine_values = [v for v in machine_values if v is not None]
        return max(machine_values) if machine_values else None
    return sum(machine_values)


def solve_parallel_machines_setup_times(
        jobs, number_of_machines, setup_times, criterion, maximum_size):
    """Enumerate the sequences of all the machines: each job order, split
    into number_of_machines consecutive (possibly empty) sequences."""
    n = len(jobs)
    number_of_splits = math.comb(n + number_of_machines - 1, number_of_machines - 1)
    if math.factorial(n) * number_of_splits > maximum_size:
        return None
    best = None
    for order in itertools.permutations(range(n)):
        # Positions of the number_of_machines - 1 separators.
        for separators in itertools.combinations(
                range(n + number_of_machines - 1),
                number_of_machines - 1):
            machine_values = []
            start = 0
            previous = -1
            for machine_id, separator in enumerate(list(separators) + [n + number_of_machines - 1]):
                length = separator - previous - 1
                sequence = order[start:start + length]
                start += length
                previous = separator
                machine_values.append(sequence_values(
                    jobs, sequence, machine_id, setup_times)[criterion])
            value = combine(criterion, machine_values)
            if better(criterion, value, best):
                best = value
    return best


def solve_parallel_machines(jobs, number_of_machines, criterion, maximum_size):
    if criterion == "makespan":
        def sort_key(job_id, machine_id):
            return jobs[job_id]["release_date"]
    elif criterion == "total-flow-time":
        if any(job["release_date"] != 0 for job in jobs):
            raise ValueError(
                    "total-flow-time on parallel machines requires "
                    "release dates equal to 0.")

        def sort_key(job_id, machine_id):
            return (jobs[job_id]["processing_times"][machine_id]
                    / jobs[job_id]["weight"])
    elif criterion == "maximum-lateness":
        if any(job["release_date"] != 0 for job in jobs):
            raise ValueError(
                    "maximum-lateness on parallel machines requires "
                    "release dates equal to 0.")

        def sort_key(job_id, machine_id):
            due_date = jobs[job_id].get("due_date", -1)
            return due_date if due_date >= 0 else math.inf
    else:
        raise ValueError(
                f"criterion '{criterion}' is not supported "
                "on parallel machines.")

    if number_of_machines ** len(jobs) > maximum_size:
        return None
    best = None
    for assignment in itertools.product(
            range(number_of_machines),
            repeat=len(jobs)):
        machine_values = []
        for machine_id in range(number_of_machines):
            sequence = sorted(
                    (job_id for job_id in range(len(jobs))
                     if assignment[job_id] == machine_id),
                    key=lambda job_id: sort_key(job_id, machine_id))
            machine_values.append(
                    sequence_values(jobs, sequence, machine_id)[criterion])
        value = combine(criterion, machine_values)
        if better(criterion, value, best):
            best = value
    return best


def solve_with_algorithm(instance_full_path, algorithm, time_limit, arguments=()):
    main = os.path.join("install", "bin", "parallelschedulingsolver")
    with tempfile.TemporaryDirectory() as tmp_dir:
        json_output_path = os.path.join(tmp_dir, "output.json")
        command = [
                main,
                "--verbosity-level", "0",
                "--input", instance_full_path,
                "--format", "json",
                "--algorithm", algorithm,
                "--output", json_output_path,
                "--time-limit", str(time_limit)] + list(arguments)
        print(" ".join(command), file=sys.stderr)
        subprocess.run(command, check=True)
        with open(json_output_path, "r") as json_output_file:
            json_data = json.load(json_output_file)
    if not bool(json_data["Output"]["Optimal"]):
        return None
    return int(json_data["Output"]["Value"])


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
        "--maximum-size",
        type=int,
        default=1000000,
        help="brute force: maximum number of sequences or assignments enumerated per instance")
parser.add_argument(
        "--time-limit",
        type=float,
        default=1,
        help="algorithm: time limit in seconds")
parser.add_argument(
        "--keep-if-solved-by",
        type=str,
        help="brute force: only keep the instances on which this algorithm proves the optimality of the reference value within '--time-limit' (used to build the test lists of slow exact algorithms)")
parser.add_argument(
        "--keep-if-solved-with",
        type=str,
        action="append",
        help="with '--keep-if-solved-by': arguments of the algorithm, e.g. '--normal-instants 0'; repeat it to require each set of arguments")
args = parser.parse_args()

with open(args.output, 'w') as output_file:
    for instances_path in args.instances:
        with open(instances_path, 'r') as instances_file:
            for line in instances_file:
                instance_base_path = line.strip()
                if not instance_base_path:
                    continue
                instance_full_path = os.path.join("data", instance_base_path)
                if args.algorithm is not None:
                    value = solve_with_algorithm(
                            instance_full_path,
                            args.algorithm,
                            args.time_limit)
                    if value is None:
                        print(f"{instance_base_path}: skipped (not proved optimal)", file=sys.stderr)
                        continue
                else:
                    with open(instance_full_path, 'r') as instance_file:
                        instance = json.load(instance_file)
                    jobs = instance["jobs"]
                    criterion = args.criterion
                    if criterion == "objective":
                        criterion = OBJECTIVE_CRITERIA[instance["objective"]]
                    for job in jobs:
                        job.setdefault("release_date", 0)
                        job.setdefault("weight", 1)
                    number_of_machines = instance["number_of_machines"]
                    setup_times = instance.get("machines")
                    if setup_times is not None and not any(
                            "setup_times" in machine for machine in setup_times):
                        setup_times = None
                    if number_of_machines == 1:
                        value = solve_single_machine(
                                jobs,
                                criterion,
                                args.maximum_size,
                                setup_times)
                    elif setup_times is not None:
                        value = solve_parallel_machines_setup_times(
                                jobs,
                                number_of_machines,
                                setup_times,
                                criterion,
                                args.maximum_size)
                    else:
                        try:
                            value = solve_parallel_machines(
                                    jobs,
                                    number_of_machines,
                                    criterion,
                                    args.maximum_size)
                        except ValueError as error:
                            # With '--criterion objective', skip the
                            # instances whose objective isn't supported.
                            if args.criterion != "objective":
                                raise
                            print(f"{instance_base_path}: skipped ({error})", file=sys.stderr)
                            continue
                    if value is None:
                        print(f"{instance_base_path}: skipped (too large)", file=sys.stderr)
                        continue
                    if args.keep_if_solved_by is not None:
                        solved = True
                        for arguments in (args.keep_if_solved_with or [""]):
                            algorithm_value = solve_with_algorithm(
                                    instance_full_path,
                                    args.keep_if_solved_by,
                                    args.time_limit,
                                    arguments.split())
                            if algorithm_value is None:
                                solved = False
                                break
                            if algorithm_value != value:
                                sys.exit(f"{instance_base_path}: {args.keep_if_solved_by} {arguments}: value {algorithm_value} differs from reference {value}")
                        if not solved:
                            print(f"{instance_base_path}: skipped (not solved by {args.keep_if_solved_by})", file=sys.stderr)
                            continue
                print(f"{instance_base_path} {value}")
                output_file.write(f"{instance_base_path} {value}\n")
