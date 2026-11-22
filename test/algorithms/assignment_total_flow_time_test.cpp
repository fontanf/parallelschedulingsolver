#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/assignment_total_flow_time.hpp"

using namespace parallelschedulingsolver;


TEST(AssignmentTotalFlowTime, RejectionCosts)
{
    Parameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(assignment_total_flow_time(rejection_costs_instance(Objective::TotalFlowTime), parameters), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        AssignmentTotalFlowTime,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Single machine: SPT order.
                "SingleMachine",
                [](const Instance& instance) { return assignment_total_flow_time(instance); },
                Objective::TotalFlowTime,
                1,
                {{{3}}, {{1}}, {{2}}},
                {{{1, 0}, {2, 1}, {0, 3}}},
                1 + 3 + 6,
                true,
            },
            {
                // The optimal solution puts each job on its own machine
                // (2 + 1). Greedy SPT puts job 1 on machine 0 (tie) and then
                // job 0 after it on machine 0 (1 + 3).
                "UnrelatedMachines",
                [](const Instance& instance) { return assignment_total_flow_time(instance); },
                Objective::TotalFlowTime,
                2,
                {{{2, 10}}, {{1, 1}}},
                {{{0, 0}}, {{1, 0}}},
                2 + 1,
                true,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        AssignmentTotalFlowTime,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return assignment_total_flow_time(instance);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "assignment_total_flow_time_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.total_flow_time_bound; })),
        exact_algorithm_test_name);
