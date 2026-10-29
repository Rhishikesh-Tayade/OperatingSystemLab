// Part 2.1 Variant 2: Threads + Atomic Operations
// S1 runs 1000 times, every 100th result -> S2
// S2 runs 100 times per input, every 100th result -> S3
// S3 runs 100 times per input, last result written to file
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include "../common/libppm.h"

using namespace std;

#define S1_ITERATIONS 1000
#define S2_ITERATIONS 100
#define S3_ITERATIONS 100
#define S1_PASS_EVERY 100  // pass to S2 every 100th S1 result

// Shared buffers (double-buffered for S1->S2 and S2->S3)
struct image_t *input_image;
struct image_t *s1_buffer;  // S1 writes its result here
struct image_t *s2_input;   // Copy of S1 result for S2
struct image_t *s2_buffer;  // S2 writes its result here
struct image_t *s3_input;   // Copy of S2 result for S3
struct image_t *s3_output;  // Final output

int HEIGHT, WIDTH;

// Signaling: how many results have been passed
atomic<int> s1_to_s2_count(0);  // Number of results S1 has passed to S2
atomic<int> s2_to_s3_count(0);  // Number of results S2 has passed to S3
atomic<int> s1_to_s2_consumed(0); // Number of results S2 has consumed
atomic<int> s2_to_s3_consumed(0); // Number of results S3 has consumed

// Flag to indicate buffer is ready
atomic<bool> s1_buffer_ready(false);
atomic<bool> s2_buffer_ready(false);

// Verification
atomic<unsigned long> s1_final_checksum(0);
atomic<unsigned long> s2_final_checksum(0);
atomic<unsigned long> s3_final_checksum(0);

void copy_image(struct image_t *src, struct image_t *dst)
{
    for(int i = 0; i < src->height; i++)
        for(int j = 0; j < src->width; j++)
            for(int k = 0; k < 3; k++)
                dst->image_pixels[i][j][k] = src->image_pixels[i][j][k];
}

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

unsigned long compute_checksum(struct image_t *img)
{
    unsigned long sum = 0;
    for(int i = 0; i < img->height; i++)
        for(int j = 0; j < img->width; j++)
            for(int k = 0; k < 3; k++)
                sum += img->image_pixels[i][j][k] * (unsigned long)(i * img->width * 3 + j * 3 + k + 1);
    return sum;
}

void thread_S1()
{
    int passes = 0;
    for(int iter = 1; iter <= S1_ITERATIONS; iter++) {
        S1_smoothen(input_image, s1_buffer);
        if(iter % S1_PASS_EVERY == 0) {
            // Wait until S2 has consumed previous buffer
            while(s1_to_s2_count.load(memory_order_acquire) > s1_to_s2_consumed.load(memory_order_acquire)) {
                // spin
            }
            copy_image(s1_buffer, s2_input);
            unsigned long cs = compute_checksum(s2_input);
            s1_final_checksum.store(cs, memory_order_relaxed);
            s1_to_s2_count.fetch_add(1, memory_order_release);
            passes++;
        }
    }
}

void thread_S2()
{
    int total_passes = S1_ITERATIONS / S1_PASS_EVERY;  // 10 passes from S1
    for(int p = 0; p < total_passes; p++) {
        // Wait for S1 to produce a result
        while(s1_to_s2_count.load(memory_order_acquire) <= p) {
            // spin
        }
        // S2 runs S2_ITERATIONS times on this input
        for(int iter = 1; iter <= S2_ITERATIONS; iter++) {
            S2_find_details(input_image, s2_input, s2_buffer);
            if(iter % S2_ITERATIONS == 0) {
                // Pass to S3: wait until S3 consumed previous
                while(s2_to_s3_count.load(memory_order_acquire) > s2_to_s3_consumed.load(memory_order_acquire)) {
                    // spin
                }
                copy_image(s2_buffer, s3_input);
                unsigned long cs = compute_checksum(s3_input);
                s2_final_checksum.store(cs, memory_order_relaxed);
                s2_to_s3_count.fetch_add(1, memory_order_release);
            }
        }
        s1_to_s2_consumed.fetch_add(1, memory_order_release);
    }
}

void thread_S3()
{
    int total_passes = S1_ITERATIONS / S1_PASS_EVERY;  // 10 passes from S2
    for(int p = 0; p < total_passes; p++) {
        // Wait for S2 to produce a result
        while(s2_to_s3_count.load(memory_order_acquire) <= p) {
            // spin
        }
        // S3 runs S3_ITERATIONS times
        for(int iter = 1; iter <= S3_ITERATIONS; iter++) {
            S3_sharpen(input_image, s3_input, s3_output);
        }
        s3_final_checksum.store(compute_checksum(s3_output), memory_order_relaxed);
        s2_to_s3_consumed.fetch_add(1, memory_order_release);
    }
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part2_1_v2 <input.ppm> <output.ppm>" << endl;
        exit(1);
    }

    input_image = read_ppm_file(argv[1]);
    HEIGHT = input_image->height;
    WIDTH = input_image->width;

    s1_buffer = allocate_image(WIDTH, HEIGHT);
    s2_input = allocate_image(WIDTH, HEIGHT);
    s2_buffer = allocate_image(WIDTH, HEIGHT);
    s3_input = allocate_image(WIDTH, HEIGHT);
    s3_output = allocate_image(WIDTH, HEIGHT);

    cout << "=== Part 2.1 Variant 2: Threads + Atomic Ops ===" << endl;
    cout << "Image: " << WIDTH << "x" << HEIGHT << endl;
    cout << "S1: " << S1_ITERATIONS << " iters, pass every " << S1_PASS_EVERY << endl;
    cout << "S2: " << S2_ITERATIONS << " iters per pass" << endl;
    cout << "S3: " << S3_ITERATIONS << " iters per pass" << endl;

    auto start = chrono::steady_clock::now();

    thread t1(thread_S1);
    thread t2(thread_S2);
    thread t3(thread_S3);

    t1.join();
    t2.join();
    t3.join();

    auto end = chrono::steady_clock::now();

    write_ppm_file(argv[2], s3_output);

    cout << "Total time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;
    cout << "Verification checksums:" << endl;
    cout << "  S1->S2 last checksum: " << s1_final_checksum.load() << endl;
    cout << "  S2->S3 last checksum: " << s2_final_checksum.load() << endl;
    cout << "  S3 output checksum:   " << s3_final_checksum.load() << endl;

    free_image(s1_buffer);
    free_image(s2_input);
    free_image(s2_buffer);
    free_image(s3_input);
    free_image(s3_output);
    free_image(input_image);

    return 0;
}
