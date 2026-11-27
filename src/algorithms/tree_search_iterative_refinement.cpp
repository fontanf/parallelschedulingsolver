#include "parallelschedulingsolver/algorithms/tree_search_iterative_refinement.hpp"

#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"

#include <cmath>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

namespace
{

/** Tolerance on the lower bounds, computed with floating point multipliers. */
constexpr double TOLERANCE = 1e-6;

/**
 * Status of the relaxation refinement of a node: EarlyTermination if it
 * reached the maximum number of steps or tailed off (refine-or-branch rule).
 */
enum class Status { Completed, MemoryOut, EarlyTermination, TimeOut };

class TreeSearchIterativeRefinement
{

public:

    TreeSearchIterativeRefinement(
            const Instance& instance,
            const TreeSearchIterativeRefinementParameters& parameters,
            TreeSearchIterativeRefinementOutput& output,
            AlgorithmFormatter& algorithm_formatter):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        algorithm_formatter_(algorithm_formatter) { }

    void run();

private:

    bool optimal() const { return lower_bound_ >= upper_bound_; }

    bool end() const { return optimal() || parameters_.timer.needs_to_end(); }

    /** Call the dynamic programming with the timer and the memory limit. */
    DynamicProgrammingTimeIndexedOutput dynamic_programming(
            DynamicProgrammingTimeIndexedParameters parameters) const
    {
        parameters.verbosity_level = 0;
        parameters.timer = parameters_.timer;
        parameters.maximum_memory = parameters_.maximum_memory;
        return dynamic_programming_time_indexed(instance_, parameters);
    }

    void update_upper_bound(
            const Solution& solution,
            Time upper_bound)
    {
        if (solution.feasible()
                && (!output_.solution.feasible()
                    || solution.objective_value() < output_.solution.objective_value())) {
            algorithm_formatter_.update_solution(solution, "relaxation refinement");
        }
        if (upper_bound >= 0 && (upper_bound_ < 0 || upper_bound < upper_bound_)) {
            upper_bound_ = upper_bound;
            output_.upper_bound = upper_bound_;
        }
    }

    /** Update the lower bound: no schedule has a value smaller than 'bound'. */
    void update_lower_bound(
            Time bound,
            const std::string& s)
    {
        bound = (std::min)(bound, upper_bound_);
        if (bound <= lower_bound_)
            return;
        lower_bound_ = bound;
        output_.lower_bound = lower_bound_;
        if (network_->restricted())
            return;
        if (network_->problem().tardiness) {
            algorithm_formatter_.update_total_tardiness_bound(lower_bound_, s);
        } else {
            algorithm_formatter_.update_total_flow_time_bound(lower_bound_, s);
        }
    }

    /**
     * Relaxation refinement of a node with a tentative upper bound, with the
     * early termination of the refine-or-branch rule if
     * 'early_termination'.
     */
    Status refine(
            const std::shared_ptr<Network>& network,
            Time& tentative_upper_bound,
            bool early_termination,
            std::shared_ptr<Network>* refined_network = nullptr);

    /**
     * Node of the interval branching: relaxation refinement, then branching
     * if the memory isn't sufficient.
     */
    Status solve_branch(
            const std::shared_ptr<Network>& node_network,
            Time& tentative_upper_bound,
            Counter forced_depth,
            Counter depth);

    const Instance& instance_;

    const TreeSearchIterativeRefinementParameters& parameters_;

    TreeSearchIterativeRefinementOutput& output_;

    AlgorithmFormatter& algorithm_formatter_;

    /** Network of the root. */
    std::shared_ptr<Network> network_;

    std::vector<double> multipliers_;

    Time upper_bound_ = -1;

    Time lower_bound_ = std::numeric_limits<Time>::min();

