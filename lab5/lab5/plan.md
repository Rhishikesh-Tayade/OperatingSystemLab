# Lab 5: Parallel Applications — Implementation Plan

## Overview
This lab involves parallelizing an image sharpening pipeline (S1→S2→S3) from Lab 1 using multiple IPC mechanisms on Linux.

---

## Folder Structure
```
lab5/
├── question.md
├── plan.md
├── report.tex
├── images/
│   └── input.ppm              # Copy a small test image from lab1
├── src/
│   ├── common/
│   │   ├── libppm.h           # PPM library header (from lab1)
│   │   └── libppm.cpp         # PPM library implementation (from lab1)
│   ├── part1/
│   │   └── image_sharpener.cpp  # Baseline sequential code (from lab1)
│   ├── part2_1/
│   │   ├── part2_1_variant1.cpp # Threads + atomic ops, variant 1
│   │   └── part2_1_variant2.cpp # Threads + atomic ops, variant 2
│   ├── part2_2/
│   │   ├── part2_2_variant1.cpp # Multi-process + pipes, variant 1
│   │   └── part2_2_variant2.cpp # Multi-process + pipes, variant 2
│   ├── part2_3/
│   │   ├── part2_3_variant1.cpp # Shared memory + semaphores, variant 1
│   │   └── part2_3_variant2.cpp # Shared memory + semaphores, variant 2
│   ├── part3_1/
│   │   ├── part3_1_variant1_A.cpp # S1+S2 on computer A (socket client)
│   │   ├── part3_1_variant1_B.cpp # S3 on computer B (socket server)
│   │   ├── part3_1_variant2_A.cpp
│   │   └── part3_1_variant2_B.cpp
│   └── part3_2/
│       ├── part3_2_variant1_A.cpp # S1 on computer A (socket client)
│       ├── part3_2_variant1_B.cpp # S2+S3 on computer B (socket server)
│       ├── part3_2_variant2_A.cpp
│       └── part3_2_variant2_B.cpp
└── Makefile                    # Top-level Makefile with all targets
```

---

## Step-by-Step Plan

### Step 1: Set up project structure
- Create the folder structure above.
- Copy `libppm.h`, `libppm.cpp` from lab1 to `src/common/`.
- Copy a test input image to `images/input.ppm`.

### Step 2: Part I — Baseline (Sequential)
- Copy `image_sharpener.cpp` from lab1 to `src/part1/`.
- Update it to use relative paths for `libppm.h`.
- Ensure `make part1` compiles and runs the baseline, outputting `output_part1.ppm`.

### Step 3: Part II.1 — Threads with Atomic Operations
**Architecture:** 3 threads in one process. Shared row buffers with `std::atomic` flags for row-level synchronization.

- **Variant 1 (pipeline, 1000 iterations):** Each iteration: S1 processes input → S2 processes S1's output → S3 processes S2's output. S1 always starts from the original input image. All 1000 iterations are pipelined across rows.
- **Variant 2 (batched, 1000×100×100):** S1 runs 1000 times; every 100th result passed to S2. S2 runs 100 times per result from S1; every 100th passed to S3. S3 runs 100 times per result from S2.
- Use `std::atomic<int>` row-completion counters for synchronization (no mutexes).
- Verify pixel ordering by checksumming row-by-row in producer and consumer.

### Step 4: Part II.2 — Processes with Pipes
**Architecture:** 3 processes connected via `pipe()`. Parent forks twice. Data flows: Process1 →pipe1→ Process2 →pipe2→ Process3.

- Serialize pixel data row-by-row through pipes.
- Each row: send width×3 bytes (RGB per pixel).
- **Variant 1 & Variant 2:** Same iteration logic as above.
- Verify ordering: embed row indices in the pipe stream; receiver checks sequential order.

### Step 5: Part II.3 — Shared Memory + Semaphores
**Architecture:** 3 processes sharing memory via `shmget()`/`shmat()`. Use POSIX semaphores (`sem_init` on shared memory) for synchronization.

- Shared memory layout: two image-sized buffers (double buffering) + atomic row counters.
- Semaphores signal row completion between stages.
- **Variant 1 & Variant 2:** Same iteration logic.
- Verify ordering: semaphore-enforced sequential row processing.

### Step 6: Part III — Distributed via Sockets
**Architecture:** TCP sockets connecting two machines. One acts as server, the other as client.

- **Part 3.1:** Computer A runs S1+S2, Computer B runs S3.
- **Part 3.2:** Computer A runs S1, Computer B runs S2+S3.
- Protocol: Send image dimensions first, then pixel data row-by-row.
- Both variants (1 and 2) of the iteration schemes.
- Command-line arguments: `<IP> <port> <input_image> <output_image>`.

### Step 7: Build the Makefile
Create a comprehensive Makefile with all required targets:
- `part1`, `part2_1_variant1`, `part2_1_variant2`, `part2_2_variant1`, `part2_2_variant2`, `part2_3_variant1`, `part2_3_variant2`
- `part3_1_variant1_A`, `part3_1_variant1_B`, `part3_1_variant2_A`, `part3_1_variant2_B`
- `part3_2_variant1_A`, `part3_2_variant1_B`, `part3_2_variant2_A`, `part3_2_variant2_B`

### Step 8: Write the Report (report.tex)
- Description of each approach
- Verification method for pixel ordering correctness
- Runtime measurements and speedup analysis
- Discussion of implementation difficulty

### Step 9: Testing & Validation
- Compile and run each part
- Verify output images match the baseline
- Collect timing data

---

## Key Design Decisions

1. **Row-level pipelining:** Instead of waiting for an entire stage to complete, we pipeline at the row granularity. As soon as S1 finishes row `i`, S2 can start processing row `i` (for rows that don't depend on `i+1`).

2. **Verification method:** Each stage computes a running CRC32/checksum of the pixel data it sends. The receiver independently computes the same checksum and compares. Row indices are also verified sequentially.

3. **Flat pixel buffer:** For IPC (pipes, shared memory, sockets), we use a flat `uint8_t` buffer (height × width × 3) instead of the triple-pointer structure, making serialization trivial.

4. **Linux-only APIs:** `fork()`, `pipe()`, `shmget()`, `sem_init()`, `socket()` — all POSIX/Linux APIs. The code is designed to run on Linux.
