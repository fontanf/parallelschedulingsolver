#include "parallelschedulingsolver/algorithms/column_generation.hpp"

#include "lagrangian_dual_internal.hpp"

#include "columngenerationsolver/algorithms/column_generation.hpp"

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::lagrangian_dual;

namespace
{

/**
 * Pricing solver: the time-indexed dynamic programming of the relaxation.
 * The duals of the job rows are the multipliers, the dual of the last row is
 * the one of the convexity constraint.
 */
class PricingSolver: public columngenerationsolver::PricingSolver
{

public:

    using Evaluate = std::function<std::vector<JobId>(const std::vector<double>&)>;

    PricingSolver(
            const time_indexed::Problem& problem,
            const Evaluate& evaluate,
            const std::function<bool()>& end):
        problem_(problem),
        evaluate_(evaluate),
        end_(end) { }

    /** Column of a path. */
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
        // The lower bounds are computed and stored by 'evaluate_'.
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

    const time_indexed::Problem& problem_;

    Evaluate evaluate_;

    std::function<bool()> end_;

};

}

const LagrangianDualOutput parallelschedulingsolver::column_generation(
        const Instance& instance,
        const ColumnGenerationParameters& parameters)
{
    LagrangianDualOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start("Column generation");

#if defined(HIGHS_FOUND) || defined(CLP_FOUND)
    LagrangianDual lagrangian_dual(instance, parameters, output, algorithm_formatter);
    algorithm_formatter.print_header();

    const time_indexed::Problem& problem = lagrangian_dual.problem();
    JobId n = lagrangian_dual.number_of_jobs();
    bool network_empty = false;

    // Evaluation of the relaxation for some multipliers.
    auto evaluate = [&lagrangian_dual, &network_empty](
            const std::vector<double>& multipliers)
    {
        std::vector<JobId> path;
        bool feasible = false;
        double lower_bound = lagrangian_dual.evaluate(multipliers, path, feasible);
        if (lower_bound == time_indexed::INF) {
            network_empty = true;
            lagrangian_dual.network_empty();
            return std::vector<JobId>();
        }
        lagrangian_dual.process(multipliers, lower_bound, path, feasible);
        return path;
    };
    auto end = [&lagrangian_dual, &network_empty]()
    {
        return network_empty || lagrangian_dual.end();
    };

    // Master problem: a row for each job and the convexity row.
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
    // Rejection of the jobs with a rejection cost.
    for (JobId job_id = 0; job_id < n; ++job_id) {
        if (problem.rejection_costs[job_id] < 0)
            continue;
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->type = columngenerationsolver::VariableType::Continuous;
        column->lower_bound = 0;
        column->upper_bound = 1;
        column->objective_coefficient = problem.rejection_costs[job_id];
        column->elements.push_back({job_id, 1});
        model.static_columns.push_back(column);
    }
    auto pricing_solver = std::unique_ptr<PricingSolver>(
            new PricingSolver(problem, evaluate, end));
    PricingSolver& pricing_solver_ref = *pricing_solver;
    model.pricing_solver = std::move(pricing_solver);

    columngenerationsolver::ColumnGenerationParameters column_generation_parameters;
    column_generation_parameters.verbosity_level = 0;
    column_generation_parameters.timer = parameters.timer;
#if defined(HIGHS_FOUND)
    column_generation_parameters.solver_name = columngenerationsolver::SolverName::Highs;
#else
    column_generation_parameters.solver_name = columngenerationsolver::SolverName::CLP;
#endif
    column_generation_parameters.static_wentges_smoothing_parameter = parameters.wentges_smoothing_parameter;
    column_generation_parameters.wentges_smoothing_best_bound_center = true;

    // Initial columns: a schedule of cost the upper bound (all the jobs
    // once; the order of the jobs doesn't matter for the master problem),
    // and the solution of the relaxation for the initial multipliers.
    {
        std::vector<JobId> path(n);
        std::iota(path.begin(), path.end(), 0);
        auto column = std::make_shared<columngenerationsolver::Column>(
                *pricing_solver_ref.column(path));
        column->objective_coefficient = lagrangian_dual.upper_bound();
        column_generation_parameters.initial_columns.push_back(column);
    }
    {
        std::vector<double> multipliers = parameters.initial_multipliers;
        if (multipliers.empty())
            multipliers.assign(n, 0);
        std::vector<JobId> path = evaluate(multipliers);
        if (end()) {
            algorithm_formatter.end();
            return output;
        }
        column_generation_parameters.initial_columns.push_back(pricing_solver_ref.column(path));
    }

    columngenerationsolver::column_generation(model, column_generation_parameters);
#else
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": "
            "column generation requires an LP solver (HiGHS or CLP).");
#endif

    algorithm_formatter.end();
    return output;
}
