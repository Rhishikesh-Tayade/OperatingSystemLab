// Part2_3 Variant2: 3 processes, shared memory, semaphores, variant 2.
// S1 1000 runs -> 10 forwarded (10xN packets). S2 100 per forwarded ->
// 10 forwarded. S3 100 per forwarded -> last written. Sync via semaphores only.
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
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

const int CAP = 1024;
struct Shm {
    sem_t full1, empty1;
    sem_t full2, empty2;
    int head1, tail1;
    int head2, tail2;
    Packet buf1[1024];
    Packet buf2[1024];
};

static void q1_push(Shm *s, const Packet &p) { sem_wait(&s->empty1); s->buf1[s->tail1] = p; s->tail1 = (s->tail1 + 1) % CAP; sem_post(&s->full1); }
static void q1_pop(Shm *s, Packet &p) { sem_wait(&s->full1); p = s->buf1[s->head1]; s->head1 = (s->head1 + 1) % CAP; sem_post(&s->empty1); }
static void q2_push(Shm *s, const Packet &p) { sem_wait(&s->empty2); s->buf2[s->tail2] = p; s->tail2 = (s->tail2 + 1) % CAP; sem_post(&s->full2); }
static void q2_pop(Shm *s, Packet &p) { sem_wait(&s->full2); p = s->buf2[s->head2]; s->head2 = (s->head2 + 1) % CAP; sem_post(&s->empty2); }

int main(int argc, char **argv) {
    if (argc != 3) { cout << "usage: ./part2_3_variant2 <input> <output>\n"; return 0; }
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

    char name[64];
    snprintf(name, sizeof(name), "/oslab5_shm_%ld", (long)getpid());
    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return 1; }
    if (ftruncate(fd, sizeof(Shm)) != 0) { perror("ftruncate"); return 1; }
    Shm *shm = (Shm*)mmap(NULL, sizeof(Shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); return 1; }
    close(fd);
    memset(shm, 0, sizeof(Shm));
    sem_init(&shm->full1, 1, 0); sem_init(&shm->empty1, 1, CAP);
    sem_init(&shm->full2, 1, 0); sem_init(&shm->empty2, 1, CAP);
    shm->head1 = shm->tail1 = shm->head2 = shm->tail2 = 0;

    pid_t pid1 = fork();
    if (pid1 < 0) { perror("fork1"); return 1; }
    if (pid1 == 0) {
        int64_t seq = 0;
        for (int run = 1; run <= 1000; run++) {
            bool fwd = (run % 100 == 0);
            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {
                    size_t p = (size_t)i * W + j;
                    uint8_t smo[3];
                    for (int k = 0; k < 3; k++) smo[k] = s1_pixel(inp.data(), W, H, i, j, k);
                    if (fwd) {
                        Packet pkt; pkt.iter = run; pkt.index = (int32_t)p; pkt.seq = seq++;
                        for (int k = 0; k < 3; k++) { pkt.inp[k] = inp[p * 3 + k]; pkt.payload[k] = smo[k]; }
                        q1_push(shm, pkt);
                    }
                }
            }
        }
        _exit(0);
    }
    pid_t pid2 = fork();
    if (pid2 < 0) { perror("fork2"); return 1; }
    if (pid2 == 0) {
        int64_t expect = 0;
        for (int g = 1; g <= 10; g++) {
            vector<Packet> group(N);
            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {
                    Packet pkt; q1_pop(shm, pkt);
                    if (pkt.seq != expect) { cerr << "S2 ORDER FAIL seq=" << pkt.seq << " expect=" << expect << "\n"; _exit(1); }
                    expect++;
                    size_t p = (size_t)i * W + j;
                    group[p] = pkt;
                }
            }
            for (int run = 1; run <= 100; run++) {
                bool fwd = (run == 100);
                for (int i = 0; i < H; i++) {
                    for (int j = 0; j < W; j++) {
                        size_t p = (size_t)i * W + j;
                        Packet out; out.iter = g; out.index = group[p].index; out.seq = group[p].seq;
                        for (int k = 0; k < 3; k++) {
                            out.inp[k] = group[p].inp[k];
                            out.payload[k] = s2_pixel(group[p].inp[k], group[p].payload[k]);
                        }
                        if (fwd) q2_push(shm, out);
                    }
                }
            }
        }
        _exit(0);
    }

    vector<uint8_t> out(N * 3, 0);
    int64_t expect = 0;
    for (int g = 1; g <= 10; g++) {
        vector<Packet> group(N);
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                Packet pkt; q2_pop(shm, pkt);
                if (pkt.seq != expect) { cerr << "S3 ORDER FAIL seq=" << pkt.seq << " expect=" << expect << "\n"; return 1; }
                expect++;
                size_t p = (size_t)i * W + j;
                group[p] = pkt;
            }
        }
        for (int run = 1; run <= 100; run++) {
            for (int i = 0; i < H; i++) {
                for (int j = 0; j < W; j++) {
                    size_t p = (size_t)i * W + j;
                    for (int k = 0; k < 3; k++) {
                        uint8_t v = s3_pixel(group[p].inp[k], group[p].payload[k]);
                        if (g == 10 && run == 100) out[p * 3 + k] = v;
                    }
                }
            }
        }
    }
    int st1, st2;
    waitpid(pid1, &st1, 0); waitpid(pid2, &st2, 0);
    sem_destroy(&shm->full1); sem_destroy(&shm->empty1);
    sem_destroy(&shm->full2); sem_destroy(&shm->empty2);
    munmap(shm, sizeof(Shm));
    shm_unlink(name);
    if (!WIFEXITED(st1) || WEXITSTATUS(st1) != 0 || !WIFEXITED(st2) || WEXITSTATUS(st2) != 0) {
        cerr << "child failed\n"; return 1;
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
    long long total = (long long)N * 10;
    cout << "order verified: " << total << " forwarded packets received in sent order (semaphore queues)\n";
    cout << "read: " << chrono::duration_cast<chrono::milliseconds>(t_read - t0).count() << " ms\n";
    cout << "pipeline (S1x1000 S2x1000 S3x1000 nested, shm+sem): " << chrono::duration_cast<chrono::milliseconds>(t_done - t_read).count() << " ms\n";
    cout << "write: " << chrono::duration_cast<chrono::milliseconds>(t_write - t_done).count() << " ms\n";
    cout << "total: " << chrono::duration_cast<chrono::milliseconds>(t_write - t0).count() << " ms\n";
    return 0;
}

