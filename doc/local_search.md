# Local search

`--algorithm local-search`, with `--seed` for the random generator.

Implementation of the unified iterated local search (UILS) of Kramer and Subramanian (2019). The code of the paper is not available; this implementation follows the description of the paper, and the details it doesn't give are listed below.

## Supported problems

The total weighted tardiness ($\sum w_j T_j$) and the total weighted flow time ($\sum w_j (C_j - r_j)$, equivalent to $\sum w_j C_j$), on single, identical, uniform or unrelated machines, with or without release dates. The algorithm throws an exception on the other objectives.

Kramer and Subramanian (2019) also consider earliness penalties and sequence-dependent setup times, which are not part of the problems of this repository.

## Principle

Multi-start iterated local search:

```
for each restart (at most I_R):
    s ← initial solution
    s ← RVND(s)
    while the number of consecutive perturbations without improvement < I_ILS:
        s' ← RVND(perturbation(s))
        if f(s') < f(s): s ← s'
return the best solution found
```

The algorithm also stops at the time limit, and when the solution reaches the bound of the objective (a total tardiness of 0).

### Initial solutions

* Parallel machines: each job on a random machine, in a random order.
* Single machine, without release dates: at random, or with a greedy randomized earliest due date rule (one of them at random).
* Single machine, with release dates: greedy randomized earliest release date rule.

The greedy randomized rules are the constructive phase of GRASP: the next job is chosen at random among the remaining jobs whose due date (or release date) is at most $\min + \alpha (\max - \min)$.

### Local search: randomized variable neighborhood descent (RVND)

A neighborhood is chosen at random among the neighborhoods not explored yet, and its best move is applied if it improves the solution. After an improvement, all the neighborhoods can be chosen again. The local search stops when no neighborhood improves the solution.

The neighborhoods move blocks of consecutive jobs:

