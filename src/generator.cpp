#include "parallelschedulingsolver/generator.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

#include <cmath>

using namespace parallelschedulingsolver;

namespace
{

/**
 * Draw an integer uniformly in [a, b].
 *
 * std::uniform_int_distribution is implementation-defined, so it doesn't
 * generate the same numbers with all standard libraries. std::mt19937_64 is
 * fully specified; rejection sampling on its output is portable.
 */
Time uniform_int(
        std::mt19937_64& generator,
        Time a,
        Time b)
{
    uint64_t range = (uint64_t)(b - a) + 1;
    uint64_t limit = std::numeric_limits<uint64_t>::max()
        - std::numeric_limits<uint64_t>::max() % range;
    for (;;) {
        uint64_t value = generator();
        if (value < limit)
            return a + (Time)(value % range);
    }
}

}

Instance parallelschedulingsolver::generate(
        const GenerateInput& input,
        std::mt19937_64& generator)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(input.objective);
    instance_builder.set_number_of_machines(input.number_of_machines);
    JobId number_of_jobs = input.number_of_machines * input.number_of_jobs_per_machine;

    // Processing times and weights.
    Time equal_processing_time = uniform_int(generator, 1, input.processing_times_range);
    double mean_processing_time_sum = 0;
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
        instance_builder.add_job();
        Time job_processing_time = (input.equal_processing_times)?
            equal_processing_time:
            uniform_int(generator, 1, input.processing_times_range);
        Time processing_time_sum = 0;
        for (MachineId machine_id = 0;
                machine_id < input.number_of_machines;
                ++machine_id) {
            Time processing_time = (input.identical_machines || input.equal_processing_times)?
                job_processing_time:
                uniform_int(generator, 1, input.processing_times_range);
            instance_builder.set_job_processing_time(
                    job_id,
                    machine_id,
                    processing_time);
            processing_time_sum += processing_time;
        }
        mean_processing_time_sum += (double)processing_time_sum / input.number_of_machines;
        instance_builder.set_job_weight(job_id, uniform_int(generator, 1, input.weights_range));
    }

    // Release dates and due dates, relative to the expected makespan without
    // idle times.
    double p = mean_processing_time_sum / input.number_of_machines;
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
        if (input.release_dates_factor > 0) {
            Time release_date = uniform_int(
                    generator,
                    0,
                    (Time)std::floor(input.release_dates_factor * p));
            instance_builder.set_job_release_date(job_id, release_date);
        }
        Time due_date_min = (Time)std::floor((std::max)(0.0,
                    p * (1 - input.tardiness_factor - input.due_dates_range / 2)));
        Time due_date_max = (Time)std::floor((std::max)(0.0,
                    p * (1 - input.tardiness_factor + input.due_dates_range / 2)));
        instance_builder.set_job_due_date(
                job_id,
                uniform_int(generator, due_date_min, due_date_max));
    }

    return instance_builder.build();
}
