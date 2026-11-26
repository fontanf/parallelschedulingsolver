#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/relaxation_refinement.hpp"
#include "parallelschedulingsolver/algorithms/brute_force.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

namespace
{

std::vector<TestInstancePath> test_instance_paths()
{
    return get_test_instance_paths(
            get_path({"test", "algorithms", "successive_sublimation_dynamic_programming_test.txt"}));
}

RelaxationRefinementParameters default_parameters()
{
    RelaxationRefinementParameters parameters;
    parameters.verbosity_level = 0;
    return parameters;
}

/** Copy of a single machine instance with rejection costs for the jobs of even id. */
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
                if (job_id_1 != job_id_2)
                    instance_builder.set_setup_time(0, job_id_1, job_id_2, instance.setup_time(0, job_id_1, job_id_2));
    }
    return instance_builder.build();
}

}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ExactAlgorithmTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(RelaxationRefinement, UnsupportedInstances)
{
    Instance parallel = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 0, 1}});
    EXPECT_THROW(relaxation_refinement(parallel, default_parameters()), std::invalid_argument);
    Instance release_dates = build_instance(Objective::TotalTardiness, 1, {{{1}, 1, 1}});
    EXPECT_THROW(relaxation_refinement(release_dates, default_parameters()), std::invalid_argument);
}

TEST(RelaxationRefinement, Exact)
{
    // Without early termination, the bound to prove is the upper bound: the
    // refinement proves the optimality of the solution, with the multipliers
    // of each algorithm of the Lagrangian dual, or with all the multipliers
    // equal to 0 and a large upper bound.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (int setting = 0; setting < 3; ++setting) {
            RelaxationRefinementParameters parameters = default_parameters();
            if (setting == 1)
                parameters.column_generation = false;
            if (setting == 2) {
                parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
                parameters.upper_bound = 1000000;
            }
            RelaxationRefinementOutput output = relaxation_refinement(instance, parameters);
            EXPECT_EQ(output.status, RelaxationRefinementStatus::BoundProved) << setting << " " << files.instance_path;
            EXPECT_EQ(output.solution.objective_value(), files.objective_value) << setting << " " << files.instance_path;
            EXPECT_EQ(output.bound(), files.objective_value) << setting << " " << files.instance_path;
        }
    }
}

TEST(RelaxationRefinement, BoundToProve)
{
    // A bound to prove smaller than the optimal value is proved.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        Time bound_to_prove = (Time)files.objective_value / 2;
        RelaxationRefinementParameters parameters = default_parameters();
        parameters.bound_to_prove = bound_to_prove;
        RelaxationRefinementOutput output = relaxation_refinement(instance, parameters);
        EXPECT_EQ(output.status, RelaxationRefinementStatus::BoundProved) << files.instance_path;
        EXPECT_EQ(output.bound_to_prove, bound_to_prove) << files.instance_path;
        EXPECT_GE(output.bound(), bound_to_prove) << files.instance_path;
        EXPECT_LE(output.bound(), files.objective_value) << files.instance_path;
    }
}

TEST(RelaxationRefinement, EarlyTermination)
{
    // Maximum number of steps, maximum memory, tailing-off: valid bounds,
    // and the status of the termination (unless the bound is proved
    // before; each termination occurs on some instances).
    std::vector<Counter> number_of_terminations(3, 0);
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (int setting = 0; setting < 3; ++setting) {
            RelaxationRefinementParameters parameters = default_parameters();
            parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
            parameters.upper_bound = 1000000;
            RelaxationRefinementStatus expected_status = RelaxationRefinementStatus::MaximumNumberOfSteps;
            if (setting == 0) {
                parameters.maximum_number_of_steps = 1;
            } else if (setting == 1) {
                parameters.maximum_memory = 1e-6;
                expected_status = RelaxationRefinementStatus::MemoryOut;
            } else {
                // The gap must be closed at each step.
                parameters.tailing_off_number_of_steps = 1;
                parameters.tailing_off_minimum_improvement = 1;
                expected_status = RelaxationRefinementStatus::TailingOff;
            }
            RelaxationRefinementOutput output = relaxation_refinement(instance, parameters);
            if (output.status != RelaxationRefinementStatus::BoundProved) {
                EXPECT_EQ(output.status, expected_status) << setting << " " << files.instance_path;
                number_of_terminations[setting]++;
            }
            if (setting == 0)
                EXPECT_LE(output.number_of_steps, 1) << files.instance_path;
            EXPECT_LE(output.bound(), files.objective_value) << setting << " " << files.instance_path;
            EXPECT_GE(output.solution.objective_value(), files.objective_value) << setting << " " << files.instance_path;
        }
    }
    for (int setting = 0; setting < 3; ++setting)
        EXPECT_GT(number_of_terminations[setting], 0) << setting;
}

TEST(RelaxationRefinement, Callback)
{
    // The callback is called at each step, with the network of the step; an
    // upper bound equal to the optimal value, returned by the callback,
    // becomes the bound to prove.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        Counter number_of_calls = 0;
        RelaxationRefinementParameters parameters = default_parameters();
        parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
        parameters.upper_bound = 1000000;
        parameters.step_callback = [&number_of_calls, &files](const RelaxationRefinementStep& step)
        {
            number_of_calls++;
            EXPECT_EQ(step.step, number_of_calls);
            EXPECT_GE(step.network.modifiers().size(), 1);
            return (Time)files.objective_value;
        };
        RelaxationRefinementOutput output = relaxation_refinement(instance, parameters);
        EXPECT_EQ(number_of_calls, output.number_of_steps) << files.instance_path;
        EXPECT_EQ(output.status, RelaxationRefinementStatus::BoundProved) << files.instance_path;
        EXPECT_LE(output.upper_bound, files.objective_value) << files.instance_path;
        EXPECT_EQ(output.bound_to_prove, output.upper_bound) << files.instance_path;
    }
}

TEST(RelaxationRefinement, RejectionCosts)
{
    // Instances with at most 6 jobs, with rejection costs: the optimal value.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance_without_rejection = get_instance(files);
        if (instance_without_rejection.number_of_jobs() > 6)
            continue;
        Instance instance = with_rejection_costs(instance_without_rejection);
        BruteForceParameters brute_force_parameters;
        brute_force_parameters.verbosity_level = 0;
        double optimal_value = brute_force(instance, brute_force_parameters).solution.objective_value();
        RelaxationRefinementOutput output = relaxation_refinement(instance, default_parameters());
        EXPECT_EQ(output.status, RelaxationRefinementStatus::BoundProved) << files.instance_path;
        EXPECT_EQ(output.solution.objective_value(), optimal_value) << files.instance_path;
        EXPECT_EQ(output.bound(), optimal_value) << files.instance_path;
    }
}
