#include "parallelschedulingsolver/algorithms/milp_positional.hpp"

#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"

#include "milp.hpp"

#include <algorithm>

using namespace parallelschedulingsolver;

namespace
{

const double inf = std::numeric_limits<double>::infinity();

struct Model
{
    mathoptsolverscmake::MathOptModel model;

    /**
     * x[i][j][k - 1]: id of the variable x_{i, j, k} (positions k = 1..n,
     * from the end of the machine).
     */
    std::vector<std::vector<std::vector<int>>> x;
};

Model create_milp_model(
        const Instance& instance,
        double upper_bound)
{
    const JobId n = instance.number_of_jobs();
    const MachineId m = instance.number_of_machines();
    const Objective objective = instance.objective();
    const bool needs_job_completion_times = (objective != Objective::Makespan);

    Model model;
    model.model.objective_direction = (objective == Objective::Throughput)?
        mathoptsolverscmake::ObjectiveDirection::Maximize:
        mathoptsolverscmake::ObjectiveDirection::Minimize;

    // Horizon of each machine: no job completes after it in a semi-active
    // schedule.
    Time maximum_release_date = 0;
    for (JobId job_id = 0; job_id < n; ++job_id)
        maximum_release_date = (std::max)(maximum_release_date, instance.job(job_id).release_date);
    std::vector<Time> horizons(m, maximum_release_date);
    for (MachineId machine_id = 0; machine_id < m; ++machine_id)
        for (JobId job_id = 0; job_id < n; ++job_id)
            horizons[machine_id] += instance.job(job_id).processing_times[machine_id];
    Time maximum_horizon = *std::max_element(horizons.begin(), horizons.end());

    // Earliest completion time of each job.
    std::vector<Time> earliest_completion_times(n);
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        earliest_completion_times[job_id] = job.release_date + *std::min_element(
                job.processing_times.begin(),
                job.processing_times.end());
    }

