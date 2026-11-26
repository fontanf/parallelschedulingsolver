# Roadmap

Algorithms planned for this repository, in particular state-of-the-art ones, grouped by problem. Each entry gives the reference and what to implement from it.

Status: **planned**, **in progress** or **done** (with the `--algorithm` option).

## Several problems

* **Done**: time-indexed MILP with normal instants, `--algorithm milp-time-indexed`. All the objectives of the repository on single, identical and unrelated machines, with release dates. See [`milp_time_indexed.md`](milp_time_indexed.md).
* **Done**: assignment MILP, `--algorithm milp-assignment`. Makespan with release dates and maximum lateness, on single, identical and unrelated machines. See [`milp_assignment.md`](milp_assignment.md).
* **Done**: positional MILP, `--algorithm milp-positional`. All the objectives of the repository on single, identical and unrelated machines, with release dates; its size doesn't depend on the processing times. See [`milp_positional.md`](milp_positional.md).
* **Done** (`--algorithm local-search`, see [`local_search.md`](local_search.md)): A. Kramer and A. Subramanian (2019). A unified heuristic and an annotated bibliography for a large class of earliness–tardiness scheduling problems. *Journal of Scheduling*, 22, 21–57. [doi:10.1007/s10951-017-0549-6](https://doi.org/10.1007/s10951-017-0549-6)
  * Done: local search (UILS) for $\sum w_j T_j$ and $\sum w_j C_j$, on single and parallel machines, with release dates and sequence-dependent setup times. Results matching the paper on the $P \mid\mid \sum T_j$ instances of Tanaka and Araki (2008).
  * Planned: the amortized $O(1)$ move evaluations for parallel machines without release dates (Section 4.2.3); the moves are evaluated in $O(n)$ time.
  * Planned: replacing the multi-start by a population-based iterated local search, as in shopschedulingsolver.
  * Missing benchmarks: the $P \mid\mid \sum w_j T_j$ instances of Rodrigues et al. (2008) and the $R \mid\mid \sum w_j T_j$ instances of Şen and Bülbül (2015).

## Relaxation refinement framework

**Planned**: split [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md) into independent algorithms, so that several published methods become configurations of the same components:
* **Done**: problem variant: a rejection cost for each job (`rejection_cost`, -1 by default, i.e. the job is mandatory), with the objective $C_{\max} + \sum e_j$, $\sum w_j C_j + \sum e_j$ or $\sum w_j T_j + \sum e_j$ over the rejected jobs, supported by the brute force for the tests, and by the generator (`--rejection-costs-range`). The rejection costs are non-negative integers: the Lagrangian multipliers, real numbers, are not rejection costs of the instance, but an input of the dynamic programming algorithm. With the rejection costs equal to the Lagrangian multipliers of the constraints "each job exactly once", it is the Lagrangian subproblem where each job is processed at most once.
* **Done**: the dynamic programming as an independent algorithm, [`dynamic-programming-time-indexed`](dynamic_programming_time_indexed.md), with the network as input and output (public types in `time_indexed_network.hpp`), called by SSDP; (LR2) and state-space modifiers, with rejection costs (not in SSDP yet: its upper bounds, from the local search, don't support them).
* Planned: in the dynamic programming algorithm, a relaxation level:
  * (LR1): jobs processed any number of times (weighted stable set on an interval graph without setup times);
  * (LR2): no job processed twice in three successive nodes;
  * state-space modifiers on a set of jobs $\mathcal{M}$ (each job of $\mathcal{M}$ at most once), up to $\mathcal{M} = \mathcal{N}$ (exact).

  Exact mode: pruning by upper bound and interval branching when the memory is not sufficient. Idle time and release dates (idle arcs, Tanaka and Fujikuma, 2012). The limit of 127 jobs only applies with modifiers.

  To keep the performance of the current implementation, the algorithm takes optional structures as input and outputs them: the reduced network (states eliminated by the upper bound stay eliminated across calls), the labels of both directions (used to generate the network of the next set $\mathcal{M}$ from the previous one), and the time windows of constraint propagation.
* **Done**: Lagrangian relaxation (conjugate subgradient) and column generation (columngenerationsolver) as independent algorithms using this pricing algorithm, [`lagrangian-relaxation` and `column-generation`](lagrangian_dual.md), with rejection costs, on single and parallel (identical or unrelated) machines; SSDP stage 1 calls them. On parallel machines, a network for each group of identical machines, of the subproblem of a machine where all the jobs are optional (rejection cost 0, the rejection costs of the instance are in the master problem or in the constant term of the relaxation), and a convexity row equal to the number of machines of the group. Planned: state-space modifiers on parallel machines (the sink cost of a job of M not processed on the machine is then its multiplier, a real-valued rejection cost), and diving heuristics.

  On a single machine, the master selects a single column, so the constraints "each job exactly once" can be moved entirely into the subproblem; on parallel machines they link the columns, and only "at most once on each machine" can be (as ng-routes in vehicle routing).
* **Done** (single machine): [relaxation refinement](relaxation_refinement.md) algorithm `relaxation-refinement`, the subprocedure of SSDP stage 2, which calls it. Planned: parallel machines (with state-space modifiers on parallel machines). Relaxation refinement algorithm: the multipliers (Lagrangian relaxation or column generation), then the refinement of the relaxation, with a cutoff (the bound to prove, e.g. the upper bound minus one, or the tentative upper bound of stage 2) and early termination parameters: maximum memory, maximum number of refinement steps, time limit, and a tailing-off rule on the bound improvement per network growth. Output: the bound, whether the cutoff is proved, a solution, and the structures needed to branch (the current network and its number of states by job and time) and to warm start the children (multipliers, network).
* Tree search algorithm calling the relaxation refinement algorithm at each node, and setting its early termination parameters: a node restricts the completion times of some jobs (interval branching), and the network of a child is the restriction of the network of its parent. The choice between refining (adding jobs to $\mathcal{M}$) and branching is then given by these parameters; currently, refinement while the memory is sufficient. Possibly with the multipliers re-optimized at each node, as in branch-and-price. The loop of stage 2 on the tentative upper bound is an iterative deepening on the cutoff.

Published methods as configurations:
* Avella, Boccia and D'Auria (2005): Lagrangian relaxation + (LR1) + Lagrangian heuristic, with idle time.
* Tanaka, Fujikuma and Araki (2009), Tanaka and Fujikuma (2012), Tanaka and Araki (2013): Lagrangian relaxation or column generation + (LR2), then refinement with state-space modifiers.
* Pessoa et al. (2010): column generation on parallel machines, with branching.

## Single machine

$1 \mid r_j \mid \sum w_j C_j$
* **Planned**: P. Avella, M. Boccia and B. D'Auria (2005). Near-optimal solutions of large-scale single-machine scheduling problems. *INFORMS Journal on Computing*, 17(2), 183–191. [doi:10.1287/ijoc.1040.0069](https://doi.org/10.1287/ijoc.1040.0069)
  * Lagrangian heuristic on the time-indexed formulation: relaxing the job constraints gives a weighted stable set problem on an interval graph, solved by dynamic programming. It builds on the time-indexed model of `milp-time-indexed` (and can use its normal instants).
  * The relaxation (LR1) of the [relaxation refinement framework](#relaxation-refinement-framework), without refinement; requires idle time and release dates in the network, and stage 1 without the limit of 127 jobs.

$1 \mid\mid \sum w_j T_j$
* **Suggested**: R. K. Congram, C. N. Potts and S. L. van de Velde (2002). An iterated dynasearch algorithm for the single-machine total weighted tardiness scheduling problem. *INFORMS Journal on Computing*, 14(1), 52–67. [doi:10.1287/ijoc.14.1.52.7712](https://doi.org/10.1287/ijoc.14.1.52.7712)
  * Iterated dynasearch: compound moves of independent swaps, found by dynamic programming.
* **Suggested**: A. Grosso, F. Della Croce and R. Tadei (2004). An enhanced dynasearch neighborhood for the single-machine total weighted tardiness scheduling problem. *Operations Research Letters*, 32(1), 68–72. [doi:10.1016/S0167-6377(03)00064-6](https://doi.org/10.1016/S0167-6377(03)00064-6)
  * Enhanced dynasearch neighborhood (swaps and insertions).
* **Suggested**: Ö. Ergun and J. B. Orlin (2006). Fast neighborhood search for the single machine total weighted tardiness problem. *Operations Research Letters*, 34(1), 41–45. [doi:10.1016/j.orl.2005.01.010](https://doi.org/10.1016/j.orl.2005.01.010)
  * Faster evaluation of the dynasearch neighborhood.
* Benchmark: the OR-Library instances wt40, wt50 and wt100 of H. A. J. Crauwels, C. N. Potts and L. N. Van Wassenhove (1998). Local search heuristics for the single machine total weighted tardiness scheduling problem. *INFORMS Journal on Computing*, 10(3), 341–350. [doi:10.1287/ijoc.10.3.341](https://doi.org/10.1287/ijoc.10.3.341)

$1 \mid r_j \mid \sum w_j C_j$, $1 \mid r_j \mid \sum w_j T_j$, $1 \mid\mid \sum w_j T_j$ (exact)
* **Suggested**: S. Tanaka, S. Fujikuma and M. Araki (2009). An exact algorithm for single-machine scheduling without machine idle time. *Journal of Scheduling*, 12, 575–593. [doi:10.1007/s10951-008-0093-5](https://doi.org/10.1007/s10951-008-0093-5)
* **Suggested**: S. Tanaka and S. Fujikuma (2012). A dynamic-programming-based exact algorithm for general single-machine scheduling with machine idle time. *Journal of Scheduling*, 15, 347–361. [doi:10.1007/s10951-011-0242-0](https://doi.org/10.1007/s10951-011-0242-0)
  * Successive sublimation dynamic programming (SSDP) on the time-indexed formulation with Lagrangian relaxation: the state-of-the-art exact method for these problems (instances with hundreds of jobs).

## Identical parallel machines

$P \mid\mid \sum w_j C_j$
* **Suggested**: A. Kramer, M. Dell'Amico and M. Iori (2019). Enhanced arc-flow formulations to minimize weighted completion time on identical parallel machines. *European Journal of Operational Research*, 275(1), 67–79. [doi:10.1016/j.ejor.2018.11.039](https://doi.org/10.1016/j.ejor.2018.11.039)
  * Arc-flow MILP with time-indexed arcs, which also relies on reducing the set of start times.

$P \mid r_j \mid \sum w_j C_j$
* **Suggested**: A. Kramer, M. Dell'Amico, D. Feillet and M. Iori (2020). Scheduling jobs with release dates on identical parallel machines by minimizing the total weighted completion time. *Computers & Operations Research*, 123, 105018. [doi:10.1016/j.cor.2020.105018](https://doi.org/10.1016/j.cor.2020.105018)

$P \mid\mid \sum w_j T_j$, $R \mid\mid \sum w_j T_j$ (exact)
* **Suggested**: A. Pessoa, E. Uchoa, M. Poggi de Aragão and R. Rodrigues (2010). Exact algorithm over an arc-time-indexed formulation for parallel machine scheduling problems. *Mathematical Programming Computation*, 2, 259–290. [doi:10.1007/s12532-010-0019-z](https://doi.org/10.1007/s12532-010-0019-z)
* **Suggested**: D. Oliveira and A. Pessoa (2020). An improved branch-cut-and-price algorithm for parallel machine scheduling problems. *INFORMS Journal on Computing*, 32(1), 90–100. [doi:10.1287/ijoc.2018.0854](https://doi.org/10.1287/ijoc.2018.0854)
  * Branch-cut-and-price over the arc-time-indexed formulation: the state-of-the-art exact method, but a large implementation effort.

## Sequence-dependent setup times

$1 \mid s_{ij} \mid \sum w_j T_j$ (exact)
* **Implemented**: S. Tanaka and M. Araki (2013), [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md).
  * Node compression (Section 3.2.3), and the modified and iterated dynasearch for the upper bounds (Sections 3.3 and 5.3), are not implemented.
* Benchmark: the instances of Cicirello (2003), `single_twt_setup_times_cicirello2003`.

$1 \mid s_{ij} \mid \sum w_j T_j$
* **Suggested**: C.-J. Liao, H.-H. Tsou and K.-L. Huang (2012). Neighborhood search procedures for single machine tardiness scheduling with sequence-dependent setups. *Theoretical Computer Science*, 434, 45–52. [doi:10.1016/j.tcs.2012.01.043](https://doi.org/10.1016/j.tcs.2012.01.043)
  * Amortized $O(1)$ move evaluations with setup times (Section 4.2.4 of Kramer and Subramanian, 2019).

$R \mid s_{ijk} \mid C_{\max}$
* **Suggested**: E. Vallada and R. Ruiz (2011). A genetic algorithm for the unrelated parallel machine scheduling problem with sequence dependent setup times. *European Journal of Operational Research*, 211(3), 612–622. [doi:10.1016/j.ejor.2011.01.011](https://doi.org/10.1016/j.ejor.2011.01.011)
* Benchmark: the instances of Vallada and Ruiz (2011), `unrelated_makespan_setup_times_vallada2011`. No algorithm of the repository supports the makespan with setup times yet; the best known values of the large instances date from 2011 and have been improved since.

## Unrelated parallel machines

$R \mid\mid C_{\max}$
* **Suggested**: L. Fanjul-Peyro and R. Ruiz (2010). Iterated greedy local search methods for unrelated parallel machine scheduling. *European Journal of Operational Research*, 207(1), 55–69. [doi:10.1016/j.ejor.2010.03.030](https://doi.org/10.1016/j.ejor.2010.03.030)
* **Suggested**: L. Fanjul-Peyro and R. Ruiz (2011). Size-reduction heuristics for the unrelated parallel machines scheduling problem. *Computers & Operations Research*, 38(1), 301–309. [doi:10.1016/j.cor.2010.05.005](https://doi.org/10.1016/j.cor.2010.05.005)
* Benchmark: the instances of Fanjul-Peyro and Ruiz (2010).

$R \mid\mid \sum w_j C_j$
* **Planned**: J. Chen, A. Sahli, K. Li, F. Xie, C. Chu and N. Xie (2026). An efficient metaheuristic algorithm for unrelated parallel machine scheduling to minimize total weighted completion time. *Computers & Operations Research*, 193, 107530. [doi:10.1016/j.cor.2026.107530](https://doi.org/10.1016/j.cor.2026.107530)
  * Discrete adaptive particle swarm optimization with a dynamic-programming-based local search (DAPSODP).
* **Suggested**: K. Bülbül and H. Şen (2017). An exact extended formulation for the unrelated parallel machine total weighted completion time problem. *Journal of Scheduling*, 20, 373–389. [doi:10.1007/s10951-016-0485-x](https://doi.org/10.1007/s10951-016-0485-x)
  * Exact Benders decomposition-based algorithm.
* **Suggested**: M. Skutella (2001). Convex quadratic and semidefinite programming relaxations in scheduling. *Journal of the ACM*, 48(2), 206–242. [doi:10.1145/375827.375840](https://doi.org/10.1145/375827.375840)
  * Convex quadratic relaxation (CQPR): the lower bound used by Chen et al. (2026).
* Benchmarks: the instances of Bülbül and Şen (2017) and of X. Wang and B. Alidaee (2019), *Omega*, 83, 261–274 ([doi:10.1016/j.omega.2018.07.005](https://doi.org/10.1016/j.omega.2018.07.005); to check that this is the paper cited by Chen et al.).
