#include "parallelschedulingsolver/algorithms/milp_time_indexed.hpp"

#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"

#include "milp.hpp"

#include <algorithm>
#include <cmath>

using namespace parallelschedulingsolver;

namespace
{

/*
 * Possible start times
 */

/** Bitset of the times 0..size-1, for the subset sum dynamic program. */
class TimeBitset
{

public:

    TimeBitset(Time size): words_((size + 63) / 64, 0), size_(size) { }

    void set(Time t) { words_[t / 64] |= ((uint64_t)1 << (t % 64)); }

    bool test(Time t) const { return (words_[t / 64] >> (t % 64)) & 1; }

    /** Add p to all the elements of the set and merge: b = b | (b << p). */
    void or_shift(Time p)
    {
        if (p <= 0)
            return;
        Time word_shift = p / 64;
        Time bit_shift = p % 64;
        // Go downwards, so that the words read have not been updated yet.
        for (Time k = (Time)words_.size() - 1; k >= word_shift; --k) {
            uint64_t value = words_[k - word_shift] << bit_shift;
            if (bit_shift != 0 && k - word_shift >= 1)
                value |= words_[k - word_shift - 1] >> (64 - bit_shift);
            words_[k] |= value;
        }
    }

    Time size() const { return size_; }

private:

    std::vector<uint64_t> words_;

    Time size_;

};

/**
 * Compute the possible start times of each job on each machine.
 *
 * In a semi-active schedule, a job starts at time 0 or at a release date, plus
 * the sum of the processing times of some of the other jobs of its machine.
 * So the start times of job j on machine i are at most
 * max_l r_l + sum_{k != j} p_{i, k}.
 */
std::vector<std::vector<std::vector<Time>>> compute_start_times(
        const Instance& instance,
        bool normal_instants)
{
    // Base times: 0 and the release dates.
    std::vector<Time> base_times = {0};
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        base_times.push_back(instance.job(job_id).release_date);
    std::sort(base_times.begin(), base_times.end());
    base_times.erase(std::unique(base_times.begin(), base_times.end()), base_times.end());
    Time base_time_max = base_times.back();

    std::vector<std::vector<std::vector<Time>>> start_times(
            instance.number_of_machines(),
            std::vector<std::vector<Time>>(instance.number_of_jobs()));
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        Time processing_time_sum = 0;
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
            processing_time_sum += instance.job(job_id).processing_times[machine_id];

        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
            const Job& job = instance.job(job_id);
            Time horizon = base_time_max + processing_time_sum
                - job.processing_times[machine_id];
            std::vector<Time>& times = start_times[machine_id][job_id];

            if (!normal_instants) {
                for (Time t = job.release_date; t <= horizon; ++t)
                    times.push_back(t);
                continue;
            }

            // Subset sums of the processing times of the other jobs, shifted
            // by the base times.
            TimeBitset reachable(horizon + 1);
            for (Time base_time: base_times)
                reachable.set(base_time);
            for (JobId job_id_2 = 0; job_id_2 < instance.number_of_jobs(); ++job_id_2) {
                if (job_id_2 == job_id)
                    continue;
                reachable.or_shift(instance.job(job_id_2).processing_times[machine_id]);
            }

            // The job can't start before its release date: a reachable time
            // t < r_j gives a start at r_j, which is a base time.
            for (Time t = job.release_date; t <= horizon; ++t)
                if (reachable.test(t))
                    times.push_back(t);
        }
    }
    return start_times;
}

/*
 * Model
 */

struct StartVariable
{
    MachineId machine_id;

    JobId job_id;

    Time start;
};

struct Model
{
    /** Model. */
    mathoptsolverscmake::MathOptModel model;

    /** Start variables x_{i, j, s}, indexed by variable id. */
    std::vector<StartVariable> start_variables;

    /** Id of the first start variable; they are consecutive. */
    int start_variables_begin = 0;
};

/** Completion time if a job starts at 'start' on a machine. */
Time completion_time(
        const Instance& instance,
        const StartVariable& start_variable)
{
    return start_variable.start
        + instance.job(start_variable.job_id).processing_times[start_variable.machine_id];
}

/** Cost of a start variable in the objective (for sum objectives). */
double start_variable_cost(
        const Instance& instance,
        const StartVariable& start_variable)
{
    const Job& job = instance.job(start_variable.job_id);
    Time completion = completion_time(instance, start_variable);
    switch (instance.objective()) {
    case Objective::TotalFlowTime:
        return job.weight * (completion - job.release_date);
    case Objective::TotalTardiness:
        return (job.due_date >= 0)? job.weight * (std::max)((Time)0, completion - job.due_date): 0;
    case Objective::Throughput:
        return (job.due_date >= 0 && completion <= job.due_date)? job.weight: 0;
    case Objective::Makespan:
    case Objective::MaximumLateness:
        return 0;
    }
    return 0;
}

/**
 * Return true if a start variable can be removed given an upper bound on the
 * objective value of an optimal solution (minimization objectives only).
 *
 * 'other_jobs_cost' is a lower bound on the total cost of the other jobs.
 */
