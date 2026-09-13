#include<iostream>
#include<fstream>
#include<sstream>
#include<vector>
#include<deque>
#include<algorithm>
#include<string>
#include<climits>
#include<chrono>
#include <iomanip>
#include <cstring>


using namespace std;

struct Brust {
    enum Type { CPU, IO };
    Type type;
    int duration;
};

struct Process {
    int pid;
    int arrival_time;
    vector<Brust> bursts;

    int current_burst_idx = 0;
    int remaining = 0;
    int completion_time = -1;

    int queue_level = 0;
    int cpu_burst_number = 0;

    bool is_finished() const {
        return static_cast<size_t>(current_burst_idx) >= bursts.size();
    }

    void start_current_burst() {
        if(!is_finished()) {
            remaining = bursts[current_burst_idx].duration;
            if(bursts[current_burst_idx].type == Brust::CPU) {
                cpu_burst_number++;
            }
        }
    }
};

struct ScheduleEntry {
    int pid;
    int cpu_burst_number;
    int start_time;
    int end_time;
    int cpu_id;
};


struct WaitingProcess {
    Process* proc;
    int io_finish_time;
};

struct CPUState {
    Process* running = nullptr;
    int quantum_remaining = 0;
    int run_start_time = -1;
};

// Input parsing 
vector<Process> parse_workload(const string& filename) {
    ifstream fin(filename);
    if(!fin.is_open()) {
        cerr << "Error opening file." << endl;
        exit(1);
    }

    vector<Process> processes;
    string line;
    int pid = 1;

    while(getline(fin, line)) {
        if(line.empty()) continue;
        istringstream iss(line);
        int val;
        if(!(iss >> val)) continue;
        
        Process p;
        p.pid = pid++;
        p.arrival_time = val;

        bool is_cpu = true;
        while(iss >> val) {
            if(val == -1) break;
            Brust b;
            b.type = is_cpu ? Brust::CPU : Brust::IO;
            b.duration = val;
            p.bursts.push_back(b);
            is_cpu = !is_cpu;
        }
        if(!p.bursts.empty()) {
            processes.push_back(p);
        }
    }
    fin.close();
    return processes;
}

enum Algorithm { FIFO, RR, MLFQ_ALGO };

struct Config {
    Algorithm algo;
    int rr_quantum = 10;
    int mlfq_boost = 0;
    int num_cpus = 1;
};

