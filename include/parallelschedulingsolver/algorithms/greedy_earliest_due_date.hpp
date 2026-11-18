#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

/**
 * Greedy algorithm, earliest due date first (EDD, Jackson's rule).
 *
 * Jobs are sorted in increasing order of due date (jobs without due date
 * last) and list scheduled: each job is appended to the machine on which it
 * completes the earliest.
 *
 * Optimal for:
 * - 1 || L_max (Jackson, 1955)
 * - 1 || sum T_j if the EDD schedule has at most one tardy job, or if
 *   processing times and due dates are agreeable (p_i <= p_j => d_i <= d_j)
 */
Output greedy_earliest_due_date(
        const Instance& instance,
        const Parameters& parameters = {});

}
