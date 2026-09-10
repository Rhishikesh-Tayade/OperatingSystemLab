/*
 * Process Scheduling Simulator
 * OS Lab 3 — IIT Dharwad
 *
 * Implements: FIFO, Round Robin, MLFQ (with optional priority boost)
 * Supports:   Single-CPU and Dual-CPU modes
 *
 * Usage:
 *   ./scheduler <algorithm> <workload-file> [num_cpus]
 *
 *   algorithm:
 *     FIFO            — First In First Out (non-preemptive)
 *     RR:<quantum>    — Round Robin with given time quantum
 *     MLFQ            — Multi-Level Feedback Queue (no boost)
 *     MLFQ:<boost>    — MLFQ with priority boost every <boost> time units
 *
 *   num_cpus:
 *     1 (default) or 2
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <deque>
#include <string>
#include <algorithm>
#include <climits>
#include <chrono>
#include <iomanip>
#include <cstring>

using namespace std;

// ─────────────────────── Data Structures ───────────────────────

struct Burst {
    enum Type { CPU, IO };
    Type type;
    int duration;
};

struct Process {
    int pid;               // 1-indexed
    int arrival_time;
    vector<Burst> bursts;

    // Runtime state
    int current_burst_idx = 0;
    int remaining = 0;     // remaining time in current burst
    int completion_time = -1;

    // MLFQ: which queue the process belongs to (0, 1, 2)
    int queue_level = 0;

    // Track which CPU burst number we are on (for schedule output, 1-indexed)
    int cpu_burst_number = 0;

    bool is_finished() const {
        return current_burst_idx >= (int)bursts.size();
    }

    void start_current_burst() {
        if (!is_finished()) {
            remaining = bursts[current_burst_idx].duration;
            if (bursts[current_burst_idx].type == Burst::CPU) {
                cpu_burst_number++;
            }
        }
    }
};

struct ScheduleEntry {
    int pid;
    int cpu_burst_number;
    int cpu_id;
    int start_time;
    int end_time;
};

// A process waiting for I/O to complete
struct WaitingProcess {
    Process* proc;
    int io_finish_time;
};

// State of one CPU
struct CPUState {
    Process* running = nullptr;
    int quantum_remaining = 0;    // for RR / MLFQ
    int run_start_time = -1;      // when did this run segment start
};

// ─────────────────────── Input Parsing ───────────────────────

vector<Process> parse_workload(const string& filename) {
    ifstream fin(filename);
    if (!fin.is_open()) {
        cerr << "Error: Cannot open file " << filename << endl;
        exit(1);
    }

    vector<Process> processes;
    string line;
    int pid = 1;

    while (getline(fin, line)) {
        if (line.empty()) continue;
        istringstream iss(line);
        int val;
        if (!(iss >> val)) continue;   // skip blank / bad lines

        Process p;
        p.pid = pid++;
        p.arrival_time = val;

        bool is_cpu = true;  // first burst after arrival is CPU
        while (iss >> val) {
            if (val == -1) break;
            Burst b;
            b.type = is_cpu ? Burst::CPU : Burst::IO;
            b.duration = val;
            p.bursts.push_back(b);
            is_cpu = !is_cpu;
        }

        if (!p.bursts.empty()) {
            processes.push_back(p);
        }
    }
    fin.close();
    return processes;
}

// ─────────────────────── Algorithm Enum ───────────────────────

enum Algorithm { FIFO, RR, MLFQ_ALGO };

struct Config {
    Algorithm algo;
    int rr_quantum = 10;         // for RR
    int mlfq_boost = 0;         // 0 = no boost; >0 = boost interval
    int num_cpus = 1;
};

Config parse_args(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0]
             << " <FIFO|RR:<q>|MLFQ|MLFQ:<boost>> <workload-file> [num_cpus]"
             << endl;
        exit(1);
    }

    Config cfg;
    string algo_str = argv[1];

    if (algo_str == "FIFO") {
        cfg.algo = FIFO;
    } else if (algo_str.substr(0, 3) == "RR:") {
        cfg.algo = RR;
        cfg.rr_quantum = stoi(algo_str.substr(3));
    } else if (algo_str.substr(0, 5) == "MLFQ:") {
        cfg.algo = MLFQ_ALGO;
        cfg.mlfq_boost = stoi(algo_str.substr(5));
    } else if (algo_str == "MLFQ") {
        cfg.algo = MLFQ_ALGO;
        cfg.mlfq_boost = 0;
    } else {
        cerr << "Unknown algorithm: " << algo_str << endl;
        exit(1);
    }

    if (argc >= 4) {
        cfg.num_cpus = stoi(argv[3]);
        if (cfg.num_cpus < 1 || cfg.num_cpus > 2)
            cfg.num_cpus = 1;
    }

    return cfg;
}

// ─────────────────────── Simulation ───────────────────────

struct SimResult {
    vector<ScheduleEntry> schedule;
    double avg_turnaround;
    double max_turnaround;
};

// Helper: pick next process from ready queue (FIFO or RR — both use front)
Process* pick_from_ready(deque<Process*>& ready_queue) {
    if (ready_queue.empty()) return nullptr;
    Process* p = ready_queue.front();
    ready_queue.pop_front();
    return p;
}

// Helper: pick next process from MLFQ queues (highest priority first)
Process* pick_from_mlfq(deque<Process*> mlfq[3]) {
    for (int q = 0; q < 3; q++) {
        if (!mlfq[q].empty()) {
            Process* p = mlfq[q].front();
            mlfq[q].pop_front();
            return p;
        }
    }
    return nullptr;
}

SimResult simulate(vector<Process>& processes, const Config& cfg) {
    int num_cpus = cfg.num_cpus;
    int time = 0;

    // Reset runtime state
    for (auto& p : processes) {
        p.current_burst_idx = 0;
        p.remaining = 0;
        p.completion_time = -1;
        p.queue_level = 0;
        p.cpu_burst_number = 0;
    }

    // Index into processes sorted by arrival time (already sorted per spec)
    int next_arrival = 0;

    // Ready queues
    deque<Process*> ready_queue;          // for FIFO, RR
    deque<Process*> mlfq_queues[3];      // for MLFQ

    // Waiting list (I/O)
    vector<WaitingProcess> waiting;

    // CPUs
    vector<CPUState> cpus(num_cpus);

    // Schedule log
    vector<ScheduleEntry> schedule;

    int mlfq_quantum = 2;  // MLFQ always uses quantum = 2
    int finished_count = 0;
    int total_processes = (int)processes.size();

    auto add_to_ready = [&](Process* p) {
        if (cfg.algo == MLFQ_ALGO) {
            mlfq_queues[p->queue_level].push_back(p);
        } else {
            ready_queue.push_back(p);
        }
    };

    auto pick_next = [&]() -> Process* {
        if (cfg.algo == MLFQ_ALGO) {
            return pick_from_mlfq(mlfq_queues);
        } else {
            return pick_from_ready(ready_queue);
        }
    };

    auto get_quantum = [&](Process* p) -> int {
        if (cfg.algo == FIFO) return INT_MAX;   // no preemption
        if (cfg.algo == RR)   return cfg.rr_quantum;
        // MLFQ: quantum = 2 for all queues
        return mlfq_quantum;
    };

    // Record a schedule entry when a process stops running on a CPU
    auto record_schedule = [&](int cpu_id, Process* p, int start, int end) {
        ScheduleEntry e;
        e.pid = p->pid;
        e.cpu_burst_number = p->cpu_burst_number;
        e.cpu_id = cpu_id;
        e.start_time = start;
        e.end_time = end;
        schedule.push_back(e);
    };

    while (finished_count < total_processes) {
        // 1. Arrivals: add newly arrived processes to ready queue
        while (next_arrival < total_processes &&
               processes[next_arrival].arrival_time <= time) {
            Process* p = &processes[next_arrival];
            p->start_current_burst();
            add_to_ready(p);
            next_arrival++;
        }

        // 2. I/O completions
        {
            vector<WaitingProcess> still_waiting;
            // Sort so that processes with earlier io_finish_time and lower pid
            // are added to ready first for determinism
            sort(waiting.begin(), waiting.end(),
                 [](const WaitingProcess& a, const WaitingProcess& b) {
                     if (a.io_finish_time != b.io_finish_time)
                         return a.io_finish_time < b.io_finish_time;
                     return a.proc->pid < b.proc->pid;
                 });
            for (auto& wp : waiting) {
                if (wp.io_finish_time <= time) {
                    // I/O done, move to next burst (should be CPU)
                    wp.proc->current_burst_idx++;
                    if (!wp.proc->is_finished()) {
                        wp.proc->start_current_burst();
                        // For MLFQ: process keeps its queue level after I/O
                        add_to_ready(wp.proc);
                    } else {
                        wp.proc->completion_time = time;
                        finished_count++;
                    }
                } else {
                    still_waiting.push_back(wp);
                }
            }
            waiting = still_waiting;
        }

        // 3. MLFQ priority boost
        if (cfg.algo == MLFQ_ALGO && cfg.mlfq_boost > 0 && time > 0 &&
            time % cfg.mlfq_boost == 0) {
            // Move everything from Q1 and Q2 to Q0
            for (int q = 1; q <= 2; q++) {
                while (!mlfq_queues[q].empty()) {
                    Process* p = mlfq_queues[q].front();
                    mlfq_queues[q].pop_front();
                    p->queue_level = 0;
                    mlfq_queues[0].push_back(p);
                }
            }
            // Also boost running processes
            for (auto& cpu : cpus) {
                if (cpu.running) {
                    cpu.running->queue_level = 0;
                }
            }
        }

        // 4. Assign processes to idle CPUs
        for (int c = 0; c < num_cpus; c++) {
            if (cpus[c].running == nullptr) {
                Process* p = pick_next();
                if (p) {
                    cpus[c].running = p;
                    cpus[c].quantum_remaining = get_quantum(p);
                    cpus[c].run_start_time = time;
                }
            }
        }

        // Check if nothing is happening (all CPUs idle, nothing waiting, no arrivals soon)
        bool any_running = false;
        for (int c = 0; c < num_cpus; c++) {
            if (cpus[c].running) any_running = true;
        }
        if (!any_running && waiting.empty()) {
            // No one running and no I/O pending → fast-forward to next arrival
            if (next_arrival < total_processes) {
                time = processes[next_arrival].arrival_time;
                continue;
            } else {
                break;  // truly nothing left
            }
        }
        if (!any_running && !waiting.empty()) {
            // Fast-forward to earliest I/O completion
            int earliest = INT_MAX;
            for (auto& wp : waiting) {
                earliest = min(earliest, wp.io_finish_time);
            }
            // Also consider next arrival
            if (next_arrival < total_processes) {
                earliest = min(earliest, processes[next_arrival].arrival_time);
            }
            time = earliest;
            continue;
        }

        // 5. Advance time by 1 tick and update running processes
        time++;

        for (int c = 0; c < num_cpus; c++) {
            if (!cpus[c].running) continue;

            Process* p = cpus[c].running;
            p->remaining--;
            cpus[c].quantum_remaining--;

            if (p->remaining == 0) {
                // CPU burst finished
                record_schedule(c, p, cpus[c].run_start_time, time);
                cpus[c].running = nullptr;

                p->current_burst_idx++;
                if (p->is_finished()) {
                    // Process fully done
                    p->completion_time = time;
                    finished_count++;
                } else {
                    // Next burst should be I/O
                    Burst& next_b = p->bursts[p->current_burst_idx];
                    if (next_b.type == Burst::IO) {
                        WaitingProcess wp;
                        wp.proc = p;
                        wp.io_finish_time = time + next_b.duration;
                        waiting.push_back(wp);
                    } else {
                        // Consecutive CPU bursts (unusual but handle it)
                        p->start_current_burst();
                        add_to_ready(p);
                    }
                }
            } else if (cpus[c].quantum_remaining == 0) {
                // Quantum expired → preempt
                record_schedule(c, p, cpus[c].run_start_time, time);
                cpus[c].running = nullptr;

                if (cfg.algo == MLFQ_ALGO) {
                    // Demote
                    if (p->queue_level < 2)
                        p->queue_level++;
                }
                add_to_ready(p);
            }
        }
    }

    // ── Compute statistics ──
    double total_tat = 0;
    int max_tat = 0;
    for (auto& p : processes) {
        int tat = p.completion_time - p.arrival_time;
        total_tat += tat;
        max_tat = max(max_tat, tat);
    }

    SimResult result;
    result.schedule = schedule;
    result.avg_turnaround = total_tat / total_processes;
    result.max_turnaround = max_tat;
    return result;
}

// ─────────────────────── Output ───────────────────────

void print_results(const SimResult& result, const Config& cfg,
                   double runtime_ms) {
    // Group schedule entries by CPU
    for (int c = 0; c < cfg.num_cpus; c++) {
        cout << "CPU" << c << endl;
        for (auto& e : result.schedule) {
            if (e.cpu_id == c) {
                cout << "P" << e.pid << "," << e.cpu_burst_number
                     << "\t" << e.start_time << "\t" << e.end_time << endl;
            }
        }
    }

    cout << endl;
    cout << "Average Turnaround Time: " << fixed << setprecision(2)
         << result.avg_turnaround << endl;
    cout << "Maximum Turnaround Time: " << result.max_turnaround << endl;
    cout << "Simulator Run Time: " << fixed << setprecision(3)
         << runtime_ms << " ms" << endl;
}

// ─────────────────────── Main ───────────────────────

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);
    string filename = argv[2];

    vector<Process> processes = parse_workload(filename);

    if (processes.empty()) {
        cerr << "No processes found in " << filename << endl;
        return 1;
    }

    // Measure simulation time (excluding I/O)
    auto start = chrono::high_resolution_clock::now();
    SimResult result = simulate(processes, cfg);
    auto end = chrono::high_resolution_clock::now();

    double runtime_ms = chrono::duration<double, milli>(end - start).count();

    print_results(result, cfg, runtime_ms);

    return 0;
}
