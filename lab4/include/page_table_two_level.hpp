#ifndef PAGE_TABLE_TWO_LEVEL_HPP
#define PAGE_TABLE_TWO_LEVEL_HPP

#include<cstdint>
#include<vector>
using namespace std;

struct TwoLevelPTE {
    uint32_t frame_number = 0;
    bool dirty = false;
    bool valid = false;
};

class TwoLevelPageTable {
public:
    static const size_t L1_ENTRIES = 1 << 10;
    static const size_t L2_ENTRIES = 1 << 10;
    static const size_t NUM_PAGES = 1 << 20;

    TwoLevelPageTable();
    ~TwoLevelPageTable();

    bool lookup(uint32_t vpn, uint32_t& frame_out) const;
    void map(uint32_t vpn, uint32_t frame);
    void unmap(uint32_t vpn);
    void set_dirty(uint32_t vpn, bool dirty = true);

    bool is_valid(uint32_t vpn) const;
    bool is_dirty(uint32_t vpn) const;

    size_t get_allocated_entries() const;
    size_t get_valid_entries_count() const { return valid_count; }
    size_t get_l2_tables_count() const { return l2_tables_count; }
    void reset();

private:
    struct Level2Table {
        TwoLevelPTE entries[L2_ENTRIES];
    };

    Level2Table* l1_dir[L1_ENTRIES];
    size_t l2_tables_count = 0;
    size_t valid_count = 0;
};


#endif