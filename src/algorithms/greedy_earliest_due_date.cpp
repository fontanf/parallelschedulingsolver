#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"

#include "parallelschedulingsolver/algorithms/list_scheduling.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

Output parallelschedulingsolver::greedy_earliest_due_date(
    const Instance& instance,
    const Parameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Greedy, earliest due date");

    if (instance.has_setup_times()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "sequence-dependent setup times are not supported.");
    }

    algorithm_formatter.print_header();

    // Sort jobs in increasing order of due date; jobs without due date last.
    // Ties are broken by shortest processing time first.
    std::vector<JobId> job_ids(instance.number_of_jobs());
    std::iota(job_ids.begin(), job_ids.end(), 0);
    std::stable_sort(job_ids.begin(), job_ids.end(),
            [&instance](JobId job_1_id, JobId job_2_id)
            {
                Time due_date_1 = instance.job(job_1_id).due_date;
                Time due_date_2 = instance.job(job_2_id).due_date;
                if ((due_date_1 < 0) != (due_date_2 < 0))
                    return due_date_2 < 0;
                if (due_date_1 != due_date_2)
                    return due_date_1 < due_date_2;
                return minimum_processing_time(instance, job_1_id)
                    < minimum_processing_time(instance, job_2_id);
            });

    Solution solution = list_scheduling(instance, job_ids);
    algorithm_formatter.update_solution(solution, "greedy");

    // Special cases in which the solution is optimal.
    if (no_release_dates(instance)) {
        // 1 || L_max and P | p_j = p | L_max.
        if (instance.number_of_machines() == 1
                || equal_processing_times(instance)) {
            algorithm_formatter.update_maximum_lateness_bound(
                    solution.maximum_lateness(),
                    "special case");
        }
        if (instance.number_of_machines() == 1 && equal_weights(instance)) {
            JobId number_of_tardy_jobs = parallelschedulingsolver::number_of_tardy_jobs(solution);
            // 1 || sum T_j with at most one tardy job or agreeable due dates.
            if (number_of_tardy_jobs <= 1 || agreeable_due_dates(instance)) {
                algorithm_formatter.update_total_tardiness_bound(
                        solution.total_tardiness(),
                        "special case");
            }
            // 1 || sum U_j with at most one tardy job.
            if (number_of_tardy_jobs <= 1) {
                algorithm_formatter.update_throughput_bound(
                        solution.throughput(),
                        "special case");
            }
        }
    }
    algorithm_formatter.end();
    return output;
}
