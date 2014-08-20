/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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

/** \file
 * Generic bitmap handling (BeOS implementation).
 *
 * This implements the interface given by desktop/bitmap.h using BBitmap.
 */

#define __STDBOOL_H__	1
//#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <GraphicsDefs.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
extern "C" {
#include "content/content.h"
#include "image/bitmap.h"
#include "utils/log.h"
}
#include "beos/bitmap.h"
#include "beos/gui.h"
#include "beos/scaffolding.h"

struct bitmap {
  BBitmap *primary;
  BBitmap *shadow; // in NetSurf's ABGR order
  BBitmap *pretile_x;
  BBitmap *pretile_y;
  BBitmap *pretile_xy;
  bool opaque;
};

#define MIN_PRETILE_WIDTH 256
#define MIN_PRETILE_HEIGHT 256

#warning TODO: check rgba order
#warning TODO: add correct locking (not strictly required)


/** Convert to BeOS RGBA32_LITTLE (strictly BGRA) from NetSurf's favoured ABGR format.
 * Copies the converted data elsewhere.  Operation is rotate left 8 bits.
 *
 * \param pixels	Array of 32-bit values, in the form of ABGR.  This will
 *			be overwritten with new data in the form of BGRA.
 * \param width		Width of the bitmap
 * \param height	Height of the bitmap
 * \param rowstride	Number of bytes to skip after each row (this
 *			implementation requires this to be a multiple of 4.)
 */
static inline void nsbeos_rgba_to_bgra(void *src, void *dst, int width, int height,
				size_t rowstride)
{
	struct abgr { uint8 a, b, g, r; };
	struct rgba { uint8 r, g, b ,a; };
	struct bgra { uint8 b, g, r, a; };
	struct rgba *from = (struct rgba *)src;
	struct bgra *to = (struct bgra *)dst;

	rowstride >>= 2;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			to[x].b = from[x].b;
			to[x].g = from[x].g;
			to[x].r = from[x].r;
			to[x].a = from[x].a;
			/*
			if (from[x].a == 0)
				*(rgb_color *)&to[x] = B_TRANSPARENT_32_BIT;
			*/
		}
		from += rowstride;
		to += rowstride;
	}
}


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
	struct bitmap *bmp = (struct bitmap *)malloc(sizeof(struct bitmap));
	if (bmp == NULL)
		return NULL;

	int32 flags = 0;
	if (state & BITMAP_CLEAR_MEMORY)
		flags |= B_BITMAP_CLEAR_TO_WHITE;

	BRect frame(0, 0, width - 1, height - 1);
	//XXX: bytes per row ?
	bmp->primary = new BBitmap(frame, flags, B_RGBA32);
	bmp->shadow = new BBitmap(frame, flags, B_RGBA32);

	bmp->pretile_x = bmp->pretile_y = bmp->pretile_xy = NULL;

	bmp->opaque = (state & BITMAP_OPAQUE) != 0;

	return bmp;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque   whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	bitmap->opaque = opaque;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return whether  the bitmap is opaque
 */
bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
/* todo: test if bitmap is opaque */
	return false;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return bitmap->opaque;
}


/**
 * Return a pointer to the pixel data in a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return pointer to the pixel buffer
 *
 * The pixel data is packed as BITMAP_FORMAT, possibly with padding at the end
 * of rows. The width of a row in bytes is given by bitmap_get_rowstride().
 */

unsigned char *bitmap_get_buffer(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return (unsigned char *)(bitmap->shadow->Bits());
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return (bitmap->primary->BytesPerRow());
}


