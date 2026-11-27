#include "parallelschedulingsolver/algorithm_formatter.hpp"
#include "parallelschedulingsolver/instance_builder.hpp"
#include "parallelschedulingsolver/algorithms/greedy_longest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_shortest_processing_time.hpp"
#include "parallelschedulingsolver/algorithms/greedy_earliest_due_date.hpp"
#include "parallelschedulingsolver/algorithms/moore_hodgson.hpp"
#include "parallelschedulingsolver/algorithms/assignment_total_flow_time.hpp"
#include "parallelschedulingsolver/algorithms/milp_time_indexed.hpp"
#include "parallelschedulingsolver/algorithms/local_search.hpp"
#include "parallelschedulingsolver/algorithms/milp_assignment.hpp"
#include "parallelschedulingsolver/algorithms/milp_positional.hpp"
#include "parallelschedulingsolver/algorithms/brute_force.hpp"
#include "parallelschedulingsolver/algorithms/successive_sublimation_dynamic_programming.hpp"
#include "parallelschedulingsolver/algorithms/dynamic_programming_time_indexed.hpp"
#include "parallelschedulingsolver/algorithms/lagrangian_relaxation.hpp"
#include "parallelschedulingsolver/algorithms/column_generation.hpp"
#include "parallelschedulingsolver/algorithms/relaxation_refinement.hpp"
#include "parallelschedulingsolver/algorithms/tree_search_iterative_refinement.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <string>

using namespace parallelschedulingsolver;
namespace po = boost::program_options;

void read_args(
        parallelschedulingsolver::Parameters& parameters,
        const po::variables_map& vm)
{
    parameters.timer.set_sigint_handler();
    parameters.messages_to_stdout = true;
    if (vm.count("time-limit"))
        parameters.timer.set_time_limit(vm["time-limit"].as<double>());
    if (vm.count("verbosity-level"))
        parameters.verbosity_level = vm["verbosity-level"].as<int>();
    if (vm.count("log2stderr"))
        parameters.log_to_stderr = vm["log-to-stderr"].as<bool>();
    if (vm.count("log"))
        parameters.log_path = vm["log"].as<std::string>();
    if (vm.count("solver-log"))
        parameters.solver_log_path = vm["solver-log"].as<std::string>();
    parameters.log_to_stderr = vm.count("log-to-stderr");
    bool only_write_at_the_end = vm.count("only-write-at-the-end");
    if (!only_write_at_the_end) {

        std::string certificate_path = "";
        if (vm.count("certificate"))
            certificate_path = vm["certificate"].as<std::string>();

        std::string json_output_path = "";
        if (vm.count("output"))
            json_output_path = vm["output"].as<std::string>();
        std::string certificate_format = "json";
        if (vm.count("certificate-format"))
            certificate_format = vm["certificate-format"].as<std::string>();

        parameters.new_solution_callback = [
            json_output_path,
            certificate_path,
            certificate_format](
                    const parallelschedulingsolver::Output& output)
        {
            if (!json_output_path.empty())
                output.write_json_output(json_output_path);
            if (!certificate_path.empty()) {
                output.solution.write(
                        certificate_path,
                        certificate_format);
            }
        };
    }
}

