#include "parallelschedulingsolver/algorithms/assignment_total_flow_time.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>

using namespace parallelschedulingsolver;

namespace
{

/**
 * Solve a rectangular assignment problem with the Hungarian algorithm.
 *
 * 'costs' has 'number_of_rows' rows and 'number_of_columns' columns, with
 * number_of_rows <= number_of_columns. Each row is assigned to a distinct
 * column. Return the column assigned to each row.
 *
 * Complexity: O(number_of_rows^2 number_of_columns).
 */
std::vector<int64_t> hungarian(
        const std::vector<std::vector<Time>>& costs,
        int64_t number_of_rows,
        int64_t number_of_columns)
{
    const Time inf = std::numeric_limits<Time>::max() / 4;
    // 1-indexed; index 0 is a dummy column.
    std::vector<Time> u(number_of_rows + 1, 0);
    std::vector<Time> v(number_of_columns + 1, 0);
    std::vector<int64_t> column_row(number_of_columns + 1, 0);
    std::vector<int64_t> way(number_of_columns + 1, 0);
    for (int64_t row = 1; row <= number_of_rows; ++row) {
        column_row[0] = row;
        int64_t column_0 = 0;
        std::vector<Time> minv(number_of_columns + 1, inf);
        std::vector<bool> used(number_of_columns + 1, false);
        do {
            used[column_0] = true;
            int64_t row_0 = column_row[column_0];
            Time delta = inf;
            int64_t column_1 = 0;
            for (int64_t column = 1; column <= number_of_columns; ++column) {
                if (used[column])
                    continue;
                Time cur = costs[row_0 - 1][column - 1] - u[row_0] - v[column];
                if (cur < minv[column]) {
                    minv[column] = cur;
                    way[column] = column_0;
                }
                if (minv[column] < delta) {
                    delta = minv[column];
                    column_1 = column;
                }
            }
            for (int64_t column = 0; column <= number_of_columns; ++column) {
                if (used[column]) {
                    u[column_row[column]] += delta;
                    v[column] -= delta;
                } else {
                    minv[column] -= delta;
                }
            }
            column_0 = column_1;
        } while (column_row[column_0] != 0);
        do {
            int64_t column_1 = way[column_0];
            column_row[column_0] = column_row[column_1];
            column_0 = column_1;
        } while (column_0 != 0);
    }

    std::vector<int64_t> row_column(number_of_rows, -1);
    for (int64_t column = 1; column <= number_of_columns; ++column)
        if (column_row[column] != 0)
            row_column[column_row[column] - 1] = column - 1;
    return row_column;
}

}

Output parallelschedulingsolver::assignment_total_flow_time(
    const Instance& instance,
    const Parameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Assignment, total flow time");
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
