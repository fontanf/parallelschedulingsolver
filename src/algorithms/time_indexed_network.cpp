#include "parallelschedulingsolver/algorithms/time_indexed_network.hpp"

#include "parallelschedulingsolver/solution_builder.hpp"

#include "hungarian.hpp"

#include <algorithm>

using namespace parallelschedulingsolver;
using namespace parallelschedulingsolver::time_indexed;

Time Problem::sequence_cost(const std::vector<JobId>& sequence) const
{
    Time t = 0;
    Time total_cost = 0;
    JobId previous_job_id = -1;
    for (JobId job_id: sequence) {
        t += setup_time(previous_job_id, job_id) + processing_times[job_id];
        total_cost += cost(job_id, t);
        previous_job_id = job_id;
    }
    return total_cost;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Getters /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int64_t Network::find(
        JobId job_id,
        Time t,
        const Mask& mask) const
{
    if (t < 0 || t > problem_->maximum_makespan)
        return -1;
    int64_t bucket = bucket_id(job_id, t);
    int64_t begin = bucket_starts_[bucket];
    int64_t end = bucket_starts_[bucket + 1];
    if (begin == end)
        return -1;
    // Without modifiers, a bucket contains at most one state, with an empty
    // mask.
    if (end - begin == 1)
        return (states_[begin].mask == mask)? begin: -1;
    int64_t lo = begin;
    int64_t hi = end;
    while (lo < hi) {
        int64_t mid = (lo + hi) / 2;
        if (states_[mid].mask < mask) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo == end || states_[lo].mask != mask)
        return -1;
    return lo;
}

std::vector<Counter> Network::number_of_states_by_job() const
{
    JobId n = problem_->number_of_jobs;
    std::vector<Counter> counts(n, 0);
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket)
        counts[bucket % n] += bucket_starts_[bucket + 1] - bucket_starts_[bucket];
    return counts;
}

std::vector<Counter> Network::number_of_states_by_time(JobId job_id) const
{
    std::vector<Counter> counts(problem_->maximum_makespan + 1, 0);
    for (Time t = 0; t <= problem_->maximum_makespan; ++t) {
        int64_t bucket = bucket_id(job_id, t);
        counts[t] = bucket_starts_[bucket + 1] - bucket_starts_[bucket];
    }
    return counts;
}

void Network::time_windows(
        std::vector<Time>& earliest_completion_times,
        std::vector<Time>& latest_completion_times) const
{
    JobId n = problem_->number_of_jobs;
    earliest_completion_times.assign(n, -1);
    latest_completion_times.assign(n, -1);
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        if (bucket_starts_[bucket + 1] == bucket_starts_[bucket])
            continue;
        JobId job_id = bucket % n;
        Time t = bucket / n;
        if (earliest_completion_times[job_id] == -1)
            earliest_completion_times[job_id] = t;
        latest_completion_times[job_id] = t;
    }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// Dynamic programming //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Network::compute_forward_labels(const std::vector<double>& multipliers)
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;
    int64_t number_of_buckets = (int64_t)bucket_starts_.size() - 1;
    bool dense = modifiers_.empty();
    if (dense)
        dense_labels_.assign(number_of_buckets, Labels());
    for (Time t = 0; t <= problem.maximum_makespan; ++t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = bucket_id(job_id, t);
            if (bucket_starts_[bucket] == bucket_starts_[bucket + 1])
                continue;
            double c = problem.cost(job_id, t) - multipliers[job_id];
            Time p = problem.processing_times[job_id];
            for (int64_t state_id = bucket_starts_[bucket];
                    state_id < bucket_starts_[bucket + 1];
                    ++state_id) {
                const State& state = states_[state_id];
                Labels labels;
                if (dense) {
                    // Without modifiers, the predecessor of the state
                    // through job_id_prev is the state of its bucket.
                    state.predecessors.for_each([&](JobId job_id_prev)
                    {
                        if (job_id_prev == n) {
                            labels.add(c, n);
                            return;
                        }
                        int64_t bucket_prev = bucket
                            - (problem.setup_time(job_id_prev, job_id) + p) * n
                            - job_id + job_id_prev;
                        if (bucket_prev < 0)
                            return;
                        double value = dense_labels_[bucket_prev].excluding(job_id);
                        if (value == INF)
                            return;
                        labels.add(value + c, job_id_prev);
                    });
                    dense_labels_[bucket] = labels;
                } else {
                    Mask mask = predecessor_mask(state.mask, job_id);
                    state.predecessors.for_each([&](JobId job_id_prev)
                    {
                        if (job_id_prev == n) {
                            labels.add(c, n);
                            return;
                        }
                        int64_t state_id_prev = find(
                                job_id_prev,
                                t - problem.setup_time(job_id_prev, job_id) - p,
                                mask);
                        if (state_id_prev == -1)
                            return;
                        double value = forward_labels_[state_id_prev].excluding(job_id);
                        if (value == INF)
                            return;
                        labels.add(value + c, job_id_prev);
                    });
                }
                forward_labels_[state_id] = labels;
            }
        }
    }
    labels_direction_ = Direction::Forward;
    set_labels_up_to_date(Direction::Forward, multipliers);
}

