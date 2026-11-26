#include "parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp"

#include "lagrangian_dual_internal.hpp"

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::lagrangian_dual;

const LagrangianDualOutput parallelschedulingsolver::lagrangian_relaxation(
        const Instance& instance,
        const LagrangianRelaxationParameters& parameters)
{
    LagrangianDualOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Lagrangian relaxation");

    LagrangianDual lagrangian_dual(instance, parameters, output, algorithm_formatter);
    algorithm_formatter.print_header();

    JobId n = lagrangian_dual.number_of_jobs();
    std::vector<double> multipliers = parameters.initial_multipliers;
    if (multipliers.empty())
        multipliers.assign(n, 0);
    std::vector<double> direction(n, 0);
    double direction_norm = 0;
    double step_size = parameters.initial_step_size;
    Counter termination_iterations = (parameters.termination_iterations >= 0)?
        parameters.termination_iterations: n;
    Counter iterations_without_improvement = 0;
    double epsilon = parameters.termination_epsilon;
    std::vector<double> best_lower_bounds;
    std::vector<double> gaps;

    while (!lagrangian_dual.end()) {
        std::vector<std::vector<JobId>> paths;
        bool feasible = false;
        double lower_bound = lagrangian_dual.evaluate(multipliers, paths, feasible);
        if (lower_bound == time_indexed::INF) {
            lagrangian_dual.network_empty();
            break;
        }
        bool improved = lagrangian_dual.process(multipliers, lower_bound, paths, feasible);
        if (lagrangian_dual.end())
            break;

        // Step size.
        if (improved) {
            step_size *= parameters.expand_factor;
            iterations_without_improvement = 0;
        } else {
            iterations_without_improvement++;
            if (iterations_without_improvement >= parameters.shrink_iterations) {
                step_size *= parameters.shrink_factor;
                iterations_without_improvement = 0;
            }
        }

        // Termination criterion.
        Time upper_bound = lagrangian_dual.upper_bound();
        best_lower_bounds.push_back(output.lagrangian_bound);
        gaps.push_back(upper_bound - output.lagrangian_bound);
        if ((Counter)best_lower_bounds.size() > termination_iterations) {
            double lower_bound_old = best_lower_bounds[best_lower_bounds.size() - 1 - termination_iterations];
            double gap_old = gaps[gaps.size() - 1 - termination_iterations];
            if (output.lagrangian_bound - lower_bound_old
                    <= std::abs(lower_bound_old) * epsilon / (1 - epsilon)
                    && gap_old - gaps.back() <= epsilon * gap_old) {
                break;
            }
        }

        std::vector<double> subgradient = lagrangian_dual.subgradient(multipliers, paths);
        double subgradient_norm = 0;
        for (JobId job_id = 0; job_id < n; ++job_id)
            subgradient_norm += subgradient[job_id] * subgradient[job_id];
        subgradient_norm = std::sqrt(subgradient_norm);
        if (subgradient_norm == 0) {
            // The solution of the relaxation satisfies the relaxed
            // constraints: the multipliers are optimal.
            break;
        }
        double ratio = (direction_norm > 0)? subgradient_norm / direction_norm: 0;
        double norm_2 = 0;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            direction[job_id] = subgradient[job_id] + ratio * direction[job_id];
            norm_2 += direction[job_id] * direction[job_id];
        }
        if (norm_2 == 0) {
            // The subgradient cancels the previous direction: restart the
            // conjugate direction from the subgradient.
            for (JobId job_id = 0; job_id < n; ++job_id)
                direction[job_id] = subgradient[job_id];
            norm_2 = subgradient_norm * subgradient_norm;
        }
        direction_norm = std::sqrt(norm_2);
        double step = step_size * ((double)upper_bound - lower_bound) / norm_2;
        for (JobId job_id = 0; job_id < n; ++job_id)
            multipliers[job_id] += step * direction[job_id];
    }

    algorithm_formatter.end();
    return output;
}
