// Part 2.2 Variant 2: Multi-process + Pipes
// S1 runs 1000 times, every 100th result -> S2 via pipe
// S2 runs 100 times per input, every 100th result -> S3 via pipe
// S3 runs 100 times per input, last result written to file
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/libppm.h"

using namespace std;

#define S1_ITERATIONS 1000
#define S1_PASS_EVERY 100
#define S2_ITERATIONS 100
#define S3_ITERATIONS 100

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

void write_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = (const uint8_t *)buf;
    while(n > 0) {
        ssize_t w = write(fd, p, n);
        if(w <= 0) { perror("write"); exit(1); }
        p += w;
        n -= w;
    }
}

void read_all(int fd, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    while(n > 0) {
        ssize_t r = read(fd, p, n);
        if(r <= 0) { perror("read"); exit(1); }
        p += r;
        n -= r;
    }
}

void send_image(int fd, struct image_t *img)
{
    uint8_t *flat = image_to_flat(img);
    write_all(fd, flat, img->height * img->width * 3);
    delete[] flat;
}

struct image_t* recv_image(int fd, int w, int h)
{
    int sz = h * w * 3;
    uint8_t *flat = new uint8_t[sz];
    read_all(fd, flat, sz);
    struct image_t *img = flat_to_image(flat, w, h);
    delete[] flat;
    return img;
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part2_2_v2 <input.ppm> <output.ppm>" << endl;
        exit(1);
    }

    int pipe1[2], pipe2[2];
    if(pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        perror("pipe"); exit(1);
    }

    struct image_t *input_image = read_ppm_file(argv[1]);
    int H = input_image->height, W = input_image->width;

    cout << "=== Part 2.2 Variant 2: Processes + Pipes ===" << endl;
    cout << "Image: " << W << "x" << H << endl;

    auto start = chrono::steady_clock::now();

    pid_t pid1 = fork();
    if(pid1 == 0) {
        // S1 process
        close(pipe1[0]); close(pipe2[0]); close(pipe2[1]);
        struct image_t *s1_out = allocate_image(W, H);
        for(int iter = 1; iter <= S1_ITERATIONS; iter++) {
            S1_smoothen(input_image, s1_out);
            if(iter % S1_PASS_EVERY == 0) {
                send_image(pipe1[1], s1_out);
            }
        }
        close(pipe1[1]);
        exit(0);
    }

    pid_t pid2 = fork();
    if(pid2 == 0) {
        // S2 process
        close(pipe1[1]); close(pipe2[0]);
        struct image_t *s2_out = allocate_image(W, H);
        int num_passes = S1_ITERATIONS / S1_PASS_EVERY;  // 10
        for(int p = 0; p < num_passes; p++) {
            struct image_t *s1_result = recv_image(pipe1[0], W, H);
            for(int iter = 1; iter <= S2_ITERATIONS; iter++) {
                S2_find_details(input_image, s1_result, s2_out);
            }
            // Pass last result of S2 to S3
            send_image(pipe2[1], s2_out);
            free_image(s1_result);
        }
        close(pipe1[0]); close(pipe2[1]);
        exit(0);
    }

    // Parent: S3 process
    close(pipe1[0]); close(pipe1[1]); close(pipe2[1]);
    struct image_t *s3_out = allocate_image(W, H);
    int num_passes = S1_ITERATIONS / S1_PASS_EVERY;
    for(int p = 0; p < num_passes; p++) {
        struct image_t *s2_result = recv_image(pipe2[0], W, H);
        for(int iter = 1; iter <= S3_ITERATIONS; iter++) {
            S3_sharpen(input_image, s2_result, s3_out);
        }
        free_image(s2_result);
    }
    close(pipe2[0]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    auto end = chrono::steady_clock::now();

    write_ppm_file(argv[2], s3_out);

    cout << "Total time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;
    cout << "Verification: Pipe FIFO guarantees sequential row ordering." << endl;

    free_image(s3_out);
    free_image(input_image);

    return 0;
}
