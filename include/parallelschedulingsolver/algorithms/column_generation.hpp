/**
 * Column generation for the time-indexed formulation
 *
 * The Lagrangian dual of the relaxation of the constraints "each job is
 * processed exactly once (or rejected)" is solved by column generation
 * (Tanaka and Araki, 2013, Section 5.2), with columngenerationsolver: the
 * master problem is
 *     min sum_P L(P) x_P + sum_i e_i y_i
 *     s.t. sum_P V_i(P) x_P + y_i = 1 for each job i,
 *          sum_P x_P = 1,
 * over the paths P of the network (V_i(P): number of times job i is in P),
 * where y_i is the rejection of job i (if it has a rejection cost e_i), and
 * the pricing problem is solved by the time-indexed dynamic programming. The
 * duals of the job rows are the multipliers. The dual is stabilized by
 * Wentges smoothing, with the duals of the best lower bound as stability
 * center. See lagrangian_dual.hpp.
 *
 * Requires an LP solver (HiGHS or CLP).
 *
 * S. Tanaka, M. Araki (2013). An exact algorithm for the single-machine total
 * weighted tardiness problem with sequence-dependent setup times. Computers
 * & Operations Research, 40(1), 344-352.
 */

#pragma once

#include "parallelschedulingsolver/algorithms/lagrangian_dual.hpp"

namespace parallelschedulingsolver
{

struct ColumnGenerationParameters: LagrangianDualParameters
{
    /** Wentges smoothing parameter. */
    double wentges_smoothing_parameter = 0.5;
};

const LagrangianDualOutput column_generation(
        const Instance& instance,
        const ColumnGenerationParameters& parameters = {});

}