Output run(
        const Instance& instance,
        const po::variables_map& vm)
{
    Seed seed = 0;
    if (vm.count("seed"))
        seed = vm["seed"].as<Seed>();
    std::mt19937_64 generator(seed);

    // Run algorithm.
    std::string algorithm = vm["algorithm"].as<std::string>();

    if (algorithm == "greedy-longest-processing-time") {
        Parameters parameters;
        read_args(parameters, vm);
        return greedy_longest_processing_time(instance, parameters);

    } else if (algorithm == "greedy-shortest-processing-time") {
        Parameters parameters;
        read_args(parameters, vm);
        return greedy_shortest_processing_time(instance, parameters);

    } else if (algorithm == "greedy-earliest-due-date") {
        Parameters parameters;
        read_args(parameters, vm);
        return greedy_earliest_due_date(instance, parameters);

    } else if (algorithm == "moore-hodgson") {
        Parameters parameters;
        read_args(parameters, vm);
        return moore_hodgson(instance, parameters);

    } else if (algorithm == "assignment-total-flow-time") {
        Parameters parameters;
        read_args(parameters, vm);
        return assignment_total_flow_time(instance, parameters);

    } else if (algorithm == "brute-force") {
        BruteForceParameters parameters;
        read_args(parameters, vm);
        if (vm.count("maximum-number-of-candidates"))
            parameters.maximum_number_of_candidates = vm["maximum-number-of-candidates"].as<Counter>();
        return brute_force(instance, parameters);

    } else if (algorithm == "successive-sublimation-dynamic-programming") {
        SuccessiveSublimationDynamicProgrammingParameters parameters;
        read_args(parameters, vm);
        if (vm.count("column-generation"))
            parameters.column_generation = vm["column-generation"].as<bool>();
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("interval-branching"))
            parameters.interval_branching = vm["interval-branching"].as<bool>();
        if (vm.count("maximum-memory"))
            parameters.maximum_memory = vm["maximum-memory"].as<double>();
        if (vm.count("initial-local-search-restarts"))
            parameters.initial_local_search_number_of_restarts = vm["initial-local-search-restarts"].as<Counter>();
        if (vm.count("lagrangian-heuristic-local-search"))
            parameters.lagrangian_heuristic_local_search = vm["lagrangian-heuristic-local-search"].as<bool>();
        if (vm.count("minimum-branching-depth"))
            parameters.minimum_branching_depth = vm["minimum-branching-depth"].as<Counter>();
        if (vm.count("maximum-number-of-stage-1-iterations"))
            parameters.maximum_number_of_stage_1_iterations = vm["maximum-number-of-stage-1-iterations"].as<Counter>();
        if (vm.count("maximum-number-of-refinement-steps"))
            parameters.maximum_number_of_refinement_steps = vm["maximum-number-of-refinement-steps"].as<Counter>();
        if (vm.count("tailing-off-number-of-steps"))
            parameters.tailing_off_number_of_steps = vm["tailing-off-number-of-steps"].as<Counter>();
        if (vm.count("tailing-off-minimum-improvement"))
            parameters.tailing_off_minimum_improvement = vm["tailing-off-minimum-improvement"].as<double>();
        if (vm.count("warm-start"))
            parameters.warm_start = vm["warm-start"].as<bool>();
        return successive_sublimation_dynamic_programming(instance, parameters);

    } else if (algorithm == "dynamic-programming-time-indexed") {
        DynamicProgrammingTimeIndexedParameters parameters;
        read_args(parameters, vm);
        if (vm.count("all-jobs-in-modifiers"))
            parameters.all_jobs_in_modifiers = vm["all-jobs-in-modifiers"].as<bool>();
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("maximum-memory"))
            parameters.maximum_memory = vm["maximum-memory"].as<double>();
        return dynamic_programming_time_indexed(instance, parameters);

    } else if (algorithm == "lagrangian-relaxation") {
        LagrangianRelaxationParameters parameters;
        read_args(parameters, vm);
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("maximum-number-of-iterations"))
            parameters.maximum_number_of_iterations = vm["maximum-number-of-iterations"].as<Counter>();
        return lagrangian_relaxation(instance, parameters);

    } else if (algorithm == "column-generation") {
        ColumnGenerationParameters parameters;
        read_args(parameters, vm);
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("maximum-number-of-iterations"))
            parameters.maximum_number_of_iterations = vm["maximum-number-of-iterations"].as<Counter>();
        return column_generation(instance, parameters);

    } else if (algorithm == "relaxation-refinement") {
        RelaxationRefinementParameters parameters;
        read_args(parameters, vm);
        if (vm.count("column-generation"))
            parameters.column_generation = vm["column-generation"].as<bool>();
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("maximum-memory"))
            parameters.maximum_memory = vm["maximum-memory"].as<double>();
        if (vm.count("bound-to-prove"))
            parameters.bound_to_prove = vm["bound-to-prove"].as<Time>();
        if (vm.count("maximum-number-of-steps"))
            parameters.maximum_number_of_steps = vm["maximum-number-of-steps"].as<Counter>();
        if (vm.count("tailing-off-number-of-steps"))
            parameters.tailing_off_number_of_steps = vm["tailing-off-number-of-steps"].as<Counter>();
        if (vm.count("tailing-off-minimum-improvement"))
            parameters.tailing_off_minimum_improvement = vm["tailing-off-minimum-improvement"].as<double>();
        return relaxation_refinement(instance, parameters);

    } else if (algorithm == "tree-search-iterative-refinement") {
        TreeSearchIterativeRefinementParameters parameters;
        read_args(parameters, vm);
        if (vm.count("column-generation"))
            parameters.column_generation = vm["column-generation"].as<bool>();
        if (vm.count("constraint-propagation"))
            parameters.constraint_propagation = vm["constraint-propagation"].as<bool>();
        if (vm.count("interval-branching"))
            parameters.interval_branching = vm["interval-branching"].as<bool>();
        if (vm.count("maximum-memory"))
            parameters.maximum_memory = vm["maximum-memory"].as<double>();
        if (vm.count("minimum-branching-depth"))
            parameters.minimum_branching_depth = vm["minimum-branching-depth"].as<Counter>();
        if (vm.count("maximum-number-of-refinement-steps"))
            parameters.maximum_number_of_refinement_steps = vm["maximum-number-of-refinement-steps"].as<Counter>();
        if (vm.count("tailing-off-number-of-steps"))
            parameters.tailing_off_number_of_steps = vm["tailing-off-number-of-steps"].as<Counter>();
        if (vm.count("tailing-off-minimum-improvement"))
            parameters.tailing_off_minimum_improvement = vm["tailing-off-minimum-improvement"].as<double>();
        if (vm.count("warm-start"))
            parameters.warm_start = vm["warm-start"].as<bool>();
        return tree_search_iterative_refinement(instance, parameters);

    } else if (algorithm == "local-search") {
        LocalSearchParameters parameters;
        read_args(parameters, vm);
        if (vm.count("maximum-number-of-restarts"))
            parameters.maximum_number_of_restarts = vm["maximum-number-of-restarts"].as<Counter>();
        if (vm.count("maximum-number-of-perturbations-without-improvement"))
            parameters.maximum_number_of_perturbations_without_improvement = vm["maximum-number-of-perturbations-without-improvement"].as<Counter>();
        return local_search(instance, generator, nullptr, parameters);

    } else if (algorithm == "milp-time-indexed") {
        MilpTimeIndexedParameters parameters;
        read_args(parameters, vm);
        if (vm.count("solver"))
            parameters.solver = vm["solver"].as<mathoptsolverscmake::SolverName>();
        if (vm.count("normal-instants"))
            parameters.normal_instants = vm["normal-instants"].as<bool>();
        return milp_time_indexed(instance, parameters);

    } else if (algorithm == "milp-assignment") {
        MilpAssignmentParameters parameters;
        read_args(parameters, vm);
        if (vm.count("solver"))
            parameters.solver = vm["solver"].as<mathoptsolverscmake::SolverName>();
        return milp_assignment(instance, parameters);

    } else if (algorithm == "milp-positional") {
        MilpPositionalParameters parameters;
        read_args(parameters, vm);
        if (vm.count("solver"))
            parameters.solver = vm["solver"].as<mathoptsolverscmake::SolverName>();
        return milp_positional(instance, parameters);

    } else {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "unknown algorithm \"" + algorithm + "\".");
    }

    return Output(instance);
}

