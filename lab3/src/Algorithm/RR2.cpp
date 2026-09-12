#include "common.h"
#include <deque>

// ---------------------------------------------------------------------------
// rr2(): Round Robin scheduler — TWO CPUs.
//
// Strategy:
//   - Both CPUs share one global Ready deque (FIFO ordering within the deque).
//   - Each CPU can independently be running a different process.
//   - We simulate event-by-event: after each quantum on any CPU, we check
//     for new arrivals, I/O completions, and preemptions.
//   - Tie-breaking: when both CPUs finish at the same time, lower CPU-ID
//     picks from Ready first.
// ---------------------------------------------------------------------------
void rr2(const string& filename, int quantum) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    const int NCPU = 2;
    int cpu_finish[NCPU]     = {0, 0};   // time when CPU c becomes free
    int cpu_proc[NCPU]       = {-1, -1}; // index of process on CPU c (-1=idle)
    int cpu_ran_for[NCPU]    = {0, 0};   // how long the current slice is

    deque<int> Ready;
    vector<int> Blocked;

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;
    int current_time      = 0;

    // Admit processes arriving at time 0.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Ready.push_back(next_arrival_idx++);
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

        // --- Release CPUs that have finished their quantum by current_time ---
        for (int c = 0; c < NCPU; ++c) {
            if (cpu_proc[c] != -1 && cpu_finish[c] <= current_time) {
                int cur    = cpu_proc[c];
                int run_for = cpu_ran_for[c];
                procs[cur].remaining_cpu -= run_for;

                if (procs[cur].remaining_cpu == 0) {
                    // CPU burst complete
                    procs[cur].burst_idx++;
                    if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                        // Next is I/O
                        int io_len = procs[cur].bursts[procs[cur].burst_idx];
                        procs[cur].io_completion_time = cpu_finish[c] + io_len;
                        procs[cur].burst_idx++;
                        if (procs[cur].burst_idx < (int)procs[cur].bursts.size())
                            procs[cur].remaining_cpu = procs[cur].bursts[procs[cur].burst_idx];
                        Blocked.push_back(cur);
                    } else {
                        procs[cur].finish_time = cpu_finish[c];
                        completed++;
                    }
                } else {
                    // Preempted — re-queue at back of Ready.
                    Ready.push_back(cur);
                }

                cpu_proc[c]  = -1;
                cpu_finish[c] = current_time;
            }
        }

        // --- Move I/O-completed processes back to Ready ---
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
        for (int idx : returning) Ready.push_back(idx);

        // --- Admit newly arrived processes ---
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Ready.push_back(next_arrival_idx++);
        }

        // --- Assign ready processes to idle CPUs ---
        for (int c = 0; c < NCPU && !Ready.empty(); ++c) {
            if (cpu_proc[c] == -1) {
                int cur = Ready.front();
                Ready.pop_front();

                int run_for    = min(procs[cur].remaining_cpu, quantum);
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
        }

        // Check if anything is happening at all.
        bool any_busy = false;
        for (int c = 0; c < NCPU; ++c)
            if (cpu_proc[c] != -1) any_busy = true;

        if (!any_busy && Ready.empty() && completed < n) {
            int t = nextEventTime();
            if (t != INT_MAX) current_time = t;
            continue;
        }

        // Advance time to the nearest CPU finish / arrival / I/O event.
        if (any_busy) {
            int t = nextEventTime();
            if (t > current_time) current_time = t;
        }
    }

    writeOutput(schedule, procs, total_cpu_runtime, NCPU);
}
