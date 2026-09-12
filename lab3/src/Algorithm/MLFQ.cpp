#include "common.h"
#include <deque>

// ---------------------------------------------------------------------------
// mlfq(): Multi-Level Feedback Queue scheduler — single CPU.
//
// Rules:
//   - 3 queues: Q0 (highest priority), Q1, Q2 (lowest). All use quantum = 2.
//   - New processes always start in Q0.
//   - If a process uses its full quantum, it is demoted to the next lower queue.
//   - If a process voluntarily yields the CPU (I/O burst), it stays at the
//     same queue level when it returns to Ready.
//   - Q0 always preempts Q1/Q2: we always pick from the highest non-empty queue.
//   - Priority boost (optional): every 'boost_interval' time units, all
//     processes in Q1 and Q2 are moved to Q0. Set boost_interval=0 to disable.
// ---------------------------------------------------------------------------
void mlfq(const string& filename, int boost_interval = 0) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    // Three ready queues, indexed by priority level (0 = highest).
    deque<int> Q[3];
    vector<int> Blocked;

    const int QUANTUM = 2;

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;
    int current_time      = 0;
    int last_boost_time   = 0; // tracks when the last priority boost happened

    // Admit all processes arriving at time 0 into Q0.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Q[0].push_back(next_arrival_idx++);
    }

    while (completed < n) {

        // --- Priority Boost (if enabled) ---
        // Every boost_interval units, reset all Q1/Q2 processes back to Q0.
        if (boost_interval > 0 && current_time > 0 &&
            (current_time - last_boost_time) >= boost_interval) {
            last_boost_time = current_time;
            for (int level = 1; level <= 2; ++level) {
                for (int idx : Q[level]) {
                    procs[idx].queue_level = 0;
                    Q[0].push_back(idx);
                }
                Q[level].clear();
            }
            // Also boost blocked processes so they return to Q0.
            for (int idx : Blocked) {
                procs[idx].queue_level = 0;
            }
        }

        // --- Move I/O-completed processes back to their queue level ---
        vector<int> returning;
        for (auto it = Blocked.begin(); it != Blocked.end(); ) {
            int idx = *it;
            if (procs[idx].io_completion_time <= current_time) {
                returning.push_back(idx);
                it = Blocked.erase(it);
            } else {
                ++it;
            }
        }
        sort(returning.begin(), returning.end(),
             [&](int a, int b){ return procs[a].pid < procs[b].pid; });
        for (int idx : returning) {
            int lv = procs[idx].queue_level;
            Q[lv].push_back(idx);
        }

        // --- Admit newly arrived processes into Q0 ---
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Q[0].push_back(next_arrival_idx++);
        }

        // --- Find the highest priority non-empty queue ---
        int chosen_level = -1;
        for (int lv = 0; lv <= 2; ++lv) {
            if (!Q[lv].empty()) { chosen_level = lv; break; }
        }

        // CPU idle — fast-forward to next event.
        if (chosen_level == -1) {
            int next_event = INT_MAX;
            if (next_arrival_idx < n)
                next_event = min(next_event, procs[next_arrival_idx].arrival_time);
            for (int idx : Blocked)
                next_event = min(next_event, procs[idx].io_completion_time);
            if (next_event != INT_MAX)
                current_time = next_event;
            continue;
        }

        // --- Dequeue from the highest-priority non-empty queue ---
        int cur = Q[chosen_level].front();
        Q[chosen_level].pop_front();

        // This process runs for at most QUANTUM time units.
        int run_for    = min(procs[cur].remaining_cpu, QUANTUM);
        int start_time = current_time;
        int end_time   = current_time + run_for;

        // Check for new arrivals / I/O completions during this slice
        // so they can enter queues before the preempted process.
        vector<int> arrivals_during;
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time < end_time) {
            arrivals_during.push_back(next_arrival_idx++);
        }

        vector<int> io_done_during;
        for (auto it = Blocked.begin(); it != Blocked.end(); ) {
            int idx = *it;
            if (procs[idx].io_completion_time <= end_time) {
                io_done_during.push_back(idx);
                it = Blocked.erase(it);
            } else {
                ++it;
            }
        }
        sort(io_done_during.begin(), io_done_during.end(),
             [&](int a, int b){ return procs[a].pid < procs[b].pid; });

        procs[cur].cpu_instance++;
        string label = "P" + to_string(procs[cur].pid) + "," +
                       to_string(procs[cur].cpu_instance);
        schedule.push_back({label, 0, start_time, end_time});

        total_cpu_runtime += run_for;
        current_time = end_time;
        procs[cur].remaining_cpu -= run_for;

        // Push arrivals and I/O completions into their queues.
        for (int idx : io_done_during) {
            Q[procs[idx].queue_level].push_back(idx);
        }
        for (int idx : arrivals_during) {
            Q[0].push_back(idx); // new arrivals always start at Q0
        }

        if (procs[cur].remaining_cpu == 0) {
            // CPU burst complete.
            procs[cur].burst_idx++;

            if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                // Next is I/O — process stays at the same queue level when it returns.
                int io_len = procs[cur].bursts[procs[cur].burst_idx];
                procs[cur].io_completion_time = current_time + io_len;
                procs[cur].burst_idx++;

                // Prime remaining_cpu for the next CPU burst.
                if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                    procs[cur].remaining_cpu = procs[cur].bursts[procs[cur].burst_idx];
                }
                // queue_level stays the same (voluntary yield — no demotion)
                Blocked.push_back(cur);
            } else {
                // Process is fully done.
                procs[cur].finish_time = current_time;
                completed++;
            }
        } else {
            // Process used its full quantum — demote to next lower queue.
            if (procs[cur].queue_level < 2)
                procs[cur].queue_level++;
            Q[procs[cur].queue_level].push_back(cur);
        }
    }

    writeOutput(schedule, procs, total_cpu_runtime, 1);
}
