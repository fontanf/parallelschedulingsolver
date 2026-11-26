#include "parallelschedulingsolver/algorithms/relaxation_refinement.hpp"

#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"
#include "parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp"
#include "parallelschedulingsolver/algorithms/column_generation.hpp"

#include <cmath>
#include <numeric>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

std::ostream& parallelschedulingsolver::operator<<(
        std::ostream& os,
        RelaxationRefinementStatus status)
{
    switch (status) {
    case RelaxationRefinementStatus::BoundProved:
        os << "BoundProved";
        break;
    case RelaxationRefinementStatus::MemoryOut:
        os << "MemoryOut";
        break;
    case RelaxationRefinementStatus::TimeOut:
        os << "TimeOut";
        break;
    case RelaxationRefinementStatus::MaximumNumberOfSteps:
        os << "MaximumNumberOfSteps";
        break;
    case RelaxationRefinementStatus::TailingOff:
        os << "TailingOff";
        break;
    }
    return os;
}

namespace
{

/** Tolerance on the lower bounds, computed with floating point multipliers. */
constexpr double TOLERANCE = 1e-6;

/** Compute the labels of a direction if they are not up to date. */
void update_labels(
        Network& network,
        Direction direction,
        const std::vector<double>& multipliers)
{
    if (network.labels_up_to_date(direction, multipliers))
        return;
    if (direction == Direction::Forward) {
        network.compute_forward_labels(multipliers);
    } else {
        network.compute_backward_labels(multipliers);
    }
}

class RelaxationRefinement
{

public:

    RelaxationRefinement(
            const Instance& instance,
            const RelaxationRefinementParameters& parameters,
            RelaxationRefinementOutput& output,
            AlgorithmFormatter& algorithm_formatter):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        algorithm_formatter_(algorithm_formatter) { }

    void run();

private:

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
            const std::string& s)
    {
        if (!solution.feasible())
            return;
        Time value = (Time)solution.objective_value();
        if (upper_bound_ >= 0 && value >= upper_bound_) {
            if (!output_.solution.feasible())
                algorithm_formatter_.update_solution(solution, s);
            return;
        }
        upper_bound_ = value;
        output_.upper_bound = upper_bound_;
        algorithm_formatter_.update_solution(solution, s);
    }

    /**
     * Update the bound of the instance: no schedule has a value smaller than
     * 'bound' (if the network isn't restricted).
     */
    void update_lower_bound(
            Time bound,
            const std::string& s)
    {
        bound = (std::min)(bound, upper_bound_);
        if (bound <= lower_bound_)
            return;
        lower_bound_ = bound;
        if (restricted_)
            return;
        if (network_->problem().tardiness) {
            algorithm_formatter_.update_total_tardiness_bound(lower_bound_, s);
        } else {
            algorithm_formatter_.update_total_flow_time_bound(lower_bound_, s);
        }
    }

    /** Multipliers of the Lagrangian dual, and reduction of the network. */
    void lagrangian_dual();

    /** Refinement steps. */
    RelaxationRefinementStatus refine();

    const Instance& instance_;

    const RelaxationRefinementParameters& parameters_;

    RelaxationRefinementOutput& output_;

    AlgorithmFormatter& algorithm_formatter_;

    std::shared_ptr<Network> network_;

    /** True if the input network is restricted. */
    bool restricted_ = false;

    std::vector<double> multipliers_;

    /** Upper bound; -1 if none. */
    Time upper_bound_ = -1;

    /** Bound of the instance. */
    Time lower_bound_ = std::numeric_limits<Time>::min();

    /** Bound to prove, at most the upper bound. */
    Time bound_to_prove_ = -1;

};

void RelaxationRefinement::lagrangian_dual()
{
    auto set_parameters = [this](LagrangianDualParameters& parameters)
    {
        parameters.verbosity_level = 0;
        parameters.timer = parameters_.timer;
        parameters.network = network_;
        parameters.upper_bound = upper_bound_;
        parameters.constraint_propagation = parameters_.constraint_propagation;
        parameters.iteration_callback = parameters_.lagrangian_dual_callback;
    };
    LagrangianDualOutput output(instance_);
#if defined(HIGHS_FOUND) || defined(CLP_FOUND)
    bool use_column_generation = parameters_.column_generation;
#else
    bool use_column_generation = false;
#endif
    if (use_column_generation) {
        ColumnGenerationParameters parameters;
        set_parameters(parameters);
        output = column_generation(instance_, parameters);
    } else {
        LagrangianRelaxationParameters parameters;
        set_parameters(parameters);
        output = lagrangian_relaxation(instance_, parameters);
    }
    output_.number_of_lagrangian_dual_iterations = output.number_of_iterations;
    update_upper_bound(output.solution, "lagrangian dual");
    if (output.upper_bound >= 0 && (upper_bound_ < 0 || output.upper_bound < upper_bound_)) {
        upper_bound_ = output.upper_bound;
        output_.upper_bound = upper_bound_;
    }
    multipliers_ = output.multipliers;
    if (multipliers_.empty())
        multipliers_.assign(network_->problem().number_of_jobs, 0);
    if (output.lagrangian_bound == INF) {
        update_lower_bound(upper_bound_, "lagrangian dual");
    } else if (output.lagrangian_bound != -INF) {
        update_lower_bound((Time)std::ceil(output.lagrangian_bound - TOLERANCE), "lagrangian dual");
    }
    output_.lagrangian_bound = output.lagrangian_bound;
}

