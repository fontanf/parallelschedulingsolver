#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/brute_force.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

#include <list>

using namespace parallelschedulingsolver;

namespace
{

Output brute_force_test(const Instance& instance)
{
    BruteForceParameters parameters;
    parameters.verbosity_level = 0;
    return brute_force(instance, parameters);
}

/**
 * Copy of an instance with the maximum lateness objective.
 *
 * The copies are kept until the end of the tests, since the solutions of the
 * outputs refer to their instance.
 */
const Instance& maximum_lateness_instance(const Instance& instance)
{
    static std::list<Instance> instances;
    std::vector<TestJob> jobs;
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        jobs.push_back({
                job.processing_times,
                job.release_date,
                job.due_date,
                job.weight});
    }
    instances.push_back(build_instance(
            Objective::MaximumLateness,
            instance.number_of_machines(),
            jobs));
    return instances.back();
}

/** Sequence of the jobs of a machine. */
std::vector<JobId> machine_sequence(
        const Solution& solution,
        MachineId machine_id)
{
    std::vector<JobId> sequence;
    for (const auto& scheduled_job: solution.machine(machine_id).jobs)
        sequence.push_back(scheduled_job.job_id);
    return sequence;
}

}

// Several optimal schedules are usually possible, and the one returned
// depends on the order of the enumeration, so the tests check the values.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(BruteForce, EmptyInstance)
{
    Instance instance = build_instance(Objective::TotalTardiness, 2, {});
    Output output = brute_force_test(instance);
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.objective_value(), 0);
    EXPECT_EQ(output.bound(), 0);
}

TEST(BruteForce, MaximumNumberOfCandidates)
{
    // 3 jobs on 2 machines: 4! / 1! = 24 arrangements of the jobs and of the
    // separator.
    Instance instance = build_instance(
            Objective::TotalTardiness,
            2,
            {{{1, 1}, 0, 1}, {{1, 1}, 0, 1}, {{1, 1}, 0, 1}});
    BruteForceParameters parameters;
    parameters.verbosity_level = 0;
    parameters.maximum_number_of_candidates = 23;
    Output output = brute_force(instance, parameters);
    EXPECT_FALSE(output.solution.feasible());
    parameters.maximum_number_of_candidates = 24;
    output = brute_force(instance, parameters);
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.objective_value(), 1);
    EXPECT_EQ(output.bound(), 1);
}

TEST(BruteForce, SetupTimesSingleMachine)
{
    // All the setup times are 10, except the initial setup of job 2 and the
    // setups 2 -> 0 and 0 -> 1, which are 0. The only sequence without a
    // setup of 10 is 2, 0, 1.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Makespan);
    instance_builder.set_number_of_machines(1);
    for (JobId job_id = 0; job_id < 3; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, 1);
    }
    for (JobId job_id_1 = -1; job_id_1 < 3; ++job_id_1)
        for (JobId job_id_2 = 0; job_id_2 < 3; ++job_id_2)
            if (job_id_1 != job_id_2)
                instance_builder.set_setup_time(0, job_id_1, job_id_2, 10);
    instance_builder.set_setup_time(0, -1, 2, 0);
    instance_builder.set_setup_time(0, 2, 0, 0);
    instance_builder.set_setup_time(0, 0, 1, 0);
    Instance instance = instance_builder.build();

    Output output = brute_force_test(instance);
    EXPECT_EQ(machine_sequence(output.solution, 0), (std::vector<JobId>{2, 0, 1}));
    EXPECT_EQ(output.solution.makespan(), 3);
    EXPECT_EQ(output.bound(), 3);
}

TEST(BruteForce, SetupTimesParallelMachines)
{
    // Two jobs of 5 on two machines. On machine 0, no initial setup but a
    // setup of 100 between the jobs; on machine 1, initial setups of 3. The
    // optimal schedules have one job on each machine: makespan 3 + 5 = 8.
    // Without the setups, a rule would sequence the jobs of each machine; the
    // setups also forbid two jobs on machine 0 (5 + 100 + 5).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Makespan);
    instance_builder.set_number_of_machines(2);
    for (JobId job_id = 0; job_id < 2; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, 5);
        instance_builder.set_job_processing_time(job_id, 1, 5);
        instance_builder.set_setup_time(1, -1, job_id, 3);
    }
    instance_builder.set_setup_time(0, 0, 1, 100);
    instance_builder.set_setup_time(0, 1, 0, 100);
    Instance instance = instance_builder.build();

    Output output = brute_force_test(instance);
    EXPECT_EQ(output.solution.machine(0).jobs.size(), 1);
    EXPECT_EQ(output.solution.machine(1).jobs.size(), 1);
    EXPECT_EQ(output.solution.makespan(), 8);
    EXPECT_EQ(output.bound(), 8);
}

TEST(BruteForce, SetupTimesBeforeReleaseDate)
{
    // The setup of job 1 (5) is performed during the idle time before its
    // release date (10): job 1 starts at 10 and completes at 11.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::TotalFlowTime);
    instance_builder.set_number_of_machines(1);
    for (JobId job_id = 0; job_id < 2; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, 1);
    }
    instance_builder.set_job_release_date(1, 10);
    instance_builder.set_setup_time(0, 0, 1, 5);
    instance_builder.set_setup_time(0, 1, 0, 5);
    Instance instance = instance_builder.build();

    // Sequence 0, 1: 1 + (11 - 10) = 2; sequence 1, 0: 1 + (17 - 0).
    Output output = brute_force_test(instance);
    EXPECT_EQ(machine_sequence(output.solution, 0), (std::vector<JobId>{0, 1}));
    EXPECT_EQ(output.solution.total_flow_time(), 2);
    EXPECT_EQ(output.bound(), 2);
}

// Reference values proved by milp-time-indexed (see doc/tests.md), for all
// the objectives of the test instances.
INSTANTIATE_TEST_SUITE_P(
        BruteForce,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    brute_force_test,
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "brute_force_test.txt"})),
                },
                [](const Solution& solution) { return solution.objective_value(); },
                [](const Output& output) { return output.bound(); })),
        exact_algorithm_test_name);

// Maximum lateness, whose reference values are proved by milp-assignment in
// its tests.
INSTANTIATE_TEST_SUITE_P(
        BruteForceMaximumLateness,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return brute_force_test(maximum_lateness_instance(instance));
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "milp_assignment_maximum_lateness_test.txt"})),
                },
                [](const Solution& solution) { return (double)solution.maximum_lateness(); },
                [](const Output& output) { return (double)output.maximum_lateness_bound; })),
        exact_algorithm_test_name);
