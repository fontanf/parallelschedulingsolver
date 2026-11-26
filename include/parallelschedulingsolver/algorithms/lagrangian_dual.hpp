/**
 * Common parameters and output of the algorithms solving the Lagrangian dual
 * of the time-indexed formulation: Lagrangian relaxation (conjugate
 * subgradient) and column generation.
 *
 * The constraints "each job is processed exactly once (or rejected)" are
 * relaxed with Lagrangian multipliers, and the relaxation is solved by the
 * time-indexed dynamic programming (relaxation (LR2) on the network, with the
 * state-space modifiers of the network). The algorithms search for the
 * multipliers giving the best lower bound.
 *
 * On parallel machines (identical or unrelated), the relaxation decomposes
 * by machine: the network of a machine is the one of its subproblem
 * (time_indexed::build_machine_problem), in which all the jobs are optional,
 * and the identical machines share a network.
 *
 * Single or parallel machines, total (weighted) tardiness or total
 * (weighted) completion time, without release dates, with or without
 * sequence-dependent setup times and rejection costs.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"
#include "parallelschedulingsolver/algorithms/time_indexed_network.hpp"

#include <functional>

namespace parallelschedulingsolver
{

/** Information given to the callback at each evaluation of the relaxation. */
struct LagrangianDualIteration
{
    /** Number of evaluations so far, including this one. */
    Counter iteration;

    /** Multipliers of the evaluation. */
    const std::vector<double>& multipliers;

    /** Lower bound of the relaxation for these multipliers. */
    double lagrangian_bound;

    /**
     * Shortest path of each network (see LagrangianDualOutput::networks):
     * jobs of the problem of the network. On a single machine, a single path.
     */
    const std::vector<std::vector<JobId>>& paths;

    /** True if the paths are a schedule. */
    bool feasible;

    /** True if the best lower bound is improved. */
    bool improved;

    /** Upper bound before the callback. */
    Time upper_bound;
};

/**
 * Callback called at each evaluation of the relaxation, before the network
 * reduction; it returns an upper bound (e.g. from a Lagrangian heuristic),
 * which replaces the upper bound of the algorithm if it is smaller.
 */
using LagrangianDualCallback = std::function<Time(const LagrangianDualIteration&)>;

struct LagrangianDualParameters: Parameters
{
    /**
     * Single machine only: network of the relaxation. If null, the initial
     * network of the instance (no state-space modifiers). It is reduced
     * during the algorithm.
     */
    std::shared_ptr<time_indexed::Network> network = nullptr;

    /** Initial multipliers; empty: all 0. */
    std::vector<double> initial_multipliers;

    /**
     * Upper bound: the value of a known schedule. If negative, the best value
     * of the schedules of all the jobs in the order of the instance, in the
     * order of the weighted shortest processing times and in the order of
     * the earliest due dates.
     */
    Time upper_bound = -1;

    /**
     * Reduce the network with the upper bound (minus one) each time the best
     * lower bound or the upper bound is improved.
     */
    bool reduction = true;

    /** Constraint propagation after each reduction. */
    bool constraint_propagation = true;

    /** Maximum number of evaluations of the relaxation. */
    Counter maximum_number_of_iterations = 10000;

    /** Callback called at each evaluation. */
    LagrangianDualCallback iteration_callback = nullptr;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Input network: " << (network != nullptr) << std::endl
            << std::setw(width) << std::left << "Upper bound: " << upper_bound << std::endl
            << std::setw(width) << std::left << "Reduction: " << reduction << std::endl
            << std::setw(width) << std::left << "Constraint propagation: " << constraint_propagation << std::endl
            << std::setw(width) << std::left << "Max. number of iterations: " << maximum_number_of_iterations << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"InputNetwork", (network != nullptr)},
                {"UpperBound", upper_bound},
                {"Reduction", reduction},
                {"ConstraintPropagation", constraint_propagation},
                {"MaximumNumberOfIterations", maximum_number_of_iterations}});
        return json;
    }
};

struct LagrangianDualOutput: Output
{
    LagrangianDualOutput(
            const Instance& instance):
        Output(instance) { }

    /**
     * Networks at the end of the algorithm: on a single machine, the network
     * of the instance; on parallel machines, the network of each group of
     * identical machines.
     */
    std::vector<std::shared_ptr<time_indexed::Network>> networks;

    /** Machines of each network. */
    std::vector<std::vector<MachineId>> network_machine_ids;

    /** Multipliers of the best lower bound. */
    std::vector<double> multipliers;

    /** Best lower bound of the relaxation; -INF if none has been computed. */
    double lagrangian_bound = -time_indexed::INF;

    /** Upper bound at the end of the algorithm. */
    Time upper_bound = -1;

    /** Number of evaluations of the relaxation. */
    Counter number_of_iterations = 0;

    /** Total number of states of the networks. */
    Counter number_of_states() const
    {
        Counter number_of_states = 0;
        for (const auto& network: networks)
            number_of_states += network->number_of_states();
        return number_of_states;
    }

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Lagrangian bound: " << lagrangian_bound << std::endl
            << std::setw(width) << std::left << "Number of iterations: " << number_of_iterations << std::endl
            << std::setw(width) << std::left << "Number of networks: " << networks.size() << std::endl
            << std::setw(width) << std::left << "Number of states: " << number_of_states() << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"LagrangianBound", lagrangian_bound},
                {"NumberOfIterations", number_of_iterations},
                {"NumberOfStates", number_of_states()},
                });
        return json;
    }
};

}
