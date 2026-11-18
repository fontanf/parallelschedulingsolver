# Greedy, earliest due date first (EDD)

`--algorithm greedy-earliest-due-date`

Source: [`src/algorithms/greedy_earliest_due_date.cpp`](../src/algorithms/greedy_earliest_due_date.cpp)

## Algorithm

1. Sort the jobs in increasing order of due date (Jackson's rule). Jobs without a due date go last. Ties are broken by shortest processing time (smallest over all machines), then by job id.
2. List scheduling: take the jobs in this order and append each one to the machine on which it completes the earliest, given the machine's current completion time and the job's release date. Ties go to the machine with the smallest id.

Breaking ties by SPT matters for the agreeable case below: among jobs with the same due date, putting the shortest first never increases the total tardiness.

Complexity: $O(n \log n + n m)$.

## Supported problems

Any instance: one or several machines (identical, uniform or unrelated), release dates, due dates and weights. The solution is always feasible. The algorithm targets due-date objectives and ignores weights.

## Optimality

| Problem | Result |
| --- | --- |
| $1 \mid\mid L_{\max}$ (and $1 \mid\mid T_{\max}$) | Optimal (Jackson, 1955). |
| $P \mid p_j = p \mid L_{\max}$ | Optimal. |
| $1 \mid\mid \sum U_j$ when the EDD schedule has at most one tardy job | Optimal. EDD finds a schedule without tardy jobs whenever one exists. |
| $1 \mid\mid \sum T_j$ when the EDD schedule has at most one tardy job | Optimal. If job $j$ is the only tardy job, $\sum T_j = L_{\max}^{EDD}$, and $L_{\max}^{EDD}$ is a lower bound on the total tardiness of any schedule. |
| $1 \mid\mid \sum T_j$ with agreeable processing times and due dates ($p_i < p_j \Rightarrow d_i \le d_j$) | Optimal (Emmons, 1969). |

For general instances, [`moore-hodgson`](moore_hodgson.md) is the optimal algorithm for the number of on-time jobs.

## Limits

| Problem | Result |
| --- | --- |
| $1 \mid r_j \mid L_{\max}$ | Not optimal; strongly NP-hard (Lenstra, Rinnooy Kan and Brucker, 1977). |
| $1 \mid\mid \sum T_j$ | Not optimal; NP-hard (Du and Leung, 1990). |
| $1 \mid\mid \sum w_j T_j$ | Not optimal, even with at most one tardy job, since weights are ignored; strongly NP-hard. |
| $P \mid\mid L_{\max}$ | Not optimal; NP-hard since it contains $P \mid\mid C_{\max}$. |

## Proved optimality

When the instance has no release dates, the algorithm sets the bound to the value of its solution, and the output reports `"Optimal": true` for the matching objective:
* maximum lateness: on a single machine, or when all processing times are equal;
* total tardiness: on a single machine with equal weights, when at most one job is tardy or due dates are agreeable;
* throughput: on a single machine with equal weights, when at most one job is tardy.

Note: in the generated test instances, the due date of a job is proportional to its processing time on a single machine ($d_j = 3 p_j$), so the single machine total tardiness instances are all agreeable and EDD solves them all optimally.

## Tests

* `test/algorithms/greedy_earliest_due_date_test.cpp`: exact schedules on small instances (ties on due dates, job without due date, identical machines).
* Optimal maximum lateness on `data/test_tt_single.txt` and `data/test_twt_single.txt`. These instances have a total tardiness objective; the test compares the maximum lateness of the solution.

## References

* J. R. Jackson (1955). Scheduling a production line to minimize maximum tardiness. Research Report 43, Management Science Research Project, UCLA.
* H. Emmons (1969). One-machine sequencing to minimize certain functions of job tardiness. *Operations Research*, 17(4), 701–715. [doi:10.1287/opre.17.4.701](https://doi.org/10.1287/opre.17.4.701)
* J. Du and J. Y.-T. Leung (1990). Minimizing total tardiness on one machine is NP-hard. *Mathematics of Operations Research*, 15(3), 483–495. [doi:10.1287/moor.15.3.483](https://doi.org/10.1287/moor.15.3.483)
* J. K. Lenstra, A. H. G. Rinnooy Kan and P. Brucker (1977). Complexity of machine scheduling problems. *Annals of Discrete Mathematics*, 1, 343–362. [doi:10.1016/S0167-5060(08)70743-X](https://doi.org/10.1016/S0167-5060(08)70743-X)
