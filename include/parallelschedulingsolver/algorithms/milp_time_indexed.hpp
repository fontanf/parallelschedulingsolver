/**
 * Time-indexed MILP model
 *
 * Variable x_{i, j, s} = 1 iff job j starts at time s on machine i. Each
 * objective of the repository is a sum of costs of the start times of the
 * jobs (or a maximum of them, for the makespan and the maximum lateness), so
 * it is linear in these variables. Release dates are taken into account by
 * the possible start times.
 *
 * The capacity constraints of each machine are written as a flow: the nodes
 * are the possible start and completion times of the machine, x_{i, j, s} is
 * an arc from s to s + p_{i, j}, and idle arcs link consecutive nodes. A unit
 * of flow goes from the first node to the last one. This has the same linear
 * relaxation as the classical constraints "at most one job in process at each
 * time", which form an interval matrix, but with far fewer non-zeros.
 *
 * Normal instants: all objectives are regular, so there exists an optimal
 * semi-active schedule, in which a job starts at a release date, or at time
 * 0, plus the sum of the processing times of some of the other jobs on its
 * machine. With 'normal_instants', the possible start times of job j on
 * machine i are restricted to these times, computed by a subset sum dynamic
 * program. Otherwise, all integer times between r_j and the same horizon are
 * possible start times.
 */

#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace parallelschedulingsolver
{

struct MilpTimeIndexedParameters: Parameters
{
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /** Restrict the start times to the normal instants. */
    bool normal_instants = true;


    virtual int format_width() const override { return 37; }

    virtual void format(std::ostream& os) const override
    {
        Parameters::format(os);
        int width = format_width();
        os
            << std::setw(width) << std::left << "Solver: " << solver << std::endl
            << std::setw(width) << std::left << "Normal instants: " << normal_instants << std::endl
            ;
    }

    virtual nlohmann::json to_json() const override
    {
        nlohmann::json json = Parameters::to_json();
        json.merge_patch({
                {"NormalInstants", normal_instants},
                });
        return json;
    }
};

Output milp_time_indexed(
        const Instance& instance,
        const MilpTimeIndexedParameters& parameters = {});

}
