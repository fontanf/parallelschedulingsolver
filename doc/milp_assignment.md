# Assignment MILP

`--algorithm milp-assignment`

Requires an MILP solver: build with `-DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON` (default solver, `--solver highs`) or `-DPARALLELSCHEDULINGSOLVER_USE_CBC=ON` (`--solver cbc`).

## Supported problems

* Makespan, with or without release dates: $R \mid r_j \mid C_{\max}$ and its special cases (single, identical and uniform machines).
* Maximum lateness, without release dates: $R \mid\mid L_{\max}$ and its special cases.

The algorithm throws an exception on the other objectives, and on the maximum lateness with release dates.

## Principle

The model only decides on which machine each job is processed. Once the jobs are assigned, a sequencing rule is optimal on each machine, and the value of the schedule it gives is linear in the assignment variables:

* makespan: earliest release date first (each job starts as early as possible);
* maximum lateness, without release dates: earliest due date first (Jackson's rule), the jobs without due date last.

This is the classical model of $R \mid\mid C_{\max}$, whose linear relaxation is the basis of the 2-approximation algorithm of Lenstra, Shmoys and Tardos (1990), extended to release dates and to the maximum lateness. There are no sequencing variables, and the model is much smaller than the [time-indexed MILP](milp_time_indexed.md): $n \, m$ binary variables, and its size doesn't depend on the processing times.

## Model

**Input**:

* machines $i = 1, \ldots, m$ and jobs $j = 1, \ldots, n$; for each job $j$: a processing time $p_{ij}$ on each machine $i$, a release date $r_j$ and, optionally, a due date $d_j$
* $R = \{r_j\}$: the distinct release dates
* $D = \{d_j\}$: the distinct due dates
* $UB$: the value of the initial solution (see [Initial solution and variable filtering](#initial-solution-and-variable-filtering))

**Variables**:

* $x_{ij} \in \{0, 1\}$: $x_{ij} = 1$ iff job $j$ is assigned to machine $i$
* $C_{\max} \in \mathbb{Z}$ or $L_{\max} \in \mathbb{Z}$, depending on the objective, at most $UB$

**Objective**: depending on the objective

```math
\min C_{\max}
\qquad \text{(makespan)}
```

```math
\min L_{\max}
\qquad \text{(maximum lateness)}
```

**Constraints**:

* Assignment: each job is assigned to one machine

```math
\forall j \qquad \sum_i x_{ij} = 1
```

* Makespan, for this objective: on each machine, the jobs released at $\rho$ or later start at $\rho$ at the earliest, and are processed one after the other

```math
\forall i, \; \forall \rho \in R \qquad C_{\max} \ge \rho + \sum_{j \,:\, r_j \ge \rho} p_{ij} \, x_{ij}
```

* Maximum lateness, for this objective: on each machine, the jobs with a due date at most $\delta$ are processed first, so the last of them completes at their total processing time, and its due date is at most $\delta$

```math
\forall i, \; \forall \delta \in D \qquad L_{\max} \ge \sum_{j \,:\, d_j \le \delta} p_{ij} \, x_{ij} - \delta
```

* Trivial lower bound: each job is processed on its fastest machine, as early as possible

```math
C_{\max} \ge \max_j \Big( r_j + \min_i p_{ij} \Big)
\qquad
L_{\max} \ge \max_{j \text{ with a due date}} \Big( \min_i p_{ij} - d_j \Big)
```

### Why the model is exact

For a fixed assignment, the makespan of the earliest release date schedule on machine $i$ is the maximum, over the jobs $j$ of the machine, of $r_j$ plus the processing times of the jobs of the machine released at $r_j$ or later. The constraints are written for all the release dates, not only the ones of the jobs of the machine, and without a big-M term. They stay valid:

* if no job of machine $i$ has a release date $\rho$, but some are released after $\rho$, the right-hand side is at most the one of the smallest of their release dates, which is a makespan of the machine;
* if no job of machine $i$ is released at $\rho$ or later, the right-hand side is $\rho \le \max_j r_j$, which is at most the makespan of any schedule.

The same argument holds for the maximum lateness, with the due dates: if no job of machine $i$ has the due date $\delta$, the right-hand side is at most the one of the largest due date of the machine below $\delta$; if none of them has a due date at most $\delta$, it is $-\delta$, at most the lateness of the jobs with due date $\delta$, which are on other machines.

Hence the optimal value of the model is the optimal value of the problem, and the solution is built from the assignment with the sequencing rule.

## Initial solution and variable filtering

Before building the model, the algorithm runs the greedy algorithms LPT, WSPT and EDD and keeps the best solution, of value $UB$, which bounds the objective variable. A variable $x_{ij}$ is removed when it can only appear in solutions worse than $UB$:

* makespan: $r_j + p_{ij} > UB$;
* maximum lateness: $p_{ij} - d_j > UB$.

## Solver

The model is solved with HiGHS (or CBC), with the default options. The solutions found by the solver are reported as they are found, and the bound of the solver is reported as a bound of the instance.

## Optimality

Exact: when the solver terminates, the bound equals the value of the solution and the output reports `"Optimal": true`. Otherwise (time limit), the output contains the best solution and the best bound found.

On the test instances, it solves the 40 makespan instances on identical machines in at most 1.3 s each (15 jobs: 46 variables, 18 constraints), while the time-indexed MILP solves 20 of them within 1 s.

## Limits

* The linear relaxation is weak: the jobs can be split between the machines, so, on identical machines, the bound is close to $\max(\sum_j p_j / m, \max_j p_j)$, and the solver has to branch, with many symmetric solutions on identical machines. It doesn't scale to large numbers of jobs.
* The other objectives need sequencing decisions: see the [time-indexed MILP](milp_time_indexed.md).

## Tests

* `test/algorithms/milp_assignment_test.cpp` (built when HiGHS is enabled): exact schedules on small instances (release dates, unrelated machines, jobs without due date), and exceptions on the unsupported problems.
* Optimal makespan and bound on the instances of `test/algorithms/milp_assignment_makespan_test.txt`.
* Optimal maximum lateness and bound on the instances of `test/algorithms/milp_assignment_maximum_lateness_test.txt`: total tardiness instances, solved with the maximum lateness objective.

## Source files

* [`include/parallelschedulingsolver/algorithms/milp_assignment.hpp`](../include/parallelschedulingsolver/algorithms/milp_assignment.hpp), [`src/algorithms/milp_assignment.cpp`](../src/algorithms/milp_assignment.cpp)
* [`src/algorithms/milp.hpp`](../src/algorithms/milp.hpp), [`src/algorithms/milp.cpp`](../src/algorithms/milp.cpp): solve a model with HiGHS or CBC, common to the MILP algorithms

## References

* J. K. Lenstra, D. B. Shmoys and É. Tardos (1990). Approximation algorithms for scheduling unrelated parallel machines. *Mathematical Programming*, 46(1–3), 259–271. [doi:10.1007/BF01585745](https://doi.org/10.1007/BF01585745)
