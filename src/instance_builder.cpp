#include "parallelschedulingsolver/instance_builder.hpp"

#include <sstream>

using namespace parallelschedulingsolver;

void InstanceBuilder::set_number_of_machines(MachineId number_of_machines)
{
    if (number_of_machines <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'number_of_machines' must be > 0; "
                "number_of_machines: " + std::to_string(number_of_machines) + ".");
    }

    instance_.machines_ = std::vector<Machine>(number_of_machines);
}

JobId InstanceBuilder::add_job()
{
    JobId job_id = (JobId)instance_.jobs_.size();
    Job job;
    job.processing_times = std::vector<Time>(instance_.number_of_machines());
    instance_.jobs_.push_back(job);
    return job_id;
}

void InstanceBuilder::set_job_processing_time(
        JobId job_id,
        MachineId machine_id,
        Time processing_time)
{
    if (job_id < 0 || job_id >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id'; "
                "job_id: " + std::to_string(job_id) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (machine_id < 0 || machine_id >= (MachineId)instance_.machines_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'machine_id'; "
                "machine_id: " + std::to_string(machine_id) + "; "
                "instance_.machines_.size(): " + std::to_string(instance_.machines_.size()) + ".");
    }
    if (processing_time <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'processing_time' must be > 0; "
                "processing_time: " + std::to_string(processing_time) + ".");
    }

    instance_.jobs_[job_id].processing_times[machine_id] = processing_time;
}

void InstanceBuilder::set_job_release_date(
        JobId job_id,
        Time release_date)
{
    if (job_id < 0 || job_id >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id'; "
                "job_id: " + std::to_string(job_id) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (release_date < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'release_date' must be >= 0; "
                "release_date: " + std::to_string(release_date) + ".");
    }

    instance_.jobs_[job_id].release_date = release_date;
}

void InstanceBuilder::set_job_due_date(
        JobId job_id,
        Time due_date)
{
    if (job_id < 0 || job_id >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id'; "
                "job_id: " + std::to_string(job_id) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (due_date < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'due_date' must be >= 0; "
                "due_date: " + std::to_string(due_date) + ".");
    }

    instance_.jobs_[job_id].due_date = due_date;
}

void InstanceBuilder::set_job_weight(
        JobId job_id,
        Time weight)
{
    if (job_id < 0 || job_id >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id'; "
                "job_id: " + std::to_string(job_id) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (weight < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'weight' must be >= 0; "
                "weight: " + std::to_string(weight) + ".");
    }

    instance_.jobs_[job_id].weight = weight;
}

void InstanceBuilder::set_setup_time(
        MachineId machine_id,
        JobId job_id_1,
        JobId job_id_2,
        Time setup_time)
{
    if (machine_id < 0 || machine_id >= (MachineId)instance_.machines_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'machine_id'; "
                "machine_id: " + std::to_string(machine_id) + "; "
                "instance_.machines_.size(): " + std::to_string(instance_.machines_.size()) + ".");
    }
    if (job_id_1 < -1 || job_id_1 >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id_1'; "
                "job_id_1: " + std::to_string(job_id_1) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (job_id_2 < 0 || job_id_2 >= (JobId)instance_.jobs_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'job_id_2'; "
                "job_id_2: " + std::to_string(job_id_2) + "; "
                "instance_.jobs_.size(): " + std::to_string(instance_.jobs_.size()) + ".");
    }
    if (setup_time < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'setup_time' must be >= 0; "
                "setup_time: " + std::to_string(setup_time) + ".");
    }

    setup_times_.push_back({machine_id, job_id_1, job_id_2, setup_time});
}

void InstanceBuilder::read(
        const std::string& instance_path,
        const std::string& format)
{
    std::ifstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + instance_path + "\".");
    }
    if (format == "" || format == "json") {
        read_json(file);
    } else if (format == "tanaka2008") {
        read_tanaka2008(file);
    } else if (format == "cicirello2003") {
        read_cicirello2003(file);
    } else if (format == "vallada2011") {
        read_vallada2011(file);
    } else {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "unknown instance format \"" + format + "\".");
    }
    file.close();
}

