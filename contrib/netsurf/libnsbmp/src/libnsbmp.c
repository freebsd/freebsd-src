/*
 * Copyright 2006 Richard Wilson <richard.wilson@netsurf-browser.org>
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 *
 * This file is part of NetSurf's libnsbmp, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <libnsbmp.h>

#include "utils/log.h"

/*	The functions provided by this file allow for the decoding of
	Microsoft's BMP and ICO image file formats.

	READING BMP FILES
	=================

	To begin decoding a BMP, the caller should initialise a
	'bmp_bitmap_callback_vt' structure with the appropriate values necessary
	to handle bitmap images.  Next, a 'bmp_image' structure should be
	initialised by calling bmp_create().  This structure should then be
	passed to bmp_analyse() along with the BMP data to process and the size
	of this data.

	Once the analysis has begun, the decoder completes the width and height
	variables.

	To decode the image, the caller must use bmp_decode() which selects the
	proper decoding method based on the BMP info header and assigns the
	decoded bitmap image to the 'bitmap' member of the 'bmp_image'
	structure.  The bitmap image is stored with 4 bytes-per-pixel in RGBA
	format.

	It should be noted that bmp_finalise() should always be called, even if
	the image was never decoded.  It is also the responsibility of the
	caller to free 'bmp_data'.

	READING ICO FILES
	=================

	To begin decoding an ICO, the caller should initialise a
	'bmp_bitmap_callback_vt' structure with the appropriate values necessary
	to handle bitmap images.  Next, an 'ico_collection' structure should be
	initialised by calling ico_create().  This structure should then be
	passed to ico_analyse() along with the ICO data to process and the size
	of this data.

	Once the analysis has begun, the decoder completes the width and height
	variables.  Because ICO collections contain multiple bitmap images, the
	width and height will contain the values of the largest available image.

	The caller then obtains a BMP from the ICO collection by calling
	ico_find() with the requested width and height.

	To decode the image, the caller must use bmp_decode() which selects the
	proper decoding method based on the BMP info header and assigns the
	decoded bitmap image to the 'bitmap' member of the 'bmp_image'
	structure.  The bitmap image is stored with 4 bytes-per-pixel in RGBA
	format.

	It should be noted that ico_finalise() should always be called, even if
	no images were decoded.  Because ico_finalise() calls bmp_finalise() for
	each bitmap within the collection, the caller is not required to perform
	this function.  However, it is the responsibility of the caller to free
	'ico_data'.

	[dynis] - Tue 1st July 2008
*/

/* squashes unused variable compiler warnings */
#define UNUSED(x) ((x)=(x))

/* BMP flags */
#define BMP_FILE_HEADER_SIZE 14
#define ICO_FILE_HEADER_SIZE 6
#define ICO_DIR_ENTRY_SIZE 16

static inline int8_t read_int8(uint8_t *data, unsigned int o) {
	return (int8_t) data[o];
}

static inline uint8_t read_uint8(uint8_t *data, unsigned int o) {
	return (uint8_t) data[o];
}

static inline int16_t read_int16(uint8_t *data, unsigned int o) {
	return (int16_t) (data[o] | (data[o+1] << 8));
}

static inline uint16_t read_uint16(uint8_t *data, unsigned int o) {
	return (uint16_t) (data[o] | (data[o+1] << 8));
}

static inline int32_t read_int32(uint8_t *data, unsigned int o) {
	return (int32_t) (data[o] | (data[o+1] << 8) | (data[o+2] << 16) | (data[o+3] << 24));
}

static inline uint32_t read_uint32(uint8_t *data, unsigned int o) {
	return (uint32_t) (data[o] | (data[o+1] << 8) | (data[o+2] << 16) | (data[o+3] << 24));
}

static bmp_result next_ico_image(ico_collection *ico, ico_image *image);
static bmp_result bmp_analyse_header(bmp_image *bmp, unsigned char *data);
static bmp_result bmp_decode_rgb24(bmp_image *bmp, uint8_t **start, int bytes);
static bmp_result bmp_decode_rgb16(bmp_image *bmp, uint8_t **start, int bytes);
static bmp_result bmp_decode_rgb(bmp_image *bmp, uint8_t **start, int bytes);
static bmp_result bmp_decode_mask(bmp_image *bmp, uint8_t *data, int bytes);
static bmp_result bmp_decode_rle(bmp_image *bmp, uint8_t *data, int bytes, int size);



/**	Initialises necessary bmp_image members.
*/
void bmp_create(bmp_image *bmp, bmp_bitmap_callback_vt *bitmap_callbacks) {
	memset(bmp, 0, sizeof(bmp_image));
	bmp->bitmap_callbacks = *bitmap_callbacks;
}


/**	Initialises necessary ico_collection members.
*/
void ico_collection_create(ico_collection *ico, bmp_bitmap_callback_vt *bitmap_callbacks) {
	memset(ico, 0, sizeof(ico_collection));
	ico->bitmap_callbacks = *bitmap_callbacks;
}


