#include "parallelschedulingsolver/algorithms/column_generation.hpp"

#include "lagrangian_dual_internal.hpp"

#include "columngenerationsolver/algorithms/column_generation.hpp"

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::lagrangian_dual;

namespace
{

/**
 * Pricing solver: the time-indexed dynamic programming of the relaxation.
 * The duals of the job rows are the multipliers, the dual of the row n + g
 * is the one of the convexity constraint of group g of identical machines.
 */
class PricingSolver: public columngenerationsolver::PricingSolver
{

public:

    using Evaluate = std::function<std::vector<std::vector<JobId>>(const std::vector<double>&)>;

    PricingSolver(
            const LagrangianDual& lagrangian_dual,
            const Evaluate& evaluate,
            const std::function<bool()>& end):
        lagrangian_dual_(lagrangian_dual),
        evaluate_(evaluate),
        end_(end) { }

    /** Column of a path of the network of a group. */
    std::shared_ptr<const columngenerationsolver::Column> column(
            Counter group_id,
            const std::vector<JobId>& path) const
    {
        JobId n = lagrangian_dual_.number_of_jobs();
        std::vector<double> occurrences(n, 0);
        for (JobId job_id: path)
            occurrences[job_id] += 1;
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->objective_coefficient = lagrangian_dual_.problem(group_id).sequence_cost(path);
        for (JobId job_id = 0; job_id < n; ++job_id)
            if (occurrences[job_id] != 0)
                column->elements.push_back({job_id, occurrences[job_id]});
        column->elements.push_back({n + group_id, 1});
        return column;
    }

    /** Reduced cost of a path of the network of a group. */
    double reduced_cost(
            Counter group_id,
            const std::vector<JobId>& path,
            const std::vector<columngenerationsolver::Value>& duals) const
    {
        JobId n = lagrangian_dual_.number_of_jobs();
        double reduced_cost = lagrangian_dual_.problem(group_id).sequence_cost(path)
            - duals[n + group_id];
        for (JobId job_id: path)
            reduced_cost -= duals[job_id];
        return reduced_cost;
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
        JobId n = lagrangian_dual_.number_of_jobs();
        std::vector<double> multipliers(duals.begin(), duals.begin() + n);
        std::vector<std::vector<JobId>> paths = evaluate_(multipliers);
        if (end_())
            return output;
        // Exact pricing: the reduced cost of the path of a group is the
        // smallest one of the group, counted once for each of its machines.
        output.overcost = 0;
        for (Counter group_id = 0; group_id < (Counter)paths.size(); ++group_id) {
            double reduced_cost = this->reduced_cost(group_id, paths[group_id], duals);
            output.overcost += lagrangian_dual_.multiplicity(group_id) * reduced_cost;
            if (lagrangian_dual_.single_machine() || reduced_cost < 0)
                output.columns.push_back(column(group_id, paths[group_id]));
        }
        return output;
    }

private:

    const LagrangianDual& lagrangian_dual_;

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

    JobId n = lagrangian_dual.number_of_jobs();
    Counter number_of_groups = lagrangian_dual.number_of_groups();
    bool network_empty = false;

    // Evaluation of the relaxation for some multipliers.
    auto evaluate = [&lagrangian_dual, &network_empty](
            const std::vector<double>& multipliers)
    {
        std::vector<std::vector<JobId>> paths;
        bool feasible = false;
        double lower_bound = lagrangian_dual.evaluate(multipliers, paths, feasible);
        if (lower_bound == time_indexed::INF) {
            network_empty = true;
            lagrangian_dual.network_empty();
            return std::vector<std::vector<JobId>>(lagrangian_dual.number_of_groups());
        }
        lagrangian_dual.process(multipliers, lower_bound, paths, feasible);
        return paths;
    };
    auto end = [&lagrangian_dual, &network_empty]()
    {
        return network_empty || lagrangian_dual.end();
    };

    // Master problem: a row for each job and a convexity row for each group
    // of identical machines (its number of machines).
    columngenerationsolver::Model model;
    model.objective_sense = optimizationtools::ObjectiveDirection::Minimize;
    for (JobId job_id = 0; job_id < n; ++job_id) {
        columngenerationsolver::Row row;
        row.lower_bound = 1;
        row.upper_bound = 1;
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = n;
        model.rows.push_back(row);
    }
    for (Counter group_id = 0; group_id < number_of_groups; ++group_id) {
        columngenerationsolver::Row row;
        row.lower_bound = lagrangian_dual.multiplicity(group_id);
        row.upper_bound = lagrangian_dual.multiplicity(group_id);
        row.coefficient_lower_bound = 0;
        row.coefficient_upper_bound = lagrangian_dual.multiplicity(group_id);
        model.rows.push_back(row);
    }
    // Rejection of the jobs with a rejection cost.
    for (JobId job_id = 0; job_id < n; ++job_id) {
        if (lagrangian_dual.rejection_cost(job_id) < 0)
            continue;
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->type = columngenerationsolver::VariableType::Continuous;
        column->lower_bound = 0;
        column->upper_bound = 1;
        column->objective_coefficient = lagrangian_dual.rejection_cost(job_id);
        column->elements.push_back({job_id, 1});
        model.static_columns.push_back(column);
    }
    auto pricing_solver = std::unique_ptr<PricingSolver>(
            new PricingSolver(lagrangian_dual, evaluate, end));
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
    // once, on all the machines: a convex combination of it and of the other
    // columns is a relaxation of the schedules), the empty path of each group
    // on parallel machines, and the solution of the relaxation for the
    // initial multipliers.
    {
        auto column = std::make_shared<columngenerationsolver::Column>();
        column->objective_coefficient = lagrangian_dual.upper_bound();
        for (JobId job_id = 0; job_id < n; ++job_id)
            column->elements.push_back({job_id, 1});
        for (Counter group_id = 0; group_id < number_of_groups; ++group_id)
            column->elements.push_back({n + group_id, (double)lagrangian_dual.multiplicity(group_id)});
        column_generation_parameters.initial_columns.push_back(column);
    }
    if (!lagrangian_dual.single_machine()) {
        for (Counter group_id = 0; group_id < number_of_groups; ++group_id)
            column_generation_parameters.initial_columns.push_back(pricing_solver_ref.column(group_id, {}));
    }
    {
        std::vector<double> multipliers = parameters.initial_multipliers;
        if (multipliers.empty())
            multipliers.assign(n, 0);
        std::vector<std::vector<JobId>> paths = evaluate(multipliers);
        if (end()) {
            algorithm_formatter.end();
            return output;
        }
        for (Counter group_id = 0; group_id < number_of_groups; ++group_id)
            if (lagrangian_dual.single_machine() || !paths[group_id].empty())
                column_generation_parameters.initial_columns.push_back(pricing_solver_ref.column(group_id, paths[group_id]));
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
