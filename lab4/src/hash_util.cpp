#include "hash_util.hpp"

uint64_t update_hash(uint64_t H, uint32_t frame_number) {
    H ^= (static_cast<uint64_t>(frame_number) * 2654435761ULL);
    H = (H << 13) | (H >> 51);
    H += frame_number;
    return H;
}
