#include "common.h"
#include <queue>

// ---------------------------------------------------------------------------
// fifo2(): First-In First-Out scheduler — TWO CPUs.
//
// Strategy:
//   - Both CPUs share one global Ready queue.
//   - At each event point, we assign ready processes to idle CPUs (lowest
//     CPU-ID first for determinism).
//   - We advance time by simulating which CPU finishes next (event-driven).
//   - I/O is still handled by a global Blocked list.
// ---------------------------------------------------------------------------
void fifo2(const string& filename) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    // cpu_finish[c] = time when CPU c becomes free; -1 = idle.
    // cpu_proc[c]   = index of process currently on CPU c; -1 = idle.
    const int NCPU = 2;
    int cpu_finish[NCPU] = {0, 0};
    int cpu_proc[NCPU]   = {-1, -1};

    queue<int> Ready;
    vector<int> Blocked;

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;
    int current_time      = 0;

    // Admit processes arriving at time 0.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Ready.push(next_arrival_idx++);
    }

    // Helper: advance time to the next significant event.
    auto nextEventTime = [&]() -> int {
        int t = INT_MAX;
        // When does the earliest busy CPU finish?
        for (int c = 0; c < NCPU; ++c)
            if (cpu_proc[c] != -1)
                t = min(t, cpu_finish[c]);
        // When does the next process arrive?
        if (next_arrival_idx < n)
            t = min(t, procs[next_arrival_idx].arrival_time);
        // When does the earliest I/O finish?
        for (int idx : Blocked)
            t = min(t, procs[idx].io_completion_time);
        return t;
    };

    while (completed < n) {

        // --- Release CPUs that have finished by current_time ---
        for (int c = 0; c < NCPU; ++c) {
            if (cpu_proc[c] != -1 && cpu_finish[c] <= current_time) {
                int cur = cpu_proc[c];

                if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
                    // Next burst is I/O
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

                cpu_proc[c]   = -1;
                cpu_finish[c] = current_time;
            }
        }

        // --- Move completed I/O processes back to Ready (sorted by PID) ---
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
        for (int idx : returning) Ready.push(idx);

        // --- Admit newly arrived processes ---
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Ready.push(next_arrival_idx++);
        }

        // --- Assign ready processes to idle CPUs ---
        for (int c = 0; c < NCPU && !Ready.empty(); ++c) {
            if (cpu_proc[c] == -1) {
                int cur = Ready.front();
                Ready.pop();

                procs[cur].cpu_instance++;
                int burst_len  = procs[cur].bursts[procs[cur].burst_idx];
                int start_time = current_time;
                int end_time   = current_time + burst_len;

                string label = "P" + to_string(procs[cur].pid) + "," +
                               to_string(procs[cur].cpu_instance);
                schedule.push_back({label, c, start_time, end_time});

                total_cpu_runtime += burst_len;
                procs[cur].burst_idx++;
                cpu_proc[c]   = cur;
                cpu_finish[c] = end_time;
            }
        }

        // --- If no CPU is busy and nothing is ready, something is wrong ---
        bool any_busy = false;
        for (int c = 0; c < NCPU; ++c)
            if (cpu_proc[c] != -1) any_busy = true;

        if (!any_busy && Ready.empty() && completed < n) {
            // All processes are in Blocked or not yet arrived — jump forward.
            int t = nextEventTime();
            if (t != INT_MAX) current_time = t;
            continue;
        }

        // Advance time to the next event (nearest CPU finishing).
        if (any_busy) {
            int t = nextEventTime();
            if (t > current_time) current_time = t;
        }
    }

    writeOutput(schedule, procs, total_cpu_runtime, NCPU);
}