void Network::compute_backward_labels(const std::vector<double>& multipliers)
{
    // The labels are pushed from each state to its predecessors, by
    // decreasing time: the labels of a state are complete when it is
    // processed.
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;
    int64_t number_of_buckets = (int64_t)bucket_starts_.size() - 1;
    bool dense = modifiers_.empty();
    if (dense)
        dense_labels_.assign(number_of_buckets, Labels());
    for (Labels& labels: backward_labels_)
        labels.reset();
    for (Time t = problem.maximum_makespan; t >= 0; --t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = bucket_id(job_id, t);
            if (bucket_starts_[bucket] == bucket_starts_[bucket + 1])
                continue;
            double c = problem.cost(job_id, t) - multipliers[job_id];
            Time p = problem.processing_times[job_id];
            for (int64_t state_id = bucket_starts_[bucket];
                    state_id < bucket_starts_[bucket + 1];
                    ++state_id) {
                const State& state = states_[state_id];
                Labels& labels = (dense)?
                    dense_labels_[bucket]:
                    backward_labels_[state_id];
                if (state.sink)
                    labels.add(sink_cost(state.mask, multipliers), n);
                if (dense)
                    backward_labels_[state_id] = labels;
                Mask mask = predecessor_mask(state.mask, job_id);
                state.predecessors.for_each([&](JobId job_id_prev)
                {
                    if (job_id_prev == n)
                        return;
                    double value = labels.excluding(job_id_prev);
                    if (value == INF)
                        return;
                    if (dense) {
                        int64_t bucket_prev = bucket
                            - (problem.setup_time(job_id_prev, job_id) + p) * n
                            - job_id + job_id_prev;
                        if (bucket_prev < 0)
                            return;
                        dense_labels_[bucket_prev].add(value + c, job_id);
                    } else {
                        int64_t state_id_prev = find(
                                job_id_prev,
                                t - problem.setup_time(job_id_prev, job_id) - p,
                                mask);
                        if (state_id_prev == -1)
                            return;
                        backward_labels_[state_id_prev].add(value + c, job_id);
                    }
                });
            }
        }
    }
    labels_direction_ = Direction::Backward;
    set_labels_up_to_date(Direction::Backward, multipliers);
}

namespace
{

struct Endpoint
{
    int64_t state_id = -1;
    JobId job_id = -1;
    Time t = -1;
    double length = INF;
};

}

double Network::shortest_path_length(
        Direction direction,
        const std::vector<double>& multipliers) const
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;
    double length = empty_path_length(multipliers);
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        for (int64_t state_id = bucket_starts_[bucket];
                state_id < bucket_starts_[bucket + 1];
                ++state_id) {
            const State& state = states_[state_id];
            if (direction == Direction::Forward) {
                if (state.sink) {
                    length = (std::min)(
                            length,
                            forward_labels_[state_id].best
                            + sink_cost(state.mask, multipliers));
                }
            } else {
                if (state.predecessors.get(n)) {
                    JobId job_id = bucket % n;
                    Time t = bucket / n;
                    length = (std::min)(
                            length,
                            problem.cost(job_id, t) - multipliers[job_id]
                            + backward_labels_[state_id].best);
                }
            }
        }
    }
    return length;
}

std::vector<JobId> Network::shortest_path(
        Direction direction,
        const std::vector<double>& multipliers) const
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;

    // Last state (forward) or first state (backward) of the path; the empty
    // path if it is the shortest one.
    Endpoint endpoint;
    endpoint.length = empty_path_length(multipliers);
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        JobId job_id = bucket % n;
        Time t = bucket / n;
        for (int64_t state_id = bucket_starts_[bucket];
                state_id < bucket_starts_[bucket + 1];
                ++state_id) {
            const State& state = states_[state_id];
            double length = INF;
            if (direction == Direction::Forward) {
                if (state.sink)
                    length = forward_labels_[state_id].best + sink_cost(state.mask, multipliers);
            } else {
                if (state.predecessors.get(n)) {
                    length = problem.cost(job_id, t) - multipliers[job_id]
                        + backward_labels_[state_id].best;
                }
            }
            if (length < endpoint.length)
                endpoint = {state_id, job_id, t, length};
        }
    }
    std::vector<JobId> sequence;
    if (endpoint.state_id == -1)
        return sequence;

    int64_t state_id = endpoint.state_id;
    JobId job_id = endpoint.job_id;
    Time t = endpoint.t;
    sequence.push_back(job_id);
    if (direction == Direction::Forward) {
        JobId job_id_prev = forward_labels_[state_id].best_job;
        while (job_id_prev != n && job_id_prev != -1) {
            Time t_prev = t - problem.setup_time(job_id_prev, job_id) - problem.processing_times[job_id];
            int64_t state_id_prev = find(
                    job_id_prev,
                    t_prev,
                    predecessor_mask(states_[state_id].mask, job_id));
            if (state_id_prev == -1)
                break;
            JobId job_id_prev_prev = forward_labels_[state_id_prev].excluding_job(job_id);
            t = t_prev;
            job_id = job_id_prev;
            state_id = state_id_prev;
            sequence.push_back(job_id);
            job_id_prev = job_id_prev_prev;
        }
        std::reverse(sequence.begin(), sequence.end());
    } else {
        JobId job_id_next = backward_labels_[state_id].best_job;
        while (job_id_next != n && job_id_next != -1) {
            Mask mask;
            successor_mask(states_[state_id].mask, job_id_next, mask);
            Time t_next = t
                + problem.setup_time(job_id, job_id_next)
                + problem.processing_times[job_id_next];
            int64_t state_id_next = find(job_id_next, t_next, mask);
            if (state_id_next == -1)
                break;
            JobId job_id_next_next = backward_labels_[state_id_next].excluding_job(job_id);
            t = t_next;
            job_id = job_id_next;
            state_id = state_id_next;
            sequence.push_back(job_id);
            job_id_next = job_id_next_next;
        }
    }
    return sequence;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Reduction ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Network::compact()
{
    int64_t write = 0;
    int64_t read = 0;
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        int64_t end = bucket_starts_[bucket + 1];
        bucket_starts_[bucket] = write;
        for (; read < end; ++read) {
            if (states_[read].alive) {
                if (write != read) {
                    states_[write] = states_[read];
                    forward_labels_[write] = forward_labels_[read];
                    backward_labels_[write] = backward_labels_[read];
                }
                write++;
            }
        }
    }
    bucket_starts_.back() = write;
    states_.resize(write);
    states_.shrink_to_fit();
    forward_labels_.resize(write);
    forward_labels_.shrink_to_fit();
    backward_labels_.resize(write);
    backward_labels_.shrink_to_fit();
}

