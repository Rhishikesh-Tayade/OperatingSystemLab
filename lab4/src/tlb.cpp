#include"tlb.hpp"

using namespace std;

TLB::TLB(size_t capacity) : max_capacity(capacity) {}

TLB::~TLB() {}

bool TLB::lookup(uint32_t vpn, uint32_t& frame_out) {
    auto it = tlb_map.find(vpn);
    if(it != tlb_map.end()) {
        frame_out = it->second->frame;
        lru_list.splice(lru_list.begin(), lru_list, it->second);
        return true;
    }
    return false;
}

void TLB::insert(uint32_t vpn, uint32_t frame) {
    auto it = tlb_map.find(vpn);
    if(it != tlb_map.end()) {
        it->second->frame = frame;
        lru_list.splice(lru_list.begin(), lru_list, it->second);
        return;
    }
    
    if(lru_list.size() >= max_capacity) {
        uint32_t victim_page = lru_list.back().vpn;
        lru_list.pop_back();
        tlb_map.erase(victim_page);
    }

    lru_list.push_front({vpn, frame});
    tlb_map[vpn] = lru_list.begin();
}

void TLB::invalidate(uint32_t vpn) {
    auto it = tlb_map.find(vpn);
    if(it != tlb_map.end()) {
        lru_list.erase(it->second);
        tlb_map.erase(it);
    }
}

void TLB::clear() {
    lru_list.clear();
    tlb_map.clear();
}