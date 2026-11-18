#pragma once

#include "parallelschedulingsolver/solution_builder.hpp"

namespace parallelschedulingsolver
{

/**
 * Build a solution by list scheduling.
 *
 * Jobs are considered in the given order. Each job is appended to the machine
 * on which it would complete the earliest, given the machine's current
 * completion time and the job's release date (ties are broken by machine id).
 */
Solution list_scheduling(
        const Instance& instance,
        const std::vector<JobId>& job_ids);

/**
 * Get the smallest processing time of a job over all machines.
 *
 * Used as the processing time of a job when sorting jobs for list
 * scheduling. On identical machines, it is just the processing time of the
 * job.
 */
Time minimum_processing_time(
        const Instance& instance,
        JobId job_id);

}
