#pragma once

#include "parallelschedulingsolver/solution_builder.hpp"

namespace parallelschedulingsolver
{

struct Output: optimizationtools::Output
{
    /** Constructor. */
    Output(const Instance& instance):
        solution(SolutionBuilder().set_instance(instance).build())
    {
        // Trivial upper bound on the throughput: all jobs with a due date are
        // on time.
        throughput_bound = 0;
        for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id)
            if (instance.job(job_id).due_date >= 0)
                throughput_bound += instance.job(job_id).weight;
    }


    /** Time at which the last improvement was found, in seconds. */
    double time = 0;

    Solution solution;

    /*
     * Bounds, initialized to trivial valid bounds. When the bound of the
     * objective equals the value of the solution, the solution is optimal.
     */

    /** Lower bound on the makespan. */
    Time makespan_bound = 0;

    /** Lower bound on the total (weighted) flow time. */
    Time total_flow_time_bound = 0;

    /** Lower bound on the total (weighted) tardiness. */
    Time total_tardiness_bound = 0;

    /** Lower bound on the maximum lateness. */
    Time maximum_lateness_bound = std::numeric_limits<Time>::min();

    /** Upper bound on the (weighted) throughput. */
    Time throughput_bound = 0;


    double bound() const
    {
        switch (solution.instance().objective()) {
        case Objective::Makespan:
            return this->makespan_bound;
        case Objective::TotalFlowTime:
            return this->total_flow_time_bound;
        case Objective::TotalTardiness:
            return this->total_tardiness_bound;
        case Objective::MaximumLateness:
            return this->maximum_lateness_bound;
        case Objective::Throughput:
            return this->throughput_bound;
        }
        return -1;
    }

    virtual nlohmann::json to_json() const
    {
        return nlohmann::json {
            {"Solution", this->solution.to_json()},
            {"MakespanBound", this->makespan_bound},
            {"TotalFlowTimeBound", this->total_flow_time_bound},
            {"TotalTardinessBound", this->total_tardiness_bound},
            {"MaximumLatenessBound", this->maximum_lateness_bound},
            {"ThroughputBound", this->throughput_bound},
            {"Bound", this->bound()},
            {"Value", this->solution.objective_value()},
            {"Optimal", this->bound() == this->solution.objective_value()},
            {"Time", this->time}
        };
    }

    virtual int format_width() const { return 11; }

    virtual void format(std::ostream& os) const
    {
        int width = format_width();
        os
            << std::setw(width) << std::left << "Time (s): " << time << std::endl
            ;
    }
};

using NewSolutionCallback = std::function<void(const Output&)>;

struct Parameters: optimizationtools::Parameters
{
    /** New solution callback. */
    NewSolutionCallback new_solution_callback = [](const Output&) { };

    /** Path of the log file of the external solver (MILP); empty: no log. */
    std::string solver_log_path = "";
};

class AlgorithmFormatter
{

public:

    /** Constructor. */
    AlgorithmFormatter(
            const Instance& instance,
            const Parameters& parameters,
            Output& output):
        instance_(instance),
        parameters_(parameters),
        output_(output),
        os_(parameters.create_os()) { }

    /** Print the header. */
    void start(
            const std::string& algorithm_name);

    /** Print the header. */
    void print_header();

    /** Print current state. */
    void print(
            const std::string& s);

    /** Update the solution. */
    void update_solution(
            const Solution& solution,
            const std::string& s);

    /** Update makespan bound. */
    void update_makespan_bound(
            Time bound,
            const std::string& s);

    /** Update total flow time bound. */
    void update_total_flow_time_bound(
            Time bound,
            const std::string& s);

    /** Update total tardiness bound. */
    void update_total_tardiness_bound(
            Time bound,
            const std::string& s);

    /** Update maximum lateness bound. */
    void update_maximum_lateness_bound(
            Time bound,
            const std::string& s);

    /** Update throughput bound. */
    void update_throughput_bound(
            Time bound,
            const std::string& s);

    /** Method to call at the end of the algorithm. */
    void end();

private:

    /** Update a bound if the new one is strictly better. */
    void update_bound(
            Time& bound_cur,
            Time bound_new,
            optimizationtools::ObjectiveDirection objective_direction,
            const std::string& s);

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    const Parameters& parameters_;

    /** Output. */
    Output& output_;

    /** Output stream. */
    std::unique_ptr<optimizationtools::ComposeStream> os_;

};

}
