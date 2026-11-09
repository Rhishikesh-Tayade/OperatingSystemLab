#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>

using namespace std;

struct Packet {
    int32_t iter;
    int32_t index;
    int64_t seq;
    uint8_t inp[3];
    uint8_t payload[3];
};

static inline uint8_t s3_pixel(uint8_t inp, uint8_t det) {
    int v = (int)inp + (int8_t)det;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}
static bool recv_all(int fd, void *buf, size_t n) {
    char *p = (char*)buf; size_t got = 0;
    while (got < n) { ssize_t r = recv(fd, p + got, n - got, 0); if (r <= 0) return false; got += (size_t)r; }
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4) { cout << "usage: ./part3_1_variant1_B <ip_of_A> <port> <output.ppm>\n"; return 0; }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    auto t0 = chrono::steady_clock::now();
    int cli = socket(AF_INET, SOCK_STREAM, 0);
    if (cli < 0) { perror("socket"); return 1; }
    sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) { cerr << "bad IP " << ip << "\n"; return 1; }
    cout << "B (S3 client) connecting to " << ip << ":" << port << " ...\n";
    if (connect(cli, (sockaddr*)&addr, sizeof(addr)) != 0) { perror("connect"); return 1; }
    int wh[2];
    if (!recv_all(cli, wh, sizeof(wh))) { cerr << "recv header failed\n"; return 1; }
    int W = wh[0], H = wh[1];
    size_t N = (size_t)W * H;
    vector<uint8_t> out(N * 3, 0);
    int64_t expect = 0;
    for (int iter = 1; iter <= 1000; iter++) {
        for (int i = 0; i < H; i++) {
            for (int j = 0; j < W; j++) {
                Packet pkt;
                if (!recv_all(cli, &pkt, sizeof(pkt))) { cerr << "recv failed\n"; return 1; }
                if (pkt.seq != expect) { cerr << "ORDER FAIL seq=" << pkt.seq << " expect=" << expect << "\n"; return 1; }
                expect++;
                size_t p = (size_t)i * W + j;
                for (int k = 0; k < 3; k++) {
                    uint8_t v = s3_pixel(pkt.inp[k], pkt.payload[k]);
                    if (iter == 1000) out[p * 3 + k] = v;
                }
            }
        }
    }
    auto t_recv = chrono::steady_clock::now();
    close(cli);
    ofstream ws(argv[3], ios::binary | ios::out);
    if (!ws.is_open()) { cerr << "failed to open file " << argv[3] << "\n"; return 1; }
    ws.write("P6\n", 3);
    string w = to_string(W); ws.write(w.c_str(), (streamsize)w.length()); ws.write(" ", 1);
    string h = to_string(H); ws.write(h.c_str(), (streamsize)h.length());
    ws.write("\n255\n", 5);
    ws.write(reinterpret_cast<const char*>(out.data()), (streamsize)(N * 3));
    ws.close();
    auto t_write = chrono::steady_clock::now();
    cout << "order verified: " << expect << " packets received in sent order\n";
    cout << "recv+S3: " << chrono::duration_cast<chrono::milliseconds>(t_recv - t0).count() << " ms, write: " << chrono::duration_cast<chrono::milliseconds>(t_write - t_recv).count() << " ms\n";
    return 0;
}
