#pragma once

/**
 * Implementation shared by the Lagrangian relaxation and the column
 * generation: evaluation of the relaxation with the time-indexed dynamic
 * programming, subgradient, update of the bounds, call of the iteration
 * callback, and reduction of the networks.
 *
 * Single machine: one network, the one of the time-indexed dynamic
 * programming (with its state-space modifiers and the rejection costs).
 *
 * Parallel machines: one network for each group of identical machines, of the
 * subproblem of a machine (time_indexed::build_machine_problem: all the jobs
 * optional, no state-space modifiers). With multipliers mu, the relaxation of
 * "each job is processed exactly once or rejected" decomposes by machine:
 *
 *     L(mu) = sum_g m_g SP_g(mu) + sum_i mu_i + sum_{i rejectable} min(0, e_i - mu_i)
 *
 * where m_g is the number of machines of group g and SP_g(mu) <= 0 the length
 * of the shortest path of its network (the empty path included), with arc
 * lengths f_i(t) - mu_i.
 */

#include "parallelschedulingsolver/algorithms/lagrangian_dual.hpp"
#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"

#include <cmath>
#include <numeric>

namespace parallelschedulingsolver
{
namespace lagrangian_dual
{

/** Tolerance on the lower bounds, computed with floating point multipliers. */
constexpr double TOLERANCE = 1e-6;

class LagrangianDual
{

public:

    LagrangianDual(
            const Instance& instance,
            const LagrangianDualParameters& parameters,
            LagrangianDualOutput& output,
            AlgorithmFormatter& algorithm_formatter):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        algorithm_formatter_(algorithm_formatter)
    {
        MachineId m = instance.number_of_machines();
        single_machine_ = (m == 1);
        if (single_machine_) {
            std::shared_ptr<time_indexed::Network> network = parameters.network;
            if (network == nullptr) {
                network = std::make_shared<time_indexed::Network>(
                        time_indexed::Network::initial_network(
                            time_indexed::build_problem(instance)));
            }
            networks_.push_back(network);
            machine_ids_.push_back({0});
            rejection_costs_ = network->problem().rejection_costs;
        } else {
            if (parameters.network != nullptr) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "an input network is only supported on a single machine.");
            }
            // A group of the machines identical to its first machine.
            for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
                bool found = false;
                for (std::vector<MachineId>& group: machine_ids_) {
                    if (time_indexed::identical_machines(instance, group[0], machine_id)) {
                        group.push_back(machine_id);
                        found = true;
                        break;
                    }
                }
                if (!found)
                    machine_ids_.push_back({machine_id});
            }
            for (const std::vector<MachineId>& group: machine_ids_) {
                networks_.push_back(std::make_shared<time_indexed::Network>(
                            time_indexed::Network::initial_network(
                                time_indexed::build_machine_problem(instance, group[0]))));
            }
            for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
                rejection_costs_.push_back(instance.job(job_id).rejection_cost);
        }
        output_.networks = networks_;
        output_.network_machine_ids = machine_ids_;
        lengths_.assign(networks_.size(), 0);

