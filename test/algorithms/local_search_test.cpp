#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/local_search.hpp"

using namespace parallelschedulingsolver;

namespace
{

Output local_search_test(const Instance& instance)
{
    std::mt19937_64 generator(0);
    LocalSearchParameters parameters;
    parameters.verbosity_level = 0;
    return local_search(instance, generator, nullptr, parameters);
}

}

// The solution of the local search can't be predicted, so there are no exact
// schedule tests.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(LocalSearch, UnsupportedObjective)
{
    Instance instance = build_instance(Objective::Makespan, 1, {{{1}}});
    EXPECT_THROW(local_search_test(instance), std::invalid_argument);
}

TEST(LocalSearch, EmptyInstance)
{
    Instance instance = build_instance(Objective::TotalTardiness, 2, {});
    Output output = local_search_test(instance);
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.objective_value(), 0);
}


TEST(LocalSearch, RejectionCosts)
{
    EXPECT_THROW(local_search_test(rejection_costs_instance(Objective::TotalTardiness)), std::invalid_argument);
}

// The instances are small enough for the local search to always find an
// optimal solution. The local search doesn't prove optimality, so the bound
// is not checked.
INSTANTIATE_TEST_SUITE_P(
        LocalSearch,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    local_search_test,
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "local_search_test.txt"})),
                    get_test_instance_paths(get_path({"test", "algorithms", "local_search_parallel_tardiness_test.txt"})),
                    get_test_instance_paths(get_path({"test", "algorithms", "local_search_setup_times_test.txt"})),
                })),
        exact_algorithm_test_name);
