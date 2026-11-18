#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"

#include "parallelschedulingsolver/algorithms/list_scheduling.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

Output parallelschedulingsolver::greedy_shortest_processing_time(
    const Instance& instance,
    const Parameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Greedy, weighted shortest processing-time");
    algorithm_formatter.print_header();

    // Sort jobs in increasing order of processing time / weight.
    std::vector<Time> processing_times(instance.number_of_jobs());
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        processing_times[job_id] = minimum_processing_time(instance, job_id);
    std::vector<JobId> job_ids(instance.number_of_jobs());
    std::iota(job_ids.begin(), job_ids.end(), 0);
    std::stable_sort(job_ids.begin(), job_ids.end(),
            [&instance, &processing_times](JobId job_1_id, JobId job_2_id)
            {
                // p_1 / w_1 < p_2 / w_2, without divisions.
                return processing_times[job_1_id] * instance.job(job_2_id).weight
                    < processing_times[job_2_id] * instance.job(job_1_id).weight;
            });

    Solution solution = list_scheduling(instance, job_ids);
    algorithm_formatter.update_solution(solution, "greedy");

    // Special cases in which the solution is optimal.
    if (no_release_dates(instance)) {
        // 1 || sum w_j C_j and P || sum C_j.
        if (instance.number_of_machines() == 1
                || (identical_machines(instance) && equal_weights(instance))) {
            algorithm_formatter.update_total_flow_time_bound(
                    solution.total_flow_time(),
                    "special case");
        }
        // 1 || sum w_j T_j when all jobs are tardy:
        // sum w_j T_j = sum w_j C_j - sum w_j d_j.
        if (instance.number_of_machines() == 1 && all_jobs_tardy(solution)) {
            algorithm_formatter.update_total_tardiness_bound(
                    solution.total_tardiness(),
                    "special case");
        }
    }
    algorithm_formatter.end();
    return output;
}
