/*
 * Copyright 2006 Richard Wilson <richard.wilson@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 *
 * This file is part of NetSurf's libnsbmp, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

/** \file
 * BMP file decoding (interface).
 */

#ifndef libnsbmp_h_
#define libnsbmp_h_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* bmp flags */
#define BMP_NEW			0
#define BMP_OPAQUE		(1 << 0)	/** image is opaque (as opposed to having an alpha mask) */
#define BMP_CLEAR_MEMORY	(1 << 1)	/** memory should be wiped */

/* error return values */
typedef enum {
	BMP_OK = 0,
	BMP_INSUFFICIENT_MEMORY = 1,
	BMP_INSUFFICIENT_DATA = 2,
	BMP_DATA_ERROR = 3
} bmp_result;

/* encoding types */
typedef enum {
  	BMP_ENCODING_RGB = 0,
  	BMP_ENCODING_RLE8 = 1,
  	BMP_ENCODING_RLE4 = 2,
  	BMP_ENCODING_BITFIELDS = 3
} bmp_encoding;

/*	API for Bitmap callbacks
*/
typedef void* (*bmp_bitmap_cb_create)(int width, int height, unsigned int state);
typedef void (*bmp_bitmap_cb_destroy)(void *bitmap);
typedef unsigned char* (*bmp_bitmap_cb_get_buffer)(void *bitmap);
typedef size_t (*bmp_bitmap_cb_get_bpp)(void *bitmap);

/*	The Bitmap callbacks function table
*/
typedef struct bmp_bitmap_callback_vt_s {
	bmp_bitmap_cb_create bitmap_create;			/**< Create a bitmap. */
	bmp_bitmap_cb_destroy bitmap_destroy;			/**< Free a bitmap. */
	bmp_bitmap_cb_get_buffer bitmap_get_buffer;		/**< Return a pointer to the pixel data in a bitmap. */
	bmp_bitmap_cb_get_bpp bitmap_get_bpp;			/**< Find the width of a pixel row in bytes. */
} bmp_bitmap_callback_vt;

typedef struct bmp_image {
	bmp_bitmap_callback_vt bitmap_callbacks;	/**< callbacks for bitmap functions */
	uint8_t *bmp_data;				/** pointer to BMP data */
	uint32_t width;					/** width of BMP (valid after _analyse) */
	uint32_t height;				/** heigth of BMP (valid after _analyse) */
	bool decoded;					/** whether the image has been decoded */
	void *bitmap;					/** decoded image */
	/**	Internal members are listed below
	*/
	uint32_t buffer_size;				/** total number of bytes of BMP data available */
	bmp_encoding encoding;				/** pixel encoding type */
	uint32_t bitmap_offset;				/** offset of bitmap data */
	uint16_t bpp;					/** bits per pixel */
	uint32_t colours;				/** number of colours */
	uint32_t *colour_table;				/** colour table */
	bool limited_trans;				/** whether to use bmp's limited transparency */
	uint32_t trans_colour;				/** colour to display for "transparent" pixels when
							  * using limited transparency */
	bool reversed;					/** scanlines are top to bottom */
	bool ico;					/** image is part of an ICO, mask follows */
	bool opaque;					/** true if the bitmap does not contain an alpha channel */
	uint32_t mask[4];				/** four bitwise mask */
	int32_t shift[4];				/** four bitwise shifts */
	uint32_t transparent_index;			/** colour representing "transparency" in the bitmap */
} bmp_image;

typedef struct ico_image {
	bmp_image bmp;
	struct ico_image *next;
} ico_image;

typedef struct ico_collection {
	bmp_bitmap_callback_vt bitmap_callbacks;	/**< callbacks for bitmap functions */
	uint16_t width;					/** width of largest BMP */
	uint16_t height;				/** heigth of largest BMP */
	/**	Internal members are listed below
	*/
	uint8_t *ico_data;				/** pointer to ICO data */
	uint32_t buffer_size;				/** total number of bytes of ICO data available */
  	ico_image *first;
} ico_collection;

void bmp_create(bmp_image *bmp, bmp_bitmap_callback_vt *bitmap_callbacks);
void ico_collection_create(ico_collection *ico, 
		bmp_bitmap_callback_vt *bitmap_callbacks);
bmp_result bmp_analyse(bmp_image *bmp, size_t size, uint8_t *data);
bmp_result bmp_decode(bmp_image *bmp);
bmp_result bmp_decode_trans(bmp_image *bmp, uint32_t transparent_colour);
void bmp_finalise(bmp_image *bmp);

bmp_result ico_analyse(ico_collection *ico, size_t size, uint8_t *data);
bmp_image *ico_find(ico_collection *ico, uint16_t width, uint16_t height);
void ico_finalise(ico_collection *ico);

#endif
