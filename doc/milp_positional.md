# Positional MILP

`--algorithm milp-positional`

Requires an MILP solver: build with `-DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON` (default solver, `--solver highs`) or `-DPARALLELSCHEDULINGSOLVER_USE_CBC=ON` (`--solver cbc`).

## Supported problems

All the objectives of the repository, on single, identical, uniform or unrelated machines, with or without release dates.

## Principle

Each job is assigned to a position on a machine, and the completion times of the positions follow from the processing times of the jobs at the earlier positions and from their release dates. The completion time of each job is linked to the completion time of its position by big-M constraints, and the objectives are written on the completion times of the jobs.

The positions are counted from the end of each machine: position 1 is the last job. This gives a valid inequality on the sum of the completion times, which makes the linear relaxation strong for $R \mid\mid \sum C_j$.

Unlike the [time-indexed MILP](milp_time_indexed.md), the size of the model is polynomial, $O(n^2 m)$, and doesn't depend on the processing times.

## Model

**Input**:

* machines $i = 1, \ldots, m$ and jobs $j = 1, \ldots, n$; for each job $j$: a processing time $p_{ij}$ on each machine $i$, a release date $r_j$, a weight $w_j$ and, optionally, a due date $d_j$
* positions $k = 1, \ldots, n$ on each machine, counted from the end
* $H_i = \max_j r_j + \sum_j p_{ij}$: the horizon of machine $i$, after which no job completes in a semi-active schedule; $H = \max_i H_i$
* $e_j = r_j + \min_i p_{ij}$: the earliest completion time of job $j$
* $M_{ij} = H_i - e_j$
* $UB$: the value of the initial solution (see [Initial solution](#initial-solution))

**Variables**:

* $x_{ijk} \in \{0, 1\}$: $x_{ijk} = 1$ iff job $j$ is at position $k$ (from the end) of machine $i$
* $C_{ik} \in [0, H_i]$: completion time of position $k$ of machine $i$ (the completion time of the previous position if position $k$ is empty)
* $C_j \in [e_j, H]$: completion time of job $j$ (not for the makespan)
* depending on the objective:
  * $z$: total weighted flow time, at most $UB$
  * $T_j \ge 0$: tardiness of job $j$, for the jobs with a due date
  * $u_j \in \{0, 1\}$: $u_j = 1$ iff job $j$ is on time, for the jobs with a due date $d_j \ge e_j$
  * $C_{\max} \in [\max_j e_j, UB]$, $L_{\max} \le UB$

**Objective**: depending on the objective

```math
\min z
\qquad \text{(total weighted flow time } \textstyle\sum w_j (C_j - r_j) \text{)}
```

```math
\min \sum_{j \text{ with a due date}} w_j \, T_j
\qquad \text{(total weighted tardiness)}
```

```math
\max \sum_j w_j \, u_j
\qquad \text{(throughput)}
```

```math
\min C_{\max}
\qquad \text{(makespan)}
```

```math
\min L_{\max}
\qquad \text{(maximum lateness)}
```

**Constraints**:

* Assignment: each job is at one position

```math
\forall j \qquad \sum_{i, k} x_{ijk} = 1
```

* Positions: each position has at most one job, and the positions of a machine are filled from the end (if position $k + 1$ has a job, position $k$ has one too), which removes the symmetric solutions with empty positions in between

```math
\forall i, k \qquad \sum_j x_{ijk} \le 1
\qquad\qquad
\forall i, \; \forall k < n \qquad \sum_j x_{ij,k+1} \le \sum_j x_{ijk}
```

* Completion times of the positions: after the previous position (position $k + 1$, with $C_{i,n+1} = 0$), and after the release date of the job of the position

```math
\forall i, k \qquad C_{ik} \ge C_{i,k+1} + \sum_j p_{ij} \, x_{ijk}
\qquad\qquad
\forall i, k \qquad C_{ik} \ge \sum_j (r_j + p_{ij}) \, x_{ijk}
```

* Completion times of the jobs, except for the makespan: the constraint is redundant when $x_{ijk} = 0$, since $C_{ik} \le H_i$ and $C_j \ge e_j$

```math
\forall i, j, k \qquad C_j \ge C_{ik} - M_{ij} \, (1 - x_{ijk})
```

* Valid inequality, except for the makespan

```math
\sum_j C_j \ge \sum_{i, j, k} k \, p_{ij} \, x_{ijk}
```

* Objective, depending on the objective

```math
z = \sum_j w_j \, (C_j - r_j)
\qquad \text{(total weighted flow time)}
```

```math
\forall j \text{ with a due date} \qquad T_j \ge C_j - d_j,
\qquad \sum_j w_j \, T_j \le UB
\qquad \text{(total weighted tardiness)}
```

```math
\forall j \text{ with } u_j \qquad C_j \le d_j + (H - d_j)(1 - u_j),
\qquad \sum_j w_j \, u_j \ge UB
\qquad \text{(throughput)}
```

```math
\forall i \qquad C_{\max} \ge C_{i1}
\qquad \text{(makespan)}
```

```math
\forall j \text{ with a due date} \qquad L_{\max} \ge C_j - d_j
\qquad \text{(maximum lateness)}
```

### Valid inequality

On a machine, the job at position $k$ from the end completes after the jobs at positions $k' \ge k$ have been processed, so its completion time is at least the sum of their processing times. Summing over the jobs, the processing time of the job at position $k$ is counted once for each of the $k$ jobs at positions $1, \ldots, k$:

```math
\sum_j C_j \ge \sum_{i, k} \sum_{k' \ge k} \sum_j p_{ij} \, x_{ijk'} = \sum_{i, j, k} k \, p_{ij} \, x_{ijk}
```

This holds with release dates and weights. Without release dates, it is an equality for the semi-active schedules, and the right-hand side is the objective of the assignment model of $R \mid\mid \sum C_j$ (Horn, 1973; Bruno, Coffman and Sethi, 1974; see [`assignment_total_flow_time.md`](assignment_total_flow_time.md)), whose linear relaxation is integral. Hence, for $R \mid\mid \sum C_j$, the linear relaxation of the positional model gives the optimal value.

Counting the positions from the start instead would require the number of jobs of each machine to write this inequality, which is not linear.

## Initial solution

Before building the model, the algorithm runs the greedy algorithms LPT, WSPT and EDD and keeps the best solution, of value $UB$, which bounds the objective of the model.

## Solver

The model is solved with HiGHS (or CBC), with the default options. The solutions found by the solver are reported as they are found, and the bound of the solver is reported as a bound of the instance.

## Optimality

Exact: when the solver terminates, the bound equals the value of the solution and the output reports `"Optimal": true`. Otherwise (time limit), the output contains the best solution and the best bound found.

With a 5 s time limit, it solves 655 of the 840 test instances (up to 25 jobs):

| Objective | Single machine | Identical | Identical, $p_j = p$ | Unrelated |
| --- | --- | --- | --- | --- |
| $C_{\max}$ | 20 / 20 | 34 / 40 | 40 / 40 | 35 / 40 |
| $\sum C_j$ | 20 / 20 | 40 / 40 | 40 / 40 | 40 / 40 |
| $\sum w_j C_j$ | 20 / 20 | 20 / 40 | 17 / 40 | 19 / 40 |
| $\sum T_j$ | 20 / 20 | 20 / 40 | 23 / 40 | 38 / 40 |
| $\sum w_j T_j$ | 20 / 20 | 19 / 40 | 20 / 40 | 38 / 40 |
| $\sum w_j U_j$ (throughput) | 20 / 20 | 33 / 40 | 20 / 40 | 39 / 40 |

The single machine column includes the instances with release dates. The $\sum C_j$ instances are all solved in at most 0.4 s.

## Comparison with the time-indexed MILP

The time-indexed MILP has a stronger linear relaxation with weights and due dates, so it solves more of the test instances, whose processing times are at most 100. The positional MILP is better:
* when the processing times are large and varied: on `tft_n3x5_s0` (identical machines, 15 jobs), with the processing times multiplied by 100 plus a random number in $[0, 99]$, the positional model has 736 variables and is solved in 0.05 s; the time-indexed model has 672,382 variables and is not solved in 30 s (1.6 GB);
* for $\sum C_j$ on parallel machines, thanks to the valid inequality.

With normal instants, multiplying all the processing times by the same factor doesn't change the size of the time-indexed model.

## Limits

* The big-M constraints make the linear relaxation weak with weights or due dates: the bound mostly comes from the valid inequality and the release dates.
* Identical machines are symmetric, and the model doesn't break this symmetry.
* The model has $n^2 m$ binary variables and $n^2 m$ big-M constraints: it doesn't scale to large numbers of jobs.

## Tests

* `test/algorithms/milp_positional_test.cpp` (built when HiGHS is enabled): exact schedules on small instances (release dates with idle time, throughput, unrelated machines, maximum lateness, weighted tardiness on two machines).
* Optimal value and bound on the instances of `test/algorithms/milp_positional_test.txt`: the instances solved in less than 0.5 s, with brute force reference values.

## Source files

* [`include/parallelschedulingsolver/algorithms/milp_positional.hpp`](../include/parallelschedulingsolver/algorithms/milp_positional.hpp), [`src/algorithms/milp_positional.cpp`](../src/algorithms/milp_positional.cpp)

## References

* A. B. Keha, A. Khowala and J. W. Fowler (2009). Mixed integer programming formulations for single machine scheduling problems. *Computers & Industrial Engineering*, 56(1), 357–367. [doi:10.1016/j.cie.2008.06.008](https://doi.org/10.1016/j.cie.2008.06.008) (comparison of the positional, time-indexed, disjunctive and linear ordering formulations)
* W. A. Horn (1973). Minimizing average flow time with parallel machines. *Operations Research*, 21(3), 846–847. [doi:10.1287/opre.21.3.846](https://doi.org/10.1287/opre.21.3.846)
* J. Bruno, E. G. Coffman Jr. and R. Sethi (1974). Scheduling independent tasks to reduce mean finishing time. *Communications of the ACM*, 17(7), 382–387. [doi:10.1145/361011.361064](https://doi.org/10.1145/361011.361064)
