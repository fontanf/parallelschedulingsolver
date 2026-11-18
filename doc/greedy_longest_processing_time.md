# Greedy, longest processing time first (LPT)

`--algorithm greedy-longest-processing-time`

Source: [`src/algorithms/greedy_longest_processing_time.cpp`](../src/algorithms/greedy_longest_processing_time.cpp)

## Algorithm

1. Sort the jobs in decreasing order of processing time. On unrelated machines, the processing time of a job is its smallest processing time over all machines. Ties keep the order of the job ids.
2. List scheduling: take the jobs in this order and append each one to the machine on which it completes the earliest, given the machine's current completion time and the job's release date. Ties go to the machine with the smallest id.

On identical machines, step 2 is Graham's rule: the job goes to the least loaded machine.

Complexity: $O(n \log n + n m)$.

## Supported problems

Any instance: one or several machines (identical, uniform or unrelated), release dates, due dates and weights. The solution is always feasible, but due dates and weights are ignored, and the algorithm targets the makespan.

## Optimality

| Problem | Result |
| --- | --- |
| $1 \mid\mid C_{\max}$ | Optimal. Any schedule without idle time is optimal; LPT plays no role. |
| $P \mid\mid C_{\max}$ with $n \le m + 1$ | Optimal. |
| $P \mid\mid C_{\max}$ when an optimal schedule has at most two jobs per machine | Optimal (Graham, 1969). |
| $P \mid p_j = p \mid C_{\max}$ | Optimal. |

The condition is **not** $n \le 2m$. For $m = 3$ and $p = (11, 7, 6, 5, 5, 5)$, LPT gives 16 while the optimum is 15 ($\{5, 5, 5\}, \{11\}, \{7, 6\}$), and every optimal schedule has 3 jobs on one machine.

## Guarantees

| Problem | Guarantee |
| --- | --- |
| $P \mid\mid C_{\max}$ | $C_{\max}^{LPT} \le \left(\frac{4}{3} - \frac{1}{3m}\right) C_{\max}^*$, tight (Graham, 1969). Tight instances have $2m + 1$ jobs, e.g. $m = 2$, $p = (3, 3, 2, 2, 2)$: LPT 7, optimum 6. |
| $P \mid\mid C_{\max}$ | When the machine that finishes last holds $k$ jobs: $C_{\max}^{LPT} \le \left(1 + \frac{1}{k} - \frac{1}{km}\right) C_{\max}^*$ (Coffman and Sethi, 1976). |
| $Q \mid\mid C_{\max}$ | $C_{\max}^{LPT} \le \frac{2m}{m + 1} C_{\max}^*$ (Gonzalez, Ibarra and Sahni, 1977). |
| $R \mid\mid C_{\max}$ | No constant guarantee. |
| $P \mid r_j \mid C_{\max}$ | No guarantee specific to LPT. |

$P2 \mid\mid C_{\max}$ is NP-hard and $P \mid\mid C_{\max}$ is strongly NP-hard, so LPT is a heuristic in general.

## Proved optimality

When the instance has no release dates and either has a single machine, or identical machines with $n \le m + 1$ or equal processing times, the algorithm sets the makespan bound to the value of its solution, and the output reports `"Optimal": true`.

## Tests

* `test/algorithms/greedy_longest_processing_time_test.cpp`: exact schedules on small instances (identical machines, ties, unrelated machines with release dates).
* Optimal value on `data/test_makespan_single.txt`.

## References

* R. L. Graham (1969). Bounds on multiprocessing timing anomalies. *SIAM Journal on Applied Mathematics*, 17(2), 416–429.
* E. G. Coffman Jr. and R. Sethi (1976). A generalized bound on LPT sequencing. *RAIRO Informatique*, 10, 17–25.
* T. Gonzalez, O. H. Ibarra and S. Sahni (1977). Bounds for LPT schedules on uniform processors. *SIAM Journal on Computing*, 6(1), 155–166.
