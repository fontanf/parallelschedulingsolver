#include "parallelschedulingsolver/algorithms/moore_hodgson.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

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

    // Completion time of the sequence of on-time jobs.
    auto completion_time = [&instance, machine_id](
            const std::vector<JobId>& sequence)
    {
        Time current_time = 0;
        for (JobId job_id: sequence) {
            const Job& job = instance.job(job_id);
            current_time = (std::max)(current_time, job.release_date)
                + job.processing_times[machine_id];
        }
        return current_time;
    };

    std::vector<JobId> on_time_job_ids;
    Time current_time = 0;
    for (JobId job_id: job_ids) {
        const Job& job = instance.job(job_id);
        on_time_job_ids.push_back(job_id);
        current_time = (std::max)(current_time, job.release_date)
            + job.processing_times[machine_id];
        if (current_time <= job.due_date)
            continue;

        // Remove the on-time job with the largest processing time / weight
        // ratio.
        auto it_max = std::max_element(
                on_time_job_ids.begin(),
                on_time_job_ids.end(),
                [&instance, machine_id](JobId job_1_id, JobId job_2_id)
                {
                    // p_1 / w_1 < p_2 / w_2, without divisions.
                    const Job& job_1 = instance.job(job_1_id);
                    const Job& job_2 = instance.job(job_2_id);
                    return job_1.processing_times[machine_id] * job_2.weight
                        < job_2.processing_times[machine_id] * job_1.weight;
                });
        late_job_ids.push_back(*it_max);
        on_time_job_ids.erase(it_max);
        current_time = completion_time(on_time_job_ids);
    }

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