void Network::clean()
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;

    // Forward: states reachable from the source.
    for (Time t = 0; t <= problem.maximum_makespan; ++t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = bucket_id(job_id, t);
            Time p = problem.processing_times[job_id];
            for (int64_t state_id = bucket_starts_[bucket];
                    state_id < bucket_starts_[bucket + 1];
                    ++state_id) {
                State& state = states_[state_id];
                if (!state.alive)
                    continue;
                Mask mask = predecessor_mask(state.mask, job_id);
                Mask predecessors = state.predecessors;
                predecessors.for_each([&](JobId job_id_prev)
                {
                    if (job_id_prev == n)
                        return;
                    int64_t state_id_prev = find(
                            job_id_prev,
                            t - problem.setup_time(job_id_prev, job_id) - p,
                            mask);
                    if (state_id_prev == -1 || !states_[state_id_prev].alive)
                        state.predecessors.reset(job_id_prev);
                });
                if (state.predecessors.empty())
                    state.alive = false;
            }
        }
    }

    // Backward: states from which the sink is reachable.
    for (Time t = problem.maximum_makespan; t >= 0; --t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = bucket_id(job_id, t);
            for (int64_t state_id = bucket_starts_[bucket];
                    state_id < bucket_starts_[bucket + 1];
                    ++state_id) {
                State& state = states_[state_id];
                if (!state.alive || state.sink)
                    continue;
                bool ok = false;
                for (JobId job_id_next = 0; job_id_next < n && !ok; ++job_id_next) {
                    if (job_id_next == job_id)
                        continue;
                    Mask mask;
                    if (!successor_mask(state.mask, job_id_next, mask))
                        continue;
                    Time t_next = t
                        + problem.setup_time(job_id, job_id_next)
                        + problem.processing_times[job_id_next];
                    int64_t state_id_next = find(job_id_next, t_next, mask);
                    if (state_id_next != -1
                            && states_[state_id_next].alive
                            && states_[state_id_next].predecessors.get(job_id)) {
                        ok = true;
                    }
                }
                if (!ok)
                    state.alive = false;
            }
        }
    }

    compact();
}

void Network::reduce(
        const std::vector<double>& multipliers,
        double sum_of_multipliers,
        double threshold)
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;
    for (Time t = 0; t <= problem.maximum_makespan; ++t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = bucket_id(job_id, t);
            if (bucket_starts_[bucket] == bucket_starts_[bucket + 1])
                continue;
            double c = problem.cost(job_id, t) - multipliers[job_id];
            Time p = problem.processing_times[job_id];
            for (int64_t state_id = bucket_starts_[bucket];
                    state_id < bucket_starts_[bucket + 1];
                    ++state_id) {
                State& state = states_[state_id];
                const Labels& forward = forward_labels_[state_id];
                const Labels& backward = backward_labels_[state_id];
                if (state.sink
                        && forward.best + sink_cost(state.mask, multipliers)
                        + sum_of_multipliers > threshold) {
                    state.sink = false;
                }
                Mask mask = predecessor_mask(state.mask, job_id);
                Mask predecessors = state.predecessors;
                predecessors.for_each([&](JobId job_id_prev)
                {
                    if (job_id_prev == n) {
                        if (c + backward.best + sum_of_multipliers > threshold)
                            state.predecessors.reset(n);
                        return;
                    }
                    int64_t state_id_prev = find(
                            job_id_prev,
                            t - problem.setup_time(job_id_prev, job_id) - p,
                            mask);
                    if (state_id_prev == -1
                            || forward_labels_[state_id_prev].excluding(job_id)
                            + c
                            + backward.excluding(job_id_prev)
                            + sum_of_multipliers > threshold) {
                        state.predecessors.reset(job_id_prev);
                    }
                });
                if (state.predecessors.empty())
                    state.alive = false;
            }
        }
    }
    // The shortest paths may have been removed.
    invalidate_labels();
    cutoff_ = (std::min)(cutoff_, threshold);
    clean();
}

void Network::restrict(
        JobId job_id,
        Time t_min,
        Time t_max)
{
    JobId n = problem_->number_of_jobs;
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        if (bucket % n != job_id)
            continue;
        Time t = bucket / n;
        if (t >= t_min && t <= t_max)
            continue;
        for (int64_t state_id = bucket_starts_[bucket];
                state_id < bucket_starts_[bucket + 1];
                ++state_id) {
            states_[state_id].alive = false;
        }
    }
    invalidate_labels();
    restricted_ = true;
    clean();
}

