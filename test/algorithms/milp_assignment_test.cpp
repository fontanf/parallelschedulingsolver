#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/milp_assignment.hpp"

#include <list>

using namespace parallelschedulingsolver;

namespace
{

Output milp_assignment_test(const Instance& instance)
{
    MilpAssignmentParameters parameters;
    parameters.verbosity_level = 0;
    return milp_assignment(instance, parameters);
}

/**
 * Copy of the instance with the maximum lateness objective: the test
 * instances with due dates have a tardiness objective.
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

}

INSTANTIATE_TEST_SUITE_P(
        MilpAssignment,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Job 1 is released at 4. Job 0 alone on machine 0 (5),
                // jobs 2 and 1 on machine 1, in order of release dates (7).
                "MakespanReleaseDates",
                milp_assignment_test,
                Objective::Makespan,
                2,
                {{{5, 9}, 0}, {{3, 3}, 4}, {{4, 4}, 0}},
                {{{0, 0}}, {{2, 0}, {1, 4}}},
                7,
                true,
            },
            {
                // Job 2 is fast on machine 1; jobs 0 and 1 on machine 0, in
                // order of due dates: job 1 completes at 4 and is 1 late.
                "MaximumLatenessUnrelated",
                milp_assignment_test,
                Objective::MaximumLateness,
                2,
                {{{2, 4}, 0, 2}, {{2, 4}, 0, 3}, {{5, 1}, 0, 1}},
                {{{0, 0}, {1, 2}}, {{2, 0}}},
                1,
                true,
            },
            {
                // The job without due date is scheduled last.
                "MaximumLatenessNoDueDate",
                milp_assignment_test,
                Objective::MaximumLateness,
                1,
                {{{3}, 0}, {{2}, 0, 2}},
                {{{1, 0}, {0, 2}}},
                0,
                true,
            },
        }),
        schedule_test_name);

TEST(MilpAssignment, UnsupportedObjective)
{
    Instance instance = build_instance(Objective::TotalFlowTime, 1, {{{1}}});
    EXPECT_THROW(milp_assignment_test(instance), std::invalid_argument);
}

TEST(MilpAssignment, MaximumLatenessReleaseDates)
{
    Instance instance = build_instance(Objective::MaximumLateness, 1, {{{1}, 1, 2}});
    EXPECT_THROW(milp_assignment_test(instance), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        MilpAssignmentMakespan,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    milp_assignment_test,
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "milp_assignment_makespan_test.txt"})),
                },
                [](const Solution& solution) { return (double)solution.makespan(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.makespan_bound; })),
        exact_algorithm_test_name);

INSTANTIATE_TEST_SUITE_P(
        MilpAssignmentMaximumLateness,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return milp_assignment_test(maximum_lateness_instance(instance));
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "milp_assignment_maximum_lateness_test.txt"})),
                },
                [](const Solution& solution) { return (double)solution.maximum_lateness(); },
                // The algorithm proves the optimality of its solution.
                [](const Output& output) { return (double)output.maximum_lateness_bound; })),
        exact_algorithm_test_name);
