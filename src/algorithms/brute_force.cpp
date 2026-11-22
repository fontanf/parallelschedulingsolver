#include "parallelschedulingsolver/algorithms/brute_force.hpp"

#include "parallelschedulingsolver/algorithms/special_cases.hpp"

#include <algorithm>
#include <numeric>

using namespace parallelschedulingsolver;

namespace
{

/*
 * A candidate is a vector of job ids and separators (-1): the jobs before the
 * first separator are the sequence of machine 0, the jobs between the first
 * and the second separator are the sequence of machine 1, etc.
 */

/**
 * Value of the objective of the instance for a candidate, whose rejected jobs
 * have a total rejection cost 'rejection_cost'.
 */
Time evaluate(
        const Instance& instance,
        const std::vector<JobId>& candidate,
        Time rejection_cost)
{
    Time makespan = 0;
    Time total_flow_time = 0;
    Time total_tardiness = 0;
    Time maximum_lateness = std::numeric_limits<Time>::min();
    Time throughput = 0;

    MachineId machine_id = 0;
    Time current_time = 0;
    JobId previous_job_id = -1;
    for (JobId job_id: candidate) {
        if (job_id == -1) {
            machine_id++;
            current_time = 0;
            previous_job_id = -1;
            continue;
        }
        const Job& job = instance.job(job_id);
        Time start = (std::max)(
                current_time + instance.setup_time(machine_id, previous_job_id, job_id),
                job.release_date);
        current_time = start + job.processing_times[machine_id];
        previous_job_id = job_id;

        makespan = (std::max)(makespan, current_time);
        total_flow_time += job.weight * (current_time - job.release_date);
        if (job.due_date >= 0) {
            total_tardiness += job.weight * (std::max)((Time)0, current_time - job.due_date);
            maximum_lateness = (std::max)(maximum_lateness, current_time - job.due_date);
            if (current_time <= job.due_date)
                throughput += job.weight;
        }
    }

    switch (instance.objective()) {
    case Objective::Makespan:
        return makespan + rejection_cost;
    case Objective::TotalFlowTime:
        return total_flow_time + rejection_cost;
    case Objective::TotalTardiness:
        return total_tardiness + rejection_cost;
    case Objective::MaximumLateness:
        return maximum_lateness;
    case Objective::Throughput:
        return throughput;
    }
    return 0;
}

/** Return true if value_1 is strictly better than value_2. */
bool better(
        const Instance& instance,
        Time value_1,
        Time value_2)
{
    if (instance.objective() == Objective::Throughput)
        return value_1 > value_2;
    return value_1 < value_2;
}

/** Build the solution of a candidate, each job as early as possible. */
Solution build_solution(
        const Instance& instance,
        const std::vector<JobId>& candidate)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    MachineId machine_id = 0;
    Time current_time = 0;
    JobId previous_job_id = -1;
    for (JobId job_id: candidate) {
        if (job_id == -1) {
            machine_id++;
            current_time = 0;
            previous_job_id = -1;
            continue;
        }
        const Job& job = instance.job(job_id);
        Time start = (std::max)(
                current_time + instance.setup_time(machine_id, previous_job_id, job_id),
                job.release_date);
        solution_builder.append_job_to_machine(machine_id, job_id, start);
        current_time = start + job.processing_times[machine_id];
        previous_job_id = job_id;
    }
    return solution_builder.build();
}

/** Value of a solution for the objective of its instance. */
Time solution_value(const Solution& solution)
{
    switch (solution.instance().objective()) {
    case Objective::Makespan:
        return solution.makespan() + solution.total_rejection_cost();
    case Objective::TotalFlowTime:
        return solution.total_flow_time() + solution.total_rejection_cost();
    case Objective::TotalTardiness:
        return solution.total_tardiness() + solution.total_rejection_cost();
    case Objective::MaximumLateness:
        return solution.maximum_lateness();
    case Objective::Throughput:
        return solution.throughput();
    }
    return 0;
}