void Network::propagate_constraints()
{
    const Problem& problem = *problem_;
    // The consistency tests assume that all the jobs are scheduled.
    if (problem.has_rejection_costs)
        return;
    JobId n = problem.number_of_jobs;
    const std::vector<Time>& pp = problem.extended_processing_times;

    // Time windows: earliest start r and latest completion d of the
    // processing times extended by the smallest setup times.
    std::vector<Time> earliest_completion_times;
    std::vector<Time> latest_completion_times;
    time_windows(earliest_completion_times, latest_completion_times);
    std::vector<Time> r(n);
    std::vector<Time> d(n);
    bool feasible = true;
    for (JobId job_id = 0; job_id < n; ++job_id) {
        if (earliest_completion_times[job_id] == -1) {
            // A job never processed: no feasible schedule.
            feasible = false;
            break;
        }
        r[job_id] = earliest_completion_times[job_id] - pp[job_id];
        d[job_id] = latest_completion_times[job_id];
    }

    // Consistency tests, until a fixed point.
    std::vector<JobId> members;
    std::vector<JobId> sorted;
    for (int iteration = 0; feasible && iteration < 16; ++iteration) {
        bool changed = false;

        // Immediate selection.
        for (JobId i = 0; i < n && feasible; ++i) {
            for (JobId j = 0; j < n; ++j) {
                if (i == j)
                    continue;
                bool j_before_i = (r[j] + pp[j] + pp[i] <= d[i]);
                bool i_before_j = (r[i] + pp[i] + pp[j] <= d[j]);
                if (!j_before_i && !i_before_j) {
                    feasible = false;
                    break;
                }
                if (!j_before_i) {
                    // i before j.
                    if (r[j] < r[i] + pp[i]) {
                        r[j] = r[i] + pp[i];
                        changed = true;
                    }
                    if (d[i] > d[j] - pp[j]) {
                        d[i] = d[j] - pp[j];
                        changed = true;
                    }
                }
            }
        }

        // Task intervals: edge-finding and not-first/not-last.
        for (JobId a = 0; a < n && feasible; ++a) {
            for (JobId b = 0; b < n && feasible; ++b) {
                if (r[a] > d[b])
                    continue;
                members.clear();
                for (JobId k = 0; k < n; ++k)
                    if (r[k] >= r[a] && d[k] <= d[b])
                        members.push_back(k);
                if (members.empty())
                    continue;
                Time r_set = std::numeric_limits<Time>::max();
                Time d_set = std::numeric_limits<Time>::min();
                Time p_set = 0;
                Time min_ect = std::numeric_limits<Time>::max();
                Time max_lst = std::numeric_limits<Time>::min();
                for (JobId k: members) {
                    r_set = (std::min)(r_set, r[k]);
                    d_set = (std::max)(d_set, d[k]);
                    p_set += pp[k];
                    min_ect = (std::min)(min_ect, r[k] + pp[k]);
                    max_lst = (std::max)(max_lst, d[k] - pp[k]);
                }
                if (r_set + p_set > d_set) {
                    feasible = false;
                    break;
                }
                // Earliest completion time of the set: max_k (r_k + sum of
                // the p of the members released at or after r_k); latest
                // start time: min_k (d_k - sum of the p of the members due
                // at or before d_k).
                sorted = members;
                std::sort(sorted.begin(), sorted.end(),
                        [&r](JobId k1, JobId k2) { return r[k1] > r[k2]; });
                Time ect = std::numeric_limits<Time>::min();
                Time sum = 0;
                for (JobId k: sorted) {
                    sum += pp[k];
                    ect = (std::max)(ect, r[k] + sum);
                }
                std::sort(sorted.begin(), sorted.end(),
                        [&d](JobId k1, JobId k2) { return d[k1] < d[k2]; });
                Time lst = std::numeric_limits<Time>::max();
                sum = 0;
                for (JobId k: sorted) {
                    sum += pp[k];
                    lst = (std::min)(lst, d[k] - sum);
                }
                size_t member_pos = 0;
                for (JobId i = 0; i < n; ++i) {
                    if (member_pos < members.size() && members[member_pos] == i) {
                        member_pos++;
                        continue;
                    }
                    Time r_i = r[i];
                    Time d_i = d[i];
                    // Edge-finding: i after (resp. before) all the jobs of
                    // the set.
                    if ((std::min)(r_set, r[i]) + p_set + pp[i] > d_set)
                        r_i = (std::max)(r_i, ect);
                    if ((std::max)(d_set, d[i]) - p_set - pp[i] < r_set)
                        d_i = (std::min)(d_i, lst);
                    // Not-first, not-last.
                    if (r[i] + pp[i] + p_set > d_set)
                        r_i = (std::max)(r_i, min_ect);
                    if (d[i] - pp[i] - p_set < r_set)
                        d_i = (std::min)(d_i, max_lst);
                    if (r_i != r[i] || d_i != d[i]) {
                        r[i] = r_i;
                        d[i] = d_i;
                        changed = true;
                    }
                }
            }
        }

        for (JobId i = 0; i < n; ++i) {
            if (r[i] + pp[i] > d[i]) {
                feasible = false;
                break;
            }
        }
        if (!changed)
            break;
    }

    // Remove the nodes outside the time windows, and direct elimination.
    bool removed = false;
    for (int64_t bucket = 0; bucket < (int64_t)bucket_starts_.size() - 1; ++bucket) {
        if (bucket_starts_[bucket] == bucket_starts_[bucket + 1])
            continue;
        JobId i = bucket % n;
        Time t = bucket / n;
        bool remove = !feasible;
        if (!remove)
            remove = (t < r[i] + pp[i] || t > d[i]);
        for (JobId j = 0; j < n && !remove; ++j) {
            if (j == i)
                continue;
            // If i precedes j, C_i <= d_j - p_j; otherwise, C_i >= r_j + p_j + p_i.
            if (t >= d[j] - pp[j] + 1 && t <= r[j] + pp[j] + pp[i] - 1)
                remove = true;
        }
        if (!remove)
            continue;
        removed = true;
        for (int64_t state_id = bucket_starts_[bucket];
                state_id < bucket_starts_[bucket + 1];
                ++state_id) {
            states_[state_id].alive = false;
        }
    }
    if (removed) {
        invalidate_labels();
        clean();
    }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Generation ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Network Network::initial_network(const std::shared_ptr<const Problem>& problem_pointer)
{
    const Problem& problem = *problem_pointer;
    Network network(problem_pointer);
    JobId n = problem.number_of_jobs;
    for (Time t = 0; t <= problem.maximum_makespan; ++t) {
        for (JobId job_id = 0; job_id < n; ++job_id) {
            int64_t bucket = network.bucket_id(job_id, t);
            network.bucket_starts_[bucket] = network.states_.size();
            Time p = problem.processing_times[job_id];
            if (t < p)
                continue;
            State state;
            if (t == problem.setup_time(-1, job_id) + p)
                state.predecessors.set(n);
            for (JobId job_id_prev = 0; job_id_prev < n; ++job_id_prev) {
                if (job_id_prev == job_id)
                    continue;
                Time t_prev = t - problem.setup_time(job_id_prev, job_id) - p;
                if (t_prev < 0)
                    continue;
                int64_t bucket_prev = network.bucket_id(job_id_prev, t_prev);
                if (network.bucket_starts_[bucket_prev]
                        != network.bucket_starts_[bucket_prev + 1]) {
                    state.predecessors.set(job_id_prev);
                }
            }
            if (state.predecessors.empty())
                continue;
            state.sink = (t >= problem.minimum_makespan);
            network.push_state(state, Labels(), Labels());
        }
    }
    network.bucket_starts_.back() = network.states_.size();
    network.clean();
    return network;
}

namespace
{

struct Candidate
{
    Mask mask;

    double value;

    JobId job_id;
};

}

bool Network::generate(
        Network& new_network,
        const Mask& modifiers,
        Direction direction,
        const std::vector<double>& multipliers,
        double sum_of_multipliers,
        double threshold,
        double maximum_memory,
        const optimizationtools::Timer* timer) const
{
    const Problem& problem = *problem_;
    JobId n = problem.number_of_jobs;
    Time t_max = problem.maximum_makespan;
    int64_t number_of_buckets = (int64_t)bucket_starts_.size() - 1;
    new_network.modifiers_ = modifiers;
    new_network.states_.clear();
    new_network.forward_labels_.clear();
    new_network.backward_labels_.clear();
    new_network.labels_direction_ = direction;
    new_network.invalidate_labels();
    new_network.restricted_ = restricted_;
    new_network.cutoff_ = (std::min)(cutoff_, threshold);
    std::vector<State>& new_states = new_network.states_;
    std::vector<Labels>& new_forward_labels = new_network.forward_labels_;
    std::vector<Labels>& new_backward_labels = new_network.backward_labels_;
    double old_memory = memory();

    // State of the current network corresponding to a state of the new
    // network.
    auto old_state = [this](JobId job_id, Time t, const Mask& mask)
    {
        return find(job_id, t, mask & modifiers_);
    };

    std::vector<Candidate> candidates;

    if (direction == Direction::Forward) {
        std::vector<int64_t>& starts = new_network.bucket_starts_;
        for (Time t = 0; t <= t_max; ++t) {
            if (timer != nullptr && timer->needs_to_end())
                return false;
            for (JobId job_id = 0; job_id < n; ++job_id) {
                int64_t bucket = bucket_id(job_id, t);
                starts[bucket] = new_states.size();
                Time p = problem.processing_times[job_id];
                if (t < p || bucket_starts_[bucket] == bucket_starts_[bucket + 1])
                    continue;
                double c = problem.cost(job_id, t) - multipliers[job_id];
                candidates.clear();

                // Arc from the source.
                if (t == problem.setup_time(-1, job_id) + p) {
                    Mask mask;
                    if (modifiers.get(job_id))
                        mask.set(job_id);
                    int64_t state_id_old = old_state(job_id, t, mask);
                    if (state_id_old != -1
                            && states_[state_id_old].predecessors.get(n)
                            && c + backward_labels_[state_id_old].best + sum_of_multipliers <= threshold) {
                        candidates.push_back({mask, c, n});
                    }
                }

                // Arcs from the states of the new network.
                for (JobId job_id_prev = 0; job_id_prev < n; ++job_id_prev) {
                    if (job_id_prev == job_id)
                        continue;
                    Time t_prev = t - problem.setup_time(job_id_prev, job_id) - p;
                    if (t_prev < 0)
                        continue;
                    int64_t bucket_prev = bucket_id(job_id_prev, t_prev);
                    for (int64_t state_id = starts[bucket_prev];
                            state_id < starts[bucket_prev + 1];
                            ++state_id) {
                        Mask mask = new_states[state_id].mask;
                        if (modifiers.get(job_id)) {
                            if (mask.get(job_id))
                                continue;
                            mask.set(job_id);
                        }
                        double value = new_forward_labels[state_id].excluding(job_id);
                        if (value == INF)
                            continue;
                        value += c;
                        int64_t state_id_old = old_state(job_id, t, mask);
                        if (state_id_old == -1
                                || !states_[state_id_old].predecessors.get(job_id_prev)) {
                            continue;
                        }
                        if (value
                                + backward_labels_[state_id_old].excluding(job_id_prev)
                                + sum_of_multipliers > threshold) {
                            continue;
                        }
                        candidates.push_back({mask, value, job_id_prev});
                    }
                }
                if (candidates.empty())
                    continue;

                // Create the states.
                std::sort(candidates.begin(), candidates.end(),
                        [](const Candidate& c1, const Candidate& c2) { return c1.mask < c2.mask; });
                for (size_t pos = 0; pos < candidates.size();) {
                    State state;
                    Labels labels;
                    state.mask = candidates[pos].mask;
                    for (; pos < candidates.size() && candidates[pos].mask == state.mask; ++pos) {
                        labels.add(candidates[pos].value, candidates[pos].job_id);
                        state.predecessors.set(candidates[pos].job_id);
                    }
                    // Arc to the sink: the jobs of M that must be scheduled
                    // are processed.
                    if (t >= problem.minimum_makespan
                            && state.mask.contains(modifiers & problem.mandatory_jobs)) {
                        int64_t state_id_old = old_state(job_id, t, state.mask);
                        if (state_id_old != -1
                                && states_[state_id_old].sink
                                && labels.best
                                + sink_cost(state.mask & modifiers_, multipliers)
                                + sum_of_multipliers <= threshold) {
                            state.sink = true;
                        }
                    }
                    new_network.push_state(state, labels, Labels());
                }
            }
            if (old_memory + new_network.memory() > maximum_memory)
                return false;
        }
        starts.back() = new_states.size();

    } else {
        // States are generated by decreasing time, job and mask, then the
        // vectors are reversed.
        std::vector<int64_t> starts(number_of_buckets, 0);
        std::vector<int64_t> sizes(number_of_buckets, 0);
        for (Time t = t_max; t >= 0; --t) {
            if (timer != nullptr && timer->needs_to_end())
                return false;
            for (JobId job_id = n - 1; job_id >= 0; --job_id) {
                int64_t bucket = bucket_id(job_id, t);
                starts[bucket] = new_states.size();
                Time p = problem.processing_times[job_id];
                if (t < p || bucket_starts_[bucket] == bucket_starts_[bucket + 1])
                    continue;
                double c = problem.cost(job_id, t) - multipliers[job_id];
                candidates.clear();

                // Arcs to the sink: for each state of the current network
                // with an arc to the sink, the masks of the new network with
                // the same jobs of the current M, and any subset of the new
                // jobs of M containing the jobs that must be scheduled (and
                // the job of the state).
                if (t >= problem.minimum_makespan) {
                    Mask new_jobs = modifiers.minus(modifiers_);
                    Mask required = new_jobs & problem.mandatory_jobs;
                    if (new_jobs.get(job_id))
                        required.set(job_id);
                    std::vector<JobId> optional_jobs;
                    new_jobs.minus(required).for_each([&optional_jobs](JobId j) { optional_jobs.push_back(j); });
                    for (int64_t state_id_old = bucket_starts_[bucket];
                            state_id_old < bucket_starts_[bucket + 1];
                            ++state_id_old) {
                        const State& state_old = states_[state_id_old];
                        if (!state_old.sink
                                || forward_labels_[state_id_old].best
                                + sink_cost(state_old.mask, multipliers)
                                + sum_of_multipliers > threshold) {
                            continue;
                        }
                        for (uint64_t subset = 0;
                                subset < ((uint64_t)1 << optional_jobs.size());
                                ++subset) {
                            Mask mask = state_old.mask | required;
                            for (size_t pos = 0; pos < optional_jobs.size(); ++pos)
                                if ((subset >> pos) & 1)
                                    mask.set(optional_jobs[pos]);
                            candidates.push_back({
                                    mask,
                                    new_network.sink_cost(mask, multipliers),
                                    n});
                        }
                    }
                }

                // Arcs to the states of the new network.
                for (JobId job_id_next = 0; job_id_next < n; ++job_id_next) {
                    if (job_id_next == job_id)
                        continue;
                    Time t_next = t
                        + problem.setup_time(job_id, job_id_next)
                        + problem.processing_times[job_id_next];
                    if (t_next > t_max)
                        continue;
                    double c_next = problem.cost(job_id_next, t_next) - multipliers[job_id_next];
                    int64_t bucket_next = bucket_id(job_id_next, t_next);
                    for (int64_t state_id = starts[bucket_next];
                            state_id < starts[bucket_next] + sizes[bucket_next];
                            ++state_id) {
                        Mask mask_next = new_states[state_id].mask;
                        Mask mask = mask_next;
                        if (modifiers.get(job_id_next))
                            mask.reset(job_id_next);
                        if (modifiers.get(job_id) && !mask.get(job_id))
                            continue;
                        double value = new_backward_labels[state_id].excluding(job_id);
                        if (value == INF)
                            continue;
                        value += c_next;
                        int64_t state_id_next_old = old_state(job_id_next, t_next, mask_next);
                        if (state_id_next_old == -1
                                || !states_[state_id_next_old].predecessors.get(job_id)) {
                            continue;
                        }
                        int64_t state_id_old = old_state(job_id, t, mask);
                        if (state_id_old == -1)
                            continue;
                        if (forward_labels_[state_id_old].excluding(job_id_next)
                                + value
                                + sum_of_multipliers > threshold) {
                            continue;
                        }
                        candidates.push_back({mask, value, job_id_next});
                        new_states[state_id].predecessors.set(job_id);
                    }
                }
                if (candidates.empty())
                    continue;

                // Create the states.
                std::sort(candidates.begin(), candidates.end(),
                        [](const Candidate& c1, const Candidate& c2) { return c1.mask > c2.mask; });
                Mask mask_first;
                if (modifiers.get(job_id))
                    mask_first.set(job_id);
                for (size_t pos = 0; pos < candidates.size();) {
                    State state;
                    Labels labels;
                    state.mask = candidates[pos].mask;
                    for (; pos < candidates.size() && candidates[pos].mask == state.mask; ++pos) {
                        if (candidates[pos].job_id == n)
                            state.sink = true;
                        labels.add(candidates[pos].value, candidates[pos].job_id);
                    }
                    if (t == problem.setup_time(-1, job_id) + p && state.mask == mask_first) {
                        int64_t state_id_old = old_state(job_id, t, state.mask);
                        if (state_id_old != -1
                                && states_[state_id_old].predecessors.get(n)
                                && c + labels.best + sum_of_multipliers <= threshold) {
                            state.predecessors.set(n);
                        }
                    }
                    new_network.push_state(state, Labels(), labels);
                }
                sizes[bucket] = new_states.size() - starts[bucket];
            }
            if (old_memory + new_network.memory() > maximum_memory)
                return false;
        }
        std::reverse(new_states.begin(), new_states.end());
        std::reverse(new_forward_labels.begin(), new_forward_labels.end());
        std::reverse(new_backward_labels.begin(), new_backward_labels.end());
        std::vector<int64_t>& new_starts = new_network.bucket_starts_;
        int64_t start = 0;
        for (int64_t bucket = 0; bucket < number_of_buckets; ++bucket) {
            new_starts[bucket] = start;
            start += sizes[bucket];
        }
        new_starts.back() = start;
    }

    new_network.clean();
    new_network.set_labels_up_to_date(direction, multipliers);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Problem /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/**
 * Check that the objective and the jobs of an instance are supported, with
 * the processing times of a machine.
 */
void check_instance(
        const Instance& instance,
        MachineId machine_id)
{
    if (instance.objective() != Objective::TotalTardiness
            && instance.objective() != Objective::TotalFlowTime) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "only the total tardiness and the total flow time are supported.");
    }
    for (JobId job_id = 0; job_id < instance.number_of_jobs(); ++job_id) {
        const Job& job = instance.job(job_id);
        if (job.release_date != 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "release dates are not supported.");
        }
        if (job.processing_times[machine_id] <= 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "processing times must be positive.");
        }
    }
}

