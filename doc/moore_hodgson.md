# Moore-Hodgson algorithm

`--algorithm moore-hodgson`

Source: [`src/algorithms/moore_hodgson.cpp`](../src/algorithms/moore_hodgson.cpp)

## Algorithm

Maximizes the number of on-time jobs, i.e. minimizes the number of tardy jobs $\sum U_j$.

1. Jobs without a due date can't be on time. They are put in the late set.
2. Take the other jobs in increasing order of due date and append each one to the sequence of on-time jobs. If the last appended job completes after its due date, remove the job of the sequence with the largest $p_j / w_j$ and move it to the late set. With unit weights, that is the longest job.
3. Schedule the on-time jobs in EDD order, then the late jobs.

The throughput objective of this repository is the total weight of the on-time jobs, so maximizing it is equivalent to minimizing $\sum w_j U_j$.

Complexity: $O(n^2)$ as implemented. The completion time is recomputed after each removal, so that release dates are handled. $O(n \log n)$ is possible with a priority queue when there are no release dates.

## Supported problems

Single machine instances only: the algorithm throws `std::invalid_argument` on more than one machine. Release dates and weights are accepted, but the algorithm is only optimal without them (see below).

## Optimality

| Problem | Result |
| --- | --- |
| $1 \mid\mid \sum U_j$ | Optimal (Moore, 1968). |
| $1 \mid\mid \sum w_j U_j$ with equal weights | Optimal (same problem as above). |

## Limits

| Problem | Result |
| --- | --- |
| $1 \mid\mid \sum w_j U_j$ | Not optimal: the $p_j / w_j$ removal rule is a heuristic. The problem is NP-hard (Karp, 1972) and can be solved in $O(n \sum p_j)$ time by dynamic programming (Lawler and Moore, 1969). |
| $1 \mid r_j \mid \sum U_j$ | Not optimal; strongly NP-hard (Lenstra, Rinnooy Kan and Brucker, 1977). |

## Proved optimality

When the instance has no release dates and equal weights, the algorithm sets the throughput bound to the value of its solution, and the output reports `"Optimal": true`.

## Tests

* `test/algorithms/moore_hodgson_test.cpp`: exact schedules on small instances (unweighted with removals and a job without due date, weighted removal rule, all jobs on time), and an exception on parallel machines.
* Optimal number of on-time jobs on `data/test_throughput_single.txt` and `data/test_tt_single.txt`. The throughput instances are weighted, so the test counts the on-time jobs instead of using the objective value.

## References

* J. M. Moore (1968). An n job, one machine sequencing algorithm for minimizing the number of late jobs. *Management Science*, 15(1), 102–109.
* E. L. Lawler and J. M. Moore (1969). A functional equation and its application to resource allocation and sequencing problems. *Management Science*, 16(1), 77–84.
* R. M. Karp (1972). Reducibility among combinatorial problems. In *Complexity of Computer Computations*, 85–103.
* J. K. Lenstra, A. H. G. Rinnooy Kan and P. Brucker (1977). Complexity of machine scheduling problems. *Annals of Discrete Mathematics*, 1, 343–362.