/**
 * Find the bytes per pixels of a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixels of the bitmap
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}


static void
nsbeos_bitmap_free_pretiles(struct bitmap *bitmap)
{
#define FREE_TILE(XY) if (bitmap->pretile_##XY) delete (bitmap->pretile_##XY); bitmap->pretile_##XY = NULL
	FREE_TILE(x);
	FREE_TILE(y);
	FREE_TILE(xy);
#undef FREE_TILE
}

/**
 * Free a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	nsbeos_bitmap_free_pretiles(bitmap);
	delete bitmap->primary;
	delete bitmap->shadow;
	free(bitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \param  path     pathname for file
 * \param  flags    modify the behaviour of the save
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *vbitmap, const char *path, unsigned flags)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	BTranslatorRoster *roster = BTranslatorRoster::Default();
	BBitmapStream stream(bitmap->primary);
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE);
	uint32 type = B_PNG_FORMAT;

	if (file.InitCheck() < B_OK)
		return false;

	if (roster->Translate(&stream, NULL, NULL, &file, type) < B_OK)
		return false;

	return true;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	// convert the shadow (ABGR) to into the primary bitmap
	nsbeos_rgba_to_bgra(bitmap->shadow->Bits(), bitmap->primary->Bits(),
		bitmap->primary->Bounds().Width() + 1,
		bitmap->primary->Bounds().Height() + 1,
		bitmap->primary->BytesPerRow());
	nsbeos_bitmap_free_pretiles(bitmap);
}

int bitmap_get_width(void *vbitmap){
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	return bitmap->primary->Bounds().Width() + 1;
}

int bitmap_get_height(void *vbitmap){
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	return bitmap->primary->Bounds().Height() + 1;
}

static BBitmap *
nsbeos_bitmap_generate_pretile(BBitmap *primary, int repeat_x, int repeat_y)
{
	int width = primary->Bounds().Width() + 1;
	int height = primary->Bounds().Height() + 1;
	size_t primary_stride = primary->BytesPerRow();
	BRect frame(0, 0, width * repeat_x - 1, height * repeat_y - 1);
	BBitmap *result = new BBitmap(frame, 0, B_RGBA32);

	char *target_buffer = (char *)result->Bits();
	int x,y,row;
	/* This algorithm won't work if the strides are not multiples */
	assert((size_t)(result->BytesPerRow()) ==
		(primary_stride * repeat_x));

	if (repeat_x == 1 && repeat_y == 1) {
		delete result;
		// just return a copy
		return new BBitmap(primary);
	}

	for (y = 0; y < repeat_y; ++y) {
		char *primary_buffer = (char *)primary->Bits();
		for (row = 0; row < height; ++row) {
			for (x = 0; x < repeat_x; ++x) {
				memcpy(target_buffer,
				       primary_buffer, primary_stride);
				target_buffer += primary_stride;
			}
			primary_buffer += primary_stride;
		}
	}
	return result;

}

/**
 * The primary image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_primary(struct bitmap* bitmap)
{
	return bitmap->primary;
}

/**
 * The X-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_x(struct bitmap* bitmap)
{
	if (!bitmap->pretile_x) {
		int width = bitmap->primary->Bounds().Width() + 1;
		int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
		LOG(("Pretiling %p for X*%d", bitmap, xmult));
		bitmap->pretile_x = nsbeos_bitmap_generate_pretile(bitmap->primary, xmult, 1);
	}
	return bitmap->pretile_x;

}

/**
 * The Y-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_y(struct bitmap* bitmap)
{
	if (!bitmap->pretile_y) {
		int height = bitmap->primary->Bounds().Height() + 1;
		int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
		LOG(("Pretiling %p for Y*%d", bitmap, ymult));
		bitmap->pretile_y = nsbeos_bitmap_generate_pretile(bitmap->primary, 1, ymult);
	}
  return bitmap->pretile_y;
}

/**
 * The XY-pretiled image associated with this bitmap object.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
BBitmap *
nsbeos_bitmap_get_pretile_xy(struct bitmap* bitmap)
{
	if (!bitmap->pretile_xy) {
		int width = bitmap->primary->Bounds().Width() + 1;
		int height = bitmap->primary->Bounds().Height() + 1;
		int xmult = (MIN_PRETILE_WIDTH + width - 1)/width;
		int ymult = (MIN_PRETILE_HEIGHT + height - 1)/height;
		LOG(("Pretiling %p for X*%d Y*%d", bitmap, xmult, ymult));
		bitmap->pretile_xy = nsbeos_bitmap_generate_pretile(bitmap->primary, xmult, ymult);
	}
  return bitmap->pretile_xy;
}