/**
 * Extended processing times of a problem whose jobs and setup times are set,
 * and the smallest and largest total setup times of a sequence of all its
 * jobs, from the assignment relaxation of the asymmetric traveling salesman
 * problem of the setup times.
 */
void complete_problem(
        Problem& problem,
        Time& setups_min,
        Time& setups_max)
{
    JobId m = problem.number_of_jobs;
    problem.extended_processing_times.resize(m);
    for (JobId job_id = 0; job_id < m; ++job_id) {
        Time setup_min = problem.setup_time(-1, job_id);
        for (JobId job_id_1 = 0; job_id_1 < m; ++job_id_1)
            if (job_id_1 != job_id)
                setup_min = (std::min)(setup_min, problem.setup_time(job_id_1, job_id));
        problem.extended_processing_times[job_id] = problem.processing_times[job_id] + setup_min;
    }

    // Bounds of the makespan from the assignment relaxation of the
    // asymmetric traveling salesman problem, with a dummy node 0.
    const Time forbidden = 1e15;
    std::vector<std::vector<Time>> costs_min(m + 1, std::vector<Time>(m + 1, 0));
    std::vector<std::vector<Time>> costs_max(m + 1, std::vector<Time>(m + 1, 0));
    for (JobId a = 0; a <= m; ++a) {
        for (JobId b = 0; b <= m; ++b) {
            Time setup = 0;
            if (a == b) {
                costs_min[a][b] = forbidden;
                costs_max[a][b] = forbidden;
                continue;
            }
            if (b > 0)
                setup = problem.setup_time(a - 1, b - 1);
            costs_min[a][b] = setup;
            costs_max[a][b] = -setup;
        }
    }
    setups_min = 0;
    setups_max = 0;
    if (m > 0) {
        std::vector<int64_t> assignment_min = hungarian(costs_min, m + 1, m + 1);
        std::vector<int64_t> assignment_max = hungarian(costs_max, m + 1, m + 1);
        for (JobId a = 0; a <= m; ++a) {
            setups_min += costs_min[a][assignment_min[a]];
            setups_max -= costs_max[a][assignment_max[a]];
        }
    }
}

}

