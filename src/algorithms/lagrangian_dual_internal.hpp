#pragma once

/**
 * Implementation shared by the Lagrangian relaxation and the column
 * generation: evaluation of the relaxation with the time-indexed dynamic
 * programming, update of the bounds, call of the iteration callback, and
 * reduction of the network.
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
        network_ = parameters.network;
        if (network_ == nullptr) {
            network_ = std::make_shared<time_indexed::Network>(
                    time_indexed::Network::initial_network(
                        time_indexed::build_problem(instance)));
        }
        output_.network = network_;
        const time_indexed::Problem& problem = network_->problem();

        // Upper bound.
        upper_bound_ = parameters.upper_bound;
        if (upper_bound_ < 0) {
            // Best of the orders of the instance, of the weighted shortest
            // processing times and of the earliest due dates, with all the
            // jobs.
            JobId n = problem.number_of_jobs;
            std::vector<JobId> identity(n);
            std::iota(identity.begin(), identity.end(), 0);
            std::vector<JobId> wspt = identity;
            std::stable_sort(wspt.begin(), wspt.end(), [&problem](JobId j1, JobId j2)
                    {
                        return problem.processing_times[j1] * problem.weights[j2]
                            < problem.processing_times[j2] * problem.weights[j1];
                    });
            std::vector<JobId> edd = identity;
            std::stable_sort(edd.begin(), edd.end(), [&problem](JobId j1, JobId j2)
                    {
                        return problem.due_dates[j1] < problem.due_dates[j2];
                    });
            upper_bound_ = std::numeric_limits<Time>::max();
            for (const std::vector<JobId>& sequence: {identity, wspt, edd}) {
                Solution solution = time_indexed::build_solution(instance, problem, sequence);
                Time value = (Time)solution.objective_value();
                if (value < upper_bound_) {
                    upper_bound_ = value;
                    algorithm_formatter_.update_solution(solution, "initial solution");
                }
            }
        }
        output_.upper_bound = upper_bound_;
    }

    const time_indexed::Problem& problem() const { return network_->problem(); }

    JobId number_of_jobs() const { return network_->problem().number_of_jobs; }

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
     * (INF if the network is empty), the path, and whether it is a schedule.
     */
    double evaluate(
            const std::vector<double>& multipliers,
            std::vector<JobId>& path,
            bool& feasible)
    {
        DynamicProgrammingTimeIndexedParameters dp_parameters;
        dp_parameters.verbosity_level = 0;
        dp_parameters.timer = parameters_.timer;
        dp_parameters.network = network_;
        dp_parameters.multipliers = multipliers;
        DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming_time_indexed(
                instance_,
                dp_parameters);
        path = dp_output.path;
        feasible = dp_output.solution.feasible();
        if (feasible)
            update_upper_bound(dp_output.solution, "relaxation");
        return dp_output.lagrangian_bound;
    }

    /**
     * Process the result of an evaluation: best lower bound and multipliers,
     * callback, and network reduction. Return true if the best lower bound
     * is improved.
     */
    bool process(
            const std::vector<double>& multipliers,
            double lagrangian_bound,
            const std::vector<JobId>& path,
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
                    + " states " + std::to_string(network_->number_of_states()));
        }

        if (parameters_.iteration_callback) {
            LagrangianDualIteration iteration{
                    output_.number_of_iterations,
                    multipliers,
                    lagrangian_bound,
                    path,
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
        // updated, with the forward labels of the current multipliers.
        if (parameters_.reduction
                && (improved || upper_bound_ < reduction_upper_bound_)) {
            reduction_upper_bound_ = upper_bound_;
            DynamicProgrammingTimeIndexedParameters dp_parameters;
            dp_parameters.verbosity_level = 0;
            dp_parameters.timer = parameters_.timer;
            dp_parameters.network = network_;
            dp_parameters.multipliers = multipliers;
            dp_parameters.cutoff = upper_bound_ - 1 + TOLERANCE;
            dp_parameters.reduction = true;
            dp_parameters.constraint_propagation = parameters_.constraint_propagation;
            dp_parameters.compute_bound = false;
            dynamic_programming_time_indexed(instance_, dp_parameters);
            if (network_->number_of_states() == 0 && network_->empty_path_length(multipliers) == time_indexed::INF) {
                // No schedule better than the upper bound.
                update_lower_bound(time_indexed::INF);
            }
        }
        return improved;
    }

    /** Network empty: no schedule better than the upper bound. */
    void network_empty() { update_lower_bound(time_indexed::INF); }

private:

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
     * all the schedules better than the upper bound are paths of the network
     * (if it isn't restricted).
     */
    void update_lower_bound(double lagrangian_bound)
    {
        Time bound = (lagrangian_bound == time_indexed::INF)?
            upper_bound_:
            (std::min)((Time)std::ceil(lagrangian_bound - TOLERANCE), upper_bound_);
        if (bound <= lower_bound_)
            return;
        lower_bound_ = bound;
        if (network_->restricted())
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

    std::shared_ptr<time_indexed::Network> network_;

    Time upper_bound_ = 0;

    /** Lower bound of the instance (integer). */
    Time lower_bound_ = std::numeric_limits<Time>::min();

    /** Upper bound at the last reduction. */
    Time reduction_upper_bound_ = std::numeric_limits<Time>::max();

};

}
}