    // Variables x_{i, j, k}.
    model.x = std::vector<std::vector<std::vector<int>>>(
            m, std::vector<std::vector<int>>(n, std::vector<int>(n)));
    for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            for (JobId k = 1; k <= n; ++k) {
                model.x[machine_id][job_id][k - 1] = add_variable(
                        model.model,
                        0,
                        1,
                        mathoptsolverscmake::VariableType::Binary,
                        0);
            }
        }
    }

    // Variables C_{i, k}: completion time of position k of machine i (the
    // completion time of the previous position if position k is empty).
    std::vector<std::vector<int>> position_completion_times(m, std::vector<int>(n));
    for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
        for (JobId k = 1; k <= n; ++k) {
            position_completion_times[machine_id][k - 1] = add_variable(
                    model.model,
                    0,
                    horizons[machine_id],
                    mathoptsolverscmake::VariableType::Continuous,
                    0);
        }
    }

    // Variables C_j: completion time of job j.
    std::vector<int> job_completion_times(n, -1);
    if (needs_job_completion_times) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            job_completion_times[job_id] = add_variable(
                    model.model,
                    earliest_completion_times[job_id],
                    maximum_horizon,
                    mathoptsolverscmake::VariableType::Continuous,
                    0);
        }
    }

    // Constraints: each job is at one position.
    for (JobId job_id = 0; job_id < n; ++job_id) {
        std::vector<std::pair<int, double>> elements;
        for (MachineId machine_id = 0; machine_id < m; ++machine_id)
            for (JobId k = 1; k <= n; ++k)
                elements.push_back({model.x[machine_id][job_id][k - 1], 1.0});
        add_constraint(model.model, elements, 1, 1);
    }

    // Constraints: each position has at most one job, and the positions of
    // a machine are filled from the end: if position k + 1 has a job, then
    // position k has one too.
    for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
        for (JobId k = 1; k <= n; ++k) {
            std::vector<std::pair<int, double>> elements;
            for (JobId job_id = 0; job_id < n; ++job_id)
                elements.push_back({model.x[machine_id][job_id][k - 1], 1.0});
            add_constraint(model.model, elements, 0, 1);
            if (k < n) {
                // sum_j x_{i, j, k} - sum_j x_{i, j, k + 1} >= 0
                for (JobId job_id = 0; job_id < n; ++job_id)
                    elements.push_back({model.x[machine_id][job_id][k], -1.0});
                add_constraint(model.model, elements, 0, inf);
            }
        }
    }

    // Constraints: completion times of the positions.
    for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
        for (JobId k = 1; k <= n; ++k) {
            int c = position_completion_times[machine_id][k - 1];
            // Processed after the previous position (k + 1):
            // C_{i, k} - C_{i, k + 1} - sum_j p_{i, j} x_{i, j, k} >= 0
            std::vector<std::pair<int, double>> elements = {{c, 1.0}};
            if (k < n)
                elements.push_back({position_completion_times[machine_id][k], -1.0});
            for (JobId job_id = 0; job_id < n; ++job_id) {
                Time p = instance.job(job_id).processing_times[machine_id];
                if (p != 0)
                    elements.push_back({model.x[machine_id][job_id][k - 1], -(double)p});
            }
            add_constraint(model.model, elements, 0, inf);
            // Processed after its release date:
            // C_{i, k} - sum_j (r_j + p_{i, j}) x_{i, j, k} >= 0
            elements = {{c, 1.0}};
            bool release_dates = false;
            for (JobId job_id = 0; job_id < n; ++job_id) {
                const Job& job = instance.job(job_id);
                if (job.release_date == 0)
                    continue;
                release_dates = true;
                elements.push_back({
                        model.x[machine_id][job_id][k - 1],
                        -(double)(job.release_date + job.processing_times[machine_id])});
            }
            if (release_dates)
                add_constraint(model.model, elements, 0, inf);
        }
    }

    if (needs_job_completion_times) {
        // Constraints: completion times of the jobs.
        // C_j - C_{i, k} - M x_{i, j, k} >= - M
        // with M = H_i - (earliest completion time of j), so that the
        // constraint is redundant when x_{i, j, k} = 0.
        for (JobId job_id = 0; job_id < n; ++job_id) {
            for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
                double big_m = horizons[machine_id] - earliest_completion_times[job_id];
                if (big_m <= 0)
                    big_m = 0;
                for (JobId k = 1; k <= n; ++k) {
                    add_constraint(
                            model.model,
                            {
                                {job_completion_times[job_id], 1.0},
                                {position_completion_times[machine_id][k - 1], -1.0},
                                {model.x[machine_id][job_id][k - 1], -big_m},
                            },
                            -big_m,
                            inf);
                }
            }
        }

        // Valid inequality: sum_j C_j >= sum_{i, j, k} k p_{i, j} x_{i, j, k}.
        std::vector<std::pair<int, double>> elements;
        for (JobId job_id = 0; job_id < n; ++job_id)
            elements.push_back({job_completion_times[job_id], 1.0});
        for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
            for (JobId job_id = 0; job_id < n; ++job_id) {
                Time p = instance.job(job_id).processing_times[machine_id];
                if (p == 0)
                    continue;
                for (JobId k = 1; k <= n; ++k)
                    elements.push_back({model.x[machine_id][job_id][k - 1], -(double)k * p});
            }
        }
        add_constraint(model.model, elements, 0, inf);
    }

    // Objective.
    switch (objective) {
    case Objective::Makespan: {
        // C_max >= C_{i, 1}: the last position of each machine.
        int cmax = add_variable(
                model.model,
                *std::max_element(earliest_completion_times.begin(), earliest_completion_times.end()),
                upper_bound,
                mathoptsolverscmake::VariableType::Continuous,
                1);
        for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
            add_constraint(
                    model.model,
                    {{cmax, 1.0}, {position_completion_times[machine_id][0], -1.0}},
                    0,
                    inf);
        }
        break;
    } case Objective::TotalFlowTime: {
        // z = sum_j w_j (C_j - r_j), z <= UB:
        // z - sum_j w_j C_j = - sum_j w_j r_j.
        int z = add_variable(
                model.model,
                0,
                upper_bound,
                mathoptsolverscmake::VariableType::Continuous,
                1);
        std::vector<std::pair<int, double>> elements = {{z, 1.0}};
        double constant = 0;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            const Job& job = instance.job(job_id);
            elements.push_back({job_completion_times[job_id], -(double)job.weight});
            constant += job.weight * job.release_date;
        }
        add_constraint(model.model, elements, -constant, -constant);
        break;
    } case Objective::TotalTardiness: {
        // T_j >= C_j - d_j, T_j >= 0; sum_j w_j T_j <= UB.
        std::vector<std::pair<int, double>> elements;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            const Job& job = instance.job(job_id);
            if (job.due_date < 0)
                continue;
            int t = add_variable(
                    model.model,
                    0,
                    inf,
                    mathoptsolverscmake::VariableType::Continuous,
                    job.weight);
            add_constraint(
                    model.model,
                    {{t, 1.0}, {job_completion_times[job_id], -1.0}},
                    -(double)job.due_date,
                    inf);
            elements.push_back({t, (double)job.weight});
        }
        if (!elements.empty())
            add_constraint(model.model, elements, -inf, upper_bound);
        break;
    } case Objective::MaximumLateness: {
        // L_max >= C_j - d_j.
        int lmax = add_variable(
                model.model,
                -inf,
                upper_bound,
                mathoptsolverscmake::VariableType::Continuous,
                1);
        for (JobId job_id = 0; job_id < n; ++job_id) {
            const Job& job = instance.job(job_id);
            if (job.due_date < 0)
                continue;
            add_constraint(
                    model.model,
                    {{lmax, 1.0}, {job_completion_times[job_id], -1.0}},
                    -(double)job.due_date,
                    inf);
        }
        break;
    } case Objective::Throughput: {
        // u_j = 1 iff job j is on time: C_j - d_j + M u_j <= M, with
        // M = (horizon) - d_j; sum_j w_j u_j >= UB.
        std::vector<std::pair<int, double>> elements;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            const Job& job = instance.job(job_id);
            if (job.due_date < 0)
                continue;
            if (earliest_completion_times[job_id] > job.due_date)
                continue;
            int u = add_variable(
                    model.model,
                    0,
                    1,
                    mathoptsolverscmake::VariableType::Binary,
                    job.weight);
            double big_m = (std::max)((Time)0, maximum_horizon - job.due_date);
            add_constraint(
                    model.model,
                    {{job_completion_times[job_id], 1.0}, {u, big_m}},
                    -inf,
                    job.due_date + big_m);
            elements.push_back({u, (double)job.weight});
        }
        if (!elements.empty())
            add_constraint(model.model, elements, upper_bound, inf);
        break;
    }
    }

    return model;
}

