/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2008 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Generic bitmap handling (RISC OS implementation).
 *
 * This implements the interface given by desktop/bitmap.h using RISC OS
 * sprites.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <swis.h>
#include <unixlib/local.h>
#include "oslib/osfile.h"
#include "oslib/osfind.h"
#include "oslib/osgbpb.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "content/content.h"
#include "image/bitmap.h"
#include "riscos/bitmap.h"
#include "riscos/image.h"
#include "utils/nsoption.h"
#include "riscos/palettes.h"
#include "riscos/content-handlers/sprite.h"
#include "riscos/tinct.h"
#include "utils/filename.h"
#include "utils/log.h"
#include "utils/utils.h"

/** Colour in the overlay sprite that allows the bitmap to show through */
#define OVERLAY_INDEX 0xfe

/** Size of buffer used when constructing mask data to be saved */
#define SAVE_CHUNK_SIZE 4096


/**
 * Initialise a bitmaps sprite area.
 *
 * \param  bitmap  the bitmap to initialise
 * \param  clear   whether to clear the image ready for use
 */

static bool bitmap_initialise(struct bitmap *bitmap)
{
	unsigned int area_size;
	osspriteop_area *sprite_area;
	osspriteop_header *sprite;

	assert(!bitmap->sprite_area);

	area_size = 16 + 44 + bitmap->width * bitmap->height * 4;
	if (bitmap->state & BITMAP_CLEAR_MEMORY)
		bitmap->sprite_area = calloc(1, area_size);
	else
		bitmap->sprite_area = malloc(area_size);

	if (!bitmap->sprite_area)
		return false;

	/* area control block */
	sprite_area = bitmap->sprite_area;
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;

	/* sprite control block */
	sprite = (osspriteop_header *) (sprite_area + 1);
	sprite->size = area_size - 16;
	memset(sprite->name, 0x00, 12);
	strncpy(sprite->name, "bitmap", 12);
	sprite->width = bitmap->width - 1;
	sprite->height = bitmap->height - 1;
	sprite->left_bit = 0;
	sprite->right_bit = 31;
	sprite->image = sprite->mask = 44;
	sprite->mode = tinct_SPRITE_MODE;

	return true;
}


/**
 * Create a bitmap.
 *
 * \param  width   width of image in pixels
 * \param  height  width of image in pixels
 * \param  clear   whether to clear the image ready for use
 * \return an opaque struct bitmap, or NULL on memory exhaustion
 */

void *bitmap_create(int width, int height, unsigned int state)
{
	struct bitmap *bitmap;

	if (width == 0 || height == 0)
		return NULL;

	bitmap = calloc(1, sizeof(struct bitmap));
	if (!bitmap)
		return NULL;
	bitmap->width = width;
	bitmap->height = height;
	bitmap->state = state;

	return bitmap;
}


/**
 * Overlay a sprite onto the given bitmap
 *
 * \param  bitmap  bitmap object
 * \param  s       8bpp sprite to be overlayed onto bitmap
 */

void bitmap_overlay_sprite(struct bitmap *bitmap, const osspriteop_header *s)
{
	const os_colour *palette;
	const byte *sp, *mp;
	bool masked = false;
	bool alpha = false;
	os_error *error;
	int dp_offset;
	int sp_offset;
	unsigned *dp;
	int x, y;
	int w, h;

	assert(sprite_bpp(s) == 8);

	if ((unsigned)s->mode & 0x80000000U)
		alpha = true;

	error = xosspriteop_read_sprite_info(osspriteop_PTR,
			(osspriteop_area *)0x100,
			(osspriteop_id)s,
			&w, &h, NULL, NULL);
	if (error) {
		LOG(("xosspriteop_read_sprite_info: 0x%x:%s",
				error->errnum, error->errmess));
		return;
	}
	sp_offset = ((s->width + 1) * 4) - w;

	if (w > bitmap->width)
		w = bitmap->width;
	if (h > bitmap->height)
		h = bitmap->height;

	dp_offset = bitmap_get_rowstride(bitmap) / 4;

	dp = (void*)bitmap_get_buffer(bitmap);
	if (!dp)
		return;
	sp = (byte*)s + s->image;
	mp = (byte*)s + s->mask;

	sp += s->left_bit / 8;
	mp += s->left_bit / 8;

	if (s->image > (int)sizeof(*s))
		palette = (os_colour*)(s + 1);
	else
		palette = default_palette8;

	if (s->mask != s->image) {
		masked = true;
		bitmap_set_opaque(bitmap, false);
	}

	/* (partially-)transparent pixels in the overlayed sprite retain
	 * their transparency in the output bitmap; opaque sprite pixels
	 * are also propagated to the bitmap, except those which are the
	 * OVERLAY_INDEX colour which allow the original bitmap contents to
	 * show through */
	for (y = 0; y < h; y++) {
		unsigned *sdp = dp;
		for(x = 0; x < w; x++) {
			os_colour d = ((unsigned)palette[(*sp) << 1]) >> 8;
			if (*sp++ == OVERLAY_INDEX)
				d = *dp;
			if (masked) {
				if (alpha)
					d |= ((*mp << 24) ^ 0xff000000U);
				else if (*mp)
					d |= 0xff000000U;
			}
			*dp++ = d;
			mp++;
		}
		dp = sdp + dp_offset;
		sp += sp_offset;
		mp += sp_offset;
	}
}


