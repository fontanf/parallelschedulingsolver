#pragma once

#include "parallelschedulingsolver/solution.hpp"

#include <algorithm>
#include <numeric>

/**
 * Instance properties used by the algorithms to detect the special cases in
 * which their solution is optimal. In these cases, they set the bound of the
 * corresponding criterion to the value of their solution.
 */

namespace parallelschedulingsolver
{

/** Return true if all release dates are 0. */
inline bool no_release_dates(const Instance& instance)
{
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).release_date != 0)
            return false;
    return true;
}

/** Return true if all jobs have the same weight. */
inline bool equal_weights(const Instance& instance)
{
    for (JobId job_id = 1; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).weight != instance.job(0).weight)
            return false;
    return true;
}

/** Return true if each job has the same processing time on all machines. */
inline bool identical_machines(const Instance& instance)
{
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        for (MachineId machine_id = 1;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            if (job.processing_times[machine_id] != job.processing_times[0])
                return false;
        }
    }
    return true;
}

/** Return true if all processing times of all jobs on all machines are equal. */
inline bool equal_processing_times(const Instance& instance)
{
    if (!identical_machines(instance))
        return false;
    for (JobId job_id = 1; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).processing_times[0] != instance.job(0).processing_times[0])
            return false;
    return true;
}

/**
 * Return true if, on a single machine, the processing times and due dates of
 * the jobs with a due date are agreeable: p_i < p_j => d_i <= d_j.
 */
inline bool agreeable_due_dates(const Instance& instance)
{
    std::vector<JobId> job_ids;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).due_date >= 0)
            job_ids.push_back(job_id);
    std::sort(job_ids.begin(), job_ids.end(),
            [&instance](JobId job_1_id, JobId job_2_id)
            {
                return instance.job(job_1_id).processing_times[0]
                    < instance.job(job_2_id).processing_times[0];
            });
    // Go through the groups of jobs with the same processing time, in
    // increasing order of processing time. The smallest due date of a group
    // must be at least the largest due date of the previous groups.
    Time due_date_max_previous_groups = std::numeric_limits<Time>::min();
    for (JobId group_start = 0; group_start < (JobId)job_ids.size();) {
        Time p = instance.job(job_ids[group_start]).processing_times[0];
        JobId group_end = group_start;
        Time due_date_max_group = std::numeric_limits<Time>::min();
        while (group_end < (JobId)job_ids.size()
                && instance.job(job_ids[group_end]).processing_times[0] == p) {
            Time due_date = instance.job(job_ids[group_end]).due_date;
            if (due_date < due_date_max_previous_groups)
                return false;
            due_date_max_group = (std::max)(due_date_max_group, due_date);
            group_end++;
        }
        due_date_max_previous_groups = (std::max)(
                due_date_max_previous_groups,
                due_date_max_group);
        group_start = group_end;
    }
    return true;
}

/** Return the number of jobs with a due date that complete after it. */
inline JobId number_of_tardy_jobs(const Solution& solution)
{
    const Instance& instance = solution.instance();
    JobId number_of_tardy_jobs = 0;
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        for (const auto& scheduled_job: solution.machine(machine_id).jobs) {
            Time due_date = instance.job(scheduled_job.job_id).due_date;
            if (due_date >= 0 && scheduled_job.completion > due_date)
                number_of_tardy_jobs++;
        }
    }
    return number_of_tardy_jobs;
}

/** Return true if all jobs have a due date and complete after it. */
inline bool all_jobs_tardy(const Solution& solution)
{
    const Instance& instance = solution.instance();
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).due_date < 0)
            return false;
    return number_of_tardy_jobs(solution) == instance.number_of_jobs();
}

}
