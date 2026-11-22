#include "parallelschedulingsolver/instance_builder.hpp"
#include "parallelschedulingsolver/solution_builder.hpp"

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <fstream>

using namespace parallelschedulingsolver;

namespace
{

// Two jobs on two unrelated machines, with setup times on machine 0 only.
Instance setup_times_instance()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Makespan);
    instance_builder.set_number_of_machines(2);
    for (JobId job_id = 0; job_id < 2; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, 3);
        instance_builder.set_job_processing_time(job_id, 1, 4);
    }
    instance_builder.set_job_release_date(1, 10);
    instance_builder.set_setup_time(0, -1, 0, 2);
    instance_builder.set_setup_time(0, 0, 1, 5);
    instance_builder.set_setup_time(0, 1, 0, 7);
    return instance_builder.build();
}

// Three jobs on one machine, jobs 0 and 2 with a rejection cost.
Instance rejection_costs_instance()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::TotalFlowTime);
    instance_builder.set_number_of_machines(1);
    for (JobId job_id = 0; job_id < 3; ++job_id) {
        instance_builder.add_job();
        instance_builder.set_job_processing_time(job_id, 0, 2);
    }
    instance_builder.set_job_rejection_cost(0, 7);
    instance_builder.set_job_rejection_cost(2, 0);
    return instance_builder.build();
}

// Write a file and read it as an instance.
Instance read_instance(
        const std::string& content,
        const std::string& format)
{
    boost::filesystem::path path = boost::filesystem::temp_directory_path()
        / boost::filesystem::unique_path("%%%%-%%%%-%%%%.txt");
    {
        std::ofstream file(path.string());
        file << content;
    }
    InstanceBuilder instance_builder;
    instance_builder.read(path.string(), format);
    boost::filesystem::remove(path);
    return instance_builder.build();
}

}

TEST(Instance, NoSetupTimes)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Makespan);
    instance_builder.set_number_of_machines(1);
    instance_builder.add_job();
    instance_builder.set_job_processing_time(0, 0, 1);
    Instance instance = instance_builder.build();
    EXPECT_FALSE(instance.has_setup_times());
    EXPECT_EQ(instance.setup_time(0, -1, 0), 0);
}

TEST(Instance, SetupTimes)
{
    Instance instance = setup_times_instance();
    EXPECT_TRUE(instance.has_setup_times());
    EXPECT_EQ(instance.setup_time(0, -1, 0), 2);
    EXPECT_EQ(instance.setup_time(0, -1, 1), 0);
    EXPECT_EQ(instance.setup_time(0, 0, 1), 5);
    EXPECT_EQ(instance.setup_time(0, 1, 0), 7);
    EXPECT_EQ(instance.setup_time(1, 0, 1), 0);
}

TEST(Instance, InvalidSetupTime)
{
    InstanceBuilder instance_builder;
    instance_builder.set_number_of_machines(1);
    instance_builder.add_job();
    EXPECT_THROW(instance_builder.set_setup_time(0, -2, 0, 1), std::invalid_argument);
    EXPECT_THROW(instance_builder.set_setup_time(0, 0, 1, 1), std::invalid_argument);
    EXPECT_THROW(instance_builder.set_setup_time(1, -1, 0, 1), std::invalid_argument);
    EXPECT_THROW(instance_builder.set_setup_time(0, -1, 0, -1), std::invalid_argument);
}

TEST(Instance, SetupTimesJson)
{
    Instance instance = setup_times_instance();
    boost::filesystem::path path = boost::filesystem::temp_directory_path()
        / boost::filesystem::unique_path("%%%%-%%%%-%%%%.json");
    instance.write(path.string(), "json");
    InstanceBuilder instance_builder;
    instance_builder.read(path.string(), "json");
    Instance instance_read = instance_builder.build();
    boost::filesystem::remove(path);
    ASSERT_TRUE(instance_read.has_setup_times());
    for (MachineId machine_id = 0; machine_id < 2; ++machine_id)
        for (JobId job_id_1 = -1; job_id_1 < 2; ++job_id_1)
            for (JobId job_id_2 = 0; job_id_2 < 2; ++job_id_2)
                EXPECT_EQ(
                        instance_read.setup_time(machine_id, job_id_1, job_id_2),
                        instance.setup_time(machine_id, job_id_1, job_id_2));
}