std::shared_ptr<const Problem> parallelschedulingsolver::time_indexed::build_problem(
        const Instance& instance,
        bool remove_zero_cost_jobs)
{
    if (instance.number_of_machines() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "only single machine instances are supported.");
    }
    check_instance(instance, 0);

    auto problem_pointer = std::make_shared<Problem>();
    Problem& problem = *problem_pointer;
    JobId n = instance.number_of_jobs();
    problem.tardiness = (instance.objective() == Objective::TotalTardiness);

    auto zero_cost = [&instance, &problem](JobId job_id)
    {
        const Job& job = instance.job(job_id);
        if (job.weight == 0)
            return true;
        return problem.tardiness && job.due_date < 0;
    };

    // Remove the zero cost jobs satisfying (40), one at a time.
    std::vector<bool> removed(n, false);
    for (bool changed = remove_zero_cost_jobs; changed;) {
        changed = false;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            if (removed[job_id] || !zero_cost(job_id))
                continue;
            Time p = instance.job(job_id).processing_times[0];
            bool ok = true;
            for (JobId job_id_1 = -1; job_id_1 < n && ok; ++job_id_1) {
                if (job_id_1 == job_id || (job_id_1 >= 0 && removed[job_id_1]))
                    continue;
                for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2) {
                    if (job_id_2 == job_id || job_id_2 == job_id_1 || removed[job_id_2])
                        continue;
                    if (instance.setup_time(0, job_id_1, job_id)
                            + p
                            + instance.setup_time(0, job_id, job_id_2)
                            < instance.setup_time(0, job_id_1, job_id_2)) {
                        ok = false;
                        break;
                    }
                }
            }
            if (ok) {
                removed[job_id] = true;
                problem.removed_job_ids.push_back(job_id);
                changed = true;
            }
        }
    }
    // The last removed job is scheduled first among the removed jobs.
    std::reverse(problem.removed_job_ids.begin(), problem.removed_job_ids.end());

    for (JobId job_id = 0; job_id < n; ++job_id) {
        if (removed[job_id])
            continue;
        const Job& job = instance.job(job_id);
        problem.instance_job_ids.push_back(job_id);
        problem.processing_times.push_back(job.processing_times[0]);
        problem.weights.push_back(job.weight);
        problem.due_dates.push_back(job.due_date);
        problem.rejection_costs.push_back(job.rejection_cost);
    }
    JobId m = problem.instance_job_ids.size();
    problem.number_of_jobs = m;
    if (m > 127) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "at most 127 jobs are supported; "
                "number of jobs: " + std::to_string(m) + ".");
    }
    for (JobId job_id = 0; job_id < m; ++job_id) {
        if (problem.rejection_costs[job_id] >= 0) {
            problem.has_rejection_costs = true;
        } else {
            problem.mandatory_jobs.set(job_id);
        }
    }
    problem.setup_times.assign((m + 1) * m, 0);
    for (JobId job_id_1 = -1; job_id_1 < m; ++job_id_1) {
        for (JobId job_id_2 = 0; job_id_2 < m; ++job_id_2) {
            if (job_id_1 == job_id_2)
                continue;
            problem.setup_times[(job_id_1 + 1) * m + job_id_2] = instance.setup_time(
                    0,
                    (job_id_1 == -1)? -1: problem.instance_job_ids[job_id_1],
                    problem.instance_job_ids[job_id_2]);
        }
    }
    Time setups_min = 0;
    Time setups_max = 0;
    complete_problem(problem, setups_min, setups_max);
    Time sum_of_processing_times = 0;
    for (Time p: problem.processing_times)
        sum_of_processing_times += p;
    // With rejection costs, the jobs may not all be scheduled.
    problem.minimum_makespan = (problem.has_rejection_costs)?
        0:
        sum_of_processing_times + setups_min;
    problem.maximum_makespan = sum_of_processing_times + setups_max;
    return problem_pointer;
}

