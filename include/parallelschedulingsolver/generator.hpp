#pragma once

#include "parallelschedulingsolver/instance.hpp"

#include <random>

namespace parallelschedulingsolver
{

struct GenerateInput
{
    Objective objective = Objective::Makespan;

    MachineId number_of_machines = 2;

    JobId number_of_jobs_per_machine = 5;

    /**
     * If true, each job has the same processing time on all machines;
     * otherwise, the processing time of a job on each machine is drawn
     * independently (unrelated machines).
     */
    bool identical_machines = false;

    /** If true, all jobs have the same processing time. */
    bool equal_processing_times = false;

    /** Processing times are drawn in [1, processing_times_range]. */
    Time processing_times_range = 100;

    /** Weights are drawn in [1, weights_range]. */
    Time weights_range = 1;

    /**
     * Release dates are drawn in [0, release_dates_factor * P], where P is the
     * sum of the mean processing times of the jobs divided by the number of
     * machines. With 0, all release dates are 0.
     */
    double release_dates_factor = 0;

    /**
     * Due dates are drawn in
     * [P (1 - TF - RDD / 2), P (1 - TF + RDD / 2)] (Potts and Van Wassenhove,
     * 1985), where TF is the tardiness factor and RDD the relative range of
     * due dates. Negative values are set to 0.
     */
    double tardiness_factor = 0.5;

    double due_dates_range = 0.6;
};

/**
 * Generate an instance.
 *
 * Random numbers are drawn with a portable method, so the same seed
 * generates the same instance on all platforms.
 */
Instance generate(
        const GenerateInput& input,
        std::mt19937_64& generator);

}
