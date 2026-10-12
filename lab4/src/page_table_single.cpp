#include "page_table_single.hpp"

SingleLevelPageTable::SingleLevelPageTable() {
    entries.resize(NUM_PAGES);
    valid_count = 0;
}

SingleLevelPageTable::~SingleLevelPageTable() = default;

bool SingleLevelPageTable::lookup(uint32_t vpn, uint32_t& frame_out) const {
    if (vpn >= NUM_PAGES || !entries[vpn].valid) {
        return false;
    }
    frame_out = entries[vpn].frame_number;
    return true;
}

void SingleLevelPageTable::map(uint32_t vpn, uint32_t frame_number) {
    if (vpn >= NUM_PAGES) return;
    if (!entries[vpn].valid) {
        valid_count++;
    }
    entries[vpn].frame_number = frame_number;
    entries[vpn].valid = true;
}

void SingleLevelPageTable::unmap(uint32_t vpn) {
    if (vpn >= NUM_PAGES) return;
    if (entries[vpn].valid) {
        entries[vpn].valid = false;
        entries[vpn].dirty = false;
        valid_count--;
    }
}

void SingleLevelPageTable::set_dirty(uint32_t vpn, bool dirty) {
    if (vpn < NUM_PAGES && entries[vpn].valid) {
        entries[vpn].dirty = dirty;
    }
}

bool SingleLevelPageTable::is_valid(uint32_t vpn) const {
    if (vpn >= NUM_PAGES) return false;
    return entries[vpn].valid;
}

bool SingleLevelPageTable::is_dirty(uint32_t vpn) const {
    if (vpn >= NUM_PAGES) return false;
    return entries[vpn].dirty;
}

void SingleLevelPageTable::reset() {
    for (auto& entry : entries) {
        entry.valid = false;
        entry.dirty = false;
        entry.frame_number = 0;
    }
    valid_count = 0;
}
