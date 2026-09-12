#include "common.h"
#include <queue>
#include <deque>

// ---------------------------------------------------------------------------
// rr(): Round Robin scheduler — single CPU.
//
// Strategy:
//   - Processes run for at most 'quantum' time units per turn.
//   - If a process uses its full quantum without finishing its CPU burst,
//     it is preempted and re-queued at the back of Ready.
//   - If a process voluntarily yields (I/O), it goes to Blocked and returns
//     to the back of Ready when I/O finishes.
//   - After each quantum expires: first check if new arrivals/I/O completions
//     happened during that slice (they enter Ready before the preempted process
//     to follow standard RR semantics where newly ready processes go ahead of
//     the preempted one if they arrived during the quantum).
// ---------------------------------------------------------------------------
void rr(const string& filename, int quantum) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    // Ready queue: deque so we can push to both front and back if needed.
    deque<int> Ready;
    vector<int> Blocked;

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;
    int current_time      = 0;

    // Admit all processes that arrive at time 0 before the loop starts.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Ready.push_back(next_arrival_idx++);
    }

    while (completed < n) {

        // Move blocked processes whose I/O is done back to Ready.
        // Tie-break by lower PID.
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
        for (int idx : returning) Ready.push_back(idx);

        // Admit newly arrived processes.
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Ready.push_back(next_arrival_idx++);
        }

        // CPU is idle — fast-forward to the next event.
        if (Ready.empty()) {
            int next_event = INT_MAX;
            if (next_arrival_idx < n)
                next_event = min(next_event, procs[next_arrival_idx].arrival_time);
            for (int idx : Blocked)
                next_event = min(next_event, procs[idx].io_completion_time);
            if (next_event != INT_MAX)
                current_time = next_event;
            continue;
        }

        // Pick next process from front of Ready queue.
        int cur = Ready.front();
        Ready.pop_front();

        // How long does this process run? The lesser of remaining CPU burst
        // and the quantum.
        int run_for    = min(procs[cur].remaining_cpu, quantum);
        int start_time = current_time;
        int end_time   = current_time + run_for;

        // Check if any I/O completions or new arrivals happen during this slice.
        // Those processes should enter Ready before the preempted process
        // (standard RR: processes that become ready during a quantum are queued
        // ahead of the preempted process).
        vector<int> arrivals_during;
        vector<int> io_done_during;

        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time < end_time) {
            arrivals_during.push_back(next_arrival_idx++);
        }
        for (auto it = Blocked.begin(); it != Blocked.end(); ) {
            int idx = *it;
            if (procs[idx].io_completion_time <= end_time) {
                io_done_during.push_back(idx);
                it = Blocked.erase(it);
            } else {
                ++it;
            }
        }
        // Sort I/O completions by lower PID for tie-breaking.
        sort(io_done_during.begin(), io_done_during.end(),
             [&](int a, int b){ return procs[a].pid < procs[b].pid; });

        procs[cur].cpu_instance++;
        string label = "P" + to_string(procs[cur].pid) + "," +
                       to_string(procs[cur].cpu_instance);
        schedule.push_back({label, 0, start_time, end_time});

        total_cpu_runtime += run_for;
        current_time = end_time;
        procs[cur].remaining_cpu -= run_for;

        // Push processes that became ready during this slice (in order: I/O completions,
        // then new arrivals — both by lower PID), then the preempted process if not done.
        for (int idx : io_done_during) Ready.push_back(idx);
        for (int idx : arrivals_during) Ready.push_back(idx);

        if (procs[cur].remaining_cpu == 0) {
            // This CPU burst is complete — advance burst_idx.
            procs[cur].burst_idx++;

            if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                // Next is an I/O burst.
                int io_len = procs[cur].bursts[procs[cur].burst_idx];
                procs[cur].io_completion_time = current_time + io_len;
                procs[cur].burst_idx++;

                // Prime remaining_cpu for the next CPU burst if it exists.
                if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                    procs[cur].remaining_cpu = procs[cur].bursts[procs[cur].burst_idx];
                }
                Blocked.push_back(cur);
            } else {
                // Process is fully done.
                procs[cur].finish_time = current_time;
                completed++;
            }
        } else {
            // Still has CPU time remaining — preempt and re-queue at the back.
            Ready.push_back(cur);
        }
    }

    writeOutput(schedule, procs, total_cpu_runtime, 1);
}
