#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"

using namespace parallelschedulingsolver;


TEST(GreedyShortestProcessingTime, RejectionCosts)
{
    Parameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(greedy_shortest_processing_time(rejection_costs_instance(Objective::TotalFlowTime), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        GreedyShortestProcessingTime,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Ratios p / w: 3, 1, 1, 2; ties keep the job order.
                "SingleMachineWeighted",
                [](const Instance& instance) { return greedy_shortest_processing_time(instance); },
                Objective::TotalFlowTime,
                1,
                {{{3}, 0, -1, 1}, {{1}, 0, -1, 1}, {{4}, 0, -1, 4}, {{2}, 0, -1, 1}},
                {{{1, 0}, {2, 1}, {3, 5}, {0, 7}}},
                1 * 1 + 4 * 5 + 1 * 7 + 1 * 10,
                true,
            },
            {
                // Order: 1 (1), 3 (2), 2 (3), 0 (4), 4 (5).
                "IdenticalMachines",
                [](const Instance& instance) { return greedy_shortest_processing_time(instance); },
                Objective::TotalFlowTime,
                2,
                {{{4, 4}}, {{1, 1}}, {{3, 3}}, {{2, 2}}, {{5, 5}}},
                {{{1, 0}, {2, 1}, {4, 4}}, {{3, 0}, {0, 2}}},
                1 + 4 + 9 + 2 + 6,
                true,
            },
            {
                // Job 1 is first but released at 4, so job 0 waits on machine
                // 0 or goes to machine 1, whichever completes first.
                "ReleaseDates",
                [](const Instance& instance) { return greedy_shortest_processing_time(instance); },
                Objective::TotalFlowTime,
                2,
                {{{3, 6}}, {{1, 1}, 4}},
                {{{1, 4}}, {{0, 0}}},
                (5 - 4) + 6,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        GreedyShortestProcessingTime,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return greedy_shortest_processing_time(instance);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "greedy_shortest_processing_time_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.total_flow_time_bound; })),
        exact_algorithm_test_name);
