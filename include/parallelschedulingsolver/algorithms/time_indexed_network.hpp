/**
 * Network of the successive sublimation dynamic programming (SSDP) algorithm
 * of Tanaka and Araki (2013) for 1 | s_ij | sum f_j(C_j).
 *
 * A node v_it corresponds to the completion of job i at time t. An arc from
 * v_{j,t - s_ji - p_i} to v_it corresponds to job i processed right after job
 * j, and its length is f_i(t) - mu_i. Without idle time, a schedule is a path
 * from the source (time 0) to the sink.
 *
 * With state-space modifiers on a set of jobs M (Tanaka et al., 2009), a node
 * (a state) is v^b_it, where b is the set of jobs of M processed up to and
 * including job i: the jobs of M are processed exactly once on the paths
 * from the source to the sink. With M empty, it is the network of the
 * relaxation (LR2) of Tanaka and Araki (2013).
 *
 * The constraints on three successive nodes (no job twice in three
 * successive jobs, i.e. no 2-cycle) are imposed by the dynamic programming:
 * each state stores the shortest and the second shortest paths with
 * different predecessors (forward) or successors (backward).
 *
 * The network is stored explicitly, but the arcs are only stored as the set
 * of the jobs of the predecessors of each state: the predecessor state
 * through job j is determined by the state.
 */

#pragma once

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "parallelschedulingsolver/solution.hpp"

