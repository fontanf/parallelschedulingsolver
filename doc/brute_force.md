# Brute force

`--algorithm brute-force`

Source: [`src/algorithms/brute_force.cpp`](../src/algorithms/brute_force.cpp)

## Algorithm

Enumerates the solutions and keeps the best one. It is only practical for very small instances, but it is simple enough for its correctness to be easy to trust, so it gives the reference optimal values of the tests of the other algorithms (see [`tests.md`](tests.md)).

All the objectives are regular, so once the sequence of the jobs of each machine is fixed, starting each job as early as possible is optimal: a job starts at $\max(C_{prev} + s_{prev, j}, r_j)$, where $C_{prev}$ is the completion time of the previous job of the machine and $s_{prev, j}$ the setup time.

In general, the algorithm enumerates all the sequences of all the machines: all the arrangements of the $n$ jobs and of $m - 1$ separators, the jobs between two separators forming the sequence of a machine. There are $(n + m - 1)! / (m - 1)!$ arrangements ($n!$ on a single machine). They are enumerated with `std::next_permutation`, which enumerates the distinct permutations of a multiset.

On parallel machines without setup times, when a sequencing rule is optimal for a fixed assignment of the jobs to the machines, the algorithm enumerates the $m^n$ assignments instead and sequences the jobs of each machine with the rule:

| Objective | Rule |
| --- | --- |
| $C_{\max}$ | Earliest release date first |
| $\sum w_j C_j$ without release dates | Weighted shortest processing time first (Smith, 1956) |
| $L_{\max}$ without release dates | Earliest due date first, jobs without due date last (Jackson, 1955) |

With rejection costs, these candidates are enumerated for each subset of the jobs with a rejection cost: the jobs of the subset are rejected, and their rejection costs are added to the value. With $r$ such jobs, there are $\sum_k \binom{r}{k} N(n - k)$ candidates, where $N(n)$ is the number of candidates with $n$ jobs.

The value of each candidate is computed directly. Each time the best value improves, the solution is built with the `SolutionBuilder`, which recomputes and checks it independently: the algorithm throws `std::logic_error` if the two values differ.

## Parameters

* `--maximum-number-of-candidates`: if the instance has more candidates than this, the algorithm returns without solution. No limit by default.
* `--time-limit`: if reached, the algorithm returns the best solution found, without proof of optimality.

## Supported problems

All the problems of the repository: single, identical, uniform and unrelated machines, release dates, sequence-dependent setup times, rejection costs, all the objectives.

Practical sizes, for $10^6$ candidates (less than 1 s):

| Machines | Jobs (full enumeration) | Jobs (assignments with a rule) |
| --- | --- | --- |
| 1 | 9 | |
| 2 | 8 | 19 |
| 3 | 7 | 12 |
| 4 | 7 | 9 |

## Proved optimality

When the enumeration completes, the algorithm sets the bound of the objective to the value of its solution, and the output reports `"Optimal": true`.

## Tests

* `test/algorithms/brute_force_test.cpp`: setup times on one and two machines, a setup performed before the release date, the empty instance, and the limit on the number of candidates.
* Rejection costs: three small instances (one machine, two machines with a sequencing rule, all the jobs rejected), and five test instances with rejection costs for the jobs of even id, whose optimal value is checked against the best, over the subsets of rejected jobs, of the optimal value of the instance without these jobs plus their rejection costs.
* Optimal values proved by the time-indexed MILP on all the test instances without setup times small enough for the brute force (`test/algorithms/brute_force_test.txt`, all the objectives).
* Optimal maximum lateness of `test/algorithms/milp_assignment_maximum_lateness_test.txt`, proved by the assignment MILP in its tests.
* Checked once, not in the tests: the optimal makespans of the 160 instances of Vallada and Ruiz (2011) with 6 jobs ($R \mid s_{ijk} \mid C_{\max}$), and the values of the Python brute force it replaces on all the reference files.

## References

* W. E. Smith (1956). Various optimizers for single-stage production. *Naval Research Logistics Quarterly*, 3(1–2), 59–66. [doi:10.1002/nav.3800030106](https://doi.org/10.1002/nav.3800030106)
* J. R. Jackson (1955). Scheduling a production line to minimize maximum tardiness. Research Report 43, Management Science Research Project, UCLA.
* E. Vallada and R. Ruiz (2011). A genetic algorithm for the unrelated parallel machine scheduling problem with sequence dependent setup times. *European Journal of Operational Research*, 211(3), 612–622. [doi:10.1016/j.ejor.2011.01.011](https://doi.org/10.1016/j.ejor.2011.01.011)
