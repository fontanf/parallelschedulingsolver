#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/milp_positional.hpp"

using namespace parallelschedulingsolver;

namespace
{

Output milp_positional_test(const Instance& instance)
{
    MilpPositionalParameters parameters;
    parameters.verbosity_level = 0;
    return milp_positional(instance, parameters);
}

}


TEST(MilpPositional, RejectionCosts)
{
    MilpPositionalParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(milp_positional(rejection_costs_instance(Objective::TotalFlowTime), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        MilpPositional,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Waiting for job 1, released at 1, is optimal:
                // C = 2 and 12 instead of 10 and 11.
                "ReleaseDates",
                milp_positional_test,
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
                milp_positional_test,
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
                milp_positional_test,
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
                milp_positional_test,
                Objective::MaximumLateness,
                1,
                {{{2}, 0, 2}, {{1}, 0, 4}},
                {{{0, 0}, {1, 2}}},
                0,
                true,
            },
            {
                // Job 2 is only fast on machine 0, and is on time there if it
                // is first. One of jobs 0 and 1 is then late: job 1, of
                // smaller weight, after job 2 (tardiness 1).
                "WeightedTardinessTwoMachines",
                milp_positional_test,
                Objective::TotalTardiness,
                2,
                {{{3, 3}, 0, 3, 2}, {{3, 3}, 0, 3, 1}, {{1, 5}, 0, 1, 1}},
                {{{2, 0}, {1, 1}}, {{0, 0}}},
                1,
                true,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        MilpPositional,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    milp_positional_test,
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "milp_positional_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return output.bound(); })),
        exact_algorithm_test_name);
