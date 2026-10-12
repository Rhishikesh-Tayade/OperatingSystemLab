# Memory Management Simulator — Data Structures & Workflow Logic

## 1. Core Data Structures

### 1.1 Trace Entry (Input)
```cpp
struct TraceEntry {
    uint32_t address;       // 32-bit virtual address (lower 32 bits from file)
    char     type;          // 'I' = instruction, 'D' = data
    char     rw;            // 'R' = read, 'W' = write
};
```
**Derived fields** (computed, not stored):
- `vpn = address >> 12` — upper 20 bits = Virtual Page Number
- `offset = address & 0xFFF` — lower 12 bits = page offset (not needed for simulation)

---

### 1.2 Page Table Entry (PTE)
```cpp
struct PageTableEntry {
    uint32_t frame_number;  // mapped physical frame
    bool     valid;         // is this entry mapped?
    bool     dirty;         // has this page been written to? (useful for Part IV)
    uint64_t last_access;   // timestamp of last access (for LRU in Part IV)
};
```

---

### 1.3 Single-Level Page Table
```cpp
// Flat array of 2^20 = 1,048,576 entries
PageTableEntry page_table[1 << 20];  // ~16-20 MB depending on PTE size
```
- **Lookup**: `page_table[vpn]` → O(1)
- **Allocation**: All entries pre-allocated, `valid = false` initially

---

### 1.4 Two-Level Page Table
```cpp
// Outer table: 1024 pointers to inner tables
PageTableEntry* outer_table[1 << 10];  // 1024 entries, each is a pointer (or nullptr)

// Each inner table: 1024 PTEs (allocated on demand)
// outer_table[vpn >> 10] points to an array of 1024 PageTableEntry
```
- **L1 index**: `vpn >> 10` (upper 10 bits of VPN)
- **L2 index**: `vpn & 0x3FF` (lower 10 bits of VPN)
- **Lookup**:
  ```
  inner = outer_table[vpn >> 10]
  if inner == nullptr → allocate new inner table
  entry = inner[vpn & 0x3FF]
  ```

---

### 1.5 TLB (Translation Lookaside Buffer)
```cpp
struct TLBEntry {
    uint32_t vpn;
    uint32_t frame_number;
    bool     valid;
};
```

**TLB organization** — 128 entries, fully associative, LRU:

```cpp
// Option A: Ordered list (simplest for LRU)
std::list<std::pair<uint32_t, uint32_t>> tlb_list;        // ordered by recency (MRU at front)
std::unordered_map<uint32_t, std::list<...>::iterator> tlb_map;  // VPN → position in list

// Option B: Array + access timestamp
TLBEntry tlb[128];
uint64_t tlb_timestamp[128];  // for finding LRU victim
```

**Recommended: Option A** — `std::list` + `std::unordered_map` gives O(1) lookup, O(1) promote-to-MRU, and O(1) eviction.

---

### 1.6 Frame Allocator
```cpp
// For Parts I–III (unlimited frames):
uint32_t next_free_frame = 0;  // simple sequential counter

// For Part IV (limited frames):
struct FrameInfo {
    uint32_t vpn;           // which page occupies this frame
    bool     occupied;
    uint64_t last_access;   // for LRU replacement
};
std::vector<FrameInfo> frames(num_frames);

// LRU tracking for page replacement (Part IV):
std::list<uint32_t> lru_list;                                    // frame numbers ordered by recency
std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lru_map;  // frame → iterator
```

---

### 1.7 Statistics Counters
```cpp
struct SimStats {
    uint64_t total_accesses      = 0;
    uint64_t total_cycles        = 0;
    uint64_t frames_used         = 0;
    uint64_t tlb_hits            = 0;
    uint64_t tlb_misses          = 0;
    uint64_t page_faults         = 0;
    uint64_t pt_entries_accessed = 0;  // unique PTEs touched
    uint64_t pt_entries_allocated= 0;  // total PTEs allocated
    uint64_t hash_value          = 0;  // running hash H

    // Per-page access count (for hypothesis B plot)
    std::unordered_map<uint32_t, uint64_t> page_access_count;
};
```

---

### 1.8 Hash Function
```cpp
// Chosen hash function: XOR-shift with prime multiplication
uint64_t update_hash(uint64_t H, uint32_t frame_number) {
    H ^= (uint64_t)frame_number * 2654435761ULL;  // Knuth's multiplicative hash constant
    H = (H << 13) | (H >> 51);                     // rotate left by 13
    H += frame_number;
    return H;
}
```
**Rationale**: Combines XOR, multiplication by a prime (good avalanche), rotation (mixes bits), and addition. Simple, fast, and produces distinct hashes for different access sequences.

---

## 2. Workflow / Control Flow

### 2.1 Main Simulation Loop (High-Level)

