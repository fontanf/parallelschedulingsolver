#pragma once

#include "parallelschedulingsolver/instance.hpp"

namespace parallelschedulingsolver
{

class InstanceBuilder
{

public:

    /** Constructor. */
    InstanceBuilder() { }

    /** Read instance from a file. */
    void read(
            const std::string& instance_path,
            const std::string& format);

    /** Set the objective. */
    void set_objective(Objective objective) { instance_.objective_ = objective; }

    /**
     * Set the number of machines.
     *
     * This resets the jobs of the instance.
     */
    void set_number_of_machines(MachineId number_of_machines);

    /** Add a job. */
    JobId add_job();

    /** Set the processing-time of a job for a given machine. */
    void set_job_processing_time(
            JobId job_id,
            MachineId machine_id,
            Time processing_time);

    /** Set the release date of a job. */
    void set_job_release_date(
            JobId job_id,
            Time release_date);

    /** Set the due date of a job. */
    void set_job_due_date(
            JobId job_id,
            Time due_date);

    /** Set the weight of a job. */
    void set_job_weight(
            JobId job_id,
            Time weight);

    /**
     * Set the rejection cost of a job: the job may not be scheduled, and then
     * its rejection cost is added to the objective; -1 if the job must be
     * scheduled (default).
     *
     * Rejection costs are supported with the makespan, the total (weighted)
     * flow time and the total (weighted) tardiness.
     */
    void set_job_rejection_cost(
            JobId job_id,
            Time rejection_cost);

    /**
     * Set the setup time when job_id_2 is processed right after job_id_1 on a
     * machine; job_id_1 = -1 for the setup time when job_id_2 is the first
     * job of the machine.
     *
     * The setup times that are not set are 0.
     */
    void set_setup_time(
            MachineId machine_id,
            JobId job_id_1,
            JobId job_id_2,
            Time setup_time);

    /*
     * Build
     */

    /** Build. */
    Instance build();

private:

    /*
     * Private methods
     */

    /** Read an instance from a file in 'json' format. */
    void read_json(std::ifstream& file);

    /**
     * Read an instance from a file in the format of the P || sum T_j
     * instances of Tanaka and Araki (2008).
     */
    void read_tanaka2008(std::ifstream& file);

    /**
     * Read an instance from a file in the format of the 1 | s_ij | sum w_j T_j
     * instances of Cicirello (2003).
     */
    void read_cicirello2003(std::ifstream& file);

    /**
     * Read an instance from a file in the format of the R | s_ijk | C_max
     * instances of Vallada and Ruiz (2011).
     */
    void read_vallada2011(std::ifstream& file);

    /*
     * Private attributes
     */

    /** Instance. */
    Instance instance_;

    /** Setup times set with set_setup_time. */
    struct SetupTime
    {
        MachineId machine_id;
        JobId job_id_1;
        JobId job_id_2;
        Time setup_time;
    };
    std::vector<SetupTime> setup_times_;

};

}
