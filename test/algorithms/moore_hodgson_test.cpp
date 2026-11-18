#include "tests.hpp"
#include "parallelschedulingsolver/algorithms/moore_hodgson.hpp"

using namespace parallelschedulingsolver;

TEST(MooreHodgson, ParallelMachines)
{
    Instance instance = build_instance(Objective::Throughput, 2, {
            {{2, 2}, 0, 3}});
    EXPECT_THROW(moore_hodgson(instance), std::invalid_argument);
}

INSTANTIATE_TEST_SUITE_P(
        MooreHodgson,
        ScheduleTest,
        testing::ValuesIn(std::vector<ScheduleTestParams>{
            {
                // Job 1 is removed when it is late, then job 4. Job 5 has no
                // due date. On-time jobs in EDD order, then late jobs.
                "Unweighted",
                [](const Instance& instance) { return moore_hodgson(instance); },
                Objective::Throughput,
                1,
                {{{2}, 0, 3}, {{4}, 0, 5}, {{3}, 0, 7}, {{1}, 0, 8}, {{5}, 0, 9}, {{1}, 0, -1}},
                {{{0, 0}, {2, 2}, {3, 5}, {5, 6}, {1, 7}, {4, 11}}},
                3,
                true,
            },
            {
                // When job 1 is late, job 0 is removed since it has the largest
                // p / w.
                "Weighted",
                [](const Instance& instance) { return moore_hodgson(instance); },
                Objective::Throughput,
                1,
                {{{2}, 0, 3, 1}, {{4}, 0, 5, 10}},
                {{{1, 0}, {0, 4}}},
                10,
            },
            {
                // All jobs on time: the EDD sequence.
                "AllOnTime",
                [](const Instance& instance) { return moore_hodgson(instance); },
                Objective::Throughput,
                1,
                {{{2}, 0, 9}, {{1}, 0, 1}, {{3}, 0, 6}},
                {{{1, 0}, {2, 1}, {0, 4}}},
                3,
                true,
            },
        }),
        schedule_test_name);

INSTANTIATE_TEST_SUITE_P(
        MooreHodgson,
        ExactAlgorithmTest,
        testing::ValuesIn(get_test_params(
                {
                    [](const Instance& instance)
                    {
                        return moore_hodgson(instance);
                    },
                },
                {
                    get_test_instance_paths(get_path({"test", "algorithms", "moore_hodgson_test.txt"})),
                },
                // Number of on-time jobs.
                [](const Solution& solution)
                {
                    const Instance& instance = solution.instance();
                    JobId number_of_on_time_jobs = 0;
                    for (const auto& scheduled_job: solution.machine(0).jobs) {
                        Time due_date = instance.job(scheduled_job.job_id).due_date;
                        if (due_date >= 0 && scheduled_job.completion <= due_date)
                            number_of_on_time_jobs++;
                    }
                    return (double)number_of_on_time_jobs;
                })),
        exact_algorithm_test_name);
