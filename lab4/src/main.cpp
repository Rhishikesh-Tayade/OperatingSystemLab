#include "trace_parser.hpp"
#include "page_table_single.hpp"
#include "page_table_two_level.hpp"
#include "tlb.hpp"
#include "frame_allocator.hpp"
#include "hash_util.hpp"
#include "stats.hpp"

#include <iostream>
#include <string>
#include <chrono>
#include <cstdlib>

using namespace std;

void print_usage() {
    cout << "Usage: ./simulator --part <1-5> --trace <trace_file> [options]\n"
         << "Options:\n"
         << "  --part <N>       Part number (1-5)\n"
         << "  --trace <file>   Path to trace file\n"
         << "  --pt <1|2>       Page table type: 1=single-level, 2=two-level (default: 1)\n"
         << "  --mem-ratio <R>  Physical/virtual memory ratio for Part IV (e.g. 0.5, 0.25)\n"
         << "  --csv <file>     Export Hypothesis B data to CSV\n"
         << "  --benchmark <N>  Benchmark name (for display only)\n";
}

// ======================== Part I: Single-Level Page Table ========================
void run_part1(TraceParser& parser, SimStats& stats, const string& csv_path) {
    SingleLevelPageTable pt;
    FrameAllocator alloc(0); // unlimited frames

    TraceEntry entry;
    while (parser.next(entry)) {
        uint32_t vpn = entry.get_vpn();
        stats.total_accesses++;
        stats.page_access_counts[vpn]++;

        uint32_t frame = 0;

        // Page table lookup: 1 memory access = 200 cycles
        stats.total_cycles += 200;

        if (!pt.lookup(vpn, frame)) {
            bool pf = false; uint32_t victim = 0;
            frame = alloc.allocate_frame(vpn, pf, victim);
            pt.map(vpn, frame);
        }

        if (entry.rw == 'W') pt.set_dirty(vpn, true);

        // Memory access: 200 cycles
        stats.total_cycles += 200;

        stats.hash_value = update_hash(stats.hash_value, frame);
    }

    stats.frames_used = alloc.get_frames_used();
    stats.pt_entries_accessed = pt.get_valid_entries_count();
    stats.pt_entries_allocated = pt.get_allocated_entries();

    if (!csv_path.empty()) stats.export_hypothesis_b_data(csv_path);
}

// ======================== Part II: Two-Level Page Table ========================
void run_part2(TraceParser& parser, SimStats& stats, const string& csv_path) {
    TwoLevelPageTable pt;
    FrameAllocator alloc(0); // unlimited frames

    TraceEntry entry;
    while (parser.next(entry)) {
        uint32_t vpn = entry.get_vpn();
        stats.total_accesses++;
        stats.page_access_counts[vpn]++;

        uint32_t frame = 0;

        // Two-level page table lookup: 2 memory accesses = 400 cycles
        stats.total_cycles += 200; // L1 directory
        stats.total_cycles += 200; // L2 table

        if (!pt.lookup(vpn, frame)) {
            bool pf = false; uint32_t victim = 0;
            frame = alloc.allocate_frame(vpn, pf, victim);
            pt.map(vpn, frame);
        }

        if (entry.rw == 'W') pt.set_dirty(vpn, true);

        // Memory access: 200 cycles
        stats.total_cycles += 200;

        stats.hash_value = update_hash(stats.hash_value, frame);
    }

    stats.frames_used = alloc.get_frames_used();
    stats.pt_entries_accessed = pt.get_valid_entries_count();
    stats.pt_entries_allocated = pt.get_allocated_entries();

    if (!csv_path.empty()) stats.export_hypothesis_b_data(csv_path);
}

// ======================== Part III: Page Table + TLB ========================
void run_part3(TraceParser& parser, SimStats& stats, int pt_type) {
    SingleLevelPageTable pt1;
    TwoLevelPageTable pt2;
    FrameAllocator alloc(0); // unlimited frames
    TLB tlb;

    TraceEntry entry;
    while (parser.next(entry)) {
        uint32_t vpn = entry.get_vpn();
        stats.total_accesses++;

        uint32_t frame = 0;
        bool tlb_hit = false;

        // TLB access: 1 cycle
        stats.total_cycles += 1;
        if (tlb.lookup(vpn, frame)) {
            stats.tlb_hits++;
            tlb_hit = true;
        } else {
            stats.tlb_misses++;
        }

        if (!tlb_hit) {
            // Page table walk
            if (pt_type == 1) {
                stats.total_cycles += 200; // single-level PT lookup
                if (!pt1.lookup(vpn, frame)) {
                    bool pf = false; uint32_t victim = 0;
                    frame = alloc.allocate_frame(vpn, pf, victim);
                    pt1.map(vpn, frame);
                }
                if (entry.rw == 'W') pt1.set_dirty(vpn, true);
            } else {
                stats.total_cycles += 200; // L1 directory
                stats.total_cycles += 200; // L2 table
                if (!pt2.lookup(vpn, frame)) {
                    bool pf = false; uint32_t victim = 0;
                    frame = alloc.allocate_frame(vpn, pf, victim);
                    pt2.map(vpn, frame);
                }
                if (entry.rw == 'W') pt2.set_dirty(vpn, true);
            }

            // Insert into TLB
            tlb.insert(vpn, frame);
        }

        // Memory access: 200 cycles
        stats.total_cycles += 200;

        stats.hash_value = update_hash(stats.hash_value, frame);
    }

    stats.frames_used = alloc.get_frames_used();
    if (pt_type == 1) {
        stats.pt_entries_accessed = pt1.get_valid_entries_count();
        stats.pt_entries_allocated = pt1.get_allocated_entries();
    } else {
        stats.pt_entries_accessed = pt2.get_valid_entries_count();
        stats.pt_entries_allocated = pt2.get_allocated_entries();
    }
}

