"""Generate the test instances in data/tests and their lists data/test_*.txt.

Each instance list data/test_<objective>_<family>.txt contains the instances
of one objective and one family:
* single: single machine
* single_release_dates: single machine with release dates
* identical: identical parallel machines
* identical_equal_processing_times: identical parallel machines, p_j = p
* unrelated: unrelated parallel machines
* single_setup_times: single machine with sequence-dependent setup times
* single_release_dates_setup_times: single machine with release dates and
  sequence-dependent setup times
* identical_setup_times: identical parallel machines with sequence-dependent
  setup times, the same on all machines
* unrelated_setup_times: unrelated parallel machines with machine- and
  sequence-dependent setup times

The setup times, including the setup times of the first job of each machine,
are drawn in [0, 20], for processing times in [1, 100].

Due dates are generated with the tardiness factor / range of due dates scheme
of Potts and Van Wassenhove (1985), so they don't depend on the processing
times of the jobs.
"""

import os
import contextlib
import sys

generator_main = os.path.join(
        "install",
        "bin",
        "parallelschedulingsolver_generator")

objectives = [
        ("makespan", "makespan"),
        ("total_flow_time", "tft"),
        ("total_weighted_flow_time", "twft"),
        ("total_tardiness", "tt"),
        ("total_weighted_tardiness", "twt"),
        ("throughput", "throughput")]

# (name, numbers of machines, generator options)
families = [
        ("single", [1], ""),
        ("single_release_dates", [1], "  --release-dates-factor 0.5"),
        ("identical", [2, 3, 4, 5], "  --identical-machines 1"),
        ("identical_equal_processing_times", [2, 3, 4, 5], "  --equal-processing-times 1"),
        ("unrelated", [2, 3, 4, 5], ""),
        ("single_setup_times", [1], "  --setup-times-range 20"),
        ("single_release_dates_setup_times", [1], "  --release-dates-factor 0.5  --setup-times-range 20"),
        ("identical_setup_times", [2, 3, 4, 5], "  --identical-machines 1  --setup-times-range 20"),
        ("unrelated_setup_times", [2, 3, 4, 5], "  --setup-times-range 20"),
]

instances_paths = {}
for objective, objective_short in objectives:
    for family, _, _ in families:
        instances_paths[(objective, family)] = os.path.join(
                "data",
                f"test_{objective_short}_{family}.txt")

with contextlib.ExitStack() as stack:
    files = {
            instances_path:
            stack.enter_context(open(instances_path, 'w'))
            for instances_path in instances_paths.values()}

    for objective, objective_short in objectives:
        for family, numbers_of_machines, family_options in families:
            for number_of_machines in numbers_of_machines:
                for number_of_jobs_per_machine in [1, 2, 3, 4, 5]:
                    for seed in [0, 1]:
                        instances_path = instances_paths[(objective, family)]

                        instance_base_path = os.path.join(
                                "tests",
                                objective + "_" + family,
                                objective_short
                                + f"_n{number_of_machines}"
                                f"x{number_of_jobs_per_machine}"
                                f"_s{seed}"
                                f".json")
                        instance_full_path = os.path.join("data", instance_base_path)
                        if not os.path.exists(os.path.dirname(instance_full_path)):
                            os.makedirs(os.path.dirname(instance_full_path))

                        weight_range = 1
                        if objective_short in ["twft", "twt", "throughput"]:
                            weight_range = 100

                        command = generator_main
                        command += "  --objective " + objective.replace("_weighted", "").replace("_", "-")
                        command += f"  --number-of-machines \"{number_of_machines}\""
                        command += f"  --number-of-jobs-per-machine \"{number_of_jobs_per_machine}\""
                        command += f"  --processing-times-range 100"
                        command += f"  --weights-range {weight_range}"
                        command += family_options
                        command += f"  --seed {seed}"
                        command += f"  --output \"{instance_full_path}\""
                        print(command)
                        status = os.system(command)
                        if status != 0:
                            sys.exit(1)

                        files[instances_path].write(f"{instance_base_path}\n")
