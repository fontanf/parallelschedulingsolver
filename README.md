# ParallelSchedulingSolver

Research code for parallel scheduling problems.

The benchmark instances and how to run the benchmarks are described in [`doc/benchmarks.md`](doc/benchmarks.md), and the tests in [`doc/tests.md`](doc/tests.md).

## Compilation

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel && cmake --install build --config Release --prefix install
```

The MILP algorithms need an MILP solver: add `-DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON` (HiGHS, default solver, `--solver highs`) or `-DPARALLELSCHEDULINGSOLVER_USE_CBC=ON` (`--solver cbc`). Add `--solver-log solver.log` to write the log of the solver to a file.

Setup Python environment to use the Python scripts:
```shell
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Download data files:
```shell
python scripts/download_data.py
```

The data archives are published as GitHub releases and pinned in `data/data.json` (URL and SHA-256 checksum), so each version of the repository downloads the data it has been tested with. Use `--archives tests` to only download the instances used by the unit tests. To publish a new version of an archive, see `scripts/create_data_archives.py`.

Run:
```shell
./install/bin/parallelschedulingsolver  --verbosity-level 1  --input ./data/tests/total_flow_time_unrelated/tft_n3x3_s0.json  --algorithm assignment-total-flow-time  --certificate solution.json
```

Instances are JSON files: the objective, the number of machines, and for each job its processing time on each machine, its release date, due date and weight. Sequence-dependent setup times are given per machine (they are 0 if absent):
```json
"machines": [
    {
        "initial_setup_times": [2, 0],
        "setup_times": [[0, 5], [7, 0]]
    }
]
```
`setup_times[i][j]` is the setup time when job `j` is processed right after job `i` on the machine, and `initial_setup_times[j]` the setup time when `j` is its first job. The setup of a job is performed right before it, and may be performed before its release date. The algorithms that don't support setup times throw an exception on instances with setup times.

A job can have a rejection cost (`"rejection_cost"`, absent if the job must be scheduled): it may then not be scheduled, and its rejection cost is added to the objective. Rejection costs are supported with the makespan, the total (weighted) flow time and the total (weighted) tardiness, by the brute force and by the [time-indexed dynamic programming](doc/dynamic_programming_time_indexed.md) for now; the other algorithms throw an exception on instances with rejection costs.

Visualize solution:
```shell
python scripts/visualize.py solution.json
```

## Implemented algorithms

The algorithms planned next, in particular state-of-the-art ones, are listed in [`doc/roadmap.md`](doc/roadmap.md).

Each algorithm has a page in [`doc/`](doc/) describing it, the problems it supports, the cases in which it is optimal and its guarantees. When an algorithm detects that the instance is one of the special cases in which it is optimal, it sets the bound to the value of its solution and the output reports `"Optimal": true`.

### All problems

* [Brute force](doc/brute_force.md) `--algorithm brute-force` (exact, very small instances only; gives the reference values of the tests)

### Single machine

#### Objective makespan

