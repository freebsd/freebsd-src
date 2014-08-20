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

#ifndef _NETSURF_WINDOWS_BITMAP_H_
#define _NETSURF_WINDOWS_BITMAP_H_

#include "desktop/plotters.h"

struct bitmap {
	HBITMAP windib;
	BITMAPV5HEADER *pbmi;
	int width;
	int height;
	uint8_t *pixdata;
	bool opaque;
};

struct bitmap *bitmap_scale(struct bitmap *prescale, int width, int height);
struct bitmap *bitmap_pretile(struct bitmap *untiled, int width, int height,
		bitmap_flags_t flags);

#endif