void RelaxationRefinement::run()
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
    restricted_ = network_->restricted();
    output_.network = network_;
    const Problem& problem = network_->problem();
    JobId n = problem.number_of_jobs;
    algorithm_formatter_.print_header();

    upper_bound_ = parameters_.upper_bound;
    output_.upper_bound = upper_bound_;
    if (n == 0) {
        update_upper_bound(build_solution(instance_, problem, {}), "no job");
        update_lower_bound(0, "no job");
        output_.status = RelaxationRefinementStatus::BoundProved;
        output_.bound_to_prove = 0;
        return;
    }

    // Multipliers.
    multipliers_ = parameters_.multipliers;
    if (multipliers_.empty()) {
        lagrangian_dual();
        if (parameters_.timer.needs_to_end()) {
            output_.multipliers = multipliers_;
            output_.status = RelaxationRefinementStatus::TimeOut;
            return;
        }
        // Reduction of the network with the multipliers, then the labels of
        // both directions.
        if (upper_bound_ >= 0) {
            DynamicProgrammingTimeIndexedParameters dp_parameters;
            dp_parameters.network = network_;
            dp_parameters.multipliers = multipliers_;
            dp_parameters.cutoff = upper_bound_ - 1 + TOLERANCE;
            dp_parameters.reduction = true;
            dp_parameters.constraint_propagation = parameters_.constraint_propagation;
            dp_parameters.both_directions_labels = true;
            DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming(dp_parameters);
            if (dp_output.lagrangian_bound > output_.lagrangian_bound)
                output_.lagrangian_bound = dp_output.lagrangian_bound;
        }
    } else if ((JobId)multipliers_.size() != n) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "wrong number of multipliers; "
                "number of multipliers: " + std::to_string(multipliers_.size()) + "; "
                "number of jobs of the problem: " + std::to_string(n) + ".");
    }
    output_.multipliers = multipliers_;
    if (upper_bound_ < 0) {
        // Best of the orders of the instance, of the weighted shortest
        // processing times and of the earliest due dates.
        std::vector<JobId> identity(n);
        std::iota(identity.begin(), identity.end(), 0);
        std::vector<JobId> wspt = identity;
        std::stable_sort(wspt.begin(), wspt.end(), [&problem](JobId j1, JobId j2)
                {
                    return problem.processing_times[j1] * problem.weights[j2]
                        < problem.processing_times[j2] * problem.weights[j1];
                });
        std::vector<JobId> edd = identity;
        std::stable_sort(edd.begin(), edd.end(), [&problem](JobId j1, JobId j2)
                {
                    return problem.due_dates[j1] < problem.due_dates[j2];
                });
        for (const std::vector<JobId>& sequence: {identity, wspt, edd})
            update_upper_bound(build_solution(instance_, problem, sequence), "initial solution");
    }
    output_.status = refine();
    output_.bound_to_prove = bound_to_prove_;
    if (output_.status == RelaxationRefinementStatus::BoundProved)
        update_lower_bound(bound_to_prove_, "bound proved");
}

