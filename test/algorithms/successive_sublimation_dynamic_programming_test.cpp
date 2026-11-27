#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/successive_sublimation_dynamic_programming.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

using namespace parallelschedulingsolver;

namespace
{

SuccessiveSublimationDynamicProgrammingParameters default_parameters()
{
    SuccessiveSublimationDynamicProgrammingParameters parameters;
    parameters.verbosity_level = 0;
    return parameters;
}

/**
 * Parameters with weak upper bounds and a single iteration of stage 1, so
 * that the optimality is proved in stage 2.
 */
SuccessiveSublimationDynamicProgrammingParameters stage_2_parameters()
{
    SuccessiveSublimationDynamicProgrammingParameters parameters = default_parameters();
    parameters.maximum_number_of_stage_1_iterations = 1;
    parameters.initial_local_search_number_of_restarts = 0;
    parameters.lagrangian_heuristic_local_search = false;
    return parameters;
}

Output ssdp_test(
        const Instance& instance,
        const SuccessiveSublimationDynamicProgrammingParameters& parameters)
{
    return successive_sublimation_dynamic_programming(instance, parameters);
}

}

// The solution depends on the upper bound heuristics, so there are no exact
// schedule tests.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(SuccessiveSublimationDynamicProgramming, UnsupportedInstances)
{
    Instance parallel = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 0, 1}});
    EXPECT_THROW(ssdp_test(parallel, default_parameters()), std::invalid_argument);
    Instance makespan = build_instance(Objective::Makespan, 1, {{{1}}});
    EXPECT_THROW(ssdp_test(makespan, default_parameters()), std::invalid_argument);
    Instance release_dates = build_instance(Objective::TotalTardiness, 1, {{{1}, 1, 1}});
    EXPECT_THROW(ssdp_test(release_dates, default_parameters()), std::invalid_argument);
}

TEST(SuccessiveSublimationDynamicProgramming, EmptyInstance)
{
    Instance instance = build_instance(Objective::TotalTardiness, 1, {});
    Output output = ssdp_test(instance, default_parameters());
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.objective_value(), 0);
    EXPECT_EQ(output.bound(), 0);
}

TEST(SuccessiveSublimationDynamicProgramming, ZeroCostJobs)
{
    // Jobs 1 and 2 have a zero weight and satisfy the condition of Section
    // 5.1: they are removed and scheduled last.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::TotalTardiness);
    instance_builder.set_number_of_machines(1);
    std::vector<std::pair<Time, Time>> jobs = {{3, 2}, {2, 1}, {4, 1}};
    for (JobId job_id = 0; job_id < 3; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, jobs[job_id].first);
        instance_builder.set_job_due_date(job_id, jobs[job_id].second);
        instance_builder.set_job_weight(job_id, (job_id == 0)? 1: 0);
    }
    instance_builder.set_setup_time(0, -1, 0, 1);
    Instance instance = instance_builder.build();
    SuccessiveSublimationDynamicProgrammingOutput output
        = successive_sublimation_dynamic_programming(instance, default_parameters());
    EXPECT_EQ(output.number_of_removed_jobs, 2);
    // Job 0 first, completed at 1 + 3 = 4.
    EXPECT_EQ(output.solution.total_tardiness(), 2);
    EXPECT_EQ(output.bound(), 2);
}


TEST(SuccessiveSublimationDynamicProgramming, RejectionCosts)
{
    SuccessiveSublimationDynamicProgrammingParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(successive_sublimation_dynamic_programming(rejection_costs_instance(Objective::TotalTardiness), parameters), std::invalid_argument);
}

// Optimal values computed by brute force.
INSTANTIATE_TEST_SUITE_P(
        SuccessiveSublimationDynamicProgramming,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return ssdp_test(instance, default_parameters());
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = default_parameters();
                        parameters.column_generation = false;
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        parameters.constraint_propagation = false;
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        parameters.minimum_branching_depth = 2;
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        parameters.maximum_number_of_refinement_steps = 1;
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        parameters.tailing_off_number_of_steps = 1;
                        parameters.tailing_off_minimum_improvement = 1;
                        return ssdp_test(instance, parameters);
                    },
                    [](const Instance& instance)
                    {
                        SuccessiveSublimationDynamicProgrammingParameters parameters = stage_2_parameters();
                        parameters.maximum_number_of_refinement_steps = 1;
                        parameters.warm_start = true;
                        return ssdp_test(instance, parameters);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                [](const Output& output) { return output.bound(); })),
        exact_algorithm_test_name);