        // Upper bound.
        upper_bound_ = parameters.upper_bound;
        if (upper_bound_ < 0)
            initial_upper_bound();
        output_.upper_bound = upper_bound_;
    }

    const time_indexed::Problem& problem(Counter group_id = 0) const { return networks_[group_id]->problem(); }

    JobId number_of_jobs() const { return networks_[0]->problem().number_of_jobs; }

    bool single_machine() const { return single_machine_; }

    /** Number of networks (groups of identical machines). */
    Counter number_of_groups() const { return networks_.size(); }

    /** Number of machines of a group. */
    MachineId multiplicity(Counter group_id) const { return machine_ids_[group_id].size(); }

    /** Rejection cost of a job of the problems; -1 if it must be scheduled. */
    Time rejection_cost(JobId job_id) const { return rejection_costs_[job_id]; }

    /**
     * Length of the shortest path of each network, without the sum of the
     * multipliers, at the last evaluation.
     */
    const std::vector<double>& lengths() const { return lengths_; }

    Time upper_bound() const { return upper_bound_; }

    /** True if the best lower bound proves that the upper bound is optimal. */
    bool optimal() const { return lower_bound_ >= upper_bound_; }

    bool end() const
    {
        return optimal()
            || parameters_.timer.needs_to_end()
            || output_.number_of_iterations >= parameters_.maximum_number_of_iterations;
    }

    /**
     * Evaluate the relaxation for some multipliers: return the lower bound
     * (INF if no path remains), the shortest path of each network, and
     * whether they are a schedule.
     */
    double evaluate(
            const std::vector<double>& multipliers,
            std::vector<std::vector<JobId>>& paths,
            bool& feasible)
    {
        if (single_machine_) {
            DynamicProgrammingTimeIndexedParameters dp_parameters;
            dp_parameters.verbosity_level = 0;
            dp_parameters.timer = parameters_.timer;
            dp_parameters.network = networks_[0];
            dp_parameters.multipliers = multipliers;
            DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming_time_indexed(
                    instance_,
                    dp_parameters);
            paths = {dp_output.path};
            feasible = dp_output.solution.feasible();
            if (feasible)
                update_upper_bound(dp_output.solution, "relaxation");
            return dp_output.lagrangian_bound;
        }

        // Parallel machines.
        JobId n = number_of_jobs();
        paths.assign(networks_.size(), {});
        double lagrangian_bound = constant_term(multipliers);
        for (Counter group_id = 0; group_id < (Counter)networks_.size(); ++group_id) {
            time_indexed::Network& network = *networks_[group_id];
            update_labels(network, time_indexed::Direction::Forward, multipliers);
            double length = network.shortest_path_length(time_indexed::Direction::Forward, multipliers);
            if (length == time_indexed::INF)
                return time_indexed::INF;
            lengths_[group_id] = length;
            paths[group_id] = network.shortest_path(time_indexed::Direction::Forward, multipliers);
            lagrangian_bound += multiplicity(group_id) * length;
        }

        // The paths are a schedule if each job is processed at most once,
        // and exactly once if it can't be rejected.
        std::vector<JobId> occurrences = this->occurrences(paths);
        feasible = true;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (occurrences[job_id] > 1
                    || (occurrences[job_id] == 0 && rejection_costs_[job_id] < 0)) {
                feasible = false;
                break;
            }
        }
        if (feasible)
            update_upper_bound(build_solution(paths), "relaxation");
        return lagrangian_bound;
    }

    /** Number of times each job is processed by the paths of the networks. */
    std::vector<JobId> occurrences(const std::vector<std::vector<JobId>>& paths) const
    {
        std::vector<JobId> occurrences(number_of_jobs(), 0);
        for (Counter group_id = 0; group_id < (Counter)paths.size(); ++group_id)
            for (JobId job_id: paths[group_id])
                occurrences[job_id] += multiplicity(group_id);
        return occurrences;
    }

    /**
     * Subgradient of the relaxation at some multipliers: 1 - (number of
     * times the job is processed) - (1 if it is rejected); a job of the
     * state-space modifiers (single machine) is processed once or rejected.
     */
    std::vector<double> subgradient(
            const std::vector<double>& multipliers,
            const std::vector<std::vector<JobId>>& paths) const
    {
        JobId n = number_of_jobs();
        std::vector<JobId> occurrences = this->occurrences(paths);
        const time_indexed::Mask& modifiers = networks_[0]->modifiers();
        std::vector<double> subgradient(n, 1);
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (single_machine_ && modifiers.get(job_id)) {
                subgradient[job_id] = 0;
                continue;
            }
            subgradient[job_id] -= occurrences[job_id];
            if (rejection_costs_[job_id] >= 0
                    && rejection_costs_[job_id] < multipliers[job_id]) {
                subgradient[job_id] -= 1;
            }
        }
        return subgradient;
    }

    /** Solution of the paths of the networks (a schedule). */
    Solution build_solution(const std::vector<std::vector<JobId>>& paths) const
    {
        if (single_machine_)
            return time_indexed::build_solution(instance_, problem(), paths[0]);
        std::vector<std::vector<JobId>> machine_sequences(instance_.number_of_machines());
        for (Counter group_id = 0; group_id < (Counter)paths.size(); ++group_id)
            for (MachineId machine_id: machine_ids_[group_id])
                machine_sequences[machine_id] = paths[group_id];
        return time_indexed::build_parallel_solution(instance_, machine_sequences);
    }

    /**
     * Process the result of an evaluation: best lower bound and multipliers,
     * callback, and network reduction. Return true if the best lower bound
     * is improved.
     */
    bool process(
            const std::vector<double>& multipliers,
            double lagrangian_bound,
            const std::vector<std::vector<JobId>>& paths,
            bool feasible)
    {
        output_.number_of_iterations++;
        bool improved = false;
        if (lagrangian_bound > output_.lagrangian_bound + TOLERANCE) {
            improved = true;
            output_.lagrangian_bound = lagrangian_bound;
            output_.multipliers = multipliers;
            update_lower_bound(lagrangian_bound);
            algorithm_formatter_.print(
                    "it " + std::to_string(output_.number_of_iterations)
                    + " lb " + std::to_string(lagrangian_bound)
                    + " states " + std::to_string(output_.number_of_states()));
        }

        if (parameters_.iteration_callback) {
            LagrangianDualIteration iteration{
                    output_.number_of_iterations,
                    multipliers,
                    lagrangian_bound,
                    paths,
                    feasible,
                    improved,
                    upper_bound_};
            Time upper_bound = parameters_.iteration_callback(iteration);
            if (upper_bound >= 0 && upper_bound < upper_bound_) {
                upper_bound_ = upper_bound;
                output_.upper_bound = upper_bound_;
                // The lower bound is capped by the upper bound.
                update_lower_bound(output_.lagrangian_bound);
            }
        }
        if (optimal())
            return improved;

        // Network reduction when the best lower bound or the upper bound is
        // updated, with the labels of the current multipliers.
        if (parameters_.reduction
                && (improved || upper_bound_ < reduction_upper_bound_)) {
            reduction_upper_bound_ = upper_bound_;
            if (single_machine_) {
                reduce_single_machine(multipliers);
            } else {
                reduce_parallel_machines(multipliers);
            }
        }
        return improved;
    }

    /** No path remains: no schedule better than the upper bound. */
    void network_empty() { update_lower_bound(time_indexed::INF); }

