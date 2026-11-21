#include "parallelschedulingsolver/algorithms/local_search.hpp"

#include "parallelschedulingsolver/algorithm_formatter.hpp"
#include "parallelschedulingsolver/solution_builder.hpp"

#include "optimizationtools/utils/common.hpp"

#include <algorithm>
#include <numeric>
#include <sstream>

using namespace parallelschedulingsolver;

namespace
{

struct LocalSearchSolution
{
    /** For each machine, the sequence of its jobs. */
    std::vector<std::vector<JobId>> machines_jobs;

    Time objective = 0;
};

struct LocalSearchData
{
    LocalSearchSolution solution;

    // completion_times[machine_id][pos]: completion time of the first pos jobs
    // of machine machine_id in the current solution, i.e. the time at which
    // the setup of the job at position pos can start. Index 0 is 0.
    std::vector<std::vector<Time>> completion_times;

    // prefix_objective[machine_id][pos]: objective of the first pos jobs of
    // machine machine_id in the current solution.
    std::vector<std::vector<Time>> prefix_objective;
};

// Return the objective contribution of one job given its completion time.
inline Time job_contribution(
        const Instance& instance,
        JobId job_id,
        Time completion_time)
{
    const Job& job = instance.job(job_id);
    if (instance.objective() == Objective::TotalFlowTime) {
        return job.weight * (completion_time - job.release_date);
    } else {
        if (job.due_date < 0)
            return 0;
        return job.weight * (std::max)(Time(0), completion_time - job.due_date);
    }
}

// Last job of the first pos jobs of a sequence; -1 if pos = 0.
inline JobId last_job(
        const std::vector<JobId>& jobs,
        JobId pos)
{
    return (pos == 0)? -1: jobs[pos - 1];
}

// Schedule a job on a machine after job 'last_job_id' completing at time
// 'time' (-1 if it's the first job of the machine): after its setup, as early
// as possible after its release date. The setup may be performed before the
// release date. Add its contribution to 'objective'.
inline void advance(
        const Instance& instance,
        MachineId machine_id,
        JobId job_id,
        Time& time,
        Time& objective,
        JobId& last_job_id)
{
    const Job& job = instance.job(job_id);
    time = (std::max)(
            time + instance.setup_time(machine_id, last_job_id, job_id),
            job.release_date)
        + job.processing_times[machine_id];
    objective += job_contribution(instance, job_id, time);
    last_job_id = job_id;
}

// Schedule the jobs [first, last) on a machine after time 'time'. Return
// false as soon as 'objective' reaches 'bound'.
inline bool advance_block(
        const Instance& instance,
        MachineId machine_id,
        std::vector<JobId>::const_iterator first,
        std::vector<JobId>::const_iterator last,
        Time& time,
        Time& objective,
        JobId& last_job_id,
        Time bound)
{
    for (auto it = first; it != last; ++it) {
        advance(instance, machine_id, *it, time, objective, last_job_id);
        if (objective >= bound)
            return false;
    }
    return true;
}

// Objective of a machine whose jobs at positions pos, pos + 1, ... are the
// ones of the current solution, the previous jobs completing at time 'time'
// with objective 'objective', the last of them being 'last_job_id'.
//
// The objectives are regular, so, when the last job is the same as in the
// current solution (always the case without setup times):
// - if 'time' equals the completion time of the first pos jobs in the
//   current solution, the end of the schedule is the same as in the current
//   solution;
// - if it is larger, the objective of the end of the schedule is at least
//   the one in the current solution.
//
// Return 'bound' as soon as the objective is known to be at least 'bound'.
inline Time evaluate_suffix(
        const Instance& instance,
        const LocalSearchData& data,
        MachineId machine_id,
        JobId pos,
        Time time,
        Time objective,
        JobId last_job_id,
        Time bound)
{
    const std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
    const std::vector<Time>& completion_times = data.completion_times[machine_id];
    const std::vector<Time>& prefix_objective = data.prefix_objective[machine_id];
    JobId n = jobs.size();
    if (objective >= bound)
        return bound;
    for (; pos < n; ++pos) {
        if (!instance.has_setup_times()
                || last_job_id == last_job(jobs, pos)) {
            Time suffix_objective = prefix_objective[n] - prefix_objective[pos];
            if (time == completion_times[pos])
                return (objective + suffix_objective < bound)? objective + suffix_objective: bound;
            if (time > completion_times[pos]
                    && objective + suffix_objective >= bound) {
                return bound;
            }
        }
        advance(instance, machine_id, jobs[pos], time, objective, last_job_id);
        if (objective >= bound)
            return bound;
    }
    return objective;
}

// Recompute completion_times and prefix_objective of a machine from position
// pos, and the objective of the solution.
void update_machine(
        const Instance& instance,
        LocalSearchData& data,
        MachineId machine_id,
        JobId pos)
{
    const std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
    std::vector<Time>& completion_times = data.completion_times[machine_id];
    std::vector<Time>& prefix_objective = data.prefix_objective[machine_id];
    JobId n = jobs.size();
    completion_times.resize(n + 1);
    prefix_objective.resize(n + 1);
    completion_times[0] = 0;
    prefix_objective[0] = 0;
    for (JobId p = (std::max)(pos, (JobId)0); p < n; ++p) {
        Time time = completion_times[p];
        Time objective = prefix_objective[p];
        JobId last_job_id = last_job(jobs, p);
        advance(instance, machine_id, jobs[p], time, objective, last_job_id);
        completion_times[p + 1] = time;
        prefix_objective[p + 1] = objective;
    }
}

void update_objective(
        LocalSearchData& data)
{
    data.solution.objective = 0;
    for (const std::vector<Time>& prefix_objective: data.prefix_objective)
        data.solution.objective += prefix_objective.back();
}

void load_solution(
        const Instance& instance,
        LocalSearchData& data,
        const LocalSearchSolution& solution)
{
    data.solution = solution;
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        update_machine(instance, data, machine_id, 0);
    }
    update_objective(data);
}

