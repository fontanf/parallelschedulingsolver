#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"

using namespace parallelschedulingsolver;


TEST(GreedyEarliestDueDate, RejectionCosts)
{
    Parameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(greedy_earliest_due_date(rejection_costs_instance(Objective::TotalTardiness), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        GreedyEarliestDueDate,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Jobs 1 and 3 have the same due date; the shortest one goes
                // first. Job 4 has no due date and goes last.
                "SingleMachine",
                [](const Instance& instance) { return greedy_earliest_due_date(instance); },
                Objective::MaximumLateness,
                1,
                {{{3}, 0, 10}, {{2}, 0, 4}, {{1}, 0, 7}, {{1}, 0, 4}, {{1}, 0, -1}},
                {{{3, 0}, {1, 1}, {2, 3}, {0, 4}, {4, 7}}},
                -1,
                true,
            },
            {
                // Order: 1 (d = 2), 2 (d = 3), 0 (d = 5).
                "IdenticalMachines",
                [](const Instance& instance) { return greedy_earliest_due_date(instance); },
                Objective::MaximumLateness,
                2,
                {{{2, 2}, 0, 5}, {{3, 3}, 0, 2}, {{2, 2}, 0, 3}},
                {{{1, 0}}, {{2, 0}, {0, 2}}},
                1,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        GreedyEarliestDueDate,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return greedy_earliest_due_date(instance);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "greedy_earliest_due_date_test.txt"})),
                },
                // Maximum lateness.
                [](const Solution& solution) { return (double)solution.maximum_lateness(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.maximum_lateness_bound; })),
        exact_algorithm_test_name);