/**
 * Analyse a BMP prior to decoding.
 *
 * This function will scan the data provided and perform simple checks to
 * ensure the data is a valid BMP.
 *
 * This function must be called before bmp_decode() and sets up all the
 * relevant values in the bmp structure.
 *
 * \param bmp	the BMP image to analyse
 * \return BMP_OK on success
 */
bmp_result bmp_analyse(bmp_image *bmp, size_t size, unsigned char *cdata) {
	uint8_t *data = (uint8_t *)cdata;

	/* ensure we aren't already initialised */
	if (bmp->bitmap)
		return BMP_OK;

	/* initialize values */
	bmp->buffer_size = size;
	bmp->bmp_data = data;

	/* standard 14-byte BMP file header is:
	 *
	 *	+0	UINT16	File Type ('BM')
	 *	+2	UINT32	Size of File (in bytes)
	 *	+6	INT16	Reserved Field (1)
	 *	+8	INT16	Reserved Field (2)
	 *	+10	UINT32	Starting Position of Image Data (offset in bytes)
	 */
	if (bmp->buffer_size < BMP_FILE_HEADER_SIZE)
		return BMP_INSUFFICIENT_DATA;
	if ((data[0] != (uint8_t)'B') || (data[1] != (uint8_t)'M'))
		return BMP_DATA_ERROR;
	bmp->bitmap_offset = read_uint32(data, 10);
	data += BMP_FILE_HEADER_SIZE;

	/* boundary checking */
	if (bmp->bitmap_offset >= size)
		return BMP_INSUFFICIENT_DATA;

	/* decode the BMP header */
	return bmp_analyse_header(bmp, data);
}


/**
 * Analyse an ICO prior to decoding.
 *
 * This function will scan the data provided and perform simple checks to
 * ensure the data is a valid ICO.
 *
 * This function must be called before ico_find().
 *
 * \param ico	the ICO image to analyse
 * \return BMP_OK on success
 */
bmp_result ico_analyse(ico_collection *ico, size_t size, uint8_t *data) {
	uint16_t count, i;
	bmp_result result;
	int area, max_area = 0;

	/* ensure we aren't already initialised */
	if (ico->first)
		return BMP_OK;

	/* initialize values */
	ico->buffer_size = size;
	ico->ico_data = data;

	/* 6-byte ICO file header is:
	 *
	 *	+0	INT16	Reserved (should be 0)
	 *	+2	UINT16	Type (1 for ICO, 2 for CUR)
	 *	+4	UINT16	Number of BMPs to follow
	 */
	if (ico->buffer_size < ICO_FILE_HEADER_SIZE)
		return BMP_INSUFFICIENT_DATA;
// 	if (read_int16(data, 2) != 0x0000)
// 		return BMP_DATA_ERROR;
	if (read_uint16(data, 2) != 0x0001)
		return BMP_DATA_ERROR;
	count = read_uint16(data, 4);
	if (count == 0)
		return BMP_DATA_ERROR;
	data += ICO_FILE_HEADER_SIZE;

	/* check if we have enough data for the directory */
	if (ico->buffer_size < (uint32_t)(ICO_FILE_HEADER_SIZE + (ICO_DIR_ENTRY_SIZE * count)))
		return BMP_INSUFFICIENT_DATA;

	/* Decode the BMP files.
	 *
	 * 16-byte ICO directory entry is:
	 *
	 *	+0	UINT8	Width (0 for 256 pixels)
	 *	+1	UINT8	Height (0 for 256 pixels)
	 *	+2	UINT8	Colour count (0 if more than 256 colours)
	 *	+3	INT8	Reserved (should be 0, but may not be)
	 *	+4	UINT16	Colour Planes (should be 0 or 1)
	 *	+6	UINT16	Bits Per Pixel
	 *	+8	UINT32	Size of BMP info header + bitmap data in bytes
	 *	+12	UINT32	Offset (points to the BMP info header, not the bitmap data)
	 */
	for (i = 0; i < count; i++) {
		ico_image *image;
		image = calloc(1, sizeof(ico_image));
		if (!image)
			return BMP_INSUFFICIENT_MEMORY;
		result = next_ico_image(ico, image);
		if (result != BMP_OK)
			return result;
		image->bmp.width = read_uint8(data, 0);
		if (image->bmp.width == 0)
			image->bmp.width = 256;
		image->bmp.height = read_uint8(data, 1);
		if (image->bmp.height == 0)
			image->bmp.height = 256;
		image->bmp.buffer_size = read_uint32(data, 8);
		image->bmp.bmp_data = ico->ico_data + read_uint32(data, 12);
		image->bmp.ico = true;
		data += ICO_DIR_ENTRY_SIZE;

		/* Ensure that the bitmap data resides in the buffer */
		if (image->bmp.bmp_data - ico->ico_data >= 0 &&
				(uint32_t)(image->bmp.bmp_data -
				ico->ico_data) >= ico->buffer_size)
			return BMP_DATA_ERROR;

		/* Ensure that we have sufficient data to read the bitmap */
		if (image->bmp.buffer_size - ICO_DIR_ENTRY_SIZE >= 
		    ico->buffer_size - (ico->ico_data - data))
			return BMP_INSUFFICIENT_DATA;

		result = bmp_analyse_header(&image->bmp, image->bmp.bmp_data);
		if (result != BMP_OK)
			return result;

		/* adjust the size based on the images available */
		area = image->bmp.width * image->bmp.height;
		if (area > max_area) {
			ico->width = image->bmp.width;
			ico->height = image->bmp.height;
			max_area = area;
		}
	}
	return BMP_OK;
}


