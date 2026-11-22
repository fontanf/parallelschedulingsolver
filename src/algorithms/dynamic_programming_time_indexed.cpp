#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"

#include <cmath>
#include <numeric>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

namespace
{

/** Tolerance on the lower bounds, computed with floating point multipliers. */
constexpr double TOLERANCE = 1e-6;

Direction opposite(Direction direction)
{
    return (direction == Direction::Forward)? Direction::Backward: Direction::Forward;
}

/** Compute the labels of a direction if they are not up to date. */
void update_labels(
        Network& network,
        Direction direction,
        const std::vector<double>& multipliers)
{
    if (network.labels_up_to_date(direction, multipliers))
        return;
    if (direction == Direction::Forward) {
        network.compute_forward_labels(multipliers);
    } else {
        network.compute_backward_labels(multipliers);
    }
}

}

const DynamicProgrammingTimeIndexedOutput parallelschedulingsolver::dynamic_programming_time_indexed(
        const Instance& instance,
        const DynamicProgrammingTimeIndexedParameters& parameters)
{
    DynamicProgrammingTimeIndexedOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Dynamic programming, time-indexed");

    if (instance.has_rejection_costs()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "rejection costs are not supported.");
    }

    // Problem and initial network.
    std::shared_ptr<Network> network = parameters.network;
    if (network == nullptr) {
        std::shared_ptr<const Problem> problem = build_problem(
                instance,
                parameters.remove_zero_cost_jobs);
        network = std::make_shared<Network>(Network::initial_network(problem));
    }
    const Problem& problem = network->problem();
    JobId n = problem.number_of_jobs;

    algorithm_formatter.print_header();

    if (n == 0) {
        algorithm_formatter.update_solution(
                build_solution(instance, problem, {}),
                "no job");
        if (problem.tardiness) {
            algorithm_formatter.update_total_tardiness_bound(0, "no job");
        } else {
            algorithm_formatter.update_total_flow_time_bound(0, "no job");
        }
        output.network = network;
        output.lagrangian_bound = 0;
        algorithm_formatter.end();
        return output;
    }

    std::vector<double> multipliers = parameters.multipliers;
    if (multipliers.empty())
        multipliers.assign(n, 0);
    if ((JobId)multipliers.size() != n) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "wrong number of multipliers; "
                "number of multipliers: " + std::to_string(multipliers.size()) + "; "
                "number of jobs of the problem: " + std::to_string(n) + ".");
    }
    double sum_of_multipliers = std::accumulate(multipliers.begin(), multipliers.end(), 0.0);
    Direction direction = parameters.direction;

    // Restrictions of the completion times.
    for (const auto& window: parameters.completion_time_windows)
        network->restrict(window.job_id, window.earliest, window.latest);

    // Generation of the network with the new modifiers.
    Mask new_modifiers = network->modifiers();
    bool generation = false;
    for (JobId job_id: parameters.new_modifier_jobs) {
        if (job_id < 0 || job_id >= n) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "invalid job of the new modifiers; "
                    "job_id: " + std::to_string(job_id) + ".");
        }
        if (!new_modifiers.get(job_id)) {
            new_modifiers.set(job_id);
            generation = true;
        }
    }
    if (parameters.all_jobs_in_modifiers) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (!new_modifiers.get(job_id)) {
                new_modifiers.set(job_id);
                generation = true;
            }
        }
    }
    if (generation) {
        // The generation in a direction uses the labels of the other
        // direction of the network.
        update_labels(*network, opposite(direction), multipliers);
        auto new_network = std::make_shared<Network>(network->problem_pointer());
        bool ok = network->generate(
                *new_network,
                new_modifiers,
                direction,
                multipliers,
                sum_of_multipliers,
                parameters.cutoff,
                parameters.maximum_memory * 1024 * 1024,
                &parameters.timer);
        if (!ok) {
            output.memory_out = !parameters.timer.needs_to_end();
            output.network = network;
            output.number_of_states = network->number_of_states();
            algorithm_formatter.end();
            return output;
        }
        network = new_network;
    }

    // Reduction.
    if (parameters.reduction) {
        update_labels(*network, Direction::Forward, multipliers);
        update_labels(*network, Direction::Backward, multipliers);
        network->reduce(multipliers, sum_of_multipliers, parameters.cutoff);
    }
    if (parameters.constraint_propagation)
        network->propagate_constraints();

    output.network = network;
    output.number_of_states = network->number_of_states();
    if (!parameters.compute_bound) {
        algorithm_formatter.end();
        return output;
    }

    // Labels, lower bound and shortest path.
    update_labels(*network, direction, multipliers);
    if (parameters.both_directions_labels)
        update_labels(*network, opposite(direction), multipliers);
    output.network = network;
    output.number_of_states = network->number_of_states();
    double length = network->shortest_path_length(direction, multipliers);
    output.lagrangian_bound = (length == INF)? INF: length + sum_of_multipliers;
    if (length != INF)
        output.path = network->shortest_path(direction, multipliers);
    algorithm_formatter.print(
            "states " + std::to_string(network->number_of_states())
            + " lb " + std::to_string(output.lagrangian_bound));

    // A path processing each job exactly once is a schedule.
    std::vector<JobId> occurrences(n, 0);
    for (JobId job_id: output.path)
        occurrences[job_id]++;
    bool feasible = (length != INF);
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (occurrences[job_id] != 1)
            feasible = false;
    if (feasible) {
        algorithm_formatter.update_solution(
                build_solution(instance, problem, output.path),
                "shortest path");
    }

    // Bound of the instance: all the schedules of cost at most the cutoff of
    // the network are paths of the network.
    if (!network->restricted()) {
        double bound = output.lagrangian_bound;
        if (network->cutoff() != INF)
            bound = (std::min)(bound, std::floor(network->cutoff()) + 1);
        if (bound != INF) {
            Time bound_time = (Time)std::ceil(bound - TOLERANCE);
            if (problem.tardiness) {
                algorithm_formatter.update_total_tardiness_bound(bound_time, "dynamic programming");
            } else {
                algorithm_formatter.update_total_flow_time_bound(bound_time, "dynamic programming");
            }
        }
    }

    algorithm_formatter.end();
    return output;
}