Solution build_solution(
        const Instance& instance,
        const LocalSearchSolution& ls_solution)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        Time time = 0;
        JobId last_job_id = -1;
        for (JobId job_id: ls_solution.machines_jobs[machine_id]) {
            const Job& job = instance.job(job_id);
            Time start = (std::max)(
                    time + instance.setup_time(machine_id, last_job_id, job_id),
                    job.release_date);
            solution_builder.append_job_to_machine(machine_id, job_id, start);
            time = start + job.processing_times[machine_id];
            last_job_id = job_id;
        }
    }
    Solution solution = solution_builder.build();
    if (solution.objective_value() != ls_solution.objective) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": wrong objective; "
                "solution: " + std::to_string(solution.objective_value()) + "; "
                "ls_solution: " + std::to_string(ls_solution.objective) + ".");
    }
    return solution;
}

LocalSearchSolution from_solution(
        const Instance& instance,
        const Solution& solution)
{
    LocalSearchSolution ls_solution;
    ls_solution.machines_jobs.resize(instance.number_of_machines());
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        for (const auto& scheduled_job: solution.machine(machine_id).jobs)
            ls_solution.machines_jobs[machine_id].push_back(scheduled_job.job_id);
    }
    return ls_solution;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Neighborhoods ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

enum class Neighborhood
{
    // l-block insertion intra-machine.
    InsertIntra,
    // (l, l')-block swap intra-machine.
    SwapIntra,
    // l-block insertion inter-machine.
    InsertInter,
    // (l, l')-block swap inter-machine.
    SwapInter,
};

struct NeighborhoodParameters
{
    Neighborhood neighborhood;

    JobId size_1;

    JobId size_2;
};

// Check that the objective of the solution after a move is the expected one.
void check_objective(
        const LocalSearchData& data,
        Time objective)
{
    if (data.solution.objective != objective) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": wrong objective after move; "
                "data.solution.objective: " + std::to_string(data.solution.objective) + "; "
                "expected: " + std::to_string(objective) + ".");
    }
}

// ---- l-block insertion intra-machine ----

// Move the block of size 'size' starting at position pos_old of a machine so
// that it starts at position pos_new.
void shift_jobs(
        const Instance& instance,
        LocalSearchData& data,
        MachineId machine_id,
        JobId size,
        JobId pos_old,
        JobId pos_new)
{
    std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
    if (pos_new > pos_old) {
        std::rotate(
                jobs.begin() + pos_old,
                jobs.begin() + pos_old + size,
                jobs.begin() + pos_new + size);
    } else {
        std::rotate(
                jobs.begin() + pos_new,
                jobs.begin() + pos_old,
                jobs.begin() + pos_old + size);
    }
    update_machine(instance, data, machine_id, (std::min)(pos_old, pos_new));
    update_objective(data);
}

