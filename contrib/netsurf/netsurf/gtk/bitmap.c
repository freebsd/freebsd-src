/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Generic bitmap handling (GDK / GTK+ implementation).
 *
 * This implements the interface given by desktop/bitmap.h using GdkPixbufs.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <cairo.h>
#include <gtk/gtk.h>

#include "content/content.h"
#include "gtk/scaffolding.h"
#include "gtk/bitmap.h"
#include "image/bitmap.h"
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
	struct bitmap *gbitmap;

	gbitmap = calloc(1, sizeof(struct bitmap));
	if (gbitmap != NULL) {
		if ((state & BITMAP_OPAQUE) != 0) {
			gbitmap->surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
		} else {
			gbitmap->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		}

		if (cairo_surface_status(gbitmap->surface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(gbitmap->surface);
			free(gbitmap);
			gbitmap = NULL;
		}
	}

	return gbitmap;
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque   whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	cairo_format_t fmt;
	cairo_surface_t *nsurface = NULL;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	if (fmt == CAIRO_FORMAT_RGB24) {
		if (opaque == false) {
			/* opaque to transparent */
			nsurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
					cairo_image_surface_get_width(gbitmap->surface), 
					cairo_image_surface_get_height(gbitmap->surface));

		}
		
	} else {
		if (opaque == true) {
			/* transparent to opaque */
			nsurface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 
					cairo_image_surface_get_width(gbitmap->surface), 
					cairo_image_surface_get_height(gbitmap->surface));

		}
	}

	if (nsurface != NULL) {
		if (cairo_surface_status(nsurface) != CAIRO_STATUS_SUCCESS) {
			cairo_surface_destroy(nsurface);
		} else {
			memcpy(cairo_image_surface_get_data(nsurface), 
			       cairo_image_surface_get_data(gbitmap->surface), 
			       cairo_image_surface_get_stride(gbitmap->surface) * cairo_image_surface_get_height(gbitmap->surface));
			cairo_surface_destroy(gbitmap->surface);
			gbitmap->surface = nsurface;

			cairo_surface_mark_dirty(gbitmap->surface);

		}

	}	
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	unsigned char *pixels;
	int pcount;
	int ploop;

	assert(gbitmap);

	pixels = cairo_image_surface_get_data(gbitmap->surface);

	pcount = cairo_image_surface_get_stride(gbitmap->surface) * 
		cairo_image_surface_get_height(gbitmap->surface);

	for (ploop = 3; ploop < pcount; ploop += 4) {
		if (pixels[ploop] != 0xff) {
			return false;
		}		
	}

	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	cairo_format_t fmt;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	if (fmt == CAIRO_FORMAT_RGB24) {
		return true;
	}

	return false;
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
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	int pixel_loop;
	int pixel_count;
	uint8_t *pixels;
	uint32_t t, r, g, b;
	cairo_format_t fmt;

	assert(gbitmap);
	
	cairo_surface_flush(gbitmap->surface);
	pixels = cairo_image_surface_get_data(gbitmap->surface);

	if (!gbitmap->converted)
		return pixels;

	fmt = cairo_image_surface_get_format(gbitmap->surface);
	pixel_count = cairo_image_surface_get_width(gbitmap->surface) *
			cairo_image_surface_get_height(gbitmap->surface);

	if (fmt == CAIRO_FORMAT_RGB24) {
		/* Opaque image */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			/* Cairo surface is ARGB, written in native endian */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			b = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			r = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];
#else
			t = pixels[4 * pixel_loop + 0];
			r = pixels[4 * pixel_loop + 1];
			g = pixels[4 * pixel_loop + 2];
			b = pixels[4 * pixel_loop + 3];
#endif

			/* Core bitmaps always have a component order of rgba,
			 * regardless of system endianness */
			pixels[4 * pixel_loop + 0] = r;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = b;
			pixels[4 * pixel_loop + 3] = t;
		}
	} else {
		/* Alpha image: de-multiply alpha */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			b = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			r = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];