Solution retrieve_solution(
        const Instance& instance,
        const Model& model,
        const std::vector<double>& milp_solution)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        // Positions from the end: the first job has the largest position.
        std::vector<std::pair<JobId, JobId>> jobs;
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
            for (JobId k = 1; k <= instance.number_of_jobs(); ++k)
                if (milp_solution[model.x[machine_id][job_id][k - 1]] > 0.5)
                    jobs.push_back({k, job_id});
        std::sort(jobs.rbegin(), jobs.rend());
        Time time = 0;
        for (const auto& p: jobs) {
            JobId job_id = p.second;
            const Job& job = instance.job(job_id);
            Time start = (std::max)(time, job.release_date);
            solution_builder.append_job_to_machine(machine_id, job_id, start);
            time = start + job.processing_times[machine_id];
        }
    }
    return solution_builder.build();
}

}

Output parallelschedulingsolver::milp_positional(
        const Instance& instance,
        const MilpPositionalParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Positional MILP");

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

    if (instance.number_of_jobs() == 0) {
        algorithm_formatter.update_solution(
                SolutionBuilder().set_instance(instance).build(),
                "");
        algorithm_formatter.end();
        return output;
    }

    // Initial solution: the best greedy solution. Its value bounds the
    // objective of the model.
    Parameters greedy_parameters;
    greedy_parameters.verbosity_level = 0;
    for (const Output& greedy_output: {
            greedy_longest_processing_time(instance, greedy_parameters),
            greedy_shortest_processing_time(instance, greedy_parameters),
            greedy_earliest_due_date(instance, greedy_parameters)}) {
        algorithm_formatter.update_solution(greedy_output.solution, "greedy");
    }
    double upper_bound = output.solution.objective_value();

    Model milp_model = create_milp_model(instance, upper_bound);
    algorithm_formatter.print(
            "MILP "
            + std::to_string(milp_model.model.number_of_variables()) + " vars "
            + std::to_string(milp_model.model.number_of_constraints()) + " rows");

    MilpSolveParameters milp_parameters;
    milp_parameters.solver = parameters.solver;
    solve_milp(
            instance,
            milp_model.model,
            milp_parameters,
            parameters,
            algorithm_formatter,
            [&instance, &milp_model](const std::vector<double>& milp_solution)
            {
                return retrieve_solution(instance, milp_model, milp_solution);
            });

    algorithm_formatter.end();
    return output;
}
