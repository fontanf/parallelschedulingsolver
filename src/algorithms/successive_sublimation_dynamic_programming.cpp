#include "parallelschedulingsolver/algorithms/successive_sublimation_dynamic_programming.hpp"

#include "parallelschedulingsolver/algorithms/local_search.hpp"

#include "ssdp_network.hpp"
#include "hungarian.hpp"

#include "columngenerationsolver/algorithms/column_generation.hpp"


#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <numeric>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::ssdp;

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

    /**
     * Build the problem: remove the zero cost jobs that can be moved to the
     * end of any schedule (Section 5.1), and compute the bounds of the
     * makespan.
     */
    void build_problem();

    /*
     * Bounds.
     */

    bool optimal() const { return lower_bound_ >= upper_bound_; }

    bool end() const { return optimal() || parameters_.timer.needs_to_end(); }

    /** Solution of the instance from a sequence of jobs of the problem. */
    Solution build_solution(const std::vector<JobId>& sequence) const;

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

    /** Reduce the network with the forward labels of the multipliers. */
    void reduce(
            Network& network,
            const std::vector<double>& multipliers,
            double threshold);

    void stage_1_conjugate_subgradient(Network& network);

    void stage_1_column_generation(Network& network);

    /**
     * Process the result of the dynamic programming for some multipliers:
     * update the best lower bound, the multipliers of the best lower bound
     * and the upper bound, and reduce the network if one of them is
     * improved. Return true if the best lower bound is improved.
     */
    bool process_stage_1_iteration(
            Network& network,
            const std::vector<double>& multipliers,
            double lower_bound,
            const std::vector<JobId>& path);

    /*
     * Stage 2.
     */

    Status subprocedure(
            const Network& network,
            Time& tentative_upper_bound);

    Status solve_branch(
            const Network& network,
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

    Problem problem_;

    /** Jobs of the instance removed in preprocessing, scheduled last. */
    std::vector<JobId> removed_job_ids_;

    std::mt19937_64 generator_ = std::mt19937_64(0);

    Time upper_bound_ = std::numeric_limits<Time>::max();

    Time lower_bound_ = 0;

    /** Multipliers and lower bound of stage 1. */
    std::vector<double> multipliers_;

    double sum_of_multipliers_ = 0;

    double best_lower_bound_ = -INF;

    std::vector<double> best_multipliers_;

    /** Upper bound at the last network reduction of stage 1. */
    Time reduction_upper_bound_ = std::numeric_limits<Time>::max();

    /** Stage 1: the Lagrangian heuristic is to be applied. */
    bool heuristic_pending_ = false;

    /** Stage 1: iteration of the last call to the Lagrangian heuristic. */
    Counter last_heuristic_iteration_ = -5;

    /** Largest depth of the interval branching for the current tentative upper bound. */
    Counter branching_depth_ = 0;

};

