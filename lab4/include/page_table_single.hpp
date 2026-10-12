#ifndef PAGE_TABLE_SINGLE_HPP
#define PAGE_TABLE_SINGLE_HPP

#include<vector>
#include<cstdint>

using namespace std;

struct SingleLevelPTE {
    uint32_t frame_number = 0;
    bool valid = false;
    bool dirty = false;
};

class SingleLevelPageTable {
private:
    vector<SingleLevelPTE> entries;
    size_t valid_count = 0;
public:
    static const size_t NUM_PAGES = 1 << 20;
    SingleLevelPageTable();
    ~SingleLevelPageTable();
    bool lookup(uint32_t vpn, uint32_t& frame_out) const;
    void map(uint32_t vpn, uint32_t frame_number);
    void unmap(uint32_t vpn);
    void set_dirty(uint32_t vpn, bool dirty = true);

    bool is_valid(uint32_t vpn) const;
    bool is_dirty(uint32_t vpn) const;

    size_t get_allocated_entries() const { return NUM_PAGES; }
    size_t get_valid_entries_count() const { return valid_count; }

    void reset();
};


#endif 