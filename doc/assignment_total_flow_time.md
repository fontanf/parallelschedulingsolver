# Assignment, total flow time

`--algorithm assignment-total-flow-time`

Source: [`src/algorithms/assignment_total_flow_time.cpp`](../src/algorithms/assignment_total_flow_time.cpp)

## Algorithm

If job $j$ is the $k$-th job from the end on machine $i$, its processing time counts in the completion times of $k$ jobs: itself and the $k - 1$ jobs after it. So it contributes $k \, p_{ij}$ to the total flow time:

$$\sum_j C_j = \sum_{i} \sum_{k} k \, p_{i, j_{ik}}$$

where $j_{ik}$ is the job in position $k$ from the end on machine $i$.

The algorithm builds the assignment problem between the $n$ jobs and the $n m$ pairs (machine $i$, position from the end $k \in \{1, \dots, n\}$), with cost $k \, p_{ij}$. It solves it with the Hungarian algorithm, then sequences the jobs of each machine by decreasing $k$. Since costs increase with $k$, an optimal assignment uses positions $1, \dots, n_i$ on each machine.

Complexity: $O(n^3 m)$ (Hungarian algorithm on an $n \times nm$ matrix).

## Supported problems

Any instance: one or several machines (identical, uniform or unrelated). Weights and release dates are ignored when building the assignment. Jobs still start no earlier than their release date, so the solution is feasible, but it is not optimal for them.

## Optimality

| Problem | Result |
| --- | --- |
| $R \mid\mid \sum C_j$ | Optimal (Horn, 1973; Bruno, Coffman and Sethi, 1974). |
| $Q \mid\mid \sum C_j$, $P \mid\mid \sum C_j$, $1 \mid\mid \sum C_j$ | Optimal (special cases). On a single machine, the solution is the SPT order. |

## Limits

| Problem | Result |
| --- | --- |
| $R \mid\mid \sum w_j C_j$ | Not optimal, since weights are ignored. NP-hard, even $P2 \mid\mid \sum w_j C_j$ (Bruno, Coffman and Sethi, 1974). |
| $R \mid r_j \mid \sum C_j$ | Not optimal, since release dates are ignored. Already $1 \mid r_j \mid \sum C_j$ is strongly NP-hard (Lenstra, Rinnooy Kan and Brucker, 1977). |

## Proved optimality

When the instance has no release dates and equal weights, the algorithm sets the total flow time bound to the value of its solution, and the output reports `"Optimal": true`. The algorithm runs in polynomial time, so `scripts/solve_test_data.py --algorithm assignment-total-flow-time` gives the optimal values of instances of any size, for example to test other algorithms on the same problem.

## Tests

* `test/algorithms/assignment_total_flow_time_test.cpp`: exact schedules on small instances (single machine, and an unrelated-machine instance where greedy SPT is not optimal).
* Optimal value on `data/test_tft_single.txt` and on the instances of `data/test_tft_parallel.txt` that are small enough to enumerate.

## References

* W. A. Horn (1973). Minimizing average flow time with parallel machines. *Operations Research*, 21(3), 846–847. [doi:10.1287/opre.21.3.846](https://doi.org/10.1287/opre.21.3.846)
* J. Bruno, E. G. Coffman Jr. and R. Sethi (1974). Scheduling independent tasks to reduce mean finishing time. *Communications of the ACM*, 17(7), 382–387. [doi:10.1145/361011.361064](https://doi.org/10.1145/361011.361064)
* H. W. Kuhn (1955). The Hungarian method for the assignment problem. *Naval Research Logistics Quarterly*, 2(1–2), 83–97. [doi:10.1002/nav.3800020109](https://doi.org/10.1002/nav.3800020109)
* J. K. Lenstra, A. H. G. Rinnooy Kan and P. Brucker (1977). Complexity of machine scheduling problems. *Annals of Discrete Mathematics*, 1, 343–362. [doi:10.1016/S0167-5060(08)70743-X](https://doi.org/10.1016/S0167-5060(08)70743-X)
