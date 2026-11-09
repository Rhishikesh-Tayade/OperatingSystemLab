#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
static bool send_all(int fd, const void *buf, size_t n) {
    const char *p = (const char*)buf;
    while (n > 0) { ssize_t w = send(fd, p, n, 0); if (w <= 0) return false; p += w; n -= (size_t)w; }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) { cout << "usage: ./part3_2_variant1_A <input.ppm> <port>\n"; return 0; }
    int port = atoi(argv[2]);
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
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int opt = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);
    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0) { perror("bind"); return 1; }
    if (listen(srv, 1) != 0) { perror("listen"); return 1; }
    cout << "A (S1 server) listening on port " << port << " ...\n";
    int cli = accept(srv, NULL, NULL);
    if (cli < 0) { perror("accept"); return 1; }
    int wh[2] = {W, H};
    if (!send_all(cli, wh, sizeof(wh))) { cerr << "send header failed\n"; return 1; }
    int64_t seq = 0;
    for (int iter = 1; iter <= 1000; iter++) {
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                size_t p = (size_t)i * W + j;
                Packet pkt; pkt.iter = iter; pkt.index = (int32_t)p; pkt.seq = seq++;
                for (int k = 0; k < 3; k++) {
                    pkt.inp[k] = inp[p * 3 + k];
                    pkt.payload[k] = s1_pixel(inp.data(), W, H, i, j, k);
                }
                if (!send_all(cli, &pkt, sizeof(pkt))) { cerr << "send failed\n"; return 1; }
            }
        }
    }
    auto t_send = chrono::steady_clock::now();
    close(cli); close(srv);
    cout << "sent " << seq << " packets. read: " << chrono::duration_cast<chrono::milliseconds>(t_read - t0).count()
         << " ms, S1+send: " << chrono::duration_cast<chrono::milliseconds>(t_send - t_read).count() << " ms\n";
    return 0;
}
