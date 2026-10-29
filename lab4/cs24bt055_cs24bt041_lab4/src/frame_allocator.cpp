#include "frame_allocator.hpp"

using namespace std;

FrameAllocator::FrameAllocator(size_t max_frames) : max_frames(max_frames) {
    next_free_frame = 0;
    frames_used_count = 0;
    if (max_frames > 0) {
        frame_to_vpn.resize(max_frames, 0);
    }
}

FrameAllocator::~FrameAllocator() {}

uint32_t FrameAllocator::allocate_frame(uint32_t vpn, bool& page_fault_occurred, uint32_t& victim_vpn) {
    page_fault_occurred = false;
    victim_vpn = 0;

    // Unlimited mode (Parts I, II, III)
    if (max_frames == 0) {
        uint32_t frame = next_free_frame++;
        frames_used_count++;
        return frame;
    }

    // Limited mode: free frames still available
    if (next_free_frame < max_frames) {
        uint32_t frame = next_free_frame++;
        frames_used_count = next_free_frame;
        frame_to_vpn[frame] = vpn;
        lru_list.push_front({frame, vpn});
        frame_to_lru_map[frame] = lru_list.begin();
        return frame;
    }

    // Limited mode: all frames occupied -> evict LRU
    page_fault_occurred = true;
    FrameNode victim = lru_list.back();
    uint32_t victim_frame = victim.frame_number;
    victim_vpn = victim.vpn;

    // Remove victim from LRU tracking
    frame_to_lru_map.erase(victim_frame);
    lru_list.pop_back();

    // Reassign frame to new page
    frame_to_vpn[victim_frame] = vpn;
    lru_list.push_front({victim_frame, vpn});
    frame_to_lru_map[victim_frame] = lru_list.begin();

    return victim_frame;
}

void FrameAllocator::touch_frame(uint32_t frame) {
    auto it = frame_to_lru_map.find(frame);
    if (it != frame_to_lru_map.end()) {
        lru_list.splice(lru_list.begin(), lru_list, it->second);
    }
}

void FrameAllocator::reset() {
    next_free_frame = 0;
    frames_used_count = 0;
    lru_list.clear();
    frame_to_lru_map.clear();
    if (max_frames > 0) {
        fill(frame_to_vpn.begin(), frame_to_vpn.end(), 0);
    }
}
