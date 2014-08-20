/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Page thumbnail creation (implementation).
 *
 * Thumbnails are created by redirecting output to a sprite and rendering the
 * page at a small scale.
 */

#include <assert.h>
#include <string.h>
#include <swis.h>
#include "rufl.h"
#include "oslib/colourtrans.h"
#include "oslib/osfile.h"
#include "oslib/osspriteop.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/plotters.h"
#include "desktop/thumbnail.h"
#include "image/bitmap.h"
#include "render/font.h"
#include "riscos/bitmap.h"
#include "riscos/gui.h"
#include "utils/nsoption.h"
#include "riscos/oslib_pre7.h"
#include "riscos/thumbnail.h"
#include "riscos/tinct.h"
#include "utils/log.h"


/*	Whether we can use 32bpp sprites
*/
static int thumbnail_32bpp_available = -1;


/*	Sprite output context saving
*/
struct thumbnail_save_area {
	osspriteop_save_area *save_area;
	int context1;
	int context2;
	int context3;
};


/*	Internal prototypes
*/
static osspriteop_area *thumbnail_create_8bpp(struct bitmap *bitmap);
static void thumbnail_test(void);
static struct thumbnail_save_area* thumbnail_switch_output(
		osspriteop_area *sprite_area,
		osspriteop_header *sprite_header);
static void thumbnail_restore_output(struct thumbnail_save_area *save_area);


/**
 * Create a thumbnail of a page.
 *
 * \param  content  content structure to thumbnail
 * \param  bitmap   the bitmap to draw to
 * \param  url      the URL the thumbnail belongs to, or NULL
 */
bool thumbnail_create(hlcache_handle *content, struct bitmap *bitmap,
		nsurl *url)
{
	struct thumbnail_save_area *save_area;
	osspriteop_area *sprite_area = NULL;
	osspriteop_header *sprite_header = NULL;
	_kernel_oserror *error;
	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &ro_plotters
	};

	assert(content);
	assert(bitmap);

	/* check if we have access to 32bpp sprites natively */
	if (thumbnail_32bpp_available == -1)
		thumbnail_test();

	/* if we don't support 32bpp sprites then we redirect to an 8bpp
	 * image and then convert back. */
	if (thumbnail_32bpp_available != 1) {
		sprite_area = thumbnail_create_8bpp(bitmap);
		if (!sprite_area)
			return false;
		sprite_header = (osspriteop_header *)(sprite_area + 1);
	} else {
		const uint8_t *pixbufp = bitmap_get_buffer(bitmap);
		if (!pixbufp || !bitmap->sprite_area)
			return false;
		sprite_area = bitmap->sprite_area;
		sprite_header = (osspriteop_header *)(sprite_area + 1);
	}

	/* set up the plotters */
	ro_plot_origin_x = 0;
	ro_plot_origin_y = bitmap->height * 2;

	/* switch output and redraw */
	save_area = thumbnail_switch_output(sprite_area, sprite_header);
	if (!save_area) {
		if (thumbnail_32bpp_available != 1)
			free(sprite_area);
		return false;
	}
	rufl_invalidate_cache();
	colourtrans_set_gcol(os_COLOUR_WHITE, colourtrans_SET_BG_GCOL,
			os_ACTION_OVERWRITE, 0);

	thumbnail_redraw(content, bitmap->width, bitmap->height, &ctx);

	thumbnail_restore_output(save_area);
	rufl_invalidate_cache();

	/* if we changed to 8bpp then go back to 32bpp */
	if (thumbnail_32bpp_available != 1) {
		const uint8_t *pixbufp = bitmap_get_buffer(bitmap);
		if (!pixbufp || !bitmap->sprite_area) {
			free(sprite_area);
			return false;
		}
		error = _swix(Tinct_ConvertSprite, _INR(2,3),
				sprite_header,
				(osspriteop_header *)(bitmap->sprite_area + 1));
		free(sprite_area);
		if (error)
			return false;
	}

	/* register the thumbnail with the URL */
	if (url)
		urldb_set_thumbnail(url, bitmap);
	bitmap_modified(bitmap);
	return true;
}


/**
 * Convert a bitmap to 8bpp.
 *
 * \param  bitmap  the bitmap to convert
 * \return a sprite area containing an 8bpp sprite
 */
osspriteop_area *thumbnail_convert_8bpp(struct bitmap *bitmap)
{
	struct thumbnail_save_area *save_area;
	osspriteop_area *sprite_area = NULL;
	osspriteop_header *sprite_header = NULL;

	sprite_area = thumbnail_create_8bpp(bitmap);
	if (!sprite_area)
		return NULL;
	sprite_header = (osspriteop_header *)(sprite_area + 1);


	/* switch output and redraw */
	save_area = thumbnail_switch_output(sprite_area, sprite_header);
	if (save_area == NULL) {
		if (thumbnail_32bpp_available != 1)
			free(sprite_area);
		return false;
	}
	_swix(Tinct_Plot, _IN(2) | _IN(3) | _IN(4) | _IN(7),
			(osspriteop_header *)(bitmap->sprite_area + 1),
			0, 0,
			tinct_ERROR_DIFFUSE);
	thumbnail_restore_output(save_area);

	if (sprite_header->image != sprite_header->mask) {
		/* build the sprite mask from the alpha channel */
		void *buf = bitmap_get_buffer(bitmap);
		unsigned *dp = (unsigned *) buf;
		if (!dp)
			return sprite_area;
		int w = bitmap_get_width(bitmap);
		int h = bitmap_get_height(bitmap);
		int dp_offset = bitmap_get_rowstride(bitmap) / 4 - w;
		int mp_offset = ((sprite_header->width + 1) * 4) - w;
		byte *mp = (byte*)sprite_header + sprite_header->mask;
		bool alpha = ((unsigned)sprite_header->mode & 0x80000000U) != 0;

		while (h-- > 0) {
			int x = 0;
			for(x = 0; x < w; x++) {
				unsigned d = *dp++;
				if (alpha)
					*mp++ = (d >> 24) ^ 0xff;
				else
					*mp++ = (d < 0xff000000U) ? 0 : 0xff;
			}
			dp += dp_offset;
			mp += mp_offset;
		}
	}

	return sprite_area;
}