int main(int argc, char *argv[])
{

    // Parse program options
    po::options_description desc("Allowed options");
    desc.add_options()
        (",h", "Produce help message")

        ("input,i", po::value<std::string>()->required(), "Input path")
        ("format,f", po::value<std::string>(), "Input format")

        ("algorithm,a", po::value<std::string>()->required(), "set algorithm")
        ("solver,", po::value<mathoptsolverscmake::SolverName>(), "set solver")
        ("solver-log,", po::value<std::string>(), "set the log path of the MILP solver")
        ("normal-instants,", po::value<bool>(), "time-indexed MILP: restrict the start times to the normal instants (0 or 1)")

        ("output,o", po::value<std::string>(), "Output path")
        ("certificate,c", po::value<std::string>(), "Certificate path")
        ("log,l", po::value<std::string>(), "Log path")
        ("time-limit,t", po::value<double>(), "Time limit in seconds")
        ("seed,s", po::value<Seed>(), "Seed (used by the local search)")
        ("column-generation,", po::value<bool>(), "successive sublimation dynamic programming: stage 1 by column generation")
        ("constraint-propagation,", po::value<bool>(), "successive sublimation dynamic programming: network reduction by constraint propagation")
        ("interval-branching,", po::value<bool>(), "successive sublimation dynamic programming: interval branching")
        ("maximum-memory,", po::value<double>(), "successive sublimation dynamic programming: maximum memory for the networks (MB)")
        ("initial-local-search-restarts,", po::value<Counter>(), "successive sublimation dynamic programming: restarts of the initial local search")
        ("lagrangian-heuristic-local-search,", po::value<bool>(), "successive sublimation dynamic programming: local search in the Lagrangian heuristic")
        ("minimum-branching-depth,", po::value<Counter>(), "successive sublimation dynamic programming: minimum depth of the interval branching")
        ("all-jobs-in-modifiers,", po::value<bool>(), "time-indexed dynamic programming: all the jobs in the state-space modifiers (exact)")
        ("maximum-number-of-iterations,", po::value<Counter>(), "Lagrangian relaxation, column generation: maximum number of evaluations of the relaxation")
        ("bound-to-prove,", po::value<Time>(), "relaxation refinement: bound to prove (default: the upper bound)")
        ("maximum-number-of-steps,", po::value<Counter>(), "relaxation refinement: maximum number of refinement steps")
        ("warm-start,", po::value<bool>(), "tree search with iterative refinement, successive sublimation dynamic programming: branching on the last network of the relaxation refinement")
        ("maximum-number-of-refinement-steps,", po::value<Counter>(), "tree search with iterative refinement, successive sublimation dynamic programming: branching after this number of refinement steps (-1: no limit)")
        ("tailing-off-number-of-steps,", po::value<Counter>(), "relaxation refinement, tree search with iterative refinement, successive sublimation dynamic programming: number of steps of the tailing-off rule (0: none)")
        ("tailing-off-minimum-improvement,", po::value<double>(), "relaxation refinement, tree search with iterative refinement, successive sublimation dynamic programming: minimum bound improvement of the tailing-off rule, relative to the gap")
        ("maximum-number-of-stage-1-iterations,", po::value<Counter>(), "successive sublimation dynamic programming: maximum number of iterations of stage 1")
        ("maximum-number-of-candidates,", po::value<Counter>(), "brute force: maximum number of candidates")
        ("maximum-number-of-restarts,", po::value<Counter>(), "local search: maximum number of restarts")
        ("maximum-number-of-perturbations-without-improvement,", po::value<Counter>(), "local search: maximum number of consecutive perturbations without improvement in a restart")
        ("only-write-at-the-end,e", "Only write output and certificate files at the end")
        ("verbosity-level,v", po::value<int>(), "Verbosity level")
        ("log-to-stderr,w", "Write log in stderr")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;;
        return 1;
    }
    try {
        po::notify(vm);
    } catch (const po::required_option& e) {
        std::cout << desc << std::endl;;
        return 1;
    }

    // Build instance.
    InstanceBuilder instance_builder;
    std::string input_format = "";
    if (vm.count("format"))
        input_format = vm["format"].as<std::string>();
    instance_builder.read(
            vm["input"].as<std::string>(),
            input_format);
    Instance instance = instance_builder.build();

    // Run.
    Output output = run(instance, vm);

    // Write output.
    if (vm.count("certificate")) {
        std::string certificate_format = "json";
        if (vm.count("certificate-format"))
            certificate_format = vm["certificate-format"].as<std::string>();
        output.solution.write(
                vm["certificate"].as<std::string>(),
                certificate_format);
    }
    if (vm.count("output"))
        output.write_json_output(vm["output"].as<std::string>());

    return 0;
}