$1 \mid \mid C_{\max}$
* [Greedy LPT](doc/greedy_longest_processing_time.md) `--algorithm greedy-longest-processing-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$1 \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

#### Objective total flow time

$1 \mid \mid \sum C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective total tardiness

$1 \mid \mid \sum T_j$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if at most one job is tardy or if due dates are agreeable)
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Successive sublimation dynamic programming](doc/successive_sublimation_dynamic_programming.md) `--algorithm successive-sublimation-dynamic-programming` (exact)
* [Dynamic programming on the time-indexed network](doc/dynamic_programming_time_indexed.md) `--algorithm dynamic-programming-time-indexed` (exact with `--all-jobs-in-modifiers 1`, small instances)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid r_j \mid \sum T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid \mid \sum w_j T_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Successive sublimation dynamic programming](doc/successive_sublimation_dynamic_programming.md) `--algorithm successive-sublimation-dynamic-programming` (exact)
* [Dynamic programming on the time-indexed network](doc/dynamic_programming_time_indexed.md) `--algorithm dynamic-programming-time-indexed` (exact with `--all-jobs-in-modifiers 1`, small instances)
* [Local search](doc/local_search.md) `--algorithm local-search`

$1 \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective maximum lateness

$1 \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$1 \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective throughput

$1 \mid \mid \sum U_j$
* [Moore-Hodgson](doc/moore_hodgson.md) `--algorithm moore-hodgson` (optimal)
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if at most one job is tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid r_j \mid \sum U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid \mid \sum w_j U_j$
* [Moore-Hodgson](doc/moore_hodgson.md) `--algorithm moore-hodgson`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

### Identical parallel machines

#### Objective makespan

$P_m \mid \mid C_{\max}$
* [Greedy LPT](doc/greedy_longest_processing_time.md) `--algorithm greedy-longest-processing-time` ($\frac{4}{3} - \frac{1}{3m}$ approximation; optimal if $n \le m + 1$, if an optimal schedule has at most two jobs per machine, or if $p_j = p$)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$P_m \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

#### Objective total flow time

$P_m \mid \mid \sum C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$P_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$P_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` ($\frac{1 + \sqrt{2}}{2}$ approximation)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$P_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective total tardiness

$P_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$P_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective maximum lateness

$P_m \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if $p_j = p$)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$P_m \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective throughput

$P_m \mid \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$P_m \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

### Unrelated parallel machines

#### Objective makespan

$R_m \mid \mid C_{\max}$
* [Greedy LPT](doc/greedy_longest_processing_time.md) `--algorithm greedy-longest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$R_m \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

#### Objective total flow time

$R_m \mid \mid \sum C_j$
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$R_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$R_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$R_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective total tardiness

$R_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

$R_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective maximum lateness

$R_m \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)
* [Assignment MILP](doc/milp_assignment.md) `--algorithm milp-assignment` (exact)

$R_m \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective throughput

$R_m \mid \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$R_m \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

### Sequence-dependent setup times

#### Objective total flow time

$1 \mid s_{ij} \mid \sum w_j C_j$
* [Successive sublimation dynamic programming](doc/successive_sublimation_dynamic_programming.md) `--algorithm successive-sublimation-dynamic-programming` (exact)
* [Dynamic programming on the time-indexed network](doc/dynamic_programming_time_indexed.md) `--algorithm dynamic-programming-time-indexed` (exact with `--all-jobs-in-modifiers 1`, small instances)

$1 \mid s_{ij} \mid \sum w_j C_j$, $P \mid s_{ij} \mid \sum w_j C_j$, $R \mid s_{ijk} \mid \sum w_j C_j$, with or without release dates
* [Local search](doc/local_search.md) `--algorithm local-search`

#### Objective total tardiness

$1 \mid s_{ij} \mid \sum w_j T_j$
* [Successive sublimation dynamic programming](doc/successive_sublimation_dynamic_programming.md) `--algorithm successive-sublimation-dynamic-programming` (exact)
* [Dynamic programming on the time-indexed network](doc/dynamic_programming_time_indexed.md) `--algorithm dynamic-programming-time-indexed` (exact with `--all-jobs-in-modifiers 1`, small instances)

$1 \mid s_{ij} \mid \sum w_j T_j$, $P \mid s_{ij} \mid \sum w_j T_j$, $R \mid s_{ijk} \mid \sum w_j T_j$, with or without release dates
* [Local search](doc/local_search.md) `--algorithm local-search`

## Tests

Build with HiGHS (the tests of the MILP algorithms need it), download the test instances and run the tests:
```shell
python3 scripts/download_data.py --archives tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON
cmake --build build --config Release --parallel
ctest --test-dir build/test --build-config Release --parallel --output-on-failure
```

See [`doc/tests.md`](doc/tests.md) for the test instances, how to write the tests of an algorithm, and how to compute the reference values.