bool explore_insert_intra_neighborhood(
        const Instance& instance,
        LocalSearchData& data,
        JobId size)
{
    // Best move over the whole neighborhood.
    Time delta_best = 0;
    MachineId machine_id_best = -1;
    JobId pos_old_best = -1;
    JobId pos_new_best = -1;

    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        const std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
        const std::vector<Time>& completion_times = data.completion_times[machine_id];
        const std::vector<Time>& prefix_objective = data.prefix_objective[machine_id];
        JobId n = jobs.size();
        Time machine_objective = prefix_objective[n];

        for (JobId pos_old = 0; pos_old + size <= n; ++pos_old) {

            // Block moved backward (pos_new < pos_old):
            // [0, pos_new) + block + [pos_new, pos_old) + [pos_old + size, n).
            for (JobId pos_new = 0; pos_new < pos_old; ++pos_new) {
                Time bound = machine_objective + delta_best;
                if (prefix_objective[pos_new] >= bound)
                    break;
                Time time = completion_times[pos_new];
                Time objective = prefix_objective[pos_new];
                JobId last_job_id = last_job(jobs, pos_new);
                if (!advance_block(
                            instance, machine_id,
                            jobs.begin() + pos_old, jobs.begin() + pos_old + size,
                            time, objective, last_job_id, bound)) {
                    continue;
                }
                if (!advance_block(
                            instance, machine_id,
                            jobs.begin() + pos_new, jobs.begin() + pos_old,
                            time, objective, last_job_id, bound)) {
                    continue;
                }
                objective = evaluate_suffix(
                        instance, data, machine_id,
                        pos_old + size, time, objective, last_job_id, bound);
                if (objective >= bound)
                    continue;
                delta_best = objective - machine_objective;
                machine_id_best = machine_id;
                pos_old_best = pos_old;
                pos_new_best = pos_new;
            }

            // Block moved forward (pos_new > pos_old):
            // [0, pos_old) + [pos_old + size, pos_new + size) + block
            // + [pos_new + size, n).
            // The jobs skipped by the block are scheduled incrementally.
            Time time_skipped = completion_times[pos_old];
            Time objective_skipped = prefix_objective[pos_old];
            JobId last_job_id_skipped = last_job(jobs, pos_old);
            for (JobId pos_new = pos_old + 1; pos_new + size <= n; ++pos_new) {
                Time bound = machine_objective + delta_best;
                advance(
                        instance, machine_id, jobs[pos_new + size - 1],
                        time_skipped, objective_skipped, last_job_id_skipped);
                if (objective_skipped >= bound)
                    break;
                Time time = time_skipped;
                Time objective = objective_skipped;
                JobId last_job_id = last_job_id_skipped;
                if (!advance_block(
                            instance, machine_id,
                            jobs.begin() + pos_old, jobs.begin() + pos_old + size,
                            time, objective, last_job_id, bound)) {
                    continue;
                }
                objective = evaluate_suffix(
                        instance, data, machine_id,
                        pos_new + size, time, objective, last_job_id, bound);
                if (objective >= bound)
                    continue;
                delta_best = objective - machine_objective;
                machine_id_best = machine_id;
                pos_old_best = pos_old;
                pos_new_best = pos_new;
            }
        }
    }

    if (machine_id_best == -1)
        return false;
    Time objective_expected = data.solution.objective + delta_best;
    shift_jobs(instance, data, machine_id_best, size, pos_old_best, pos_new_best);
    check_objective(data, objective_expected);
    return true;
}

// ---- (l, l')-block swap intra-machine ----

// Swap the block of size size_1 starting at position pos_1 with the block of
// size size_2 starting at position pos_2 > pos_1 of a machine.
void swap_jobs_intra(
        const Instance& instance,
        LocalSearchData& data,
        MachineId machine_id,
        JobId pos_1,
        JobId size_1,
        JobId pos_2,
        JobId size_2)
{
    std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
    // Transforms [B1 M B2] into [B2 M B1] using two std::rotate calls.
    JobId gap = pos_2 - pos_1 - size_1;
    std::rotate(
            jobs.begin() + pos_1,
            jobs.begin() + pos_1 + size_1,
            jobs.begin() + pos_2 + size_2);
    std::rotate(
            jobs.begin() + pos_1,
            jobs.begin() + pos_1 + gap,
            jobs.begin() + pos_1 + gap + size_2);
    update_machine(instance, data, machine_id, pos_1);
    update_objective(data);
}

bool explore_swap_intra_neighborhood(
        const Instance& instance,
        LocalSearchData& data,
        JobId size_1,
        JobId size_2)
{
    Time delta_best = 0;
    MachineId machine_id_best = -1;
    JobId pos_1_best = -1;
    JobId size_1_best = -1;
    JobId pos_2_best = -1;
    JobId size_2_best = -1;

    // Both orders of the block sizes.
    std::vector<std::pair<JobId, JobId>> sizes = {{size_1, size_2}};
    if (size_1 != size_2)
        sizes.push_back({size_2, size_1});

    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        const std::vector<JobId>& jobs = data.solution.machines_jobs[machine_id];
        const std::vector<Time>& completion_times = data.completion_times[machine_id];
        const std::vector<Time>& prefix_objective = data.prefix_objective[machine_id];
        JobId n = jobs.size();
        Time machine_objective = prefix_objective[n];

        for (const auto& block_sizes: sizes) {
            JobId s1 = block_sizes.first;
            JobId s2 = block_sizes.second;
            for (JobId pos_1 = 0; pos_1 + s1 + s2 <= n; ++pos_1) {
                for (JobId pos_2 = pos_1 + s1; pos_2 + s2 <= n; ++pos_2) {
                    Time bound = machine_objective + delta_best;
                    if (prefix_objective[pos_1] >= bound)
                        break;
                    // [0, pos_1) + B2 + [pos_1 + s1, pos_2) + B1 + [pos_2 + s2, n).
                    Time time = completion_times[pos_1];
                    Time objective = prefix_objective[pos_1];
                    JobId last_job_id = last_job(jobs, pos_1);
                    if (!advance_block(
                                instance, machine_id,
                                jobs.begin() + pos_2, jobs.begin() + pos_2 + s2,
                                time, objective, last_job_id, bound)) {
                        continue;
                    }
                    if (!advance_block(
                                instance, machine_id,
                                jobs.begin() + pos_1 + s1, jobs.begin() + pos_2,
                                time, objective, last_job_id, bound)) {
                        continue;
                    }
                    if (!advance_block(
                                instance, machine_id,
                                jobs.begin() + pos_1, jobs.begin() + pos_1 + s1,
                                time, objective, last_job_id, bound)) {
                        continue;
                    }
                    objective = evaluate_suffix(
                            instance, data, machine_id,
                            pos_2 + s2, time, objective, last_job_id, bound);
                    if (objective >= bound)
                        continue;
                    delta_best = objective - machine_objective;
                    machine_id_best = machine_id;
                    pos_1_best = pos_1;
                    size_1_best = s1;
                    pos_2_best = pos_2;
                    size_2_best = s2;
                }
            }
        }
    }

    if (machine_id_best == -1)
        return false;
    Time objective_expected = data.solution.objective + delta_best;
    swap_jobs_intra(
            instance, data, machine_id_best,
            pos_1_best, size_1_best, pos_2_best, size_2_best);
    check_objective(data, objective_expected);
    return true;
}

