# Time-indexed MILP

`--algorithm milp-time-indexed`

Requires an MILP solver: build with `-DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON` (default solver, `--solver highs`) or `-DPARALLELSCHEDULINGSOLVER_USE_CBC=ON` (`--solver cbc`).

## Supported problems

All the objectives of the repository, on single, identical, uniform or unrelated machines, with or without release dates.

## Principle

Time is discretized: a binary variable tells whether a job starts at a given time on a given machine. The objectives are then linear in these variables, and release dates are handled by removing the start times before them. The capacity of each machine is modeled as a unit flow over time. The start times are restricted to normal instants, and the variables which can't improve an initial greedy solution are removed.

## Model

**Input**:

* machines $i = 1, \ldots, m$ and jobs $j = 1, \ldots, n$; for each job $j$: a processing time $p_{ij}$ on each machine $i$, a release date $r_j$, a weight $w_j$ and, optionally, a due date $d_j$
* for each machine $i$ and each job $j$: a set $S_{ij}$ of possible start times, with $s \ge r_j$ for all $s \in S_{ij}$ (see [Start times](#start-times)); $C_{ijs} = s + p_{ij}$ is the completion time of job $j$ if it starts at time $s$ on machine $i$
* for each machine $i$: the set $T_i = \{0\} \cup \{s, C_{ijs} : j, s \in S_{ij}\}$ of the possible start and completion times, sorted, $t_{i,0} = 0 < t_{i,1} < \cdots < t_{i,K_i}$

**Variables**:

* $x_{ijs} \in \{0, 1\}$, $s \in S_{ij}$: $x_{ijs} = 1$ iff job $j$ starts at time $s$ on machine $i$
* $y_{ik} \in [0, 1]$, $k = 0, \ldots, K_i - 1$: idle arc of machine $i$ from time $t_{i,k}$ to time $t_{i,k+1}$
* $C_{\max}$, $L_{\max}$: for the corresponding objectives

**Objective**: depending on the objective

$$
\min \sum_{i, j, s} w_j \, (C_{ijs} - r_j) \, x_{ijs}
\qquad \text{(total weighted flow time } \textstyle\sum w_j (C_j - r_j) \text{, which differs from } \sum w_j C_j \text{ by a constant)}
$$

$$
\min \sum_{i, j, s} w_j \max(0, C_{ijs} - d_j) \, x_{ijs}
\qquad \text{(total weighted tardiness } \textstyle\sum w_j T_j \text{)}
$$

$$
\max \sum_{i, j, s \,:\, C_{ijs} \le d_j} w_j \, x_{ijs}
\qquad \text{(throughput } \textstyle\sum w_j (1 - U_j) \text{)}
$$

$$
\min C_{\max}
\qquad \text{(makespan)}
$$

$$
\min L_{\max}
\qquad \text{(maximum lateness)}
$$

**Constraints**:

* Assignment: each job starts exactly once

$$
\forall j \qquad \sum_{i, s \in S_{ij}} x_{ijs} = 1
$$

* Capacity, as a flow: on each machine, one unit of flow goes from time $t_{i,0}$ to time $t_{i,K_i}$, either through a job, from its start time to its completion time, or through an idle arc. Hence the jobs of a machine don't overlap.

$$
\forall i, \; \forall k = 0, \ldots, K_i \qquad
\sum_{j, s \,:\, s = t_{i,k}} x_{ijs} + y_{ik}
- \sum_{j, s \,:\, C_{ijs} = t_{i,k}} x_{ijs} - y_{i,k-1}
= \begin{cases} 1 & \text{if } k = 0 \\ -1 & \text{if } k = K_i \\ 0 & \text{otherwise} \end{cases}
$$

(with $y_{i,-1} = y_{i,K_i} = 0$; jobs with $p_{ij} = 0$ don't use any capacity and don't appear in these constraints)

* Makespan, for this objective

$$
\forall j \qquad C_{\max} \ge \sum_{i, s \in S_{ij}} C_{ijs} \, x_{ijs}
$$

* Maximum lateness, for this objective

$$
\forall j \text{ with a due date} \qquad L_{\max} \ge \sum_{i, s \in S_{ij}} (C_{ijs} - d_j) \, x_{ijs}
$$

The classical capacity constraints are "at most one job in process on machine $i$ at time $t$", for each $t$; they have $O(n \, T \, \bar{p})$ non-zeros, where $T$ is the horizon and $\bar{p}$ the average processing time. Their matrix is an interval matrix, and the flow constraints are obtained by taking the differences of consecutive rows: they have the same linear relaxation, but each $x_{ijs}$ appears in only 3 constraints.

## Start times

All the objectives are regular (non-decreasing in the completion times), so there exists an optimal semi-active schedule: no job can start earlier without changing the order of the jobs on its machine. In such a schedule, a job starts at time 0 or at a release date, plus the sum of the processing times of some of the other jobs of its machine.

With `--normal-instants 1` (default), $S_{ij}$ is restricted to these times, the analogue of normal patterns in packing (Herz, 1972; Christofides and Whitlock, 1977). They are computed by a subset sum dynamic program on bitsets, for each machine and each job, excluding the job itself. This also gives the horizon: $\max_l r_l + \sum_{k \ne j} p_{ik}$.

With `--normal-instants 0`, $S_{ij}$ contains all integers between $r_j$ and the same horizon.

Writing the flow constraints only at the possible start and completion times doesn't weaken the linear relaxation: the number of jobs in process only increases at a start time, so its maximum is reached at one.

Examples of model sizes (solve times with HiGHS 1.16.0):

| Instance | All instants | Normal instants |
| --- | --- | --- |
| `tft_n1x5_s0` (single machine, 5 jobs) | 395 variables, solved in 0.02 s | 90 variables, 0.01 s |
| `tft_n3x5_s0`, identical machines (15 jobs) | 32,986 variables, 2.9 s | 28,714 variables, 1.5 s |
| `tft_n3x5_s0`, $p_j = p$ (15 jobs) | 64,171 variables, 3.2 s | 721 variables, 0.02 s |

The gain is large when processing times are equal or few, small on random processing times: the subset sums of many random processing times cover almost all the integers up to the horizon.

## Initial solution and variable filtering

Before building the model, the algorithm runs the greedy algorithms LPT, WSPT and EDD and keeps the best solution, of value $UB$. A start variable is removed when it can only appear in solutions worse than $UB$:

* $C_{\max}$: $C_{ijs} > UB$;
* $L_{\max}$: $C_{ijs} - d_j > UB$;
* sum objectives: its cost plus the smallest costs of the other jobs exceeds $UB$.

## Solver

HiGHS presolve is turned off: it barely reduces the model and slows down the solve. With a 1 s time limit, HiGHS 1.16.0 solves 713 of the 840 test instances with presolve off (with normal instants), 657 with presolve on. With HiGHS 1.12.0, presolve also took up to 14 s and 2.9 GB on a 16-job instance, without checking the time limit.

Without presolve, HiGHS 1.16.0 doesn't check the time limit during the feasibility jump heuristic and the first solve of the root linear relaxation, so a run can exceed the time limit by a few seconds on the largest models.

For the sum objectives, the cost is moved to an integer variable $z = \sum c_{ijs} x_{ijs}$, and the model optimizes $z$, with $z \le UB$ ($z \ge UB$ for the throughput). Otherwise, after finding a first solution, HiGHS derives cliques from the objective cutoff on the binary variables (`HighsCliqueTable::extractObjCliques`), which exhausted the memory (more than 20 GB) on instances with 15 to 25 jobs with HiGHS 1.12.0 (fixed in 1.16.0). The bound on $z$ is not implied by the constraint defining it, so presolve doesn't substitute $z$ back into the objective.

## Optimality

Exact: when the solver terminates, the bound equals the value of the solution and the output reports `"Optimal": true`. Otherwise (time limit), the output contains the best solution and the best bound found.

## Limits

* The size of the model is pseudo-polynomial: about $n \, m \, T$ variables, where $T$ is the horizon. It doesn't scale to large processing times.
* The linear relaxation is strong for the sum objectives, but weak for $C_{\max}$: with a 1 s time limit and normal instants, 20 of the 40 makespan test instances on identical machines are solved (28 of the 40 total flow time ones). The [assignment MILP](milp_assignment.md) is better for the makespan and the maximum lateness.

## Tests

* `test/algorithms/milp_time_indexed_test.cpp` (built when HiGHS is enabled): exact schedules on small instances (release dates with idle time, throughput, unrelated machines, maximum lateness, without normal instants).
* Optimal value and bound on the instances of `test/algorithms/milp_time_indexed_test.txt`, with and without normal instants.

## Source files

* [`include/parallelschedulingsolver/algorithms/milp_time_indexed.hpp`](../include/parallelschedulingsolver/algorithms/milp_time_indexed.hpp), [`src/algorithms/milp_time_indexed.cpp`](../src/algorithms/milp_time_indexed.cpp)

## References

* J. P. Sousa and L. A. Wolsey (1992). A time indexed formulation of non-preemptive single machine scheduling problems. *Mathematical Programming*, 54(1–3), 353–367. [doi:10.1007/BF01586059](https://doi.org/10.1007/BF01586059)
* M. E. Dyer and L. A. Wolsey (1990). Formulating the single machine sequencing problem with release dates as a mixed integer program. *Discrete Applied Mathematics*, 26(2–3), 255–270. [doi:10.1016/0166-218X(90)90104-K](https://doi.org/10.1016/0166-218X(90)90104-K)
* J. M. van den Akker, C. A. J. Hurkens and M. W. P. Savelsbergh (2000). Time-indexed formulations for machine scheduling problems: column generation. *INFORMS Journal on Computing*, 12(2), 111–124. [doi:10.1287/ijoc.12.2.111.11896](https://doi.org/10.1287/ijoc.12.2.111.11896)
* J. C. Herz (1972). Recursive computational procedure for two-dimensional stock cutting. *IBM Journal of Research and Development*, 16(5), 462–469. [doi:10.1147/rd.165.0462](https://doi.org/10.1147/rd.165.0462)
* N. Christofides and C. Whitlock (1977). An algorithm for two-dimensional cutting problems. *Operations Research*, 25(1), 30–44. [doi:10.1287/opre.25.1.30](https://doi.org/10.1287/opre.25.1.30)
