#ifndef FRAME_ALLOCATOR_HPP
#define FRAME_ALLOCATOR_HPP

#include <cstdint>
#include <vector>
#include <list>
#include <unordered_map>

using namespace std;

class FrameAllocator {
public:
    // max_frames = 0 means unlimited frames (Parts I, II, III).
    // max_frames > 0 means limited memory with LRU replacement (Part IV).
    explicit FrameAllocator(size_t max_frames = 0);
    ~FrameAllocator();

    uint32_t allocate_frame(uint32_t vpn, bool& page_fault_occurred, uint32_t& victim_vpn);
    void touch_frame(uint32_t frame);

    size_t get_frames_used() const { return frames_used_count; }
    size_t get_max_frames() const { return max_frames; }
    bool is_limited() const { return max_frames > 0; }
    void reset();

private:
    size_t max_frames;
    uint32_t next_free_frame = 0;
    size_t frames_used_count = 0;

    struct FrameNode {
        uint32_t frame_number;
        uint32_t vpn;
    };

    list<FrameNode> lru_list; // Front = MRU, Back = LRU
    unordered_map<uint32_t, list<FrameNode>::iterator> frame_to_lru_map;
    vector<uint32_t> frame_to_vpn;
};

#endif
