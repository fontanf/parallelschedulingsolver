# Lagrangian relaxation and column generation

`--algorithm lagrangian-relaxation`, `--algorithm column-generation`

Two algorithms for the Lagrangian dual of the time-indexed formulation: the constraints "each job is processed exactly once (or rejected)" are relaxed with Lagrangian multipliers, the relaxation is solved by the [time-indexed dynamic programming](dynamic_programming_time_indexed.md), and the algorithms search for the multipliers giving the best lower bound. They are stage 1 of the [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) algorithm (SSDP), as independent algorithms.

## Supported problems

Single machine, identical or unrelated parallel machines; total (weighted) tardiness or total (weighted) completion time; without release dates; with or without sequence-dependent setup times and rejection costs. On a single machine, those of the [time-indexed dynamic programming](dynamic_programming_time_indexed.md).

## Parallel machines

The relaxation decomposes by machine. The machines are grouped by identical processing times and setup times; each group $g$ of $m_g$ machines has a network, the one of the subproblem of a machine (`time_indexed::build_machine_problem`): the jobs of the instance (none removed), with the processing times and the setup times of the machine, all optional (rejection cost 0 in the subproblem, no lower bound of the makespan: the empty path is a path), without state-space modifiers. With $SP_g(\mu) \le 0$ the length of the shortest path of the network of group $g$, with arc lengths $f_i(t) - \mu_i$, the Lagrangian bound is

$$L(\mu) = \sum_g m_g SP_g(\mu) + \sum_i \mu_i + \sum_{i: e_i \ge 0} \min(0, e_i - \mu_i).$$

The paths are a schedule if each job is processed at most once (the path of a group is counted once for each of its machines), and exactly once if it can't be rejected.

Upper bound of the makespan of a machine (the horizon of its network): the one of a single machine, $\sum_j p_j$ plus the largest total setup time (assignment relaxation). If all the machines are identical, also $(L + (m - 1) q) / m$, where $L = \sum_j (p_j + \max_i s_{ij})$ and $q = \max_j (p_j + \max_i s_{ij})$ (with the initial setup times among the $s_{ij}$): in an optimal schedule minimizing the sum of the completion times among the optimal ones, moving the last job $j$ of a machine $a$ to the end of another machine $b$ doesn't decrease its completion time, so $C_a \le C_b + q$ for all $b \ne a$, and with $\sum_b C_b \le L$, $C_a \le (L - C_a)/(m - 1) + q$.

