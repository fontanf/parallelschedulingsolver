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
./install/bin/parallelschedulingsolver  --verbosity-level 1  --input ./data/tests/total_flow_time_parallel/tft_n3x3_s0.json  --algorithm assignment-total-flow-time  --certificate solution.json
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

$1 \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total flow time

$1 \mid \mid \sum C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total tardiness

$1 \mid \mid \sum T_j$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if at most one job is tardy or if due dates are agreeable)
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid \mid \sum w_j T_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal if all jobs are tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective maximum lateness

$1 \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective throughput

$1 \mid \mid \sum U_j$
* [Moore-Hodgson](doc/moore_hodgson.md) `--algorithm moore-hodgson` (optimal)
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if at most one job is tardy)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid \mid \sum w_j U_j$
* [Moore-Hodgson](doc/moore_hodgson.md) `--algorithm moore-hodgson`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$1 \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

### Identical parallel machines

#### Objective makespan

$P_m \mid \mid C_{\max}$
* [Greedy LPT](doc/greedy_longest_processing_time.md) `--algorithm greedy-longest-processing-time` ($\frac{4}{3} - \frac{1}{3m}$ approximation; optimal if $n \le m + 1$, if an optimal schedule has at most two jobs per machine, or if $p_j = p$)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total flow time

$P_m \mid \mid \sum C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` (optimal)
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time` ($\frac{1 + \sqrt{2}}{2}$ approximation)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total tardiness

$P_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective maximum lateness

$P_m \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date` (optimal if $p_j = p$)
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective throughput

$P_m \mid \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$P_m \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

### Unrelated parallel machines

#### Objective makespan

$R_m \mid \mid C_{\max}$
* [Greedy LPT](doc/greedy_longest_processing_time.md) `--algorithm greedy-longest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid C_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total flow time

$R_m \mid \mid \sum C_j$
* [Assignment](doc/assignment_total_flow_time.md) `--algorithm assignment-total-flow-time` (optimal)
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid \sum C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid \mid \sum w_j C_j$
* [Greedy WSPT](doc/greedy_shortest_processing_time.md) `--algorithm greedy-shortest-processing-time`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid \sum w_j C_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective total tardiness

$R_m \mid \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid \sum w_j T_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective maximum lateness

$R_m \mid \mid L_{\max}$
* [Greedy EDD](doc/greedy_earliest_due_date.md) `--algorithm greedy-earliest-due-date`
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid L_{\max}$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

#### Objective throughput

$R_m \mid \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

$R_m \mid r_j \mid \sum w_j U_j$
* [Time-indexed MILP](doc/milp_time_indexed.md) `--algorithm milp-time-indexed` (exact)

## Tests

Build and run the unit tests:
```shell
cmake --build build --config Release --parallel
ctest --test-dir build/test --output-on-failure
```

There are two kinds of tests in `test/algorithms/`:
* exact schedules returned on small hand-written instances (`ScheduleTest`);
* optimal values on the generated test instances (`ExactAlgorithmTest`). Each `<algorithm>_test.txt` file lists instances and their optimal value. When the algorithm proves optimality on these instances, the test also checks its bound.

The test instances are generated by `scripts/generate_data.py` (`data/tests`, lists `data/test_<objective>_<family>.txt`). The families are: `single`, `single_release_dates`, `identical`, `identical_equal_processing_times` and `unrelated`. Due dates follow the tardiness factor / range of due dates scheme of Potts and Van Wassenhove (1985), so they don't depend on the processing times.

The reference values are computed by `scripts/solve_test_data.py`, either by brute force on the small instances (`--criterion`) or by running an algorithm that proves optimality (`--algorithm`):
```shell
python3 scripts/solve_test_data.py --criterion makespan --output test/algorithms/greedy_longest_processing_time_test.txt --instances \
    data/test_makespan_single.txt \
    data/test_makespan_identical_equal_processing_times.txt
python3 scripts/solve_test_data.py --criterion total-flow-time --output test/algorithms/greedy_shortest_processing_time_test.txt --instances \
    data/test_tft_single.txt \
    data/test_twft_single.txt \
    data/test_tft_identical.txt \
    data/test_tft_identical_equal_processing_times.txt
python3 scripts/solve_test_data.py --criterion maximum-lateness --output test/algorithms/greedy_earliest_due_date_test.txt --instances \
    data/test_tt_single.txt \
    data/test_twt_single.txt \
    data/test_tt_identical_equal_processing_times.txt \
    data/test_twt_identical_equal_processing_times.txt
python3 scripts/solve_test_data.py --criterion number-of-on-time-jobs --output test/algorithms/moore_hodgson_test.txt --instances \
    data/test_tt_single.txt \
    data/test_tft_single.txt \
    data/test_makespan_single.txt
python3 scripts/solve_test_data.py --criterion total-flow-time --output test/algorithms/assignment_total_flow_time_test.txt --instances \
    data/test_tft_single.txt \
    data/test_tft_identical.txt \
    data/test_tft_identical_equal_processing_times.txt \
    data/test_tft_unrelated.txt
python3 scripts/solve_test_data.py --criterion objective --output test/algorithms/milp_time_indexed_test.txt \
    --keep-if-solved-by milp-time-indexed --time-limit 0.5 \
    "--keep-if-solved-with=--normal-instants 1" "--keep-if-solved-with=--normal-instants 0" \
    --instances data/test_*.txt
```

The last command keeps the instances on which the time-indexed MILP proves the optimality of the reference value within 0.5 s, with and without normal instants, so that its tests stay fast. It requires a build with HiGHS.

For example, to get the optimal values of all the unrelated machines total flow time instances, including the ones too large to enumerate:
```shell
python3 scripts/solve_test_data.py --algorithm assignment-total-flow-time --instances data/test_tft_unrelated.txt --output tft_unrelated.txt
```
