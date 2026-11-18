#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

/**
 * Greedy algorithm, weighted shortest processing time first (WSPT, Smith's
 * rule).
 *
 * Jobs are sorted in increasing order of processing time (smallest processing
 * time over all machines) divided by weight and list scheduled: each job is
 * appended to the machine on which it completes the earliest. With unit
 * weights, this is the shortest processing time first rule (SPT).
 *
 * Optimal for:
 * - 1 || sum w_j C_j (Smith, 1956)
 * - P || sum C_j
 *
 * Approximation ratio (1 + sqrt(2)) / 2 for P || sum w_j C_j (Kawaguchi and
 * Kyan, 1986).
 */
Output greedy_shortest_processing_time(
        const Instance& instance,
        const Parameters& parameters = {});

}
