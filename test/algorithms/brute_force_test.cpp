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


/**
 * Copy of an instance with only some of its jobs, without rejection costs.
 */
Instance sub_instance(
        const Instance& instance,
        const std::vector<JobId>& job_ids)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(instance.objective());
    instance_builder.set_number_of_machines(instance.number_of_machines());
    for (JobId job_id: job_ids) {
        const Job& job = instance.job(job_id);
        JobId new_job_id = instance_builder.add_job();
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            instance_builder.set_job_processing_time(
                    new_job_id,
                    machine_id,
                    job.processing_times[machine_id]);
        }
        instance_builder.set_job_release_date(new_job_id, job.release_date);
        if (job.due_date >= 0)
            instance_builder.set_job_due_date(new_job_id, job.due_date);
        instance_builder.set_job_weight(new_job_id, job.weight);
    }
    if (instance.has_setup_times()) {
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            for (JobId pos_2 = 0; pos_2 < (JobId)job_ids.size(); ++pos_2) {
                instance_builder.set_setup_time(
                        machine_id,
                        -1,
                        pos_2,
                        instance.setup_time(machine_id, -1, job_ids[pos_2]));
                for (JobId pos_1 = 0; pos_1 < (JobId)job_ids.size(); ++pos_1) {
                    instance_builder.set_setup_time(
                            machine_id,
                            pos_1,
                            pos_2,
                            instance.setup_time(machine_id, job_ids[pos_1], job_ids[pos_2]));
                }
            }
        }
    }
    return instance_builder.build();
}

/** Copy of an instance with rejection costs (-1: the job can't be rejected). */
Instance with_rejection_costs(
        const Instance& instance,
        const std::vector<Time>& rejection_costs)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(instance.objective());
    instance_builder.set_number_of_machines(instance.number_of_machines());
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        instance_builder.add_job();
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            instance_builder.set_job_processing_time(
                    job_id,
                    machine_id,
                    job.processing_times[machine_id]);
        }
        instance_builder.set_job_release_date(job_id, job.release_date);
        if (job.due_date >= 0)
            instance_builder.set_job_due_date(job_id, job.due_date);
        instance_builder.set_job_weight(job_id, job.weight);
        instance_builder.set_job_rejection_cost(job_id, rejection_costs[job_id]);
    }
    if (instance.has_setup_times()) {
        for (MachineId machine_id = 0;
                machine_id < instance.number_of_machines();
                ++machine_id) {
            for (JobId job_id_1 = -1; job_id_1 < instance.number_of_jobs(); ++job_id_1) {
                for (JobId job_id_2 = 0; job_id_2 < instance.number_of_jobs(); ++job_id_2) {
                    instance_builder.set_setup_time(
                            machine_id,
                            job_id_1,
                            job_id_2,
                            instance.setup_time(machine_id, job_id_1, job_id_2));
                }
            }
        }
    }
    return instance_builder.build();
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

TEST(BruteForce, RejectionCostsSingleMachine)
{
    // Job 0 (p = 10) has a rejection cost of 5, job 1 (p = 1) can't be
    // rejected. Scheduling both: 1 + 11 = 12; rejecting job 0: 1 + 5 = 6.
    Instance instance = build_instance(
            Objective::TotalFlowTime,
            1,
            {{{10}, 0, -1, 1, 5}, {{1}, 0, -1, 1}});
    Output output = brute_force_test(instance);
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_FALSE(output.solution.contains(0));
    EXPECT_EQ(output.solution.number_of_rejected_jobs(), 1);
    EXPECT_EQ(output.solution.total_rejection_cost(), 5);
    EXPECT_EQ(output.solution.objective_value(), 6);
    EXPECT_EQ(output.bound(), 6);
}

TEST(BruteForce, RejectionCostsParallelMachines)
{
    // Three jobs of 4 on two identical machines, job 2 with a rejection cost
    // of 3: makespan 8 with all the jobs, 4 + 3 = 7 rejecting job 2. The
    // assignments are enumerated with a sequencing rule.
    Instance instance = build_instance(
            Objective::Makespan,
            2,
            {{{4, 4}}, {{4, 4}}, {{4, 4}, 0, -1, 1, 3}});
    Output output = brute_force_test(instance);
    EXPECT_FALSE(output.solution.contains(2));
    EXPECT_EQ(output.solution.makespan(), 4);
    EXPECT_EQ(output.solution.objective_value(), 7);
    EXPECT_EQ(output.bound(), 7);
}

TEST(BruteForce, RejectionCostsAllJobsRejected)
{
    // Rejecting both jobs (1 + 1) is better than scheduling them (3 + 7).
    Instance instance = build_instance(
            Objective::TotalTardiness,
            1,
            {{{5}, 0, 2, 1, 1}, {{5}, 0, 3, 1, 1}});
    Output output = brute_force_test(instance);
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(output.solution.number_of_rejected_jobs(), 2);
    EXPECT_EQ(output.solution.objective_value(), 2);
    EXPECT_EQ(output.bound(), 2);
}

TEST(BruteForce, RejectionCostsAgainstSubsets)
{
    // On test instances with rejection costs for the jobs of even id, the
    // optimal value is the best, over the subsets of rejected jobs, of the
    // optimal value of the instance without these jobs (and without
    // rejection costs) plus their rejection costs.
    JobId number_of_rejectable_jobs = 0;
    JobId number_of_rejected_jobs = 0;
    for (const std::string& path: {
            "tests/total_flow_time_single_setup_times/tft_n1x5_s0.json",
            "tests/total_weighted_tardiness_single_release_dates/twt_n1x5_s1.json",
            "tests/total_tardiness_identical/tt_n2x3_s1.json",
            "tests/total_flow_time_identical/tft_n2x3_s0.json",
            "tests/makespan_unrelated_setup_times/makespan_n2x2_s0.json"}) {
        InstanceBuilder instance_builder;
        instance_builder.read(get_path({"data", path}), "json");
        Instance instance_without_rejection = instance_builder.build();
        JobId n = instance_without_rejection.number_of_jobs();
        std::vector<Time> rejection_costs(n, -1);
        for (JobId job_id = 0; job_id < n; job_id += 2)
            rejection_costs[job_id] = 10 + 37 * job_id;
        Instance instance = with_rejection_costs(instance_without_rejection, rejection_costs);
        Output output = brute_force_test(instance);
        number_of_rejectable_jobs += (n + 1) / 2;
        number_of_rejected_jobs += output.solution.number_of_rejected_jobs();

        double expected = std::numeric_limits<double>::infinity();
        for (JobId subset = 0; subset < (1 << ((n + 1) / 2)); ++subset) {
            std::vector<JobId> job_ids;
            Time rejection_cost = 0;
            for (JobId job_id = 0; job_id < n; ++job_id) {
                if (job_id % 2 == 0 && ((subset >> (job_id / 2)) & 1)) {
                    rejection_cost += rejection_costs[job_id];
                } else {
                    job_ids.push_back(job_id);
                }
            }
            Instance instance_subset = sub_instance(instance_without_rejection, job_ids);
            Output output_subset = brute_force_test(instance_subset);
            expected = (std::min)(
                    expected,
                    output_subset.solution.objective_value() + rejection_cost);
        }
        EXPECT_EQ(output.solution.objective_value(), expected) << path;
        EXPECT_EQ(output.bound(), expected) << path;
    }
    // Some jobs are rejected in the optimal solutions, and some are not.
    EXPECT_GT(number_of_rejected_jobs, 0);
    EXPECT_LT(number_of_rejected_jobs, number_of_rejectable_jobs);
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
