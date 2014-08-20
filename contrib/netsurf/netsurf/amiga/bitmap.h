/*
 * Copyright 2008,2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_BITMAP_H
#define AMIGA_BITMAP_H
#include "image/bitmap.h"

#include <exec/types.h>
#include <proto/graphics.h>
#include <intuition/classusr.h>
#include <libraries/Picasso96.h>

#define AMI_BITMAP_FORMAT RGBFB_R8G8B8A8

struct bitmap {
	int width;
	int height;
	UBYTE *pixdata;
	bool opaque;
	struct BitMap *nativebm;
	int nativebmwidth;
	int nativebmheight;
	PLANEPTR native_mask;
	Object *dto;
	char *url;   /* temporary storage space */
	char *title; /* temporary storage space */
	ULONG *icondata; /* for appicons */
};

struct BitMap *ami_bitmap_get_native(struct bitmap *bitmap,
				int width, int height, struct BitMap *friendbm);
PLANEPTR ami_bitmap_get_mask(struct bitmap *bitmap, int width,
				int height, struct BitMap *n_bm);

Object *ami_datatype_object_from_bitmap(struct bitmap *bitmap);
struct bitmap *ami_bitmap_from_datatype(char *filename);

#ifndef __amigaos4__
void ami_bitmap_argb_to_rgba(struct bitmap *bm);
#endif

#endif
