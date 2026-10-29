// Part 2.2 Variant 1: Multi-process + Pipes
// Pipeline: S1->S2->S3, 1000 iterations
// Each iteration: S1 uses original input, passes result to S2 via pipe,
// S2 passes result to S3 via pipe. 1000th S3 result written to file.
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include "../common/libppm.h"

using namespace std;

#define NUM_ITERATIONS 1000

void S1_smoothen_row(struct image_t *src, struct image_t *dst, int row)
{
    int m = src->height, n = src->width;
    if(row == 0 || row == m - 1) {
        for(int j = 0; j < n; j++)
            for(int k = 0; k < 3; k++)
                dst->image_pixels[row][j][k] = src->image_pixels[row][j][k];
        return;
    }
    for(int k = 0; k < 3; k++) {
        dst->image_pixels[row][0][k] = src->image_pixels[row][0][k];
        dst->image_pixels[row][n-1][k] = src->image_pixels[row][n-1][k];
    }
    for(int j = 1; j < n - 1; j++)
        for(int k = 0; k < 3; k++) {
            int temp = 0;
            for(int x = row - 1; x < row + 2; x++)
                for(int y = j - 1; y < j + 2; y++)
                    temp += src->image_pixels[x][y][k];
            dst->image_pixels[row][j][k] = temp / 9;
        }
}

void S2_find_details_row(struct image_t *orig, struct image_t *smooth, struct image_t *dst, int row)
{
    for(int j = 0; j < orig->width; j++)
        for(int k = 0; k < 3; k++) {
            int diff = (int)orig->image_pixels[row][j][k] - (int)smooth->image_pixels[row][j][k];
            dst->image_pixels[row][j][k] = (uint8_t)(int8_t)diff;
        }
}

void S3_sharpen_row(struct image_t *orig, struct image_t *details, struct image_t *dst, int row)
{
    for(int j = 0; j < orig->width; j++)
        for(int k = 0; k < 3; k++) {
            int val = (int)orig->image_pixels[row][j][k] + (int8_t)details->image_pixels[row][j][k];
            if(val < 0) val = 0;
            if(val > 255) val = 255;
            dst->image_pixels[row][j][k] = (uint8_t)val;
        }
}

// Helper: write exactly n bytes to fd
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

// Helper: read exactly n bytes from fd
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

unsigned long compute_row_checksum(const uint8_t *buf, int width)
{
    unsigned long sum = 0;
    for(int j = 0; j < width; j++)
        for(int k = 0; k < 3; k++)
            sum += buf[j * 3 + k] * (unsigned long)(j * 3 + k + 1);
    return sum;
}

void row_to_flat(struct image_t *img, int row, uint8_t *buf)
{
    int idx = 0;
    for(int j = 0; j < img->width; j++)
        for(int k = 0; k < 3; k++)
            buf[idx++] = img->image_pixels[row][j][k];
}

