#include "parallelschedulingsolver/algorithms/moore_hodgson.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include "optimizationtools/containers/indexed_binary_heap.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

namespace
{

/**
 * Key of an on-time job in the heap of the jobs to remove.
 *
 * The heap is a min-heap, so a key is smaller when the job should be removed
 * first: larger processing time / weight ratio first, then the job added
 * first, i.e. the one with the smallest due date.
 */
struct RemovalKey
{
    Time processing_time = 0;

    Time weight = 1;

    /** Position of the job in the EDD order. */
    JobId rank = 0;

    bool operator<(const RemovalKey& key) const
    {
        // p / w > p' / w', without divisions.
        Time lhs = processing_time * key.weight;
        Time rhs = key.processing_time * weight;
        if (lhs != rhs)
            return lhs > rhs;
        return rank < key.rank;
    }

    bool operator>(const RemovalKey& key) const { return key < *this; }

    bool operator<=(const RemovalKey& key) const { return !(key < *this); }

    bool operator>=(const RemovalKey& key) const { return !(*this < key); }
};

}

Output parallelschedulingsolver::moore_hodgson(
    const Instance& instance,
    const Parameters& parameters)
{
    if (instance.number_of_machines() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the Moore-Hodgson algorithm only supports single machine instances; "
                "instance.number_of_machines(): " + std::to_string(instance.number_of_machines()) + ".");
    }

    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Moore-Hodgson");
    algorithm_formatter.print_header();

    MachineId machine_id = 0;

    // Sort jobs with a due date in increasing order of due date. Jobs without
    // due date can't be on time.
    std::vector<JobId> job_ids;
    std::vector<JobId> late_job_ids;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        if (instance.job(job_id).due_date >= 0) {
            job_ids.push_back(job_id);
        } else {
            late_job_ids.push_back(job_id);
        }
    }
    std::stable_sort(job_ids.begin(), job_ids.end(),
            [&instance](JobId job_1_id, JobId job_2_id)
            {
                return instance.job(job_1_id).due_date
                    < instance.job(job_2_id).due_date;
            });

    // Heap of the on-time jobs; the top is the next job to remove.
    optimizationtools::IndexedBinaryHeap<RemovalKey> on_time_jobs(instance.number_of_jobs());
    bool has_release_dates = !no_release_dates(instance);
    Time current_time = 0;
    for (JobId job_pos = 0; job_pos < (JobId)job_ids.size(); ++job_pos) {
        JobId job_id = job_ids[job_pos];
        const Job& job = instance.job(job_id);
        on_time_jobs.update_key(job_id, {
                job.processing_times[machine_id],
                job.weight,
                job_pos});
        current_time = (std::max)(current_time, job.release_date)
            + job.processing_times[machine_id];
        if (current_time <= job.due_date)
            continue;

        // Remove the on-time job with the largest processing time / weight
        // ratio.
        JobId removed_job_id = on_time_jobs.top().first;
        on_time_jobs.pop();
        late_job_ids.push_back(removed_job_id);
        if (!has_release_dates) {
            current_time -= instance.job(removed_job_id).processing_times[machine_id];
        } else {
            // With release dates, removing a job may not decrease the
            // completion time by its processing time: recompute it.
            current_time = 0;
            for (JobId job_pos_2 = 0; job_pos_2 <= job_pos; ++job_pos_2) {
                JobId job_id_2 = job_ids[job_pos_2];
                if (!on_time_jobs.contains(job_id_2))
                    continue;
                const Job& job_2 = instance.job(job_id_2);
                current_time = (std::max)(current_time, job_2.release_date)
                    + job_2.processing_times[machine_id];
            }
        }
    }

    // On-time jobs, in increasing order of due date.
    std::vector<JobId> on_time_job_ids;
    for (JobId job_id: job_ids)
        if (on_time_jobs.contains(job_id))
            on_time_job_ids.push_back(job_id);

    // Build solution.
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    current_time = 0;
    for (const std::vector<JobId>* sequence: {&on_time_job_ids, &late_job_ids}) {
        for (JobId job_id: *sequence) {
            const Job& job = instance.job(job_id);
            Time start = (std::max)(current_time, job.release_date);
            solution_builder.append_job_to_machine(machine_id, job_id, start);
            current_time = start + job.processing_times[machine_id];
        }
    }

    Solution solution = solution_builder.build();
    algorithm_formatter.update_solution(solution, "moore-hodgson");

    // 1 || sum U_j: the solution is optimal.
    if (no_release_dates(instance) && equal_weights(instance)) {
        algorithm_formatter.update_throughput_bound(
                solution.throughput(),
                "special case");
    }
    algorithm_formatter.end();
    return output;
}