void SuccessiveSublimationDynamicProgramming::build_problem()
{
    JobId n = instance_.number_of_jobs();
    problem_.tardiness = (instance_.objective() == Objective::TotalTardiness);

    auto zero_cost = [this](JobId job_id)
    {
        const Job& job = instance_.job(job_id);
        if (job.weight == 0)
            return true;
        return problem_.tardiness && job.due_date < 0;
    };

    // Remove the zero cost jobs satisfying (40), one at a time.
    std::vector<bool> removed(n, false);
    for (bool changed = true; changed;) {
        changed = false;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (removed[job_id] || !zero_cost(job_id))
                continue;
            Time p = instance_.job(job_id).processing_times[0];
            bool ok = true;
            for (JobId job_id_1 = -1; job_id_1 < n && ok; ++job_id_1) {
                if (job_id_1 == job_id || (job_id_1 >= 0 && removed[job_id_1]))
                    continue;
                for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2) {
                    if (job_id_2 == job_id || job_id_2 == job_id_1 || removed[job_id_2])
                        continue;
                    if (instance_.setup_time(0, job_id_1, job_id)
                            + p
                            + instance_.setup_time(0, job_id, job_id_2)
                            < instance_.setup_time(0, job_id_1, job_id_2)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (ok) {
                removed[job_id] = true;
                removed_job_ids_.push_back(job_id);
                changed = true;
            }
        }
    }
    // The last removed job is scheduled first among the removed jobs.
    std::reverse(removed_job_ids_.begin(), removed_job_ids_.end());
    output_.number_of_removed_jobs = removed_job_ids_.size();

    for (JobId job_id = 0; job_id < n; ++job_id) {
        if (removed[job_id])
            continue;
        const Job& job = instance_.job(job_id);
        problem_.instance_job_ids.push_back(job_id);
        problem_.processing_times.push_back(job.processing_times[0]);
        problem_.weights.push_back(job.weight);
        problem_.due_dates.push_back(job.due_date);
    }
    JobId m = problem_.instance_job_ids.size();
    problem_.number_of_jobs = m;
    if (m > 127) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "at most 127 jobs are supported; "
                "number of jobs: " + std::to_string(m) + ".");
    }
    problem_.setup_times.assign((m + 1) * m, 0);
    for (JobId job_id_1 = -1; job_id_1 < m; ++job_id_1) {
        for (JobId job_id_2 = 0; job_id_2 < m; ++job_id_2) {
            if (job_id_1 == job_id_2)
                continue;
            problem_.setup_times[(job_id_1 + 1) * m + job_id_2] = instance_.setup_time(
                    0,
                    (job_id_1 == -1)? -1: problem_.instance_job_ids[job_id_1],
                    problem_.instance_job_ids[job_id_2]);
        }
    }
    problem_.extended_processing_times.resize(m);
    for (JobId job_id = 0; job_id < m; ++job_id) {
        Time setup_min = problem_.setup_time(-1, job_id);
        for (JobId job_id_1 = 0; job_id_1 < m; ++job_id_1)
            if (job_id_1 != job_id)
                setup_min = (std::min)(setup_min, problem_.setup_time(job_id_1, job_id));
        problem_.extended_processing_times[job_id] = problem_.processing_times[job_id] + setup_min;
    }

    // Bounds of the makespan from the assignment relaxation of the
    // asymmetric traveling salesman problem, with a dummy node 0.
    Time sum_of_processing_times = std::accumulate(
            problem_.processing_times.begin(),
            problem_.processing_times.end(),
            (Time)0);
    const Time forbidden = 1e15;
    std::vector<std::vector<Time>> costs_min(m + 1, std::vector<Time>(m + 1, 0));
    std::vector<std::vector<Time>> costs_max(m + 1, std::vector<Time>(m + 1, 0));
    for (JobId a = 0; a <= m; ++a) {
        for (JobId b = 0; b <= m; ++b) {
            Time setup = 0;
            if (a == b) {
                costs_min[a][b] = forbidden;
                costs_max[a][b] = forbidden;
                continue;
            }
            if (b > 0)
                setup = problem_.setup_time(a - 1, b - 1);
            costs_min[a][b] = setup;
            costs_max[a][b] = -setup;
        }
    }
    Time setups_min = 0;
    Time setups_max = 0;
    if (m > 0) {
        std::vector<int64_t> assignment_min = hungarian(costs_min, m + 1, m + 1);
        std::vector<int64_t> assignment_max = hungarian(costs_max, m + 1, m + 1);
        for (JobId a = 0; a <= m; ++a) {
            setups_min += costs_min[a][assignment_min[a]];
            setups_max -= costs_max[a][assignment_max[a]];
        }
    }
    problem_.minimum_makespan = sum_of_processing_times + setups_min;
    problem_.maximum_makespan = sum_of_processing_times + setups_max;
}

