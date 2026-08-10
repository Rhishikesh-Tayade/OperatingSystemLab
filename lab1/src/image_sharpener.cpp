#include <iostream>
#include "libppm.h"
#include <cstdint>

using namespace std;

struct image_t* S1_smoothen(struct image_t *input_image)
{
	// TODO
	// remember to allocate space for smoothened_image. See read_ppm_file() in libppm.c for some help.
	struct image_t* image = new struct image_t;
	image->height = input_image->height;
	image->width = input_image->width;

	image->image_pixels = new uint8_t**[image->height];
	for(int i = 0 ; i < image->height ; i++) {
		image->image_pixels[i] = new uint8_t*[image->width];
		for(int j = 0 ; j < image->width ; j++) 
			image->image_pixels[i][j] = new uint8_t[3];
	}

	int m = input_image->height, n = input_image->width;

	// Copy border pixels from input image
	for(int i = 0 ; i < m ; i++) {
		for(int j = 0 ; j < n ; j++) {
			for(int k = 0 ; k < 3 ; k++) {
				image->image_pixels[i][j][k] = input_image->image_pixels[i][j][k];
			}
		}
	}

	for(int i = 1 ; i < m - 1 ; i++) {
		for(int j = 1 ; j < n - 1 ; j++) {
			for(int k = 0 ; k < 3 ; k++) {
				int temp = 0;
				for(int x = i - 1 ; x < i + 2 ; x++) {
					for(int y = j - 1 ; y < j + 2 ; y++) {
						temp += input_image->image_pixels[x][y][k];
					}
				}
				image->image_pixels[i][j][k] = temp / 9;
			}
		}
	}

	return image;
}

struct image_t* S2_find_details(struct image_t *input_image, struct image_t *smoothened_image)
{
	// TODO
	struct image_t* image = new struct image_t;
	image->height = input_image->height;
	image->width = input_image->width;

	image->image_pixels = new uint8_t**[image->height];
	for(int i = 0 ; i < image->height ; i++) {
		image->image_pixels[i] = new uint8_t*[image->width];
		for(int j = 0 ; j < image->width ; j++) 
			image->image_pixels[i][j] = new uint8_t[3];
	}

	for(int i = 0 ; i < input_image->height ; i++) {
		for(int j = 0 ; j < input_image->width ; j++) {
			for(int k = 0 ; k < 3 ; k++) {
				int diff = (int)input_image->image_pixels[i][j][k] - (int)smoothened_image->image_pixels[i][j][k];
				image->image_pixels[i][j][k] = (uint8_t)(int8_t)diff;
			}
		}
	}

	return image;
}

struct image_t* S3_sharpen(struct image_t *input_image, struct image_t *details_image)
{
	// TODO
	struct image_t* image = new struct image_t;
	image->height = input_image->height;
	image->width = input_image->width;

	image->image_pixels = new uint8_t**[image->height];
	for(int i = 0 ; i < image->height ; i++) {
		image->image_pixels[i] = new uint8_t*[image->width];
		for(int j = 0 ; j < image->width ; j++) 
			image->image_pixels[i][j] = new uint8_t[3];
	}

	for(int i = 0 ; i < input_image->height ; i++) {
		for(int j = 0 ; j < input_image->width ; j++) {
			for(int k = 0 ; k < 3 ; k++) {
				int val = (int)input_image->image_pixels[i][j][k] + (int8_t)details_image->image_pixels[i][j][k];
				if(val < 0) val = 0;
				if(val > 255) val = 255;
				image->image_pixels[i][j][k] = (uint8_t)val;
			}
		}
	}

	return image;
}

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
		exit(0);
	}
	
	struct image_t *input_image = read_ppm_file(argv[1]);
	
	struct image_t *smoothened_image = S1_smoothen(input_image);
	
	struct image_t *details_image = S2_find_details(input_image, smoothened_image);
	
	struct image_t *sharpened_image = S3_sharpen(input_image, details_image);
	
	write_ppm_file(argv[2], sharpened_image);
	
	return 0;
}