/**
 * Creates an 8bpp canvas.
 *
 * \param  bitmap  the bitmap to clone the size of
 * \return a sprite area containing an 8bpp sprite
 */
osspriteop_area *thumbnail_create_8bpp(struct bitmap *bitmap)
{
	unsigned image_size = ((bitmap->width + 3) & ~3) * bitmap->height;
	bool opaque = bitmap_get_opaque(bitmap);
	osspriteop_header *sprite_header = NULL;
	osspriteop_area *sprite_area = NULL;
	unsigned area_size;

	/* clone the sprite */
	area_size = sizeof(osspriteop_area) +
			sizeof(osspriteop_header) +
			image_size +
			2048;

	if (!opaque) area_size += image_size;

	sprite_area = (osspriteop_area *)malloc(area_size);
	if (!sprite_area) {
		LOG(("no memory for malloc()"));
		return NULL;
	}
	sprite_area->size = area_size;
	sprite_area->sprite_count = 1;
	sprite_area->first = 16;
	sprite_area->used = area_size;
	sprite_header = (osspriteop_header *)(sprite_area + 1);
	sprite_header->size = area_size - sizeof(osspriteop_area);
	memset(sprite_header->name, 0x00, 12);
	strcpy(sprite_header->name, "bitmap");
	sprite_header->left_bit = 0;
	sprite_header->height = bitmap->height - 1;
	sprite_header->mode = os_MODE8BPP90X90;
	sprite_header->right_bit = ((bitmap->width << 3) - 1) & 31;
	sprite_header->width = ((bitmap->width + 3) >> 2) - 1;
	sprite_header->image = sizeof(osspriteop_header) + 2048;
	sprite_header->mask = sizeof(osspriteop_header) + 2048;
	if (!opaque) sprite_header->mask += image_size;

	/* create the palette. we don't read the necessary size like
	 * we really should as we know it's going to have 256 entries
	 * of 8 bytes = 2048. */
	xcolourtrans_read_palette((osspriteop_area *)os_MODE8BPP90X90,
			(osspriteop_id)0,
			(os_palette *)(sprite_header + 1), 2048,
			(colourtrans_palette_flags)(1 << 1), 0);
	return sprite_area;
}


/**
 * Check to see whether 32bpp sprites are available.
 *
 * Rather than using Wimp_ReadSysInfo we test if 32bpp sprites are available
 * in case the user has a 3rd party patch to enable them.
 */
static void thumbnail_test(void)
{
	unsigned int area_size;
	osspriteop_area *sprite_area;

	/* try to create a 1x1 32bpp sprite */
	area_size = sizeof(osspriteop_area) +
			sizeof(osspriteop_header) + sizeof(int);
	if ((sprite_area = (osspriteop_area *)malloc(area_size)) == NULL) {
		LOG(("Insufficient memory to perform sprite test."));
		return;
	}
	sprite_area->size = area_size + 1;
	sprite_area->sprite_count = 0;
	sprite_area->first = 16;
	sprite_area->used = 16;
	if (xosspriteop_create_sprite(osspriteop_NAME, sprite_area,
			"test",	false, 1, 1, (os_mode)tinct_SPRITE_MODE))
		thumbnail_32bpp_available = 0;
	else
		thumbnail_32bpp_available = 1;
	free(sprite_area);
}


/**
 * Switches output to the specified sprite and returns the previous context.
 */
static struct thumbnail_save_area* thumbnail_switch_output(
		osspriteop_area *sprite_area,
		osspriteop_header *sprite_header)
{
	struct thumbnail_save_area *save_area;
	int size;

	/* create a save area */
	save_area = calloc(sizeof(struct thumbnail_save_area), 1);
	if (save_area == NULL) return NULL;

	/* allocate OS_SpriteOp save area */
	if (xosspriteop_read_save_area_size(osspriteop_PTR, sprite_area,
			(osspriteop_id)sprite_header, &size)) {
		free(save_area);
		return NULL;
	}

	/* create the save area */
	save_area->save_area = malloc((unsigned)size);
	if (save_area->save_area == NULL) {
		free(save_area);
		return NULL;
	}
	save_area->save_area->a[0] = 0;

	/* switch output to sprite */
	if (xosspriteop_switch_output_to_sprite(osspriteop_PTR, sprite_area,
			(osspriteop_id)sprite_header, save_area->save_area,
			0, &save_area->context1, &save_area->context2,
			&save_area->context3)) {
		free(save_area->save_area);
		free(save_area);
		return NULL;
	}
	return save_area;
}


/**
 * Restores output to the specified context, and destroys it.
 */
static void thumbnail_restore_output(struct thumbnail_save_area *save_area)
{
	/* we don't care if we err, as there's nothing we can do about it */
	xosspriteop_switch_output_to_sprite(osspriteop_PTR,
			(osspriteop_area *)save_area->context1,
			(osspriteop_id)save_area->context2,
			(osspriteop_save_area *)save_area->context3,
			0, 0, 0, 0);
	free(save_area->save_area);
	free(save_area);
}
