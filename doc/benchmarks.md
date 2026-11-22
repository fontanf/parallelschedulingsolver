# Benchmark instances

The benchmark instances of the repository, with their origin, the problems they were designed for, and how they were generated. They are listed in the CSV files `data/data_<problem>.csv`, with their format, a time limit, the best known solution value and bound, and the results reported in the literature, and are downloaded with `scripts/download_data.py` (the archives are listed in `data/data.json`). The benchmarks are run with `scripts/run_benchmarks.py` (see the [README](../README.md)).

| Benchmark | Problem | Instances | Jobs | Machines | Reference values |
| --- | --- | --- | --- | --- | --- |
| `identical_tt_tanaka2008` | $P \mid\mid \sum T_j$ | 2,250 | 20, 25 | 2 to 10 | optimal (all) |
| `single_twt_setup_times_cicirello2003` | $1 \mid s_{ij} \mid \sum w_j T_j$ | 120 | 60 | 1 | optimal (all) |
| `unrelated_makespan_setup_times_vallada2011` | $R \mid s_{ijk} \mid C_{\max}$ | 1,640 | 6 to 250 | 2 to 30 | optimal (640 small), best known (1,000 large) |

The test instances, generated for the unit tests, are described in [tests.md](tests.md).

## Tanaka and Araki (2008): $P \mid\mid \sum T_j$

* Archive `tanaka2008`, format `tanaka2008`, `data/data_identical_tt.csv`.
* Origin: the instances of the branch-and-bound algorithm with Lagrangian relaxation of Tanaka and Araki (2008), distributed by S. Tanaka under the Apache License 2.0 (`data/tanaka2008/LICENSE.TXT`).
* Designed for: total tardiness on identical parallel machines, without release dates nor setup times. The jobs have unit weights.
* Size: $n \in \{20, 25\}$ jobs, $m \in \{2, \dots, 10\}$ machines; for each $(n, m)$, 5 instances for each of the 25 combinations of the due date parameters, i.e. 125 instances, and 2,250 in total. Path: `tanaka2008/n=<n>/m=<m>/<n>_<m>_<tau>_<RDD>_<instance>.dat`.
* Generation: integer processing times uniform in $[1, 100]$; due dates by the rule of Potts and Van Wassenhove (1985) with the total processing time divided by the number of machines, uniform in $[P(1 - \tau - RDD/2),\ P(1 - \tau + RDD/2)]$ with $P = \sum_j p_j / m$, for the tardiness factor $\tau \in \{0.2, 0.4, 0.6, 0.8, 1.0\}$ and the relative range of due dates $RDD \in \{0.2, 0.4, 0.6, 0.8, 1.0\}$ (given in the first line of each file). A large $\tau$ gives tight due dates; due dates can be 0. These rules hold on all the instances.
* Reference values: the optimal values of Tanaka and Araki (2008), with the running times of their algorithm (column `tanaka2008 / Time`).
* Time limit of the CSV file: 10 s (a choice of the repository, not of the paper).
* Results in the repository: [local search](local_search.md).

## Cicirello (2003): $1 \mid s_{ij} \mid \sum w_j T_j$

* Archive `cicirello2003`, format `cicirello2003`, `data/data_single_twt_setup_times.csv`.
* Origin: the benchmark library of Cicirello (2003), MIT License (`data/cicirello2003/LICENSE`); see https://www.cicirello.org/datasets/wtsds/.
* Designed for: total weighted tardiness on a single machine with sequence-dependent setup times, as a benchmark for heuristics and stochastic search. Cicirello's best known values were improved by many heuristics, then proved optimal by Tanaka and Araki (2013).
* Size: 60 jobs; 10 instances for each of the 12 combinations of the parameters, i.e. 120 instances `wt_sds_<1..120>.instance`.
* Generation: procedure of Lee, Bhaskaran and Pinedo (1997), whose instances were not public, with three parameters: the due date tightness factor $\tau \in \{0.3, 0.6, 0.9\}$, the due date range factor $R \in \{0.25, 0.75\}$ and the setup time severity factor $\eta \in \{0.25, 0.75\}$. The parameters of each instance are given in its header. Observed in the files:
  * processing times in $[50, 150]$ (mean $\bar p = 100$);
  * weights in $[0, 10]$: some jobs have a zero weight;
  * setup times in $[0, 2 \bar s]$ with $\bar s = \eta \bar p$ (25 or 75), including the setup time of the first job (line `-1 j s` of the file);
  * the makespan estimate $C_{\max} = n(\bar p + 0.3 \bar s)$ (6,450 or 7,350), the mean due date $\bar d = (1 - \tau) C_{\max}$, and due dates uniform in $[(1 - R) \bar d, \bar d]$ with probability $\tau$ and in $[\bar d, \bar d + R (C_{\max} - \bar d)]$ otherwise (a fraction $\tau$ of the due dates is below $\bar d$).
