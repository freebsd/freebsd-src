/*
 * This file is part of librosprite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 James Shaw <js102@zepler.net>
 */

#include <stdio.h>
#include <stdlib.h>

#include "librosprite.h"
 
int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("Usage: palette2c palettefile\n");
		exit(EXIT_FAILURE);
	}

	char* filename = argv[1];

	FILE* f = fopen(filename, "rb");
	if (f == NULL) {
		printf("Can't load palettefile %s\n", filename);
		exit(EXIT_FAILURE);
	}

	struct rosprite_file_context* ctx;
	if (rosprite_create_file_context(f, &ctx) != ROSPRITE_OK) {
		exit(EXIT_FAILURE);
	}
	
	struct rosprite_palette* palette;
	if (rosprite_load_palette(rosprite_file_reader, ctx, &palette) != ROSPRITE_OK) {
		exit(EXIT_FAILURE);
	}

	for (uint32_t i = 0; i < palette->size; i++) {
		printf("0x%x, ", palette->palette[i]);
	}

	fclose(f);

	rosprite_destroy_file_context(ctx);
	rosprite_destroy_palette(palette);

	return EXIT_SUCCESS;
}
