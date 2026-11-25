/**
 * Lagrangian relaxation of the time-indexed formulation
 *
 * The multipliers of the relaxation of the constraints "each job is processed
 * exactly once (or rejected)" are adjusted by the conjugate subgradient
 * algorithm, with the step size control of Tanaka and Fujikuma (2012); the
 * relaxation is solved by the time-indexed dynamic programming. See
 * lagrangian_dual.hpp.
 *
 * S. Tanaka, S. Fujikuma (2012). A dynamic-programming-based exact algorithm
 * for general single-machine scheduling with machine idle time. Journal of
 * Scheduling, 15, 347-361.
 */

#pragma once

#include "parallelschedulingsolver/algorithms/lagrangian_dual.hpp"

namespace parallelschedulingsolver
{

struct LagrangianRelaxationParameters: LagrangianDualParameters
{
    /*
     * Parameters of the conjugate subgradient algorithm (Tanaka and Fujikuma,
     * 2012, Section 7.1).
     */

    /** Initial step size parameter. */
    double initial_step_size = 1.2;

    /** Iterations for the termination criterion; if negative, n. */
    Counter termination_iterations = -1;

    /** Iterations without improvement before decreasing the step size. */
    Counter shrink_iterations = 2;

    /** Relative improvement for the termination criterion. */
    double termination_epsilon = 0.02;

    /** Factor of the step size when it is decreased. */
    double shrink_factor = 0.95;

    /** Factor of the step size when it is increased. */
    double expand_factor = 1.1;
};

const LagrangianDualOutput lagrangian_relaxation(
        const Instance& instance,
        const LagrangianRelaxationParameters& parameters = {});

}