RelaxationRefinementStatus RelaxationRefinement::refine()
{
    const Problem& problem = network_->problem();
    JobId n = problem.number_of_jobs;
    const std::vector<double>& multipliers = multipliers_;
    double maximum_memory = parameters_.maximum_memory * 1024 * 1024;

    bound_to_prove_ = (parameters_.bound_to_prove < 0)?
        upper_bound_:
        (std::min)(parameters_.bound_to_prove, upper_bound_);

    // The labels of both directions of the multipliers.
    update_labels(*network_, Direction::Forward, multipliers);
    update_labels(*network_, Direction::Backward, multipliers);
    double lower_bound = network_->shortest_path_length(Direction::Forward, multipliers);
    output_.maximum_number_of_states = network_->number_of_states();
    if (lower_bound == INF) {
        // No path: no schedule better than the cutoff of the network.
        output_.lagrangian_bound = INF;
        return RelaxationRefinementStatus::BoundProved;
    }
    lower_bound += network_->constant_term(multipliers);
    double best_lower_bound = lower_bound;
    output_.lagrangian_bound = (std::max)(output_.lagrangian_bound, lower_bound);
    std::vector<JobId> path = network_->shortest_path(Direction::Forward, multipliers);
    std::vector<double> best_lower_bounds = {best_lower_bound};

    // Strategy for the choice of the modifiers: (S1) if the memory usage is
    // small, (S2) otherwise.
    bool strategy_1 = (network_->memory() / maximum_memory < std::pow(2.0, -6));

    std::shared_ptr<Network> network_old = network_;
    Direction direction = Direction::Forward;
    Mask modifiers = network_->modifiers();
    JobId number_of_modifiers = modifiers.size();
    output_.number_of_modifiers = number_of_modifiers;

    for (;;) {
        if ((double)bound_to_prove_ - lower_bound < 1 - TOLERANCE)
            return RelaxationRefinementStatus::BoundProved;
        if (parameters_.timer.needs_to_end())
            return RelaxationRefinementStatus::TimeOut;
        if (parameters_.maximum_number_of_steps >= 0
                && output_.number_of_steps >= parameters_.maximum_number_of_steps) {
            return RelaxationRefinementStatus::MaximumNumberOfSteps;
        }
        Counter k = parameters_.tailing_off_number_of_steps;
        if (k > 0 && (Counter)best_lower_bounds.size() > k) {
            double lower_bound_old = best_lower_bounds[best_lower_bounds.size() - 1 - k];
            double gap_old = (double)bound_to_prove_ - lower_bound_old;
            if (best_lower_bound - lower_bound_old
                    < parameters_.tailing_off_minimum_improvement * gap_old) {
                return RelaxationRefinementStatus::TailingOff;
            }
        }
        if (number_of_modifiers == n) {
            throw std::logic_error(
                    FUNC_SIGNATURE + ": "
                    "gap with all the jobs in M.");
        }

        // Number of jobs to add, from the memory usage.
        double memory_ratio = network_old->memory() / maximum_memory;
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
        dp_parameters.cutoff = bound_to_prove_ - 1 + TOLERANCE;
        dp_parameters.constraint_propagation = parameters_.constraint_propagation;
        DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming(dp_parameters);
        if (dp_output.memory_out)
            return RelaxationRefinementStatus::MemoryOut;
        if (parameters_.timer.needs_to_end())
            return RelaxationRefinementStatus::TimeOut;
        std::shared_ptr<Network> network_new = dp_output.network;
        output_.number_of_steps++;
        modifiers = new_modifiers;
        number_of_modifiers += selected_job_ids.size();
        output_.number_of_modifiers = number_of_modifiers;
        output_.network = network_new;
        output_.maximum_number_of_states = (std::max)(
                output_.maximum_number_of_states,
                network_new->number_of_states());

        lower_bound = dp_output.lagrangian_bound;
        path = dp_output.path;
        algorithm_formatter_.print(
                "m " + std::to_string(number_of_modifiers)
                + " states " + std::to_string(network_new->number_of_states())
                + " lb " + std::to_string(lower_bound)
                + " ubt " + std::to_string(bound_to_prove_));
        // A path visiting each job that must be scheduled exactly once, and
        // the other jobs at most once, is a schedule; it is always the case
        // when all the jobs are in M.
        bool feasible = dp_output.solution.feasible();
        if (feasible)
            update_upper_bound(dp_output.solution, "relaxation");
        bool improved = (lower_bound > best_lower_bound + TOLERANCE);
        if (parameters_.step_callback) {
            RelaxationRefinementStep step{
                    output_.number_of_steps,
                    *network_new,
                    lower_bound,
                    path,
                    direction,
                    feasible,
                    improved,
                    bound_to_prove_};
            Time upper_bound = parameters_.step_callback(step);
            if (upper_bound >= 0 && (upper_bound_ < 0 || upper_bound < upper_bound_)) {
                upper_bound_ = upper_bound;
                output_.upper_bound = upper_bound_;
            }
        }
        best_lower_bound = (std::max)(best_lower_bound, lower_bound);
        best_lower_bounds.push_back(best_lower_bound);
        output_.lagrangian_bound = (std::max)(output_.lagrangian_bound, best_lower_bound);
        if (best_lower_bound != INF && !network_new->restricted()) {
            update_lower_bound(
                    (std::min)((Time)std::ceil(best_lower_bound - TOLERANCE), bound_to_prove_),
                    "refinement");
        }
        bound_to_prove_ = (std::min)(bound_to_prove_, upper_bound_);

        network_old = network_new;
        direction = (direction == Direction::Forward)? Direction::Backward: Direction::Forward;
    }
}

}

const RelaxationRefinementOutput parallelschedulingsolver::relaxation_refinement(
        const Instance& instance,
        const RelaxationRefinementParameters& parameters)
{
    RelaxationRefinementOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Relaxation refinement");

    RelaxationRefinement algorithm(
            instance,
            parameters,
            output,
            algorithm_formatter);
    algorithm.run();

    algorithm_formatter.end();
    return output;
}