bool exceeds_upper_bound(
        const Instance& instance,
        const StartVariable& start_variable,
        double cost,
        double other_jobs_cost,
        double upper_bound)
{
    const Job& job = instance.job(start_variable.job_id);
    Time completion = completion_time(instance, start_variable);
    switch (instance.objective()) {
    case Objective::Makespan:
        return completion > upper_bound;
    case Objective::MaximumLateness:
        return job.due_date >= 0 && completion - job.due_date > upper_bound;
    case Objective::TotalFlowTime:
    case Objective::TotalTardiness:
        return cost + other_jobs_cost > upper_bound;
    case Objective::Throughput:
        return false;
    }
    return false;
}

Model create_milp_model(
        const Instance& instance,
        bool normal_instants,
        double upper_bound)
{
    const double inf = std::numeric_limits<double>::infinity();
    Model model;
    model.model.objective_direction = (instance.objective() == Objective::Throughput)?
        mathoptsolverscmake::ObjectiveDirection::Maximize:
        mathoptsolverscmake::ObjectiveDirection::Minimize;

    std::vector<std::vector<std::vector<Time>>> start_times = compute_start_times(
            instance,
            normal_instants);

    // Smallest cost of each job, for the filtering of the start variables.
    std::vector<double> job_minimum_costs(instance.number_of_jobs(), inf);
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
            for (Time start: start_times[machine_id][job_id]) {
                double cost = start_variable_cost(instance, {machine_id, job_id, start});
                job_minimum_costs[job_id] = (std::min)(job_minimum_costs[job_id], cost);
            }
        }
    }
    double minimum_costs_sum = 0;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        minimum_costs_sum += job_minimum_costs[job_id];

    // Variables x_{i, j, s}.
    // Without this filtering, HiGHS may run out of memory when it derives
    // cliques from the objective cutoff on large models.
    model.start_variables_begin = model.model.number_of_variables();
    std::vector<std::vector<int>> job_variables(instance.number_of_jobs());
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
            double other_jobs_cost = minimum_costs_sum - job_minimum_costs[job_id];
            std::vector<Time> kept_start_times;
            for (Time start: start_times[machine_id][job_id]) {
                StartVariable start_variable = {machine_id, job_id, start};
                double cost = start_variable_cost(instance, start_variable);
                if (exceeds_upper_bound(instance, start_variable, cost, other_jobs_cost, upper_bound))
                    continue;
                kept_start_times.push_back(start);
                int variable_id = add_variable(
                        model.model,
                        0,
                        1,
                        mathoptsolverscmake::VariableType::Binary,
                        cost);
                model.start_variables.push_back(start_variable);
                job_variables[job_id].push_back(variable_id);
            }
            start_times[machine_id][job_id] = kept_start_times;
        }
    }
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        if (job_variables[job_id].empty()) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": "
                    "no start variable left for job " + std::to_string(job_id) + ".");
        }
    }

    // Constraints: each job starts exactly once.
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        std::vector<std::pair<int, double>> elements;
        for (int variable_id: job_variables[job_id])
            elements.push_back({variable_id, 1.0});
        add_constraint(model.model, elements, 1, 1);
    }

    // Constraints: capacity of the machines, as a unit flow from the first
    // to the last time of each machine.
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        // Nodes: the possible start and completion times.
        std::vector<Time> times = {0};
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
            Time p = instance.job(job_id).processing_times[machine_id];
            for (Time start: start_times[machine_id][job_id]) {
                times.push_back(start);
                times.push_back(start + p);
            }
        }
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
        auto node = [&times](Time t)
        {
            return std::lower_bound(times.begin(), times.end(), t) - times.begin();
        };

        // Flow conservation elements of each node: out - in.
        std::vector<std::vector<std::pair<int, double>>> node_elements(times.size());
        for (int variable_id = model.start_variables_begin;
                variable_id < model.start_variables_begin + (int)model.start_variables.size();
                ++variable_id) {
            const StartVariable& start_variable = model.start_variables[variable_id - model.start_variables_begin];
            if (start_variable.machine_id != machine_id)
                continue;
            Time completion = completion_time(instance, start_variable);
            if (completion == start_variable.start)
                continue;
            node_elements[node(start_variable.start)].push_back({variable_id, 1.0});
            node_elements[node(completion)].push_back({variable_id, -1.0});
        }
        // Idle arcs.
        for (int node_id = 0; node_id + 1 < (int)times.size(); ++node_id) {
            int variable_id = add_variable(
                    model.model,
                    0,
                    1,
                    mathoptsolverscmake::VariableType::Continuous,
                    0);
            node_elements[node_id].push_back({variable_id, 1.0});
            node_elements[node_id + 1].push_back({variable_id, -1.0});
        }
        for (int node_id = 0; node_id < (int)times.size(); ++node_id) {
            double rhs = 0;
            if (node_id == 0)
                rhs = 1;
            if (node_id == (int)times.size() - 1)
                rhs -= 1;
            add_constraint(model.model, node_elements[node_id], rhs, rhs);
        }
    }

    // Sum objectives: move the costs of the start variables to a variable
    // z = sum c_{i, j, s} x_{i, j, s}, and optimize z. This has the
    // same linear relaxation, but HiGHS then doesn't derive cliques from the
    // objective cutoff on the binary variables, which can exhaust the memory
    // on large time-indexed models.
    if (instance.objective() == Objective::TotalFlowTime
            || instance.objective() == Objective::TotalTardiness
            || instance.objective() == Objective::Throughput) {
        // The bound from the initial solution is not implied by the
        // constraint defining z, so presolve doesn't substitute z back into
        // the objective.
        bool maximize = (instance.objective() == Objective::Throughput);
        int objective_variable_id = add_variable(
                model.model,
                (maximize)? upper_bound: 0,
                (maximize)? inf: upper_bound,
                mathoptsolverscmake::VariableType::Integer,
                1);
        std::vector<std::pair<int, double>> elements = {{objective_variable_id, -1.0}};
        for (int variable_id = model.start_variables_begin;
                variable_id < model.start_variables_begin + (int)model.start_variables.size();
                ++variable_id) {
            double cost = model.model.objective_coefficients[variable_id];
            if (cost == 0)
                continue;
            elements.push_back({variable_id, cost});
            model.model.objective_coefficients[variable_id] = 0;
        }
        add_constraint(model.model, elements, 0, 0);
    }

    // Makespan and maximum lateness: maximum of the completion times (or of
    // the latenesses) of the jobs.
    if (instance.objective() == Objective::Makespan
            || instance.objective() == Objective::MaximumLateness) {
        bool lateness = (instance.objective() == Objective::MaximumLateness);
        bool has_due_date = false;
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
            if (instance.job(job_id).due_date >= 0)
                has_due_date = true;
        if (!lateness || has_due_date) {
            int max_variable_id = add_variable(
                    model.model,
                    (lateness)? -inf: 0,
                    inf,
                    mathoptsolverscmake::VariableType::Continuous,
                    1);
            for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
                const Job& job = instance.job(job_id);
                if (lateness && job.due_date < 0)
                    continue;
                // max - sum (C_{i, j, s} - d_j) x_{i, j, s} >= 0
                std::vector<std::pair<int, double>> elements = {{max_variable_id, 1.0}};
                for (int variable_id: job_variables[job_id]) {
                    const StartVariable& start_variable = model.start_variables[variable_id - model.start_variables_begin];
                    double value = completion_time(instance, start_variable);
                    if (lateness)
                        value -= job.due_date;
                    elements.push_back({variable_id, -value});
                }
                add_constraint(model.model, elements, 0, inf);
            }
        }
    }

    return model;
}

