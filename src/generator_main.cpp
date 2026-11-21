#include "parallelschedulingsolver/generator.hpp"

#include <boost/program_options.hpp>

using namespace parallelschedulingsolver;

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;

    GenerateInput input;
    Seed seed = 0;
    std::string output_path = "";

    // Parse program options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("objective,", po::value<Objective>(&input.objective), "set objective")
        ("number-of-machines,", po::value<MachineId>(&input.number_of_machines), "set number of machines")
        ("number-of-jobs-per-machine,", po::value<JobId>(&input.number_of_jobs_per_machine), "set number of jobs per machine")
        ("processing-times-range,", po::value<Time>(&input.processing_times_range), "set processing times range")
        ("weights-range,", po::value<Time>(&input.weights_range), "set weights range")
        ("setup-times-range,", po::value<Time>(&input.setup_times_range), "setup times in [0, range]; 0: no setup times")
        ("identical-machines,", po::value<bool>(&input.identical_machines), "same processing time on all machines (0 or 1)")
        ("equal-processing-times,", po::value<bool>(&input.equal_processing_times), "same processing time for all jobs (0 or 1)")
        ("release-dates-factor,", po::value<double>(&input.release_dates_factor), "release dates in [0, factor * P]; 0: no release dates")
        ("tardiness-factor,", po::value<double>(&input.tardiness_factor), "tardiness factor TF of the due dates")
        ("due-dates-range,", po::value<double>(&input.due_dates_range), "relative range RDD of the due dates")
        ("seed,", po::value<Seed>(&seed), "set seed")
        ("output,", po::value<std::string>(&output_path)->required(), "set output path")
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

    std::mt19937_64 generator(seed);
    Instance instance = generate(
            input,
            generator);
    instance.write(output_path, "json");

    return 0;
}
