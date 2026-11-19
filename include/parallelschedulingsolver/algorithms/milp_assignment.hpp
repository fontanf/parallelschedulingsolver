/**
 * Assignment MILP model
 *
 * Variable x_{i, j} = 1 iff job j is assigned to machine i. Once the jobs are
 * assigned, a sequencing rule is optimal on each machine, and the value of
 * the resulting schedule is linear in the assignment variables:
 *
 * - makespan: earliest release date first; the completion time of machine i
 *   is max_rho (rho + sum_{k: r_k >= rho} p_{i, k} x_{i, k}), over the
 *   release dates rho;
 * - maximum lateness, without release dates: earliest due date first; the
 *   maximum lateness of machine i is
 *   max_delta (sum_{k: d_k <= delta} p_{i, k} x_{i, k} - delta), over the due
 *   dates delta.
 *
 * Hence the model is exact for R | r_j | C_max and R || L_max. It doesn't
 * support the other objectives.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace parallelschedulingsolver
{

struct MilpAssignmentParameters: Parameters
{
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Solver: " << solver << std::endl
            ;
    }
};

Output milp_assignment(
        const Instance& instance,
        const MilpAssignmentParameters& parameters = {});

}