Solution SuccessiveSublimationDynamicProgramming::build_solution(
        const std::vector<JobId>& sequence) const
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance_);
    Time t = 0;
    JobId job_id_prev = -1;
    auto append = [&](JobId job_id)
    {
        Time start = t + instance_.setup_time(0, job_id_prev, job_id);
        solution_builder.append_job_to_machine(0, job_id, start);
        t = start + instance_.job(job_id).processing_times[0];
        job_id_prev = job_id;
    };
    for (JobId job_id: sequence)
        append(problem_.instance_job_ids[job_id]);
    for (JobId job_id: removed_job_ids_)
        append(job_id);
    return solution_builder.build();
}

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
    if (problem_.tardiness) {
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
            Time cost = problem_.sequence_cost(sequence);
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
    JobId n = problem_.number_of_jobs;

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
                return problem_.processing_times[job_id_1] < problem_.processing_times[job_id_2];
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
    JobId n = problem_.number_of_jobs;
    std::vector<JobId> job_ids(n);
    std::iota(job_ids.begin(), job_ids.end(), 0);
    std::vector<std::vector<JobId>> orders;
    auto add_order = [&](const std::function<bool(JobId, JobId)>& compare)
    {
        std::vector<JobId> order = job_ids;
        std::stable_sort(order.begin(), order.end(), compare);
        orders.push_back(order);
    };
    const std::vector<Time>& p = problem_.processing_times;
    const std::vector<Time>& d = problem_.due_dates;
    add_order([&p](JobId j1, JobId j2) { return p[j1] < p[j2]; });
    add_order([&p](JobId j1, JobId j2) { return p[j1] > p[j2]; });
    if (problem_.tardiness) {
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

void SuccessiveSublimationDynamicProgramming::reduce(
        Network& network,
        const std::vector<double>& multipliers,
        double threshold)
{
    double sum_of_multipliers = std::accumulate(multipliers.begin(), multipliers.end(), 0.0);
    network.compute_backward_labels(multipliers);
    network.reduce(multipliers, sum_of_multipliers, threshold);
    if (parameters_.constraint_propagation)
        network.propagate_constraints();
    output_.maximum_number_of_states = (std::max)(
            output_.maximum_number_of_states,
            network.number_of_states());
}

bool SuccessiveSublimationDynamicProgramming::process_stage_1_iteration(
        Network& network,
        const std::vector<double>& multipliers,
        double lower_bound,
        const std::vector<JobId>& path)
{
    output_.number_of_stage_1_iterations++;

    // A path visiting each job exactly once is a feasible solution.
    std::vector<JobId> occurrences(problem_.number_of_jobs, 0);
    for (JobId job_id: path)
        occurrences[job_id]++;
    bool feasible = true;
    for (JobId job_id = 0; job_id < problem_.number_of_jobs; ++job_id)
        if (occurrences[job_id] != 1)
            feasible = false;
    if (feasible)
        update_upper_bound(build_solution(path), "relaxation");

    bool improved = false;
    if (lower_bound > best_lower_bound_ + TOLERANCE) {
        improved = true;
        best_lower_bound_ = lower_bound;
        best_multipliers_ = multipliers;
        update_lower_bound(lower_bound, "stage 1");
        algorithm_formatter_.print(
                "stage 1 it " + std::to_string(output_.number_of_stage_1_iterations)
                + " lb " + std::to_string(lower_bound)
                + " states " + std::to_string(network.number_of_states()));
        heuristic_pending_ = true;
    }
    // Lagrangian heuristic every five iterations, if the best lower bound
    // has been updated.
    if (heuristic_pending_
            && !feasible
            && !optimal()
            && output_.number_of_stage_1_iterations >= last_heuristic_iteration_ + 5) {
        heuristic_pending_ = false;
        last_heuristic_iteration_ = output_.number_of_stage_1_iterations;
        lagrangian_heuristic(path, Direction::Forward);
    }
    if (optimal())
        return improved;

    // Network reduction when the best lower bound or the upper bound is
    // updated, with the forward labels of the current multipliers.
    if (improved || upper_bound_ < reduction_upper_bound_) {
        reduction_upper_bound_ = upper_bound_;
        reduce(network, multipliers, upper_bound_ - 1 + TOLERANCE);
        if (network.number_of_states() == 0)
            update_lower_bound(INF, "network reduction");
    }
    return improved;
}

void SuccessiveSublimationDynamicProgramming::stage_1_conjugate_subgradient(
        Network& network)
{
    JobId n = problem_.number_of_jobs;
    std::vector<double> multipliers(n, 0);
    std::vector<double> direction(n, 0);
    double direction_norm = 0;
    double step_size = parameters_.initial_step_size;
    Counter termination_iterations = (parameters_.termination_iterations >= 0)?
        parameters_.termination_iterations: n;
    Counter iterations_without_improvement = 0;
    double epsilon = parameters_.termination_epsilon;
    std::vector<double> best_lower_bounds;
    std::vector<double> gaps;

    for (Counter iteration = 0;
            iteration < parameters_.maximum_number_of_stage_1_iterations;
            ++iteration) {
        if (end())
            return;

        network.compute_forward_labels(multipliers);
        double sum_of_multipliers = std::accumulate(multipliers.begin(), multipliers.end(), 0.0);
        double length = network.shortest_path_length(Direction::Forward, multipliers);
        if (length == INF) {
            update_lower_bound(INF, "stage 1");
            return;
        }
        double lower_bound = length + sum_of_multipliers;
        std::vector<JobId> path = network.shortest_path(Direction::Forward, multipliers);
        bool improved = process_stage_1_iteration(network, multipliers, lower_bound, path);
        if (end())
            return;

        // Step size.
        if (improved) {
            step_size *= parameters_.expand_factor;
            iterations_without_improvement = 0;
        } else {
            iterations_without_improvement++;
            if (iterations_without_improvement >= parameters_.shrink_iterations) {
                step_size *= parameters_.shrink_factor;
                iterations_without_improvement = 0;
            }
        }

        // Termination criterion.
        best_lower_bounds.push_back(best_lower_bound_);
        gaps.push_back(upper_bound_ - best_lower_bound_);
        if ((Counter)best_lower_bounds.size() > termination_iterations) {
            double lower_bound_old = best_lower_bounds[best_lower_bounds.size() - 1 - termination_iterations];
            double gap_old = gaps[gaps.size() - 1 - termination_iterations];
            if (best_lower_bound_ - lower_bound_old
                    <= std::abs(lower_bound_old) * epsilon / (1 - epsilon)
                    && gap_old - gaps.back() <= epsilon * gap_old) {
                return;
            }
        }

        // Subgradient and conjugate direction.
        std::vector<double> subgradient(n, 1);
        for (JobId job_id: path)
            subgradient[job_id] -= 1;
        double subgradient_norm = 0;
        for (JobId job_id = 0; job_id < n; ++job_id)
            subgradient_norm += subgradient[job_id] * subgradient[job_id];
        subgradient_norm = std::sqrt(subgradient_norm);
        if (subgradient_norm == 0) {
            // The path is feasible: the multipliers are optimal.
            return;
        }
        double ratio = (direction_norm > 0)? subgradient_norm / direction_norm: 0;
        double norm_2 = 0;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            direction[job_id] = subgradient[job_id] + ratio * direction[job_id];
            norm_2 += direction[job_id] * direction[job_id];
        }
        direction_norm = std::sqrt(norm_2);
        double step = step_size * ((double)upper_bound_ - lower_bound) / norm_2;
        for (JobId job_id = 0; job_id < n; ++job_id)
            multipliers[job_id] += step * direction[job_id];
    }
}

namespace
{

/**
 * Pricing solver of the column generation of stage 1: the dynamic
 * programming of the relaxation (LR2).
 *
 * Master problem: min sum_P L(P) x_P s.t. sum_P V_i(P) x_P = 1 for each job
 * i, sum_P x_P = 1. The duals of the job rows are the multipliers.
 */
class Stage1PricingSolver: public columngenerationsolver::PricingSolver
{

public:

    using Evaluate = std::function<std::vector<JobId>(const std::vector<double>&)>;

    Stage1PricingSolver(
            const Problem& problem,
            const Evaluate& evaluate,
            const std::function<bool()>& end):
        problem_(problem),
        evaluate_(evaluate),
        end_(end) { }

    std::shared_ptr<const columngenerationsolver::Column> column(
            const std::vector<JobId>& path) const
    {
        JobId n = problem_.number_of_jobs;
        std::vector<double> occurrences(n, 0);
        for (JobId job_id: path)
            occurrences[job_id] += 1;
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->objective_coefficient = problem_.sequence_cost(path);
        for (JobId job_id = 0; job_id < n; ++job_id)
            if (occurrences[job_id] != 0)
                column->elements.push_back({job_id, occurrences[job_id]});
        column->elements.push_back({n, 1});
        return column;
    }

    virtual PricingOutput solve_pricing(
            bool,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Column>, columngenerationsolver::Value>>&,
            const std::vector<std::shared_ptr<const columngenerationsolver::BranchingDecision>>&,
            const std::unordered_set<std::shared_ptr<const columngenerationsolver::Column>>&,
            const std::vector<columngenerationsolver::Value>& duals,
            const std::vector<std::pair<std::shared_ptr<const columngenerationsolver::Cut>, columngenerationsolver::Value>>&,
            PricingType,
            columngenerationsolver::Value) override
    {
        // The lower bounds are also computed and stored by 'evaluate_'.
        PricingOutput output;
        if (end_())
            return output;
        JobId n = problem_.number_of_jobs;
        std::vector<double> multipliers(duals.begin(), duals.begin() + n);
        std::vector<JobId> path = evaluate_(multipliers);
        if (end_())
            return output;
        output.columns.push_back(column(path));
        // Exact pricing: the reduced cost of the path is the smallest one.
        double reduced_cost = problem_.sequence_cost(path) - duals[n];
        for (JobId job_id: path)
            reduced_cost -= duals[job_id];
        output.overcost = reduced_cost;
        return output;
    }

private:

    const Problem& problem_;

    Evaluate evaluate_;

    std::function<bool()> end_;

};

}

void SuccessiveSublimationDynamicProgramming::stage_1_column_generation(
        Network& network)
{
#if defined(HIGHS_FOUND) || defined(CLP_FOUND)
    JobId n = problem_.number_of_jobs;
    bool infeasible = false;

    // Dynamic programming for some multipliers.
    auto evaluate = [this, &network, &infeasible](
            const std::vector<double>& multipliers)
    {
        network.compute_forward_labels(multipliers);
        double length = network.shortest_path_length(Direction::Forward, multipliers);
        if (length == INF) {
            infeasible = true;
            update_lower_bound(INF, "stage 1");
            return std::vector<JobId>();
        }
        double sum_of_multipliers = std::accumulate(multipliers.begin(), multipliers.end(), 0.0);
        std::vector<JobId> path = network.shortest_path(Direction::Forward, multipliers);
        process_stage_1_iteration(network, multipliers, length + sum_of_multipliers, path);
        return path;
    };
    auto stop = [this, &infeasible]()
    {
        return infeasible
            || end()
            || output_.number_of_stage_1_iterations >= parameters_.maximum_number_of_stage_1_iterations;
    };

    columngenerationsolver::Model model;
    model.objective_sense = optimizationtools::ObjectiveDirection::Minimize;
    for (JobId job_id = 0; job_id <= n; ++job_id) {
        columngenerationsolver::Row row;
        row.lower_bound = 1;
        row.upper_bound = 1;
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = (job_id < n)? n: 1;
        model.rows.push_back(row);
    }
    auto pricing_solver = std::unique_ptr<Stage1PricingSolver>(
            new Stage1PricingSolver(problem_, evaluate, stop));
    Stage1PricingSolver& pricing_solver_ref = *pricing_solver;
    model.pricing_solver = std::move(pricing_solver);

    columngenerationsolver::ColumnGenerationParameters column_generation_parameters;
    column_generation_parameters.verbosity_level = 0;
    column_generation_parameters.timer = parameters_.timer;
#if defined(HIGHS_FOUND)
    column_generation_parameters.solver_name = columngenerationsolver::SolverName::Highs;
#else
    column_generation_parameters.solver_name = columngenerationsolver::SolverName::CLP;
#endif
    // Wentges smoothing with the multipliers of the best lower bound.
    column_generation_parameters.static_wentges_smoothing_parameter = 0.5;
    column_generation_parameters.wentges_smoothing_best_bound_center = true;

    // Initial columns: the best known solution (all the jobs once, with the
    // upper bound as cost; the order of the jobs doesn't matter for the
    // master problem), and the solution of the relaxation for the
    // multipliers equal to 0.
    {
        std::vector<JobId> path(n);
        std::iota(path.begin(), path.end(), 0);
        auto column = pricing_solver_ref.column(path);
        auto column_copy = std::make_shared<columngenerationsolver::Column>(*column);
        column_copy->objective_coefficient = upper_bound_;
        column_generation_parameters.initial_columns.push_back(column_copy);
    }
    {
        std::vector<JobId> path = evaluate(std::vector<double>(n, 0));
        if (end())
            return;
        column_generation_parameters.initial_columns.push_back(pricing_solver_ref.column(path));
    }

    columngenerationsolver::column_generation(model, column_generation_parameters);
#else
    (void)network;
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": "
            "column generation requires an LP solver.");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Stage 2 ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Status SuccessiveSublimationDynamicProgramming::subprocedure(
        const Network& network,
        Time& tentative_upper_bound)
{
    JobId n = problem_.number_of_jobs;
    const std::vector<double>& multipliers = multipliers_;
    output_.number_of_subprocedures++;

    // The network has the forward and backward labels of the multipliers.
    double lower_bound = network.shortest_path_length(Direction::Forward, multipliers)
        + sum_of_multipliers_;
    double best_lower_bound = lower_bound;
    std::vector<JobId> path = network.shortest_path(Direction::Forward, multipliers);

    // Strategy for the choice of the modifiers: (S1) if the memory usage is
    // small, (S2) otherwise.
    bool strategy_1 = (network.memory() / maximum_memory_ < std::pow(2.0, -6));

    const Network* network_old = &network;
    std::unique_ptr<Network> network_old_owned;
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
        auto network_new = std::make_unique<Network>(problem_);
        bool ok = network_old->generate(
                *network_new,
                new_modifiers,
                direction,
                multipliers,
                sum_of_multipliers_,
                tentative_upper_bound - 1 + TOLERANCE,
                maximum_memory_,
                &parameters_.timer);
        if (!ok) {
            if (parameters_.timer.needs_to_end())
                return Status::TimeOut;
            return Status::MemoryOut;
        }
        modifiers = new_modifiers;
        number_of_modifiers += selected_job_ids.size();
        output_.maximum_number_of_modifiers = (std::max)(
                output_.maximum_number_of_modifiers,
                number_of_modifiers);
        output_.maximum_number_of_states = (std::max)(
                output_.maximum_number_of_states,
                network_new->number_of_states());

        if (parameters_.constraint_propagation) {
            Counter number_of_states = network_new->number_of_states();
            network_new->propagate_constraints();
            if (network_new->number_of_states() != number_of_states) {
                if (direction == Direction::Forward) {
                    network_new->compute_forward_labels(multipliers);
                } else {
                    network_new->compute_backward_labels(multipliers);
                }
            }
        }

        lower_bound = network_new->shortest_path_length(direction, multipliers)
            + sum_of_multipliers_;
        path = network_new->shortest_path(direction, multipliers);
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

        network_old_owned = std::move(network_new);
        network_old = network_old_owned.get();
        direction = (direction == Direction::Forward)? Direction::Backward: Direction::Forward;
    }
}

