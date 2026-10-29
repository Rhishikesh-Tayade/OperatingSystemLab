// Part 2.3 Variant 2: Shared Memory + Semaphores
// S1 runs 1000 times, every 100th result -> S2
// S2 runs 100 times per input, result -> S3
// S3 runs 100 times per input, last result written to file
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>
#include "../common/libppm.h"

using namespace std;

#define S1_ITERATIONS 1000
#define S1_PASS_EVERY 100
#define S2_ITERATIONS 100
#define S3_ITERATIONS 100

struct shm_header {
    sem_t s1_ready;
    sem_t s2_ready;
    sem_t s1_can_write;
    sem_t s2_can_write;
    int width;
    int height;
};

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

void image_to_flat_buf(struct image_t *img, uint8_t *buf)
{
    int idx = 0;
    for(int i = 0; i < img->height; i++)
        for(int j = 0; j < img->width; j++)
            for(int k = 0; k < 3; k++)
                buf[idx++] = img->image_pixels[i][j][k];
}

void flat_buf_to_image(uint8_t *buf, struct image_t *img)
{
    int idx = 0;
    for(int i = 0; i < img->height; i++)
        for(int j = 0; j < img->width; j++)
            for(int k = 0; k < 3; k++)
                img->image_pixels[i][j][k] = buf[idx++];
}

int main(int argc, char **argv)
{
    if(argc != 3) {
        cout << "usage: ./part2_3_v2 <input.ppm> <output.ppm>" << endl;
        exit(1);
    }

    struct image_t *input_image = read_ppm_file(argv[1]);
    int H = input_image->height, W = input_image->width;
    int img_size = H * W * 3;

    size_t shm_size = sizeof(struct shm_header) + 2 * img_size;
    int shmid = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | 0666);
    if(shmid < 0) { perror("shmget"); exit(1); }

    void *shm_ptr = shmat(shmid, NULL, 0);
    if(shm_ptr == (void*)-1) { perror("shmat"); exit(1); }

    struct shm_header *header = (struct shm_header *)shm_ptr;
    uint8_t *s1_buf = (uint8_t *)shm_ptr + sizeof(struct shm_header);
    uint8_t *s2_buf = s1_buf + img_size;

    sem_init(&header->s1_ready, 1, 0);
    sem_init(&header->s2_ready, 1, 0);
    sem_init(&header->s1_can_write, 1, 1);
    sem_init(&header->s2_can_write, 1, 1);
    header->width = W;
    header->height = H;

    cout << "=== Part 2.3 Variant 2: Shared Memory + Semaphores ===" << endl;
    cout << "Image: " << W << "x" << H << endl;

    auto start = chrono::steady_clock::now();

    pid_t pid1 = fork();
    if(pid1 == 0) {
        // S1 process
        struct image_t *s1_out = allocate_image(W, H);
        for(int iter = 1; iter <= S1_ITERATIONS; iter++) {
            S1_smoothen(input_image, s1_out);
            if(iter % S1_PASS_EVERY == 0) {
                sem_wait(&header->s1_can_write);
                image_to_flat_buf(s1_out, s1_buf);
                sem_post(&header->s1_ready);
            }
        }
        free_image(s1_out);
        shmdt(shm_ptr);
        exit(0);
    }

    pid_t pid2 = fork();
    if(pid2 == 0) {
        // S2 process
        struct image_t *s1_result = allocate_image(W, H);
        struct image_t *s2_out = allocate_image(W, H);
        int num_passes = S1_ITERATIONS / S1_PASS_EVERY;
        for(int p = 0; p < num_passes; p++) {
            sem_wait(&header->s1_ready);
            flat_buf_to_image(s1_buf, s1_result);
            sem_post(&header->s1_can_write);
            for(int iter = 1; iter <= S2_ITERATIONS; iter++) {
                S2_find_details(input_image, s1_result, s2_out);
            }
            sem_wait(&header->s2_can_write);
            image_to_flat_buf(s2_out, s2_buf);
            sem_post(&header->s2_ready);
        }
        free_image(s1_result);
        free_image(s2_out);
        shmdt(shm_ptr);
        exit(0);
    }

    // Parent: S3
    struct image_t *s2_result = allocate_image(W, H);
    struct image_t *s3_out = allocate_image(W, H);
    int num_passes = S1_ITERATIONS / S1_PASS_EVERY;
    for(int p = 0; p < num_passes; p++) {
        sem_wait(&header->s2_ready);
        flat_buf_to_image(s2_buf, s2_result);
        sem_post(&header->s2_can_write);
        for(int iter = 1; iter <= S3_ITERATIONS; iter++) {
            S3_sharpen(input_image, s2_result, s3_out);
        }
    }

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    auto end = chrono::steady_clock::now();

    write_ppm_file(argv[2], s3_out);

    cout << "Total time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << " ms" << endl;
    cout << "Verification: Semaphore ordering guarantees sequential delivery." << endl;

    sem_destroy(&header->s1_ready);
    sem_destroy(&header->s2_ready);
    sem_destroy(&header->s1_can_write);
    sem_destroy(&header->s2_can_write);
    shmdt(shm_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    free_image(s2_result);
    free_image(s3_out);
    free_image(input_image);

    return 0;
}
