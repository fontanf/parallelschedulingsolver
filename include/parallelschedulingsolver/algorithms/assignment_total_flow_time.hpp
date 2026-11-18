#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

/**
 * Assignment-based algorithm for the total flow time.
 *
 * If job j is scheduled on machine i in the k-th position from the end, it
 * contributes k p_ij to the total flow time. The algorithm solves the
 * assignment problem between jobs and (machine, position from the end) pairs
 * with the Hungarian algorithm (Horn, 1973; Bruno, Coffman and Sethi, 1974).
 *
 * Weights and release dates are ignored when building the assignment.
 *
 * Optimal for:
 * - R || sum C_j (and therefore Q || sum C_j, P || sum C_j and 1 || sum C_j)
 */
Output assignment_total_flow_time(
        const Instance& instance,
        const Parameters& parameters = {});

}