/**
 * Allocates memory for the next BMP in an ICO collection
 *
 * Sets proper structure values
 *
 * \param ico		the ICO collection to add the image to
 * \param image		a pointer to the ICO image to be initialised
 */
static bmp_result next_ico_image(ico_collection *ico, ico_image *image) {
	bmp_create(&image->bmp, &ico->bitmap_callbacks);
	image->next = ico->first;
	ico->first = image;
	return BMP_OK;
}


static bmp_result bmp_analyse_header(bmp_image *bmp, uint8_t *data) {
	uint32_t header_size;
	uint32_t i;
	uint8_t j;
	int32_t width, height;
	uint8_t palette_size;
	unsigned int flags = 0;

	/* a variety of different bitmap headers can follow, depending
	 * on the BMP variant. A full description of the various headers
	 * can be found at
	 * http://msdn.microsoft.com/en-us/library/ms532301(VS.85).aspx
	 */
	header_size = read_uint32(data, 0);
	if (bmp->buffer_size < (14 + header_size))
		return BMP_INSUFFICIENT_DATA;
	if (header_size == 12) {
		/* the following header is for os/2 and windows 2.x and consists of:
		 *
		 *	+0	UINT32	size of this header (in bytes)
		 *	+4	INT16	image width (in pixels)
		 *	+6	INT16	image height (in pixels)
		 *	+8	UINT16	number of colour planes (always 1)
		 *	+10	UINT16	number of bits per pixel
		 */
		width = read_int16(data, 4);
		height = read_int16(data, 6);
		if ((width <= 0) || (height == 0))
			return BMP_DATA_ERROR;
		if (height < 0) {
			bmp->reversed = true;
			height = -height;
		}
		/* ICOs only support 256*256 resolutions
		 * In the case of the ICO header, the height is actually the added
		 * height of XOR-Bitmap and AND-Bitmap (double the visible height)
		 * Technically we could remove this check and ICOs with bitmaps
		 * of any size could be processed; this is to conform to the spec.
		 */
		if (bmp->ico) {
			if ((width > 256) || (height > 512)) {
				return BMP_DATA_ERROR;
			} else {
				bmp->width = width;
				bmp->height = height / 2;
			}
		} else {
			bmp->width = width;
			bmp->height = height;
		}
		if (read_uint16(data, 8) != 1)
			return BMP_DATA_ERROR;
		bmp->bpp = read_uint16(data, 10);
		/**
		 * The bpp value should be in the range 1-32, but the only
		 * values considered legal are:
		 * RGB ENCODING: 1, 4, 8, 16, 24 and 32
		 */
		if ((bmp->bpp != 1) && (bmp->bpp != 4) &&
				(bmp->bpp != 8) &&
				(bmp->bpp != 16) &&
				(bmp->bpp != 24) &&
				(bmp->bpp != 32))
			return BMP_DATA_ERROR;
		bmp->colours = (1 << bmp->bpp);
		palette_size = 3;
	} else if (header_size < 40) {
		return BMP_DATA_ERROR;
	} else {
		/* the following header is for windows 3.x and onwards. it is a
		 * minimum of 40 bytes and (as of Windows 95) a maximum of 108 bytes.
		 *
		 *	+0	UINT32	size of this header (in bytes)
		 *	+4	INT32	image width (in pixels)
		 *	+8	INT32	image height (in pixels)
 		 *	+12	UINT16	number of colour planes (always 1)
		 *	+14	UINT16	number of bits per pixel
		 *	+16	UINT32	compression methods used
		 *	+20	UINT32	size of bitmap (in bytes)
		 *	+24	UINT32	horizontal resolution (in pixels per meter)
		 *	+28	UINT32	vertical resolution (in pixels per meter)
		 *	+32	UINT32	number of colours in the image
		 *	+36	UINT32	number of important colours
		 *	+40	UINT32	mask identifying bits of red component
		 *	+44	UINT32	mask identifying bits of green component
		 *	+48	UINT32	mask identifying bits of blue component
		 *	+52	UINT32	mask identifying bits of alpha component
		 *	+56	UINT32	color space type
		 *	+60	UINT32	x coordinate of red endpoint
		 *	+64	UINT32	y coordinate of red endpoint
		 *	+68	UINT32	z coordinate of red endpoint
		 *	+72	UINT32	x coordinate of green endpoint
		 *	+76	UINT32	y coordinate of green endpoint
		 *	+80	UINT32	z coordinate of green endpoint
		 *	+84	UINT32	x coordinate of blue endpoint
		 *	+88	UINT32	y coordinate of blue endpoint
		 *	+92	UINT32	z coordinate of blue endpoint
		 *	+96	UINT32	gamma red coordinate scale value
		 *	+100	UINT32	gamma green coordinate scale value
		 *	+104	UINT32	gamma blue coordinate scale value
		 */
		width = read_int32(data, 4);
		height = read_int32(data, 8);
		if ((width <= 0) || (height == 0))
			return BMP_DATA_ERROR;
		if (height < 0) {
			bmp->reversed = true;
			height = -height;
		}
		/* ICOs only support 256*256 resolutions
		 * In the case of the ICO header, the height is actually the added
		 * height of XOR-Bitmap and AND-Bitmap (double the visible height)
		 * Technically we could remove this check and ICOs with bitmaps
		 * of any size could be processed; this is to conform to the spec.
		 */
		if (bmp->ico) {
			if ((width > 256) || (height > 512)) {
				return BMP_DATA_ERROR;
			} else {
				bmp->width = width;
				bmp->height = height / 2;
			}
		} else {
			bmp->width = width;
			bmp->height = height;
		}
		if (read_uint16(data, 12) != 1)
			return BMP_DATA_ERROR;
		bmp->bpp = read_uint16(data, 14);
		if (bmp->bpp == 0)
			bmp->bpp = 8;
		bmp->encoding = read_uint32(data, 16);
		/**
		 * The bpp value should be in the range 1-32, but the only
		 * values considered legal are:
		 * RGB ENCODING: 1, 4, 8, 16, 24 and 32
		 * RLE4 ENCODING: 4
		 * RLE8 ENCODING: 8
		 * BITFIELD ENCODING: 16 and 32
		 */
		switch (bmp->encoding) {
			case BMP_ENCODING_RGB:
				if ((bmp->bpp != 1) && (bmp->bpp != 4) &&
						(bmp->bpp != 8) &&
						(bmp->bpp != 16) &&
						(bmp->bpp != 24) &&
						(bmp->bpp != 32))
					return BMP_DATA_ERROR;
				break;
			case BMP_ENCODING_RLE8:
				if (bmp->bpp != 8)
					return BMP_DATA_ERROR;
				break;
			case BMP_ENCODING_RLE4:
				if (bmp->bpp != 4)
					return BMP_DATA_ERROR;
				break;
			case BMP_ENCODING_BITFIELDS:
				if ((bmp->bpp != 16) && (bmp->bpp != 32))
					return BMP_DATA_ERROR;
				break;
			/* invalid encoding */
			default:
				return BMP_DATA_ERROR;
				break;
		}
		/* Bitfield encoding means we have red, green, blue, and alpha masks.
		 * Here we aquire the masks and determine the required bit shift to
		 * align them in our 24-bit color 8-bit alpha format.
		 */
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			if (header_size == 40) {
				header_size += 12;
				if (bmp->buffer_size < (14 + header_size))
					return BMP_INSUFFICIENT_DATA;
				for (i = 0; i < 3; i++)
					bmp->mask[i] = read_uint32(data, 40 + (i << 2));
			} else {
				for (i = 0; i < 4; i++)
					bmp->mask[i] = read_uint32(data, 40 + (i << 2));
			}
			for (i = 0; i < 4; i++) {
				if (bmp->mask[i] == 0)
					break;
				for (j = 31; j > 0; j--)
					if (bmp->mask[i] & (1 << j)) {
					  	if ((j - 7) > 0)
					  		bmp->mask[i] &= 0xff << (j - 7);
					  	else
					  		bmp->mask[i] &= 0xff >> (-(j - 7));
					  	bmp->shift[i] = (i << 3) - (j - 7);
						break;
					}
			}
		}
		bmp->colours = read_uint32(data, 32);
		if (bmp->colours == 0)
			bmp->colours = (1 << bmp->bpp);
		palette_size = 4;
	}
	data += header_size;

	/* if there's no alpha mask, flag the bmp opaque */
	if ((!bmp->ico) && (bmp->mask[3] == 0)) {
		flags |= BMP_OPAQUE;
		bmp->opaque = true;
	}

	/* we only have a palette for <16bpp */
	if (bmp->bpp < 16) {
		/* we now have a series of palette entries of the format:
		 *
		 *	+0	BYTE	blue
		 *	+1	BYTE	green
		 *	+2	BYTE	red
		 *
		 * if the palette is from an OS/2 or Win2.x file then the entries
		 * are padded with an extra byte.
		 */

		/* boundary checking */
		if (bmp->buffer_size < (14 + header_size + ((uint64_t)4 * bmp->colours)))
			return BMP_INSUFFICIENT_DATA;

		/* create the colour table */
		bmp->colour_table = (uint32_t *)malloc(bmp->colours * 4);
		if (!bmp->colour_table)
			return BMP_INSUFFICIENT_MEMORY;
		for (i = 0; i < bmp->colours; i++) {
			bmp->colour_table[i] = data[2] | (data[1] << 8) | (data[0] << 16);
			if (bmp->opaque)
				bmp->colour_table[i] |= (0xff << 24);
			data += palette_size;
			bmp->colour_table[i] = read_uint32((uint8_t *)&bmp->colour_table[i],0);
		}
	}

	/* create our bitmap */
	flags |= BMP_NEW | BMP_CLEAR_MEMORY;
	bmp->bitmap = bmp->bitmap_callbacks.bitmap_create(bmp->width, bmp->height, flags);
	if (!bmp->bitmap) {
		if (bmp->colour_table)
			free(bmp->colour_table);
		bmp->colour_table = NULL;
		return BMP_INSUFFICIENT_MEMORY;
	}
	/* BMPs within ICOs don't have BMP file headers, so the image data should
	 * always be right after the colour table.
	 */
	if (bmp->ico)
		bmp->bitmap_offset = (intptr_t)data - (intptr_t)bmp->bmp_data;
	return BMP_OK;
}