* Instances by parameters: 1–40 have $\tau = 0.3$ (loose due dates), 41–80 $\tau = 0.6$, 81–120 $\tau = 0.9$ (tight); within each group of 40, $(R, \eta) = (0.25, 0.25), (0.25, 0.75), (0.75, 0.25), (0.75, 0.75)$ for blocks of 10. Many loose instances have an optimal value of 0 (22 of the 40), and they are the hardest for exact algorithms: Tanaka and Araki (2013) needed the improvements of their Section 5, and up to several weeks for two of them.
* Reference values: the optimal values of Tanaka and Araki (2013).
* Time limit of the CSV file: 60 s (a choice of the repository).
* Results in the repository: [local search](local_search.md), [successive sublimation dynamic programming](successive_sublimation_dynamic_programming.md).

## Vallada and Ruiz (2011): $R \mid s_{ijk} \mid C_{\max}$

* Archive `vallada2011`, format `vallada2011`, `data/data_unrelated_makespan_setup_times.csv`.
* Origin: the instances of the genetic algorithm of Vallada and Ruiz (2011).
* Designed for: makespan on unrelated parallel machines with machine- and sequence-dependent setup times.
* Size:
  * small: $n \in \{6, 8, 10, 12\}$ jobs, $m \in \{2, 3, 4, 5\}$ machines, 640 instances;
  * large: $n \in \{50, 100, 150, 200, 250\}$ jobs, $m \in \{10, 15, 20, 25, 30\}$ machines, 1,000 instances.

  10 instances for each combination of $n$, $m$ and the range of the setup times. Path: `vallada2011/<small|large>/I_<n>_<m>_S_1-<s_max>_<instance>.txt`.
* Generation: integer processing times uniform in $[1, 100]$ (observed in the files), setup times uniform in $[1, s_{\max}]$ with $s_{\max} \in \{9, 49, 99, 124\}$, i.e. setup times of about 10%, 50%, 100% and 125% of the processing times. The files also give a setup time of 0 between a job and itself, which is not used. There is no setup time before the first job of a machine.
* Reference values: the optimal values of the small instances and the best known values of the large instances given by Vallada and Ruiz (2011). The best known values of the large instances date from 2011 and have been improved since.
* Time limit of the CSV file: $n \cdot (m / 2) \cdot 50$ ms, the stopping criterion of Vallada and Ruiz (2011) with $t = 50$.
* No algorithm of the repository supports the makespan with setup times yet (see the [roadmap](roadmap.md)).

## References

* S. Tanaka and M. Araki (2008). A branch and bound algorithm with Lagrangian relaxation to minimize total tardiness on identical parallel machines. *International Journal of Production Economics*, 113(1), 446–458. [doi:10.1016/j.ijpe.2007.10.006](https://doi.org/10.1016/j.ijpe.2007.10.006)
* C. N. Potts and L. N. Van Wassenhove (1985). A branch and bound algorithm for the total weighted tardiness problem. *Operations Research*, 33(2), 363–377. [doi:10.1287/opre.33.2.363](https://doi.org/10.1287/opre.33.2.363)
* V. A. Cicirello (2003). Weighted tardiness scheduling with sequence-dependent setups: a benchmark library. Technical report, Intelligent Coordination and Logistics Laboratory, Robotics Institute, Carnegie Mellon University.
* Y. H. Lee, K. Bhaskaran and M. Pinedo (1997). A heuristic to minimize the total weighted tardiness with sequence-dependent setups. *IIE Transactions*, 29(1), 45–52. [doi:10.1080/07408179708966311](https://doi.org/10.1080/07408179708966311)
* S. Tanaka and M. Araki (2013). An exact algorithm for the single-machine total weighted tardiness problem with sequence-dependent setup times. *Computers & Operations Research*, 40(1), 344–352. [doi:10.1016/j.cor.2012.07.004](https://doi.org/10.1016/j.cor.2012.07.004)
* E. Vallada and R. Ruiz (2011). A genetic algorithm for the unrelated parallel machine scheduling problem with sequence dependent setup times. *European Journal of Operational Research*, 211(3), 612–622. [doi:10.1016/j.ejor.2011.01.011](https://doi.org/10.1016/j.ejor.2011.01.011)