private:

    /** Compute the labels of a direction if they are not up to date. */
    static void update_labels(
            time_indexed::Network& network,
            time_indexed::Direction direction,
            const std::vector<double>& multipliers)
    {
        if (network.labels_up_to_date(direction, multipliers))
            return;
        if (direction == time_indexed::Direction::Forward) {
            network.compute_forward_labels(multipliers);
        } else {
            network.compute_backward_labels(multipliers);
        }
    }

    /**
     * Parallel machines: sum of the multipliers plus min(0, rejection cost -
     * multiplier) for each job that can be rejected.
     */
    double constant_term(const std::vector<double>& multipliers) const
    {
        double constant = 0;
        for (JobId job_id = 0; job_id < number_of_jobs(); ++job_id) {
            constant += multipliers[job_id];
            if (rejection_costs_[job_id] >= 0) {
                constant += (std::min)(
                        0.0,
                        rejection_costs_[job_id] - multipliers[job_id]);
            }
        }
        return constant;
    }

    void reduce_single_machine(const std::vector<double>& multipliers)
    {
        time_indexed::Network& network = *networks_[0];
        DynamicProgrammingTimeIndexedParameters dp_parameters;
        dp_parameters.verbosity_level = 0;
        dp_parameters.timer = parameters_.timer;
        dp_parameters.network = networks_[0];
        dp_parameters.multipliers = multipliers;
        dp_parameters.cutoff = upper_bound_ - 1 + TOLERANCE;
        dp_parameters.reduction = true;
        dp_parameters.constraint_propagation = parameters_.constraint_propagation;
        dp_parameters.compute_bound = false;
        dynamic_programming_time_indexed(instance_, dp_parameters);
        if (network.number_of_states() == 0 && network.empty_path_length(multipliers) == time_indexed::INF) {
            // No schedule better than the upper bound.
            update_lower_bound(time_indexed::INF);
        }
    }

    /**
     * Parallel machines: the cost of a schedule whose machine k is in group
     * g(k) and processes the path P_k is at least
     *     sum_k length(P_k) + constant_term >= length(P_k) + sum_{k' != k} SP_{g(k')} + constant_term,
     * so the arcs of the network of a group whose paths are longer than the
     * upper bound minus one minus the other terms are removed. The
     * constraint propagation assumes that all the jobs are processed on the
     * machine; it isn't used.
     */
    void reduce_parallel_machines(const std::vector<double>& multipliers)
    {
        double constant = constant_term(multipliers);
        double total = 0;
        for (Counter group_id = 0; group_id < (Counter)networks_.size(); ++group_id)
            total += multiplicity(group_id) * lengths_[group_id];
        for (Counter group_id = 0; group_id < (Counter)networks_.size(); ++group_id) {
            if (parameters_.timer.needs_to_end())
                return;
            time_indexed::Network& network = *networks_[group_id];
            update_labels(network, time_indexed::Direction::Forward, multipliers);
            update_labels(network, time_indexed::Direction::Backward, multipliers);
            network.reduce(
                    multipliers,
                    constant + total - lengths_[group_id],
                    upper_bound_ - 1 + TOLERANCE);
        }
    }

    /**
     * Initial upper bound: the best of the schedules of all the jobs in the
     * order of the instance, of the weighted shortest processing times and
     * of the earliest due dates; on parallel machines, each job on the
     * machine where it completes first.
     */
    void initial_upper_bound()
    {
        JobId n = number_of_jobs();
        MachineId m = instance_.number_of_machines();
        // Processing times of the problem: on parallel machines, the
        // smallest one.
        std::vector<Time> processing_times(n);
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (single_machine_) {
                processing_times[job_id] = problem().processing_times[job_id];
            } else {
                processing_times[job_id] = std::numeric_limits<Time>::max();
                for (MachineId machine_id = 0; machine_id < m; ++machine_id)
                    processing_times[job_id] = (std::min)(
                            processing_times[job_id],
                            instance_.job(job_id).processing_times[machine_id]);
            }
        }
        const time_indexed::Problem& problem = this->problem();
        std::vector<JobId> identity(n);
        std::iota(identity.begin(), identity.end(), 0);
        std::vector<JobId> wspt = identity;
        std::stable_sort(wspt.begin(), wspt.end(), [&problem, &processing_times](JobId j1, JobId j2)
                {
                    return processing_times[j1] * problem.weights[j2]
                        < processing_times[j2] * problem.weights[j1];
                });
        std::vector<JobId> edd = identity;
        std::stable_sort(edd.begin(), edd.end(), [&problem](JobId j1, JobId j2)
                {
                    return problem.due_dates[j1] < problem.due_dates[j2];
                });
        upper_bound_ = std::numeric_limits<Time>::max();
        for (const std::vector<JobId>& sequence: {identity, wspt, edd}) {
            Solution solution = (single_machine_)?
                time_indexed::build_solution(instance_, problem, sequence):
                list_schedule(sequence);
            Time value = (Time)solution.objective_value();
            if (value < upper_bound_) {
                upper_bound_ = value;
                algorithm_formatter_.update_solution(solution, "initial solution");
            }
        }
    }

    /**
     * Parallel machines: schedule of all the jobs in the order of a
     * sequence, each on the machine where it completes first.
     */
    Solution list_schedule(const std::vector<JobId>& sequence) const
    {
        MachineId m = instance_.number_of_machines();
        std::vector<std::vector<JobId>> machine_sequences(m);
        std::vector<Time> times(m, 0);
        for (JobId job_id: sequence) {
            MachineId best_machine_id = -1;
            Time best_completion = 0;
            for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
                JobId job_id_prev = (machine_sequences[machine_id].empty())?
                    -1: machine_sequences[machine_id].back();
                Time completion = times[machine_id]
                    + instance_.setup_time(machine_id, job_id_prev, job_id)
                    + instance_.job(job_id).processing_times[machine_id];
                if (best_machine_id == -1 || completion < best_completion) {
                    best_machine_id = machine_id;
                    best_completion = completion;
                }
            }
            machine_sequences[best_machine_id].push_back(job_id);
            times[best_machine_id] = best_completion;
        }
        return time_indexed::build_parallel_solution(instance_, machine_sequences);
    }

    void update_upper_bound(
            const Solution& solution,
            const std::string& s)
    {
        Time value = (Time)solution.objective_value();
        if (value >= upper_bound_) {
            if (!output_.solution.feasible())
                algorithm_formatter_.update_solution(solution, s);
            return;
        }
        upper_bound_ = value;
        output_.upper_bound = upper_bound_;
        algorithm_formatter_.update_solution(solution, s);
    }

    /**
     * Update the bound of the instance with a lower bound of the relaxation:
     * all the schedules better than the upper bound are paths of the
     * networks (if they aren't restricted).
     */
    void update_lower_bound(double lagrangian_bound)
    {
        Time bound = (lagrangian_bound == time_indexed::INF)?
            upper_bound_:
            (std::min)((Time)std::ceil(lagrangian_bound - TOLERANCE), upper_bound_);
        if (bound <= lower_bound_)
            return;
        lower_bound_ = bound;
        for (const auto& network: networks_)
            if (network->restricted())
                return;
        if (problem().tardiness) {
            algorithm_formatter_.update_total_tardiness_bound(lower_bound_, "lagrangian dual");
        } else {
            algorithm_formatter_.update_total_flow_time_bound(lower_bound_, "lagrangian dual");
        }
    }

    const Instance& instance_;

    const LagrangianDualParameters& parameters_;

    LagrangianDualOutput& output_;

    AlgorithmFormatter& algorithm_formatter_;

    bool single_machine_ = true;

    /** Network of each group of identical machines. */
    std::vector<std::shared_ptr<time_indexed::Network>> networks_;

    /** Machines of each group. */
    std::vector<std::vector<MachineId>> machine_ids_;

    /** Rejection cost of each job of the problems. */
    std::vector<Time> rejection_costs_;

    /** See lengths(). */
    std::vector<double> lengths_;

    Time upper_bound_ = 0;

    /** Lower bound of the instance (integer). */
    Time lower_bound_ = std::numeric_limits<Time>::min();

    /** Upper bound at the last reduction. */
    Time reduction_upper_bound_ = std::numeric_limits<Time>::max();

};

}
}
