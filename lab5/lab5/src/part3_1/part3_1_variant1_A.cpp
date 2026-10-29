// Part 3.1 Variant 1: Socket-based distributed computing
// Computer A: Runs S1 and S2, sends result to Computer B
// Pipeline variant: 1000 iterations, S1 uses original input each time
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

#define NUM_ITERATIONS 1000

void S1_smoothen(struct image_t *src, struct image_t *dst)
{
    int m = src->height, n = src->width;
    for(int i = 0; i < m; i++)
        for(int j = 0; j < n; j++)
            for(int k = 0; k < 3; k++)
                dst->image_pixels[i][j][k] = src->image_pixels[i][j][k];

    for(int i = 1; i < m - 1; i++)
        for(int j = 1; j < n - 1; j++)
            for(int k = 0; k < 3; k++) {
                int temp = 0;
                for(int x = i - 1; x < i + 2; x++)
                    for(int y = j - 1; y < j + 2; y++)
                        temp += src->image_pixels[x][y][k];
                dst->image_pixels[i][j][k] = temp / 9;
            }
}

void S2_find_details(struct image_t *orig, struct image_t *smooth, struct image_t *dst)
{
    for(int i = 0; i < orig->height; i++)
        for(int j = 0; j < orig->width; j++)
            for(int k = 0; k < 3; k++) {
                int diff = (int)orig->image_pixels[i][j][k] - (int)smooth->image_pixels[i][j][k];
                dst->image_pixels[i][j][k] = (uint8_t)(int8_t)diff;
            }
}

void write_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = (const uint8_t *)buf;
    while(n > 0) {
        ssize_t w = write(fd, p, n);
        if(w <= 0) { perror("write"); exit(1); }
        p += w; n -= w;
    }
}

int main(int argc, char **argv)
{
    // Usage: ./part3_1_v1_A <input.ppm> <server_ip> <port>
    if(argc != 4) {
        cout << "usage: ./part3_1_v1_A <input.ppm> <server_ip> <port>" << endl;
        exit(1);
    }

    struct image_t *input_image = read_ppm_file(argv[1]);
    int H = input_image->height, W = input_image->width;
    int img_size = H * W * 3;

    // Connect to Computer B (S3 server)
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    inet_pton(AF_INET, argv[2], &serv_addr.sin_addr);

    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(1);
    }

    cout << "=== Part 3.1 Variant 1: Computer A (S1+S2) ===" << endl;
    cout << "Connected to " << argv[2] << ":" << argv[3] << endl;

    // Send image dimensions and iteration count
    int32_t dims[3] = {W, H, NUM_ITERATIONS};
    write_all(sockfd, dims, sizeof(dims));

    // Also send the original input image (needed by S3 for sharpening)
    uint8_t *input_flat = image_to_flat(input_image);
    write_all(sockfd, input_flat, img_size);

    auto start = chrono::steady_clock::now();

    struct image_t *s1_out = allocate_image(W, H);
    struct image_t *s2_out = allocate_image(W, H);

    for(int iter = 0; iter < NUM_ITERATIONS; iter++) {
        S1_smoothen(input_image, s1_out);
        S2_find_details(input_image, s1_out, s2_out);
        // Send S2 result to Computer B
        uint8_t *flat = image_to_flat(s2_out);
        write_all(sockfd, flat, img_size);
        delete[] flat;
    }

    auto end = chrono::steady_clock::now();

    cout << "Total time (A): " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;

    close(sockfd);
    delete[] input_flat;
    free_image(s1_out);
    free_image(s2_out);
    free_image(input_image);

    return 0;
}
