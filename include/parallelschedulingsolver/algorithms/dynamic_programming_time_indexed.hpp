/**
 * Dynamic programming on the time-indexed network
 *
 * Single machine scheduling problems 1 | s_ij | sum w_j T_j and
 * 1 | s_ij | sum w_j C_j without release dates. A schedule without idle time
 * is a path in a time-expanded network, where a node v_it is the completion
 * of job i at time t. The constraints "each job is processed exactly once"
 * are relaxed with Lagrangian multipliers, except for a set of jobs M with
 * state-space modifiers, and the relaxation is solved by dynamic
 * programming, with the constraint that a job is not processed twice in
 * three successive positions (relaxation (LR2) of Tanaka and Araki, 2013).
 * With M containing all the jobs, the dynamic programming is exact.
 *
 * One call of the algorithm performs, in this order, on the network given
 * as input (or the initial network of the instance, with M empty):
 * - the restriction of the completion times of some jobs (branching);
 * - the generation of the network with more jobs in M, from the given
 *   network;
 * - the reduction of the network with a cutoff (and constraint
 *   propagation);
 * - the computation of the labels, of the lower bound and of the shortest
 *   path.
 * The network is part of the output, so that the next call can start from
 * it, as in the successive sublimation dynamic programming algorithm.
 *
 * S. Tanaka, M. Araki (2013). An exact algorithm for the single-machine total
 * weighted tardiness problem with sequence-dependent setup times. Computers
 * & Operations Research, 40(1), 344-352.
 *
 * S. Tanaka, S. Fujikuma, M. Araki (2009). An exact algorithm for
 * single-machine scheduling without machine idle time. Journal of
 * Scheduling, 12, 575-593.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"
#include "parallelschedulingsolver/algorithms/time_indexed_network.hpp"

namespace parallelschedulingsolver
{

struct DynamicProgrammingTimeIndexedParameters: Parameters
{
    /**
     * Network to start from. If null, the initial network of the instance (M
     * empty). The network is modified by the restrictions, the reduction and
     * the computation of the labels: pass a copy to keep it.
     */
    std::shared_ptr<time_indexed::Network> network = nullptr;

    /**
     * Lagrangian multipliers of the jobs of the problem of the network; empty:
     * all 0.
     */
    std::vector<double> multipliers;

    /**
     * Restrictions of the completion times of jobs of the problem: for each
     * (job, earliest, latest), only the states of the job completed in
     * [earliest, latest] are kept.
     */
    struct CompletionTimeWindow
    {
        JobId job_id;
        Time earliest;
        Time latest;
    };
    std::vector<CompletionTimeWindow> completion_time_windows;

    /**
     * Jobs of the problem added to M: the network is generated from the given
     * network, in the given direction. If 'all_jobs_in_modifiers', all the
     * jobs are added.
     */
    std::vector<JobId> new_modifier_jobs;

    /** Add all the jobs to M: the dynamic programming is exact. */
    bool all_jobs_in_modifiers = false;

    /**
     * Direction of the generation, and of the labels of the lower bound and
     * of the shortest path.
     */
    time_indexed::Direction direction = time_indexed::Direction::Forward;

    /**
     * Cutoff: the generation and the reduction remove the states through
     * which all the paths have a length larger than the cutoff. With integer
     * costs, a cutoff of UB - 1 keeps all the schedules better than UB.
     */
    double cutoff = time_indexed::INF;

    /** Reduce the network with the cutoff (Section 3.2.1). */
    bool reduction = false;

    /** Reduce the network by constraint propagation (Section 3.2.2). */
    bool constraint_propagation = false;

    /**
     * Compute the labels, the lower bound and the shortest path at the end;
     * if false, the network is only modified (restriction, generation,
     * reduction), and the output has no bound nor path.
     */
    bool compute_bound = true;

    /** Also compute the labels of the other direction at the end. */
    bool both_directions_labels = false;

    /** Maximum memory of the networks, in MB, for the generation. */
    double maximum_memory = 2048;

    /**
     * Remove the zero cost jobs (Section 5.1) when building the problem (if
     * no network is given).
     */
    bool remove_zero_cost_jobs = true;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Input network: " << (network != nullptr) << std::endl
            << std::setw(width) << std::left << "Number of new modifiers: " << new_modifier_jobs.size() << std::endl
            << std::setw(width) << std::left << "All jobs in modifiers: " << all_jobs_in_modifiers << std::endl
            << std::setw(width) << std::left << "Cutoff: " << cutoff << std::endl
            << std::setw(width) << std::left << "Reduction: " << reduction << std::endl
            << std::setw(width) << std::left << "Constraint propagation: " << constraint_propagation << std::endl
            << std::setw(width) << std::left << "Maximum memory (MB): " << maximum_memory << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"InputNetwork", (network != nullptr)},
                {"NumberOfNewModifiers", new_modifier_jobs.size()},
                {"AllJobsInModifiers", all_jobs_in_modifiers},
                {"Reduction", reduction},
                {"ConstraintPropagation", constraint_propagation},
                {"MaximumMemory", maximum_memory}});
        return json;
    }
};

struct DynamicProgrammingTimeIndexedOutput: Output
{
    DynamicProgrammingTimeIndexedOutput(
            const Instance& instance):
        Output(instance) { }

    /** Network at the end of the algorithm. */
    std::shared_ptr<time_indexed::Network> network = nullptr;

    /**
     * Length of the shortest path plus the sum of the multipliers: a lower
     * bound of the cost of the schedules of the network; INF if the network
     * is empty.
     */
    double lagrangian_bound = time_indexed::INF;

    /** Jobs of the problem of the shortest path. */
    std::vector<JobId> path;

    /** True if the generation stopped because of the memory limit. */
    bool memory_out = false;

    /** Number of states of the network at the end. */
    Counter number_of_states = 0;

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Lagrangian bound: " << lagrangian_bound << std::endl
            << std::setw(width) << std::left << "Memory out: " << memory_out << std::endl
            << std::setw(width) << std::left << "Number of states: " << number_of_states << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"LagrangianBound", lagrangian_bound},
                {"MemoryOut", memory_out},
                {"NumberOfStates", number_of_states},
                });
        return json;
    }
};

/**
 * The bound of the output is set when the network isn't restricted: it is
 * then the smallest of the ceiling of the Lagrangian bound and of the cutoff
 * of the network plus one (the costs are integers).
 */
const DynamicProgrammingTimeIndexedOutput dynamic_programming_time_indexed(
        const Instance& instance,
        const DynamicProgrammingTimeIndexedParameters& parameters = {});

}
