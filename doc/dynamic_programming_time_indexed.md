# Dynamic programming on the time-indexed network

`--algorithm dynamic-programming-time-indexed`

The dynamic programming of the [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) algorithm (SSDP) of Tanaka et al., as an independent algorithm: one call computes a lower bound (and possibly an optimal schedule) on a time-indexed network, starting from the network of a previous call. SSDP is a sequence of calls to this algorithm.

## Supported problems

Single machine, without release dates, with or without sequence-dependent setup times, without rejection costs:
* total weighted tardiness $1 \mid s_{ij} \mid \sum w_j T_j$;
* total weighted completion time $1 \mid s_{ij} \mid \sum w_j C_j$.

At most 127 jobs (after the removal of the zero cost jobs).

## Principle

A schedule without idle time is a path in a time-expanded network: a node $v_{it}$ is the completion of job $i$ at time $t$, and an arc from $v_{j, t - s_{ji} - p_i}$ to $v_{it}$ means that job $i$ is processed right after job $j$. The constraints "each job is processed exactly once" are relaxed with Lagrangian multipliers $\mu_i$ (the length of an arc into $v_{it}$ is $f_i(t) - \mu_i$), except for the jobs of a set $\mathcal{M}$, which are processed exactly once on every path thanks to state-space modifiers: the nodes become $v^b_{it}$, where $b$ is the set of the jobs of $\mathcal{M}$ processed up to job $i$. The dynamic programming computes the shortest paths with the constraint that a job is not processed twice in three successive positions (relaxation (LR2) of Tanaka and Araki, 2013), from the source (forward labels) or to the sink (backward labels).

The length of the shortest path plus $\sum_i \mu_i$ is a lower bound of the cost of the schedules of the network. With $\mathcal{M}$ containing all the jobs, the shortest path is an optimal schedule.

See [the SSDP documentation](successive_sublimation_dynamic_programming.md) for the details of the network, of its generation, of its reduction and of the constraint propagation.

## One call

One call performs, in this order:
1. Network: the network given as input (`parameters.network`, modified in place), or the initial network of the instance ($\mathcal{M} = \emptyset$, the zero cost jobs removed with `remove_zero_cost_jobs`).
2. Restriction of the completion times of some jobs (`completion_time_windows`), as in the interval branching of SSDP.
3. Generation of the network with more jobs in $\mathcal{M}$ (`new_modifier_jobs`, or `all_jobs_in_modifiers`), in a direction (`direction`), from the labels of the other direction of the network (computed if needed). The states through which all the paths are longer than `cutoff` are not generated. If the memory (`maximum_memory`) is not sufficient, the call stops (`memory_out`).
4. Reduction of the network with `cutoff` (`reduction`), from the labels of both directions, and constraint propagation (`constraint_propagation`).
5. Labels of `direction` (and of the other direction with `both_directions_labels`), lower bound (`lagrangian_bound`) and shortest path (`path`), unless `compute_bound` is false.

The network keeps track of the multipliers of its labels, so the labels are only computed if they are not up to date: a sequence of calls does the same work as the network operations it is made of. It also keeps track of whether it has been restricted, and of the smallest cutoff it has been reduced or generated with.

## Output

* `network`: the network at the end, to pass to the next call.
* `lagrangian_bound`, `path` (jobs of the problem, i.e. without the removed jobs).
* `solution`: the schedule of the shortest path if it processes each job exactly once, followed by the removed jobs.
* `bound()`: if the network isn't restricted, the smallest of $\lceil$ `lagrangian_bound` $\rceil$ and of $\lfloor$cutoff$\rfloor + 1$, where cutoff is the smallest cutoff of the network: all the schedules of cost at most the cutoff are paths of the network (the costs are integers).
* `memory_out`, `number_of_states`.

From the command line, the algorithm computes the bound of (LR2) with multipliers equal to 0, or solves the instance with `--all-jobs-in-modifiers 1` (only for small instances: up to $2^n$ states per node).

## Public types

`include/parallelschedulingsolver/algorithms/time_indexed_network.hpp` (namespace `parallelschedulingsolver::time_indexed`): the problem (`Problem`, `build_problem`, `build_solution`) and the network (`Network`), with its operations (labels, shortest paths, reduction, constraint propagation, restriction, generation) and its statistics (number of states by job and by time, time windows, memory), used by SSDP to choose the jobs of $\mathcal{M}$ and to branch.

## Tests

`test/algorithms/dynamic_programming_time_indexed_test.cpp`: the optimal values (brute force) of the single machine test instances of SSDP, with all the jobs in $\mathcal{M}$ at once, and with a chain of three calls ((LR2) with reduction and constraint propagation, then half of the jobs in $\mathcal{M}$ backward, then the other half forward, with a cutoff); the bound of (LR2) on the same instances; a restricted network, whose bound is not a bound of the instance; the memory limit; the unsupported instances.

## Source files

* `include/parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp`, `src/algorithms/dynamic_programming_time_indexed.cpp`
* `include/parallelschedulingsolver/algorithms/time_indexed_network.hpp`, `src/algorithms/time_indexed_network.cpp`

## References

* S. Tanaka, S. Fujikuma and M. Araki (2009). An exact algorithm for single-machine scheduling without machine idle time. *Journal of Scheduling*, 12, 575–593. [doi:10.1007/s10951-008-0093-5](https://doi.org/10.1007/s10951-008-0093-5)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