TEST(Instance, RejectionCosts)
{
    Instance instance = rejection_costs_instance();
    EXPECT_TRUE(instance.has_rejection_costs());
    EXPECT_EQ(instance.job(0).rejection_cost, 7);
    EXPECT_EQ(instance.job(1).rejection_cost, -1);
    EXPECT_EQ(instance.job(2).rejection_cost, 0);
    EXPECT_FALSE(setup_times_instance().has_rejection_costs());
}

TEST(Instance, InvalidRejectionCost)
{
    InstanceBuilder instance_builder;
    instance_builder.set_number_of_machines(1);
    instance_builder.add_job();
    EXPECT_THROW(instance_builder.set_job_rejection_cost(0, -2), std::invalid_argument);
}

TEST(Instance, RejectionCostsUnsupportedObjective)
{
    for (Objective objective: {Objective::MaximumLateness, Objective::Throughput}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(objective);
        instance_builder.set_number_of_machines(1);
        instance_builder.add_job();
        instance_builder.set_job_processing_time(0, 0, 1);
        instance_builder.set_job_due_date(0, 1);
        instance_builder.set_job_rejection_cost(0, 1);
        EXPECT_THROW(instance_builder.build(), std::invalid_argument);
    }
}

TEST(Instance, RejectionCostsJson)
{
    Instance instance = rejection_costs_instance();
    boost::filesystem::path path = boost::filesystem::temp_directory_path()
        / boost::filesystem::unique_path("%%%%-%%%%-%%%%.json");
    instance.write(path.string(), "json");
    InstanceBuilder instance_builder;
    instance_builder.read(path.string(), "json");
    Instance instance_read = instance_builder.build();
    boost::filesystem::remove(path);
    EXPECT_TRUE(instance_read.has_rejection_costs());
    for (JobId job_id = 0; job_id < 3; ++job_id)
        EXPECT_EQ(instance_read.job(job_id).rejection_cost, instance.job(job_id).rejection_cost);
}

TEST(Solution, RejectedJobs)
{
    Instance instance = rejection_costs_instance();
    // Job 1 alone: jobs 0 and 2 are rejected.
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(0, 1, 0);
    Solution solution = solution_builder.build();
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_rejected_jobs(), 2);
    EXPECT_EQ(solution.total_rejection_cost(), 7);
    EXPECT_EQ(solution.total_flow_time(), 2);
    EXPECT_EQ(solution.objective_value(), 9);

    // Jobs 1 and 0: job 2 is rejected at no cost.
    SolutionBuilder solution_builder_2;
    solution_builder_2.set_instance(instance);
    solution_builder_2.append_job_to_machine(0, 1, 0);
    solution_builder_2.append_job_to_machine(0, 0, 2);
    Solution solution_2 = solution_builder_2.build();
    EXPECT_EQ(solution_2.objective_value(), 2 + 4);
    EXPECT_TRUE(solution_2.strictly_better(solution));
    EXPECT_FALSE(solution.strictly_better(solution_2));
}

TEST(Solution, MandatoryJobMissing)
{
    // Job 1 can't be rejected.
    Instance instance = rejection_costs_instance();
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(0, 0, 0);
    Solution solution = solution_builder.build();
    EXPECT_FALSE(solution.feasible());
}

TEST(Solution, SetupTimes)
{
    Instance instance = setup_times_instance();
    // Job 0 after its initial setup [0, 2), then job 1 after its setup
    // [5, 10): the setup is performed before the release date of job 1.
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(0, 0, 2);
    solution_builder.append_job_to_machine(0, 1, 10);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.machine(0).jobs[0].setup, 2);
    EXPECT_EQ(solution.machine(0).jobs[1].setup, 5);
    EXPECT_EQ(solution.makespan(), 13);
}

