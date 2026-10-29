#ifndef LIBPPM_H
#define LIBPPM_H
#include <cstdint>

struct image_t
{
	int width;
	int height;
	uint8_t*** image_pixels;
};

struct image_t* read_ppm_file(char* path_to_input_file);
void write_ppm_file(char* path_to_output_file, struct image_t* image);

// Flat buffer utilities for IPC
uint8_t* image_to_flat(struct image_t* img);
struct image_t* flat_to_image(uint8_t* flat, int width, int height);
struct image_t* allocate_image(int width, int height);
void free_image(struct image_t* img);

#endif