Solution retrieve_solution(
        const Instance& instance,
        const Model& model,
        const std::vector<double>& milp_solution)
{
    std::vector<std::vector<std::pair<Time, JobId>>> machine_jobs(instance.number_of_machines());
    for (int variable_id = model.start_variables_begin;
            variable_id < model.start_variables_begin + (int)model.start_variables.size();
            ++variable_id) {
        if (milp_solution[variable_id] < 0.5)
            continue;
        const StartVariable& start_variable = model.start_variables[variable_id - model.start_variables_begin];
        machine_jobs[start_variable.machine_id].push_back({start_variable.start, start_variable.job_id});
    }
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        std::sort(machine_jobs[machine_id].begin(), machine_jobs[machine_id].end());
        for (const auto& p: machine_jobs[machine_id])
            solution_builder.append_job_to_machine(machine_id, p.second, p.first);
    }
    return solution_builder.build();
}

}

Output parallelschedulingsolver::milp_time_indexed(
        const Instance& instance,
        const MilpTimeIndexedParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Time-indexed MILP");

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

    // Initial solution: the best greedy solution. Its value is an upper bound
    // used to remove start variables.
    Parameters greedy_parameters;
    greedy_parameters.verbosity_level = 0;
    for (const Output& greedy_output: {
            greedy_longest_processing_time(instance, greedy_parameters),
            greedy_shortest_processing_time(instance, greedy_parameters),
            greedy_earliest_due_date(instance, greedy_parameters)}) {
        algorithm_formatter.update_solution(greedy_output.solution, "greedy");
    }
    double upper_bound = output.solution.objective_value();

    Model milp_model = create_milp_model(
            instance,
            parameters.normal_instants,
            upper_bound);
    algorithm_formatter.print(
            "MILP "
            + std::to_string(milp_model.model.number_of_variables()) + " vars "
            + std::to_string(milp_model.model.number_of_constraints()) + " rows");

    MilpSolveParameters milp_parameters;
    milp_parameters.solver = parameters.solver;
    // HiGHS MIP presolve barely reduces this model and slows down the
    // solve: with HiGHS 1.16.0 and a 1 s time limit, 713 of the 840 test
    // instances are solved with presolve off, 657 with presolve on.
    milp_parameters.presolve = false;
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