Network reduction: the cost of a schedule is at least the length of the path of one of its machines (in group $g$) plus $(m_g - 1) SP_g + \sum_{g' \ne g} m_{g'} SP_{g'}$ plus the constant term, so the arcs of the network of group $g$ whose paths are longer than the upper bound minus one minus these terms are removed. The constraint propagation, which assumes that all the jobs are processed on the machine, isn't used. An input network (`network`) is only supported on a single machine.

Initial upper bound (if `upper_bound` is negative): list scheduling (each job on the machine where it completes first, with the setup times) in the orders of the instance, of the weighted shortest processing times (smallest processing time over the machines) and of the earliest due dates.

## Lagrangian relaxation

Conjugate subgradient algorithm with the step size control of Tanaka and Fujikuma (2012), and their parameters $(\gamma^{\text{ini}}, \delta_T, \delta_S, \epsilon, \kappa_S, \kappa_E) = (1.2, n, 2, 0.02, 0.95, 1.1)$ by default. The subgradient of job $i$ is $1 - V_i - R_i$, where $V_i$ is the number of times job $i$ is in the paths of the relaxation (on parallel machines, the path of a group counted once for each of its machines) and $R_i = 1$ if the relaxation rejects it (a job not in $\mathcal{M}$ with $e_i < \mu_i$); it is 0 for the jobs of $\mathcal{M}$. If the subgradient cancels the previous direction, the conjugate direction is restarted from the subgradient. The algorithm stops with the termination criterion of Tanaka and Fujikuma (2012), or when the subgradient is 0.

## Column generation

The Lagrangian dual is solved as the linear relaxation of the master problem (Tanaka and Araki, 2013, Section 5.2)

$$\min \sum_g \sum_{P \in \mathcal{P}_g} c(P) x_P + \sum_{i: e_i \ge 0} e_i y_i \quad \text{s.t.} \quad \sum_g \sum_{P \in \mathcal{P}_g} V_i(P) x_P + y_i = 1 \ \ \forall i, \quad \sum_{P \in \mathcal{P}_g} x_P = m_g \ \ \forall g$$

over the paths $\mathcal{P}_g$ of the network of each group $g$ of identical machines ($m_g$ machines; a single group of one machine on a single machine), with [columngenerationsolver](https://github.com/fontanf/columngenerationsolver) (HiGHS or CLP), where $y_i \in [0, 1]$ is the rejection of job $i$ (a static column). The dynamic programming is the pricing problem, exact: the overcost of the pricing is the sum over the groups of $m_g$ times the reduced cost of the path of the group. On parallel machines, the paths of negative reduced cost are added. The dual is stabilized by Wentges smoothing ($\alpha = 0.5$) with the duals of the best Lagrangian bound as stability center. The first columns are a schedule of cost the upper bound (all the jobs once, and $m_g$ in the convexity row of each group: the schedule uses all the machines, so the combinations of the columns are a relaxation of the schedules), on parallel machines the empty path of each group, and the solution of the relaxation for the initial multipliers.

## Common behavior

At each evaluation of the relaxation:
1. If the paths are a schedule, it is a solution (upper bound).
2. The best lower bound and its multipliers are updated.
3. The callback (`iteration_callback`) is called, with the multipliers, the bound, the paths (one per network), and whether they are feasible and the best lower bound improved; it returns an upper bound, e.g. from a Lagrangian heuristic (in SSDP).
4. The algorithm stops if the bound proves the optimality of the upper bound.
5. If the best lower bound or the upper bound is improved, the networks are reduced with the upper bound minus one (`reduction`), on a single machine with constraint propagation (`constraint_propagation`). If no path remains, no schedule is better than the upper bound.

On a single machine, the network is given as input (`network`, e.g. with state-space modifiers) or is the initial network of the instance. The networks and their machines are part of the output (`networks`, `network_machine_ids`). The upper bound (`upper_bound`) is the value of a known schedule; by default, the best of the schedules of all the jobs in the order of the instance, of the weighted shortest processing times and of the earliest due dates. The step sizes of the subgradient algorithm are proportional to the gap with the upper bound, so a poor upper bound gives poor bounds.

Output: the best Lagrangian bound (`lagrangian_bound`, of the reduced network: it can be larger than the optimal value once no path better than the upper bound remains), its multipliers, the network, the upper bound and the solution, the number of evaluations; the bound of the instance is the smallest of the ceiling of the Lagrangian bound and of the upper bound (if the network isn't restricted).

## Tests

`test/algorithms/lagrangian_dual_test.cpp`, for both algorithms, on the single machine test instances of SSDP (optimal values by brute force):
* valid bounds and feasible solutions, with the default parameters, and with a large upper bound without reduction (large steps);
* with all the jobs in the state-space modifiers of the input network (exact relaxation): the optimal value and its proof;
* with rejection costs for the jobs of even id (instances with at most 6 jobs): valid bounds, and the optimal value with the exact network;
* the column generation bound is at least the Lagrangian relaxation bound (without reduction);
* the callback: called at each evaluation, its upper bound is used;
* unsupported instances.

On parallel machines, `test/algorithms/lagrangian_dual_parallel_test.txt`: identical and unrelated machines, with and without setup times, optimal values by brute force:
* valid bounds and feasible solutions, with the default parameters, and with a large upper bound without reduction;
* one network for each group of identical machines, and one path for each network;
* with rejection costs for the jobs of even id (instances with at most 6 jobs): valid bounds;
* the column generation bound is at least the Lagrangian relaxation bound (without reduction), with a large upper bound and with the upper bound of the initial solution (unless the bound proves the optimality of the upper bound);
* the horizon of the subproblem of a machine, identical and unrelated.

Mean bound over optimal value on these instances, with the default parameters: column generation 0.995 to 1.00 depending on the family; Lagrangian relaxation 0.94 to 1.00 on unrelated machines and 0.59 to 0.87 on identical machines (with identical machines, all the machines of the relaxation process the same path, and the subgradient steps oscillate).

## Source files

* `include/parallelschedulingsolver/algorithms/lagrangian_dual.hpp`: common parameters, output and callback.
* `include/parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp`, `src/algorithms/lagrangian_relaxation.cpp`
* `include/parallelschedulingsolver/algorithms/column_generation.hpp`, `src/algorithms/column_generation.cpp`
* `src/algorithms/lagrangian_dual_internal.hpp`: evaluation, subgradient, bounds, callback and reduction, shared by both, on single and parallel machines.
* `time_indexed::build_machine_problem`, `time_indexed::identical_machines` and `time_indexed::build_parallel_solution` in `src/algorithms/time_indexed_network.cpp`: the subproblem of a machine.

## References

* S. Tanaka and S. Fujikuma (2012). A dynamic-programming-based exact algorithm for general single-machine scheduling with machine idle time. *Journal of Scheduling*, 15, 347–361. [doi:10.1007/s10951-011-0242-0](https://doi.org/10.1007/s10951-011-0242-0)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
* P. Wentges (1997). Weighted Dantzig–Wolfe decomposition for linear mixed-integer programming. *International Transactions in Operational Research*, 4(2), 151–162. [doi:10.1111/j.1475-3995.1997.tb00071.x](https://doi.org/10.1111/j.1475-3995.1997.tb00071.x)