#else
			t = pixels[4 * pixel_loop + 0];
			r = pixels[4 * pixel_loop + 1];
			g = pixels[4 * pixel_loop + 2];
			b = pixels[4 * pixel_loop + 3];
#endif

			if (t != 0) {
				r = (r << 8) / t;
				g = (g << 8) / t;
				b = (b << 8) / t;

				r = (r > 255) ? 255 : r;
				g = (g > 255) ? 255 : g;
				b = (b > 255) ? 255 : b;
			} else {
				r = g = b = 0;
			}

			pixels[4 * pixel_loop + 0] = r;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = b;
			pixels[4 * pixel_loop + 3] = t;
		}
	}

	gbitmap->converted = false;
	
	return (unsigned char *) pixels;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_stride(gbitmap->surface);
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return 4;
}



/**
 * Free a bitmap.
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *vbitmap)
{
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	if (gbitmap->surface != NULL) {
		cairo_surface_destroy(gbitmap->surface);
	}
	if (gbitmap->scsurface != NULL) {
		cairo_surface_destroy(gbitmap->scsurface);
	}
	free(gbitmap);
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
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return false;
}


/**
 * The bitmap image has changed, so flush any persistant cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	int pixel_loop;
	int pixel_count; 
	uint8_t *pixels;
	uint32_t t, r, g, b;
	cairo_format_t fmt;

	assert(gbitmap);

	fmt = cairo_image_surface_get_format(gbitmap->surface);

	pixel_count = cairo_image_surface_get_width(gbitmap->surface) * 
		cairo_image_surface_get_height(gbitmap->surface);
	pixels = cairo_image_surface_get_data(gbitmap->surface);

	if (gbitmap->converted) {
		cairo_surface_mark_dirty(gbitmap->surface);
		return;
	}

	if (fmt == CAIRO_FORMAT_RGB24) {
		/* Opaque image */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			/* Core bitmaps always have a component order of rgba,
			 * regardless of system endianness */
			r = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			b = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];

			/* Cairo surface is ARGB, written in native endian */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			pixels[4 * pixel_loop + 0] = b;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = r;
			pixels[4 * pixel_loop + 3] = t;
#else
			pixels[4 * pixel_loop + 0] = t;
			pixels[4 * pixel_loop + 1] = r;
			pixels[4 * pixel_loop + 2] = g;
			pixels[4 * pixel_loop + 3] = b;
#endif
		}
	} else {
		/* Alpha image: pre-multiply alpha */
		for (pixel_loop=0; pixel_loop < pixel_count; pixel_loop++) {
			r = pixels[4 * pixel_loop + 0];
			g = pixels[4 * pixel_loop + 1];
			b = pixels[4 * pixel_loop + 2];
			t = pixels[4 * pixel_loop + 3];

			if (t != 0) {
				r = ((r * (t + 1)) >> 8) & 0xff;
				g = ((g * (t + 1)) >> 8) & 0xff;
				b = ((b * (t + 1)) >> 8) & 0xff;
			} else {
				r = g = b = 0;
			}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
			pixels[4 * pixel_loop + 0] = b;
			pixels[4 * pixel_loop + 1] = g;
			pixels[4 * pixel_loop + 2] = r;
			pixels[4 * pixel_loop + 3] = t;
#else
			pixels[4 * pixel_loop + 0] = t;
			pixels[4 * pixel_loop + 1] = r;
			pixels[4 * pixel_loop + 2] = g;
			pixels[4 * pixel_loop + 3] = b;
#endif
		}
	}
	
	cairo_surface_mark_dirty(gbitmap->surface);

	gbitmap->converted = true;
}

int bitmap_get_width(void *vbitmap){
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_width(gbitmap->surface);
}

int bitmap_get_height(void *vbitmap){
	struct bitmap *gbitmap = (struct bitmap *)vbitmap;
	assert(gbitmap);

	return cairo_image_surface_get_height(gbitmap->surface);
}


