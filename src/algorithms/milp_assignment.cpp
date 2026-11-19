#include "parallelschedulingsolver/algorithms/milp_assignment.hpp"

#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"

#include "milp.hpp"

#include <algorithm>

using namespace parallelschedulingsolver;

namespace
{

const double inf = std::numeric_limits<double>::infinity();

struct AssignmentVariable
{
    MachineId machine_id;

    JobId job_id;
};

struct Model
{
    mathoptsolverscmake::MathOptModel model;

    /** Assignment variables, with ids 0 .. size-1 in the model. */
    std::vector<AssignmentVariable> assignment_variables;
};

/**
 * For the makespan, the release date of the job, and for the maximum
 * lateness, minus its due date (jobs without due date last): on each machine,
 * the jobs are sequenced by increasing key, and the constraints of the
 * model are written for each key.
 */
Time job_key(
        const Instance& instance,
        JobId job_id)
{
    const Job& job = instance.job(job_id);
    if (instance.objective() == Objective::Makespan)
        return job.release_date;
    return (job.due_date >= 0)? job.due_date: std::numeric_limits<Time>::max();
}

Model create_milp_model(
        const Instance& instance,
        double upper_bound)
{
    bool lateness = (instance.objective() == Objective::MaximumLateness);
    Model model;
    model.model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

    // Variables x_{i, j}, without the assignments that can only appear in
    // solutions worse than the initial one.
    std::vector<std::vector<int>> job_variables(instance.number_of_jobs());
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            Time p = job.processing_times[machine_id];
            if (!lateness && job.release_date + p > upper_bound)
                continue;
            if (lateness && job.due_date >= 0 && p - job.due_date > upper_bound)
                continue;
            int variable_id = add_variable(
                    model.model,
                    0,
                    1,
                    mathoptsolverscmake::VariableType::Binary,
                    0);
            model.assignment_variables.push_back({machine_id, job_id});
            job_variables[job_id].push_back(variable_id);
        }
        if (job_variables[job_id].empty()) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": "
                    "no possible machine for job " + std::to_string(job_id) + ".");
        }
    }

    // Variable C_max or L_max, with a trivial lower bound: each job is
    // processed on its fastest machine at the earliest.
    double lower_bound = -inf;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        if (lateness && job.due_date < 0)
            continue;
        Time p_min = *std::min_element(
                job.processing_times.begin(),
                job.processing_times.end());
        double value = (lateness)?
            p_min - job.due_date:
            job.release_date + p_min;
        lower_bound = (std::max)(lower_bound, value);
    }
    if (lower_bound == -inf)
        lower_bound = upper_bound;
    int max_variable_id = add_variable(
            model.model,
            lower_bound,
            upper_bound,
            mathoptsolverscmake::VariableType::Integer,
            1);

    // Constraints: each job is assigned to one machine.
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        std::vector<std::pair<int, double>> elements;
        for (int variable_id: job_variables[job_id])
            elements.push_back({variable_id, 1.0});
        add_constraint(model.model, elements, 1, 1);
    }

    // Keys: release dates (makespan) or due dates (maximum lateness).
    std::vector<Time> keys;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        if (lateness && instance.job(job_id).due_date < 0)
            continue;
        keys.push_back(job_key(instance, job_id));
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    // Constraints: value of each machine.
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        for (Time key: keys) {
            // Makespan: C_max - sum_{k: r_k >= rho} p_{i, k} x_{i, k} >= rho.
            // Maximum lateness:
            // L_max - sum_{k: d_k <= delta} p_{i, k} x_{i, k} >= -delta.
            std::vector<std::pair<int, double>> elements = {{max_variable_id, 1.0}};
            for (int variable_id = 0;
                    variable_id < (int)model.assignment_variables.size();
                    ++variable_id) {
                const AssignmentVariable& variable = model.assignment_variables[variable_id];
                if (variable.machine_id != machine_id)
                    continue;
                const Job& job = instance.job(variable.job_id);
                Time job_key_value = job_key(instance, variable.job_id);
                bool in_sum = (lateness)?
                    (job.due_date >= 0 && job_key_value <= key):
                    (job_key_value >= key);
                if (!in_sum || job.processing_times[machine_id] == 0)
                    continue;
                elements.push_back({variable_id, -(double)job.processing_times[machine_id]});
            }
            if (elements.size() == 1)
                continue;
            add_constraint(
                    model.model,
                    elements,
                    (lateness)? -(double)key: (double)key,
                    inf);
        }
    }

    return model;
}

Solution retrieve_solution(
        const Instance& instance,
        const Model& model,
        const std::vector<double>& milp_solution)
{
    std::vector<std::vector<JobId>> machine_jobs(instance.number_of_machines());
    for (int variable_id = 0;
            variable_id < (int)model.assignment_variables.size();
            ++variable_id) {
        if (milp_solution[variable_id] < 0.5)
            continue;
        const AssignmentVariable& variable = model.assignment_variables[variable_id];
        machine_jobs[variable.machine_id].push_back(variable.job_id);
    }
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        std::vector<JobId>& jobs = machine_jobs[machine_id];
        std::stable_sort(
                jobs.begin(),
                jobs.end(),
                [&instance](JobId job_id_1, JobId job_id_2)
                {
                    return job_key(instance, job_id_1) < job_key(instance, job_id_2);
                });
        Time time = 0;
        for (JobId job_id: jobs) {
            const Job& job = instance.job(job_id);
            Time start = (std::max)(time, job.release_date);
            solution_builder.append_job_to_machine(machine_id, job_id, start);
            time = start + job.processing_times[machine_id];
        }
    }
    return solution_builder.build();
}

}

Output parallelschedulingsolver::milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Assignment MILP");

    if (instance.objective() != Objective::Makespan
            && instance.objective() != Objective::MaximumLateness) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the assignment MILP only supports the makespan and the "
                "maximum lateness objectives.");
    }
    if (instance.objective() == Objective::MaximumLateness) {
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
            if (instance.job(job_id).release_date != 0) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "the assignment MILP doesn't support release dates "
                        "with the maximum lateness objective.");
            }
        }
    }

    algorithm_formatter.print_header();

    if (instance.number_of_jobs() == 0) {
        algorithm_formatter.update_solution(
                SolutionBuilder().set_instance(instance).build(),
                "");
        algorithm_formatter.end();
        return output;
    }

    // Initial solution: the best greedy solution. Its value is an upper bound
    // used to remove assignment variables.
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
