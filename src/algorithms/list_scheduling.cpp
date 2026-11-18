#include "parallelschedulingsolver/algorithms/list_scheduling.hpp"

using namespace parallelschedulingsolver;

Time parallelschedulingsolver::minimum_processing_time(
        const Instance& instance,
        JobId job_id)
{
    const Job& job = instance.job(job_id);
    Time p = std::numeric_limits<Time>::max();
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        p = (std::min)(p, job.processing_times[machine_id]);
    }
    return p;
}

Solution parallelschedulingsolver::list_scheduling(
        const Instance& instance,
        const std::vector<JobId>& job_ids)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);

    std::vector<Time> machine_times(instance.number_of_machines(), 0);
    for (JobId job_id: job_ids) {
        const Job& job = instance.job(job_id);
        MachineId best_machine_id = -1;
        Time best_start = 0;
        Time best_completion = 0;
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            Time start = (std::max)(machine_times[machine_id], job.release_date);
            Time completion = start + job.processing_times[machine_id];
            if (best_machine_id == -1 || completion < best_completion) {
                best_machine_id = machine_id;
                best_start = start;
                best_completion = completion;
            }
        }
        solution_builder.append_job_to_machine(best_machine_id, job_id, best_start);
        machine_times[best_machine_id] = best_completion;
    }

    return solution_builder.build();
}
