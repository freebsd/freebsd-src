/*
 * This file is part of librosprite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 James Shaw <js102@zepler.net>
 */

/**
 * \file
 * A test harness using SDL to display all sprites in a sprite file.
 *
 * Usage: example \<spritefile\>
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>

#include "librosprite.h"

void sdl_draw_pixel(SDL_Surface* surface, uint32_t x, uint32_t y, uint32_t color);
void sdl_blank(SDL_Surface* surface);
int load_file_to_memory(const char *filename, uint8_t **result);
int create_file_context(char* filename, void** result);
int create_mem_context(char* filename, void** result);

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: example spritefile\n");
		exit(EXIT_FAILURE);
	}

	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	atexit(SDL_Quit);

	char* filename = argv[1];
	void* ctx;
	if (create_file_context(filename, &ctx) < 0) {
		fprintf(stderr, "Unable to create file reader context\n");
		exit(EXIT_FAILURE);
	}
	
	printf("Loading %s\n", filename);

	struct rosprite_area* sprite_area;
	if (rosprite_load(rosprite_file_reader, ctx, &sprite_area) != ROSPRITE_OK) {
		fprintf(stderr, "Error loading spritefile\n");
		exit(EXIT_FAILURE);
	};
	printf("sprite_count %u\n", sprite_area->sprite_count);
	printf("extension_size %u\n", sprite_area->extension_size);

	SDL_Surface *screen;
	screen = SDL_SetVideoMode(800, 600, 32, SDL_ANYFORMAT);
	SDL_SetAlpha(screen, SDL_SRCALPHA, 0);

	for (unsigned int i = 0; i < sprite_area->sprite_count; i++) {
		struct rosprite* sprite = sprite_area->sprites[i];
		printf("\nname %s\n", sprite->name);
		printf("color_model %s\n", sprite->mode.color_model == ROSPRITE_RGB ? "RGB" : "CMYK");
		printf("colorbpp %u\n", sprite->mode.colorbpp);
		printf("xdpi %u\n", sprite->mode.xdpi);
		printf("ydpi %u\n", sprite->mode.ydpi);
		printf("width %u px\n", sprite->width);
		printf("height %u px\n", sprite->height);
	
		printf("hasPalette %s\n", sprite->has_palette ? "YES" : "NO");
		if (sprite->has_palette) printf("paletteSize %u\n", sprite->palettesize);

		printf("hasMask %s\n", sprite->has_mask ? "YES" : "NO");
		if (sprite->has_mask) printf("mask_width %u\n", sprite->mode.mask_width);
		if (sprite->has_mask) printf("maskbpp %u\n", sprite->mode.maskbpp);

		sdl_blank(screen);

		for (uint32_t y = 0; y < sprite->height; y++) {
			for (uint32_t x = 0; x < sprite->width; x++) {
				sdl_draw_pixel(screen, x, y, sprite->image[y*sprite->width + x]);
			}
		}

		SDL_UpdateRect(screen, 0, 0, 0, 0);
		fgetc(stdin);
	}

	rosprite_destroy_mem_context(ctx);
	rosprite_destroy_sprite_area(sprite_area);

	return EXIT_SUCCESS;
}

int create_file_context(char* filename, void** result)
{
	FILE *f = fopen(filename, "rb");
	if (!f) {
		*result = NULL;
		return -1;
	}

	struct rosprite_file_context* ctx;
	if (rosprite_create_file_context(f, &ctx) != ROSPRITE_OK) {
		return -1;
	}
	*result = ctx;

	return 0;
}

int create_mem_context(char* filename, void** result)
{
	uint8_t* content;

	int size = load_file_to_memory(filename, &content);
	if (size < 0) return -1;
	struct rosprite_mem_context* ctx;	
	if (rosprite_create_mem_context(content, size, &ctx) != ROSPRITE_OK) {
		return -1;
	}
	*result = ctx;

	return 0;
}

int load_file_to_memory(const char *filename, uint8_t **result)
{
	int size = 0;
	FILE *f = fopen(filename, "rb");
	if (f == NULL) 
	{
		*result = NULL;
		return -1; // -1 means file opening fail
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	*result = (uint8_t *)malloc(size+1);
	if ((unsigned int) size != fread(*result, sizeof(char), size, f))
	{
		free(*result);
		return -2; // -2 means file reading fail
	}
	fclose(f);
	(*result)[size] = 0;
	return size;
}

/* color is 0xrrggbbaa */
void sdl_draw_pixel(SDL_Surface* surface, uint32_t x, uint32_t y, uint32_t color)
{
	uint32_t* pixel = ((uint32_t*) (surface->pixels)) + (y * surface->pitch/4) + x;
	/* pretty sure SDL can do this, but can't figure out how */
	uint32_t bg_color = ((int) (x / 4.0) + ((int)(y / 4.0) % 2)) % 2 ? 0x99 : 0x66;

	uint32_t alpha = color & 0x000000ff;
	uint32_t r = (color & 0xff000000) >> 24;
	uint32_t g = (color & 0x00ff0000) >> 16;
	uint32_t b = (color & 0x0000ff00) >> 8;
	r = ((alpha / 255.0) * r) + (((255-alpha) / 255.0) * bg_color);
	g = ((alpha / 255.0) * g) + (((255-alpha) / 255.0) * bg_color);
	b = ((alpha / 255.0) * b) + (((255-alpha) / 255.0) * bg_color);
	uint32_t mapped_color = SDL_MapRGB(surface->format, r, g, b);
	
	*pixel = mapped_color;
}

void sdl_blank(SDL_Surface* surface)
{
	for (uint32_t y = 0; y < (uint32_t) surface->h; y++) {
		for (uint32_t x = 0; x < (uint32_t) surface->w; x++) {
			sdl_draw_pixel(surface, x, y, (uint32_t) ((int) (x / 4.0) + ((int)(y / 4.0) % 2)) % 2 ? 0x999999ff : 0x666666ff);
		}
	}
}
