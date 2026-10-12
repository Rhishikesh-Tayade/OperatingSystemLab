# Memory Management Simulator — Theoretical Plan

## 1. Overview

This assignment requires building a **memory virtualization simulator** in C++ that models:
- Single-level page tables
- Two-level (multi-level) page tables
- Translation Lookaside Buffer (TLB)
- Page replacement with limited physical memory

The simulator reads **trace files** where each line is `<address>,<I/D>,<R/W>` and simulates address translation, measuring performance metrics.

---

## 2. System Parameters (Given)

| Parameter | Value |
|---|---|
| Virtual Address Space | 2^32 Bytes (4 GB) |
| Page Size | 4 KB (2^12 Bytes) |
| Number of Virtual Pages | 2^20 = 1,048,576 |
| TLB Entries | 128, fully associative, LRU |
| TLB Access Latency | 1 cycle |
| Memory Access Latency | 200 cycles |
| Disk Access Latency | 20,000,000 cycles (Part IV only) |

### Address Breakdown (32-bit virtual address)
- **Page Offset**: lower 12 bits (since 4 KB = 2^12)
- **Virtual Page Number (VPN)**: upper 20 bits

---

## 3. Part-by-Part Theoretical Plan

### Part I — Single-Level Page Table

**Goal**: Simulate a flat (single-level) page table. Frames are unlimited (more frames than pages).

#### Theory
A single-level page table is a flat array of 2^20 entries (one per virtual page). Each entry maps a VPN → frame number. Since we assume unlimited frames, every unique page seen gets a new frame — **no page faults occur**.

#### Frame Allocation Scheme
Use a **sequential (next-fit) allocator**:
- Maintain a counter `next_free_frame` starting at 0.
- When a new page is first accessed, assign `next_free_frame` to it and increment the counter.
- This is simple, deterministic, and avoids fragmentation concerns (since we have unlimited frames).

#### Metrics to Collect
- **A. Frames used** — simply the value of `next_free_frame` at the end.
- **B. Access distribution** — count accesses per page, sort descending, plot CDF (cumulative fraction of accesses vs. fraction of pages). This will show if a Pareto-like distribution holds (80/20 rule).
- **C. Total cycles** — for each access: 1 memory access to read the page table + 1 to access the actual data = 2 × 200 = **400 cycles per access** (no TLB yet).
- **D. Page table utilization** — (number of PT entries that have valid mappings) / (total PT entries allocated = 2^20).
- **E. Hash** — define `H = f(H, frame_number)` with a function like `H = H XOR (frame_number * some_prime) + frame_number` or a simple polynomial rolling hash.

---

### Part II — Two-Level Page Table

**Goal**: Replace the flat page table with a two-level structure. Same unlimited-frame assumption.

#### Theory
Split the 20-bit VPN into:
- **Level-1 index (outer)**: upper 10 bits → 1024 entries in the outer page table
- **Level-2 index (inner)**: lower 10 bits → 1024 entries per inner page table

The outer table always exists (1024 entries). Each inner table is **allocated on demand** — only when the first page in that range is accessed.

#### Key Difference from Part I
- **Space savings**: Only allocate inner page tables for VPN ranges actually used. Utilization should be much higher since we don't waste entries for untouched regions.
- **Extra memory access**: Each translation now requires 2 memory accesses (outer + inner) plus 1 for the actual data = 3 × 200 = **600 cycles per access** (no TLB).

#### Hash Verification
Since the VPN-to-frame mapping is **identical** (same allocation scheme, same order of accesses), the hash in Part II **must match** Part I. This serves as a correctness check.

---

### Part III — Adding TLB

**Goal**: Add a 128-entry fully associative TLB with LRU eviction to both Part I and Part II systems.

#### Theory
Before consulting the page table, check the TLB:
- **TLB Hit**: frame number found in TLB → cost = 1 (TLB) + 200 (memory access) = **201 cycles**
- **TLB Miss (single-level)**: consult page table → cost = 1 (TLB) + 200 (PT lookup) + 200 (memory access) = **401 cycles**
- **TLB Miss (two-level)**: consult two-level page table → cost = 1 (TLB) + 200 (L1 PT) + 200 (L2 PT) + 200 (memory access) = **601 cycles**

On a TLB miss, insert the mapping into TLB; if full (128 entries), evict the **Least Recently Used** entry.

#### Metrics
- **A. TLB hit rate** — `TLB_hits / total_accesses`
- **B. Cycles** — will be much lower than Parts I & II due to temporal/spatial locality in real benchmarks.

---

### Part IV — Limited Physical Memory + Page Replacement

**Goal**: Physical memory is now smaller than virtual memory. Pages may need to be swapped out.

#### Theory
With limited frames, when all frames are occupied and a new page must be loaded:
1. A **page fault** occurs.
2. Use **LRU replacement** to pick a victim frame.
3. Evict the victim page (if dirty, write back to disk — but assignment says count disk cost uniformly).
4. Load the new page into the freed frame.
5. Cost of page fault = 20,000,000 cycles (disk) + normal translation cost.
6. First-time page loads are **free** (no disk penalty until frames run out).

#### Experiment Sizes
| Multiplier | Physical Memory | Number of Frames |
|---|---|---|
| 0.5× | 2 GB | 524,288 |
| 0.25× | 1 GB | 262,144 |
| 0.125× | 512 MB | 131,072 |
| 0.0625× | 256 MB | 65,536 |
| ... | ... | ... |

#### Metrics
- **A. Page fault rate** — `page_faults / total_accesses`
- **B. Total cycles** — should increase dramatically as memory shrinks.

---

### Part V — Performance Optimization

**Goal**: Make the simulator as fast as possible (wall-clock time).

#### Optimization Strategies
1. **Read trace file in bulk** — use large I/O buffers or memory-mapped files.
2. **Parse addresses efficiently** — use `strtoul` or manual hex parsing instead of `std::istringstream`.
3. **Use arrays over maps** — flat arrays for page tables are O(1) and cache-friendly.
4. **TLB as array + LRU list** — use a doubly-linked list with a hash map for O(1) TLB lookup.
5. **Compile with `-O2` or `-O3`** optimizations.
6. **Avoid unnecessary allocations** — preallocate where possible.

---

## 4. Development Strategy

### Phase 1: Core Infrastructure
1. Build the trace file parser.
2. Implement the frame allocator.
3. Implement the hash function.

### Phase 2: Page Tables
4. Implement single-level page table (Part I).
5. Implement two-level page table (Part II).
6. Verify hashes match between Part I and Part II.

### Phase 3: TLB
7. Implement the TLB with LRU (Part III).
8. Integrate TLB with both page table types.

### Phase 4: Page Replacement
9. Implement LRU page replacement (Part IV).
10. Experiment with different memory sizes.

### Phase 5: Polish
11. Performance optimize (Part V).
12. Generate plots and write the report.

---

## 5. Submission Checklist
- [ ] Source code (modular `.cpp`/`.h` files)
- [ ] Makefile
- [ ] Report with graphs (access distribution, cycles vs. memory size, TLB hit rates, page fault rates)
- [ ] All packaged in a single `.zip` file
