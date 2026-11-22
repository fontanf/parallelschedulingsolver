#include "tests.hpp"

#include "parallelschedulingsolver/instance_builder.hpp"

#include <boost/filesystem.hpp>

#include <cctype>
#include <fstream>
#include <sstream>

using namespace parallelschedulingsolver;

namespace fs = boost::filesystem;

std::string parallelschedulingsolver::get_path(
        const std::vector<std::string>& path)
{
    if (path.empty())
        return "";
    fs::path p(path[0]);
    for (size_t i = 1; i < path.size(); ++i)
        p /= fs::path(path[i]);
    return p.string();
}

std::vector<TestInstancePath> parallelschedulingsolver::get_test_instance_paths(
        const std::string& file_path)
{
    std::ifstream file(file_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }

    // Each line contains the path of an instance and its optimal value.
    std::vector<TestInstancePath> output;
    std::string line;
    while (getline(file, line)) {
        if (line.empty())
            continue;
        std::istringstream line_stream(line);
        TestInstancePath test_instance_path;
        if (!(line_stream >> test_instance_path.instance_path >> test_instance_path.objective_value)) {
            throw std::runtime_error(
                    "Invalid line in file \"" + file_path + "\": "
                    "\"" + line + "\"; "
                    "expected \"<instance path> <objective value>\".");
        }
        output.push_back(test_instance_path);
    }
    return output;
}

std::vector<TestParams> parallelschedulingsolver::get_test_params(
        const std::vector<Algorithm>& algorithms,
        const std::vector<std::vector<TestInstancePath>>& instance_paths,
        const Criterion& criterion,
        const BoundCriterion& bound_criterion)
{
    std::vector<TestParams> output;
    for (const Algorithm& algorithm: algorithms) {
        for (const auto& v: instance_paths) {
            for (const TestInstancePath& files: v) {
                TestParams test_params;
                test_params.algorithm = algorithm;
                test_params.files = files;
                test_params.criterion = criterion;
                test_params.bound_criterion = bound_criterion;
                output.push_back(test_params);
            }
        }
    }
    return output;
}

const Instance parallelschedulingsolver::get_instance(
            const TestInstancePath& files)
{
    std::string instance_path = get_path({
            "data",
            files.instance_path});
    InstanceBuilder instance_builder;
    std::cout << "Instance path:  " << instance_path << std::endl;
    instance_builder.read(instance_path, files.instance_format);
    return instance_builder.build();
}

Instance parallelschedulingsolver::build_instance(
        Objective objective,
        MachineId number_of_machines,
        const std::vector<TestJob>& jobs)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(objective);
    instance_builder.set_number_of_machines(number_of_machines);
    for (const TestJob& job: jobs) {
        JobId job_id = instance_builder.add_job();
        for (MachineId machine_id = 0;
                machine_id < number_of_machines;
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
        instance_builder.set_job_rejection_cost(job_id, job.rejection_cost);
    }
    return instance_builder.build();
}

Instance parallelschedulingsolver::rejection_costs_instance(Objective objective)
{
    return build_instance(
            objective,
            1,
            {{{2}, 0, 1, 1, 3}, {{1}, 0, 1}});
}

void parallelschedulingsolver::check_schedule(
        const Solution& solution,
        const ExpectedSchedule& expected_schedule)
{
    const Instance& instance = solution.instance();
    ASSERT_EQ(instance.number_of_machines(), (MachineId)expected_schedule.size());
    EXPECT_TRUE(solution.feasible());
    for (MachineId machine_id = 0;
            machine_id < instance.number_of_machines();
            ++machine_id) {
        std::vector<std::pair<JobId, Time>> schedule;
        for (const auto& scheduled_job: solution.machine(machine_id).jobs)
            schedule.push_back({scheduled_job.job_id, scheduled_job.start});
        EXPECT_EQ(schedule, expected_schedule[machine_id])
            << "machine_id: " << machine_id;
    }
}

TEST_P(ExactAlgorithmTest, ExactAlgorithm)
{
    TestParams test_params = GetParam();
    const Instance instance = get_instance(test_params.files);
    auto output = test_params.algorithm(instance);
    std::cout << std::endl;
    std::cout << "Reference value: " << test_params.files.objective_value << std::endl;
    EXPECT_TRUE(output.solution.feasible());
    EXPECT_EQ(test_params.criterion(output.solution), test_params.files.objective_value);
    if (test_params.bound_criterion)
        EXPECT_EQ(test_params.bound_criterion(output), test_params.files.objective_value);
}

TEST_P(ScheduleTest, Schedule)
{
    ScheduleTestParams test_params = GetParam();
    const Instance instance = build_instance(
            test_params.objective,
            test_params.number_of_machines,
            test_params.jobs);
    auto output = test_params.algorithm(instance);
    check_schedule(output.solution, test_params.expected_schedule);
    EXPECT_EQ(output.solution.objective_value(), test_params.objective_value);
    EXPECT_EQ(output.bound() == output.solution.objective_value(), test_params.optimal);
}

std::string parallelschedulingsolver::exact_algorithm_test_name(
        const testing::TestParamInfo<TestParams>& info)
{
    std::string name = std::to_string(info.index) + "_"
        + fs::path(info.param.files.instance_path).stem().string();
    for (char& c: name)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    return name;
}

std::string parallelschedulingsolver::schedule_test_name(
        const testing::TestParamInfo<ScheduleTestParams>& info)
{
    return info.param.name;
}

void parallelschedulingsolver::PrintTo(
        const TestParams& test_params,
        std::ostream* os)
{
    *os << test_params.files.instance_path;
}

void parallelschedulingsolver::PrintTo(
        const ScheduleTestParams& test_params,
        std::ostream* os)
{
    *os << test_params.name;
}
