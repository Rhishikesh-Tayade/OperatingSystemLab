// Part 3.2 Variant 1: Socket-based distributed computing
// Computer B: Runs S2 and S3, receives S1 result from Computer A
// Pipeline: 1000 iterations
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "../common/libppm.h"

using namespace std;

void S2_find_details(struct image_t *orig, struct image_t *smooth, struct image_t *dst)
{
    for(int i = 0; i < orig->height; i++)
        for(int j = 0; j < orig->width; j++)
            for(int k = 0; k < 3; k++) {
                int diff = (int)orig->image_pixels[i][j][k] - (int)smooth->image_pixels[i][j][k];
                dst->image_pixels[i][j][k] = (uint8_t)(int8_t)diff;
            }
}

void S3_sharpen(struct image_t *orig, struct image_t *details, struct image_t *dst)
{
    for(int i = 0; i < orig->height; i++)
        for(int j = 0; j < orig->width; j++)
            for(int k = 0; k < 3; k++) {
                int val = (int)orig->image_pixels[i][j][k] + (int8_t)details->image_pixels[i][j][k];
                if(val < 0) val = 0;
                if(val > 255) val = 255;
                dst->image_pixels[i][j][k] = (uint8_t)val;
            }
}

void read_all(int fd, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    while(n > 0) { ssize_t r = read(fd, p, n); if(r <= 0) { perror("read"); exit(1); } p += r; n -= r; }
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part3_2_v1_B <port> <output.ppm>" << endl;
        exit(1);
    }

    int port = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 1);

    cout << "=== Part 3.2 Variant 1: Computer B (S2+S3) ===" << endl;
    cout << "Listening on port " << port << "..." << endl;

    int client_fd = accept(server_fd, NULL, NULL);
    cout << "Client connected!" << endl;

    int32_t dims[3];
    read_all(client_fd, dims, sizeof(dims));
    int W = dims[0], H = dims[1], NUM_ITERATIONS = dims[2];
    int img_size = H * W * 3;

    uint8_t *input_flat = new uint8_t[img_size];
    read_all(client_fd, input_flat, img_size);
    struct image_t *input_image = flat_to_image(input_flat, W, H);
    delete[] input_flat;

    auto start = chrono::steady_clock::now();

    struct image_t *s2_out = allocate_image(W, H);
    struct image_t *s3_out = allocate_image(W, H);
    uint8_t *recv_buf = new uint8_t[img_size];

    for(int iter = 0; iter < NUM_ITERATIONS; iter++) {
        read_all(client_fd, recv_buf, img_size);
        struct image_t *s1_result = flat_to_image(recv_buf, W, H);
        S2_find_details(input_image, s1_result, s2_out);
        S3_sharpen(input_image, s2_out, s3_out);
        free_image(s1_result);
    }

    auto end = chrono::steady_clock::now();

    write_ppm_file(argv[2], s3_out);
    cout << "Total time (B): " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;

    close(client_fd); close(server_fd);
    delete[] recv_buf; free_image(s2_out); free_image(s3_out); free_image(input_image);
    return 0;
}