void flat_to_row(const uint8_t *buf, struct image_t *img, int row)
{
    int idx = 0;
    for(int j = 0; j < img->width; j++)
        for(int k = 0; k < 3; k++)
            img->image_pixels[row][j][k] = buf[idx++];
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part2_2_v1 <input.ppm> <output.ppm>" << endl;
        exit(1);
    }

    // Pipes: pipe1 = S1->S2, pipe2 = S2->S3
    int pipe1[2], pipe2[2];
    if(pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        perror("pipe");
        exit(1);
    }

    struct image_t *input_image = read_ppm_file(argv[1]);
    int HEIGHT = input_image->height;
    int WIDTH = input_image->width;
    int row_bytes = WIDTH * 3;

    cout << "=== Part 2.2 Variant 1: Processes + Pipes (Row Pipelining) ===" << endl;
    cout << "Image: " << WIDTH << "x" << HEIGHT << ", Iterations: " << NUM_ITERATIONS << endl;

    auto start = chrono::steady_clock::now();

    pid_t pid1 = fork();
    if(pid1 == 0) {
        // Child 1: S1 process
        close(pipe1[0]);  // close read end of pipe1
        close(pipe2[0]);
        close(pipe2[1]);

        struct image_t *s1_out = allocate_image(WIDTH, HEIGHT);
        uint8_t *row_buf = new uint8_t[row_bytes];

        for(int iter = 0; iter < NUM_ITERATIONS; iter++) {
            for(int row = 0; row < HEIGHT; row++) {
                S1_smoothen_row(input_image, s1_out, row);
                row_to_flat(s1_out, row, row_buf);
                int32_t r_idx = row;
                unsigned long cs = compute_row_checksum(row_buf, WIDTH);
                write_all(pipe1[1], &r_idx, sizeof(r_idx));
                write_all(pipe1[1], &cs, sizeof(cs));
                write_all(pipe1[1], row_buf, row_bytes);
            }
        }
        delete[] row_buf;
        free_image(s1_out);
        close(pipe1[1]);
        exit(0);
    }

    pid_t pid2 = fork();
    if(pid2 == 0) {
        // Child 2: S2 process
        close(pipe1[1]);  // close write end of pipe1
        close(pipe2[0]);  // close read end of pipe2

        struct image_t *s1_in = allocate_image(WIDTH, HEIGHT);
        struct image_t *s2_out = allocate_image(WIDTH, HEIGHT);
        uint8_t *recv_row_buf = new uint8_t[row_bytes];
        uint8_t *send_row_buf = new uint8_t[row_bytes];

        for(int iter = 0; iter < NUM_ITERATIONS; iter++) {
            for(int row = 0; row < HEIGHT; row++) {
                int32_t r_idx;
                unsigned long expected_cs;
                read_all(pipe1[0], &r_idx, sizeof(r_idx));
                read_all(pipe1[0], &expected_cs, sizeof(expected_cs));
                read_all(pipe1[0], recv_row_buf, row_bytes);

                // Verification check: exact order and checksum
                unsigned long actual_cs = compute_row_checksum(recv_row_buf, WIDTH);
                if(r_idx != row || actual_cs != expected_cs) {
                    cerr << "Error in S2: order or checksum mismatch at row " << row << endl;
                    exit(1);
                }

                flat_to_row(recv_row_buf, s1_in, row);
                S2_find_details_row(input_image, s1_in, s2_out, row);
                row_to_flat(s2_out, row, send_row_buf);

                unsigned long s2_cs = compute_row_checksum(send_row_buf, WIDTH);
                write_all(pipe2[1], &r_idx, sizeof(r_idx));
                write_all(pipe2[1], &s2_cs, sizeof(s2_cs));
                write_all(pipe2[1], send_row_buf, row_bytes);
            }
        }
        delete[] recv_row_buf;
        delete[] send_row_buf;
        free_image(s1_in);
        free_image(s2_out);
        close(pipe1[0]);
        close(pipe2[1]);
        exit(0);
    }

    // Parent: S3 process
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[1]);  // close write end of pipe2

    struct image_t *s2_in = allocate_image(WIDTH, HEIGHT);
    struct image_t *s3_out = allocate_image(WIDTH, HEIGHT);
    uint8_t *recv_s2_buf = new uint8_t[row_bytes];

    for(int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for(int row = 0; row < HEIGHT; row++) {
            int32_t r_idx;
            unsigned long expected_cs;
            read_all(pipe2[0], &r_idx, sizeof(r_idx));
            read_all(pipe2[0], &expected_cs, sizeof(expected_cs));
            read_all(pipe2[0], recv_s2_buf, row_bytes);

            // Verification check
            unsigned long actual_cs = compute_row_checksum(recv_s2_buf, WIDTH);
            if(r_idx != row || actual_cs != expected_cs) {
                cerr << "Error in S3: order or checksum mismatch at row " << row << endl;
                exit(1);
            }

            flat_to_row(recv_s2_buf, s2_in, row);
            S3_sharpen_row(input_image, s2_in, s3_out, row);
        }
    }
    close(pipe2[0]);
    delete[] recv_s2_buf;
    free_image(s2_in);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    auto end = chrono::steady_clock::now();

    write_ppm_file(argv[2], s3_out);

    cout << "Total time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;

    // Verification: compute output checksum
    unsigned long checksum = 0;
    for(int i = 0; i < HEIGHT; i++)
        for(int j = 0; j < WIDTH; j++)
            for(int k = 0; k < 3; k++)
                checksum += s3_out->image_pixels[i][j][k] * (unsigned long)(i * WIDTH * 3 + j * 3 + k + 1);
    cout << "Output checksum: " << checksum << endl;
    cout << "Verification: Pipe guarantees FIFO ordering. Rows sent sequentially, received sequentially." << endl;

    free_image(s3_out);
    free_image(input_image);

    return 0;
}
