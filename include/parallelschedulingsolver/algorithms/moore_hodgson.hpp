#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

/**
 * Moore-Hodgson algorithm for the single machine problem.
 *
 * Jobs are added in increasing order of due date. Whenever the last added job
 * is late, the job with the largest processing time / weight ratio is removed
 * from the on-time jobs. The on-time jobs are scheduled first in increasing
 * order of due date, then the late jobs.
 *
 * Optimal for:
 * - 1 || sum U_j, i.e. maximizing the number of on-time jobs (Moore, 1968)
 * - 1 || sum w_j U_j if all weights are equal
 *
 * Only single machine instances are supported.
 */
Output moore_hodgson(
        const Instance& instance,
        const Parameters& parameters = {});

}