```
INITIALIZE page_table, tlb, frame_allocator, stats
OPEN trace_file

FOR EACH line in trace_file:
    PARSE → (address, type, rw)
    vpn = address >> 12
    stats.total_accesses++

    // Step 1: TLB Lookup (if TLB enabled, i.e., Part III)
    IF tlb_enabled:
        cycles += 1  // TLB access cost
        IF TLB_LOOKUP(vpn) → hit:
            frame = tlb_result
            stats.tlb_hits++
            GOTO step3
        ELSE:
            stats.tlb_misses++
            // Fall through to page table

    // Step 2: Page Table Lookup
    IF single_level:
        cycles += 200  // 1 memory access for PT lookup
        entry = page_table[vpn]
    ELSE IF two_level:
        cycles += 200  // L1 lookup
        cycles += 200  // L2 lookup
        entry = two_level_lookup(vpn)

    IF entry.valid == false:
        // Page fault (only matters in Part IV with limited frames)
        IF limited_memory AND all_frames_occupied:
            EVICT victim using LRU
            cycles += 20,000,000  // disk access penalty
        frame = ALLOCATE_FRAME(vpn)
        entry.frame_number = frame
        entry.valid = true
    ELSE:
        frame = entry.frame_number

    // Insert into TLB (if enabled)
    IF tlb_enabled:
        TLB_INSERT(vpn, frame)  // evict LRU if full

step3:
    // Step 3: Access memory
    cycles += 200  // actual memory access

    // Step 4: Update stats
    stats.total_cycles += cycles
    stats.hash_value = update_hash(stats.hash_value, frame)
    stats.page_access_count[vpn]++

    // Step 5: Update LRU (Part IV)
    IF limited_memory:
        UPDATE_LRU(frame)

PRINT stats
```

---

### 2.2 TLB Operations Detail

```
TLB_LOOKUP(vpn):
    IF vpn in tlb_map:
        Move entry to front of tlb_list (mark as MRU)
        RETURN (hit, frame_number)
    ELSE:
        RETURN (miss)

TLB_INSERT(vpn, frame):
    IF vpn in tlb_map:
        // Already exists — just update & promote
        Update frame, move to front
    ELSE:
        IF tlb_list.size() == 128:
            // Evict LRU (back of list)
            Remove last entry from tlb_list
            Remove from tlb_map
        Add (vpn, frame) to front of tlb_list
        Add to tlb_map
```

---

### 2.3 Two-Level Page Table Lookup Detail

```
TWO_LEVEL_LOOKUP(vpn):
    l1_index = vpn >> 10        // upper 10 bits
    l2_index = vpn & 0x3FF      // lower 10 bits

    IF outer_table[l1_index] == nullptr:
        outer_table[l1_index] = new PageTableEntry[1024]  // allocate inner table
        stats.pt_entries_allocated += 1024

    RETURN &outer_table[l1_index][l2_index]
```

---

### 2.4 LRU Page Replacement (Part IV) Detail

```
ALLOCATE_FRAME(vpn):
    IF next_free_frame < max_frames:
        frame = next_free_frame++
        frames[frame].vpn = vpn
        Add frame to front of lru_list
    ELSE:
        // All frames used — evict LRU
        victim_frame = lru_list.back()       // least recently used
        victim_vpn = frames[victim_frame].vpn

        // Invalidate victim in page table
        page_table[victim_vpn].valid = false

        // Invalidate victim in TLB (if present)
        TLB_INVALIDATE(victim_vpn)

        // Reuse frame
        frame = victim_frame
        Remove from back of lru_list
        Add to front of lru_list
        frames[frame].vpn = vpn

        stats.page_faults++

    RETURN frame

UPDATE_LRU(frame):
    Move frame to front of lru_list  // mark as most recently used
```

---

## 3. Cycle Cost Summary Table

| Scenario | TLB Hit | TLB Miss (1-Level) | TLB Miss (2-Level) | Page Fault Extra |
|---|---|---|---|---|
| No TLB, 1-Level PT | N/A | 200 (PT) + 200 (mem) = **400** | N/A | N/A |
| No TLB, 2-Level PT | N/A | N/A | 200+200 (PT) + 200 (mem) = **600** | N/A |
| With TLB, 1-Level PT | 1 + 200 = **201** | 1 + 200 + 200 = **401** | N/A | N/A |
| With TLB, 2-Level PT | 1 + 200 = **201** | N/A | 1 + 200+200 + 200 = **601** | N/A |
| Part IV (any + page fault) | — | — | — | + **20,000,000** |

---

## 4. File / Module Structure

```
lab4/
├── Makefile
├── src/
│   ├── main.cpp              # Entry point, argument parsing, orchestration
│   ├── trace_parser.h/.cpp   # Reads and parses trace files
│   ├── page_table.h/.cpp     # Single-level page table class
│   ├── page_table_2l.h/.cpp  # Two-level page table class
│   ├── tlb.h/.cpp            # TLB class (fully associative, LRU)
│   ├── frame_alloc.h/.cpp    # Frame allocator + LRU replacement
│   ├── hash.h/.cpp           # Hash update function
│   └── stats.h/.cpp          # Statistics collection and reporting
├── traces/                   # Benchmark trace files
└── report/
    └── report.pdf            # Graphs + analysis
```

---

## 5. Key Invariant / Correctness Check

> **Part I hash == Part II hash**

Both use the same sequential frame allocator and process the same trace in the same order. The only difference is the page table structure (flat vs. two-level), which should not change the VPN → frame mapping. If hashes differ, there is a bug.
