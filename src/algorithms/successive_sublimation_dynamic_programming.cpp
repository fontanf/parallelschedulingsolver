#include "parallelschedulingsolver/algorithms/successive_sublimation_dynamic_programming.hpp"

#include "parallelschedulingsolver/algorithms/local_search.hpp"

#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"

#include "parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp"
#include "parallelschedulingsolver/algorithms/column_generation.hpp"


#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <numeric>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

namespace
{

/** Tolerance on the lower bounds, computed with floating point multipliers. */
constexpr double TOLERANCE = 1e-6;

enum class Status { Completed, MemoryOut, TimeOut };

class SuccessiveSublimationDynamicProgramming
{

public:

    SuccessiveSublimationDynamicProgramming(
            const Instance& instance,
            const SuccessiveSublimationDynamicProgrammingParameters& parameters,
            SuccessiveSublimationDynamicProgrammingOutput& output,
            AlgorithmFormatter& algorithm_formatter):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        algorithm_formatter_(algorithm_formatter),
        maximum_memory_(parameters.maximum_memory * 1024 * 1024) { }

    void run();

private:

    /*
     * Preprocessing.
     */


    /*
     * Bounds.
     */

    bool optimal() const { return lower_bound_ >= upper_bound_; }

    bool end() const { return optimal() || parameters_.timer.needs_to_end(); }

    /** Solution of the instance from a sequence of jobs of the problem. */
    Solution build_solution(const std::vector<JobId>& sequence) const
    {
        return time_indexed::build_solution(instance_, *problem_, sequence);
    }

    void update_upper_bound(
            const Solution& solution,
            const std::string& s);

    /** Update the lower bound with a lower bound of the problem. */
    void update_lower_bound(
            double lower_bound,
            const std::string& s);

    /** Insert jobs one by one at their best positions in a sequence. */
    void insert_jobs(
            std::vector<JobId>& sequence,
            const std::vector<JobId>& job_ids) const;

    /**
     * Initial upper bound (Tanaka and Fujikuma, 2012, Section 6.3): best of
     * the greedy insertions in SPT, LPT, EDD and LDD orders, improved by
     * local search.
     */
    void initial_upper_bound();

    /**
     * Lagrangian heuristic (Section 3.3): remove the duplicated jobs of the
     * path, insert the missing jobs, and improve the solution by local
     * search.
     */
    void lagrangian_heuristic(
            const std::vector<JobId>& path,
            Direction direction);

    /*
     * Stage 1.
     */

    /**
     * Call the dynamic programming algorithm, with the timer and the memory
     * limit of the parameters.
     */
    DynamicProgrammingTimeIndexedOutput dynamic_programming(
            DynamicProgrammingTimeIndexedParameters parameters) const
    {
        parameters.verbosity_level = 0;
        parameters.timer = parameters_.timer;
        parameters.maximum_memory = parameters_.maximum_memory;
        return dynamic_programming_time_indexed(instance_, parameters);
    }

    /**
     * Stage 1: the multipliers, by column generation or Lagrangian
     * relaxation (conjugate subgradient).
     */
    void stage_1();

    /**
     * Callback of each evaluation of the relaxation in stage 1: upper bound
     * from the path, lower bound, Lagrangian heuristic every five iterations
     * if the best lower bound has been updated.
     */
    Time stage_1_iteration(const LagrangianDualIteration& iteration);

    /*
     * Stage 2.
     */

    Status subprocedure(
            const std::shared_ptr<Network>& network,
            Time& tentative_upper_bound);

    Status solve_branch(
            const std::shared_ptr<Network>& network,
            Time& tentative_upper_bound,
            Counter forced_depth,
            Counter depth);

    /*
     * Attributes.
     */

    const Instance& instance_;

    const SuccessiveSublimationDynamicProgrammingParameters& parameters_;

    SuccessiveSublimationDynamicProgrammingOutput& output_;

    AlgorithmFormatter& algorithm_formatter_;

    double maximum_memory_;

    std::shared_ptr<const Problem> problem_;

