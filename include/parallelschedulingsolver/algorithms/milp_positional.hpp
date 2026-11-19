/**
 * Positional MILP model
 *
 * Variable x_{i, j, k} = 1 iff job j is at position k on machine i, the
 * positions being counted from the end of the machine: position 1 is the
 * last job. C_{i, k} is the completion time of position k, and C_j the
 * completion time of job j, linked to the positions by big-M constraints.
 *
 * The size of the model is polynomial, O(n^2 m), and doesn't depend on the
 * processing times, unlike the time-indexed model. It supports all the
 * objectives of the repository, with release dates.
 *
 * Counting the positions from the end gives the valid inequality
 * sum_j C_j >= sum_{i, j, k} k p_{i, j} x_{i, j, k}: the completion time of
 * a job is at least the sum of the processing times of the jobs before it
 * on its machine and of its own, so each job contributes its processing time
 * once per job at or after its position. Without release dates and weights,
 * it is the assignment model of R || sum C_j, so the linear relaxation is
 * strong for this problem.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace parallelschedulingsolver
{

struct MilpPositionalParameters: Parameters
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

Output milp_positional(
        const Instance& instance,
        const MilpPositionalParameters& parameters = {});

}
