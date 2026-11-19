# ParallelSchedulingSolver

Research code for parallel scheduling problems.

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

Visualize solution:
```shell
python scripts/visualize.py solution.json
```

## Implemented algorithms

The algorithms planned next, in particular state-of-the-art ones, are listed in [`doc/roadmap.md`](doc/roadmap.md).

Each algorithm has a page in [`doc/`](doc/) describing it, the problems it supports, the cases in which it is optimal and its guarantees. When an algorithm detects that the instance is one of the special cases in which it is optimal, it sets the bound to the value of its solution and the output reports `"Optimal": true`.

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

$1 \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective total tardiness

$1 \mid \mid \sum T_j$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if at most one job is tardy or if due dates are agreeable)
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid r_j \mid \sum T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid \mid \sum w_j T_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$1 \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

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

$P_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$P_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` ($\frac{1 + \sqrt{2}}{2}$ approximation)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$P_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective total tardiness

$P_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$P_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

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

$R_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$R_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$R_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

#### Objective total tardiness

$R_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

$R_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)
* [Positional MILP](doc/milp_positional.md) `--algorithm milp-positional` (exact)

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

## Tests

Build with HiGHS (the tests of the MILP algorithms need it), download the test instances and run the tests:
```shell
python3 scripts/download_data.py --archives tests
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON
cmake --build build --config Release --parallel
ctest --test-dir build/test --build-config Release --parallel --output-on-failure
```

See [`doc/tests.md`](doc/tests.md) for the test instances, how to write the tests of an algorithm, and how to compute the reference values.
