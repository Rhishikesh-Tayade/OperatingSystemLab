# Lab 5 - Pixel order proof and runtime study (method)

## S1 / S2 / S3 (from Lab 1, unchanged)

- S1 smoothen: border pixels copied, interior = average of 3x3 neighbourhood (integer div by 9).
- S2 details: `diff = input - smooth` stored as `(uint8_t)(int8_t)diff`.
- S3 sharpen: `input + (int8_t)details`, clamped to 0..255.
- All parallel versions use these scalar definitions exactly, so the final image is byte-identical to Part I (Variant 1 and Variant 2 are idempotent recomputations on the same input).

## Pixel-order proof (all parallel cases)

Every transmitted pixel is a fixed-size packet:

```
iter (int32), index (int32 pixel number), seq (int64 global sequence),
inp[3] (original pixel), payload[3] (smooth or details pixel)
```

- Part II.1 (threads): `seq` is implicit in the (iter, index) order. S2 spins on `smooth_ready[p]==iter` (acquire) and S3 on `det_ready[p]==iter`, processing pixels in strict 0..N-1 order. Atomic counters `g_verify_smooth` / `g_verify_det` count received pixels; the program exits 1 if the count is not N*1000 (Variant 1) or N*10 (Variant 2).
- Part II.2 (pipes) and Part II.3 (shared memory + semaphores): each packet carries an explicit `seq` starting at 0 and increasing by 1 per pixel. The receiver checks `pkt.seq == expect; expect++` for every packet. Any drop, duplication or reordering causes `ORDER FAIL` and immediate exit. On success the program prints `order verified: <count> packets received in sent order`.
- Part III (sockets): same `seq` check on the receiving computer (B). A sends `W,H` header first, then the packet stream. B verifies `seq` order before writing.

Streaming guarantee: pixels are forwarded as soon as they are ready (per-pixel/per-row), never waiting for the whole image. See the per-pixel loops in each source file.

## Runtime / speed-up study

Each program prints read / pipeline / write / total times (steady_clock, ms). Compare:

- Part I single-pass baseline (fastest per pass, but IO is once).
- Part II variants are compute-intensive: Variant 1 does 1000*3 stage passes, Variant 2 does 1000+1000+1000 stage passes nested (same totals, different forwarding). Expect negligible speed-up for the plain single-pass pipeline (read/write dominate), but measurable pipeline overlap in the 1000-iteration versions. Threads share memory (cheapest), pipes pay copy+syscalls, shared-memory pays sem wait/post, sockets additionally pay network.
- Part III splits S1(+S2) vs S3 across two hosts; the larger network payload (fewer forwarded packets in Variant 2) favours the partition with fewer packets over the wire.

## Ease / difficulty

- Threads + atomics: easiest to debug (single memory space, gdb shows all), spin-wait is the only subtle part.
- Pipes: easy to reason about (FIFO order), harder to debug (three address spaces, need to close fd ends correctly).
- Shared memory + semaphores: hardest (bounded-buffer indices, full/empty initial values, cleanup of shm/sem).
- Sockets: easiest to run on one host (127.0.0.1) once firewall allows the port; hardest to reproduce on two real machines.

## Build / run

See `Makefile` (`make help`). Part III: start `_A` (server) in one terminal, then `_B` (client) in another; on a single host use `HOST=127.0.0.1`.