// ---- l-block insertion inter-machine ----

// Move the block of size 'size' starting at position pos_1 of machine
// machine_id_1 to position pos_2 of machine machine_id_2.
void move_jobs_inter(
        const Instance& instance,
        LocalSearchData& data,
        MachineId machine_id_1,
        JobId pos_1,
        JobId size,
        MachineId machine_id_2,
        JobId pos_2)
{
    std::vector<JobId>& jobs_1 = data.solution.machines_jobs[machine_id_1];
    std::vector<JobId>& jobs_2 = data.solution.machines_jobs[machine_id_2];
    jobs_2.insert(
            jobs_2.begin() + pos_2,
            jobs_1.begin() + pos_1,
            jobs_1.begin() + pos_1 + size);
    jobs_1.erase(
            jobs_1.begin() + pos_1,
            jobs_1.begin() + pos_1 + size);
    update_machine(instance, data, machine_id_1, pos_1);
    update_machine(instance, data, machine_id_2, pos_2);
    update_objective(data);
}

bool explore_insert_inter_neighborhood(
        const Instance& instance,
        LocalSearchData& data,
        JobId size)
{
    Time delta_best = 0;
    MachineId machine_id_1_best = -1;
    JobId pos_1_best = -1;
    MachineId machine_id_2_best = -1;
    JobId pos_2_best = -1;

    for (MachineId machine_id_1 = 0;
            machine_id_1 < instance.number_of_machines();
            ++machine_id_1) {
        const std::vector<JobId>& jobs_1 = data.solution.machines_jobs[machine_id_1];
        const std::vector<Time>& completion_times_1 = data.completion_times[machine_id_1];
        const std::vector<Time>& prefix_objective_1 = data.prefix_objective[machine_id_1];
        JobId n_1 = jobs_1.size();

        for (JobId pos_1 = 0; pos_1 + size <= n_1; ++pos_1) {

            // Objective of machine_id_1 without the block. It is computed
            // once for all the insertion positions.
            Time delta_1 = evaluate_suffix(
                    instance, data, machine_id_1,
                    pos_1 + size,
                    completion_times_1[pos_1],
                    prefix_objective_1[pos_1],
                    last_job(jobs_1, pos_1),
                    std::numeric_limits<Time>::max())
                - prefix_objective_1[n_1];

            for (MachineId machine_id_2 = 0;
                    machine_id_2 < instance.number_of_machines();
                    ++machine_id_2) {
                if (machine_id_2 == machine_id_1)
                    continue;
                const std::vector<JobId>& jobs_2 = data.solution.machines_jobs[machine_id_2];
                const std::vector<Time>& completion_times_2 = data.completion_times[machine_id_2];
                const std::vector<Time>& prefix_objective_2 = data.prefix_objective[machine_id_2];
                JobId n_2 = jobs_2.size();
                Time machine_objective_2 = prefix_objective_2[n_2];

                for (JobId pos_2 = 0; pos_2 <= n_2; ++pos_2) {
                    Time bound = machine_objective_2 + delta_best - delta_1;
                    if (prefix_objective_2[pos_2] >= bound)
                        break;
                    // [0, pos_2) + block + [pos_2, n_2) on machine_id_2.
                    Time time = completion_times_2[pos_2];
                    Time objective = prefix_objective_2[pos_2];
                    JobId last_job_id = last_job(jobs_2, pos_2);
                    if (!advance_block(
                                instance, machine_id_2,
                                jobs_1.begin() + pos_1, jobs_1.begin() + pos_1 + size,
                                time, objective, last_job_id, bound)) {
                        continue;
                    }
                    objective = evaluate_suffix(
                            instance, data, machine_id_2,
                            pos_2, time, objective, last_job_id, bound);
                    if (objective >= bound)
                        continue;
                    delta_best = delta_1 + objective - machine_objective_2;
                    machine_id_1_best = machine_id_1;
                    pos_1_best = pos_1;
                    machine_id_2_best = machine_id_2;
                    pos_2_best = pos_2;
                }
            }
        }
    }

    if (machine_id_1_best == -1)
        return false;
    Time objective_expected = data.solution.objective + delta_best;
    move_jobs_inter(
            instance, data,
            machine_id_1_best, pos_1_best, size,
            machine_id_2_best, pos_2_best);
    check_objective(data, objective_expected);
    return true;
}

// ---- (l, l')-block swap inter-machine ----

