# Tree search with iterative refinement

`--algorithm tree-search-iterative-refinement`

Tree search on the time-indexed network with the [relaxation refinement](relaxation_refinement.md) at each node: stage 2 of the [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) algorithm (SSDP), as an independent algorithm. SSDP is its stage 1 (the multipliers, by the [Lagrangian dual](lagrangian_dual.md)) followed by this algorithm, with the upper bounds of the local search and of the Lagrangian heuristic.

## Supported problems

Single machine, total (weighted) tardiness or total (weighted) completion time, without release dates, with or without sequence-dependent setup times and rejection costs (those of the time-indexed dynamic programming).

## Principle

Input: the network of the root (`network`, by default the initial network of the instance), the multipliers (`multipliers`), an upper bound (`upper_bound`) and the lower bound of the relaxation of the root (`lower_bound`). If the multipliers, the upper bound or the lower bound aren't given, they are those of the relaxation refinement without refinement step: the Lagrangian dual (column generation, or Lagrangian relaxation with `--column-generation 0`) and the reduction of the network with the upper bound, if the multipliers aren't given.

Iterative deepening on the bound to prove: the relaxation refinement is applied with a tentative upper bound $UB^{\text{tent}}$ as bound to prove, increased by $\Delta = 0.01\, UB$ (`tentative_upper_bound_increment`; $\Delta = 1$ if the lower bound of the root is at most 0) from the lower bound of the root until $UB^{\text{tent}} = UB$. When it is proved, no schedule is better than $UB^{\text{tent}}$, which becomes a lower bound (the callback `lower_bound_callback` is called with it). When $UB^{\text{tent}} = UB$, $UB$ is optimal. The tentative upper bound decreases to the upper bound when it is improved, so the same bound may be proved twice.

Interval branching (Tanaka and Araki, 2013, Section 5.4, `interval_branching`): if the memory (`maximum_memory`, in MB, 2048 by default) isn't sufficient for the relaxation refinement of a node, the completion times of the two jobs with the most states in its network are split into two intervals with balanced numbers of states, and the four restricted networks (reduced with $UB^{\text{tent}}$, with constraint propagation) are the children of the node. The largest depth reached for a tentative upper bound is used from the start for the next one; `minimum_branching_depth` forces the branching from the start (for testing).

The callback of the steps of the relaxation refinements (`step_callback`) returns an upper bound, e.g. from a Lagrangian heuristic (in SSDP).

Output: the solution and the bound, the multipliers, the upper and lower bounds, the number of relaxation refinements, the largest number of modifiers and of states, the largest branching depth.

## Tests

`test/algorithms/tree_search_iterative_refinement_test.cpp`, on the single machine test instances of SSDP (optimal values by brute force):
* the optimal value and its proof: default parameters, Lagrangian relaxation, without constraint propagation, with forced interval branching, and with all the multipliers equal to 0 and a large upper bound;
* a tiny memory, with and without interval branching: valid bounds;
* the callbacks: non-decreasing lower bounds, at most the optimal value;
* with rejection costs for the jobs of even id (instances with at most 6 jobs): the optimal value (brute force);
* unsupported instances.

## Source files

* `include/parallelschedulingsolver/algorithms/tree_search_iterative_refinement.hpp`
* `src/algorithms/tree_search_iterative_refinement.cpp`

## References

* S. Tanaka, S. Fujikuma and M. Araki (2009). An exact algorithm for single-machine scheduling without machine idle time. *Journal of Scheduling*, 12, 575–593. [doi:10.1007/s10951-008-0093-5](https://doi.org/10.1007/s10951-008-0093-5)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
