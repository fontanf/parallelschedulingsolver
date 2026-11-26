#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp"
#include "parallelschedulingsolver/algorithms/column_generation.hpp"
#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"
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

/** Instances with identical or unrelated parallel machines. */
std::vector<TestInstancePath> parallel_test_instance_paths()
{
    return get_test_instance_paths(
            get_path({"test", "algorithms", "lagrangian_dual_parallel_test.txt"}));
}

/** The two algorithms. */
using LagrangianDualAlgorithm = std::function<LagrangianDualOutput(
        const Instance&,
        const LagrangianDualParameters&)>;

std::vector<std::pair<std::string, LagrangianDualAlgorithm>> algorithms()
{
    return {
        {"lagrangian relaxation", [](const Instance& instance, const LagrangianDualParameters& common)
            {
                LagrangianRelaxationParameters parameters;
                static_cast<LagrangianDualParameters&>(parameters) = common;
                return lagrangian_relaxation(instance, parameters);
            }},
        {"column generation", [](const Instance& instance, const LagrangianDualParameters& common)
            {
                ColumnGenerationParameters parameters;
                static_cast<LagrangianDualParameters&>(parameters) = common;
                return column_generation(instance, parameters);
            }},
    };
}

LagrangianDualParameters default_parameters()
{
    LagrangianDualParameters parameters;
    parameters.verbosity_level = 0;
    return parameters;
}

/** Copy of an instance with rejection costs for the jobs of even id. */
Instance with_rejection_costs(const Instance& instance)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(instance.objective());
    MachineId m = instance.number_of_machines();
    instance_builder.set_number_of_machines(m);
    JobId n = instance.number_of_jobs();
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        instance_builder.add_job();
        for (MachineId machine_id = 0; machine_id < m; ++machine_id)
            instance_builder.set_job_processing_time(job_id, machine_id, job.processing_times[machine_id]);
        if (job.due_date >= 0)
            instance_builder.set_job_due_date(job_id, job.due_date);
        instance_builder.set_job_weight(job_id, job.weight);
        if (job_id % 2 == 0)
            instance_builder.set_job_rejection_cost(job_id, 5 + 23 * job_id);
    }
    if (instance.has_setup_times()) {
        for (MachineId machine_id = 0; machine_id < m; ++machine_id)
            for (JobId job_id_1 = -1; job_id_1 < n; ++job_id_1)
                for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2)
                    if (job_id_1 != job_id_2)
                        instance_builder.set_setup_time(machine_id, job_id_1, job_id_2, instance.setup_time(machine_id, job_id_1, job_id_2));
    }
    return instance_builder.build();
}

double brute_force_value(const Instance& instance)
{
    BruteForceParameters parameters;
    parameters.verbosity_level = 0;
    return brute_force(instance, parameters).solution.objective_value();
}

}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ExactAlgorithmTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScheduleTest);

TEST(LagrangianDual, UnsupportedInstances)
{
    Instance release_dates = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 1, 1}});
    Instance makespan = build_instance(Objective::Makespan, 2, {{{1, 1}, 0, 1}});
    for (const auto& algorithm: algorithms()) {
        EXPECT_THROW(algorithm.second(release_dates, default_parameters()), std::invalid_argument) << algorithm.first;
        EXPECT_THROW(algorithm.second(makespan, default_parameters()), std::invalid_argument) << algorithm.first;
    }

    // An input network is only supported on a single machine.
    Instance parallel = build_instance(Objective::TotalTardiness, 2, {{{1, 1}, 0, 1}});
    Instance single = build_instance(Objective::TotalTardiness, 1, {{{1}, 0, 1}});
    LagrangianDualParameters parameters = default_parameters();
    parameters.network = std::make_shared<Network>(
            Network::initial_network(build_problem(single)));
    for (const auto& algorithm: algorithms())
        EXPECT_THROW(algorithm.second(parallel, parameters), std::invalid_argument) << algorithm.first;
}

