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
static unique_ptr<atomic<int>[]> g_smooth_gen;
static unique_ptr<atomic<int>[]> g_det_gen;
static atomic<int> g_s2_consumed{0};
static atomic<int> g_s3_consumed{0};
static atomic<long long> g_verify_smooth{0};
static atomic<long long> g_verify_det{0};

static void s1_thread() {
    for (int run = 1; run <= 1000; run++) {
        bool fwd = (run % 100 == 0);
        int gen = fwd ? (run / 100) : 0;
        if (fwd && gen > 1) {
            while (g_s2_consumed.load(memory_order_acquire) < gen - 1) { }
        }
        for (int i = 0; i < g_H; i++) {
            for (int j = 0; j < g_W; j++) {
                size_t p = (size_t)i * g_W + j;
                for (int k = 0; k < 3; k++)
                    g_smooth[p * 3 + k] = s1_pixel(g_inp.data(), g_W, g_H, i, j, k);
                if (fwd) g_smooth_gen[p].store(gen, memory_order_release);
            }
        }
    }
}

static void s2_thread() {
    for (int g = 1; g <= 10; g++) {
        if (g > 1) {
            while (g_s3_consumed.load(memory_order_acquire) < g - 1) { }
        }
        for (int run = 1; run <= 100; run++) {
            bool fwd = (run == 100);
            for (int i = 0; i < g_H; i++) {
                for (int j = 0; j < g_W; j++) {
                    size_t p = (size_t)i * g_W + j;
                    if (run == 1) {
                        while (g_smooth_gen[p].load(memory_order_acquire) != g) { }
                    }
                    for (int k = 0; k < 3; k++)
                        g_det[p * 3 + k] = s2_pixel(g_inp[p * 3 + k], g_smooth[p * 3 + k]);
                    if (run == 1) g_verify_smooth.fetch_add(1, memory_order_release);
                    if (fwd) g_det_gen[p].store(g, memory_order_release);
                }
            }
        }
        g_s2_consumed.store(g, memory_order_release);
    }
}

static void s3_thread() {
    for (int g = 1; g <= 10; g++) {
        for (int run = 1; run <= 100; run++) {
            for (int i = 0; i < g_H; i++) {
                for (int j = 0; j < g_W; j++) {
                    size_t p = (size_t)i * g_W + j;
                    if (run == 1) {
                        while (g_det_gen[p].load(memory_order_acquire) != g) { }
                    }
                    for (int k = 0; k < 3; k++)
                        g_out[p * 3 + k] = s3_pixel(g_inp[p * 3 + k], g_det[p * 3 + k]);
                    if (run == 1) g_verify_det.fetch_add(1, memory_order_release);
                }
            }
        }
        g_s3_consumed.store(g, memory_order_release);
    }
}

static void write_flat(const char *path) {
    ofstream ws(path, ios::binary | ios::out);
    if (!ws.is_open()) { cerr << "failed to open file " << path << "\n"; exit(1); }
    ws.write("P6\n", 3);
    string w = to_string(g_W); ws.write(w.c_str(), (streamsize)w.length()); ws.write(" ", 1);
    string h = to_string(g_H); ws.write(h.c_str(), (streamsize)h.length());
    ws.write("\n255\n", 5);
    ws.write(reinterpret_cast<const char*>(g_out.data()), (streamsize)(g_N * 3));
    ws.close();
}

int main(int argc, char **argv) {
    if (argc != 3) { cout << "usage: ./part2_1_variant2 <input> <output>\n"; return 0; }
    auto t0 = chrono::steady_clock::now();
    image_t *img = read_ppm_file(argv[1]);
    g_W = img->width; g_H = img->height; g_N = (size_t)g_W * g_H;
    g_inp.resize(g_N * 3);
    for (int i = 0; i < g_H; i++)
        for (int j = 0; j < g_W; j++)
            for (int k = 0; k < 3; k++)
                g_inp[((size_t)i * g_W + j) * 3 + k] = img->image_pixels[i][j][k];
    auto t_read = chrono::steady_clock::now();
    g_smooth.resize(g_N * 3); g_det.resize(g_N * 3); g_out.resize(g_N * 3);
    g_smooth_gen = make_unique<atomic<int>[]>(g_N);
    g_det_gen = make_unique<atomic<int>[]>(g_N);
    for (size_t p = 0; p < g_N; p++) {
        g_smooth_gen[p].store(0, memory_order_relaxed);
        g_det_gen[p].store(0, memory_order_relaxed);
    }
    thread th1(s1_thread); thread th2(s2_thread); thread th3(s3_thread);
    th1.join(); th2.join(); th3.join();
    auto t_done = chrono::steady_clock::now();
    write_flat(argv[2]);
    auto t_write = chrono::steady_clock::now();
    long long expect = (long long)g_N * 10;
    if (g_verify_smooth.load() != expect || g_verify_det.load() != expect) {
        cerr << "ORDER CHECK FAILED: smooth=" << g_verify_smooth.load()
             << " det=" << g_verify_det.load() << " expect=" << expect << "\n";
        return 1;
    }
    cout << "order verified: " << expect << " forwarded pixels received in sent order (S1->S2 and S2->S3)\n";
    cout << "read: " << chrono::duration_cast<chrono::milliseconds>(t_read - t0).count() << " ms\n";
    cout << "pipeline (S1x1000 S2x1000 S3x1000 nested, atomics): " << chrono::duration_cast<chrono::milliseconds>(t_done - t_read).count() << " ms\n";
    cout << "write: " << chrono::duration_cast<chrono::milliseconds>(t_write - t_done).count() << " ms\n";
    cout << "total: " << chrono::duration_cast<chrono::milliseconds>(t_write - t0).count() << " ms\n";
    return 0;
}
