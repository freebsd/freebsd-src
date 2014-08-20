/*
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 * Copyright 2008 James Bursa <james@netsurf-browser.org>
 *
 * This file is part of NetSurf's libnsgif, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../libnsgif.h"

unsigned char *load_file(const char *path, size_t *data_size);
void warning(const char *context, int code);
void *bitmap_create(int width, int height);
void bitmap_set_opaque(void *bitmap, bool opaque);
bool bitmap_test_opaque(void *bitmap);
unsigned char *bitmap_get_buffer(void *bitmap);
void bitmap_destroy(void *bitmap);
void bitmap_modified(void *bitmap);


int main(int argc, char *argv[])
{
	gif_bitmap_callback_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_get_buffer,
		bitmap_set_opaque,
		bitmap_test_opaque,
		bitmap_modified
	};
	gif_animation gif;
	size_t size;
	gif_result code;
	unsigned int i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s image.gif\n", argv[0]);
		return 1;
	}

	/* create our gif animation */
	gif_create(&gif, &bitmap_callbacks);

	/* load file into memory */
	unsigned char *data = load_file(argv[1], &size);

	/* begin decoding */
	do {
		code = gif_initialise(&gif, size, data);
		if (code != GIF_OK && code != GIF_WORKING) {
			warning("gif_initialise", code);
			exit(1);
		}
	} while (code != GIF_OK);

	printf("P3\n");
	printf("# %s\n", argv[1]);
	printf("# width                %u \n", gif.width);
	printf("# height               %u \n", gif.height);
	printf("# frame_count          %u \n", gif.frame_count);
	printf("# frame_count_partial  %u \n", gif.frame_count_partial);
	printf("# loop_count           %u \n", gif.loop_count);
	printf("%u %u 256\n", gif.width, gif.height * gif.frame_count);

	/* decode the frames */
	for (i = 0; i != gif.frame_count; i++) {
		unsigned int row, col;
		unsigned char *image;

		code = gif_decode_frame(&gif, i);
		if (code != GIF_OK)
			warning("gif_decode_frame", code);

		printf("# frame %u:\n", i);
		image = (unsigned char *) gif.frame_image;
		for (row = 0; row != gif.height; row++) {
			for (col = 0; col != gif.width; col++) {
				size_t z = (row * gif.width + col) * 4;
				printf("%u %u %u ",
					(unsigned char) image[z],
					(unsigned char) image[z + 1],
					(unsigned char) image[z + 2]);
			}
			printf("\n");
		}
	}

	/* clean up */
	gif_finalise(&gif);
	free(data);

	return 0;
}


unsigned char *load_file(const char *path, size_t *data_size)
{
	FILE *fd;
	struct stat sb;
	unsigned char *buffer;
	size_t size;
	size_t n;

	fd = fopen(path, "rb");
	if (!fd) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	if (stat(path, &sb)) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	size = sb.st_size;

	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %lld bytes\n",
				(long long) size);
		exit(EXIT_FAILURE);
	}

	n = fread(buffer, 1, size, fd);
	if (n != size) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	fclose(fd);

	*data_size = size;
	return buffer;
}


void warning(const char *context, gif_result code)
{
	fprintf(stderr, "%s failed: ", context);
	switch (code)
	{
	case GIF_INSUFFICIENT_FRAME_DATA:
		fprintf(stderr, "GIF_INSUFFICIENT_FRAME_DATA");
		break;
	case GIF_FRAME_DATA_ERROR:
		fprintf(stderr, "GIF_FRAME_DATA_ERROR");
		break;
	case GIF_INSUFFICIENT_DATA:
		fprintf(stderr, "GIF_INSUFFICIENT_DATA");
		break;
	case GIF_DATA_ERROR:
		fprintf(stderr, "GIF_DATA_ERROR");
		break;
	case GIF_INSUFFICIENT_MEMORY:
		fprintf(stderr, "GIF_INSUFFICIENT_MEMORY");
		break;
	default:
		fprintf(stderr, "unknown code %i", code);
		break;
	}
	fprintf(stderr, "\n");
}


void *bitmap_create(int width, int height)
{
	return calloc(width * height, 4);
}


void bitmap_set_opaque(void *bitmap, bool opaque)
{
	(void) opaque;  /* unused */
	assert(bitmap);
}


bool bitmap_test_opaque(void *bitmap)
{
	assert(bitmap);
	return false;
}


unsigned char *bitmap_get_buffer(void *bitmap)
{
	assert(bitmap);
	return bitmap;
}


void bitmap_destroy(void *bitmap)
{
	assert(bitmap);
	free(bitmap);
}


void bitmap_modified(void *bitmap)
{
	assert(bitmap);
	return;
}

