/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "oslib/font.h"

#include "rufl_internal.h"

static int rufl_unicode_map_search_cmp(const void *keyval, const void *datum);

/**
 * Read a font's metrics (sized for a 1pt font)
 */
rufl_code rufl_font_metrics(const char *font_family, rufl_style font_style,
		os_box *bbox, int *xkern, int *ykern, int *italic,
		int *ascent, int *descent,
		int *xheight, int *cap_height,
		signed char *uline_position, unsigned char *uline_thickness)
{
	unsigned int font;
	font_f f;
	int misc_size;
	font_metrics_misc_info *misc_info;
	rufl_code code;

	code = rufl_find_font_family(font_family, font_style, &font,
			NULL, NULL);
	if (code != rufl_OK)
		return code;

	code = rufl_find_font(font, 16 /* 1pt */, NULL, &f);
	if (code != rufl_OK)
		return code;

	rufl_fm_error = xfont_read_font_metrics(f, 0, 0, 0, 0, 0,
			0, 0, 0, 0, &misc_size, 0);
	if (rufl_fm_error) {
		LOG("xfont_read_font_metrics: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	if (misc_size == 0) {
		LOG("no miscellaneous information in metrics for %s",
				rufl_font_list[font].identifier);
		/** \todo better error code */
		return rufl_FONT_NOT_FOUND;
	}

	misc_info = (font_metrics_misc_info *)malloc(misc_size);
	if (!misc_info)
		return rufl_OUT_OF_MEMORY;

	rufl_fm_error = xfont_read_font_metrics(f, 0, 0, 0, misc_info, 0,
			0, 0, 0, 0, 0, 0);
	if (rufl_fm_error) {
		LOG("xfont_read_font_metrics: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		free(misc_info);
		return rufl_FONT_MANAGER_ERROR;
	}

	/* and fill in output */
	if (bbox) {
		bbox->x0 = misc_info->x0;
		bbox->y0 = misc_info->y0;
		bbox->x1 = misc_info->x1;
		bbox->y1 = misc_info->y1;
	}

	if (xkern)
		(*xkern) = misc_info->xkern;

	if (ykern)
		(*ykern) = misc_info->ykern;

	if (italic)
		(*italic) = misc_info->italic_correction;

	if (ascent)
		(*ascent) = misc_info->ascender;

	if (descent)
		(*descent) = misc_info->descender;

	if (xheight)
		(*xheight) = misc_info->xheight;

	if (cap_height)
		(*cap_height) = misc_info->cap_height;

	if (uline_position)
		(*uline_position) = misc_info->underline_position;

	if (uline_thickness)
		(*uline_thickness) = misc_info->underline_thickness;

	free(misc_info);

	return rufl_OK;
}

/**
 * Read a glyph's metrics
 */
rufl_code rufl_glyph_metrics(const char *font_family,
		rufl_style font_style, unsigned int font_size,
		const char *string, size_t length,
		int *x_bearing, int *y_bearing,
		int *width, int *height,
		int *x_advance, int *y_advance)
{
	const char *font_encoding = NULL;
	unsigned int font, font1, u;
	unsigned short u1[2];
	struct rufl_character_set *charset;
	struct rufl_unicode_map_entry *umap_entry = NULL;
	font_f f;
	rufl_code code;
	font_scan_block block;
	font_string_flags flags;
	int xa, ya;

	/* Find font family containing glyph */
	code = rufl_find_font_family(font_family, font_style,
			&font, NULL, &charset);
	if (code != rufl_OK)
		return code;

	rufl_utf8_read(string, length, u);
	if (charset && rufl_character_set_test(charset, u))
		font1 = font;
	else if (u < 0x10000)
		font1 = rufl_substitution_table[u];
	else
		font1 = rufl_CACHE_CORPUS;

	/* Old font managers need the font encoding, too */
	if (rufl_old_font_manager && font1 != rufl_CACHE_CORPUS) {
		unsigned int i;
		unsigned short u16 = (unsigned short) u;

		for (i = 0; i < rufl_font_list[font1].num_umaps; i++) {
			struct rufl_unicode_map *map =
					rufl_font_list[font1].umap + i;

			umap_entry = bsearch(&u16, map->map, map->entries,
					sizeof map->map[0],
					rufl_unicode_map_search_cmp);
			if (umap_entry) {
				font_encoding = map->encoding;
				break;
			}
		}

		assert(umap_entry != NULL);
	}

	code = rufl_find_font(font1, font_size, font_encoding, &f);
	if (code != rufl_OK)
		return code;

	/*
	 * Glyph Metrics for horizontal text:
	 *
	 *  ^   x0    x1  ¦
	 *  |   ¦     ¦   ¦           Xbearing          : x0 - oX
	 *  |   +-----+---¦----- y1   Ybearing          : y1 - oY
	 *  |   |     |   ¦           Xadvance          : aX - oX
	 *  |   |     |   ¦           Yadvance          : 0
	 *  o---|-----|---a-->        Glyph width       : x1 - x0
	 *  |   |     |   ¦           Glyph height      : y1 - y0
	 *  |   +-----+---¦----- y0   Right side bearing: aX - x1
	 *  |             ¦
	 *
	 *  The rectangle (x0,y0),(x1,y1) is the glyph bounding box.
	 *
	 * Glyph Metrics for vertical text:
	 *
	 *  -------o--------->
	 *  y1--+--|--+               Xbearing          : x0 - oX
	 *      |  |  |               Ybearing          : oY - y1
	 *      |  |  |               Xadvance          : 0
	 *      |  |  |               Yadvance          : aY - oY
	 *      |  |  |               Glyph width       : x1 - x0
	 *  y0--+-----+               Glyph height      : y1 - y0
	 *  ----¦--a--¦---------      Right side bearing: N/A
	 *     x0  v  x1
	 *
	 *  The rectangle (x0,y0),(x1,y1) is the glyph bounding box.
	 *
	 *
	 * In order to extract the information we want from the
	 * Font Manager, a little bit of hackery is required.
	 *
	 * Firstly, we can take the origin as being (0,0). This is an
	 * arbitrary choice but makes the maths simpler.
	 *
	 * Secondly, the bounding box returned by Font_CharBBox /
	 * Font_ScanString / Font_StringBBox represents the ink area of
	 * the glyph (i.e. the smallest box needed to contain all the
	 * glyph path segments). This means that, for glyphs with no
	 * displayed content (such as a space), the bounding box will be 0.
	 * These SWIs therefore allow us to retrieve the (x0,y0),(x1,y1)
	 * coordinates marked in the diagrams above.
	 *
	 * Finally, we need to retrieve the glyph advance distance. This is
	 * returned in R3/R4 on exit from Font_ScanString (providing bit 17
	 * of the flags word on entry is clear). It is important to note,
	 * however, that the height will be returned as 0 for fonts with no
	 * Yadvance values in the font data file. Therefore, in order to
	 * achieve vertical layout of text, further work will be needed
	 * (We're also ignoring the fact that the X coordinates of all
	 * values will be in the wrong place and the Y coordinates will have
	 * the wrong sign due to the differing definitions of the Y axis for
	 * horizontal and vertical text.)
	 *
	 * Note that all values (that we're interested in, at least)
	 * returned by the SWIs mentioned above are in _millipoints_.
	 */

	block.space.x = block.space.y = 0;
	block.letter.x = block.letter.y = 0;
	block.split_char = -1;

	flags = font_GIVEN_BLOCK | font_GIVEN_LENGTH | font_GIVEN_FONT |
		font_RETURN_BBOX;

	u1[0] = (unsigned short)u;
	u1[1] = 0;

	if (font1 == rufl_CACHE_CORPUS) {
		/* Fallback Glyph */
		/** \todo implement this properly */
		xa = 1000 * font_size;
		ya = 0;
		block.bbox.x0 = block.bbox.y0 = 0;
		block.bbox.x1 = block.bbox.y1 = xa;
	} else if (rufl_old_font_manager) {
		/* Old Font Manager */
		char s[2];

		/* We found the correct umap entry when 
		 * looking for the font encoding */
		s[0] = umap_entry->c;
		s[1] = 0;

		rufl_fm_error = xfont_scan_string(f, s, flags,
				0x7fffffff, 0x7fffffff, &block, 0, 1,
				0, &xa, &ya, 0);
		if (rufl_fm_error) {
			LOG("xfont_scan_string: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
	} else {
		/* UCS Font Manager */
		rufl_fm_error = xfont_scan_string(f, (const char *)u1,
				flags | font_GIVEN16_BIT,
				0x7fffffff, 0x7fffffff, &block, 0, 2,
				0, &xa, &ya, 0);
		if (rufl_fm_error) {
			LOG("xfont_scan_string: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
	}

	/** \todo handle vertical text */
	if (x_bearing)
		(*x_bearing) = block.bbox.x0;

	if (y_bearing)
		(*y_bearing) = block.bbox.y1;

	if (width)
		(*width) = block.bbox.x1 - block.bbox.x0;

	if (height)
		(*height) = block.bbox.y1 - block.bbox.y0;

	if (x_advance)
		(*x_advance) = xa;

	if (y_advance)
		(*y_advance) = ya;

	return rufl_OK;
}


int rufl_unicode_map_search_cmp(const void *keyval, const void *datum)
{
	const unsigned short *key = keyval;
	const struct rufl_unicode_map_entry *entry = datum;
	if (*key < entry->u)
		return -1;
	else if (entry->u < *key)
		return 1;
	return 0;
}