    /** Network of stage 1. */
    std::shared_ptr<Network> network_;


    std::mt19937_64 generator_ = std::mt19937_64(0);

    Time upper_bound_ = std::numeric_limits<Time>::max();

    Time lower_bound_ = 0;

    /** Multipliers and lower bound of stage 1. */
    std::vector<double> multipliers_;

    double sum_of_multipliers_ = 0;

    double best_lower_bound_ = -INF;

    std::vector<double> best_multipliers_;


    /** Stage 1: the Lagrangian heuristic is to be applied. */
    bool heuristic_pending_ = false;

    /** Stage 1: iteration of the last call to the Lagrangian heuristic. */
    Counter last_heuristic_iteration_ = -5;

    /** Largest depth of the interval branching for the current tentative upper bound. */
    Counter branching_depth_ = 0;

};

void SuccessiveSublimationDynamicProgramming::update_upper_bound(
        const Solution& solution,
        const std::string& s)
{
    if (!solution.feasible())
        return;
    Time value = (Time)solution.objective_value();
    if (value >= upper_bound_)
        return;
    upper_bound_ = value;
    algorithm_formatter_.update_solution(solution, s);
}

void SuccessiveSublimationDynamicProgramming::update_lower_bound(
        double lower_bound,
        const std::string& s)
{
    if (lower_bound == INF) {
        lower_bound_ = upper_bound_;
    } else {
        Time bound = (Time)std::ceil(lower_bound - TOLERANCE);
        // A lower bound larger than the upper bound proves its optimality.
        bound = (std::min)(bound, upper_bound_);
        if (bound <= lower_bound_)
            return;
        lower_bound_ = bound;
    }
    if (problem_->tardiness) {
        algorithm_formatter_.update_total_tardiness_bound(lower_bound_, s);
    } else {
        algorithm_formatter_.update_total_flow_time_bound(lower_bound_, s);
    }
}

void SuccessiveSublimationDynamicProgramming::insert_jobs(
        std::vector<JobId>& sequence,
        const std::vector<JobId>& job_ids) const
{
    for (JobId job_id: job_ids) {
        Time cost_best = std::numeric_limits<Time>::max();
        JobId pos_best = 0;
        for (JobId pos = 0; pos <= (JobId)sequence.size(); ++pos) {
            sequence.insert(sequence.begin() + pos, job_id);
            Time cost = problem_->sequence_cost(sequence);
            sequence.erase(sequence.begin() + pos);
            if (cost < cost_best) {
                cost_best = cost;
                pos_best = pos;
            }
        }
        sequence.insert(sequence.begin() + pos_best, job_id);
    }
}

void SuccessiveSublimationDynamicProgramming::lagrangian_heuristic(
        const std::vector<JobId>& path,
        Direction direction)
{
    JobId n = problem_->number_of_jobs;

    // Remove the duplicated jobs: keep the first occurrence for a forward
    // path, the last one for a backward path.
    std::vector<bool> scheduled(n, false);
    std::vector<JobId> sequence;
    if (direction == Direction::Forward) {
        for (JobId job_id: path) {
            if (!scheduled[job_id]) {
                scheduled[job_id] = true;
                sequence.push_back(job_id);
            }
        }
    } else {
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            if (!scheduled[*it]) {
                scheduled[*it] = true;
                sequence.push_back(*it);
            }
        }
        std::reverse(sequence.begin(), sequence.end());
    }

    // Insert the missing jobs in SPT order at their best positions.
    std::vector<JobId> missing_job_ids;
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (!scheduled[job_id])
            missing_job_ids.push_back(job_id);
    std::stable_sort(missing_job_ids.begin(), missing_job_ids.end(),
            [this](JobId job_id_1, JobId job_id_2)
            {
                return problem_->processing_times[job_id_1] < problem_->processing_times[job_id_2];
            });
    insert_jobs(sequence, missing_job_ids);

    Solution solution = build_solution(sequence);
    update_upper_bound(solution, "lagrangian heuristic");
    if (optimal()
            || parameters_.timer.needs_to_end()
            || !parameters_.lagrangian_heuristic_local_search) {
        return;
    }

