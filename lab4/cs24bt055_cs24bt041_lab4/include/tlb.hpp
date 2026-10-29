#ifndef TLB_HPP 
#define TLB_HPP

#include<list>
#include<unordered_map>
#include<cstdint>
using namespace std;

class TLB {
private:
    struct TLBEntry {
        uint32_t vpn;
        uint32_t frame;
    };
    size_t max_capacity;
    list<TLBEntry> lru_list;
    unordered_map<uint32_t, list<TLBEntry>::iterator> tlb_map;

public:
    static const size_t DEFAULT_CAPACITY = 128;

    explicit TLB(size_t capacity = DEFAULT_CAPACITY);
    ~TLB();

    bool lookup(uint32_t vpn, uint32_t& frame_out);
    void insert(uint32_t vpn, uint32_t frame);
    void invalidate(uint32_t vpn);
    void clear();

    size_t size() const { return tlb_map.size(); }
    size_t capacity() const { return max_capacity; }
};

#endif