void InstanceBuilder::read_json(std::ifstream& file)
{
    nlohmann::json j;
    file >> j;

    if (j.contains("objective")) {
        std::stringstream objective_ss;
        objective_ss << std::string(j["objective"]);
        Objective objective;
        if (!(objective_ss >> objective)) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "unknown objective \"" + std::string(j["objective"]) + "\".");
        }
        set_objective(objective);
    }

    MachineId number_of_machines = j["number_of_machines"];
    set_number_of_machines(number_of_machines);

    JobId number_of_jobs = (JobId)j["jobs"].size();
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
        add_job();
        for (MachineId machine_id = 0;
                machine_id < number_of_machines;
                ++machine_id) {
            set_job_processing_time(
                    job_id,
                    machine_id,
                    j["jobs"][job_id]["processing_times"][machine_id]);
        }
        if (j["jobs"][job_id].contains("release_date"))
            set_job_release_date(job_id, j["jobs"][job_id]["release_date"]);
        if (j["jobs"][job_id].contains("due_date"))
            set_job_due_date(job_id, j["jobs"][job_id]["due_date"]);
        if (j["jobs"][job_id].contains("weight"))
            set_job_weight(job_id, j["jobs"][job_id]["weight"]);
    }

    // Setup times.
    for (MachineId machine_id = 0;
            machine_id < number_of_machines;
            ++machine_id) {
        if (!j.contains("machines")
                || machine_id >= (MachineId)j["machines"].size()) {
            continue;
        }
        const nlohmann::json& json_machine = j["machines"][machine_id];
        if (json_machine.contains("initial_setup_times")) {
            for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
                Time setup_time = json_machine["initial_setup_times"][job_id];
                if (setup_time != 0)
                    set_setup_time(machine_id, -1, job_id, setup_time);
            }
        }
        if (json_machine.contains("setup_times")) {
            for (JobId job_id_1 = 0; job_id_1 < number_of_jobs; ++job_id_1) {
                for (JobId job_id_2 = 0; job_id_2 < number_of_jobs; ++job_id_2) {
                    Time setup_time = json_machine["setup_times"][job_id_1][job_id_2];
                    if (setup_time != 0)
                        set_setup_time(machine_id, job_id_1, job_id_2, setup_time);
                }
            }
        }
    }
}

Instance InstanceBuilder::build()
{
    // Setup times.
    instance_.has_setup_times_ = false;
    JobId number_of_jobs = instance_.jobs_.size();
    for (Machine& machine: instance_.machines_)
        machine.setup_times.clear();
    for (const SetupTime& setup_time: setup_times_) {
        if (setup_time.machine_id >= (MachineId)instance_.machines_.size()
                || setup_time.job_id_1 >= number_of_jobs
                || setup_time.job_id_2 >= number_of_jobs) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "a setup time refers to a machine or a job that doesn't exist.");
        }
        if (!instance_.has_setup_times_) {
            instance_.has_setup_times_ = true;
            for (Machine& machine: instance_.machines_) {
                machine.setup_times = std::vector<std::vector<Time>>(
                        number_of_jobs + 1,
                        std::vector<Time>(number_of_jobs, 0));
            }
        }
        instance_.machines_[setup_time.machine_id].setup_times
            [setup_time.job_id_1 + 1][setup_time.job_id_2] = setup_time.setup_time;
    }
    setup_times_.clear();

    return std::move(instance_);
}

void InstanceBuilder::read_tanaka2008(std::ifstream& file)
{
    // Instances of P || sum T_j of Tanaka and Araki (2008):
    //
    //     # comment lines
    //     n  m
    //     # comment line
    //     job processing_time due_date   (n lines, jobs numbered from 1)
    //
    // The lines starting with '#' are comments.
    auto next_line = [&file](std::string& line)
    {
        while (std::getline(file, line)) {
            std::size_t pos = line.find_first_not_of(" \t\r");
            if (pos != std::string::npos && line[pos] != '#')
                return true;
        }
        return false;
    };
    std::string line;
    if (!next_line(line)) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "missing number of jobs and machines.");
    }
    JobId number_of_jobs = -1;
    MachineId number_of_machines = -1;
    std::istringstream(line) >> number_of_jobs >> number_of_machines;
    if (number_of_jobs < 0 || number_of_machines <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "wrong number of jobs or machines.");
    }
    set_objective(Objective::TotalTardiness);
    set_number_of_machines(number_of_machines);
    for (JobId job_pos = 0; job_pos < number_of_jobs; ++job_pos) {
        if (!next_line(line)) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "missing job " + std::to_string(job_pos + 1) + ".");
        }
        JobId job_number = -1;
        Time processing_time = -1;
        Time due_date = -1;
        std::istringstream(line) >> job_number >> processing_time >> due_date;
        if (processing_time < 0 || due_date < 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "wrong job " + std::to_string(job_pos + 1) + ".");
        }
        JobId job_id = add_job();
        for (MachineId machine_id = 0;
                machine_id < number_of_machines;
                ++machine_id) {
            set_job_processing_time(job_id, machine_id, processing_time);
        }
        set_job_due_date(job_id, due_date);
    }
}