Config parse_args(int argc, char* argv[]) {
    if(argc < 3) {
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

struct SimResult {
    vector<ScheduleEntry> schedule;
    double avg_turnaround;
    double max_turnaround;
};

Process* pick_from_ready(deque<Process*>& ready_queue) {
    if(ready_queue.empty()) return nullptr;
    Process* p = ready_queue.front();
    ready_queue.pop_front();
    return p;
}

Process* pick_from_mlfq(deque<Process*> mlfq[3]) {
    for(int q = 0 ; q < 3 ; q++) {
        if(!mlfq[q].empty()) {
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

    for(auto& p : processes) {
        p.current_burst_idx = 0;
        p.remaining = 0;
        p.completion_time = -1;
        p.queue_level = 0;
        p.cpu_burst_number = 0;
    }

    deque<Process*> ready_queue;
    deque<Process*> mlfq_queues[3];
    
    vector<WaitingProcess> waiting;
    vector<CPUState> cpus(num_cpus);
    vector<ScheduleEntry> schedule;

    int next_arrival = 0;
    int mlfq_quantum = 2;
    int finished_count = 0;
    int total_count = (int)processes.size();

    auto add_to_ready = [&](Process* p) {
        if(cfg.algo == MLFQ_ALGO) {
            mlfq_queues[p->queue_level].push_back(p);
        } else {
            ready_queue.push_back(p);
        }
    };

    auto pick_next = [&]() {
        if(cfg.algo == MLFQ_ALGO) {
            return pick_from_mlfq(mlfq_queues);
        }else {
            return pick_from_ready(ready_queue);
        }
    };

    auto get_quantum = [&](Process*) {
        if(cfg.algo == RR) return cfg.rr_quantum;
        else if(cfg.algo == FIFO) return INT_MAX;
        return mlfq_quantum;
    };

    auto schedule_record = [&](int cpu_id, Process* p, int start, int end) {
        ScheduleEntry s;
        s.pid = p->pid;
        s.cpu_burst_number = p->cpu_burst_number;
        s.start_time = start;
        s.end_time = end;
        s.cpu_id = cpu_id;
        schedule.push_back(s);
    };

    while(finished_count < total_count) {
        // Add newly arrived processes 
        while(next_arrival < total_count && processes[next_arrival].arrival_time <= time) {
            Process* p = &processes[next_arrival];
            p->start_current_burst();
            add_to_ready(p);
            next_arrival++;
        }

        // I/O completions
        {
            vector<WaitingProcess> still_waiting;
            sort(waiting.begin(), waiting.end(), [](const WaitingProcess& wp1, const WaitingProcess& wp2) {
                if(wp1.io_finish_time != wp2.io_finish_time) 
                    return wp1.io_finish_time < wp2.io_finish_time;
                return wp1.proc->pid < wp2.proc->pid;
            });
            for(auto& wp : waiting) {
                if(wp.io_finish_time <= time) {
                    wp.proc->current_burst_idx++;
                    if(!wp.proc->is_finished()) {
                        wp.proc->start_current_burst();
                        add_to_ready(wp.proc);
                    } else{
                        wp.proc->completion_time = time;
                        finished_count++;
                    }
                } else {
                    still_waiting.push_back(wp);
                }
            }
            waiting = still_waiting;
        }

        // MLFQ Boost
        if(cfg.algo == MLFQ_ALGO && cfg.mlfq_boost > 0 && time > 0 && time % cfg.mlfq_boost == 0) {
            for(int q = 1 ; q <= 2 ; q++) {
                while(!mlfq_queues[q].empty()) {
                    Process *p = mlfq_queues[q].front();
                    mlfq_queues[q].pop_front();
                    p->queue_level = 0;
                    mlfq_queues[0].push_back(p);
                }
            }
            for(auto& cpu : cpus) {
                if(cpu.running) {
                    cpu.running->queue_level = 0;
                }
            } 
        }

        for(int c = 0 ; c < num_cpus ; c++) {
            if(cpus[c].running == nullptr) {
                Process* p = pick_next();
                if(p) {
                    cpus[c].running = p;
                    cpus[c].quantum_remaining = get_quantum(p);
                    cpus[c].run_start_time = time;
                }
            }
        }   

        bool any_running = false;
        for(auto& cpu : cpus) {
            if(cpu.running) any_running = true;
        }

        if(!any_running && waiting.empty()) {
            if(next_arrival < total_count) {
                time = processes[next_arrival].arrival_time;
                continue;
            } else {
                break;
            }
        }

        if(!any_running && !waiting.empty()) {
            int earliest = INT_MAX;
            for(auto& wp : waiting) {
                earliest = min(earliest, wp.io_finish_time);
            }
            if(next_arrival < total_count) {
                earliest = min(earliest, processes[next_arrival].arrival_time);
            }
            time = earliest;
            continue;
        }

        time++;

        for(int c = 0 ; c < num_cpus ; c++) {
            if(cpus[c].running == nullptr) continue;

            Process* p = cpus[c].running;
            p->remaining--;
            cpus[c].quantum_remaining--;

            if(p->remaining == 0) {
                schedule_record(c, p, cpus[c].run_start_time, time);
                cpus[c].running = nullptr;
                
                p->current_burst_idx++;
                if(p->is_finished()) {
                    p->completion_time = time;
                    finished_count++;
                } else {
                    Brust& next_b = p->bursts[p->current_burst_idx];
                    if(next_b.type == Brust::IO) {
                        WaitingProcess wp;
                        wp.proc = p;
                        wp.io_finish_time = time + next_b.duration;
                        waiting.push_back(wp);
                    } else {
                        p->start_current_burst();
                        add_to_ready(p);
                    }
                }
            } else if (cpus[c].quantum_remaining == 0) {
                schedule_record(c, p, cpus[c].run_start_time, time);
                cpus[c].running = nullptr;
                
                if(cfg.algo == MLFQ_ALGO) {
                    if(p->queue_level < 2) 
                        p->queue_level++;
                }

                add_to_ready(p);
            }
        }
    }

    double total_tat = 0;
    int max_tat = 0;
    for(auto& p : processes) {
        int tat = p.completion_time - p.arrival_time;
        total_tat += tat;
        max_tat = max(max_tat, tat);
    }

    SimResult result;
    result.schedule = schedule;
    result.avg_turnaround = total_tat / total_count;
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


int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);
    string filename = argv[2];

    vector<Process> processes = parse_workload(filename);

    if (processes.empty()) {
        cerr << "No processes found in " << filename << endl;
        return 1;
    }

    auto start = chrono::high_resolution_clock::now();
    SimResult result = simulate(processes, cfg);
    auto end   = chrono::high_resolution_clock::now();

    double runtime_ms = chrono::duration<double, milli>(end-start).count(); 

    print_results(result, cfg, runtime_ms);

    return 0;
}