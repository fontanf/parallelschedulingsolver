#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/milp_time_indexed.hpp"

using namespace parallelschedulingsolver;

namespace
{

Output milp_time_indexed_test(
        const Instance& instance,
        bool normal_instants)
{
    MilpTimeIndexedParameters parameters;
    parameters.verbosity_level = 0;
    parameters.normal_instants = normal_instants;
    return milp_time_indexed(instance, parameters);
}

}


TEST(MilpTimeIndexed, RejectionCosts)
{
    MilpTimeIndexedParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(milp_time_indexed(rejection_costs_instance(Objective::TotalFlowTime), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        MilpTimeIndexed,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Waiting for job 1, released at 1, is optimal:
                // C = 2 and 12 instead of 10 and 11.
                "ReleaseDates",
                [](const Instance& instance) { return milp_time_indexed_test(instance, true); },
                Objective::TotalFlowTime,
                1,
                {{{10}, 0}, {{1}, 1}},
                {{{1, 1}, {0, 2}}},
                (2 - 1) + 12,
                true,
            },
            {
                // Job 1 (weight 5) must be first to be on time; job 0 is late
                // and starts right after it.
                "Throughput",
                [](const Instance& instance) { return milp_time_indexed_test(instance, true); },
                Objective::Throughput,
                1,
                {{{3}, 0, 3, 1}, {{2}, 0, 2, 5}},
                {{{1, 0}, {0, 2}}},
                5,
                true,
            },
            {
                // Each job is fast on one machine.
                "UnrelatedMachines",
                [](const Instance& instance) { return milp_time_indexed_test(instance, true); },
                Objective::Makespan,
                2,
                {{{1, 10}}, {{10, 1}}},
                {{{0, 0}}, {{1, 0}}},
                1,
                true,
            },
            {
                // EDD order; job 1 starts right after job 0.
                "MaximumLateness",
                [](const Instance& instance) { return milp_time_indexed_test(instance, true); },
                Objective::MaximumLateness,
                1,
                {{{2}, 0, 2}, {{1}, 0, 4}},
                {{{0, 0}, {1, 2}}},
                0,
                true,
            },
            {
                // Same as ReleaseDates, without normal instants.
                "ReleaseDatesAllInstants",
                [](const Instance& instance) { return milp_time_indexed_test(instance, false); },
                Objective::TotalFlowTime,
                1,
                {{{10}, 0}, {{1}, 1}},
                {{{1, 1}, {0, 2}}},
                (2 - 1) + 12,
                true,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        MilpTimeIndexed,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance) { return milp_time_indexed_test(instance, true); },
                    [](const Instance& instance) { return milp_time_indexed_test(instance, false); },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "milp_time_indexed_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return output.bound(); })),
        exact_algorithm_test_name);