void InstanceBuilder::read_cicirello2003(std::ifstream& file)
{
    // Instances of 1 | s_ij | sum w_j T_j of Cicirello (2003):
    //
    //     Problem Instance: <number>
    //     Problem Size: <n>
    //     Begin Generator Parameters
    //     ...
    //     End Generator Parameters
    //     Begin Problem Specification
    //     Process Times:
    //     <n lines: processing time>
    //     Weights:
    //     <n lines: weight>
    //     Duedates:
    //     <n lines: due date>
    //     Setup Times:
    //     <lines: job_1 job_2 setup time, job_1 = -1 for the first job>
    //     End Problem Specification
    std::string line;
    JobId number_of_jobs = -1;
    while (std::getline(file, line)) {
        if (line.rfind("Problem Size:", 0) == 0) {
            number_of_jobs = std::stol(line.substr(13));
            break;
        }
    }
    if (number_of_jobs < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "missing problem size.");
    }
    set_objective(Objective::TotalTardiness);
    set_number_of_machines(1);
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id)
        add_job();

    auto read_section = [this, &file, &line, number_of_jobs](
            const std::string& header)
    {
        while (std::getline(file, line))
            if (line.rfind(header, 0) == 0)
                break;
        if (file.eof()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "missing section \"" + header + "\".");
        }
        std::vector<Time> values(number_of_jobs);
        for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
            if (!(file >> values[job_id])) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "wrong section \"" + header + "\".");
            }
        }
        return values;
    };
    std::vector<Time> processing_times = read_section("Process Times:");
    std::vector<Time> weights = read_section("Weights:");
    std::vector<Time> due_dates = read_section("Duedates:");
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
        set_job_processing_time(job_id, 0, processing_times[job_id]);
        set_job_weight(job_id, weights[job_id]);
        set_job_due_date(job_id, due_dates[job_id]);
    }

    while (std::getline(file, line))
        if (line.rfind("Setup Times:", 0) == 0)
            break;
    while (std::getline(file, line)) {
        if (line.rfind("End", 0) == 0)
            break;
        JobId job_id_1 = -2;
        JobId job_id_2 = -1;
        Time setup_time = -1;
        if (!(std::istringstream(line) >> job_id_1 >> job_id_2 >> setup_time))
            continue;
        set_setup_time(0, job_id_1, job_id_2, setup_time);
    }
}

void InstanceBuilder::read_vallada2011(std::ifstream& file)
{
    // Instances of R | s_ijk | C_max of Vallada and Ruiz (2011):
    //
    //     n m 1
    //     m
    //     <n lines: for each machine, machine_id processing_time>
    //     SSD
    //     <for each machine: a line "M<machine_id>", then n lines of n setup
    //     times; line i, column j: setup time when job j follows job i>
    //
    // There is no setup time for the first job of a machine.
    JobId number_of_jobs = -1;
    MachineId number_of_machines = -1;
    file >> number_of_jobs >> number_of_machines;
    if (number_of_jobs < 0 || number_of_machines <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "wrong number of jobs or machines.");
    }
    std::string line;
    std::getline(file, line);
    std::getline(file, line);
    set_objective(Objective::Makespan);
    set_number_of_machines(number_of_machines);
    for (JobId job_id = 0; job_id < number_of_jobs; ++job_id) {
        add_job();
        for (MachineId machine_pos = 0;
                machine_pos < number_of_machines;
                ++machine_pos) {
            MachineId machine_id = -1;
            Time processing_time = -1;
            if (!(file >> machine_id >> processing_time)) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "wrong processing times of job " + std::to_string(job_id) + ".");
            }
            set_job_processing_time(job_id, machine_id, processing_time);
        }
    }
    std::string token;
    if (!(file >> token) || token != "SSD") {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "missing setup times.");
    }
    for (MachineId machine_id = 0;
            machine_id < number_of_machines;
            ++machine_id) {
        // Machine label.
        file >> token;
        for (JobId job_id_1 = 0; job_id_1 < number_of_jobs; ++job_id_1) {
            for (JobId job_id_2 = 0; job_id_2 < number_of_jobs; ++job_id_2) {
                Time setup_time = -1;
                if (!(file >> setup_time)) {
                    throw std::invalid_argument(
                            FUNC_SIGNATURE + ": "
                            "wrong setup times of machine " + std::to_string(machine_id) + ".");
                }
                if (job_id_1 != job_id_2 && setup_time != 0)
                    set_setup_time(machine_id, job_id_1, job_id_2, setup_time);
            }
        }
    }
}