/** Number of candidates for n jobs and m machines, without rejection. */
double number_of_candidates_without_rejection(
        JobId n,
        MachineId m,
        bool use_rule)
{
    double number = 1;
    if (use_rule) {
        // m^n assignments.
        for (JobId job_id = 0; job_id < n; ++job_id)
            number *= m;
    } else {
        // (n + m - 1)! / (m - 1)! arrangements of the jobs and the m - 1
        // separators.
        for (JobId k = m; k <= n + m - 1; ++k)
            number *= k;
    }
    return number;
}

/**
 * Number of candidates, as a floating point number to avoid overflows: for
 * each subset of the jobs with a rejection cost, the arrangements (or the
 * assignments) of the other jobs.
 */
double number_of_candidates(
        const Instance& instance,
        bool use_rule)
{
    JobId n = instance.number_of_jobs();
    MachineId m = instance.number_of_machines();
    // Number of jobs with a rejection cost.
    JobId r = 0;
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (instance.job(job_id).rejection_cost >= 0)
            r++;
    // Number of subsets of k rejected jobs among r.
    double number_of_subsets = 1;
    double total = 0;
    for (JobId k = 0; k <= r; ++k) {
        total += number_of_subsets * number_of_candidates_without_rejection(n - k, m, use_rule);
        number_of_subsets = number_of_subsets * (r - k) / (k + 1);
    }
    return total;
}

}

