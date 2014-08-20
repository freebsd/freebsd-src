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
 * Generic bitmap handling (interface).
 *
 * This interface wraps the native platform-specific image format, so that
 * portable image convertors can be written.
 *
 * Bitmaps are required to be 32bpp with components in the order RR GG BB AA.
 * 
 * For example, an opaque 1x1 pixel image would yield the following bitmap 
 * data:
 * 
 * Red  : 0xff 0x00 0x00 0x00
 * Green: 0x00 0xff 0x00 0x00
 * Blue : 0x00 0x00 0xff 0x00
 *
 * Any attempt to read pixels by casting bitmap data to uint32_t or similar
 * will need to cater for the order of bytes in a word being different on
 * big and little endian systems. To avoid confusion, it is recommended
 * that pixel data is loaded as follows:
 *
 * uint32_t read_pixel(const uint8_t *bmp)
 * {
 *     //     red      green           blue              alpha
 *     return bmp[0] | (bmp[1] << 8) | (bmp[2] << 16) | (bmp[3] << 24);
 * }
 *
 * and *not* as follows:
 *
 * uint32_t read_pixel(const uint8_t *bmp)
 * {
 *     return *((uint32_t *) bmp);
 * }
 */

#ifndef _NETSURF_IMAGE_BITMAP_H_
#define _NETSURF_IMAGE_BITMAP_H_

#include <stdbool.h>
#include <stdlib.h>

#define BITMAP_NEW		0
#define BITMAP_OPAQUE		(1 << 0)	/** image is opaque */
#define BITMAP_MODIFIED		(1 << 1)	/** buffer has been modified */
#define BITMAP_CLEAR_MEMORY	(1 << 2)	/** memory should be wiped */

struct content;

/** An opaque image. */
struct bitmap;

void *bitmap_create(int width, int height, unsigned int state);
void bitmap_set_opaque(void *bitmap, bool opaque);
bool bitmap_test_opaque(void *bitmap);
bool bitmap_get_opaque(void *bitmap);
unsigned char *bitmap_get_buffer(void *bitmap);
size_t bitmap_get_rowstride(void *bitmap);
size_t bitmap_get_bpp(void *bitmap);
void bitmap_destroy(void *bitmap);
bool bitmap_save(void *bitmap, const char *path, unsigned flags);
void bitmap_modified(void *bitmap);

int bitmap_get_width(void *bitmap);
int bitmap_get_height(void *bitmap);

#endif
