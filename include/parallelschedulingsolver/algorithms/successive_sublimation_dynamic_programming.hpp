/**
 * Successive sublimation dynamic programming (SSDP)
 *
 * Exact algorithm of Tanaka and Araki (2013) for the single machine total
 * weighted tardiness problem with sequence-dependent setup times
 * 1 | s_ij | sum w_j T_j, an extension of the algorithms of Tanaka, Fujikuma
 * and Araki (2009) and Tanaka and Fujikuma (2012) for the problem without
 * setup times.
 *
 * The problem is a shortest path problem on a time-expanded network, where
 * each job must be visited exactly once. These constraints are relaxed by
 * Lagrangian relaxation, and the relaxation is solved by dynamic programming,
 * with the constraint that a job is not visited twice in three successive
 * nodes.
 *
 * - Stage 1: the Lagrangian multipliers are adjusted by column generation
 *   (or the conjugate subgradient algorithm).
 * - Stage 2: the constraints are successively recovered for subsets of jobs
 *   (state-space modifiers), until the gap between the lower and upper
 *   bounds vanishes, with a tentative upper bound.
 * - The dynamic programming states are eliminated with the upper bound
 *   (network reduction) and by constraint propagation. When the memory is
 *   not sufficient, the algorithm branches on the completion times of jobs.
 * - Upper bounds: Lagrangian heuristic and local search.
 *
 * Also solves 1 | s_ij | sum w_j C_j, and the problems without setup times.
 * Single machine instances without release dates only.
 *
 * S. Tanaka, M. Araki (2013). An exact algorithm for the single-machine total
 * weighted tardiness problem with sequence-dependent setup times. Computers
 * & Operations Research, 40(1), 344-352.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

struct SuccessiveSublimationDynamicProgrammingParameters: Parameters
{
    /**
     * Adjust the multipliers of stage 1 by column generation (Section 5.2 of
     * the paper) instead of the conjugate subgradient algorithm. Without
     * HiGHS, the conjugate subgradient algorithm is always used.
     */
    bool column_generation = true;

    /** Network reduction by constraint propagation (Section 3.2.2). */
    bool constraint_propagation = true;

    /** Branching on the completion times when the memory is not sufficient (Section 5.4). */
    bool interval_branching = true;

    /**
     * Depth of the interval branching applied from the start of stage 2,
     * even if the memory is sufficient (for testing).
     */
    Counter minimum_branching_depth = 0;

    /** Maximum memory for the networks, in MB. */
    double maximum_memory = 2048;

    /**
     * Number of restarts of the local search for the initial upper bound,
     * started from the best greedy solution; 0: no local search.
     */
    Counter initial_local_search_number_of_restarts = 1;

    /** Improve the solutions of the Lagrangian heuristic by local search. */
    bool lagrangian_heuristic_local_search = true;

    /**
     * Maximum number of perturbations without improvement of the local
     * search applied to the solutions of the Lagrangian heuristic.
     */
    Counter local_search_number_of_perturbations = 100;

    /*
     * Parameters of the conjugate subgradient algorithm (Tanaka and Fujikuma,
     * 2012, Section 7.1).
     */

    /** Initial step size parameter. */
    double initial_step_size = 1.2;

    /** Iterations for the termination criterion; if negative, n. */
    Counter termination_iterations = -1;

    /** Iterations without improvement before decreasing the step size. */
    Counter shrink_iterations = 2;

    /** Relative improvement for the termination criterion. */
    double termination_epsilon = 0.02;

    /** Factor of the step size when it is decreased. */
    double shrink_factor = 0.95;

    /** Factor of the step size when it is increased. */
    double expand_factor = 1.1;

    /** Maximum number of iterations of stage 1. */
    Counter maximum_number_of_stage_1_iterations = 10000;

    /*
     * Stage 2.
     */

    /** Increment of the tentative upper bound, relative to the upper bound. */
    double tentative_upper_bound_increment = 0.01;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Column generation: " << column_generation << std::endl
            << std::setw(width) << std::left << "Constraint propagation: " << constraint_propagation << std::endl
            << std::setw(width) << std::left << "Interval branching: " << interval_branching << std::endl
            << std::setw(width) << std::left << "Maximum memory (MB): " << maximum_memory << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"ColumnGeneration", column_generation},
                {"ConstraintPropagation", constraint_propagation},
                {"IntervalBranching", interval_branching},
                {"MaximumMemory", maximum_memory}});
        return json;
    }
};

struct SuccessiveSublimationDynamicProgrammingOutput: Output
{
    SuccessiveSublimationDynamicProgrammingOutput(
            const Instance& instance):
        Output(instance) { }

    /** Number of jobs removed in preprocessing (zero cost jobs). */
    JobId number_of_removed_jobs = 0;

    /** Lower bound at the end of stage 1. */
    double stage_1_lower_bound = 0;

    /** Number of iterations of stage 1. */
    Counter number_of_stage_1_iterations = 0;

    /** Number of calls to the subprocedure of stage 2. */
    Counter number_of_subprocedures = 0;

    /** Largest number of jobs with state-space modifiers. */
    JobId maximum_number_of_modifiers = 0;

    /** Largest number of states of a network. */
    Counter maximum_number_of_states = 0;

    /** Largest depth of the interval branching. */
    Counter maximum_branching_depth = 0;

    /** Stage in which the algorithm terminated (0: preprocessing). */
    int stage = 0;

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Number of removed jobs: " << number_of_removed_jobs << std::endl
            << std::setw(width) << std::left << "Stage 1 lower bound: " << stage_1_lower_bound << std::endl
            << std::setw(width) << std::left << "Stage 1 iterations: " << number_of_stage_1_iterations << std::endl
            << std::setw(width) << std::left << "Number of subprocedures: " << number_of_subprocedures << std::endl
            << std::setw(width) << std::left << "Max. number of modifiers: " << maximum_number_of_modifiers << std::endl
            << std::setw(width) << std::left << "Max. number of states: " << maximum_number_of_states << std::endl
            << std::setw(width) << std::left << "Max. branching depth: " << maximum_branching_depth << std::endl
            << std::setw(width) << std::left << "Stage: " << stage << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"NumberOfRemovedJobs", number_of_removed_jobs},
                {"Stage1LowerBound", stage_1_lower_bound},
                {"NumberOfStage1Iterations", number_of_stage_1_iterations},
                {"NumberOfSubprocedures", number_of_subprocedures},
                {"MaximumNumberOfModifiers", maximum_number_of_modifiers},
                {"MaximumNumberOfStates", maximum_number_of_states},
                {"MaximumBranchingDepth", maximum_branching_depth},
                {"Stage", stage},
                });
        return json;
    }
};

const SuccessiveSublimationDynamicProgrammingOutput successive_sublimation_dynamic_programming(
        const Instance& instance,
        const SuccessiveSublimationDynamicProgrammingParameters& parameters = {});

}