Output parallelschedulingsolver::brute_force(
        const Instance& instance,
        const BruteForceParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Brute force");
    algorithm_formatter.print_header();

    JobId n = instance.number_of_jobs();
    MachineId m = instance.number_of_machines();

    // Check if a sequencing rule is optimal for a fixed assignment.
    bool use_rule = false;
    if (m > 1 && !instance.has_setup_times()) {
        switch (instance.objective()) {
        case Objective::Makespan:
            use_rule = true;
            break;
        case Objective::TotalFlowTime:
        case Objective::MaximumLateness:
            use_rule = no_release_dates(instance);
            break;
        default:
            break;
        }
    }

    if (parameters.maximum_number_of_candidates >= 0
            && number_of_candidates(instance, use_rule)
            > parameters.maximum_number_of_candidates) {
        algorithm_formatter.print("too many candidates");
        algorithm_formatter.end();
        return output;
    }

    // Jobs with a rejection cost.
    std::vector<JobId> rejectable_job_ids;
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (instance.job(job_id).rejection_cost >= 0)
            rejectable_job_ids.push_back(job_id);
    JobId r = rejectable_job_ids.size();
    if (r >= 63) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "too many jobs with a rejection cost; "
                "number of jobs with a rejection cost: " + std::to_string(r) + ".");
    }

    // Best candidate.
    bool has_best = false;
    Time best_value = 0;
    std::vector<JobId> best_candidate;
    auto update = [&](
            const std::vector<JobId>& candidate,
            Time rejection_cost)
    {
        Time value = evaluate(instance, candidate, rejection_cost);
        if (has_best && !better(instance, value, best_value))
            return;
        has_best = true;
        best_value = value;
        best_candidate = candidate;
        Solution solution = build_solution(instance, candidate);
        if (solution_value(solution) != value) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": "
                    "wrong evaluation of a candidate; "
                    "value: " + std::to_string(value) + "; "
                    "solution value: " + std::to_string(solution_value(solution)) + ".");
        }
        algorithm_formatter.update_solution(solution, "brute force");
    };

    // Sequencing rule: return true if job_id_1 goes before job_id_2 on
    // machine_id.
    auto before = [&instance](
            MachineId machine_id,
            JobId job_id_1,
            JobId job_id_2)
    {
        const Job& job_1 = instance.job(job_id_1);
        const Job& job_2 = instance.job(job_id_2);
        switch (instance.objective()) {
        case Objective::Makespan:
            return job_1.release_date < job_2.release_date;
        case Objective::TotalFlowTime:
            // p_1 / w_1 < p_2 / w_2, without divisions.
            return job_1.processing_times[machine_id] * job_2.weight
                < job_2.processing_times[machine_id] * job_1.weight;
        case Objective::MaximumLateness: {
            // Jobs without due date last.
            if (job_2.due_date < 0)
                return job_1.due_date >= 0;
            return job_1.due_date >= 0 && job_1.due_date < job_2.due_date;
        } default:
            return false;
        }
    };

    Counter number_of_enumerated_candidates = 0;
    bool interrupted = false;

    // All the subsets of rejected jobs.
    for (uint64_t subset = 0;
            subset < ((uint64_t)1 << r) && !interrupted;
            ++subset) {

        // Scheduled jobs and total rejection cost.
        std::vector<int8_t> rejected(n, 0);
        Time rejection_cost = 0;
        for (JobId pos = 0; pos < r; ++pos) {
            if ((subset >> pos) & 1) {
                JobId job_id = rejectable_job_ids[pos];
                rejected[job_id] = 1;
                rejection_cost += instance.job(job_id).rejection_cost;
            }
        }
        std::vector<JobId> job_ids;
        for (JobId job_id = 0; job_id < n; ++job_id)
            if (!rejected[job_id])
                job_ids.push_back(job_id);
        JobId number_of_scheduled_jobs = job_ids.size();

        if (!use_rule) {
            // All the arrangements of the jobs and the m - 1 separators.
            // std::next_permutation enumerates the distinct permutations of
            // a multiset in lexicographic order, starting from the sorted
            // one.
            std::vector<JobId> candidate(m - 1, -1);
            candidate.insert(candidate.end(), job_ids.begin(), job_ids.end());
            do {
                if (number_of_enumerated_candidates % 1024 == 0
                        && parameters.timer.needs_to_end()) {
                    interrupted = true;
                    break;
                }
                number_of_enumerated_candidates++;
                update(candidate, rejection_cost);
            } while (std::next_permutation(candidate.begin(), candidate.end()));

        } else {
            // All the assignments, as a number in base m.
            std::vector<MachineId> assignment(number_of_scheduled_jobs, 0);
            std::vector<std::vector<JobId>> sequences(m);
            std::vector<JobId> candidate;
            for (;;) {
                if (number_of_enumerated_candidates % 1024 == 0
                        && parameters.timer.needs_to_end()) {
                    interrupted = true;
                    break;
                }
                number_of_enumerated_candidates++;

                for (MachineId machine_id = 0; machine_id < m; ++machine_id)
                    sequences[machine_id].clear();
                for (JobId pos = 0; pos < number_of_scheduled_jobs; ++pos)
                    sequences[assignment[pos]].push_back(job_ids[pos]);
                candidate.clear();
                for (MachineId machine_id = 0; machine_id < m; ++machine_id) {
                    std::stable_sort(
                            sequences[machine_id].begin(),
                            sequences[machine_id].end(),
                            [&before, machine_id](JobId job_id_1, JobId job_id_2)
                            {
                                return before(machine_id, job_id_1, job_id_2);
                            });
                    if (machine_id > 0)
                        candidate.push_back(-1);
                    candidate.insert(
                            candidate.end(),
                            sequences[machine_id].begin(),
                            sequences[machine_id].end());
                }
                update(candidate, rejection_cost);

                // Next assignment.
                JobId pos = 0;
                while (pos < number_of_scheduled_jobs && assignment[pos] == m - 1) {
                    assignment[pos] = 0;
                    pos++;
                }
                if (pos == number_of_scheduled_jobs)
                    break;
                assignment[pos]++;
            }
        }
    }

    algorithm_formatter.print(
            "candidates: " + std::to_string(number_of_enumerated_candidates));

    // The enumeration is complete: the best solution is optimal.
    if (!interrupted && has_best) {
        switch (instance.objective()) {
        case Objective::Makespan:
            algorithm_formatter.update_makespan_bound(best_value, "enumeration");
            break;
        case Objective::TotalFlowTime:
            algorithm_formatter.update_total_flow_time_bound(best_value, "enumeration");
            break;
        case Objective::TotalTardiness:
            algorithm_formatter.update_total_tardiness_bound(best_value, "enumeration");
            break;
        case Objective::MaximumLateness:
            algorithm_formatter.update_maximum_lateness_bound(best_value, "enumeration");
            break;
        case Objective::Throughput:
            algorithm_formatter.update_throughput_bound(best_value, "enumeration");
            break;
        }
    }

    algorithm_formatter.end();
    return output;
}
