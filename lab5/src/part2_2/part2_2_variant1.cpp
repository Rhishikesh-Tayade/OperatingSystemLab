#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include "libppm.h"

using namespace std;

struct Packet {
    int32_t iter;
    int32_t index;
    int64_t seq;
    uint8_t inp[3];
    uint8_t payload[3];
};

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

static bool write_all(int fd, const void *buf, size_t n) {
    const char *p = (const char*)buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return false;
        p += w; n -= (size_t)w;
    }
    return true;
}
static bool read_all(int fd, void *buf, size_t n) {
    char *p = (char*)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r <= 0) return false;
        got += (size_t)r;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) { cout << "usage: ./part2_2_variant1 <input> <output>\n"; return 0; }
    auto t0 = chrono::steady_clock::now();
    image_t *img = read_ppm_file(argv[1]);
    int W = img->width, H = img->height;
    size_t N = (size_t)W * H;
    vector<uint8_t> inp(N * 3);
    for (int i = 0; i < H; i++)
        for (int j = 0; j < W; j++)
            for (int k = 0; k < 3; k++)
                inp[((size_t)i * W + j) * 3 + k] = img->image_pixels[i][j][k];
    auto t_read = chrono::steady_clock::now();

    int pipe1[2], pipe2[2];
    if (pipe(pipe1) != 0 || pipe(pipe2) != 0) { perror("pipe"); return 1; }

    pid_t pid1 = fork();
    if (pid1 < 0) { perror("fork1"); return 1; }
    if (pid1 == 0) {

        close(pipe1[0]); close(pipe2[0]); close(pipe2[1]);
        int64_t seq = 0;
        for (int iter = 1; iter <= 1000; iter++) {
            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {
                    size_t p = (size_t)i * W + j;
                    Packet pkt;
                    pkt.iter = iter;
                    pkt.index = (int32_t)p;
                    pkt.seq = seq++;
                    for (int k = 0; k < 3; k++) {
                        pkt.inp[k] = inp[p * 3 + k];
                        pkt.payload[k] = s1_pixel(inp.data(), W, H, i, j, k);
                    }
                    if (!write_all(pipe1[1], &pkt, sizeof(pkt))) _exit(1);
                }
            }
        }
        close(pipe1[1]);
        _exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) { perror("fork2"); return 1; }
    if (pid2 == 0) {

        close(pipe1[1]); close(pipe2[0]);
        int64_t expect = 0;
        for (int iter = 1; iter <= 1000; iter++) {
            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {
                    Packet pkt;
                    if (!read_all(pipe1[0], &pkt, sizeof(pkt))) _exit(1);
                    if (pkt.seq != expect) {
                        cerr << "S2 ORDER FAIL seq=" << pkt.seq << " expect=" << expect << "\n";
                        _exit(1);
                    }
                    expect++;
                    Packet out;
                    out.iter = pkt.iter; out.index = pkt.index; out.seq = pkt.seq;
                    for (int k = 0; k < 3; k++) {
                        out.inp[k] = pkt.inp[k];
                        out.payload[k] = s2_pixel(pkt.inp[k], pkt.payload[k]);
                    }
                    if (!write_all(pipe2[1], &out, sizeof(out))) _exit(1);
                }
            }
        }
        close(pipe1[0]); close(pipe2[1]);
        _exit(0);
    }

    close(pipe1[0]); close(pipe1[1]); close(pipe2[1]);
    vector<uint8_t> out(N * 3, 0);
    int64_t expect = 0;
    for (int iter = 1; iter <= 1000; iter++) {
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                Packet pkt;
                if (!read_all(pipe2[0], &pkt, sizeof(pkt))) { cerr << "S3 read fail\n"; return 1; }
                if (pkt.seq != expect) { cerr << "S3 ORDER FAIL seq=" << pkt.seq << " expect=" << expect << "\n"; return 1; }
                expect++;
                size_t p = (size_t)i * W + j;
                for (int k = 0; k < 3; k++) {
                    uint8_t v = s3_pixel(pkt.inp[k], pkt.payload[k]);
                    if (iter == 1000) out[p * 3 + k] = v;
                }
            }
        }
    }
    close(pipe2[0]);
    int st1, st2;
    waitpid(pid1, &st1, 0); waitpid(pid2, &st2, 0);
    if (!WIFEXITED(st1) || WEXITSTATUS(st1) != 0 || !WIFEXITED(st2) || WEXITSTATUS(st2) != 0) {
        cerr << "child failed p1=" << WEXITSTATUS(st1) << " p2=" << WEXITSTATUS(st2) << "\n";
        return 1;
    }
    auto t_done = chrono::steady_clock::now();
    ofstream ws(argv[2], ios::binary | ios::out);
    if (!ws.is_open()) { cerr << "failed to open file " << argv[2] << "\n"; return 1; }
    ws.write("P6\n", 3);
    string w = to_string(W); ws.write(w.c_str(), (streamsize)w.length()); ws.write(" ", 1);
    string h = to_string(H); ws.write(h.c_str(), (streamsize)h.length());
    ws.write("\n255\n", 5);
    ws.write(reinterpret_cast<const char*>(out.data()), (streamsize)(N * 3));
    ws.close();
    auto t_write = chrono::steady_clock::now();
    long long total = (long long)N * 1000;
    cout << "order verified: " << total << " packets received in sent order (S1->S2, S2->S3, seq checked)\n";
    cout << "read: " << chrono::duration_cast<chrono::milliseconds>(t_read - t0).count() << " ms\n";
    cout << "pipeline (1000 iters, 3 processes, pipes): " << chrono::duration_cast<chrono::milliseconds>(t_done - t_read).count() << " ms\n";
    cout << "write: " << chrono::duration_cast<chrono::milliseconds>(t_write - t_done).count() << " ms\n";
    cout << "total: " << chrono::duration_cast<chrono::milliseconds>(t_write - t0).count() << " ms\n";
    return 0;
}