// Swap the block of size size_1 starting at position pos_1 of machine
// machine_id_1 with the block of size size_2 starting at position pos_2 of
// machine machine_id_2.
void swap_jobs_inter(
        const Instance& instance,
        LocalSearchData& data,
        MachineId machine_id_1,
        JobId pos_1,
        JobId size_1,
        MachineId machine_id_2,
        JobId pos_2,
        JobId size_2)
{
    std::vector<JobId>& jobs_1 = data.solution.machines_jobs[machine_id_1];
    std::vector<JobId>& jobs_2 = data.solution.machines_jobs[machine_id_2];
    std::vector<JobId> block_1(jobs_1.begin() + pos_1, jobs_1.begin() + pos_1 + size_1);
    std::vector<JobId> block_2(jobs_2.begin() + pos_2, jobs_2.begin() + pos_2 + size_2);
    jobs_1.erase(jobs_1.begin() + pos_1, jobs_1.begin() + pos_1 + size_1);
    jobs_1.insert(jobs_1.begin() + pos_1, block_2.begin(), block_2.end());
    jobs_2.erase(jobs_2.begin() + pos_2, jobs_2.begin() + pos_2 + size_2);
    jobs_2.insert(jobs_2.begin() + pos_2, block_1.begin(), block_1.end());
    update_machine(instance, data, machine_id_1, pos_1);
    update_machine(instance, data, machine_id_2, pos_2);
    update_objective(data);
}

bool explore_swap_inter_neighborhood(
        const Instance& instance,
        LocalSearchData& data,
        JobId size_1,
        JobId size_2)
{
    Time delta_best = 0;
    MachineId machine_id_1_best = -1;
    JobId pos_1_best = -1;
    MachineId machine_id_2_best = -1;
    JobId pos_2_best = -1;

    for (MachineId machine_id_1 = 0;
            machine_id_1 < instance.number_of_machines();
            ++machine_id_1) {
        const std::vector<JobId>& jobs_1 = data.solution.machines_jobs[machine_id_1];
        const std::vector<Time>& completion_times_1 = data.completion_times[machine_id_1];
        const std::vector<Time>& prefix_objective_1 = data.prefix_objective[machine_id_1];
        JobId n_1 = jobs_1.size();
        Time machine_objective_1 = prefix_objective_1[n_1];

        for (MachineId machine_id_2 = 0;
                machine_id_2 < instance.number_of_machines();
                ++machine_id_2) {
            if (machine_id_2 == machine_id_1)
                continue;
            // With blocks of the same size, each pair of machines is
            // considered once.
            if (size_1 == size_2 && machine_id_2 < machine_id_1)
                continue;
            const std::vector<JobId>& jobs_2 = data.solution.machines_jobs[machine_id_2];
            const std::vector<Time>& completion_times_2 = data.completion_times[machine_id_2];
            const std::vector<Time>& prefix_objective_2 = data.prefix_objective[machine_id_2];
            JobId n_2 = jobs_2.size();
            Time machine_objective_2 = prefix_objective_2[n_2];

            for (JobId pos_1 = 0; pos_1 + size_1 <= n_1; ++pos_1) {
                for (JobId pos_2 = 0; pos_2 + size_2 <= n_2; ++pos_2) {
                    // The objective of machine_id_2 after the move is at
                    // least the one of its first pos_2 jobs.
                    Time bound_1 = machine_objective_1 + delta_best
                        + (machine_objective_2 - prefix_objective_2[pos_2]);
                    if (prefix_objective_1[pos_1] >= bound_1)
                        break;
                    // [0, pos_1) + B2 + [pos_1 + size_1, n_1) on machine_id_1.
                    Time time = completion_times_1[pos_1];
                    Time objective_1 = prefix_objective_1[pos_1];
                    JobId last_job_id = last_job(jobs_1, pos_1);
                    if (!advance_block(
                                instance, machine_id_1,
                                jobs_2.begin() + pos_2, jobs_2.begin() + pos_2 + size_2,
                                time, objective_1, last_job_id, bound_1)) {
                        continue;
                    }
                    objective_1 = evaluate_suffix(
                            instance, data, machine_id_1,
                            pos_1 + size_1, time, objective_1, last_job_id, bound_1);
                    if (objective_1 >= bound_1)
                        continue;
                    Time delta_1 = objective_1 - machine_objective_1;
                    // [0, pos_2) + B1 + [pos_2 + size_2, n_2) on machine_id_2.
                    Time bound_2 = machine_objective_2 + delta_best - delta_1;
                    time = completion_times_2[pos_2];
                    Time objective_2 = prefix_objective_2[pos_2];
                    last_job_id = last_job(jobs_2, pos_2);
                    if (!advance_block(
                                instance, machine_id_2,
                                jobs_1.begin() + pos_1, jobs_1.begin() + pos_1 + size_1,
                                time, objective_2, last_job_id, bound_2)) {
                        continue;
                    }
                    objective_2 = evaluate_suffix(
                            instance, data, machine_id_2,
                            pos_2 + size_2, time, objective_2, last_job_id, bound_2);
                    if (objective_2 >= bound_2)
                        continue;
                    delta_best = delta_1 + objective_2 - machine_objective_2;
                    machine_id_1_best = machine_id_1;
                    pos_1_best = pos_1;
                    machine_id_2_best = machine_id_2;
                    pos_2_best = pos_2;
                }
            }
        }
    }

    if (machine_id_1_best == -1)
        return false;
    Time objective_expected = data.solution.objective + delta_best;
    swap_jobs_inter(
            instance, data,
            machine_id_1_best, pos_1_best, size_1,
            machine_id_2_best, pos_2_best, size_2);
    check_objective(data, objective_expected);
    return true;
}

