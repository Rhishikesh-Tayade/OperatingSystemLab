#include "common.h"
#include <deque>

// ---------------------------------------------------------------------------
// mlfq2(): Multi-Level Feedback Queue scheduler — TWO CPUs.
//
// Rules (same as single-CPU MLFQ, now with two independent CPUs sharing
// the same three priority queues):
//   - 3 queues: Q0 (highest), Q1, Q2. All use quantum = 2.
//   - New processes always enter Q0.
//   - Quantum exhausted → demoted to next lower queue.
//   - Voluntary CPU yield (I/O) → stays at same queue level.
//   - Optional priority boost every boost_interval units.
//   - Both CPUs always pick from the highest non-empty queue.
// ---------------------------------------------------------------------------
void mlfq2(const string& filename, int boost_interval = 0) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    const int NCPU   = 2;
    const int QUANTUM = 2;

    int cpu_finish[NCPU]  = {0, 0};
    int cpu_proc[NCPU]    = {-1, -1};
    int cpu_ran_for[NCPU] = {0, 0};

    deque<int> Q[3]; // Q[0]=highest priority
    vector<int> Blocked;

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;
    int current_time      = 0;
    int last_boost_time   = 0;

    // Admit processes arriving at time 0 into Q0.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Q[0].push_back(next_arrival_idx++);
    }

    auto nextEventTime = [&]() -> int {
        int t = INT_MAX;
        for (int c = 0; c < NCPU; ++c)
            if (cpu_proc[c] != -1)
                t = min(t, cpu_finish[c]);
        if (next_arrival_idx < n)
            t = min(t, procs[next_arrival_idx].arrival_time);
        for (int idx : Blocked)
            t = min(t, procs[idx].io_completion_time);
        return t;
    };

    while (completed < n) {

        // --- Priority Boost ---
        if (boost_interval > 0 && current_time > 0 &&
            (current_time - last_boost_time) >= boost_interval) {
            last_boost_time = current_time;
            for (int lv = 1; lv <= 2; ++lv) {
                for (int idx : Q[lv]) {
                    procs[idx].queue_level = 0;
                    Q[0].push_back(idx);
                }
                Q[lv].clear();
            }
            for (int idx : Blocked)
                procs[idx].queue_level = 0;
        }

        // --- Release CPUs whose quantum has expired ---
        for (int c = 0; c < NCPU; ++c) {
            if (cpu_proc[c] != -1 && cpu_finish[c] <= current_time) {
                int cur     = cpu_proc[c];
                int run_for = cpu_ran_for[c];
                procs[cur].remaining_cpu -= run_for;

                if (procs[cur].remaining_cpu == 0) {
                    procs[cur].burst_idx++;
                    if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                        int io_len = procs[cur].bursts[procs[cur].burst_idx];
                        procs[cur].io_completion_time = cpu_finish[c] + io_len;
                        procs[cur].burst_idx++;
                        if (procs[cur].burst_idx < (int)procs[cur].bursts.size())
                            procs[cur].remaining_cpu = procs[cur].bursts[procs[cur].burst_idx];
                        // Voluntary yield — stays at same queue level.
                        Blocked.push_back(cur);
                    } else {
                        procs[cur].finish_time = cpu_finish[c];
                        completed++;
                    }
                } else {
                    // Quantum exhausted — demote.
                    if (procs[cur].queue_level < 2)
                        procs[cur].queue_level++;
                    Q[procs[cur].queue_level].push_back(cur);
                }

                cpu_proc[c]  = -1;
                cpu_finish[c] = current_time;
            }
        }

        // --- Move I/O-completed processes back to their queue ---
        vector<int> returning;
        for (auto it = Blocked.begin(); it != Blocked.end(); ) {
            int idx = *it;
            if (procs[idx].io_completion_time <= current_time) {
                returning.push_back(idx);
                it = Blocked.erase(it);
            } else { ++it; }
        }
        sort(returning.begin(), returning.end(),
             [&](int a, int b){ return procs[a].pid < procs[b].pid; });
        for (int idx : returning)
            Q[procs[idx].queue_level].push_back(idx);

        // --- Admit newly arrived processes into Q0 ---
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Q[0].push_back(next_arrival_idx++);
        }

        // --- Assign processes to idle CPUs from highest-priority queue ---
        for (int c = 0; c < NCPU; ++c) {
            if (cpu_proc[c] != -1) continue; // CPU busy

            int chosen_level = -1;
            for (int lv = 0; lv <= 2; ++lv) {
                if (!Q[lv].empty()) { chosen_level = lv; break; }
            }
            if (chosen_level == -1) break; // nothing to schedule

            int cur = Q[chosen_level].front();
            Q[chosen_level].pop_front();

            int run_for    = min(procs[cur].remaining_cpu, QUANTUM);
            int start_time = current_time;
            int end_time   = current_time + run_for;

            procs[cur].cpu_instance++;
            string label = "P" + to_string(procs[cur].pid) + "," +
                           to_string(procs[cur].cpu_instance);
            schedule.push_back({label, c, start_time, end_time});

            total_cpu_runtime += run_for;
            cpu_proc[c]    = cur;
            cpu_finish[c]  = end_time;
            cpu_ran_for[c] = run_for;
        }

        bool any_busy = false;
        for (int c = 0; c < NCPU; ++c)
            if (cpu_proc[c] != -1) any_busy = true;

        if (!any_busy && completed < n) {
            int t = nextEventTime();
            if (t != INT_MAX) current_time = t;
            continue;
        }

        if (any_busy) {
            int t = nextEventTime();
            if (t > current_time) current_time = t;
        }
    }

    writeOutput(schedule, procs, total_cpu_runtime, NCPU);
}
