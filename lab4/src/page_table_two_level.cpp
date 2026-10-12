#include "page_table_two_level.hpp"

using namespace std;

TwoLevelPageTable::TwoLevelPageTable() {
    for (size_t i = 0; i < L1_ENTRIES; i++) {
        l1_dir[i] = nullptr;
    }
    l2_tables_count = 0;
    valid_count = 0;
}

TwoLevelPageTable::~TwoLevelPageTable() {
    for (size_t i = 0; i < L1_ENTRIES; i++) {
        if (l1_dir[i] != nullptr) {
            delete l1_dir[i];
        }
    }
}

bool TwoLevelPageTable::lookup(uint32_t vpn, uint32_t& frame_out) const {
    if (vpn >= NUM_PAGES) return false;
    uint32_t l1_index = (vpn >> 10) & 0x3FF;
    uint32_t l2_index = vpn & 0x3FF;
    
    if(l1_dir[l1_index] == nullptr) return false;
    if(l1_dir[l1_index]->entries[l2_index].valid) {
        frame_out = l1_dir[l1_index]->entries[l2_index].frame_number;
        return true;
    }
    return false;
}

void TwoLevelPageTable::map(uint32_t vpn, uint32_t frame) {
    if(vpn >= NUM_PAGES) return;
    uint32_t l1_index = (vpn >> 10) & 0x3FF;
    uint32_t l2_index = vpn & 0x3FF;

    if(l1_dir[l1_index] == nullptr) {
        l1_dir[l1_index] = new Level2Table();
        l2_tables_count++;
    }

    if (!l1_dir[l1_index]->entries[l2_index].valid) valid_count++;

    l1_dir[l1_index]->entries[l2_index].frame_number = frame;
    l1_dir[l1_index]->entries[l2_index].valid = true;   
}

void TwoLevelPageTable::unmap(uint32_t vpn) {
    uint32_t l1_idx = (vpn >> 10) & 0x3FF;
    uint32_t l2_idx = vpn & 0x3FF;

    if(l1_dir[l1_idx] != nullptr && l1_dir[l1_idx]->entries[l2_idx].valid) {
        l1_dir[l1_idx]->entries[l2_idx].valid = false;
        valid_count--;
    } 
} 

void TwoLevelPageTable::set_dirty(uint32_t vpn, bool dirty) {
    uint32_t l1_idx = (vpn >> 10) & 0x3FF;
    uint32_t l2_idx = vpn & 0x3FF;

    if(l1_dir[l1_idx] != nullptr && l1_dir[l1_idx]->entries[l2_idx].valid) {
        l1_dir[l1_idx]->entries[l2_idx].dirty = dirty;
    }
}

bool TwoLevelPageTable::is_valid(uint32_t vpn) const {
    uint32_t l1_idx = (vpn >> 10) & 0x3FF;
    uint32_t l2_idx = vpn & 0x3FF;

    if(l1_dir[l1_idx] == nullptr) return false ;
    return l1_dir[l1_idx]->entries[l2_idx].valid;
}

bool TwoLevelPageTable::is_dirty(uint32_t vpn) const {
    uint32_t l1_idx = (vpn >> 10) & 0x3FF;
    uint32_t l2_idx = vpn & 0x3FF;

    if(l1_dir[l1_idx] == nullptr) return false ;
    return l1_dir[l1_idx]->entries[l2_idx].dirty;
}

size_t TwoLevelPageTable::get_allocated_entries() const {
    return L1_ENTRIES + (L2_ENTRIES * l2_tables_count);
}

void TwoLevelPageTable::reset() {
    for(size_t i = 0 ; i < L1_ENTRIES ; i++) {
        if(l1_dir[i] != nullptr) {
            delete l1_dir[i];
            l1_dir[i] = nullptr;
        }
    }
    l2_tables_count = 0;
    valid_count = 0;
}