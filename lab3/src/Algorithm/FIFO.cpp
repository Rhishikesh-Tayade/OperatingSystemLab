#include "common.h"
#include <queue>

// ---------------------------------------------------------------------------
// fifo(): First-In First-Out (non-preemptive) scheduler — single CPU.
//
// Strategy:
//   - A Ready queue (FIFO order) holds processes waiting for the CPU.
//   - A Blocked list holds processes waiting for I/O; each stores the
//     simulated time when its I/O will finish so they can complete
//     independently and in any order (unlike a simple FIFO queue of I/O jobs).
//   - The CPU runs one process at a time to completion of its current CPU
//     burst, then either puts it in Blocked (if more bursts remain) or
//     marks it done.
//   - I/O runs in parallel: while the CPU is running a process, blocked
//     processes may finish their I/O and re-enter Ready.
// ---------------------------------------------------------------------------
void fifo(const string& filename) {

    vector<ProcessInfo> procs = loadProcesses(filename);
    int n = (int)procs.size();

    // Ready: queue of indices into procs[], in arrival/FIFO order.
    // Blocked: list of indices of processes currently doing I/O.
    queue<int> Ready;
    vector<int> Blocked;

    // Design note: We use two separate data structures:
    //   - Ready  : a queue of process indices waiting for the CPU (FIFO order).
    //   - Blocked: a list of process indices currently doing I/O.
    // Using a separate Blocked list lets us track each process's I/O finish time
    // independently. This is important because I/O bursts finish at different times
    // (e.g. P1 needs 10s of I/O while P2 only needs 2s), so we cannot simply push
    // I/O processes to the back of the Ready queue and hope they come out in the
    // right order.
    //
    // Input format reminder:
    //   <arrival_time> <cpu_burst_1> <io_burst_1> <cpu_burst_2> <io_burst_2> ... -1
    //   e.g. P1: 0 10 2 10 -1  means arrival=0, CPU=10, I/O=2, CPU=10, then done.

    vector<ScheduleEntry> schedule;
    int total_cpu_runtime = 0;
    int completed         = 0;
    int next_arrival_idx  = 0;

    // 'current_time' is our simulated clock. It advances as processes run on the CPU.
    int current_time = 0;

    // Before entering the main loop, push any processes that have already
    // arrived at time 0 into the Ready queue.
    while (next_arrival_idx < n &&
           procs[next_arrival_idx].arrival_time <= current_time) {
        Ready.push(next_arrival_idx++);
    }

    // Main simulation loop: keep running until every process has finished.
    // The loop exits once 'completed' equals the total number of processes.
    while (completed < n) {

        // Move any blocked processes whose I/O has finished back into the Ready queue.
        // Tie-break: lower PID enters Ready first (sort before pushing).
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

        // Also admit any processes that have newly arrived.
        while (next_arrival_idx < n &&
               procs[next_arrival_idx].arrival_time <= current_time) {
            Ready.push(next_arrival_idx++);
        }

        // CPU is idle: no process is ready right now.
        // Fast-forward the clock to the next interesting event so we don't spin.
        // The next event is either the earliest I/O completion or the next process arrival,
        // whichever comes first.
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

        // Pick the process at the front of the Ready queue (FIFO: first in, first out).
        // Increment its CPU instance counter so the output label is P<pid>,<instance>
        // (e.g., P1,1 for the first time P1 runs, P1,2 for the second time, etc.).
        // Run it to completion of its current CPU burst (non-preemptive).
        int cur = Ready.front();
        Ready.pop();

        procs[cur].cpu_instance++;
        int burst_len  = procs[cur].bursts[procs[cur].burst_idx];
        int start_time = current_time;
        int end_time   = current_time + burst_len;

        string label = "P" + to_string(procs[cur].pid) + "," +
                       to_string(procs[cur].cpu_instance);
        schedule.push_back({label, 0, start_time, end_time});

        total_cpu_runtime += burst_len;
        current_time = end_time;
        procs[cur].burst_idx++;

        // After a CPU burst, check what comes next for this process:
        //   - If there are more bursts remaining, the next one is always an I/O burst
        //     (the format alternates: CPU, I/O, CPU, I/O, ..., -1).
        //     Compute when its I/O will finish and move it to the Blocked list.
        //     The CPU immediately picks the next process from Ready; I/O runs in parallel.
        //   - If no bursts remain, the process is finished.
        if (procs[cur].burst_idx < (int)procs[cur].bursts.size()) {
            // Next burst is I/O
            int io_len = procs[cur].bursts[procs[cur].burst_idx];
            procs[cur].io_completion_time = current_time + io_len;
            procs[cur].burst_idx++;
            Blocked.push_back(cur);
        } else {
            // Process has finished all bursts
            procs[cur].finish_time = current_time;
            completed++;
        }

        // End of one scheduling cycle. Loop back to check for I/O completions,
        // new arrivals, and schedule the next ready process.
    }

    // Write the schedule to scheduler.txt and also print it to the terminal.
    // Format per line: P<pid>,<instance>  <start_time>  <end_time>
    writeOutput(schedule, procs, total_cpu_runtime, 1);
}
