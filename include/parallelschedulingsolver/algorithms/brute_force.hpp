/**
 * Brute force
 *
 * Exact algorithm for all the problems of the repository, by enumeration of
 * the solutions. It is only practical for very small instances, but its
 * correctness is easy to trust, so it gives the reference optimal values of
 * the tests of the other algorithms.
 *
 * All the objectives are regular, so for a given sequence of jobs on each
 * machine, starting each job as early as possible is dominant: a job starts
 * at max(completion of the previous job + setup time, release date).
 *
 * - In general, all the sequences of all the machines are enumerated: all
 *   the orders of the jobs, each split into one (possibly empty) sequence per
 *   machine, i.e. (n + m - 1)! / (m - 1)! candidates (n! on a single
 *   machine).
 * - On parallel machines without setup times, when a sequencing rule is
 *   optimal for a fixed assignment of the jobs to the machines, all the m^n
 *   assignments are enumerated and the jobs of each machine are sequenced
 *   with the rule:
 *   - makespan: earliest release date first;
 *   - total (weighted) flow time without release dates: weighted shortest
 *     processing time first (Smith, 1956);
 *   - maximum lateness without release dates: earliest due date first
 *     (Jackson, 1955).
 *
 * The algorithm proves optimality when the enumeration completes.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

namespace parallelschedulingsolver
{

struct BruteForceParameters: Parameters
{
    /**
     * Maximum number of candidates to enumerate.
     *
     * If the instance has more candidates, the algorithm returns without
     * solution. If negative, no limit.
     */
    Counter maximum_number_of_candidates = -1;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Max. # of candidates: " << maximum_number_of_candidates << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"MaximumNumberOfCandidates", maximum_number_of_candidates}});
        return json;
    }
};

Output brute_force(
        const Instance& instance,
        const BruteForceParameters& parameters = {});

}