// ======================== Part IV: Limited Memory + Page Replacement ========================
void run_part4(TraceParser& parser, SimStats& stats, int pt_type, double mem_ratio) {
    SingleLevelPageTable pt1;
    TwoLevelPageTable pt2;

    // Calculate number of physical frames based on ratio
    // Virtual memory = 2^32 bytes, page size = 4KB = 2^12
    // Total virtual pages = 2^20 = 1048576
    size_t total_virtual_pages = 1 << 20;
    size_t max_frames = static_cast<size_t>(total_virtual_pages * mem_ratio);
    if (max_frames == 0) max_frames = 1;

    FrameAllocator alloc(max_frames);
    TLB tlb;

    TraceEntry entry;
    while (parser.next(entry)) {
        uint32_t vpn = entry.get_vpn();
        stats.total_accesses++;

        uint32_t frame = 0;
        bool tlb_hit = false;

        // TLB access: 1 cycle
        stats.total_cycles += 1;
        if (tlb.lookup(vpn, frame)) {
            stats.tlb_hits++;
            tlb_hit = true;
        } else {
            stats.tlb_misses++;
        }

        if (!tlb_hit) {
            bool is_mapped = false;

            if (pt_type == 1) {
                stats.total_cycles += 200;
                is_mapped = pt1.lookup(vpn, frame);
            } else {
                stats.total_cycles += 200;
                stats.total_cycles += 200;
                is_mapped = pt2.lookup(vpn, frame);
            }

            if (!is_mapped) {
                bool page_fault = false;
                uint32_t victim_vpn = 0;
                frame = alloc.allocate_frame(vpn, page_fault, victim_vpn);

                if (page_fault) {
                    stats.page_faults++;
                    stats.total_cycles += 20000000; // 20 million cycles disk access

                    // Invalidate victim in page table and TLB
                    if (pt_type == 1) {
                        pt1.unmap(victim_vpn);
                    } else {
                        pt2.unmap(victim_vpn);
                    }
                    tlb.invalidate(victim_vpn);
                }

                if (pt_type == 1) {
                    pt1.map(vpn, frame);
                } else {
                    pt2.map(vpn, frame);
                }
            }

            if (entry.rw == 'W') {
                if (pt_type == 1) pt1.set_dirty(vpn, true);
                else pt2.set_dirty(vpn, true);
            }

            tlb.insert(vpn, frame);
        }

        // Memory access: 200 cycles
        stats.total_cycles += 200;

        // Update LRU for the accessed frame
        alloc.touch_frame(frame);

        stats.hash_value = update_hash(stats.hash_value, frame);
    }

    stats.frames_used = alloc.get_frames_used();
    if (pt_type == 1) {
        stats.pt_entries_accessed = pt1.get_valid_entries_count();
        stats.pt_entries_allocated = pt1.get_allocated_entries();
    } else {
        stats.pt_entries_accessed = pt2.get_valid_entries_count();
        stats.pt_entries_allocated = pt2.get_allocated_entries();
    }
}

// ======================== Main ========================
int main(int argc, char* argv[]) {
    int part = 0;
    int pt_type = 1;
    double mem_ratio = 0.5;
    string trace_file = "";
    string csv_path = "";
    string benchmark_name = "";

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--part" && i + 1 < argc) {
            part = atoi(argv[++i]);
        } else if (arg == "--trace" && i + 1 < argc) {
            trace_file = argv[++i];
        } else if (arg == "--pt" && i + 1 < argc) {
            pt_type = atoi(argv[++i]);
        } else if (arg == "--mem-ratio" && i + 1 < argc) {
            mem_ratio = atof(argv[++i]);
        } else if (arg == "--csv" && i + 1 < argc) {
            csv_path = argv[++i];
        } else if (arg == "--benchmark" && i + 1 < argc) {
            benchmark_name = argv[++i];
        } else if (arg == "--help") {
            print_usage();
            return 0;
        }
    }

    if (part == 0 || trace_file.empty()) {
        print_usage();
        return 1;
    }

    if (pt_type != 1 && pt_type != 2) {
        cerr << "Error: --pt must be 1 or 2\n";
        return 1;
    }

    if (part == 4 && (!(mem_ratio > 0.0) || mem_ratio > 1.0)) {
        cerr << "Error: --mem-ratio must be in (0, 1], got " << mem_ratio << "\n";
        return 1;
    }

    TraceParser parser(trace_file);
    if (!parser.is_open()) {
        cerr << "Error: cannot open trace file: " << trace_file << "\n";
        return 1;
    }

    SimStats stats;
    auto start_time = chrono::high_resolution_clock::now();

    switch (part) {
        case 1:
            run_part1(parser, stats, csv_path);
            break;
        case 2:
            run_part2(parser, stats, csv_path);
            break;
        case 3:
            run_part3(parser, stats, pt_type);
            break;
        case 4:
            run_part4(parser, stats, pt_type, mem_ratio);
            break;
        case 5: {
            // Part V: same as Part III (with TLB) but we measure wall-clock time
            run_part3(parser, stats, pt_type);
            break;
        }
        default:
            cerr << "Error: --part must be 1-5\n";
            return 1;
    }

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double> elapsed = end_time - start_time;
    stats.execution_time_sec = elapsed.count();

    stats.print_summary(part, benchmark_name);

    return 0;
}