#include "optimizationtools/utils/timer.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace parallelschedulingsolver
{
namespace time_indexed
{

constexpr double INF = std::numeric_limits<double>::infinity();


/** Number of trailing zero bits of a nonzero word. */
inline int count_trailing_zeros(uint64_t word)
{
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, word);
    return (int)index;
#else
    return __builtin_ctzll(word);
#endif
}

/**
 * Set of jobs, with one more element (the source or the sink); at most 127
 * jobs.
 */
struct Mask
{
    std::array<uint64_t, 2> words = {0, 0};

    bool get(JobId job_id) const { return (words[job_id >> 6] >> (job_id & 63)) & 1; }

    void set(JobId job_id) { words[job_id >> 6] |= ((uint64_t)1 << (job_id & 63)); }

    void reset(JobId job_id) { words[job_id >> 6] &= ~((uint64_t)1 << (job_id & 63)); }

    bool empty() const { return words[0] == 0 && words[1] == 0; }

    bool operator==(const Mask& mask) const { return words == mask.words; }

    bool operator!=(const Mask& mask) const { return words != mask.words; }

    bool operator<(const Mask& mask) const { return words < mask.words; }

    bool operator>(const Mask& mask) const { return words > mask.words; }

    Mask operator&(const Mask& mask) const { return {{words[0] & mask.words[0], words[1] & mask.words[1]}}; }

    /** Call f(job_id) for each element, in increasing order. */
    template <typename F>
    void for_each(F f) const
    {
        for (int word_id = 0; word_id < 2; ++word_id) {
            uint64_t word = words[word_id];
            while (word != 0) {
                int bit = count_trailing_zeros(word);
                f((JobId)(64 * word_id + bit));
                word &= word - 1;
            }
        }
    }
};

/**
 * Problem solved by the network: the jobs of the instance without the jobs
 * removed in preprocessing, indexed from 0 to n - 1.
 */
struct Problem
{
    /** Number of jobs. */
    JobId number_of_jobs = 0;

    /** Job of the instance of each job. */
    std::vector<JobId> instance_job_ids;

    /**
     * Jobs of the instance removed in preprocessing (zero cost jobs), in the
     * order in which they are scheduled, after the other jobs.
     */
    std::vector<JobId> removed_job_ids;

    std::vector<Time> processing_times;

    /** Setup times, indexed by (job_id_1 + 1) * n + job_id_2, -1 for the first job. */
    std::vector<Time> setup_times;

    std::vector<Time> weights;

    std::vector<Time> due_dates;

    /** Total weighted tardiness if true, total weighted completion time otherwise. */
    bool tardiness = true;

    /** Lower bound of the makespan. */
    Time minimum_makespan = 0;

    /** Upper bound of the makespan. */
    Time maximum_makespan = 0;

    /**
     * Processing time plus the smallest setup time before the job, for the
     * constraint propagation.
     */
    std::vector<Time> extended_processing_times;

    Time setup_time(JobId job_id_1, JobId job_id_2) const { return setup_times[(job_id_1 + 1) * number_of_jobs + job_id_2]; }

    /** Cost f_i(t) of job i completed at t. */
    Time cost(JobId job_id, Time t) const
    {
        if (tardiness) {
            if (due_dates[job_id] < 0 || t <= due_dates[job_id])
                return 0;
            return weights[job_id] * (t - due_dates[job_id]);
        }
        return weights[job_id] * t;
    }

    /** Total cost of a sequence of jobs. */
    Time sequence_cost(const std::vector<JobId>& sequence) const;
};

/**
 * Build the problem of an instance: single machine, total (weighted)
 * tardiness or total (weighted) completion time, without release dates.
 *
 * If 'remove_zero_cost_jobs', the zero cost jobs that can be moved to the end
 * of any schedule without increasing its cost (Section 5.1 of Tanaka and
 * Araki, 2013) are removed, one at a time. The bounds of the makespan are
 * computed with the assignment relaxation of the asymmetric traveling
 * salesman problem of the setup times.
 *
 * Throw std::invalid_argument if the instance is not supported, or if the
 * problem has more than 127 jobs.
 */
std::shared_ptr<const Problem> build_problem(
        const Instance& instance,
        bool remove_zero_cost_jobs = true);

/**
 * Solution of the instance from a sequence of jobs of the problem, followed
 * by the removed jobs, each job as early as possible.
 */
Solution build_solution(
        const Instance& instance,
        const Problem& problem,
        const std::vector<JobId>& sequence);

/**
 * Shortest and second shortest path lengths with different neighbor jobs:
 * the predecessor in the forward direction, the successor in the backward
 * direction.
 */
struct Labels
{
    double best = INF;

    double second = INF;

    int16_t best_job = -1;

    int16_t second_job = -1;

    void reset() { *this = Labels(); }

    /** Add a path of a given length through a given neighbor job. */
    void add(double value, JobId job_id)
    {
        if (job_id == best_job) {
            if (value < best)
                best = value;
        } else if (value < best) {
            second = best;
            second_job = best_job;
            best = value;
            best_job = job_id;
        } else if (job_id == second_job) {
            if (value < second)
                second = value;
        } else if (value < second) {
            second = value;
            second_job = job_id;
        }
    }

    /** Shortest path length whose neighbor job is not job_id. */
    double excluding(JobId job_id) const { return (best_job != job_id)? best: second; }

    /** Neighbor job of the shortest path whose neighbor job is not job_id. */
    JobId excluding_job(JobId job_id) const { return (best_job != job_id)? best_job: second_job; }
};

struct State
{
    /** Jobs of M processed up to and including the job of the state. */
    Mask mask;

    /** Jobs of the predecessors (arcs); element n: the source. */
    Mask predecessors;

    /** True if the arc to the sink exists. */
    bool sink = false;

    /** False if the state is to be removed. */
    bool alive = true;
};

enum class Direction { Forward, Backward };

class Network
{

public:

    /** Create an empty network. */
    Network(const std::shared_ptr<const Problem>& problem):
        problem_(problem),
        bucket_starts_((problem->maximum_makespan + 1) * problem->number_of_jobs + 1, 0) { }

    /*
     * Getters.
     */

    const Problem& problem() const { return *problem_; }

    const std::shared_ptr<const Problem>& problem_pointer() const { return problem_; }

    /** Jobs with state-space modifiers (M). */
    const Mask& modifiers() const { return modifiers_; }

    Counter number_of_states() const { return states_.size(); }

    /** Number of states of each job. */
    std::vector<Counter> number_of_states_by_job() const;

    /** Number of states of a job by completion time. */
    std::vector<Counter> number_of_states_by_time(JobId job_id) const;

    /** Smallest and largest completion times of each job in the network. */
    void time_windows(
            std::vector<Time>& earliest_completion_times,
            std::vector<Time>& latest_completion_times) const;

    /** Memory used, in bytes. */
    double memory() const
    {
        return (double)states_.capacity() * (sizeof(State) + 2 * sizeof(Labels))
            + (double)bucket_starts_.capacity() * sizeof(int64_t);
    }

    /** Direction of the labels computed last. */
    Direction labels_direction() const { return labels_direction_; }

    /**
     * True if the labels of the given direction have been computed with
     * these multipliers, and the network hasn't been modified since.
     */
    bool labels_up_to_date(
            Direction direction,
            const std::vector<double>& multipliers) const
    {
        int d = (direction == Direction::Forward)? 0: 1;
        return labels_valid_[d] && labels_multipliers_[d] == multipliers;
    }

    /**
     * True if some completion times have been restricted (restrict), in this
     * network or in the networks it has been generated from: the paths of the
     * network are then not all the schedules of the problem.
     */
    bool restricted() const { return restricted_; }

    /**
     * Smallest threshold of the reductions and generations of this network
     * and of the networks it has been generated from: all the schedules of
     * cost at most this threshold are paths of the network (unless it is
     * restricted). INF if none.
     */
    double cutoff() const { return cutoff_; }

    /*
     * Dynamic programming on the current network.
     */

    /** Compute the forward labels. */
    void compute_forward_labels(const std::vector<double>& multipliers);

    /** Compute the backward labels. */
    void compute_backward_labels(const std::vector<double>& multipliers);

    /**
     * Length of the shortest path (without the sum of the multipliers),
     * from the labels of the given direction; INF if the network is empty.
     */
    double shortest_path_length(
            Direction direction,
            const std::vector<double>& multipliers) const;

    /** Sequence of jobs of the shortest path, from the labels of the given direction. */
    std::vector<JobId> shortest_path(
            Direction direction,
            const std::vector<double>& multipliers) const;

    /*
     * Network reduction.
     */

    /**
     * Remove the arcs (and the states) whose lower bound of the length of the
     * paths through them (including the sum of the multipliers) is larger
     * than 'threshold'. Both the forward and the backward labels must have
     * been computed with the same multipliers.
     */
    void reduce(
            const std::vector<double>& multipliers,
            double sum_of_multipliers,
            double threshold);

    /**
     * Constraint propagation (Tanaka and Fujikuma, 2012): time windows from
     * the network, consistency tests, removal of the nodes outside the time
     * windows and direct elimination.
     */
    void propagate_constraints();

    /** Only keep the states of job_id completed in [t_min, t_max]. */
    void restrict(
            JobId job_id,
            Time t_min,
            Time t_max);

    /*
     * Generation.
     */

    /**
     * Generate the network with the modifiers of this network plus the
     * given jobs, in the given direction, with the labels of this direction.
     *
     * The labels of the other direction of this network must have been
     * computed with the same multipliers: an arc of the new network is not
     * generated if the lower bound of the length of the paths through it is
     * larger than 'threshold'.
     *
     * Return false if the memory used by both networks exceeds
     * 'maximum_memory' or if the time limit is reached.
     */
    bool generate(
            Network& new_network,
            const Mask& new_modifiers,
            Direction direction,
            const std::vector<double>& multipliers,
            double sum_of_multipliers,
            double threshold,
            double maximum_memory,
            const optimizationtools::Timer* timer) const;

    /**
     * Initial network (M empty) with all the states reachable from the
     * source and from which the sink is reachable.
     */
    static Network initial_network(const std::shared_ptr<const Problem>& problem);

private:

    /*
     * Private methods.
     */

    int64_t bucket_id(JobId job_id, Time t) const { return t * problem_->number_of_jobs + job_id; }

    /** Index of the state of job_id completed at t with a given mask; -1 if none. */
    int64_t find(JobId job_id, Time t, const Mask& mask) const;

    /** Add a state at the end of the states. */
    void push_state(
            const State& state,
            const Labels& forward,
            const Labels& backward)
    {
        states_.push_back(state);
        forward_labels_.push_back(forward);
        backward_labels_.push_back(backward);
    }

    /** Mask of the predecessor of a state of job_id with a given mask. */
    Mask predecessor_mask(const Mask& mask, JobId job_id) const
    {
        Mask mask_2 = mask;
        if (modifiers_.get(job_id))
            mask_2.reset(job_id);
        return mask_2;
    }

    /**
     * Mask of the successor job_id of a state with a given mask; false if
     * job_id is in M and already processed.
     */
    bool successor_mask(const Mask& mask, JobId job_id, Mask& successor_mask) const
    {
        successor_mask = mask;
        if (modifiers_.get(job_id)) {
            if (mask.get(job_id))
                return false;
            successor_mask.set(job_id);
        }
        return true;
    }

    /** Record that the labels of a direction are computed with these multipliers. */
    void set_labels_up_to_date(
            Direction direction,
            const std::vector<double>& multipliers)
    {
        int d = (direction == Direction::Forward)? 0: 1;
        labels_valid_[d] = true;
        labels_multipliers_[d] = multipliers;
    }

    /** Record that the labels are not valid anymore (states removed). */
    void invalidate_labels()
    {
        labels_valid_[0] = false;
        labels_valid_[1] = false;
    }

    /** Remove the states that are not on a path from the source to the sink. */
    void clean();

    /** Remove the states that are not alive. */
    void compact();

    /*
     * Private attributes.
     */

    std::shared_ptr<const Problem> problem_;

    Mask modifiers_;

    /** States, sorted by time, job and mask. */
    std::vector<State> states_;

    /**
     * Forward labels of the states: paths from the source, including the
     * cost of the job of the state.
     */
    std::vector<Labels> forward_labels_;

    /**
     * Backward labels of the states: paths to the sink, excluding the cost
     * of the job of the state.
     */
    std::vector<Labels> backward_labels_;

    /** Start of the states of each (time, job) in 'states_'. */
    std::vector<int64_t> bucket_starts_;

    Direction labels_direction_ = Direction::Forward;

    /** Multipliers of the labels of each direction (forward, backward). */
    std::vector<double> labels_multipliers_[2];

    /** True if the labels of each direction are valid for labels_multipliers_. */
    bool labels_valid_[2] = {false, false};

    /** See restricted(). */
    bool restricted_ = false;

    /** See cutoff(). */
    double cutoff_ = INF;

    /** Labels by bucket, for the dynamic programming without modifiers. */
    std::vector<Labels> dense_labels_;

};

}
}
