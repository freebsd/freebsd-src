/*
 * This file is part of librosprite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 James Shaw <js102@zepler.net>
 */

/**
 * \mainpage
 *
 * librosprite is a library for reading RISC OS sprite and palette files.  The following subformats are supported:
 * <ul>
 *   <li>1-bit and 8-bit transparency masks</li>
 *   <li>Sprites with custom palettes</li>
 *   <li>Standard RISC OS 1-bit, 2-bit palettes, and 4-bit and 8-bit colour palettes</li>
 *   <li>Old-style sprites with most screen modes from 0-49 supported</li>
 *   <li>32bpp CMYK</li>
 *   <li>Inline alpha channel, as used by Photodesk and Tinct</li>
 * </ul>
 *
 */

/**
 * \file librosprite.h
 *
 * Sprite file reading is performed by rosprite_load(), and palette file reading by rosprite_load_palette().
 *
 * Retrieving sprite or palette data is performed by a reader.
 * librosprite implements file and memory readers.
 * To use a reader, create a context by calling the
 * rosprite_create_file_context() or rosprite_create_mem_context().
 * Pass the reader function, and the context you have created, to the load function,
 * typically rosprite_load().
 */

#ifndef ROSPRITE_H
#define ROSPRITE_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef enum { ROSPRITE_OK, ROSPRITE_NOMEM, ROSPRITE_EOF, ROSPRITE_BADMODE } rosprite_error;

typedef enum { ROSPRITE_RGB, ROSPRITE_CMYK } rosprite_color_model;

/**
 * A reader interface used to load sprite files.
 */
typedef int (*reader)(uint8_t* buf, size_t count, void* ctx);

struct rosprite_file_context;

/**
 * A sprite area comprises zero or more rosprites.  Optionally, it may also have an extension_words block.
 */
struct rosprite_area {
	uint32_t extension_size; /* size of extension_words in bytes */
	uint8_t* extension_words;
	uint32_t sprite_count;
	struct rosprite** sprites; /* array of length sprite_count */
};

/**
 * A sprite mode defines the colour depth, colour model and bitmap resolution of a sprite.
 */
struct rosprite_mode {
	/**
	 * The bits per colour channel.  Legal values are 1, 2, 4, 8, 16, 24 and 32.
	 */
	uint32_t colorbpp;
	/* maskbpp denotes the amount of alpha bpp used
	 * while mask_width is the bits used to store the mask.
	 * Old modes have the same mask_width as their colorbpp, but the value
	 * is always all-zeroes or all-ones.
	 * New modes can have 1bpp or 8bpp masks
	 */
	uint32_t maskbpp;
	uint32_t mask_width; /* in pixels */

	/**
	 * Horizontal dots per inch.  Typical values are 22, 45, 90 and 180.
	 */
	uint32_t xdpi;

	/**
	 * Vertical dots per inch.  Typical values are 22, 45, 90 and 180.
	 */
	uint32_t ydpi;
	rosprite_color_model color_model;
};

struct rosprite_palette {
	uint32_t size; /* in number of entries (each entry is a word) */
	uint32_t* palette;
};

/**
 * A sprite is a bitmap image which has a mode, width and height.
 */
struct rosprite {

	/**
	 * The sprite name.  This may be up to 12 characters long, and must be zero terminated.
	 */
	unsigned char name[13];
	struct rosprite_mode mode;
	bool has_mask;
	bool has_palette;
	uint32_t palettesize; /* in number of entries (each entry is a word) */
	uint32_t* palette;

	/**
	 * Width in pixels
	 */
	uint32_t width;

	/**
	 * Height in pixels
	 */
	uint32_t height;

	/**
	 * Image data is a series of words, appearing on screen left-to-right, top-to-bottom.
	 * Each word takes the form 0xRRGGBBAA.  A is the alpha channel, where 0 is transparent, and 255 is opaque.
	 */
	uint32_t* image; /* image data in 0xRRGGBBAA words */
};

struct rosprite_file_context;

/**
 * Create a file reader context using the specified file handle.
 * Clients must call rosprite_destroy_file_context() to dispose of the context.
 *
 * \param[out] result
 */
rosprite_error rosprite_create_file_context(FILE* f, struct rosprite_file_context** result);
void rosprite_destroy_file_context(struct rosprite_file_context* ctx);
int rosprite_file_reader(uint8_t* buf, size_t count, void* ctx);

struct rosprite_mem_context;

/**
 * Create a memory reader context using the specified memory pointer.
 * Clients must call rosprite_destroy_mem_context() to dispose of the context.
 *
 * \param[in]  p          pointer to the start of the memory block
 * \param[in]  total_size the size of the block pointed to by p, in bytes
 * \param[out] result
 */
rosprite_error rosprite_create_mem_context(uint8_t* p, unsigned long total_size, struct rosprite_mem_context** result);
void rosprite_destroy_mem_context(struct rosprite_mem_context* ctx);
int rosprite_mem_reader(uint8_t* buf, size_t count, void* ctx);

/**
 * Load a rosprite_area using the reader provided.
 * Clients must call rosprite_destroy_sprite_area() to dispose of the rosprite_area.
 *
 * \param[out] result The pointer to be populated by this function.
 */
rosprite_error rosprite_load(reader reader, void* ctx, struct rosprite_area** result);

/**
 * Dispose of a rosprite_area and its children.
 */
void rosprite_destroy_sprite_area(struct rosprite_area *);

/**
 * Load a RISC OS palette file.  A palette file has RISC OS filetype 0xFED,
 * and is a series of VDU 19 Set Palette commands, each command being 6 bytes long.
 *
 * Clients must call rosprite_destroy_palette() to dispose of the rosprite_palette.
 *
 * \param[out] result The pointer to be populated by this function.
 * \see http://www.drobe.co.uk/show_manual.php?manual=/sh-cgi?manual=Vdu%26page=19
 */
rosprite_error rosprite_load_palette(reader reader, void* ctx, struct rosprite_palette** result);

/**
 * Dispose of a rosprite_palette and its children.
 */
void rosprite_destroy_palette(struct rosprite_palette *);

#endif