/**
 * Sets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  opaque  whether the bitmap should be plotted opaque
 */
void bitmap_set_opaque(void *vbitmap, bool opaque)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);

	if (opaque)
		bitmap->state |= BITMAP_OPAQUE;
	else
		bitmap->state &= ~BITMAP_OPAQUE;
}


/**
 * Tests whether a bitmap has an opaque alpha channel
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return whether the bitmap is opaque
 */
bool bitmap_test_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	unsigned char *sprite;
	unsigned int width, height, size;
	osspriteop_header *sprite_header;
	unsigned *p, *ep;

	assert(bitmap);

	sprite = bitmap_get_buffer(bitmap);
	if (!sprite)
		return false;

	width = bitmap_get_rowstride(bitmap);

	sprite_header = (osspriteop_header *) (bitmap->sprite_area + 1);

	height = (sprite_header->height + 1);
	
	size = width * height;
	
	p = (void *) sprite;

	ep = (void *) (sprite + (size & ~31));
	while (p < ep) {
		/* \todo prefetch(p, 128)? */
		if (((p[0] & p[1] & p[2] & p[3] & p[4] & p[5] & p[6] & p[7])
				& 0xff000000U) != 0xff000000U)
			return false;
		p += 8;
	}

	ep = (void *) (sprite + size);
	while (p < ep) {
		if ((*p & 0xff000000U) != 0xff000000U) return false;
		p++;
	}

	return true;
}


/**
 * Gets whether a bitmap should be plotted opaque
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
bool bitmap_get_opaque(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);
	return (bitmap->state & BITMAP_OPAQUE);
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

unsigned char *bitmap_get_buffer(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	assert(bitmap);

	/* dynamically create the buffer */
	if (bitmap->sprite_area == NULL) {
		if (!bitmap_initialise(bitmap))
			return NULL;
	}

	/* image data area should exist */
	if (bitmap->sprite_area)
		return ((unsigned char *) (bitmap->sprite_area)) + 16 + 44;

	return NULL;
}


/**
 * Find the width of a pixel row in bytes.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \return width of a pixel row in the bitmap
 */

size_t bitmap_get_rowstride(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->width * 4;
}


/**
 * Free a bitmap.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */

void bitmap_destroy(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;

	assert(bitmap);

	/* destroy bitmap */
	if (bitmap->sprite_area) {
		free(bitmap->sprite_area);
	}

	free(bitmap);
}


/**
 * Save a bitmap in the platform's native format.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 * \param  path	   pathname for file
 * \param  flags   modify the behaviour of the save
 * \return true on success, false on error and error reported
 */