    // Local search.
    LocalSearchParameters local_search_parameters;
    local_search_parameters.verbosity_level = 0;
    local_search_parameters.timer.set_time_limit(parameters_.timer.remaining_time());
    local_search_parameters.maximum_number_of_restarts = 1;
    local_search_parameters.maximum_number_of_perturbations_without_improvement
        = parameters_.local_search_number_of_perturbations;
    LocalSearchOutput local_search_output = local_search(
            instance_,
            generator_,
            &solution,
            local_search_parameters);
    update_upper_bound(local_search_output.solution, "local search");
}

void SuccessiveSublimationDynamicProgramming::initial_upper_bound()
{
    JobId n = problem_->number_of_jobs;
    std::vector<JobId> job_ids(n);
    std::iota(job_ids.begin(), job_ids.end(), 0);
    std::vector<std::vector<JobId>> orders;
    auto add_order = [&](const std::function<bool(JobId, JobId)>& compare)
    {
        std::vector<JobId> order = job_ids;
        std::stable_sort(order.begin(), order.end(), compare);
        orders.push_back(order);
    };
    const std::vector<Time>& p = problem_->processing_times;
    const std::vector<Time>& d = problem_->due_dates;
    add_order([&p](JobId j1, JobId j2) { return p[j1] < p[j2]; });
    add_order([&p](JobId j1, JobId j2) { return p[j1] > p[j2]; });
    if (problem_->tardiness) {
        add_order([&d](JobId j1, JobId j2) { return d[j1] < d[j2]; });
        add_order([&d](JobId j1, JobId j2) { return d[j1] > d[j2]; });
    }
    Solution solution_best = build_solution(job_ids);
    for (const std::vector<JobId>& order: orders) {
        std::vector<JobId> sequence;
        insert_jobs(sequence, order);
        Solution solution = build_solution(sequence);
        if (solution.objective_value() < solution_best.objective_value())
            solution_best = solution;
    }
    update_upper_bound(solution_best, "greedy");
    if (optimal()
            || parameters_.timer.needs_to_end()
            || parameters_.initial_local_search_number_of_restarts <= 0) {
        return;
    }

    LocalSearchParameters local_search_parameters;
    local_search_parameters.verbosity_level = 0;
    local_search_parameters.timer.set_time_limit(parameters_.timer.remaining_time());
    local_search_parameters.maximum_number_of_restarts
        = parameters_.initial_local_search_number_of_restarts;
    LocalSearchOutput local_search_output = local_search(
            instance_,
            generator_,
            &solution_best,
            local_search_parameters);
    update_upper_bound(local_search_output.solution, "initial local search");
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Stage 1 ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Time SuccessiveSublimationDynamicProgramming::stage_1_iteration(
        const LagrangianDualIteration& iteration)
{
    output_.number_of_stage_1_iterations = iteration.iteration;
    // A path visiting each job exactly once is a feasible solution.
    if (iteration.feasible)
        update_upper_bound(build_solution(iteration.paths[0]), "relaxation");
    if (iteration.improved) {
        update_lower_bound(iteration.lagrangian_bound, "stage 1");
        algorithm_formatter_.print(
                "stage 1 it " + std::to_string(iteration.iteration)
                + " lb " + std::to_string(iteration.lagrangian_bound)
                + " states " + std::to_string(network_->number_of_states()));
        heuristic_pending_ = true;
    }
    // Lagrangian heuristic every five iterations, if the best lower bound
    // has been updated.
    if (heuristic_pending_
            && !iteration.feasible
            && !optimal()
            && iteration.iteration >= last_heuristic_iteration_ + 5) {
        heuristic_pending_ = false;
        last_heuristic_iteration_ = iteration.iteration;
        lagrangian_heuristic(iteration.paths[0], Direction::Forward);
    }
    return upper_bound_;
}

void SuccessiveSublimationDynamicProgramming::stage_1()
{
#if defined(HIGHS_FOUND) || defined(CLP_FOUND)
    bool use_column_generation = parameters_.column_generation;
#else
    // Without LP solver, the conjugate subgradient algorithm is used.
    bool use_column_generation = false;
#endif
    auto set_parameters = [this](LagrangianDualParameters& parameters)
    {
        parameters.verbosity_level = 0;
        parameters.timer = parameters_.timer;
        parameters.network = network_;
        parameters.upper_bound = upper_bound_;
        parameters.constraint_propagation = parameters_.constraint_propagation;
        parameters.maximum_number_of_iterations = parameters_.maximum_number_of_stage_1_iterations;
        parameters.iteration_callback = [this](const LagrangianDualIteration& iteration)
        {
            return stage_1_iteration(iteration);
        };
    };
    LagrangianDualOutput output(instance_);
    if (use_column_generation) {
        ColumnGenerationParameters parameters;
        set_parameters(parameters);
        output = column_generation(instance_, parameters);
    } else {
        LagrangianRelaxationParameters parameters;
        set_parameters(parameters);
        parameters.initial_step_size = parameters_.initial_step_size;
        parameters.termination_iterations = parameters_.termination_iterations;
        parameters.shrink_iterations = parameters_.shrink_iterations;
        parameters.termination_epsilon = parameters_.termination_epsilon;
        parameters.shrink_factor = parameters_.shrink_factor;
        parameters.expand_factor = parameters_.expand_factor;
        output = lagrangian_relaxation(instance_, parameters);
    }
    best_lower_bound_ = output.lagrangian_bound;
    best_multipliers_ = output.multipliers;
    output_.maximum_number_of_states = (std::max)(
            output_.maximum_number_of_states,
            network_->number_of_states());
    // No schedule better than the upper bound.
    if (output.lagrangian_bound == INF
            || (network_->number_of_states() == 0
                && network_->empty_path_length(best_multipliers_) == INF)) {
        update_lower_bound(INF, "stage 1");
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Stage 2 ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Status SuccessiveSublimationDynamicProgramming::subprocedure(
        const std::shared_ptr<Network>& network,
        Time& tentative_upper_bound)
{
    JobId n = problem_->number_of_jobs;
    const std::vector<double>& multipliers = multipliers_;
    output_.number_of_subprocedures++;

    // The network has the forward and backward labels of the multipliers.
    double lower_bound = network->shortest_path_length(Direction::Forward, multipliers)
        + sum_of_multipliers_;
    double best_lower_bound = lower_bound;
    std::vector<JobId> path = network->shortest_path(Direction::Forward, multipliers);

    // Strategy for the choice of the modifiers: (S1) if the memory usage is
    // small, (S2) otherwise.
    bool strategy_1 = (network->memory() / maximum_memory_ < std::pow(2.0, -6));

    std::shared_ptr<Network> network_old = network;
    Direction direction = Direction::Forward;
    Mask modifiers;
    JobId number_of_modifiers = 0;

    for (;;) {
        if ((double)tentative_upper_bound - lower_bound < 1 - TOLERANCE)
            return Status::Completed;
        if (parameters_.timer.needs_to_end())
            return Status::TimeOut;
        if (number_of_modifiers == n) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": "
                    "gap with all the jobs in M.");
        }

        // Number of jobs to add, from the memory usage.
        double memory_ratio = network_old->memory() / maximum_memory_;
        JobId number_of_new_modifiers = 3;
        if (memory_ratio > std::pow(2.0, -2)) {
            number_of_new_modifiers = 1;
        } else if (memory_ratio > std::pow(2.0, -3)) {
            number_of_new_modifiers = 2;
        }

        // Choice of the jobs.
        std::vector<Counter> number_of_states = network_old->number_of_states_by_job();
        std::vector<JobId> occurrences(n, 0);
        for (JobId job_id: path)
            occurrences[job_id]++;
        auto by_number_of_states = [&number_of_states](JobId job_id_1, JobId job_id_2)
        {
            return number_of_states[job_id_1] < number_of_states[job_id_2];
        };
        std::vector<JobId> selected_job_ids;
        if (strategy_1) {
            std::vector<JobId> job_ids;
            for (JobId job_id = 0; job_id < n; ++job_id)
                if (!modifiers.get(job_id) && occurrences[job_id] == 0)
                    job_ids.push_back(job_id);
            std::stable_sort(job_ids.begin(), job_ids.end(), by_number_of_states);
            for (JobId job_id: job_ids)
                if ((JobId)selected_job_ids.size() < number_of_new_modifiers)
                    selected_job_ids.push_back(job_id);
            job_ids.clear();
            for (JobId job_id = 0; job_id < n; ++job_id)
                if (!modifiers.get(job_id) && occurrences[job_id] > 1)
                    job_ids.push_back(job_id);
            std::stable_sort(job_ids.begin(), job_ids.end(), by_number_of_states);
            for (JobId job_id: job_ids)
                if ((JobId)selected_job_ids.size() < number_of_new_modifiers)
                    selected_job_ids.push_back(job_id);
        }
        if (selected_job_ids.empty()) {
            // Strategy (S2), or no job violates the constraints in the path.
            std::vector<JobId> job_ids;
            for (JobId job_id = 0; job_id < n; ++job_id)
                if (!modifiers.get(job_id))
                    job_ids.push_back(job_id);
            std::stable_sort(job_ids.begin(), job_ids.end(), by_number_of_states);
            for (JobId job_id: job_ids)
                if ((JobId)selected_job_ids.size() < number_of_new_modifiers)
                    selected_job_ids.push_back(job_id);
        }
        Mask new_modifiers = modifiers;
        for (JobId job_id: selected_job_ids)
            new_modifiers.set(job_id);

        // Generate the new network.
        DynamicProgrammingTimeIndexedParameters dp_parameters;
        dp_parameters.network = network_old;
        dp_parameters.multipliers = multipliers;
        dp_parameters.new_modifier_jobs = selected_job_ids;
        dp_parameters.direction = direction;
        dp_parameters.cutoff = tentative_upper_bound - 1 + TOLERANCE;
        dp_parameters.constraint_propagation = parameters_.constraint_propagation;
        DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming(dp_parameters);
        if (dp_output.memory_out)
            return Status::MemoryOut;
        if (parameters_.timer.needs_to_end())
            return Status::TimeOut;
        std::shared_ptr<Network> network_new = dp_output.network;
        modifiers = new_modifiers;
        number_of_modifiers += selected_job_ids.size();
        output_.maximum_number_of_modifiers = (std::max)(
                output_.maximum_number_of_modifiers,
                number_of_modifiers);
        output_.maximum_number_of_states = (std::max)(
                output_.maximum_number_of_states,
                network_new->number_of_states());

        lower_bound = dp_output.lagrangian_bound;
        path = dp_output.path;
        algorithm_formatter_.print(
                "m " + std::to_string(number_of_modifiers)
                + " states " + std::to_string(network_new->number_of_states())
                + " lb " + std::to_string(lower_bound)
                + " ubt " + std::to_string(tentative_upper_bound));
        // A path visiting each job exactly once is a feasible solution; it is
        // always the case when all the jobs are in M.
        std::vector<JobId> path_occurrences(n, 0);
        for (JobId job_id: path)
            path_occurrences[job_id]++;
        bool path_feasible = !path.empty();
        for (JobId job_id = 0; job_id < n; ++job_id)
            if (path_occurrences[job_id] != 1)
                path_feasible = false;
        if (path_feasible) {
            update_upper_bound(build_solution(path), "relaxation");
        } else if (lower_bound > best_lower_bound + TOLERANCE && !path.empty()) {
            lagrangian_heuristic(path, direction);
        }
        best_lower_bound = (std::max)(best_lower_bound, lower_bound);
        tentative_upper_bound = (std::min)(tentative_upper_bound, upper_bound_);

        network_old = network_new;
        direction = (direction == Direction::Forward)? Direction::Backward: Direction::Forward;
    }
}

Status SuccessiveSublimationDynamicProgramming::solve_branch(
        const std::shared_ptr<Network>& network,
        Time& tentative_upper_bound,
        Counter forced_depth,
        Counter depth)
{
    bool subprocedure_run = false;
    if (depth >= forced_depth) {
        Status status = subprocedure(network, tentative_upper_bound);
        if (status != Status::MemoryOut || !parameters_.interval_branching)
            return status;
        subprocedure_run = true;
    }

    // Interval branching (Section 5.4): split the completion times of the
    // two jobs with the most states into two intervals with balanced
    // numbers of states.
    JobId n = problem_->number_of_jobs;
    std::vector<Counter> number_of_states = network->number_of_states_by_job();
    std::vector<Time> earliest_completion_times;
    std::vector<Time> latest_completion_times;
    network->time_windows(earliest_completion_times, latest_completion_times);
    std::vector<JobId> job_ids;
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (earliest_completion_times[job_id] < latest_completion_times[job_id])
            job_ids.push_back(job_id);
    if (job_ids.empty()) {
        // No job to branch on.
        if (subprocedure_run)
            return Status::MemoryOut;
        return subprocedure(network, tentative_upper_bound);
    }
    std::stable_sort(job_ids.begin(), job_ids.end(),
            [&number_of_states](JobId job_id_1, JobId job_id_2)
            {
                return number_of_states[job_id_1] > number_of_states[job_id_2];
            });
    if (job_ids.size() > 2)
        job_ids.resize(2);

    // Split time of each job: the numbers of states of the job completed at
    // t <= c and at t > c are balanced.
    std::vector<Time> splits;
    for (JobId job_id: job_ids) {
        std::vector<Counter> counts = network->number_of_states_by_time(job_id);
        Counter total = number_of_states[job_id];
        Counter sum = 0;
        Time split = earliest_completion_times[job_id];
        for (Time t = earliest_completion_times[job_id]; t < latest_completion_times[job_id]; ++t) {
            sum += counts[t];
            split = t;
            if (2 * sum >= total)
                break;
        }
        splits.push_back(split);
    }

    output_.maximum_branching_depth = (std::max)(
            output_.maximum_branching_depth,
            depth + 1);
    branching_depth_ = (std::max)(branching_depth_, depth + 1);
    algorithm_formatter_.print("branching depth " + std::to_string(depth + 1));

    Counter number_of_children = (Counter)1 << job_ids.size();
    for (Counter child = 0; child < number_of_children; ++child) {
        if (parameters_.timer.needs_to_end())
            return Status::TimeOut;
        DynamicProgrammingTimeIndexedParameters dp_parameters;
        dp_parameters.network = std::make_shared<Network>(*network);
        dp_parameters.multipliers = multipliers_;
        for (size_t pos = 0; pos < job_ids.size(); ++pos) {
            JobId job_id = job_ids[pos];
            if ((child >> pos) & 1) {
                dp_parameters.completion_time_windows.push_back({
                        job_id,
                        splits[pos] + 1,
                        latest_completion_times[job_id]});
            } else {
                dp_parameters.completion_time_windows.push_back({
                        job_id,
                        earliest_completion_times[job_id],
                        splits[pos]});
            }
        }
        dp_parameters.cutoff = tentative_upper_bound - 1 + TOLERANCE;
        dp_parameters.reduction = true;
        dp_parameters.constraint_propagation = parameters_.constraint_propagation;
        dp_parameters.both_directions_labels = true;
        DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming(dp_parameters);
        std::shared_ptr<Network> network_child = dp_output.network;
        if (network_child->number_of_states() == 0)
            continue;
        Status status = solve_branch(
                network_child,
                tentative_upper_bound,
                forced_depth,
                depth + 1);
        if (status != Status::Completed)
            return status;
    }
    return Status::Completed;
}

void SuccessiveSublimationDynamicProgramming::run()
{
    problem_ = build_problem(instance_);
    output_.number_of_removed_jobs = problem_->removed_job_ids.size();
    JobId n = problem_->number_of_jobs;

    initial_upper_bound();
    if (n == 0) {
        update_upper_bound(build_solution({}), "");
        update_lower_bound(0, "");
        return;
    }
    if (end())
        return;

    // Stage 1.
    output_.stage = 1;
    network_ = std::make_shared<Network>(Network::initial_network(problem_));
    output_.maximum_number_of_states = network_->number_of_states();
    algorithm_formatter_.print(
            "stage 1 states " + std::to_string(network_->number_of_states()));
    stage_1();
    if (end())
        return;
    multipliers_ = best_multipliers_;
    sum_of_multipliers_ = std::accumulate(multipliers_.begin(), multipliers_.end(), 0.0);

    // Network reduction with the multipliers of stage 1, then the labels of
    // both directions for stage 2.
    DynamicProgrammingTimeIndexedParameters dp_parameters;
    dp_parameters.network = network_;
    dp_parameters.multipliers = multipliers_;
    dp_parameters.cutoff = upper_bound_ - 1 + TOLERANCE;
    dp_parameters.reduction = true;
    dp_parameters.constraint_propagation = parameters_.constraint_propagation;
    dp_parameters.both_directions_labels = true;
    DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming(dp_parameters);
    output_.maximum_number_of_states = (std::max)(
            output_.maximum_number_of_states,
            network_->number_of_states());
    if (network_->number_of_states() == 0) {
        update_lower_bound(INF, "stage 1");
        return;
    }
    double stage_1_lower_bound = dp_output.lagrangian_bound;
    stage_1_lower_bound = (std::max)(stage_1_lower_bound, best_lower_bound_);
    output_.stage_1_lower_bound = stage_1_lower_bound;
    update_lower_bound(stage_1_lower_bound, "stage 1");
    algorithm_formatter_.print(
            "stage 1 lb " + std::to_string(stage_1_lower_bound)
            + " states " + std::to_string(network_->number_of_states()));
    if (end())
        return;

    // Stage 2: subprocedures with increasing tentative upper bounds.
    output_.stage = 2;
    Time increment = (Time)std::llround(parameters_.tentative_upper_bound_increment * upper_bound_);
    if (std::ceil(stage_1_lower_bound - TOLERANCE) <= 0)
        increment = 1;
    increment = (std::max)(increment, (Time)1);
    Counter forced_depth = parameters_.minimum_branching_depth;
    for (Counter k = 1; !end(); ++k) {
        Time tentative_upper_bound = (Time)std::llround(stage_1_lower_bound + k * increment);
        tentative_upper_bound = (std::min)(tentative_upper_bound, upper_bound_);
        if (tentative_upper_bound <= lower_bound_)
            continue;
        branching_depth_ = 0;
        Status status = solve_branch(network_, tentative_upper_bound, forced_depth, 0);
        if (status != Status::Completed)
            return;
        // No solution better than the tentative upper bound.
        update_lower_bound(tentative_upper_bound, "stage 2");
        forced_depth = (std::max)(branching_depth_, parameters_.minimum_branching_depth);
    }
}

}

const SuccessiveSublimationDynamicProgrammingOutput parallelschedulingsolver::successive_sublimation_dynamic_programming(
        const Instance& instance,
        const SuccessiveSublimationDynamicProgrammingParameters& parameters)
{
    SuccessiveSublimationDynamicProgrammingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Successive sublimation dynamic programming");

    if (instance.has_rejection_costs()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "rejection costs are not supported.");
    }

    if (instance.number_of_machines() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "only single machine instances are supported.");
    }
    if (instance.objective() != Objective::TotalTardiness
            && instance.objective() != Objective::TotalFlowTime) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "only the total tardiness and the total flow time are supported.");
    }
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        if (job.release_date != 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "release dates are not supported.");
        }
        if (job.processing_times[0] <= 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "processing times must be positive.");
        }
    }

    algorithm_formatter.print_header();

    SuccessiveSublimationDynamicProgramming algorithm(
            instance,
            parameters,
            output,
            algorithm_formatter);
    algorithm.run();

    algorithm_formatter.end();
    return output;
}
