#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

namespace
{

DynamicProgrammingTimeIndexedParameters default_parameters()
{
    DynamicProgrammingTimeIndexedParameters parameters;
    parameters.verbosity_level = 0;
    return parameters;
}

/** All the jobs in M at once, from the initial network. */
Output exact_test(const Instance& instance)
{
    DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
    parameters.all_jobs_in_modifiers = true;
    return dynamic_programming_time_indexed(instance, parameters);
}

/**
 * Chain of calls, each one starting from the network of the previous one:
 * the relaxation (LR2) with the labels of both directions, the first half of
 * the jobs in M (backward), then the other half (forward), with a cutoff, the
 * reduction and the constraint propagation.
 */
Output chain_test(const Instance& instance)
{
    // Cutoff: the cost of the jobs in the order of the instance, which
    // keeps the optimal schedules.
    std::shared_ptr<const Problem> problem = build_problem(instance);
    std::vector<JobId> sequence(problem->number_of_jobs);
    for (JobId job_id = 0; job_id < problem->number_of_jobs; ++job_id)
        sequence[job_id] = job_id;
    double cutoff = (double)problem->sequence_cost(sequence);

    DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
    parameters.cutoff = cutoff;
    parameters.reduction = true;
    parameters.constraint_propagation = true;
    parameters.both_directions_labels = true;
    DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(instance, parameters);
    if (output.network == nullptr || output.network->number_of_states() == 0)
        return output;

    JobId n = output.network->problem().number_of_jobs;
    std::vector<JobId> first_half;
    std::vector<JobId> second_half;
    for (JobId job_id = 0; job_id < n; ++job_id)
        ((job_id < n / 2)? first_half: second_half).push_back(job_id);

    DynamicProgrammingTimeIndexedParameters parameters_2 = default_parameters();
    parameters_2.network = output.network;
    parameters_2.new_modifier_jobs = first_half;
    parameters_2.direction = Direction::Backward;
    parameters_2.cutoff = cutoff;
    parameters_2.constraint_propagation = true;
    DynamicProgrammingTimeIndexedOutput output_2 = dynamic_programming_time_indexed(instance, parameters_2);

    DynamicProgrammingTimeIndexedParameters parameters_3 = default_parameters();
    parameters_3.network = output_2.network;
    parameters_3.new_modifier_jobs = second_half;
    parameters_3.direction = Direction::Forward;
    parameters_3.cutoff = cutoff;
    return dynamic_programming_time_indexed(instance, parameters_3);
}

}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(DynamicProgrammingTimeIndexed, UnsupportedInstances)
{
    Instance parallel = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 0, 1}});
    EXPECT_THROW(dynamic_programming_time_indexed(parallel, default_parameters()), std::invalid_argument);
    Instance makespan = build_instance(Objective::Makespan, 1, {{{1}}});
    EXPECT_THROW(dynamic_programming_time_indexed(makespan, default_parameters()), std::invalid_argument);
    Instance release_dates = build_instance(Objective::TotalTardiness, 1, {{{1}, 1, 1}});
    EXPECT_THROW(dynamic_programming_time_indexed(release_dates, default_parameters()), std::invalid_argument);
    EXPECT_THROW(
            dynamic_programming_time_indexed(
                rejection_costs_instance(Objective::TotalTardiness),
                default_parameters()),
            std::invalid_argument);
}

TEST(DynamicProgrammingTimeIndexed, EmptyInstance)
{
    Instance instance = build_instance(Objective::TotalTardiness, 1, {});
    Output output = dynamic_programming_time_indexed(instance, default_parameters());
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.objective_value(), 0);
    EXPECT_EQ(output.bound(), 0);
}

TEST(DynamicProgrammingTimeIndexed, RelaxationBound)
{
    // Without modifiers, the bound of the relaxation (LR2) is at most the
    // optimal value, and is the bound of the output.
    for (const TestInstancePath& files: get_test_instance_paths(
                get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"}))) {
        const Instance instance = get_instance(files);
        DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(
                instance,
                default_parameters());
        EXPECT_LE(output.lagrangian_bound, files.objective_value + 1e-6) << files.instance_path;
        EXPECT_EQ(output.bound(), std::ceil(output.lagrangian_bound - 1e-6)) << files.instance_path;
    }
}

TEST(DynamicProgrammingTimeIndexed, Restriction)
{
    // Job 0 (p = 2, d = 2) completed at 2 or 3 at most: the bound of the
    // restricted network is not a bound of the instance.
    Instance instance = build_instance(
            Objective::TotalTardiness,
            1,
            {{{2}, 0, 2}, {{3}, 0, 3}});
    DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
    parameters.all_jobs_in_modifiers = true;
    parameters.completion_time_windows = {{0, 5, 5}};
    DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(instance, parameters);
    // Job 1 then job 0: tardiness 3 of job 0.
    EXPECT_EQ(output.lagrangian_bound, 3);
    EXPECT_TRUE(output.network->restricted());
    EXPECT_EQ(output.solution.objective_value(), 3);
    EXPECT_EQ(output.bound(), 0);
}

TEST(DynamicProgrammingTimeIndexed, MemoryOut)
{
    // All the jobs in M at once, with 1 kB of memory.
    const Instance instance = get_instance({
            "tests/total_weighted_tardiness_single_setup_times/twt_n1x5_s0.json",
            "json"});
    DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
    parameters.all_jobs_in_modifiers = true;
    parameters.maximum_memory = 1e-3;
    DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(instance, parameters);
    EXPECT_TRUE(output.memory_out);
    EXPECT_FALSE(output.solution.feasible());
}

// Optimal values computed by brute force.
INSTANTIATE_TEST_SUITE_P(
        DynamicProgrammingTimeIndexed,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    exact_test,
                    chain_test,
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                [](const Output& output) { return output.bound(); })),
        exact_algorithm_test_name);
