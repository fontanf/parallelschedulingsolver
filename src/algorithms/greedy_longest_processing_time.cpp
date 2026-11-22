#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"

#include "parallelschedulingsolver/algorithms/list_scheduling.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

Output parallelschedulingsolver::greedy_longest_processing_time(
    const Instance& instance,
    const Parameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Greedy, longest processing-time");

    if (instance.has_rejection_costs()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "rejection costs are not supported.");
    }

    if (instance.has_setup_times()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "sequence-dependent setup times are not supported.");
    }

    algorithm_formatter.print_header();

    // Sort jobs in decreasing order of processing time.
    std::vector<Time> processing_times(instance.number_of_jobs());
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        processing_times[job_id] = minimum_processing_time(instance, job_id);
    std::vector<JobId> job_ids(instance.number_of_jobs());
    std::iota(job_ids.begin(), job_ids.end(), 0);
    std::stable_sort(job_ids.begin(), job_ids.end(),
            [&processing_times](JobId job_1_id, JobId job_2_id)
            {
                return processing_times[job_1_id] > processing_times[job_2_id];
            });

    Solution solution = list_scheduling(instance, job_ids);
    algorithm_formatter.update_solution(solution, "greedy");

    // Special cases in which the solution is optimal for the makespan.
    if (no_release_dates(instance)
            && (instance.number_of_machines() == 1
                || (identical_machines(instance)
                    && (instance.number_of_jobs() <= instance.number_of_machines() + 1
                        || equal_processing_times(instance))))) {
        algorithm_formatter.update_makespan_bound(solution.makespan(), "special case");
    }
    algorithm_formatter.end();
    return output;
}