* $l$-block insertion intra-machine: move a block of $l$ jobs to another position of its machine;
* $(l, l')$-block swap intra-machine: swap a block of $l$ jobs with a block of $l'$ jobs of the same machine;
* $l$-block insertion inter-machine: move a block of $l$ jobs to a position of another machine;
* $(l, l')$-block swap inter-machine: swap a block of $l$ jobs with a block of $l'$ jobs of another machine.

The block sizes are the ones selected in the paper:

| | Single machine | Parallel machines |
| --- | --- | --- |
| Insertion intra-machine | $l \in \{1, 2, 3\}$ | $l \in \{1, 2\}$ |
| Swap intra-machine | $(1, 1)$ | $(1, 1)$ |
| Insertion inter-machine | | $l \in \{1, 2\}$ |
| Swap inter-machine | | $(1, 1)$, $(1, 2)$, $(1, 3)$, $(2, 2)$, $(2, 3)$, $(2, 4)$, $(3, 3)$, $(3, 4)$, $(4, 4)$ |

### Perturbations

* Single machine: one $(l, l')$-block swap intra-machine at random, with $l, l' \in \{2, \ldots, n / 4\}$.
* Parallel machines, one of these two at random:
  * multiple $(1, 1)$-insertion inter-machine: a job of a machine is moved to another machine, while a job of this other machine is moved to the first one, the machines, jobs and positions being chosen at random (both jobs are chosen before the move, so the job moved to the second machine can't be moved back);
  * multiple $(l, l')$-block insertion inter-machine: the same with blocks of $l \in \{1, 2\}$ and $l' \in \{2, 3\}$ jobs.

  They are repeated 1, 2 or 3 times.

### Parameters

* `--maximum-number-of-restarts` ($I_R$): 10 by default, as in the paper.
* `--maximum-number-of-perturbations-without-improvement` ($I_{ILS}$): by default $4 n$ without release dates and $n$ with release dates, as in the paper ($n$ for the problems with idle time).
* `--time-limit`: the paper limits the total time to 600 s.

## Move evaluation

The jobs of a machine start as early as possible, after the previous job and after their release date. Without earliness penalties, this is optimal for a given sequence, so the timing problem is trivial, even with release dates.

For each machine, the algorithm stores the completion times and the objective of the first $k$ jobs, for each $k$. A move is evaluated by scheduling the modified part of the sequence, starting from the stored values at the first modified position, and then the rest of the sequence, with three shortcuts:

* when the completion time of the modified part equals the stored completion time at the same position of the current solution, the rest of the schedule is unchanged, and its objective is read from the stored values;
* when it is larger, the objective of the rest is at least the one in the current solution (the objectives are regular), which gives a lower bound;
* the evaluation stops as soon as the objective is not better than the best move found.

For the forward insertions within a machine, the jobs skipped by the block are scheduled incrementally, as in Algorithm 3 of the paper.

A move is thus evaluated in $O(n)$ time in the worst case, as in the "straightforward" evaluation of Section 4.2.2 of the paper. The amortized $O(1)$ evaluations of Section 4.2.3 (extending Ergun and Orlin, 2006), with piecewise linear cost functions, are not implemented: according to the paper, they speed up the algorithm by a factor up to 4 on instances with 100 to 200 jobs on few machines, and slow it down on instances with few jobs per machine.

## Details not given in the paper

* The value of $\alpha$ of the greedy randomized rules: drawn uniformly in $[0, 1]$ at each restart.
* The probability to start from a random solution rather than the greedy randomized rule, on a single machine without release dates: 1/2.
* The number of repetitions of the multiple insertion perturbations: drawn uniformly in $\{1, 2, 3\}$.
* The perturbation of a single machine when $n / 4 < 2$: blocks of one job.
* The value of $I_{ILS}$ with release dates and without earliness penalties: the paper adopts $I_{ILS} = n$ for the problems with idle time, mainly the earliness–tardiness problems, where idle time is inserted on purpose, and also calls idle time the waiting due to release dates (Section 4.2.1). This implementation uses $I_{ILS} = n$ whenever there are release dates.
* The algorithm also stops when the solution reaches the bound of the objective, which doesn't change the solution returned.

## Results

### $P \mid\mid \sum T_j$: instances of Tanaka and Araki (2008)

2,250 instances with 20 and 25 jobs and 2 to 10 machines, with their optimal values (benchmark `identical_tt_tanaka2008`). As in the paper, the algorithm was run 10 times on each instance (seeds 0 to 9), with the default parameters.

| | Kramer and Subramanian (2019) | This implementation |
| --- | --- | --- |
| Instances for which the best run is optimal | 2,250 | 2,250 |
| Runs reaching the optimal value | 22,465 / 22,500 | 22,449 / 22,500 |
| Average gap of the runs | 0.00 % | 0.001 % |
| Average time of a run | 0.4 s (Intel i7-2600 3.4 GHz) | 0.34 s (4 runs in parallel) |

Per group of 125 instances, the number of runs reaching the optimal value (out of 1,250):

| Group | Paper | This implementation | Group | Paper | This implementation |
| --- | --- | --- | --- | --- | --- |
| N20_M2 | 1250 | 1250 | N25_M2 | 1250 | 1250 |
| N20_M3 | 1250 | 1250 | N25_M3 | 1247 | 1239 |
| N20_M4 | 1250 | 1247 | N25_M4 | 1248 | 1248 |
| N20_M5 | 1250 | 1250 | N25_M5 | 1244 | 1238 |
| N20_M6 | 1250 | 1250 | N25_M6 | 1246 | 1247 |
| N20_M7 | 1250 | 1250 | N25_M7 | 1250 | 1250 |
| N20_M8 | 1244 | 1249 | N25_M8 | 1248 | 1243 |
| N20_M9 | 1247 | 1246 | N25_M9 | 1243 | 1244 |
| N20_M10 | 1248 | 1249 | N25_M10 | 1250 | 1249 |

The paper reports that each instance is solved to optimality in at least 9 of the 10 runs; with this implementation, 9 instances are solved in fewer than 9 runs (in 1 to 8 runs).

On a few instances, this implementation is less reliable than the one of the paper. With 100 seeds, `25_05_04_02_004` (25 jobs, 5 machines) is solved to optimality in 16 runs, and `25_03_02_02_001` (25 jobs, 3 machines) in 13 runs. On these instances:

* The local optima where the runs get stuck (365 instead of 362, and 157 instead of 155) are all different, and each machine is sequenced optimally (checked by dynamic programming): the runs are stuck on the assignment of the jobs to the machines.
* They are far from the optimal solutions: 11 to 15 of the 25 jobs are on another machine, for the best matching of the machines.
* No larger move improves them: block insertions and swaps of any size within and between machines, block reversals, cyclic exchanges of jobs between 3 machines.
* A brute-force check after each local search confirmed that the neighborhoods of the paper are explored completely (no improving move is missed).

The following variants of the details not given in the paper were tested (with a previous version of the multiple insertion perturbations, where the second job was chosen after the first move), without improving the results on the whole benchmark: accepting the solutions as good as the best solution of the restart, breaking the ties between the best moves of a neighborhood at random, 3 or 5 repetitions of the perturbations, and initial solutions built by list scheduling from a random order of the jobs. For example, with 3 repetitions and the acceptance of ties, 22,448 runs reach the optimal value (22,450 without), and 2 instances are never solved in 10 runs. The difference with the paper on these instances is thus not explained.

### Other benchmarks of the paper

The instances of the other problems of the paper that are in the scope of this repository are not available any more at the addresses given in the paper:

* $P \mid\mid \sum w_j T_j$, instances of Rodrigues et al. (2008) (groups wt40, wt50, wt100): the website of the AlgoX group of the Federal University of Amazonas (`algox.icomp.ufam.edu.br`) doesn't answer, and the Internet Archive has the page listing the instances but not the archives.
* $R \mid\mid \sum w_j T_j$, instances of Şen and Bülbül (2015): the archive on the website of Kerem Bülbül (`people.sabanciuniv.edu/~bulbul`) returns an error 403, and it is not in the Internet Archive.

## Tests

* `test/algorithms/local_search_test.cpp`: the local search finds the optimal value of the total flow time and total tardiness test instances, with seed 0. The reference values come from brute force (`local_search_test.txt`) and, for the total tardiness on parallel machines, from the time-indexed MILP (`local_search_parallel_tardiness_test.txt`).

## Source files

* [`include/parallelschedulingsolver/algorithms/local_search.hpp`](../include/parallelschedulingsolver/algorithms/local_search.hpp), [`src/algorithms/local_search.cpp`](../src/algorithms/local_search.cpp)

## References

* A. Kramer and A. Subramanian (2019). A unified heuristic and an annotated bibliography for a large class of earliness–tardiness scheduling problems. *Journal of Scheduling*, 22, 21–57. [doi:10.1007/s10951-017-0549-6](https://doi.org/10.1007/s10951-017-0549-6)
* S. Tanaka and M. Araki (2008). A branch-and-bound algorithm with Lagrangian relaxation to minimize total tardiness on identical parallel machines. *International Journal of Production Economics*, 113(1), 446–458. [doi:10.1016/j.ijpe.2007.10.006](https://doi.org/10.1016/j.ijpe.2007.10.006)
* Ö. Ergun and J. B. Orlin (2006). Fast neighborhood search for the single machine total weighted tardiness problem. *Operations Research Letters*, 34(1), 41–45. [doi:10.1016/j.orl.2005.01.010](https://doi.org/10.1016/j.orl.2005.01.010)
