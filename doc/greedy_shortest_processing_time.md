# Greedy, weighted shortest processing time first (WSPT)

`--algorithm greedy-shortest-processing-time`

Source: [`src/algorithms/greedy_shortest_processing_time.cpp`](../src/algorithms/greedy_shortest_processing_time.cpp)

## Algorithm

1. Sort the jobs in increasing order of $p_j / w_j$ (Smith's rule). On unrelated machines, $p_j$ is the job's smallest processing time over all machines. Ties keep the order of the job ids. With unit weights, this is the shortest processing time first rule (SPT).
2. List scheduling: take the jobs in this order and append each one to the machine on which it completes the earliest, given the machine's current completion time and the job's release date. Ties go to the machine with the smallest id.

Complexity: $O(n \log n + n m)$.

## Supported problems

Any instance: one or several machines (identical, uniform or unrelated), release dates, due dates and weights. The solution is always feasible. The algorithm targets the total (weighted) flow time and ignores due dates.

## Optimality

| Problem | Result |
| --- | --- |
| $1 \mid\mid \sum C_j$ | Optimal (SPT). |
| $1 \mid\mid \sum w_j C_j$ | Optimal (Smith, 1956). |
| $P \mid\mid \sum C_j$ | Optimal (Conway, Maxwell and Miller, 1967). On identical machines, list scheduling in SPT order assigns the jobs to the machines in a round-robin way. |
| $1 \mid\mid \sum w_j T_j$ when all jobs are tardy in the WSPT schedule | Optimal: then $\sum w_j T_j = \sum w_j C_j - \sum w_j d_j$, and WSPT minimizes $\sum w_j C_j$. The same holds for $1 \mid\mid \sum T_j$ with SPT. |

## Guarantees and limits

| Problem | Result |
| --- | --- |
| $P \mid\mid \sum w_j C_j$ | $\le \frac{1 + \sqrt{2}}{2} \approx 1.207$ times the optimum (Kawaguchi and Kyan, 1986). The problem is NP-hard, even for $m = 2$ (Bruno, Coffman and Sethi, 1974). |
| $Q \mid\mid \sum C_j$, $R \mid\mid \sum C_j$ | Not optimal. Use [`assignment-total-flow-time`](assignment_total_flow_time.md), which is optimal. |
| $1 \mid r_j \mid \sum C_j$ | Not optimal. The problem is strongly NP-hard (Lenstra, Rinnooy Kan and Brucker, 1977). |

## Proved optimality

When the instance has no release dates, the algorithm sets the bound to the value of its solution, and the output reports `"Optimal": true` for the matching objective:
* total flow time: on a single machine, or on identical machines with equal weights;
* total tardiness: on a single machine when all jobs are tardy.

## Tests

* `test/algorithms/greedy_shortest_processing_time_test.cpp`: exact schedules on small instances (single machine with weights and ties, identical machines, release dates).
* Optimal value on `data/test_tft_single.txt` and `data/test_twft_single.txt`.

## References

* W. E. Smith (1956). Various optimizers for single-stage production. *Naval Research Logistics Quarterly*, 3(1–2), 59–66.
* R. W. Conway, W. L. Maxwell and L. W. Miller (1967). *Theory of Scheduling*. Addison-Wesley.
* T. Kawaguchi and S. Kyan (1986). Worst case bound of an LRF schedule for the mean weighted flow-time problem. *SIAM Journal on Computing*, 15(4), 1119–1129.
* J. Bruno, E. G. Coffman Jr. and R. Sethi (1974). Scheduling independent tasks to reduce mean finishing time. *Communications of the ACM*, 17(7), 382–387.
* J. K. Lenstra, A. H. G. Rinnooy Kan and P. Brucker (1977). Complexity of machine scheduling problems. *Annals of Discrete Mathematics*, 1, 343–362.