/**
 * Finds the closest BMP within an ICO collection
 *
 * This function finds the BMP with dimensions as close to a specified set
 * as possible from the images in the collection.
 *
 * \param ico		the ICO collection to examine
 * \param width		the preferred width (0 to use ICO header width)
 * \param height	the preferred height (0 to use ICO header height)
 */
bmp_image *ico_find(ico_collection *ico, uint16_t width, uint16_t height) {
	bmp_image *bmp = NULL;
	ico_image *image;
	int x, y, cur, distance = (1 << 24);

	if (width == 0)
		width = ico->width;
	if (height == 0)
		height = ico->height;
	for (image = ico->first; image; image = image->next) {
		if ((image->bmp.width == width) && (image->bmp.height == height))
			return &image->bmp;
		x = image->bmp.width - width;
		y = image->bmp.height - height;
		cur = (x * x) + (y * y);
		if (cur < distance) {
			distance = cur;
			bmp = &image->bmp;
		}
	}
	return bmp;
}


/**
 * Decode a BMP
 *
 * This function decodes the BMP data such that bmp->bitmap is a valid
 * image. The state of bmp->decoded is set to TRUE on exit such that it
 * can easily be identified which BMPs are in a fully decoded state.
 *
 * \param bmp	the BMP image to decode
 * \return BMP_OK on success
 */
