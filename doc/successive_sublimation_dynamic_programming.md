# Successive sublimation dynamic programming

`--algorithm successive-sublimation-dynamic-programming`

Exact algorithm of Tanaka and Araki (2013) for $1 \mid s_{ij} \mid \sum w_j T_j$, an extension of the SSDP (successive sublimation dynamic programming) algorithms of Tanaka, Fujikuma and Araki (2009) and Tanaka and Fujikuma (2012) for the single machine problems without setup times.

## Supported problems

Single machine, without release dates, with or without sequence-dependent setup times:
* total weighted tardiness $1 \mid s_{ij} \mid \sum w_j T_j$;
* total weighted completion time $1 \mid s_{ij} \mid \sum w_j C_j$.

The algorithm throws `std::invalid_argument` on the other instances. It supports at most 127 jobs (after the removal of the zero cost jobs).

## Principle

Without idle time, a schedule is a path in a time-expanded network: a node $v_{it}$ is the completion of job $i$ at time $t$, and an arc from $v_{j, t - s_{ji} - p_i}$ to $v_{it}$ means that job $i$ is processed right after job $j$. The source is time 0, and the arcs to the sink start at the nodes $v_{jt}$ with $T_{\min} \le t \le T_{\max}$, the bounds of the makespan given by the assignment relaxation of the asymmetric traveling salesman problem of the setup times. The problem is a shortest path problem where each job is visited exactly once.

The constraints "each job is visited exactly once" are relaxed with Lagrangian multipliers $\mu_i$: the length of an arc into $v_{it}$ becomes $f_i(t) - \mu_i$, and the relaxation (LR2) is a shortest path problem, solved by dynamic programming, with the constraint that a job is not visited twice in three successive nodes (no 2-cycle). Each node stores the shortest and second shortest paths with different neighbor jobs. With setup times, the relaxation without this constraint has the same complexity $O(n^2 T_{\max})$, so only (LR2) is used.

Then, the constraints are recovered for a growing set of jobs $\mathcal{M}$ with state-space modifiers: the nodes become $v^b_{it}$, where $b$ is the set of the jobs of $\mathcal{M}$ processed up to job $i$, and the jobs of $\mathcal{M}$ are processed exactly once on every path. With $\mathcal{M} = \mathcal{N}$, the relaxation is the original problem.

### Stage 1: multipliers