Status SuccessiveSublimationDynamicProgramming::solve_branch(
        const Network& network,
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
    JobId n = problem_.number_of_jobs;
    std::vector<Counter> number_of_states = network.number_of_states_by_job();
    std::vector<Time> earliest_completion_times;
    std::vector<Time> latest_completion_times;
    network.time_windows(earliest_completion_times, latest_completion_times);
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
        std::vector<Counter> counts = network.number_of_states_by_time(job_id);
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
        Network network_child = network;
        for (size_t pos = 0; pos < job_ids.size(); ++pos) {
            JobId job_id = job_ids[pos];
            if ((child >> pos) & 1) {
                network_child.restrict(
                        job_id,
                        splits[pos] + 1,
                        latest_completion_times[job_id]);
            } else {
                network_child.restrict(
                        job_id,
                        earliest_completion_times[job_id],
                        splits[pos]);
            }
        }
        network_child.compute_forward_labels(multipliers_);
        network_child.compute_backward_labels(multipliers_);
        network_child.reduce(
                multipliers_,
                sum_of_multipliers_,
                tentative_upper_bound - 1 + TOLERANCE);
        if (parameters_.constraint_propagation)
            network_child.propagate_constraints();
        if (network_child.number_of_states() == 0)
            continue;
        network_child.compute_forward_labels(multipliers_);
        network_child.compute_backward_labels(multipliers_);
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
    build_problem();
    JobId n = problem_.number_of_jobs;

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
    Network network = Network::initial_network(problem_);
    output_.maximum_number_of_states = network.number_of_states();
    algorithm_formatter_.print(
            "stage 1 states " + std::to_string(network.number_of_states()));
#if defined(HIGHS_FOUND) || defined(CLP_FOUND)
    bool column_generation = parameters_.column_generation;
#else
    // Without LP solver, the conjugate subgradient algorithm is used.
    bool column_generation = false;
#endif
    if (column_generation) {
        stage_1_column_generation(network);
    } else {
        stage_1_conjugate_subgradient(network);
    }
    if (end())
        return;
    multipliers_ = best_multipliers_;
    sum_of_multipliers_ = std::accumulate(multipliers_.begin(), multipliers_.end(), 0.0);

    // Network reduction with the multipliers of stage 1, then the labels of
    // both directions for stage 2.
    network.compute_forward_labels(multipliers_);
    reduce(network, multipliers_, upper_bound_ - 1 + TOLERANCE);
    if (network.number_of_states() == 0) {
        update_lower_bound(INF, "stage 1");
        return;
    }
    network.compute_forward_labels(multipliers_);
    network.compute_backward_labels(multipliers_);
    double stage_1_lower_bound = network.shortest_path_length(Direction::Forward, multipliers_)
        + sum_of_multipliers_;
    stage_1_lower_bound = (std::max)(stage_1_lower_bound, best_lower_bound_);
    output_.stage_1_lower_bound = stage_1_lower_bound;
    update_lower_bound(stage_1_lower_bound, "stage 1");
    algorithm_formatter_.print(
            "stage 1 lb " + std::to_string(stage_1_lower_bound)
            + " states " + std::to_string(network.number_of_states()));
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
        Status status = solve_branch(network, tentative_upper_bound, forced_depth, 0);
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
