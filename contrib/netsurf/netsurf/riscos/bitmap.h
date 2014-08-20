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

#ifndef _NETSURF_RISCOS_BITMAP_H_
#define _NETSURF_RISCOS_BITMAP_H_

#include <stdbool.h>
#include "oslib/osspriteop.h"
#include "image/bitmap.h"

#define BITMAP_SAVE_FULL_ALPHA	(1 << 0)	/** save with full alpha channel (if not opaque) */

struct osspriteop_area;

struct bitmap {
	int width;
	int height;

	unsigned int state;

	osspriteop_area *sprite_area;	/** Uncompressed data, or NULL */
};

void bitmap_overlay_sprite(struct bitmap *bitmap, const osspriteop_header *s);

#endif
