// Part2_1 Variant1: 3 threads, atomic operations, variant 1.
// Pipeline: S1 (smoothen) -> S2 (details) -> S3 (sharpen).
// Variant 1: 1000 linked iterations. Input image is read once.
// Iteration r: S1_r(input) -> S2_r(input, S1_r) -> S3_r(input, S2_r).
// S3_1000 result is written. Since S1/S2/S3 are deterministic on the
// same input, the final output equals the Part I output.
// Streaming: pixels are forwarded per-pixel as soon as they are ready,
// never waiting for the whole image.
// Order proof: every pixel carries (iter, index). S2 spins until
// smooth_ready[p]==iter (acquire) and S3 until det_ready[p]==iter,
// processing pixels in strict 0..N-1 order. Atomic verify counters
// count pixels received; main thread checks they equal N*1000.
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include "libppm.h"

using namespace std;

static inline uint8_t s1_pixel(const uint8_t *inp, int W, int H, int i, int j, int k) {
    if (i == 0 || i == H - 1 || j == 0 || j == W - 1)
        return inp[(i * W + j) * 3 + k];
    int t = 0;
    for (int x = i - 1; x < i + 2; x++)
        for (int y = j - 1; y < j + 2; y++)
            t += inp[(x * W + y) * 3 + k];
    return (uint8_t)(t / 9);
}

static inline uint8_t s2_pixel(uint8_t inp, uint8_t smo) {
    int diff = (int)inp - (int)smo;
    return (uint8_t)(int8_t)diff;
}

static inline uint8_t s3_pixel(uint8_t inp, uint8_t det) {
    int v = (int)inp + (int8_t)det;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

static vector<uint8_t> g_inp;
static vector<uint8_t> g_smooth;
static vector<uint8_t> g_det;
static vector<uint8_t> g_out;
static int g_W = 0, g_H = 0;
static size_t g_N = 0;
static unique_ptr<atomic<int>[]> g_smooth_ready;
static unique_ptr<atomic<int>[]> g_det_ready;
static atomic<int> g_s2_done{0};
static atomic<int> g_s3_done{0};
static atomic<long long> g_verify_smooth{0};
static atomic<long long> g_verify_det{0};
static const int kIters = 1000;

static void s1_thread() {
    for (int iter = 1; iter <= kIters; iter++) {
        // iteration barrier: do not overwrite smooth buffer until S2 finished prev iter
        while (g_s2_done.load(memory_order_acquire) < iter - 1) {
        }
        for (int i = 0; i < g_H; i++) {
            for (int j = 0; j < g_W; j++) {
                size_t p = (size_t)i * g_W + j;
                for (int k = 0; k < 3; k++) {
                    g_smooth[p * 3 + k] = s1_pixel(g_inp.data(), g_W, g_H, i, j, k);
                }
                g_smooth_ready[p].store(iter, memory_order_release);
            }
        }
    }
}

static void s2_thread() {
    for (int iter = 1; iter <= kIters; iter++) {
        // iteration barrier: do not overwrite det buffer until S3 finished prev iter
        while (g_s3_done.load(memory_order_acquire) < iter - 1) {
        }
        for (int i = 0; i < g_H; i++) {
            for (int j = 0; j < g_W; j++) {
                size_t p = (size_t)i * g_W + j;
                while (g_smooth_ready[p].load(memory_order_acquire) != iter) {
                    // spin wait - atomic synchronization
                }
                for (int k = 0; k < 3; k++) {
                    g_det[p * 3 + k] = s2_pixel(g_inp[p * 3 + k], g_smooth[p * 3 + k]);
                }
                g_verify_smooth.fetch_add(1, memory_order_release);
                g_det_ready[p].store(iter, memory_order_release);
            }
        }
        g_s2_done.store(iter, memory_order_release);
    }
}

static void s3_thread() {
    for (int iter = 1; iter <= kIters; iter++) {
        for (int i = 0; i < g_H; i++) {
            for (int j = 0; j < g_W; j++) {
                size_t p = (size_t)i * g_W + j;
                while (g_det_ready[p].load(memory_order_acquire) != iter) {
                }
                for (int k = 0; k < 3; k++) {
                    g_out[p * 3 + k] = s3_pixel(g_inp[p * 3 + k], g_det[p * 3 + k]);
                }
                g_verify_det.fetch_add(1, memory_order_release);
            }
        }
        g_s3_done.store(iter, memory_order_release);
    }
}

static void write_flat(const char *path) {
    ofstream ws(path, ios::binary | ios::out);
    if (!ws.is_open()) { cerr << "failed to open file " << path << "\n"; exit(1); }
    ws.write("P6\n", 3);
    string w = to_string(g_W);
    ws.write(w.c_str(), (streamsize)w.length());
    ws.write(" ", 1);
    string h = to_string(g_H);
    ws.write(h.c_str(), (streamsize)h.length());
    ws.write("\n255\n", 5);
    ws.write(reinterpret_cast<const char*>(g_out.data()), (streamsize)(g_N * 3));
    ws.close();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        cout << "usage: ./part2_1_variant1 <path-to-original-image> <path-to-transformed-image>\n";
        return 0;
    }
    auto t0 = chrono::steady_clock::now();
    image_t *img = read_ppm_file(argv[1]);
    g_W = img->width;
    g_H = img->height;
    g_N = (size_t)g_W * g_H;
    g_inp.resize(g_N * 3);
    for (int i = 0; i < g_H; i++)
        for (int j = 0; j < g_W; j++)
            for (int k = 0; k < 3; k++)
                g_inp[((size_t)i * g_W + j) * 3 + k] = img->image_pixels[i][j][k];
    auto t_read = chrono::steady_clock::now();

    g_smooth.resize(g_N * 3);
    g_det.resize(g_N * 3);
    g_out.resize(g_N * 3);
    g_smooth_ready = make_unique<atomic<int>[]>(g_N);
    g_det_ready = make_unique<atomic<int>[]>(g_N);
    for (size_t p = 0; p < g_N; p++) {
        g_smooth_ready[p].store(0, memory_order_relaxed);
        g_det_ready[p].store(0, memory_order_relaxed);
    }

    thread th1(s1_thread);
    thread th2(s2_thread);
    thread th3(s3_thread);
    th1.join(); th2.join(); th3.join();
    auto t_done = chrono::steady_clock::now();

    write_flat(argv[2]);
    auto t_write = chrono::steady_clock::now();

    long long expect = (long long)g_N * kIters;
    if (g_verify_smooth.load() != expect || g_verify_det.load() != expect) {
        cerr << "ORDER CHECK FAILED: smooth=" << g_verify_smooth.load()
             << " det=" << g_verify_det.load() << " expect=" << expect << "\n";
        return 1;
    }
    cout << "order verified: " << expect << " pixels received in sent order (S1->S2 and S2->S3)\n";
    cout << "read: " << chrono::duration_cast<chrono::milliseconds>(t_read - t0).count() << " ms\n";
    cout << "pipeline (1000 iters, 3 threads, atomics): " << chrono::duration_cast<chrono::milliseconds>(t_done - t_read).count() << " ms\n";
    cout << "write: " << chrono::duration_cast<chrono::milliseconds>(t_write - t_done).count() << " ms\n";
    cout << "total: " << chrono::duration_cast<chrono::milliseconds>(t_write - t0).count() << " ms\n";
    return 0;
}
