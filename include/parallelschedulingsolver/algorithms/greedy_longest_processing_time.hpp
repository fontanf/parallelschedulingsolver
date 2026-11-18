#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

/**
 * Greedy algorithm, longest processing time first (LPT).
 *
 * Jobs are sorted in decreasing order of processing time (smallest processing
 * time over all machines) and list scheduled: each job is appended to the
 * machine on which it completes the earliest.
 *
 * Optimal for:
 * - 1 || C_max (any schedule without idle time is optimal)
 * - P || C_max if n <= m + 1, if an optimal schedule has at most two jobs
 *   per machine (Graham, 1969), or if all processing times are equal
 *
 * Approximation ratio 4/3 - 1/(3m) for P || C_max (Graham, 1969).
 */
Output greedy_longest_processing_time(
        const Instance& instance,
        const Parameters& parameters = {});

}
