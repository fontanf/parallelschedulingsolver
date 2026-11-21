# Roadmap

Algorithms planned for this repository, in particular state-of-the-art ones, grouped by problem. Each entry gives the reference and what to implement from it.

Status: **planned**, **in progress** or **done** (with the `--algorithm` option).

## Several problems

* **Done**: time-indexed MILP with normal instants, `--algorithm milp-time-indexed`. All the objectives of the repository on single, identical and unrelated machines, with release dates. See [`milp_time_indexed.md`](milp_time_indexed.md).
* **Done**: assignment MILP, `--algorithm milp-assignment`. Makespan with release dates and maximum lateness, on single, identical and unrelated machines. See [`milp_assignment.md`](milp_assignment.md).
* **Done**: positional MILP, `--algorithm milp-positional`. All the objectives of the repository on single, identical and unrelated machines, with release dates; its size doesn't depend on the processing times. See [`milp_positional.md`](milp_positional.md).
* **Done** (`--algorithm local-search`, see [`local_search.md`](local_search.md)): A. Kramer and A. Subramanian (2019). A unified heuristic and an annotated bibliography for a large class of earliness–tardiness scheduling problems. *Journal of Scheduling*, 22, 21–57. [doi:10.1007/s10951-017-0549-6](https://doi.org/10.1007/s10951-017-0549-6)
  * Done: local search (UILS) for $\sum w_j T_j$ and $\sum w_j C_j$, on single and parallel machines, with release dates. Results matching the paper on the $P \mid\mid \sum T_j$ instances of Tanaka and Araki (2008).
  * Planned: the amortized $O(1)$ move evaluations for parallel machines without release dates (Section 4.2.3); the moves are evaluated in $O(n)$ time.
  * Planned: replacing the multi-start by a population-based iterated local search, as in shopschedulingsolver.
  * Missing benchmarks: the $P \mid\mid \sum w_j T_j$ instances of Rodrigues et al. (2008) and the $R \mid\mid \sum w_j T_j$ instances of Şen and Bülbül (2015).

## Single machine

$1 \mid r_j \mid \sum w_j C_j$
* **Planned**: P. Avella, M. Boccia and B. D'Auria (2005). Near-optimal solutions of large-scale single-machine scheduling problems. *INFORMS Journal on Computing*, 17(2), 183–191. [doi:10.1287/ijoc.1040.0069](https://doi.org/10.1287/ijoc.1040.0069)
  * Lagrangian heuristic on the time-indexed formulation: relaxing the job constraints gives a weighted stable set problem on an interval graph, solved by dynamic programming. It builds on the time-indexed model of `milp-time-indexed` (and can use its normal instants).

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