bool parallelschedulingsolver::time_indexed::identical_machines(
        const Instance& instance,
        MachineId machine_id_1,
        MachineId machine_id_2)
{
    JobId n = instance.number_of_jobs();
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        if (job.processing_times[machine_id_1] != job.processing_times[machine_id_2])
            return false;
    }
    if (!instance.has_setup_times())
        return true;
    for (JobId job_id_1 = -1; job_id_1 < n; ++job_id_1) {
        for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2) {
            if (job_id_1 == job_id_2)
                continue;
            if (instance.setup_time(machine_id_1, job_id_1, job_id_2)
                    != instance.setup_time(machine_id_2, job_id_1, job_id_2)) {
                return false;
            }
        }
    }
    return true;
}

std::shared_ptr<const Problem> parallelschedulingsolver::time_indexed::build_machine_problem(
        const Instance& instance,
        MachineId machine_id)
{
    if (machine_id < 0 || machine_id >= instance.number_of_machines()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid machine; "
                "machine_id: " + std::to_string(machine_id) + ".");
    }
    check_instance(instance, machine_id);

    auto problem_pointer = std::make_shared<Problem>();
    Problem& problem = *problem_pointer;
    JobId n = instance.number_of_jobs();
    if (n > 127) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "at most 127 jobs are supported; "
                "number of jobs: " + std::to_string(n) + ".");
    }
    problem.tardiness = (instance.objective() == Objective::TotalTardiness);
    problem.machine_subproblem = true;
    problem.number_of_jobs = n;
    for (JobId job_id = 0; job_id < n; ++job_id) {
        const Job& job = instance.job(job_id);
        problem.instance_job_ids.push_back(job_id);
        problem.processing_times.push_back(job.processing_times[machine_id]);
        problem.weights.push_back(job.weight);
        problem.due_dates.push_back(job.due_date);
        // Any job may be processed on another machine: it is optional, and
        // its cost is in the relaxation of the instance.
        problem.rejection_costs.push_back(0);
    }
    problem.has_rejection_costs = (n > 0);
    problem.setup_times.assign((n + 1) * n, 0);
    for (JobId job_id_1 = -1; job_id_1 < n; ++job_id_1) {
        for (JobId job_id_2 = 0; job_id_2 < n; ++job_id_2) {
            if (job_id_1 == job_id_2)
                continue;
            problem.setup_times[(job_id_1 + 1) * n + job_id_2]
                = instance.setup_time(machine_id, job_id_1, job_id_2);
        }
    }
    Time setups_min = 0;
    Time setups_max = 0;
    complete_problem(problem, setups_min, setups_max);
    Time sum_of_processing_times = 0;
    for (Time p: problem.processing_times)
        sum_of_processing_times += p;
    problem.minimum_makespan = 0;
    problem.maximum_makespan = sum_of_processing_times + setups_max;

    // Identical machines: in an optimal schedule minimizing the sum of the
    // completion times among the optimal schedules, the last job j of a
    // machine a can't be moved to the end of another machine b, so
    // C_a <= C_b + q with q = max_j (p_j + largest setup time before j).
    // With L the sum over the jobs of p_j + largest setup time before j,
    // sum_b C_b <= L, hence C_a <= (L + (m - 1) q) / m.
    MachineId m = instance.number_of_machines();
    bool identical = true;
    for (MachineId machine_id_2 = 0; machine_id_2 < m && identical; ++machine_id_2)
        if (machine_id_2 != machine_id && !identical_machines(instance, machine_id, machine_id_2))
            identical = false;
    if (m > 1 && identical && n > 0) {
        Time total = 0;
        Time q = 0;
        for (JobId job_id = 0; job_id < n; ++job_id) {
            Time setup_max = problem.setup_time(-1, job_id);
            for (JobId job_id_1 = 0; job_id_1 < n; ++job_id_1)
                if (job_id_1 != job_id)
                    setup_max = (std::max)(setup_max, problem.setup_time(job_id_1, job_id));
            Time extended = problem.processing_times[job_id] + setup_max;
            total += extended;
            q = (std::max)(q, extended);
        }
        problem.maximum_makespan = (std::min)(
                problem.maximum_makespan,
                (total + (m - 1) * q) / m);
    }
    return problem_pointer;
}