bmp_result bmp_decode(bmp_image *bmp) {
	uint8_t *data;
	uint32_t bytes;
	bmp_result result = BMP_OK;

	assert(bmp->bitmap);

	data = bmp->bmp_data + bmp->bitmap_offset;
	bytes = bmp->buffer_size - bmp->bitmap_offset;

	switch (bmp->encoding) {
		case BMP_ENCODING_RGB:
			if ((bmp->bpp == 24) || (bmp->bpp == 32))
				result = bmp_decode_rgb24(bmp, &data, bytes);
			else if (bmp->bpp == 16)
				result = bmp_decode_rgb16(bmp, &data, bytes);
			else
				result = bmp_decode_rgb(bmp, &data, bytes);
			break;
		case BMP_ENCODING_RLE8:
			result = bmp_decode_rle(bmp, data, bytes, 8);
			break;
		case BMP_ENCODING_RLE4:
			result = bmp_decode_rle(bmp, data, bytes, 4);
			break;
		case BMP_ENCODING_BITFIELDS:
			if (bmp->bpp == 32)
				result = bmp_decode_rgb24(bmp, &data, bytes);
			else if (bmp->bpp == 16)
				result = bmp_decode_rgb16(bmp, &data, bytes);
			else
				return BMP_DATA_ERROR;
	}

	if ((!bmp->ico) || (result != BMP_OK))
		return result;

	bytes = (uintptr_t)bmp->bmp_data + bmp->buffer_size - (uintptr_t)data;
	return bmp_decode_mask(bmp, data, bytes);
}

/**
 * Decode a BMP using "limited transparency"
 *
 * Bitmaps do not have native transparency support.  However, there is a
 * "trick" that is used in some instances in which the first pixel of the
 * bitmap becomes the "transparency index".  The decoding application can
 * replace this index with whatever background colour it chooses to
 * create the illusion of transparency.
 *
 * When to use transparency is at the discretion of the decoding
 * application.
 * 
 * \param bmp		the BMP image to decode
 * \param colour	the colour to use as "transparent"
 * \return BMP_OK on success
 */
bmp_result bmp_decode_trans(bmp_image *bmp, uint32_t colour) {
	bmp->limited_trans = true;
	bmp->trans_colour = colour;
	return bmp_decode(bmp);
}


/**
 * Decode BMP data stored in 24bpp colour.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return	BMP_OK on success
 *		BMP_INSUFFICIENT_DATA if the bitmap data ends unexpectedly;
 *			in this case, the image may be partially viewable
 */
