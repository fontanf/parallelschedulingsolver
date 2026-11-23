#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"
#include "parallelschedulingsolver/algorithms/brute_force.hpp"

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


/** Copy of an instance with rejection costs for the jobs of even id. */
Instance with_rejection_costs(const Instance& instance)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(instance.objective());
    instance_builder.set_number_of_machines(1);
    JobId n = instance.number_of_jobs();
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, job.processing_times[0]);
        if (job.due_date >= 0)
            instance_builder.set_job_due_date(job_id, job.due_date);
        instance_builder.set_job_weight(job_id, job.weight);
        if (job_id % 2 == 0)
            instance_builder.set_job_rejection_cost(job_id, 5 + 23 * job_id);
    }
    if (instance.has_setup_times()) {
        for (JobId job_id_1 = -1; job_id_1 < n; ++job_id_1)
            for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2)
                instance_builder.set_setup_time(0, job_id_1, job_id_2, instance.setup_time(0, job_id_1, job_id_2));
    }
    return instance_builder.build();
}

/** Small single machine test instances (at most 6 jobs), with rejection costs. */
std::vector<Instance> rejection_costs_instances()
{
    std::vector<Instance> instances;
    for (const TestInstancePath& files: get_test_instance_paths(
                get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"}))) {
        const Instance instance = get_instance(files);
        if (instance.number_of_jobs() > 6)
            continue;
        instances.push_back(with_rejection_costs(instance));
    }
    return instances;
}

double brute_force_value(const Instance& instance)
{
    BruteForceParameters parameters;
    parameters.verbosity_level = 0;
    return brute_force(instance, parameters).solution.objective_value();
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

TEST(DynamicProgrammingTimeIndexed, RejectionCostsAllJobsRejected)
{
    // Rejecting both jobs (1 + 1) is better than scheduling them: the empty
    // schedule.
    Instance instance = build_instance(
            Objective::TotalTardiness,
            1,
            {{{5}, 0, 2, 1, 1}, {{5}, 0, 3, 1, 1}});
    DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
    parameters.all_jobs_in_modifiers = true;
    DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(instance, parameters);
    EXPECT_TRUE(output.path.empty());
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.number_of_rejected_jobs(), 2);
    EXPECT_EQ(output.solution.objective_value(), 2);
    EXPECT_EQ(output.bound(), 2);
}

TEST(DynamicProgrammingTimeIndexed, RejectionCostsExact)
{
    // With all the jobs in M, at once or with a chain of calls, the optimal
    // value of the brute force.
    std::vector<Instance> instances = rejection_costs_instances();
    ASSERT_FALSE(instances.empty());
    JobId number_of_rejected_jobs = 0;
    for (const Instance& instance: instances) {
        double optimal_value = brute_force_value(instance);
        Output output = exact_test(instance);
        EXPECT_EQ(output.solution.objective_value(), optimal_value);
        EXPECT_EQ(output.bound(), optimal_value);
        number_of_rejected_jobs += output.solution.number_of_rejected_jobs();
        Output output_chain = chain_test(instance);
        EXPECT_EQ(output_chain.solution.objective_value(), optimal_value);
        EXPECT_EQ(output_chain.bound(), optimal_value);
    }
    // Some jobs are rejected in the optimal solutions.
    EXPECT_GT(number_of_rejected_jobs, 0);
}

TEST(DynamicProgrammingTimeIndexed, LagrangianBoundsWithMultipliers)
{
    // With multipliers, the bound is at most the optimal value whatever the
    // jobs of M, and equal to it with all the jobs in M; with and without
    // rejection costs, with the network generated in both directions.
    std::vector<Instance> instances = rejection_costs_instances();
    for (const TestInstancePath& files: get_test_instance_paths(
                get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"}))) {
        const Instance instance = get_instance(files);
        if (instance.number_of_jobs() <= 6)
            instances.push_back(get_instance(files));
    }
    for (const Instance& instance: instances) {
        double optimal_value = brute_force_value(instance);
        std::shared_ptr<const Problem> problem = build_problem(instance);
        JobId n = problem->number_of_jobs;
        if (n == 0)
            continue;
        for (int k = 0; k < 3; ++k) {
            std::vector<double> multipliers(n);
            for (JobId job_id = 0; job_id < n; ++job_id)
                multipliers[job_id] = ((job_id * 7 + k * 13) % 11 - 5) * 7.5;
            for (JobId number_of_modifiers: {(JobId)0, n / 2, n})
            for (Direction direction: {Direction::Forward, Direction::Backward}) {
                DynamicProgrammingTimeIndexedParameters parameters = default_parameters();
                parameters.multipliers = multipliers;
                parameters.direction = direction;
                for (JobId job_id = 0; job_id < number_of_modifiers; ++job_id)
                    parameters.new_modifier_jobs.push_back(job_id);
                DynamicProgrammingTimeIndexedOutput output = dynamic_programming_time_indexed(instance, parameters);
                if (number_of_modifiers == n) {
                    EXPECT_NEAR(output.lagrangian_bound, optimal_value, 1e-6);
                } else {
                    EXPECT_LE(output.lagrangian_bound, optimal_value + 1e-6);
                }
            }
        }
    }
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
