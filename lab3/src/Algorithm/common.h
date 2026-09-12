#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <climits>
#include <iomanip>
using namespace std;

// ---------------------------------------------------------------------------
// ProcessInfo: holds both the static description from the workload file
// and the runtime state (where in the burst list we are, etc.)
// ---------------------------------------------------------------------------
struct ProcessInfo {
    int pid;                    // 1-indexed
    int arrival_time;
    vector<int> bursts;         // alternating: cpu, io, cpu, io, ... (no trailing -1)
    int burst_idx    = 0;       // index into bursts[] — even = CPU, odd = I/O
    int cpu_instance = 0;       // how many times this process has been scheduled on CPU
    int io_completion_time = -1;// simulated-clock time when current I/O finishes
    int finish_time  = 0;       // simulated-clock time when process fully completes
    int queue_level  = 0;       // used by MLFQ: 0=Q0 (highest), 1=Q1, 2=Q2
    int remaining_cpu = 0;      // used by RR/MLFQ: remaining time in current CPU burst
};

// ---------------------------------------------------------------------------
// ScheduleEntry: one row in the output schedule
// ---------------------------------------------------------------------------
struct ScheduleEntry {
    string name;   // e.g. "P1,2"
    int cpu_id;    // 0 or 1 (for 2-CPU variants)
    int start;
    int end;
};

// ---------------------------------------------------------------------------
// loadProcesses(): reads the workload file and returns a vector of ProcessInfo.
// Each row format: <arrival> <cpu1> <io1> <cpu2> <io2> ... -1
// ---------------------------------------------------------------------------
vector<ProcessInfo> loadProcesses(const string& filename) {
    ifstream file(filename);
    vector<ProcessInfo> result;
    string line;
    int pid = 1;

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        ProcessInfo p;
        p.pid = pid++;

        // First value is arrival time
        ss >> p.arrival_time;

        // Remaining values: alternating CPU/IO burst durations, terminated by -1
        int x;
        while (ss >> x && x != -1) {
            p.bursts.push_back(x);
        }

        if (!p.bursts.empty()) {
            p.remaining_cpu = p.bursts[0]; // prime the first CPU burst
        }

        result.push_back(p);
    }
    return result;
}

// ---------------------------------------------------------------------------
// writeOutput(): writes the schedule to scheduler.txt and stdout,
// then prints turnaround time metrics and CPU runtime.
// ---------------------------------------------------------------------------
void writeOutput(const vector<ScheduleEntry>& schedule,
                 const vector<ProcessInfo>& procs,
                 int total_cpu_runtime,
                 int num_cpus = 1)
{
    ofstream outfile("scheduler.txt");

    // Print per-CPU sections
    for (int c = 0; c < num_cpus; ++c) {
        string header = "CPU" + to_string(c);
        outfile << header << "\n";
        cout   << header << "\n";
        for (const auto& e : schedule) {
            if (e.cpu_id != c) continue;
            outfile << e.name << "\t" << e.start << "\t" << e.end << "\n";
            cout   << e.name << "\t" << e.start << "\t" << e.end << "\n";
        }
    }

    // Compute and print metrics
    double total_tat = 0;
    int    max_tat   = 0;
    int    n         = (int)procs.size();

    for (const auto& p : procs) {
        int tat = p.finish_time - p.arrival_time;
        total_tat += tat;
        max_tat    = max(max_tat, tat);
    }
    double avg_tat = (n > 0) ? total_tat / n : 0.0;

    cout << "\n--- Metrics ---\n";
    cout << "Average Turnaround Time : " << fixed << setprecision(2) << avg_tat << "\n";
    cout << "Maximum Turnaround Time : " << max_tat << "\n";
    cout << "Run Time (CPU only, excl. I/O): " << total_cpu_runtime << "\n";

    outfile.close();
}
