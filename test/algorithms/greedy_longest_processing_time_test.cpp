#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"

using namespace parallelschedulingsolver;


TEST(GreedyLongestProcessingTime, RejectionCosts)
{
    Parameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(greedy_longest_processing_time(rejection_costs_instance(Objective::Makespan), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        GreedyLongestProcessingTime,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Order: 1 (5), 3 (4), 2 (3), 0 (2), 4 (1).
                // Job 4 completes at 8 on both machines; ties go to the first
                // machine.
                "IdenticalMachines",
                [](const Instance& instance) { return greedy_longest_processing_time(instance); },
                Objective::Makespan,
                2,
                {{{2, 2}}, {{5, 5}}, {{3, 3}}, {{4, 4}}, {{1, 1}}},
                {{{1, 0}, {0, 5}, {4, 7}}, {{3, 0}, {2, 4}}},
                8,
            },
            {
                // Same processing times: ties keep the job order.
                "Ties",
                [](const Instance& instance) { return greedy_longest_processing_time(instance); },
                Objective::Makespan,
                2,
                {{{3, 3}}, {{3, 3}}, {{3, 3}}},
                {{{0, 0}, {2, 3}}, {{1, 0}}},
                6,
                true,
            },
            {
                // Jobs are sorted by their smallest processing time:
                // 1 (4), 2 (2), 0 (1). Job 2 is released at 5 and completes at
                // 7 on both machines.
                "UnrelatedMachinesReleaseDates",
                [](const Instance& instance) { return greedy_longest_processing_time(instance); },
                Objective::Makespan,
                2,
                {{{3, 1}}, {{4, 6}}, {{2, 2}, 5}},
                {{{1, 0}, {2, 5}}, {{0, 0}}},
                7,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        GreedyLongestProcessingTime,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return greedy_longest_processing_time(instance);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "greedy_longest_processing_time_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.makespan_bound; })),
        exact_algorithm_test_name);
