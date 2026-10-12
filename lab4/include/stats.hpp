#ifndef STATS_HPP
#define STATS_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

using namespace std;

struct SimStats {
    uint64_t total_accesses = 0;
    uint64_t total_cycles = 0;
    uint64_t frames_used = 0;
    uint64_t tlb_hits = 0;
    uint64_t tlb_misses = 0;
    uint64_t page_faults = 0;
    uint64_t pt_entries_accessed = 0;
    uint64_t pt_entries_allocated = 0;
    uint64_t hash_value = 0;
    double execution_time_sec = 0.0;

    // Per-page access count for Hypothesis B (VPN -> count)
    unordered_map<uint32_t, uint64_t> page_access_counts;

    void reset();
    double get_tlb_hit_rate() const;
    double get_pt_utilization() const;
    double get_page_fault_rate() const;
    void print_summary(int part_num, const string& benchmark = "") const;
    void export_hypothesis_b_data(const string& output_csv_path) const;
};

#endif
