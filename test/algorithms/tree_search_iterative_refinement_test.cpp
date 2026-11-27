#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/tree_search_iterative_refinement.hpp"
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

TreeSearchIterativeRefinementParameters default_parameters()
{
    TreeSearchIterativeRefinementParameters parameters;
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

TEST(TreeSearchIterativeRefinement, UnsupportedInstances)
{
    Instance parallel = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 0, 1}});
    EXPECT_THROW(tree_search_iterative_refinement(parallel, default_parameters()), std::invalid_argument);
    Instance release_dates = build_instance(Objective::TotalTardiness, 1, {{{1}, 1, 1}});
    EXPECT_THROW(tree_search_iterative_refinement(release_dates, default_parameters()), std::invalid_argument);
}

TEST(TreeSearchIterativeRefinement, Exact)
{
    // The optimal value and its proof: default parameters, Lagrangian
    // relaxation, without constraint propagation, with forced interval
    // branching, and with all the multipliers equal to 0 and a large upper
    // bound.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (int setting = 0; setting < 5; ++setting) {
            TreeSearchIterativeRefinementParameters parameters = default_parameters();
            if (setting == 1)
                parameters.column_generation = false;
            if (setting == 2)
                parameters.constraint_propagation = false;
            if (setting == 3)
                parameters.minimum_branching_depth = 2;
            if (setting == 4) {
                parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
                parameters.upper_bound = 1000000;
            }
            TreeSearchIterativeRefinementOutput output = tree_search_iterative_refinement(instance, parameters);
            EXPECT_EQ(output.solution.objective_value(), files.objective_value) << setting << " " << files.instance_path;
            EXPECT_EQ(output.bound(), files.objective_value) << setting << " " << files.instance_path;
            if (setting == 3 && output.number_of_refinements > 0)
                EXPECT_GE(output.maximum_branching_depth, 2) << files.instance_path;
        }
    }
}

TEST(TreeSearchIterativeRefinement, MemoryLimit)
{
    // With a tiny memory, with and without interval branching: valid
    // bounds.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (bool interval_branching: {false, true}) {
            TreeSearchIterativeRefinementParameters parameters = default_parameters();
            parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
            parameters.upper_bound = 1000000;
            parameters.maximum_memory = 1e-6;
            parameters.interval_branching = interval_branching;
            parameters.warm_start = interval_branching;
            TreeSearchIterativeRefinementOutput output = tree_search_iterative_refinement(instance, parameters);
            EXPECT_LE(output.bound(), files.objective_value) << interval_branching << " " << files.instance_path;
            EXPECT_GE(output.solution.objective_value(), files.objective_value) << interval_branching << " " << files.instance_path;
        }
    }
}

TEST(TreeSearchIterativeRefinement, Callbacks)
{
    // The lower bound callback is called with non-decreasing bounds (when the
    // upper bound decreases during a refinement, the bound to prove decreases
    // to it, and may be proved again), at most the
    // optimal value; the step callback at each step of the refinements.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        std::vector<Time> lower_bounds;
        Counter number_of_steps = 0;
        TreeSearchIterativeRefinementParameters parameters = default_parameters();
        parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
        parameters.upper_bound = 1000000;
        parameters.lower_bound_callback = [&lower_bounds](Time lower_bound)
        {
            lower_bounds.push_back(lower_bound);
        };
        parameters.step_callback = [&number_of_steps](const RelaxationRefinementStep&)
        {
            number_of_steps++;
            return (Time)-1;
        };
        TreeSearchIterativeRefinementOutput output = tree_search_iterative_refinement(instance, parameters);
        EXPECT_EQ(output.bound(), files.objective_value) << files.instance_path;
        for (size_t pos = 0; pos < lower_bounds.size(); ++pos) {
            EXPECT_LE(lower_bounds[pos], files.objective_value) << files.instance_path;
            if (pos > 0)
                EXPECT_GE(lower_bounds[pos], lower_bounds[pos - 1]) << files.instance_path;
        }
        if (output.number_of_refinements > 0)
            EXPECT_FALSE(lower_bounds.empty()) << files.instance_path;
    }
}

TEST(TreeSearchIterativeRefinement, RejectionCosts)
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
        TreeSearchIterativeRefinementOutput output = tree_search_iterative_refinement(instance, default_parameters());
        EXPECT_EQ(output.solution.objective_value(), optimal_value) << files.instance_path;
        EXPECT_EQ(output.bound(), optimal_value) << files.instance_path;
    }
}

TEST(TreeSearchIterativeRefinement, RefineOrBranch)
{
    // Branching after one refinement step, or when the refinement tails off
    // (the gap must be closed at each step), with all the multipliers equal
    // to 0 and a large upper bound: the optimal value and its proof, and the
    // branching occurs on some instances.
    // With and without warm start of the children.
    for (int setting = 0; setting < 4; ++setting) {
        Counter number_of_branchings = 0;
        for (const TestInstancePath& files: test_instance_paths()) {
            const Instance instance = get_instance(files);
            TreeSearchIterativeRefinementParameters parameters = default_parameters();
            parameters.multipliers.assign(build_problem(instance)->number_of_jobs, 0);
            parameters.upper_bound = 1000000;
            parameters.warm_start = (setting >= 2);
            if (setting % 2 == 0) {
                parameters.maximum_number_of_refinement_steps = 1;
            } else {
                parameters.tailing_off_number_of_steps = 1;
                parameters.tailing_off_minimum_improvement = 1;
            }
            TreeSearchIterativeRefinementOutput output = tree_search_iterative_refinement(instance, parameters);
            EXPECT_EQ(output.solution.objective_value(), files.objective_value) << setting << " " << files.instance_path;
            EXPECT_EQ(output.bound(), files.objective_value) << setting << " " << files.instance_path;
            if (output.maximum_branching_depth > 0)
                number_of_branchings++;
        }
        EXPECT_GT(number_of_branchings, 0) << setting;
    }
}
