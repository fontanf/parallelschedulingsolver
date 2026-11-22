#include "parallelschedulingsolver/algorithms/assignment_total_flow_time.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include "hungarian.hpp"

#include <algorithm>

using namespace parallelschedulingsolver;

Output parallelschedulingsolver::assignment_total_flow_time(
    const Instance& instance,
    const Parameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Assignment, total flow time");

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

    JobId n = instance.number_of_jobs();
    MachineId m = instance.number_of_machines();

    // Column machine_id * n + (k - 1) corresponds to the k-th position from the
    // end of machine machine_id.
    std::vector<std::vector<Time>> costs(n, std::vector<Time>(m * n));
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        for (MachineId machine_id = 0; machine_id < m; ++machine_id)
            for (JobId k = 1; k <= n; ++k)
                costs[job_id][machine_id * n + k - 1] = k * job.processing_times[machine_id];
    }
    std::vector<int64_t> job_columns = hungarian(costs, n, m * n);

    // Retrieve the jobs of each machine and sort them by decreasing position
    // from the end.
    std::vector<std::vector<std::pair<JobId, JobId>>> machine_jobs(m);
    for (JobId job_id = 0; job_id < n; ++job_id) {
        MachineId machine_id = job_columns[job_id] / n;
        JobId k = job_columns[job_id] % n + 1;
        machine_jobs[machine_id].push_back({k, job_id});
    }

    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
        std::sort(
                machine_jobs[machine_id].begin(),
                machine_jobs[machine_id].end(),
                std::greater<std::pair<JobId, JobId>>());
        Time current_time = 0;
        for (const auto& p: machine_jobs[machine_id]) {
            const Job& job = instance.job(p.second);
            Time start = (std::max)(current_time, job.release_date);
            solution_builder.append_job_to_machine(machine_id, p.second, start);
            current_time = start + job.processing_times[machine_id];
        }
    }

    Solution solution = solution_builder.build();
    algorithm_formatter.update_solution(solution, "assignment");

    // R || sum C_j: the solution is optimal.
    if (no_release_dates(instance) && equal_weights(instance)) {
        algorithm_formatter.update_total_flow_time_bound(
                solution.total_flow_time(),
                "special case");
    }
    algorithm_formatter.end();
    return output;
}