static bmp_result bmp_decode_rgb24(bmp_image *bmp, uint8_t **start, int bytes) {
	uint8_t *top, *bottom, *end, *data;
	uint32_t *scanline;
	uint32_t x, y;
	uint32_t swidth, skip;
	intptr_t addr;
	uint8_t i;
	uint32_t word;

	data = *start;
	swidth = bmp->bitmap_callbacks.bitmap_get_bpp(bmp->bitmap) * bmp->width;
	top = bmp->bitmap_callbacks.bitmap_get_buffer(bmp->bitmap);
	if (!top)
		return BMP_INSUFFICIENT_MEMORY;
	bottom = top + (uint64_t)swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((intptr_t)data) & 3;
	skip = bmp->bpp >> 3;
	bmp->decoded = true;

	/* Determine transparent index */
	if (bmp->limited_trans) {
		if ((data + skip) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->encoding == BMP_ENCODING_BITFIELDS)
			bmp->transparent_index = read_uint32(data, 0);
		else
			bmp->transparent_index = data[2] | (data[1] << 8) | (data[0] << 16);
	}

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((intptr_t)data) & 3))
			data++;
		if ((data + (skip * bmp->width)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (void *)(top + (y * swidth));
		else
			scanline = (void *)(bottom - (y * swidth));
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			for (x = 0; x < bmp->width; x++) {
				word = read_uint32(data, 0);
				for (i = 0; i < 4; i++)
					if (bmp->shift[i] > 0)
						scanline[x] |= ((word & bmp->mask[i]) << bmp->shift[i]);
					else
						scanline[x] |= ((word & bmp->mask[i]) >> (-bmp->shift[i]));
				/* 32-bit BMPs have alpha masks, but sometimes they're not utilized */
				if (bmp->opaque)
					scanline[x] |= (0xff << 24);
				data += skip;
				scanline[x] = read_uint32((uint8_t *)&scanline[x],0);
			}
		} else {
			for (x = 0; x < bmp->width; x++) {
				scanline[x] = data[2] | (data[1] << 8) | (data[0] << 16);
				if ((bmp->limited_trans) && (scanline[x] == bmp->transparent_index))
					scanline[x] = bmp->trans_colour;
				if (bmp->opaque)
					scanline[x] |= (0xff << 24);
				data += skip;
				scanline[x] = read_uint32((uint8_t *)&scanline[x],0);
			}
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode BMP data stored in 16bpp colour.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return	BMP_OK on success
 *		BMP_INSUFFICIENT_DATA if the bitmap data ends unexpectedly;
 *			in this case, the image may be partially viewable
 */
static bmp_result bmp_decode_rgb16(bmp_image *bmp, uint8_t **start, int bytes) {
	uint8_t *top, *bottom, *end, *data;
	uint32_t *scanline;
	uint32_t x, y, swidth;
	intptr_t addr;
	uint8_t i;
	uint16_t word;

	data = *start;
	swidth = bmp->bitmap_callbacks.bitmap_get_bpp(bmp->bitmap) * bmp->width;
	top = bmp->bitmap_callbacks.bitmap_get_buffer(bmp->bitmap);
	if (!top)
		return BMP_INSUFFICIENT_MEMORY;
	bottom = top + (uint64_t)swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((intptr_t)data) & 3;
	bmp->decoded = true;

	/* Determine transparent index */
	if (bmp->limited_trans) {
		if ((data + 2) > end)
			return BMP_INSUFFICIENT_DATA;
		bmp->transparent_index = read_uint16(data, 0);
	}

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((intptr_t)data) & 3))
			data += 2;
		if ((data + (2 * bmp->width)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (void *)(top + (y * swidth));
		else
			scanline = (void *)(bottom - (y * swidth));
		if (bmp->encoding == BMP_ENCODING_BITFIELDS) {
			for (x = 0; x < bmp->width; x++) {
				word = read_uint16(data, 0);
				if ((bmp->limited_trans) && (word == bmp->transparent_index))
					scanline[x] = bmp->trans_colour;
				else {
					scanline[x] = 0;
					for (i = 0; i < 4; i++)
						if (bmp->shift[i] > 0)
							scanline[x] |= ((word & bmp->mask[i]) << bmp->shift[i]);
						else
							scanline[x] |= ((word & bmp->mask[i]) >> (-bmp->shift[i]));
					if (bmp->opaque)
						scanline[x] |= (0xff << 24);
				}
				data += 2;
				scanline[x] = read_uint32((uint8_t *)&scanline[x],0);
			}
		} else {
			for (x = 0; x < bmp->width; x++) {
				word = read_uint16(data, 0);
				if ((bmp->limited_trans) && (word == bmp->transparent_index))
					scanline[x] = bmp->trans_colour;
				else {
					/* 16-bit RGB defaults to RGB555 */
					scanline[x] = ((word & (31 << 0)) << 19) |
							((word & (31 << 5)) << 6) |
							((word & (31 << 10)) >> 7);
				}
				if (bmp->opaque)
					scanline[x] |= (0xff << 24);
				data += 2;
				scanline[x] = read_uint32((uint8_t *)&scanline[x],0);
			}
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode BMP data stored with a palette and in 8bpp colour or less.
 *
 * \param bmp	the BMP image to decode
 * \param start	the data to decode, updated to last byte read on success
 * \param bytes	the number of bytes of data available
 * \return	BMP_OK on success
 *		BMP_INSUFFICIENT_DATA if the bitmap data ends unexpectedly;
 *			in this case, the image may be partially viewable
 */
static bmp_result bmp_decode_rgb(bmp_image *bmp, uint8_t **start, int bytes) {
	uint8_t *top, *bottom, *end, *data;
	uint32_t *scanline;
	intptr_t addr;
	uint32_t x, y, swidth;
	uint8_t bit_shifts[8];
	uint8_t ppb = 8 / bmp->bpp;
	uint8_t bit_mask = (1 << bmp->bpp) - 1;
	uint8_t cur_byte = 0, bit, i;

	for (i = 0; i < ppb; i++)
	    bit_shifts[i] = 8 - ((i + 1) * bmp->bpp);

	data = *start;
	swidth = bmp->bitmap_callbacks.bitmap_get_bpp(bmp->bitmap) * bmp->width;
	top = bmp->bitmap_callbacks.bitmap_get_buffer(bmp->bitmap);
	if (!top)
		return BMP_INSUFFICIENT_MEMORY;
	bottom = top + (uint64_t)swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((intptr_t)data) & 3;
	bmp->decoded = true;

	/* Determine transparent index */
	if (bmp->limited_trans)
		bmp->transparent_index = bmp->colour_table[(*data >> bit_shifts[0]) & bit_mask];

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((intptr_t)data) & 3))
			data++;
		bit = 8;
		if ((data + (bmp->width / ppb)) > end)
			return BMP_INSUFFICIENT_DATA;
		if (bmp->reversed)
			scanline = (void *)(top + (y * swidth));
		else
			scanline = (void *)(bottom - (y * swidth));
		for (x = 0; x < bmp->width; x++) {
			if (bit >= ppb) {
				bit = 0;
				cur_byte = *data++;
			}
			scanline[x] = bmp->colour_table[(cur_byte >> bit_shifts[bit++]) & bit_mask];
			if ((bmp->limited_trans) && (scanline[x] == bmp->transparent_index))
				scanline[x] = bmp->trans_colour;
		}
	}
	*start = data;
	return BMP_OK;
}


/**
 * Decode a 1bpp mask for an ICO
 *
 * \param bmp	the BMP image to decode
 * \param data	the data to decode
 * \param bytes	the number of bytes of data available
 * \return BMP_OK on success
 */
static bmp_result bmp_decode_mask(bmp_image *bmp, uint8_t *data, int bytes) {
	uint8_t *top, *bottom, *end;
	uint32_t *scanline;
	intptr_t addr;
	uint32_t x, y, swidth;
	uint32_t cur_byte = 0;

	swidth = bmp->bitmap_callbacks.bitmap_get_bpp(bmp->bitmap) * bmp->width;
	top = bmp->bitmap_callbacks.bitmap_get_buffer(bmp->bitmap);
	if (!top)
		return BMP_INSUFFICIENT_MEMORY;
	bottom = top + (uint64_t)swidth * (bmp->height - 1);
	end = data + bytes;
	addr = ((intptr_t)data) & 3;

	for (y = 0; y < bmp->height; y++) {
		while (addr != (((intptr_t)data) & 3))
			data++;
		if ((data + (bmp->width >> 3)) > end)
			return BMP_INSUFFICIENT_DATA;
		scanline = (void *)(bottom - (y * swidth));
		for (x = 0; x < bmp->width; x++) {
			if ((x & 7) == 0)
				cur_byte = *data++;
			if ((cur_byte & 128) == 0) {
				scanline[x] = read_uint32((uint8_t *)&scanline[x], 0);
				scanline[x] |= (0xff << 24);
				scanline[x] = read_uint32((uint8_t *)&scanline[x], 0);
			}
			cur_byte = cur_byte << 1;
		}
	}
	return BMP_OK;
}


/**
 * Decode BMP data stored encoded in either RLE4 or RLE8.
 *
 * \param bmp	the BMP image to decode
 * \param data	the data to decode
 * \param bytes	the number of bytes of data available
 * \param size	the size of the RLE tokens (4 or 8)
 * \return	BMP_OK on success
 *		BMP_INSUFFICIENT_DATA if the bitmap data ends unexpectedly;
 *			in this case, the image may be partially viewable
 */
static bmp_result bmp_decode_rle(bmp_image *bmp, uint8_t *data, int bytes, int size) {
	uint8_t *top, *bottom, *end;
	uint32_t *scanline;
	uint32_t swidth;
	uint32_t i, length, pixels_left;
	uint32_t x = 0, y = 0, last_y = 0;
	uint32_t pixel = 0, pixel2;

	if (bmp->ico)
		return BMP_DATA_ERROR;

	swidth = bmp->bitmap_callbacks.bitmap_get_bpp(bmp->bitmap) * bmp->width;
	top = bmp->bitmap_callbacks.bitmap_get_buffer(bmp->bitmap);
	if (!top)
		return BMP_INSUFFICIENT_MEMORY;
	bottom = top + (uint64_t)swidth * (bmp->height - 1);
	end = data + bytes;
	bmp->decoded = true;

	do {
		if (data + 2 > end)
			return BMP_INSUFFICIENT_DATA;
		length = *data++;
		if (length == 0) {
			length = *data++;
			if (length == 0) {
				/* 00 - 00 means end of scanline */
				x = 0;
				if (last_y == y) {
					if (++y > bmp->height)
						return BMP_DATA_ERROR;
				}
				last_y = y;
			} else if (length == 1) {
				/* 00 - 01 means end of RLE data */
				return BMP_OK;
			} else if (length == 2) {
				/* 00 - 02 - XX - YY means move cursor */
				if (data + 2 > end)
					return BMP_INSUFFICIENT_DATA;
				x += *data++;
				if (x >= bmp->width)
					return BMP_DATA_ERROR;
				y += *data++;
				if (y >= bmp->height)
					return BMP_DATA_ERROR;
			} else {
				/* 00 - NN means escape NN pixels */
				if (bmp->reversed) {
					pixels_left = (y + 1) * bmp->width - x;
					scanline = (void *)(top + (y * swidth));
				} else {
					pixels_left = (bmp->height - y + 1) * bmp->width - x;
					scanline = (void *)(bottom - (y * swidth));
				}
				if (length > pixels_left)
					length = pixels_left;
				if (data + length > end)
					return BMP_INSUFFICIENT_DATA;

				/* the following code could be easily optimised by simply
				 * checking the bounds on entry and using some simply copying
				 * routines if so */
				if (size == 8) {
					for (i = 0; i < length; i++) {
						if (x >= bmp->width) {
							x = 0;
							if (++y > bmp->height)
								return BMP_DATA_ERROR;
							scanline -= bmp->width;
						}
						scanline[x++] = bmp->colour_table[(int)*data++];
					}
				} else {
					for (i = 0; i < length; i++) {
						if (x >= bmp->width) {
							x = 0;
							if (++y > bmp->height)
								return BMP_DATA_ERROR;
							scanline -= bmp->width;
						}
						if ((i & 1) == 0) {
							pixel = *data++;
							scanline[x++] = bmp->colour_table
									[pixel >> 4];
						} else {
							scanline[x++] = bmp->colour_table
									[pixel & 0xf];
						}
					}
					length = (length + 1) >> 1;
				}
				if ((length & 1) && (*data++ != 0x00))
					return BMP_DATA_ERROR;

			}
		} else {
			/* NN means perform RLE for NN pixels */
			if (bmp->reversed) {
				pixels_left = (y + 1) * bmp->width - x;
				scanline = (void *)(top + (y * swidth));
			} else {
				pixels_left = (bmp->height - y + 1) * bmp->width - x;
				scanline = (void *)(bottom - (y * swidth));
			}
			if (length > pixels_left)
				length = pixels_left;
			
			/* boundary checking */
			if (data + 1 > end)
				return BMP_INSUFFICIENT_DATA;

			/* the following code could be easily optimised by simply
			 * checking the bounds on entry and using some simply copying
			 * routines if so */
			if (size == 8) {
				pixel = bmp->colour_table[(int)*data++];
				for (i = 0; i < length; i++) {
					if (x >= bmp->width) {
						x = 0;
						if (++y > bmp->height)
							return BMP_DATA_ERROR;
						scanline -= bmp->width;
					}
					scanline[x++] = pixel;
				}
			} else {
				pixel2 = *data++;
				pixel = bmp->colour_table[pixel2 >> 4];
				pixel2 = bmp->colour_table[pixel2 & 0xf];
				for (i = 0; i < length; i++) {
					if (x >= bmp->width) {
						x = 0;
						if (++y > bmp->height)
							return BMP_DATA_ERROR;
						scanline -= bmp->width;
					}
					if ((i & 1) == 0)
						scanline[x++] = pixel;
					else
						scanline[x++] = pixel2;
				}
			}
		}
	} while (data < end);
	return BMP_OK;
}


/**
 * Finalise a BMP prior to destruction.
 *
 * \param bmp	the BMP image to finalise
 */
void bmp_finalise(bmp_image *bmp) {
	if (bmp->bitmap)
		bmp->bitmap_callbacks.bitmap_destroy(bmp->bitmap);
	bmp->bitmap = NULL;
	if (bmp->colour_table)
		free(bmp->colour_table);
	bmp->colour_table = NULL;
}


/**
 * Finalise an ICO prior to destruction.
 *
 * \param ico	the ICO image to finalise
 */
void ico_finalise(ico_collection *ico) {
	ico_image *image;

	for (image = ico->first; image; image = image->next)
		bmp_finalise(&image->bmp);
	while (ico->first) {
		image = ico->first;
		ico->first = image->next;
		free(image);
	}
}