// ---- Randomized variable neighborhood descent ----

std::vector<NeighborhoodParameters> neighborhoods(
        const Instance& instance)
{
    std::vector<NeighborhoodParameters> neighborhoods;
    if (instance.number_of_machines() == 1) {
        // L_intra = {1, 2, 3}, L'_intra = {(1, 1)}.
        for (JobId size: {1, 2, 3})
            neighborhoods.push_back({Neighborhood::InsertIntra, size, -1});
        neighborhoods.push_back({Neighborhood::SwapIntra, 1, 1});
    } else {
        // L_intra = {1, 2}, L'_intra = {(1, 1)}, L_inter = {1, 2},
        // L'_inter = {(1, 1), (1, 2), (1, 3), (2, 2), (2, 3), (2, 4), (3, 3),
        // (3, 4), (4, 4)}.
        for (JobId size: {1, 2})
            neighborhoods.push_back({Neighborhood::InsertIntra, size, -1});
        neighborhoods.push_back({Neighborhood::SwapIntra, 1, 1});
        for (JobId size: {1, 2})
            neighborhoods.push_back({Neighborhood::InsertInter, size, -1});
        for (const auto& block_sizes: std::vector<std::pair<JobId, JobId>>{
                {1, 1}, {1, 2}, {1, 3},
                {2, 2}, {2, 3}, {2, 4},
                {3, 3}, {3, 4}, {4, 4}}) {
            neighborhoods.push_back({
                    Neighborhood::SwapInter,
                    block_sizes.first,
                    block_sizes.second});
        }
    }
    return neighborhoods;
}