TEST(LagrangianDual, ValidBounds)
{
    // The bound is at most the optimal value and the solution is feasible.
    // (With the reduction, the Lagrangian bound of the reduced network may be
    // larger than the optimal value: no schedule better than the upper bound
    // remains.)
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (const auto& algorithm: algorithms()) {
            LagrangianDualOutput output = algorithm.second(instance, default_parameters());
            EXPECT_TRUE(output.solution.feasible()) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_GE(output.solution.objective_value(), files.objective_value) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, WeakUpperBound)
{
    // With a large upper bound and without reduction (large steps of the
    // subgradient algorithm), the bounds are still valid, including the
    // Lagrangian bound.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (const auto& algorithm: algorithms()) {
            LagrangianDualParameters parameters = default_parameters();
            parameters.upper_bound = 1000000;
            parameters.reduction = false;
            LagrangianDualOutput output = algorithm.second(instance, parameters);
            EXPECT_LE(output.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.lagrangian_bound, files.objective_value + 1e-6) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ExactNetwork)
{
    // With all the jobs in the state-space modifiers of the input network,
    // the relaxation is exact.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (const auto& algorithm: algorithms()) {
            DynamicProgrammingTimeIndexedParameters dp_parameters;
            dp_parameters.verbosity_level = 0;
            dp_parameters.all_jobs_in_modifiers = true;
            DynamicProgrammingTimeIndexedOutput dp_output = dynamic_programming_time_indexed(instance, dp_parameters);
            LagrangianDualParameters parameters = default_parameters();
            parameters.network = dp_output.network;
            LagrangianDualOutput output = algorithm.second(instance, parameters);
            EXPECT_EQ(output.solution.objective_value(), files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_EQ(output.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, RejectionCosts)
{
    // Instances with at most 6 jobs, with rejection costs: valid bounds, and
    // exact with all the jobs in the state-space modifiers.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance_without_rejection = get_instance(files);
        if (instance_without_rejection.number_of_jobs() > 6)
            continue;
        Instance instance = with_rejection_costs(instance_without_rejection);
        double optimal_value = brute_force_value(instance);
        for (const auto& algorithm: algorithms()) {
            LagrangianDualParameters parameters = default_parameters();
            parameters.upper_bound = 1000000;
            parameters.reduction = false;
            LagrangianDualOutput output = algorithm.second(instance, parameters);
            EXPECT_LE(output.lagrangian_bound, optimal_value + 1e-6) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.bound(), optimal_value) << algorithm.first << " " << files.instance_path;

            DynamicProgrammingTimeIndexedParameters dp_parameters;
            dp_parameters.verbosity_level = 0;
            dp_parameters.all_jobs_in_modifiers = true;
            LagrangianDualParameters parameters_exact = default_parameters();
            parameters_exact.network = dynamic_programming_time_indexed(instance, dp_parameters).network;
            LagrangianDualOutput output_exact = algorithm.second(instance, parameters_exact);
            EXPECT_EQ(output_exact.solution.objective_value(), optimal_value) << algorithm.first << " " << files.instance_path;
            EXPECT_EQ(output_exact.bound(), optimal_value) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ColumnGenerationConvergence)
{
    // Without reduction and with a large upper bound, the column generation
    // solves the Lagrangian dual: its bound is at least the bound of the
    // relaxation for any multipliers, in particular those of the Lagrangian
    // relaxation.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        LagrangianDualParameters parameters = default_parameters();
        parameters.upper_bound = 1000000;
        parameters.reduction = false;
        LagrangianDualOutput output_lr = algorithms()[0].second(instance, parameters);
        LagrangianDualOutput output_cg = algorithms()[1].second(instance, parameters);
        EXPECT_GE(output_cg.lagrangian_bound, output_lr.lagrangian_bound - 1e-6) << files.instance_path;
    }
}

TEST(LagrangianDual, Callback)
{
    // The callback is called at each evaluation; an upper bound equal to the
    // optimal value, returned by the callback, is proved optimal or remains
    // the upper bound.
    for (const TestInstancePath& files: test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (const auto& algorithm: algorithms()) {
            Counter number_of_calls = 0;
            LagrangianDualParameters parameters = default_parameters();
            parameters.iteration_callback = [&number_of_calls, &files](const LagrangianDualIteration& iteration)
            {
                number_of_calls++;
                EXPECT_EQ(iteration.iteration, number_of_calls);
                return (Time)files.objective_value;
            };
            LagrangianDualOutput output = algorithm.second(instance, parameters);
            EXPECT_EQ(number_of_calls, output.number_of_iterations) << algorithm.first << " " << files.instance_path;
            EXPECT_EQ(output.upper_bound, files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ParallelMachinesValidBounds)
{
    // Identical and unrelated machines: valid bounds and feasible solutions,
    // with the default parameters, and with a large upper bound without
    // reduction (then the Lagrangian bound is at most the optimal value).
    for (const TestInstancePath& files: parallel_test_instance_paths()) {
        const Instance instance = get_instance(files);
        for (const auto& algorithm: algorithms()) {
            LagrangianDualOutput output = algorithm.second(instance, default_parameters());
            EXPECT_TRUE(output.solution.feasible()) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_GE(output.solution.objective_value(), files.objective_value) << algorithm.first << " " << files.instance_path;

            LagrangianDualParameters parameters = default_parameters();
            parameters.upper_bound = 1000000;
            parameters.reduction = false;
            LagrangianDualOutput output_weak = algorithm.second(instance, parameters);
            EXPECT_LE(output_weak.bound(), files.objective_value) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output_weak.lagrangian_bound, files.objective_value + 1e-6) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ParallelMachinesNetworks)
{
    // A network for each group of identical machines, and a path for each
    // network at each iteration.
    for (const TestInstancePath& files: parallel_test_instance_paths()) {
        const Instance instance = get_instance(files);
        MachineId m = instance.number_of_machines();
        for (const auto& algorithm: algorithms()) {
            Counter number_of_networks = -1;
            LagrangianDualParameters parameters = default_parameters();
            parameters.iteration_callback = [&number_of_networks](const LagrangianDualIteration& iteration)
            {
                if (number_of_networks == -1)
                    number_of_networks = iteration.paths.size();
                EXPECT_EQ((Counter)iteration.paths.size(), number_of_networks);
                return (Time)-1;
            };
            LagrangianDualOutput output = algorithm.second(instance, parameters);
            ASSERT_EQ(output.networks.size(), output.network_machine_ids.size()) << files.instance_path;
            if (number_of_networks != -1)
                EXPECT_EQ(number_of_networks, (Counter)output.networks.size()) << files.instance_path;
            std::vector<bool> machines(m, false);
            for (const std::vector<MachineId>& machine_ids: output.network_machine_ids) {
                for (MachineId machine_id: machine_ids) {
                    EXPECT_FALSE(machines[machine_id]) << files.instance_path;
                    machines[machine_id] = true;
                    EXPECT_TRUE(identical_machines(instance, machine_ids[0], machine_id)) << files.instance_path;
                }
            }
            for (MachineId machine_id = 0; machine_id < m; ++machine_id)
                EXPECT_TRUE(machines[machine_id]) << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ParallelMachinesRejectionCosts)
{
    // Instances with at most 6 jobs, with rejection costs: valid bounds.
    for (const TestInstancePath& files: parallel_test_instance_paths()) {
        const Instance instance_without_rejection = get_instance(files);
        if (instance_without_rejection.number_of_jobs() > 6)
            continue;
        Instance instance = with_rejection_costs(instance_without_rejection);
        double optimal_value = brute_force_value(instance);
        for (const auto& algorithm: algorithms()) {
            LagrangianDualOutput output = algorithm.second(instance, default_parameters());
            EXPECT_TRUE(output.solution.feasible()) << algorithm.first << " " << files.instance_path;
            EXPECT_LE(output.bound(), optimal_value) << algorithm.first << " " << files.instance_path;
            EXPECT_GE(output.solution.objective_value(), optimal_value) << algorithm.first << " " << files.instance_path;

            LagrangianDualParameters parameters = default_parameters();
            parameters.upper_bound = 1000000;
            parameters.reduction = false;
            LagrangianDualOutput output_weak = algorithm.second(instance, parameters);
            EXPECT_LE(output_weak.lagrangian_bound, optimal_value + 1e-6) << algorithm.first << " " << files.instance_path;
        }
    }
}

TEST(LagrangianDual, ParallelMachinesColumnGenerationConvergence)
{
    // Without reduction, the column generation bound is at least the
    // Lagrangian relaxation bound.
    for (const TestInstancePath& files: parallel_test_instance_paths()) {
        const Instance instance = get_instance(files);
        LagrangianDualParameters parameters = default_parameters();
        parameters.upper_bound = 1000000;
        parameters.reduction = false;
        LagrangianDualOutput output_lr = algorithms()[0].second(instance, parameters);
        LagrangianDualOutput output_cg = algorithms()[1].second(instance, parameters);
        EXPECT_GE(output_cg.lagrangian_bound, output_lr.lagrangian_bound - 1e-6) << files.instance_path;
    }
}

TEST(LagrangianDual, MachineProblem)
{
    // Two identical machines and three jobs of processing times 2, 3, 4,
    // without setup times: L = 9, q = 4, horizon (9 + 4) / 2 = 6; unrelated
    // machines: the sum of the processing times of the machine.
    Instance identical = build_instance(Objective::TotalFlowTime, 2, {
            {{2, 2}, 0, 1},
            {{3, 3}, 0, 1},
            {{4, 4}, 0, 1}});
    std::shared_ptr<const Problem> problem = build_machine_problem(identical, 1);
    EXPECT_EQ(problem->number_of_jobs, 3);
    EXPECT_TRUE(problem->machine_subproblem);
    EXPECT_EQ(problem->minimum_makespan, 0);
    EXPECT_EQ(problem->maximum_makespan, 6);
    EXPECT_TRUE(problem->mandatory_jobs.empty());

    Instance unrelated = build_instance(Objective::TotalFlowTime, 2, {
            {{2, 1}, 0, 1},
            {{3, 1}, 0, 1},
            {{4, 1}, 0, 1}});
    EXPECT_FALSE(identical_machines(unrelated, 0, 1));
    EXPECT_EQ(build_machine_problem(unrelated, 0)->maximum_makespan, 9);
    EXPECT_EQ(build_machine_problem(unrelated, 1)->maximum_makespan, 3);
    EXPECT_EQ(build_machine_problem(unrelated, 1)->processing_times, (std::vector<Time>{1, 1, 1}));
}