bool bitmap_save(void *vbitmap, const char *path, unsigned flags)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	os_error *error;

	if (!bitmap->sprite_area)
		bitmap_get_buffer(bitmap);
	if (!bitmap->sprite_area)
		return false;

	if (bitmap_get_opaque(bitmap)) {
		error = xosspriteop_save_sprite_file(osspriteop_USER_AREA,
				(bitmap->sprite_area), path);
		if (error) {
			LOG(("xosspriteop_save_sprite_file: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
			return false;
		}
		return true;
	} else {
		/* to make the saved sprite useful we must convert from 'Tinct'
		 * format to either a bi-level mask or a Select-style full
		 * alpha channel */
		osspriteop_area *area = bitmap->sprite_area;
		osspriteop_header *hdr = (void *) ((char *) area + area->first);
		unsigned width = hdr->width + 1, height = hdr->height + 1;
		unsigned image_size = height * width * 4;
		unsigned char *chunk_buf;
		unsigned *p, *elp, *eip;
		unsigned mask_size;
		size_t chunk_pix;
		struct {
			osspriteop_area   area;
			osspriteop_header hdr;
		} file_hdr;
		os_fw fw;

		/* we only support 32bpp sprites */
		if ((((unsigned)hdr->mode >> 27)&15) != 6) {
			assert(!"Unsupported sprite format in bitmap_save");
			return false;
		}

		chunk_buf = malloc(SAVE_CHUNK_SIZE);
		if (!chunk_buf) {
			warn_user("NoMemory", NULL);
			return false;
		}

		file_hdr.area = *area;
		file_hdr.hdr  = *hdr;

		if (flags & BITMAP_SAVE_FULL_ALPHA) {
			mask_size = ((width + 3) & ~3) * height;
			chunk_pix = SAVE_CHUNK_SIZE;
			file_hdr.hdr.mode = (os_mode)((unsigned)file_hdr.hdr.mode
					| (1U<<31));
		} else {
			mask_size = (((width + 31) & ~31)/8) * height;
			chunk_pix = SAVE_CHUNK_SIZE<<3;
			file_hdr.hdr.mode = (os_mode)((unsigned)file_hdr.hdr.mode
					& ~(1U<<31));
		}

		file_hdr.area.sprite_count = 1;
		file_hdr.area.first = sizeof(file_hdr.area);
		file_hdr.area.used = sizeof(file_hdr) + image_size + mask_size;

		file_hdr.hdr.image = sizeof(file_hdr.hdr);
		file_hdr.hdr.mask = file_hdr.hdr.image + image_size;
		file_hdr.hdr.size = file_hdr.hdr.mask + mask_size;

		error = xosfind_openoutw(0, path, NULL, &fw);
		if (error) {
			LOG(("xosfind_openoutw: 0x%x: %s",
					error->errnum, error->errmess));
			free(chunk_buf);
			warn_user("SaveError", error->errmess);
			return false;
		}

		p = (void *) ((char *) hdr + hdr->image);

		/* write out the area header, sprite header and image data */
		error = xosgbpb_writew(fw, (byte*)&file_hdr + 4,
				sizeof(file_hdr)-4, NULL);
		if (!error)
			error = xosgbpb_writew(fw, (byte*)p, image_size, NULL);
		if (error) {
			LOG(("xosgbpb_writew: 0x%x: %s", error->errnum, error->errmess));
			free(chunk_buf);
			xosfind_closew(fw);
			warn_user("SaveError", error->errmess);
			return false;
		}

		/* then write out the mask data in chunks */
		eip = p + (width * height);  /* end of image */
		elp = p + width;  /* end of line */

		while (p < eip) {
			unsigned char *dp = chunk_buf;
			unsigned *ep = p + chunk_pix;
			if (ep > elp) ep = elp;

			if (flags & BITMAP_SAVE_FULL_ALPHA) {
				while (p < ep) {
					*dp++ = ((unsigned char*)p)[3];
					p++;
				}
			}
			else {
				unsigned char mb = 0;
				int msh = 0;
				while (p < ep) {
					if (((unsigned char*)p)[3]) mb |= (1 << msh);
					if (++msh >= 8) {
						*dp++ = mb;
						msh = 0;
						mb = 0;
					}
					p++;
				}
				if (msh > 0) *dp++ = mb;
			}

			if (p >= elp) {  /* end of line yet? */
				/* align to word boundary */
				while ((int)dp & 3) *dp++ = 0;
				/* advance end of line pointer */
				elp += width;
			}
			error = xosgbpb_writew(fw, (byte*)chunk_buf, dp-chunk_buf, NULL);
			if (error) {
				LOG(("xosgbpb_writew: 0x%x: %s",
					error->errnum, error->errmess));
				free(chunk_buf);
				xosfind_closew(fw);
				warn_user("SaveError", error->errmess);
				return false;
			}
		}

		error = xosfind_closew(fw);
		if (error) {
			LOG(("xosfind_closew: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		error = xosfile_set_type(path, osfile_TYPE_SPRITE);
		if (error) {
			LOG(("xosfile_set_type: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("SaveError", error->errmess);
		}

		free(chunk_buf);
		return true;
	}
}


/**
 * The bitmap image has changed, so flush any persistent cache.
 *
 * \param  bitmap  a bitmap, as returned by bitmap_create()
 */
void bitmap_modified(void *vbitmap) {
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	bitmap->state |= BITMAP_MODIFIED;
}


int bitmap_get_width(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->width;
}


int bitmap_get_height(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *) vbitmap;
	return bitmap->height;
}


/**
 * Find the bytes per pixel of a bitmap
 *
 * \param  vbitmap  a bitmap, as returned by bitmap_create()
 * \return bytes per pixel
 */

size_t bitmap_get_bpp(void *vbitmap)
{
	struct bitmap *bitmap = (struct bitmap *)vbitmap;
	assert(bitmap);
	return 4;
}

