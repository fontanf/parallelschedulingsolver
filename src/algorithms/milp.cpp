#include "milp.hpp"

#ifdef CBC_FOUND
#include "mathoptsolverscmake/mathopt_cbc.hpp"
#endif
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <cmath>

using namespace parallelschedulingsolver;

namespace
{

/** Update the bound of the objective of the instance. */
void update_bound(
        const Instance& instance,
        AlgorithmFormatter& algorithm_formatter,
        double milp_bound,
        const std::string& s)
{
    if (!std::isfinite(milp_bound))
        return;
    // The objective values are integers.
    if (instance.objective() == Objective::Throughput) {
        Time bound = std::floor(milp_bound + 1e-5);
        algorithm_formatter.update_throughput_bound(bound, s);
        return;
    }
    Time bound = std::ceil(milp_bound - 1e-5);
    switch (instance.objective()) {
    case Objective::Makespan:
        algorithm_formatter.update_makespan_bound(bound, s);
        break;
    case Objective::TotalFlowTime:
        algorithm_formatter.update_total_flow_time_bound(bound, s);
        break;
    case Objective::TotalTardiness:
        algorithm_formatter.update_total_tardiness_bound(bound, s);
        break;
    case Objective::MaximumLateness:
        algorithm_formatter.update_maximum_lateness_bound(bound, s);
        break;
    case Objective::Throughput:
        break;
    }
}

}

void parallelschedulingsolver::solve_milp(
        const Instance& instance,
        const mathoptsolverscmake::MathOptModel& model,
        const MilpSolveParameters& milp_parameters,
        const Parameters& parameters,
        AlgorithmFormatter& algorithm_formatter,
        const MilpSolutionRetriever& retrieve_solution)
{
    std::vector<double> milp_solution;
    double milp_bound = std::numeric_limits<double>::quiet_NaN();

    if (milp_parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
        Highs highs;
        mathoptsolverscmake::reduce_printout(highs);
        mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
        if (!parameters.solver_log_path.empty())
            mathoptsolverscmake::set_log_file(highs, parameters.solver_log_path);
        mathoptsolverscmake::load(highs, model);
        if (!milp_parameters.presolve)
            highs.setOptionValue("presolve", "off");
        highs.setCallback([
                &instance,
                &parameters,
                &algorithm_formatter,
                &retrieve_solution](
                    const int,
                    const std::string&,
                    const HighsCallbackOutput* highs_output,
                    HighsCallbackInput* highs_input,
                    void*)
                {
                    std::string s = "node " + std::to_string(highs_output->mip_node_count);
                    if (!highs_output->mip_solution.empty()) {
                        algorithm_formatter.update_solution(
                                retrieve_solution(highs_output->mip_solution),
                                s);
                    }
                    update_bound(instance, algorithm_formatter, highs_output->mip_dual_bound, s);

                    // Check end.
                    if (parameters.timer.needs_to_end())
                        highs_input->user_interrupt = 1;
                },
                nullptr);
        highs.startCallback(HighsCallbackType::kCallbackMipImprovingSolution);
        highs.startCallback(HighsCallbackType::kCallbackMipInterrupt);
        mathoptsolverscmake::solve(highs);
        milp_solution = mathoptsolverscmake::get_solution(highs);
        milp_bound = mathoptsolverscmake::get_bound(highs);
#else
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "HiGHS is not available; "
                "build with -DPARALLELSCHEDULINGSOLVER_USE_HIGHS=ON.");
#endif

    } else if (milp_parameters.solver == mathoptsolverscmake::SolverName::Cbc) {
#ifdef CBC_FOUND
        mathoptsolverscmake::CbcModel cbc_model;
        mathoptsolverscmake::reduce_printout(cbc_model);
        if (!parameters.solver_log_path.empty())
            mathoptsolverscmake::set_log_file(cbc_model, parameters.solver_log_path);
        mathoptsolverscmake::set_time_limit(cbc_model, parameters.timer.remaining_time());
        mathoptsolverscmake::load(cbc_model, model);
        mathoptsolverscmake::solve(cbc_model);
        milp_solution = mathoptsolverscmake::get_solution(cbc_model);
        milp_bound = mathoptsolverscmake::get_bound(cbc_model);
#else
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "CBC is not available; "
                "build with -DPARALLELSCHEDULINGSOLVER_USE_CBC=ON.");
#endif

    } else {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "unsupported solver.");
    }

    // Retrieve solution.
    if (!milp_solution.empty())
        algorithm_formatter.update_solution(retrieve_solution(milp_solution), "");

    // Retrieve bound.
    update_bound(instance, algorithm_formatter, milp_bound, "");
}
