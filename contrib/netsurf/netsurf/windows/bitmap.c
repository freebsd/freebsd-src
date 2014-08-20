/*
 * Copyright 2008 Vincent Sanders <vince@simtec.co.uk>
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <sys/types.h>
#include <string.h>

#include "utils/config.h"

#include <windows.h>

#include "image/bitmap.h"
#include "windows/bitmap.h"

#include "utils/log.h"

/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  state   a flag word indicating the initial state
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;
	BITMAPV5HEADER *pbmi;
	HBITMAP windib;
	uint8_t *pixdata;

	LOG(("width %d, height %d, state %u",width,height,state));

	pbmi = calloc(1, sizeof(BITMAPV5HEADER));
	if (pbmi == NULL) {
		return NULL;
	}
	pbmi->bV5Size = sizeof(BITMAPV5HEADER); 
	pbmi->bV5Width = width; 
	pbmi->bV5Height = -height; 
	pbmi->bV5Planes = 1; 
	pbmi->bV5BitCount = 32;
	pbmi->bV5Compression = BI_BITFIELDS;

	pbmi->bV5RedMask = 0xff; /* red mask */
	pbmi->bV5GreenMask = 0xff00; /* green mask */
	pbmi->bV5BlueMask = 0xff0000; /* blue mask */
	pbmi->bV5AlphaMask = 0xff000000; /* alpha mask */

	windib = CreateDIBSection(NULL, (BITMAPINFO *)pbmi, DIB_RGB_COLORS, (void **)&pixdata, NULL, 0);


	if (windib == NULL) {
		free(pbmi);
		return NULL;
	}

	bitmap = calloc(1 , sizeof(struct bitmap));
	if (bitmap == NULL) {
		DeleteObject(windib);
		free(pbmi);
		return NULL;
	}

	bitmap->width = width;
	bitmap->height = height;
	bitmap->windib = windib;
	bitmap->pbmi = pbmi;
	bitmap->pixdata = pixdata;
	bitmap->opaque = false;

	LOG(("bitmap %p", bitmap));

	return bitmap;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;
	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return NULL;
	}
		
	return bm->pixdata;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}

	return (bm->width) * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return;
	}

	DeleteObject(bm->windib);
	free(bm->pbmi);
	free(bm);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path    pathname for file
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *bitmap) {
}

/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *bitmap, bool opaque)
{	
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return;
	}

	LOG(("setting bitmap %p to %s", bm, opaque?"opaque":"transparent"));
	bm->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *bitmap)
{
	int tst;
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return false;
	}

	tst = bm->width * bm->height;

	while (tst-- > 0) {
		if (bm->pixdata[(tst << 2) + 3] != 0xff) {
			LOG(("bitmap %p has transparency",bm));
			return false;		     
		}   
	}
	LOG(("bitmap %p is opaque", bm));
	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return false;
	}

	return bm->opaque;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}

	return(bm->width);
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if (bitmap == NULL) {
		LOG(("NULL bitmap!"));
		return 0;
	}

	return(bm->height);
}

size_t bitmap_get_bpp(void *bitmap)
{
	return 4;
}

struct bitmap *bitmap_scale(struct bitmap *prescale, int width, int height)
{
	struct bitmap *ret = malloc(sizeof(struct bitmap));
	int i, ii, v, vv;
	uint32_t *retpixdata, *inpixdata; /* 4 byte types for quicker
					   * transfer */
	if (ret == NULL)
		return NULL;
	retpixdata = malloc(width * height * 4);
	if (retpixdata == NULL) {
		free(ret);
		return NULL;
	}
	inpixdata = (uint32_t *)prescale->pixdata;
	ret->pixdata = (uint8_t *)retpixdata;
	ret->height = height;
	ret->width = width;
	for (i = 0; i < height; i++) {
		v = i * width;
		vv = (int)((i * prescale->height) / height) * prescale->width;
		for (ii = 0; ii < width; ii++) {
			retpixdata[v + ii] = inpixdata[vv + (int)
					((ii * prescale->width) / width)];
		}
	}
	return ret;
	
}

struct bitmap *bitmap_pretile(struct bitmap *untiled, int width, int height,
		bitmap_flags_t flags)
{
	struct bitmap *ret = malloc(sizeof(struct bitmap));
	if (ret == NULL)
		return NULL;
	int i, hrepeat, vrepeat, repeat;
	vrepeat = ((flags & BITMAPF_REPEAT_Y) != 0) ? 
			((height + untiled->height - 1) / untiled->height) : 1;
	hrepeat = ((flags & BITMAPF_REPEAT_X) != 0) ? 
			((width + untiled->width - 1) / untiled->width) : 1;
	width = untiled->width * hrepeat;
	height = untiled->height * vrepeat;
	uint8_t *indata = untiled->pixdata;
	uint8_t *newdata = malloc(4 * width * height);
	if (newdata == NULL) {
		free(ret);
		return NULL;
	}
	ret->pixdata = newdata;
	size_t stride = untiled->width * 4;

	/* horizontal tiling */
	for (i = 0; i < untiled->height; i++) {
		for (repeat = 0; repeat < hrepeat; repeat ++) {
			memcpy(newdata, indata, stride);
			newdata += stride;
		}
		indata += stride;
	}
	
	/* vertical tiling */
	stride = untiled->height * width * 4;
	newdata = ret->pixdata + stride;
	indata = ret->pixdata;

	for (repeat = 1; repeat < vrepeat; repeat++) {
		memcpy(newdata, indata, stride);
		newdata += stride;		
	}
	ret->width = width;
	ret->height = height;
	return ret;
}