The multipliers are adjusted either by:
* column generation (Section 5.2 of the paper, default): the Lagrangian dual is solved as the LP $\max \sum_i \mu_i + \lambda$ s.t. $L(\mathcal{P}) - \sum_i \mu_i V_i(\mathcal{P}) \ge \lambda$ for the generated paths $\mathcal{P}$ (the dual of the master problem $\min \sum_{\mathcal{P}} L(\mathcal{P}) x_{\mathcal{P}}$ s.t. $\sum_{\mathcal{P}} V_i(\mathcal{P}) x_{\mathcal{P}} = 1$ for each job $i$ and $\sum_{\mathcal{P}} x_{\mathcal{P}} = 1$), with [columngenerationsolver](https://github.com/fontanf/columngenerationsolver) (HiGHS, or CLP), and the dynamic programming as pricing problem. The dual is stabilized by Wentges smoothing with the multipliers of the best lower bound as stability center ($\alpha = 0.5$, with the mispricing rule of Pessoa et al., 2010). The first paths are the initial upper bound solution and the solution of the relaxation for the multipliers equal to 0;
* the conjugate subgradient algorithm (Section 3.4.1, `--column-generation 0`, and always without LP solver), with the step size control of Tanaka and Fujikuma (2012) and their parameters for stage 1: $(\gamma^{\text{ini}}, \delta_T, \delta_S, \epsilon, \kappa_S, \kappa_E) = (1.2, n, 2, 0.02, 0.95, 1.1)$.

The network is reduced every time the best lower bound or the upper bound is updated. The algorithm stops if the gap between the bounds is smaller than one.

### Stage 2: constraint addition

With the multipliers of stage 1 fixed, the subprocedure is repeated with a tentative upper bound $UB^{\text{tent}}$, increased by $\Delta = 0.01\, UB$ ($\Delta = 1$ if the lower bound of stage 1 is 0) from the lower bound of stage 1 until $UB^{\text{tent}} = UB$:
1. $\mathcal{M} = \emptyset$.
2. Add at most three jobs to $\mathcal{M}$, and generate the network of (LR2$^m$) from the previous one, by forward or backward dynamic programming in turns. A state of the new network is only generated if the lower bound of the paths through it, from its label and the label of the other direction of its projection in the previous network, is at most $UB^{\text{tent}} - 1$.
3. Search for a new upper bound if the lower bound is improved; update $UB$ and $UB^{\text{tent}}$.
4. Stop if $UB^{\text{tent}} - LB < 1$; otherwise go to 2.

When a subprocedure stops, no solution is better than $UB^{\text{tent}}$, which becomes a lower bound. When $UB^{\text{tent}} = UB$, $UB$ is optimal.

The number of jobs added to $\mathcal{M}$ (1, 2 or 3) and their choice follow Tanaka et al. (2009): if the networks use little memory, the jobs that don't occur in the current solution of the relaxation, then those occurring more than once, the jobs with the fewest states first (S1); otherwise, the jobs with the fewest states (S2).

### Network reduction

* By upper bound (Section 3.2.1): an arc is removed if the lower bound of the paths through it, from the forward and backward labels, is larger than $UB - 1$ (the costs are integers).
* By constraint propagation (Section 3.2.2, `--constraint-propagation`): the time windows of the jobs are derived from the network, tightened by immediate selection, edge-finding and not-first/not-last on task intervals, with the processing times $p'_i = p_i + \min_j s_{ji}$; then the nodes outside the time windows are removed, as well as the nodes $v_{it}$ with $d_j - p'_j < t < r_j + p'_j + p'_i$ for some job $j$ (direct elimination).
* The states that are not on a path from the source to the sink are removed.

### Interval branching

If the memory (`--maximum-memory`, in MB, 2048 by default) is not sufficient in the subprocedure, the algorithm branches (Section 5.4, `--interval-branching`): the completion times of the two jobs with the most states are split into two intervals with balanced numbers of states, and the subprocedure is applied to each of the four networks, recursively. The largest depth reached for a tentative upper bound is used from the start for the next one.

### Upper bounds

* Initial upper bound: best of the greedy insertions in SPT, LPT, EDD and LDD orders (Tanaka and Fujikuma, 2012), improved by the [local search](local_search.md) with one restart.
* Lagrangian heuristic: the duplicated jobs of the solution of the relaxation are removed, then the missing jobs are inserted at their best positions in SPT order, and the solution is improved by the local search with one restart and 100 perturbations without improvement. A solution of the relaxation that visits each job exactly once is a feasible solution.

### Zero cost jobs

A job $i$ with $w_i = 0$ (or without due date) satisfying $s_{ji} + p_i + s_{ik} \ge s_{jk}$ for all the other jobs $j$ (and the initial setup) and $k$ can be moved to the end of any solution without increasing its cost (Section 5.1). Such jobs are removed one at a time, and scheduled last.

## Differences with the paper

* Upper bounds: the paper uses a modified dynasearch (Section 3.3) and an iterated dynasearch (Section 5.3); the algorithm uses the local search of Kramer and Subramanian (2019) of the repository. The correctness of the algorithm doesn't depend on the upper bounds.
* Node compression (Section 3.2.3) is not implemented: in this implementation, the arcs are not stored, only the set of the jobs of the predecessors of each state, so the memory saving of super-nodes is small.
* The network is stored with the labels of both directions; the paper doesn't give its data structures.
* Parameters not given by the paper: the smoothing parameter of the column generation, the frequency of the Lagrangian heuristic in stage 1 (every five iterations, as in Tanaka et al., 2009), the parameters of the local search.

## Parameters

| Option | Default | Description |
| --- | --- | --- |
| `--column-generation` | 1 | Stage 1 by column generation (requires HiGHS or CLP) instead of the conjugate subgradient |
| `--constraint-propagation` | 1 | Network reduction by constraint propagation |
| `--interval-branching` | 1 | Branching when the memory is not sufficient |
| `--maximum-memory` | 2048 | Memory for the networks (MB) |
| `--initial-local-search-restarts` | 1 | Restarts of the initial local search (0: greedy only) |
| `--lagrangian-heuristic-local-search` | 1 | Local search in the Lagrangian heuristic |
| `--maximum-number-of-stage-1-iterations` | 10000 | Iterations of stage 1 |
| `--minimum-branching-depth` | 0 | Branching from the start of stage 2 (for testing) |

## Results

### $1 \mid s_{ij} \mid \sum w_j T_j$: instances of Cicirello (2003), moderate and tight due dates

Instances 41 to 120 (60 jobs), with the default parameters (2048 MB), a time limit of 1800 s, 4 runs in parallel on a 6-core computer:
```shell
python3 scripts/run_benchmarks.py single_twt_setup_times_cicirello2003 successive-sublimation-dynamic-programming --name ssdp --time-limit-factor 30
```

| Instances | Due dates | Optimal | Mean time (s) | Max time (s) | Paper: mean / max time (s) |
| --- | --- | --- | --- | --- | --- |
| 41–80 | moderate | 40 / 40 | 249 | 611 | 164 / 302 |
| 81–120 | tight | 40 / 40 | 272 | 432 | 178 / 312 |

All the optimal values are those of Tanaka and Araki (2013). The mean times are about 1.5 times those of the paper, on a different computer (Intel Core i7 980X, 512 MB for the networks, in the paper); the mean times vary by up to 20% between two benchmarks of the same version, with 4 runs in parallel. 78 instances are solved in stage 2, without interval branching. With a column generation written directly with HiGHS, run in parallel with these runs, the total time is 1.8% larger (21,224 s against 20,851 s).

The instances 1 to 40 (loose due dates) are much harder: the paper solves them with the improvements of Section 5, in up to several weeks for two of them; 22 have an optimal value of 0. A sample, with a time limit of 1800 s, 3 runs in parallel (with the column generation written directly with HiGHS):

| Instance | Optimal value | Value | Bound | Time (s) | Branching depth |
| --- | --- | --- | --- | --- | --- |
| 1 | 453 | 453 | 453 | 409 | 1 |
| 3 | 1390 | 1410 | 1366 | 1800 | 3 |
| 8 | 100 | 100 | 100 | 302 | 1 |
| 12 | 0 | 0 | 0 | 0 | 0 |
| 15 | 724 | 964 | 695 | 1800 | 6 |
| 24 | 761 | 830 | 736 | 1800 | 2 |

On these instances, the networks need interval branching, and the upper bounds of the local search are far from the optimal values on the unsolved ones.

## Tests

`test/algorithms/successive_sublimation_dynamic_programming_test.cpp`: the optimal values (brute force) of all the single machine instances without release dates of the tests, with and without setup times, for the total (weighted) tardiness and completion time, with five settings: default, conjugate subgradient, and weak upper bounds with a single iteration of stage 1 (so that the optimality is proved in stage 2) with and without constraint propagation, and with forced interval branching. Also checked once, not in the tests: 32 random instances with 9 and 10 jobs against the brute force, with the same settings.

## Source files

* `include/parallelschedulingsolver/algorithms/successive_sublimation_dynamic_programming.hpp`
* `src/algorithms/successive_sublimation_dynamic_programming.cpp`: stages, upper bounds, branching.
* `src/algorithms/ssdp_network.hpp`, `src/algorithms/ssdp_network.cpp`: network, dynamic programming, generation, reduction, constraint propagation.

## References

* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
* S. Tanaka, S. Fujikuma and M. Araki (2009). An exact algorithm for single-machine scheduling without machine idle time. *Journal of Scheduling*, 12, 575–593. [doi:10.1007/s10951-008-0093-5](https://doi.org/10.1007/s10951-008-0093-5)
* S. Tanaka and S. Fujikuma (2012). A dynamic-programming-based exact algorithm for general single-machine scheduling with machine idle time. *Journal of Scheduling*, 15, 347–361. [doi:10.1007/s10951-011-0242-0](https://doi.org/10.1007/s10951-011-0242-0)
* A. Pessoa, E. Uchoa, M. Poggi de Aragão and R. Rodrigues (2010). Exact algorithm over an arc-time-indexed formulation for parallel machine scheduling problems. *Mathematical Programming Computation*, 2, 259–290. [doi:10.1007/s12532-010-0019-z](https://doi.org/10.1007/s12532-010-0019-z)
* V. A. Cicirello (2003). Weighted tardiness scheduling with sequence-dependent setups: a benchmark library. Technical report, Carnegie Mellon University.
