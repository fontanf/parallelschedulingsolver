# Lagrangian relaxation and column generation

`--algorithm lagrangian-relaxation`, `--algorithm column-generation`

Two algorithms for the Lagrangian dual of the time-indexed formulation: the constraints "each job is processed exactly once (or rejected)" are relaxed with Lagrangian multipliers, the relaxation is solved by the [time-indexed dynamic programming](dynamic_programming_time_indexed.md), and the algorithms search for the multipliers giving the best lower bound. They are stage 1 of the [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) algorithm (SSDP), as independent algorithms.

## Supported problems

Those of the [time-indexed dynamic programming](dynamic_programming_time_indexed.md): single machine, total (weighted) tardiness or total (weighted) completion time, without release dates, with or without sequence-dependent setup times and rejection costs.

## Lagrangian relaxation

Conjugate subgradient algorithm with the step size control of Tanaka and Fujikuma (2012), and their parameters $(\gamma^{\text{ini}}, \delta_T, \delta_S, \epsilon, \kappa_S, \kappa_E) = (1.2, n, 2, 0.02, 0.95, 1.1)$ by default. The subgradient of job $i$ is $1 - V_i - R_i$, where $V_i$ is the number of times job $i$ is in the path of the relaxation and $R_i = 1$ if the relaxation rejects it (a job not in $\mathcal{M}$ with $e_i < \mu_i$); it is 0 for the jobs of $\mathcal{M}$. If the subgradient cancels the previous direction, the conjugate direction is restarted from the subgradient. The algorithm stops with the termination criterion of Tanaka and Fujikuma (2012), or when the subgradient is 0.

## Column generation

The Lagrangian dual is solved as the linear relaxation of the master problem (Tanaka and Araki, 2013, Section 5.2)

$$\min \sum_P L(P) x_P + \sum_{i: e_i \ge 0} e_i y_i \quad \text{s.t.} \quad \sum_P V_i(P) x_P + y_i = 1 \ \ \forall i, \quad \sum_P x_P = 1$$

over the paths $P$ of the network, with [columngenerationsolver](https://github.com/fontanf/columngenerationsolver) (HiGHS or CLP), where $y_i \in [0, 1]$ is the rejection of job $i$ (a static column). The dynamic programming is the pricing problem, exact: the reduced cost of its path is the overcost of the pricing. The dual is stabilized by Wentges smoothing ($\alpha = 0.5$) with the duals of the best Lagrangian bound as stability center. The first columns are a schedule of cost the upper bound, and the solution of the relaxation for the initial multipliers.

## Common behavior

At each evaluation of the relaxation:
1. If the path is a schedule, it is a solution (upper bound).
2. The best lower bound and its multipliers are updated.
3. The callback (`iteration_callback`) is called, with the multipliers, the bound, the path, and whether the path is feasible and the best lower bound improved; it returns an upper bound, e.g. from a Lagrangian heuristic (in SSDP).
4. The algorithm stops if the bound proves the optimality of the upper bound.
5. If the best lower bound or the upper bound is improved, the network is reduced with the upper bound minus one (`reduction`), with constraint propagation (`constraint_propagation`). If no path remains, no schedule is better than the upper bound.

The network is given as input (`network`, e.g. with state-space modifiers) or is the initial network of the instance, and is part of the output. The upper bound (`upper_bound`) is the value of a known schedule; by default, the best of the schedules of all the jobs in the order of the instance, of the weighted shortest processing times and of the earliest due dates. The step sizes of the subgradient algorithm are proportional to the gap with the upper bound, so a poor upper bound gives poor bounds.

Output: the best Lagrangian bound (`lagrangian_bound`, of the reduced network: it can be larger than the optimal value once no path better than the upper bound remains), its multipliers, the network, the upper bound and the solution, the number of evaluations; the bound of the instance is the smallest of the ceiling of the Lagrangian bound and of the upper bound (if the network isn't restricted).

## Tests

`test/algorithms/lagrangian_dual_test.cpp`, for both algorithms, on the single machine test instances of SSDP (optimal values by brute force):
* valid bounds and feasible solutions, with the default parameters, and with a large upper bound without reduction (large steps);
* with all the jobs in the state-space modifiers of the input network (exact relaxation): the optimal value and its proof;
* with rejection costs for the jobs of even id (instances with at most 6 jobs): valid bounds, and the optimal value with the exact network;
* the column generation bound is at least the Lagrangian relaxation bound (without reduction);
* the callback: called at each evaluation, its upper bound is used;
* unsupported instances.

## Source files

* `include/parallelschedulingsolver/algorithms/lagrangian_dual.hpp`: common parameters, output and callback.
* `include/parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp`, `src/algorithms/lagrangian_relaxation.cpp`
* `include/parallelschedulingsolver/algorithms/column_generation.hpp`, `src/algorithms/column_generation.cpp`
* `src/algorithms/lagrangian_dual_internal.hpp`: evaluation, bounds, callback and reduction, shared by both.

## References

* S. Tanaka and S. Fujikuma (2012). A dynamic-programming-based exact algorithm for general single-machine scheduling with machine idle time. *Journal of Scheduling*, 15, 347–361. [doi:10.1007/s10951-011-0242-0](https://doi.org/10.1007/s10951-011-0242-0)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
* P. Wentges (1997). Weighted Dantzig–Wolfe decomposition for linear mixed-integer programming. *International Transactions in Operational Research*, 4(2), 151–162. [doi:10.1111/j.1475-3995.1997.tb00071.x](https://doi.org/10.1111/j.1475-3995.1997.tb00071.x)
