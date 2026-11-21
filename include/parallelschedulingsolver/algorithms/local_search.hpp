/**
 * Local search
 *
 * Multi-start iterated local search of Kramer and Subramanian (2019) (UILS),
 * for the total weighted tardiness and the total weighted flow time, on
 * single, identical, uniform or unrelated machines, with release dates.
 *
 * - Each restart starts from a random solution (or a randomized EDD/ERD
 *   solution on a single machine), improved by the local search.
 * - The local search is a randomized variable neighborhood descent (RVND)
 *   over block insertion and block swap neighborhoods, within a machine and
 *   between two machines.
 * - The best solution of the restart is perturbed and improved by the local
 *   search, until a number of consecutive perturbations without improvement.
 *
 * A. Kramer, A. Subramanian (2019). A unified heuristic and an annotated
 * bibliography for a large class of earliness-tardiness scheduling problems.
 * Journal of Scheduling, 22, 21-57.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include <random>

namespace parallelschedulingsolver
{

struct LocalSearchParameters: Parameters
{
    /** Maximum number of restarts (I_R). */
    Counter maximum_number_of_restarts = 10;

    /**
     * Maximum number of consecutive perturbations without improvement in a
     * restart (I_ILS).
     *
     * If negative, 4 n without release dates and n with release dates (when
     * idle time may be needed), as in the paper.
     */
    Counter maximum_number_of_perturbations_without_improvement = -1;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Max. # of restarts: " << maximum_number_of_restarts << std::endl
            << std::setw(width) << std::left << "Max. # of pert. without impr.: " << maximum_number_of_perturbations_without_improvement << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"MaximumNumberOfRestarts", maximum_number_of_restarts},
                {"MaximumNumberOfPerturbationsWithoutImprovement", maximum_number_of_perturbations_without_improvement},
                });
        return json;
    }
};

struct LocalSearchOutput: Output
{
    LocalSearchOutput(
            const Instance& instance):
        Output(instance) { }

    /** Number of restarts. */
    Counter number_of_restarts = 0;

    /** Number of perturbations. */
    Counter number_of_perturbations = 0;

    virtual int format_width() const override { return 31; }

    virtual void format(std::ostream& os) const override
    {
        Output::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Number of restarts: " << number_of_restarts << std::endl
            << std::setw(width) << std::left << "Number of perturbations: " << number_of_perturbations << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Output::to_json();
        json.merge_patch({
                {"NumberOfRestarts", this->number_of_restarts},
                {"NumberOfPerturbations", this->number_of_perturbations},
                });
        return json;
    }
};

const LocalSearchOutput local_search(
        const Instance& instance,
        std::mt19937_64& generator,
        const Solution* initial_solution = nullptr,
        const LocalSearchParameters& parameters = {});

}
