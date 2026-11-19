/**
 * Common code of the MILP algorithms: solve a model with HiGHS or CBC and
 * report the solutions and bounds of the solver.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

#include <functional>

namespace parallelschedulingsolver
{

/** Add a variable to the model and return its id. */
inline int add_variable(
        mathoptsolverscmake::MathOptModel& model,
        double lower_bound,
        double upper_bound,
        mathoptsolverscmake::VariableType type,
        double objective_coefficient)
{
    int variable_id = model.variables_lower_bounds.size();
    model.variables_lower_bounds.push_back(lower_bound);
    model.variables_upper_bounds.push_back(upper_bound);
    model.variables_types.push_back(type);
    model.objective_coefficients.push_back(objective_coefficient);
    return variable_id;
}

/** Add a constraint 'lower_bound <= sum elements <= upper_bound'. */
inline void add_constraint(
        mathoptsolverscmake::MathOptModel& model,
        const std::vector<std::pair<int, double>>& elements,
        double lower_bound,
        double upper_bound)
{
    model.constraints_starts.push_back(model.elements_variables.size());
    for (const auto& element: elements) {
        model.elements_variables.push_back(element.first);
        model.elements_coefficients.push_back(element.second);
    }
    model.constraints_lower_bounds.push_back(lower_bound);
    model.constraints_upper_bounds.push_back(upper_bound);
}

struct MilpSolveParameters
{
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /** Run the presolve of HiGHS. */
    bool presolve = true;
};

/** Convert a solution of the MILP into a solution of the instance. */
using MilpSolutionRetriever = std::function<Solution(const std::vector<double>&)>;

/**
 * Solve a MILP model of the instance.
 *
 * The solutions found by the solver are converted with 'retrieve_solution'
 * and reported to the algorithm formatter, as well as the bound of the
 * solver, rounded to an integer since the objective values are integers. The
 * solver is stopped at the time limit of the parameters.
 */
void solve_milp(
        const Instance& instance,
        const mathoptsolverscmake::MathOptModel& model,
        const MilpSolveParameters& milp_parameters,
        const Parameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        const MilpSolutionRetriever& retrieve_solution);

}
