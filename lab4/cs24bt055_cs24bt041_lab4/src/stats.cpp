#include "stats.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace std;

void SimStats::reset() {
    total_accesses = 0;
    total_cycles = 0;
    frames_used = 0;
    tlb_hits = 0;
    tlb_misses = 0;
    page_faults = 0;
    pt_entries_accessed = 0;
    pt_entries_allocated = 0;
    hash_value = 0;
    execution_time_sec = 0.0;
    page_access_counts.clear();
}

double SimStats::get_tlb_hit_rate() const {
    return total_accesses ? static_cast<double>(tlb_hits) / static_cast<double>(total_accesses) : 0.0;
}

double SimStats::get_pt_utilization() const {
    return pt_entries_allocated ? static_cast<double>(pt_entries_accessed) / static_cast<double>(pt_entries_allocated) : 0.0;
}

double SimStats::get_page_fault_rate() const {
    return total_accesses ? static_cast<double>(page_faults) / static_cast<double>(total_accesses) : 0.0;
}

void SimStats::print_summary(int part_num, const string& benchmark) const {
    cout << "\n=======================================================\n";
    cout << " Results - Part " << part_num;
    if (!benchmark.empty()) cout << " (" << benchmark << ")";
    cout << "\n=======================================================\n";
    cout << left << setw(30) << "Total Accesses:" << total_accesses << "\n";
    cout << left << setw(30) << "Total Execution Cycles:" << total_cycles << "\n";
    cout << left << setw(30) << "Frames Used:" << frames_used << "\n";
    cout << left << setw(30) << "PT Entries Allocated:" << pt_entries_allocated << "\n";
    cout << left << setw(30) << "PT Entries Accessed:" << pt_entries_accessed << "\n";
    cout << left << setw(30) << "PT Utilization:" 
         << fixed << setprecision(4) << (get_pt_utilization() * 100.0) << " %\n";
    cout << left << setw(30) << "Computed Hash (H):" << "0x" 
         << hex << uppercase << hash_value << dec << "\n";

    if (part_num >= 3) {
        cout << "-------------------------------------------------------\n";
        cout << left << setw(30) << "TLB Hits:" << tlb_hits << "\n";
        cout << left << setw(30) << "TLB Misses:" << tlb_misses << "\n";
        cout << left << setw(30) << "TLB Hit Rate:" 
             << fixed << setprecision(4) << (get_tlb_hit_rate() * 100.0) << " %\n";
    }

    if (part_num >= 4) {
        cout << "-------------------------------------------------------\n";
        cout << left << setw(30) << "Page Faults:" << page_faults << "\n";
        cout << left << setw(30) << "Page Fault Rate:" 
             << fixed << setprecision(6) << (get_page_fault_rate() * 100.0) << " %\n";
    }

    if (execution_time_sec > 0.0) {
        cout << "-------------------------------------------------------\n";
        cout << left << setw(30) << "Simulator Time:" 
             << fixed << setprecision(4) << execution_time_sec << " s\n";
    }
    cout << "=======================================================\n\n";
}

void SimStats::export_hypothesis_b_data(const string& output_csv_path) const {
    if (page_access_counts.empty() || total_accesses == 0) return;

    // Collect access counts and sort descending
    vector<uint64_t> counts;
    counts.reserve(page_access_counts.size());
    for (const auto& pair : page_access_counts) {
        counts.push_back(pair.second);
    }
    sort(counts.rbegin(), counts.rend());

    ofstream out(output_csv_path);
    if (!out.is_open()) return;

    out << "rank,vpn_count,cumulative_page_fraction,cumulative_access_fraction\n";
    uint64_t cumulative_accesses = 0;
    size_t total_unique_pages = counts.size();

    for (size_t i = 0; i < total_unique_pages; ++i) {
        cumulative_accesses += counts[i];
        double page_frac = static_cast<double>(i + 1) / total_unique_pages;
        double access_frac = static_cast<double>(cumulative_accesses) / static_cast<double>(total_accesses);
        out << (i + 1) << "," << counts[i] << "," << page_frac << "," << access_frac << "\n";
    }
    out.close();
}