    /** Largest depth of the interval branching for the current tentative upper bound. */
    Counter branching_depth_ = 0;

};

Status TreeSearchIterativeRefinement::refine(
        const std::shared_ptr<Network>& network,
        Time& tentative_upper_bound,
        bool early_termination,
        std::shared_ptr<Network>* refined_network)
{
    output_.number_of_refinements++;

    // The network has the forward and backward labels of the multipliers.
    RelaxationRefinementParameters refinement_parameters;
    refinement_parameters.verbosity_level = 0;
    refinement_parameters.timer = parameters_.timer;
    refinement_parameters.network = network;
    refinement_parameters.multipliers = multipliers_;
    refinement_parameters.upper_bound = upper_bound_;
    refinement_parameters.bound_to_prove = tentative_upper_bound;
    refinement_parameters.constraint_propagation = parameters_.constraint_propagation;
    refinement_parameters.maximum_memory = parameters_.maximum_memory;
    refinement_parameters.step_callback = parameters_.step_callback;
    if (early_termination) {
        refinement_parameters.maximum_number_of_steps = parameters_.maximum_number_of_refinement_steps;
        refinement_parameters.tailing_off_number_of_steps = parameters_.tailing_off_number_of_steps;
        refinement_parameters.tailing_off_minimum_improvement = parameters_.tailing_off_minimum_improvement;
    }
    RelaxationRefinementOutput refinement_output = relaxation_refinement(
            instance_,
            refinement_parameters);
    output_.maximum_number_of_modifiers = (std::max)(
            output_.maximum_number_of_modifiers,
            refinement_output.number_of_modifiers);
    output_.maximum_number_of_states = (std::max)(
            output_.maximum_number_of_states,
            refinement_output.maximum_number_of_states);
    update_upper_bound(refinement_output.solution, refinement_output.upper_bound);
    tentative_upper_bound = (std::min)(tentative_upper_bound, upper_bound_);
    if (refined_network != nullptr)
        *refined_network = refinement_output.network;
    switch (refinement_output.status) {
    case RelaxationRefinementStatus::BoundProved:
        return Status::Completed;
    case RelaxationRefinementStatus::MemoryOut:
        return Status::MemoryOut;
    case RelaxationRefinementStatus::MaximumNumberOfSteps:
    case RelaxationRefinementStatus::TailingOff:
        return Status::EarlyTermination;
    default:
        return Status::TimeOut;
    }
}

Status TreeSearchIterativeRefinement::solve_branch(
        const std::shared_ptr<Network>& node_network,
        Time& tentative_upper_bound,
        Counter forced_depth,
        Counter depth)
{
    // The early termination of the refine-or-branch rule is only used with
    // interval branching.
    bool early_termination = parameters_.interval_branching;
    Status refinement_status = Status::Completed;
    bool refinement_run = false;
    // Network to branch on: the network of the node, or, with warm start,
    // the last network of its relaxation refinement.
    std::shared_ptr<Network> network = node_network;
    if (depth >= forced_depth) {
        std::shared_ptr<Network> refined_network = nullptr;
        refinement_status = refine(
                node_network,
                tentative_upper_bound,
                early_termination,
                &refined_network);
        if ((refinement_status != Status::MemoryOut
                    && refinement_status != Status::EarlyTermination)
                || !parameters_.interval_branching) {
            return refinement_status;
        }
        refinement_run = true;
        if (parameters_.warm_start && refined_network != nullptr)
            network = refined_network;
    }

    // Interval branching (Tanaka and Araki, 2013, Section 5.4): split the
    // completion times of the two jobs with the most states into two
    // intervals with balanced numbers of states.
    JobId n = network->problem().number_of_jobs;
    std::vector<Counter> number_of_states = network->number_of_states_by_job();
    std::vector<Time> earliest_completion_times;
    std::vector<Time> latest_completion_times;
    network->time_windows(earliest_completion_times, latest_completion_times);
    std::vector<JobId> job_ids;
    for (JobId job_id = 0; job_id < n; ++job_id)
        if (earliest_completion_times[job_id] < latest_completion_times[job_id])
            job_ids.push_back(job_id);
    if (job_ids.empty()) {
        // No job to branch on: the relaxation refinement without early
        // termination.
        if (refinement_run && refinement_status == Status::MemoryOut)
            return Status::MemoryOut;
        return refine(network, tentative_upper_bound, false);
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

void TreeSearchIterativeRefinement::run()
{
    if (instance_.number_of_machines() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "only single machine instances are supported.");
    }
    network_ = parameters_.network;
    if (network_ == nullptr) {
        network_ = std::make_shared<Network>(
                Network::initial_network(build_problem(instance_)));
    }
    algorithm_formatter_.print_header();
    multipliers_ = parameters_.multipliers;
    upper_bound_ = parameters_.upper_bound;
    output_.upper_bound = upper_bound_;
    double root_lower_bound = parameters_.lower_bound;

    if (multipliers_.empty() || upper_bound_ < 0 || root_lower_bound == -INF) {
        // Multipliers (Lagrangian dual and reduction of the network), upper
        // bound and lower bound of the root: the relaxation refinement
        // without refinement step.
        RelaxationRefinementParameters refinement_parameters;
        refinement_parameters.verbosity_level = 0;
        refinement_parameters.timer = parameters_.timer;
        refinement_parameters.network = network_;
        refinement_parameters.multipliers = multipliers_;
        refinement_parameters.column_generation = parameters_.column_generation;
        refinement_parameters.lagrangian_dual_callback = parameters_.lagrangian_dual_callback;
        refinement_parameters.upper_bound = upper_bound_;
        refinement_parameters.constraint_propagation = parameters_.constraint_propagation;
        refinement_parameters.maximum_memory = parameters_.maximum_memory;
        refinement_parameters.maximum_number_of_steps = 0;
        RelaxationRefinementOutput refinement_output = relaxation_refinement(
                instance_,
                refinement_parameters);
        multipliers_ = refinement_output.multipliers;
        update_upper_bound(refinement_output.solution, refinement_output.upper_bound);
        if (root_lower_bound == -INF)
            root_lower_bound = refinement_output.lagrangian_bound;
        if (refinement_output.status == RelaxationRefinementStatus::BoundProved)
            root_lower_bound = INF;
    }
    output_.multipliers = multipliers_;
    if (network_->problem().number_of_jobs == 0 || root_lower_bound == INF) {
        update_lower_bound(upper_bound_, "root");
        return;
    }
    update_lower_bound((Time)std::ceil(root_lower_bound - TOLERANCE), "root");
    if (end())
        return;

    // Relaxation refinements with increasing tentative upper bounds.
    Time increment = (Time)std::llround(parameters_.tentative_upper_bound_increment * upper_bound_);
    if (std::ceil(root_lower_bound - TOLERANCE) <= 0)
        increment = 1;
    increment = (std::max)(increment, (Time)1);
    Counter forced_depth = parameters_.minimum_branching_depth;
    for (Counter k = 1; !end(); ++k) {
        Time tentative_upper_bound = (Time)std::llround(root_lower_bound + k * increment);
        tentative_upper_bound = (std::min)(tentative_upper_bound, upper_bound_);
        if (tentative_upper_bound <= lower_bound_)
            continue;
        branching_depth_ = 0;
        Status status = solve_branch(network_, tentative_upper_bound, forced_depth, 0);
        if (status != Status::Completed)
            return;
        // No solution better than the tentative upper bound.
        update_lower_bound(tentative_upper_bound, "tentative upper bound");
        if (parameters_.lower_bound_callback)
            parameters_.lower_bound_callback(tentative_upper_bound);
        forced_depth = (std::max)(branching_depth_, parameters_.minimum_branching_depth);
    }
}

}

const TreeSearchIterativeRefinementOutput parallelschedulingsolver::tree_search_iterative_refinement(
        const Instance& instance,
        const TreeSearchIterativeRefinementParameters& parameters)
{
    TreeSearchIterativeRefinementOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Tree search, iterative refinement");

    TreeSearchIterativeRefinement algorithm(
            instance,
            parameters,
            output,
            algorithm_formatter);
    algorithm.run();

    algorithm_formatter.end();
    return output;
}
