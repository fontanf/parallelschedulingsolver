#pragma once

#include "parallelschedulingsolver/algorithm_formatter.hpp"

#include <gtest/gtest.h>

namespace parallelschedulingsolver
{

std::string get_path(const std::vector<std::string>& path);

struct TestInstancePath
{
    std::string instance_path;
    std::string instance_format;

    /** Optimal value of the instance for the tested criterion. */
    double objective_value = 0;
};

std::vector<TestInstancePath> get_test_instance_paths(
        const std::string& file_path);

using Algorithm = std::function<const Output(const Instance&)>;

/** Criterion compared to the reference value; by default the objective value. */
using Criterion = std::function<double(const Solution&)>;

/**
 * Bound expected to be equal to the reference value, i.e. the algorithm
 * proves the optimality of its solution; empty if not checked.
 */
using BoundCriterion = std::function<double(const Output&)>;

struct TestParams
{
    Algorithm algorithm;
    TestInstancePath files;
    Criterion criterion;
    BoundCriterion bound_criterion;
};

std::vector<TestParams> get_test_params(
        const std::vector<Algorithm>& algorithms,
        const std::vector<std::vector<TestInstancePath>>& instance_paths,
        const Criterion& criterion = [](const Solution& solution) { return solution.objective_value(); },
        const BoundCriterion& bound_criterion = nullptr);

const Instance get_instance(
        const TestInstancePath& files);

class ExactAlgorithmTest: public testing::TestWithParam<TestParams> { };

/*
 * Helpers to check the exact solution returned on small hand-written instances.
 */

struct TestJob
{
    /** Processing time on each machine. */
    std::vector<Time> processing_times;

    Time release_date = 0;

    Time due_date = -1;

    Time weight = 1;
};

Instance build_instance(
        Objective objective,
        MachineId number_of_machines,
        const std::vector<TestJob>& jobs);

/** For each machine, the sequence of (job id, start time). */
using ExpectedSchedule = std::vector<std::vector<std::pair<JobId, Time>>>;

void check_schedule(
        const Solution& solution,
        const ExpectedSchedule& expected_schedule);

/** Test case checking the exact solution returned on a small instance. */
struct ScheduleTestParams
{
    /** Name of the test case. */
    std::string name;

    Algorithm algorithm;

    Objective objective;

    MachineId number_of_machines;

    std::vector<TestJob> jobs;

    ExpectedSchedule expected_schedule;

    /** Expected objective value of the solution. */
    double objective_value;

    /** True if the algorithm is expected to prove the optimality of its solution. */
    bool optimal = false;
};

class ScheduleTest: public testing::TestWithParam<ScheduleTestParams> { };

/** Print test parameters in gtest messages instead of their bytes. */
void PrintTo(const TestParams& test_params, std::ostream* os);

void PrintTo(const ScheduleTestParams& test_params, std::ostream* os);

/** Name generator for ExactAlgorithmTest: index and instance file name. */
std::string exact_algorithm_test_name(
        const testing::TestParamInfo<TestParams>& info);

/** Name generator for ScheduleTest. */
std::string schedule_test_name(
        const testing::TestParamInfo<ScheduleTestParams>& info);

}
