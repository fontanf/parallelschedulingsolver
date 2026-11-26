/**
 * Tree search on the time-indexed network, with the relaxation refinement at
 * each node: stage 2 of the successive sublimation dynamic programming
 * algorithm (Tanaka et al., 2009; Tanaka and Araki, 2013).
 *
 * With fixed Lagrangian multipliers, the relaxation refinement is applied
 * with a tentative upper bound as bound to prove, increased from the lower
 * bound up to the upper bound (iterative deepening on the bound to prove).
 * If the memory isn't sufficient, the completion times of the two jobs with
 * the most states are split into two intervals (interval branching), and the
 * relaxation refinement is applied to each of the four restricted networks,
 * recursively.
 *
 * Single machine, total (weighted) tardiness or total (weighted) completion
 * time, without release dates, with or without sequence-dependent setup
 * times and rejection costs.
 */

#pragma once

#include "parallelschedulingsolver/algorithms/relaxation_refinement.hpp"

namespace parallelschedulingsolver
{

struct TreeSearchIterativeRefinementParameters: Parameters
{
    /**
     * Network of the root. If null, the initial network of the instance. If
     * the multipliers are given, it must be reduced with the upper bound.
     */
    std::shared_ptr<time_indexed::Network> network = nullptr;

    /**
     * Multipliers. If empty, those of the Lagrangian dual of the network,
     * and the network is then reduced with the upper bound.
     */
    std::vector<double> multipliers;

    /** Lagrangian dual by column generation, else by Lagrangian relaxation. */
    bool column_generation = true;

    /** Callback of the Lagrangian dual. */
    LagrangianDualCallback lagrangian_dual_callback = nullptr;

    /**
     * Upper bound: the value of a known schedule. If negative, the one of
     * the relaxation refinement.
     */
    Time upper_bound = -1;

    /**
     * Lower bound of the relaxation of the root network, the start of the
     * tentative upper bounds; if -INF, computed from the network.
     */
    double lower_bound = -time_indexed::INF;

    /** Increment of the tentative upper bound, relative to the upper bound. */
    double tentative_upper_bound_increment = 0.01;

    /** Branching on the completion times when the memory is not sufficient. */
    bool interval_branching = true;

    /**
     * Depth of the interval branching from the start (for testing): the
     * relaxation refinement isn't applied at the nodes of smaller depth.
     */
    Counter minimum_branching_depth = 0;

    /** Constraint propagation after each network generation. */
    bool constraint_propagation = true;

    /** Maximum memory of the networks (MB). */
    double maximum_memory = 2048;

    /** Callback called at each step of the relaxation refinements. */
    RelaxationRefinementCallback step_callback = nullptr;

    /**
     * Callback called each time a tentative upper bound is proved, with the
     * new lower bound.
     */
    std::function<void(Time)> lower_bound_callback = nullptr;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Input network: " << (network != nullptr) << std::endl
            << std::setw(width) << std::left << "Input multipliers: " << !multipliers.empty() << std::endl
            << std::setw(width) << std::left << "Column generation: " << column_generation << std::endl
            << std::setw(width) << std::left << "Upper bound: " << upper_bound << std::endl
            << std::setw(width) << std::left << "Tentative UB increment: " << tentative_upper_bound_increment << std::endl
            << std::setw(width) << std::left << "Interval branching: " << interval_branching << std::endl
            << std::setw(width) << std::left << "Minimum branching depth: " << minimum_branching_depth << std::endl
            << std::setw(width) << std::left << "Constraint propagation: " << constraint_propagation << std::endl
            << std::setw(width) << std::left << "Maximum memory (MB): " << maximum_memory << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"InputNetwork", (network != nullptr)},
                {"InputMultipliers", !multipliers.empty()},
                {"ColumnGeneration", column_generation},
                {"UpperBound", upper_bound},
                {"TentativeUpperBoundIncrement", tentative_upper_bound_increment},
                {"IntervalBranching", interval_branching},
                {"MinimumBranchingDepth", minimum_branching_depth},
                {"ConstraintPropagation", constraint_propagation},
                {"MaximumMemory", maximum_memory}});
        return json;
    }
};

struct TreeSearchIterativeRefinementOutput: Output
{
    TreeSearchIterativeRefinementOutput(
            const Instance& instance):
        Output(instance) { }

    /** Multipliers. */
    std::vector<double> multipliers;

    /** Upper bound at the end of the algorithm. */
    Time upper_bound = -1;

    /** Lower bound of the instance at the end of the algorithm. */
    Time lower_bound = 0;

    /** Number of calls to the relaxation refinement. */
    Counter number_of_refinements = 0;

    /** Largest number of jobs with state-space modifiers. */
    JobId maximum_number_of_modifiers = 0;

    /** Largest number of states of a network. */
    Counter maximum_number_of_states = 0;

    /** Largest depth of the interval branching. */
    Counter maximum_branching_depth = 0;

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Number of refinements: " << number_of_refinements << std::endl
            << std::setw(width) << std::left << "Max. number of modifiers: " << maximum_number_of_modifiers << std::endl
            << std::setw(width) << std::left << "Max. number of states: " << maximum_number_of_states << std::endl
            << std::setw(width) << std::left << "Max. branching depth: " << maximum_branching_depth << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"NumberOfRefinements", number_of_refinements},
                {"MaximumNumberOfModifiers", maximum_number_of_modifiers},
                {"MaximumNumberOfStates", maximum_number_of_states},
                {"MaximumBranchingDepth", maximum_branching_depth},
                });
        return json;
    }
};

const TreeSearchIterativeRefinementOutput tree_search_iterative_refinement(
        const Instance& instance,
        const TreeSearchIterativeRefinementParameters& parameters = {});

}
