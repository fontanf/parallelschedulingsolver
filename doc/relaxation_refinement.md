# Relaxation refinement

`--algorithm relaxation-refinement`

With fixed Lagrangian multipliers, the relaxation (LR2) of the [time-indexed dynamic programming](dynamic_programming_time_indexed.md) is refined by adding jobs to the set $\mathcal{M}$ of the state-space modifiers, until a bound to prove is proved or an early termination criterion is met. It is the subprocedure of stage 2 of the [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) algorithm (SSDP), as an independent algorithm, so that a tree search can call it at each node and set its early termination parameters.

## Supported problems

Single machine, total (weighted) tardiness or total (weighted) completion time, without release dates, with or without sequence-dependent setup times and rejection costs (those of the time-indexed dynamic programming).

## Principle

Input: a network (`network`, by default the initial network of the instance), the multipliers (`multipliers`), an upper bound (`upper_bound`, the value of a known schedule) and the bound to prove (`bound_to_prove`, by default the upper bound: the optimality of the upper bound).

If the multipliers aren't given, they are those of the [Lagrangian dual](lagrangian_dual.md) of the network (column generation, or Lagrangian relaxation with `--column-generation 0`), and the network is then reduced with the upper bound, as in SSDP after stage 1. If the upper bound isn't given, it is the one of the Lagrangian dual, or, with input multipliers, the best of the schedules of all the jobs in the orders of the instance, of the weighted shortest processing times and of the earliest due dates.

Each step:
1. Stop if the gap between the bound to prove and the lower bound of the current network is smaller than one: no schedule has a value smaller than the bound to prove (status `BoundProved`).
2. Choose at most three jobs to add to $\mathcal{M}$ (fewer when the networks use more memory), as in SSDP (Tanaka et al., 2009): if the networks use little memory, the jobs that don't occur in the shortest path, then those occurring more than once, the jobs with the fewest states first; otherwise, the jobs with the fewest states.
3. Generate the network of the new $\mathcal{M}$ from the previous one, by forward or backward dynamic programming in turns, without the states whose paths are longer than the bound to prove minus one (the cutoff), with constraint propagation (`constraint_propagation`).
4. If the shortest path is a schedule, it is a solution. The callback (`step_callback`) is called with the network, the bound, the path, its direction and whether the best lower bound is improved; it returns an upper bound, e.g. from a Lagrangian heuristic (in SSDP). The bound to prove is decreased to the upper bound.

Early termination:
* time limit (`TimeOut`);
* maximum memory of the networks (`maximum_memory`, in MB, 2048 by default; `MemoryOut`);
* maximum number of steps (`maximum_number_of_steps`, no limit by default; `MaximumNumberOfSteps`);
* tailing-off (`tailing_off_number_of_steps` $k$ and `tailing_off_minimum_improvement` $\epsilon$; none by default): stop if, over the last $k$ steps, the best lower bound improved by less than $\epsilon$ times the gap with the bound to prove at the start of these steps (`TailingOff`).

Output: the status, the best lower bound of the networks (`lagrangian_bound`), the bound to prove at the end, the network of the last step (to branch on it, e.g. with its number of states by job and time, and to warm start), the multipliers, a solution and the upper bound, the number of steps and of modifiers, the largest number of states. The bound of the instance (if the input network isn't restricted) is the bound to prove if it is proved, and the smallest of the ceiling of the best lower bound and of the bound to prove otherwise: all the schedules of value smaller than the bound to prove are paths of the networks.

## Tests

`test/algorithms/relaxation_refinement_test.cpp`, on the single machine test instances of SSDP (optimal values by brute force):
* exact without early termination: the optimal value and its proof, with the multipliers of the column generation, of the Lagrangian relaxation, and with all the multipliers equal to 0 and a large upper bound;
* a bound to prove smaller than the optimal value: proved;
* early termination (one step, a tiny memory, a tailing-off closing the gap at each step), with the multipliers equal to 0 and a large upper bound: valid bounds, the status of the termination, which occurs on some instances for each criterion;
* the callback: called at each step, its upper bound becomes the bound to prove;
* with rejection costs for the jobs of even id (instances with at most 6 jobs): the optimal value (brute force);
* unsupported instances.

## Source files

* `include/parallelschedulingsolver/algorithms/relaxation_refinement.hpp`
* `src/algorithms/relaxation_refinement.cpp`

## References

* S. Tanaka, S. Fujikuma and M. Araki (2009). An exact algorithm for single-machine scheduling without machine idle time. *Journal of Scheduling*, 12, 575–593. [doi:10.1007/s10951-008-0093-5](https://doi.org/10.1007/s10951-008-0093-5)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
