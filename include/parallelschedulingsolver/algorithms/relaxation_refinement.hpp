/**
 * Relaxation refinement on the time-indexed network: with fixed Lagrangian
 * multipliers, the relaxation (LR2) is refined by adding jobs to the set M of
 * the state-space modifiers, until the bound to prove is proved or an early
 * termination criterion is met. It is the subprocedure of stage 2 of the
 * successive sublimation dynamic programming algorithm (Tanaka et al., 2009;
 * Tanaka and Araki, 2013).
 *
 * At each step, at most three jobs are added to M, and the network of the new
 * M is generated from the previous one by forward or backward dynamic
 * programming in turns, without the states whose paths are longer than the
 * cutoff (the bound to prove minus one).
 *
 * The multipliers and the network are an input (e.g. of a parent node of a
 * tree search); if the multipliers aren't given, they are those of the
 * Lagrangian dual (column generation or Lagrangian relaxation).
 *
 * Single machine, total (weighted) tardiness or total (weighted) completion
 * time, without release dates, with or without sequence-dependent setup
 * times and rejection costs.
 */

#pragma once

#include "parallelschedulingsolver/algorithms/lagrangian_dual.hpp"

namespace parallelschedulingsolver
{

/** Information given to the callback at each refinement step. */
struct RelaxationRefinementStep
{
    /** Number of steps so far, including this one. */
    Counter step;

    /** Network of the step. */
    const time_indexed::Network& network;

    /** Lower bound of the relaxation of the network. */
    double lagrangian_bound;

    /** Jobs of the problem of the shortest path. */
    const std::vector<JobId>& path;

    /** Direction of the dynamic programming of the step. */
    time_indexed::Direction direction;

    /** True if the path is a schedule. */
    bool feasible;

    /** True if the best lower bound of the refinement is improved. */
    bool improved;

    /** Bound to prove of the step. */
    Time bound_to_prove;
};

/**
 * Callback called at each refinement step; it returns an upper bound (e.g.
 * from a Lagrangian heuristic), which replaces the upper bound of the
 * algorithm if it is smaller.
 */
using RelaxationRefinementCallback = std::function<Time(const RelaxationRefinementStep&)>;

enum class RelaxationRefinementStatus
{
    /** No schedule has a value smaller than the bound to prove. */
    BoundProved,

    /** The memory limit is reached. */
    MemoryOut,

    /** The time limit is reached. */
    TimeOut,

    /** The maximum number of steps is reached. */
    MaximumNumberOfSteps,

    /** The bound improvement over the last steps is too small. */
    TailingOff,
};

std::ostream& operator<<(
        std::ostream& os,
        RelaxationRefinementStatus status);

struct RelaxationRefinementParameters: Parameters
{
    /**
     * Network of the relaxation, with or without state-space modifiers. If
     * null, the initial network of the instance. It isn't modified: the
     * networks of the refinement are new ones.
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
     * Upper bound: the value of a known schedule. If negative, the best of
     * the schedules of all the jobs in the orders of the instance, of the
     * weighted shortest processing times and of the earliest due dates (or
     * the upper bound of the Lagrangian dual).
     */
    Time upper_bound = -1;

    /**
     * Bound to prove: the refinement stops when no schedule has a value
     * smaller than it (or than the upper bound, if smaller). If negative,
     * the upper bound (the optimality of the upper bound).
     */
    Time bound_to_prove = -1;

    /** Constraint propagation after each network generation. */
    bool constraint_propagation = true;

    /** Maximum memory of the networks (MB). */
    double maximum_memory = 2048;

    /** Maximum number of refinement steps; -1: no limit. */
    Counter maximum_number_of_steps = -1;

    /**
     * Tailing-off: stop if, over the last 'tailing_off_number_of_steps'
     * steps, the best lower bound improved by less than
     * 'tailing_off_minimum_improvement' times the gap with the bound to
     * prove at the start of these steps. 0: no tailing-off.
     */
    Counter tailing_off_number_of_steps = 0;

    double tailing_off_minimum_improvement = 0.01;

    /** Callback called at each refinement step. */
    RelaxationRefinementCallback step_callback = nullptr;


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
            << std::setw(width) << std::left << "Bound to prove: " << bound_to_prove << std::endl
            << std::setw(width) << std::left << "Constraint propagation: " << constraint_propagation << std::endl
            << std::setw(width) << std::left << "Maximum memory (MB): " << maximum_memory << std::endl
            << std::setw(width) << std::left << "Max. number of steps: " << maximum_number_of_steps << std::endl
            << std::setw(width) << std::left << "Tailing-off number of steps: " << tailing_off_number_of_steps << std::endl
            << std::setw(width) << std::left << "Tailing-off min. improvement: " << tailing_off_minimum_improvement << std::endl
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
                {"BoundToProve", bound_to_prove},
                {"ConstraintPropagation", constraint_propagation},
                {"MaximumMemory", maximum_memory},
                {"MaximumNumberOfSteps", maximum_number_of_steps},
                {"TailingOffNumberOfSteps", tailing_off_number_of_steps},
                {"TailingOffMinimumImprovement", tailing_off_minimum_improvement}});
        return json;
    }
};

struct RelaxationRefinementOutput: Output
{
    RelaxationRefinementOutput(
            const Instance& instance):
        Output(instance) { }

    RelaxationRefinementStatus status = RelaxationRefinementStatus::TimeOut;

    /**
     * Network at the end of the algorithm: the input network after the
     * Lagrangian dual and its reduction, or the network of the last step.
     */
    std::shared_ptr<time_indexed::Network> network = nullptr;

    /** Multipliers. */
    std::vector<double> multipliers;

    /** Best lower bound of the relaxations of the networks. */
    double lagrangian_bound = -time_indexed::INF;

    /** Upper bound at the end of the algorithm. */
    Time upper_bound = -1;

    /** Bound to prove at the end of the algorithm (at most the upper bound). */
    Time bound_to_prove = -1;

    /** Number of refinement steps. */
    Counter number_of_steps = 0;

    /** Number of jobs in the state-space modifiers of the last network. */
    JobId number_of_modifiers = 0;

    /** Largest number of states of a network. */
    Counter maximum_number_of_states = 0;

    /** Number of evaluations of the Lagrangian dual. */
    Counter number_of_lagrangian_dual_iterations = 0;

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Status: " << status << std::endl
            << std::setw(width) << std::left << "Lagrangian bound: " << lagrangian_bound << std::endl
            << std::setw(width) << std::left << "Bound to prove: " << bound_to_prove << std::endl
            << std::setw(width) << std::left << "Lagrangian dual iterations: " << number_of_lagrangian_dual_iterations << std::endl
            << std::setw(width) << std::left << "Number of steps: " << number_of_steps << std::endl
            << std::setw(width) << std::left << "Number of modifiers: " << number_of_modifiers << std::endl
            << std::setw(width) << std::left << "Max. number of states: " << maximum_number_of_states << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        std::stringstream ss;
        ss << status;
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"Status", ss.str()},
                {"LagrangianBound", lagrangian_bound},
                {"BoundToProve", bound_to_prove},
                {"NumberOfLagrangianDualIterations", number_of_lagrangian_dual_iterations},
                {"NumberOfSteps", number_of_steps},
                {"NumberOfModifiers", number_of_modifiers},
                {"MaximumNumberOfStates", maximum_number_of_states},
                });
        return json;
    }
};

const RelaxationRefinementOutput relaxation_refinement(
        const Instance& instance,
        const RelaxationRefinementParameters& parameters = {});

}