Solution parallelschedulingsolver::time_indexed::build_parallel_solution(
        const Instance& instance,
        const std::vector<std::vector<JobId>>& machine_sequences)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    for (MachineId machine_id = 0;
            machine_id < (MachineId)machine_sequences.size();
            ++machine_id) {
        Time t = 0;
        JobId job_id_prev = -1;
        for (JobId job_id: machine_sequences[machine_id]) {
            Time start = t + instance.setup_time(machine_id, job_id_prev, job_id);
            solution_builder.append_job_to_machine(machine_id, job_id, start);
            t = start + instance.job(job_id).processing_times[machine_id];
            job_id_prev = job_id;
        }
    }
    return solution_builder.build();
}

Solution parallelschedulingsolver::time_indexed::build_solution(
        const Instance& instance,
        const Problem& problem,
        const std::vector<JobId>& sequence)
{
    SolutionBuilder solution_builder;
    solution_builder.set_instance(instance);
    Time t = 0;
    JobId job_id_prev = -1;
    auto append = [&](JobId job_id)
    {
        Time start = t + instance.setup_time(0, job_id_prev, job_id);
        solution_builder.append_job_to_machine(0, job_id, start);
        t = start + instance.job(job_id).processing_times[0];
        job_id_prev = job_id;
    };
    for (JobId job_id: sequence)
        append(problem.instance_job_ids[job_id]);
    for (JobId job_id: problem.removed_job_ids)
        append(job_id);
    return solution_builder.build();
}
