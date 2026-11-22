#pragma once

#include "optimizationtools/utils/output.hpp"
#include "optimizationtools/utils/common.hpp"
#include "optimizationtools/utils/utils.hpp"

#include <cstdint>
#include <vector>
#include <iostream>

namespace parallelschedulingsolver
{

using MachineId = int64_t;
using JobId = int64_t;
using Time = int64_t;
using Seed = int64_t;
using Counter = int64_t;

enum class Objective
{
    Makespan,
    TotalFlowTime,
    Throughput,
    TotalTardiness,
    MaximumLateness,
};

std::istream& operator>>(
        std::istream& in,
        Objective& objective);

std::ostream& operator<<(
        std::ostream& os,
        Objective objective);

inline optimizationtools::ObjectiveDirection objective_direction(
        Objective objective)
{
    switch (objective) {
    case Objective::Makespan:
        return optimizationtools::ObjectiveDirection::Minimize;
    case Objective::TotalFlowTime:
        return optimizationtools::ObjectiveDirection::Minimize;
    case Objective::TotalTardiness:
        return optimizationtools::ObjectiveDirection::Minimize;
    case Objective::MaximumLateness:
        return optimizationtools::ObjectiveDirection::Minimize;
    case Objective::Throughput:
        return optimizationtools::ObjectiveDirection::Maximize;
    }
    return optimizationtools::ObjectiveDirection::Minimize;
}

/**
 * Structure for a machine.
 */
struct Machine
{
    /**
     * Sequence-dependent setup times of the machine, empty if the instance has
     * no setup times.
     *
     * setup_times[job_id_1 + 1][job_id_2] is the setup time when job_id_2 is
     * processed right after job_id_1 on the machine; setup_times[0][job_id_2]
     * is the setup time when job_id_2 is the first job of the machine.
     */
    std::vector<std::vector<Time>> setup_times;
};

/**
 * Structure for a job.
 */
struct Job
{
    /** Processing-time on each machine. */
    std::vector<Time> processing_times;

    /** Release date. */
    Time release_date = 0;

    /** Due date. */
    Time due_date = -1;

    /** Weight. */
    Time weight = 1;

    /**
     * Rejection cost: the job may not be scheduled, and then its rejection
     * cost is added to the objective; -1 if the job must be scheduled.
     */
    Time rejection_cost = -1;
};

/**
 * Instance class.
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the objective. */
    Objective objective() const { return objective_; }

    /** Get the number of machines. */
    MachineId number_of_machines() const { return machines_.size(); }

    /** Get a machine. */
    const Machine& machine(MachineId machine_id) const { return machines_[machine_id]; }

    /** Get the number of jobs. */
    JobId number_of_jobs() const { return jobs_.size(); }

    /** Get a job. */
    const Job& job(JobId job_id) const { return jobs_[job_id]; }

    /** Return true if the instance has sequence-dependent setup times. */
    bool has_setup_times() const { return has_setup_times_; }

    /** Return true if some jobs can be rejected (have a rejection cost). */
    bool has_rejection_costs() const { return has_rejection_costs_; }

    /**
     * Get the setup time when job_id_2 is processed right after job_id_1 on a
     * machine; job_id_1 = -1 if job_id_2 is the first job of the machine.
     *
     * The setup of a job is performed right before it, and may be performed
     * before its release date; the job itself starts at or after its release
     * date.
     */
    Time setup_time(
            MachineId machine_id,
            JobId job_id_1,
            JobId job_id_2) const
    {
        if (!has_setup_times_)
            return 0;
        return machines_[machine_id].setup_times[job_id_1 + 1][job_id_2];
    }

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /** Write the instance to a file. */
    void write(
            const std::string& instance_path,
            const std::string& format) const;

private:

    /*
     * Private methods
     */

    /** Create an instance manually. */
    Instance() { }

    /*
     * Private attributes
     */

    /** Objective. */
    Objective objective_;

    /** Machines. */
    std::vector<Machine> machines_;

    /** Jobs. */
    std::vector<Job> jobs_;

    /** True if the instance has sequence-dependent setup times. */
    bool has_setup_times_ = false;

    /** True if some jobs can be rejected. */
    bool has_rejection_costs_ = false;

    friend class InstanceBuilder;

};

}