TEST(Solution, StartBeforeSetup)
{
    Instance instance = setup_times_instance();
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(0, 1, 10);
    solution_builder.append_job_to_machine(0, 0, 15);
    // Job 0 completes at 13 and its setup after job 1 takes 7.
    EXPECT_THROW(solution_builder.build(), std::invalid_argument);
}

TEST(Solution, StartBeforeInitialSetup)
{
    Instance instance = setup_times_instance();
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(0, 0, 1);
    EXPECT_THROW(solution_builder.build(), std::invalid_argument);
}

TEST(Solution, StartBeforeReleaseDate)
{
    Instance instance = setup_times_instance();
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(1, 1, 9);
    EXPECT_THROW(solution_builder.build(), std::invalid_argument);
}

TEST(Solution, Overlap)
{
    Instance instance = setup_times_instance();
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    solution_builder.append_job_to_machine(1, 0, 0);
    solution_builder.append_job_to_machine(1, 1, 10);
    EXPECT_NO_THROW(solution_builder.build());
    SolutionBuilder solution_builder_2;
    solution_builder_2.set_instance(instance);
    solution_builder_2.append_job_to_machine(1, 1, 10);
    solution_builder_2.append_job_to_machine(1, 0, 13);
    EXPECT_THROW(solution_builder_2.build(), std::invalid_argument);
}

TEST(Instance, ReadCicirello2003)
{
    Instance instance = read_instance(
            "Problem Instance: 1\n"
            "Problem Size: 2\n"
            "Begin Generator Parameters\n"
            "Tau: 0.3\n"
            "End Generator Parameters\n"
            "Begin Problem Specification\n"
            "Process Times:\n"
            "81\n"
            "71\n"
            "Weights:\n"
            "4\n"
            "0\n"
            "Duedates:\n"
            "100\n"
            "200\n"
            "Setup Times:\n"
            "-1\t0\t43\n"
            "-1\t1\t7\n"
            "0\t1\t12\n"
            "1\t0\t30\n"
            "End Problem Specification\n",
            "cicirello2003");
    EXPECT_EQ(instance.objective(), Objective::TotalTardiness);
    ASSERT_EQ(instance.number_of_machines(), 1);
    ASSERT_EQ(instance.number_of_jobs(), 2);
    EXPECT_EQ(instance.job(0).processing_times[0], 81);
    EXPECT_EQ(instance.job(1).weight, 0);
    EXPECT_EQ(instance.job(1).due_date, 200);
    EXPECT_EQ(instance.setup_time(0, -1, 0), 43);
    EXPECT_EQ(instance.setup_time(0, -1, 1), 7);
    EXPECT_EQ(instance.setup_time(0, 0, 1), 12);
    EXPECT_EQ(instance.setup_time(0, 1, 0), 30);
}

TEST(Instance, ReadVallada2011)
{
    Instance instance = read_instance(
            "2  2  1\n"
            "2\n"
            "\t0\t42\t1\t60\n"
            "\t0\t88\t1\t41\n"
            "SSD\n"
            "M0\n"
            "0\t35\n"
            "120\t0\n"
            "M1\n"
            "0\t10\n"
            "99\t0\n",
            "vallada2011");
    EXPECT_EQ(instance.objective(), Objective::Makespan);
    ASSERT_EQ(instance.number_of_machines(), 2);
    ASSERT_EQ(instance.number_of_jobs(), 2);
    EXPECT_EQ(instance.job(0).processing_times[1], 60);
    EXPECT_EQ(instance.job(1).processing_times[0], 88);
    EXPECT_EQ(instance.setup_time(0, -1, 0), 0);
    EXPECT_EQ(instance.setup_time(0, 0, 1), 35);
    EXPECT_EQ(instance.setup_time(0, 1, 0), 120);
    EXPECT_EQ(instance.setup_time(1, 0, 1), 10);
    EXPECT_EQ(instance.setup_time(1, 1, 0), 99);
}