void local_search(
        const Instance& instance,
        const LocalSearchParameters& parameters,
        std::mt19937_64& generator,
        LocalSearchOutput& output,
        AlgorithmFormatter& algorithm_formatter,
        LocalSearchData& data,
        const std::vector<NeighborhoodParameters>& all_neighborhoods)
{
    // Randomly choose an unexplored neighborhood; when a neighborhood
    // improves the solution, all the neighborhoods can be chosen again.
    std::vector<NeighborhoodParameters> remaining_neighborhoods = all_neighborhoods;
    while (!remaining_neighborhoods.empty()) {
        if (parameters.timer.needs_to_end())
            break;
        if (data.solution.objective <= output.bound())
            break;

        std::uniform_int_distribution<size_t> distribution(
                0, remaining_neighborhoods.size() - 1);
        size_t neighborhood_pos = distribution(generator);
        const NeighborhoodParameters& neighborhood = remaining_neighborhoods[neighborhood_pos];

        bool improved = false;
        switch (neighborhood.neighborhood) {
        case Neighborhood::InsertIntra: {
            improved = explore_insert_intra_neighborhood(instance, data, neighborhood.size_1);
            break;
        } case Neighborhood::SwapIntra: {
            improved = explore_swap_intra_neighborhood(instance, data, neighborhood.size_1, neighborhood.size_2);
            break;
        } case Neighborhood::InsertInter: {
            improved = explore_insert_inter_neighborhood(instance, data, neighborhood.size_1);
            break;
        } case Neighborhood::SwapInter: {
            improved = explore_swap_inter_neighborhood(instance, data, neighborhood.size_1, neighborhood.size_2);
            break;
        }
        }

        if (improved) {
            remaining_neighborhoods = all_neighborhoods;
            if (!output.solution.feasible()
                    || output.solution.objective_value() > data.solution.objective) {
                std::stringstream ss;
                ss << "restart " << output.number_of_restarts
                    << " pert " << output.number_of_perturbations;
                algorithm_formatter.update_solution(
                        build_solution(instance, data.solution), ss.str());
            }
        } else {
            remaining_neighborhoods.erase(remaining_neighborhoods.begin() + neighborhood_pos);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// Initial solutions //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Random solution: each job on a random machine, in a random order.
LocalSearchSolution generate_initial_solution_random(
        const Instance& instance,
        std::mt19937_64& generator)
{
    LocalSearchSolution solution;
    solution.machines_jobs.resize(instance.number_of_machines());
    std::vector<JobId> jobs(instance.number_of_jobs());
    std::iota(jobs.begin(), jobs.end(), 0);
    std::shuffle(jobs.begin(), jobs.end(), generator);
    std::uniform_int_distribution<MachineId> distribution(
            0, instance.number_of_machines() - 1);
    for (JobId job_id: jobs)
        solution.machines_jobs[distribution(generator)].push_back(job_id);
    return solution;
}

// Greedy randomized solution on a single machine (constructive phase of
// GRASP): the next job is chosen at random among the remaining jobs whose
// key (due date or release date) is at most min + alpha (max - min).
LocalSearchSolution generate_initial_solution_greedy_randomized(
        const Instance& instance,
        std::mt19937_64& generator,
        bool release_dates)
{
    auto key = [&instance, release_dates](JobId job_id)
    {
        const Job& job = instance.job(job_id);
        if (release_dates)
            return job.release_date;
        return (job.due_date >= 0)? job.due_date: std::numeric_limits<Time>::max();
    };
    double alpha = std::uniform_real_distribution<double>(0, 1)(generator);

    LocalSearchSolution solution;
    solution.machines_jobs.resize(instance.number_of_machines());
    std::vector<JobId> remaining_jobs(instance.number_of_jobs());
    std::iota(remaining_jobs.begin(), remaining_jobs.end(), 0);
    std::vector<JobId> candidates;
    while (!remaining_jobs.empty()) {
        Time key_min = std::numeric_limits<Time>::max();
        Time key_max = std::numeric_limits<Time>::min();
        for (JobId job_id: remaining_jobs) {
            key_min = (std::min)(key_min, key(job_id));
            key_max = (std::max)(key_max, key(job_id));
        }
        double threshold = key_min + alpha * ((double)key_max - key_min);
        candidates.clear();
        for (JobId pos = 0; pos < (JobId)remaining_jobs.size(); ++pos)
            if (key(remaining_jobs[pos]) <= threshold)
                candidates.push_back(pos);
        JobId pos = candidates[std::uniform_int_distribution<size_t>(
                0, candidates.size() - 1)(generator)];
        solution.machines_jobs[0].push_back(remaining_jobs[pos]);
        remaining_jobs[pos] = remaining_jobs.back();
        remaining_jobs.pop_back();
    }
    return solution;
}

LocalSearchSolution generate_initial_solution(
        const Instance& instance,
        std::mt19937_64& generator)
{
    if (instance.number_of_machines() > 1)
        return generate_initial_solution_random(instance, generator);
    bool release_dates = false;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).release_date != 0)
            release_dates = true;
    if (release_dates)
        return generate_initial_solution_greedy_randomized(instance, generator, true);
    if (std::uniform_int_distribution<int>(0, 1)(generator) == 0)
        return generate_initial_solution_random(instance, generator);
    return generate_initial_solution_greedy_randomized(instance, generator, false);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Perturbations ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Single machine: one (l, l')-block swap intra-machine at random, with l and
// l' in {2, ..., n / 4}.
void perturbation_swap_intra(
        const Instance& instance,
        LocalSearchData& data,
        std::mt19937_64& generator)
{
    JobId n = data.solution.machines_jobs[0].size();
    JobId size_max = (std::max)((JobId)1, n / 4);
    JobId size_min = (std::min)((JobId)2, size_max);
    if (2 * size_min > n)
        return;
    std::uniform_int_distribution<JobId> distribution_size(size_min, size_max);
    JobId size_1 = distribution_size(generator);
    JobId size_2 = distribution_size(generator);
    if (size_1 + size_2 > n)
        size_1 = size_2 = size_min;
    JobId pos_1 = std::uniform_int_distribution<JobId>(0, n - size_1 - size_2)(generator);
    JobId pos_2 = std::uniform_int_distribution<JobId>(pos_1 + size_1, n - size_2)(generator);
    swap_jobs_intra(instance, data, 0, pos_1, size_1, pos_2, size_2);
}

// Parallel machines: a block of size_1 jobs of a machine is moved to another
// machine and a block of size_2 jobs of this other machine is moved to the
// first one, the machines and positions being chosen at random; repeated 1,
// 2 or 3 times.
// - multiple (1, 1)-insertion inter-machine: size_1 = size_2 = 1;
// - multiple (l, l')-block insertion inter-machine: size_1 in {1, 2},
//   size_2 in {2, 3}.
void perturbation_insert_inter(
        const Instance& instance,
        LocalSearchData& data,
        std::mt19937_64& generator,
        bool blocks)
{
    MachineId m = instance.number_of_machines();
    JobId number_of_moves = std::uniform_int_distribution<JobId>(1, 3)(generator);
    for (JobId move = 0; move < number_of_moves; ++move) {
        MachineId machine_id_1 = std::uniform_int_distribution<MachineId>(0, m - 1)(generator);
        MachineId machine_id_2 = std::uniform_int_distribution<MachineId>(0, m - 2)(generator);
        if (machine_id_2 >= machine_id_1)
            machine_id_2++;
        std::vector<JobId>& jobs_1 = data.solution.machines_jobs[machine_id_1];
        std::vector<JobId>& jobs_2 = data.solution.machines_jobs[machine_id_2];
        JobId n_1 = jobs_1.size();
        JobId n_2 = jobs_2.size();
        JobId size_1 = (blocks)? std::uniform_int_distribution<JobId>(1, 2)(generator): 1;
        JobId size_2 = (blocks)? std::uniform_int_distribution<JobId>(2, 3)(generator): 1;
        size_1 = (std::min)(size_1, n_1);
        size_2 = (std::min)(size_2, n_2);

        // The two blocks are chosen in the current sequences, so that the
        // block moved to machine_id_2 can't be moved back.
        JobId pos_1 = std::uniform_int_distribution<JobId>(0, n_1 - size_1)(generator);
        JobId pos_2 = std::uniform_int_distribution<JobId>(0, n_2 - size_2)(generator);
        std::vector<JobId> block_1(
                jobs_1.begin() + pos_1,
                jobs_1.begin() + pos_1 + size_1);
        std::vector<JobId> block_2(
                jobs_2.begin() + pos_2,
                jobs_2.begin() + pos_2 + size_2);
        jobs_1.erase(jobs_1.begin() + pos_1, jobs_1.begin() + pos_1 + size_1);
        jobs_2.erase(jobs_2.begin() + pos_2, jobs_2.begin() + pos_2 + size_2);

        // Insert each block at a random position of the other machine.
        JobId pos_1_new = std::uniform_int_distribution<JobId>(0, jobs_1.size())(generator);
        JobId pos_2_new = std::uniform_int_distribution<JobId>(0, jobs_2.size())(generator);
        jobs_1.insert(jobs_1.begin() + pos_1_new, block_2.begin(), block_2.end());
        jobs_2.insert(jobs_2.begin() + pos_2_new, block_1.begin(), block_1.end());
        update_machine(instance, data, machine_id_1, (std::min)(pos_1, pos_1_new));
        update_machine(instance, data, machine_id_2, (std::min)(pos_2, pos_2_new));
        update_objective(data);
    }
}

void perturbation(
        const Instance& instance,
        LocalSearchData& data,
        std::mt19937_64& generator)
{
    if (instance.number_of_machines() == 1) {
        perturbation_swap_intra(instance, data, generator);
    } else {
        bool blocks = (std::uniform_int_distribution<int>(0, 1)(generator) == 1);
        perturbation_insert_inter(instance, data, generator, blocks);
    }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Algorithm ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Multi-start iterated local search.
void run_algorithm(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters,
        LocalSearchOutput& output,
        AlgorithmFormatter& algorithm_formatter,
        LocalSearchData& data)
{
    bool release_dates = false;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
        if (instance.job(job_id).release_date != 0)
            release_dates = true;
    Counter maximum_number_of_perturbations_without_improvement
        = parameters.maximum_number_of_perturbations_without_improvement;
    if (maximum_number_of_perturbations_without_improvement < 0) {
        maximum_number_of_perturbations_without_improvement = (release_dates)?
            instance.number_of_jobs():
            4 * instance.number_of_jobs();
    }

    std::vector<NeighborhoodParameters> all_neighborhoods = neighborhoods(instance);

    for (Counter restart = 0;
            restart < parameters.maximum_number_of_restarts;
            ++restart) {
        if (parameters.timer.needs_to_end())
            break;
        if (output.solution.feasible()
                && output.solution.objective_value() <= output.bound()) {
            break;
        }
        output.number_of_restarts = restart + 1;

        // Initial solution of the restart.
        if (output.number_of_restarts == 1 && initial_solution != nullptr) {
            load_solution(instance, data, from_solution(instance, *initial_solution));
        } else {
            load_solution(instance, data, generate_initial_solution(instance, generator));
        }
        if (!output.solution.feasible()
                || output.solution.objective_value() > data.solution.objective) {
            std::stringstream ss;
            ss << "restart " << output.number_of_restarts << " initial";
            algorithm_formatter.update_solution(
                    build_solution(instance, data.solution), ss.str());
        }
        local_search(
                instance, parameters, generator,
                output, algorithm_formatter,
                data, all_neighborhoods);
        LocalSearchSolution restart_best_solution = data.solution;

        // Iterated local search.
        Counter number_of_perturbations_without_improvement = 0;
        while (number_of_perturbations_without_improvement
                < maximum_number_of_perturbations_without_improvement) {
            if (parameters.timer.needs_to_end())
                break;
            if (output.solution.objective_value() <= output.bound())
                break;
            output.number_of_perturbations++;

            load_solution(instance, data, restart_best_solution);
            perturbation(instance, data, generator);
            local_search(
                    instance, parameters, generator,
                    output, algorithm_formatter,
                    data, all_neighborhoods);

            if (data.solution.objective < restart_best_solution.objective) {
                restart_best_solution = data.solution;
                number_of_perturbations_without_improvement = 0;
            } else {
                number_of_perturbations_without_improvement++;
            }
        }
    }
}

}

const LocalSearchOutput parallelschedulingsolver::local_search(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution,
        const LocalSearchParameters& parameters)
{
    LocalSearchOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Local search");

    if (instance.objective() != Objective::TotalFlowTime
            && instance.objective() != Objective::TotalTardiness) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": unsupported objective; "
                "only TotalFlowTime and TotalTardiness are supported.");
    }

    algorithm_formatter.print_header();

    if (instance.number_of_jobs() == 0) {
        algorithm_formatter.update_solution(
                SolutionBuilder().set_instance(instance).build(),
                "");
        algorithm_formatter.end();
        return output;
    }

    LocalSearchData data;
    data.completion_times.resize(instance.number_of_machines());
    data.prefix_objective.resize(instance.number_of_machines());

    run_algorithm(
            instance, generator, initial_solution, parameters,
            output, algorithm_formatter, data);

    algorithm_formatter.end();
    return output;
}
