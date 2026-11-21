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
}

Instance InstanceBuilder::build()
{
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
