/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "oslib/font.h"
#include "rufl_internal.h"


typedef enum { rufl_PAINT, rufl_WIDTH, rufl_X_TO_OFFSET,
		rufl_SPLIT, rufl_PAINT_CALLBACK, rufl_FONT_BBOX } rufl_action;
#define rufl_PROCESS_CHUNK 200

bool rufl_can_background_blend = false;

static const os_trfm trfm_oblique =
		{ { { 65536, 0 }, { 13930, 65536 }, { 0, 0 } } };


static rufl_code rufl_process(rufl_action action,
		const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string0, size_t length,
		int x, int y, unsigned int flags,
		int *width, int click_x, size_t *char_offset, int *actual_x,
		rufl_callback_t callback, void *context);
static rufl_code rufl_process_span(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font, unsigned int font_size, unsigned int slant,
		int *x, int y, unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context);
static rufl_code rufl_process_span_old(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font, unsigned int font_size, unsigned int slant,
		int *x, int y, unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context);
static int rufl_unicode_map_search_cmp(const void *keyval, const void *datum);
static rufl_code rufl_process_not_available(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font_size, int *x, int y,
		unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context);


/**
 * Render Unicode text.
 */

rufl_code rufl_paint(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int x, int y, unsigned int flags)
{
	return rufl_process(rufl_PAINT,
			font_family, font_style, font_size, string,
			length, x, y, flags, 0, 0, 0, 0, 0, 0);
}


/**
 * Measure the width of Unicode text.
 */

rufl_code rufl_width(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int *width)
{
	return rufl_process(rufl_WIDTH,
			font_family, font_style, font_size, string,
			length, 0, 0, 0, width, 0, 0, 0, 0, 0);
}


/**
 * Find the nearest character boundary in a string to where an x coordinate
 * falls.
 */

rufl_code rufl_x_to_offset(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int click_x,
		size_t *char_offset, int *actual_x)
{
	return rufl_process(rufl_X_TO_OFFSET,
			font_family, font_style, font_size, string,
			length, 0, 0, 0, 0,
			click_x, char_offset, actual_x, 0, 0);
}


/**
 * Find the prefix of a string that will fit in a specified width.
 */

rufl_code rufl_split(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int width,
		size_t *char_offset, int *actual_x)
{
	return rufl_process(rufl_SPLIT,
			font_family, font_style, font_size, string,
			length, 0, 0, 0, 0,
			width, char_offset, actual_x, 0, 0);
}


/**
 * Render text, but call a callback instead of each call to Font_Paint.
 */

rufl_code rufl_paint_callback(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string, size_t length,
		int x, int y,
		rufl_callback_t callback, void *context)
{
	return rufl_process(rufl_PAINT_CALLBACK,
			font_family, font_style, font_size, string,
			length, x, y, 0, 0, 0, 0, 0, callback, context);
}


/**
 * Determine the maximum bounding box of a font.
 */

rufl_code rufl_font_bbox(const char *font_family, rufl_style font_style,
		unsigned int font_size,
		int *bbox)
{
	return rufl_process(rufl_FONT_BBOX,
			font_family, font_style, font_size, 0,
			0, 0, 0, 0, bbox, 0, 0, 0, 0, 0);
}


/**
 * Render, measure, or split Unicode text.
 */

rufl_code rufl_process(rufl_action action,
		const char *font_family, rufl_style font_style,
		unsigned int font_size,
		const char *string0, size_t length,
		int x, int y, unsigned int flags,
		int *width, int click_x, size_t *char_offset, int *actual_x,
		rufl_callback_t callback, void *context)
{
	unsigned short s[rufl_PROCESS_CHUNK];
	unsigned int font;
	unsigned int font0, font1;
	unsigned int n;
	unsigned int u;
	size_t offset;
	size_t offset_u;
	size_t offset_map[rufl_PROCESS_CHUNK];
	unsigned int slant;
	const char *string = string0;
	struct rufl_character_set *charset;
	rufl_code code;

	assert(action == rufl_PAINT ||
			(action == rufl_WIDTH && width) ||
			(action == rufl_X_TO_OFFSET && char_offset &&
					actual_x) ||
			(action == rufl_SPLIT && char_offset &&
					actual_x) ||
			(action == rufl_PAINT_CALLBACK && callback) ||
			(action == rufl_FONT_BBOX && width));

	if ((flags & rufl_BLEND_FONT) && !rufl_can_background_blend) {
		/* unsuitable FM => clear blending bit */
		flags &= ~rufl_BLEND_FONT;
	}

	if (length == 0 && action != rufl_FONT_BBOX) {
		if (action == rufl_WIDTH)
			*width = 0;
		else if (action == rufl_X_TO_OFFSET || action == rufl_SPLIT) {
			*char_offset = 0;
			*actual_x = 0;
		}
		return rufl_OK;
	}
	if ((action == rufl_X_TO_OFFSET || action == rufl_SPLIT) &&
			click_x <= 0) {
		*char_offset = 0;
		*actual_x = 0;
		return rufl_OK;
	}

	code = rufl_find_font_family(font_family, font_style,
			&font, &slant, &charset);
	if (code != rufl_OK)
		return code;

	if (action == rufl_FONT_BBOX) {
		if (rufl_old_font_manager)
			code = rufl_process_span_old(action, 0, 0, font,
					font_size, slant, width, 0, 0,
					0, 0, 0, 0);
		else
			code = rufl_process_span(action, 0, 0, font,
					font_size, slant, width, 0, 0,
					0, 0, 0, 0);
		return code;
	}

	offset_u = 0;
	rufl_utf8_read(string, length, u);
	if (u <= 0x001f || (0x007f <= u && u <= 0x009f))
		font1 = NOT_AVAILABLE;
	else if (charset && rufl_character_set_test(charset, u))
		font1 = font;
	else if (u < 0x10000)
		font1 = rufl_substitution_table[u];
	else
		font1 = NOT_AVAILABLE;
	do {
		s[0] = u;
		offset_map[0] = offset_u;
		n = 1;
		font0 = font1;
		/* invariant: s[0..n) is in font font0 */
		while (0 < length && n < rufl_PROCESS_CHUNK && font1 == font0) {
			offset_u = string - string0;
			rufl_utf8_read(string, length, u);
			s[n] = u;
			offset_map[n] = offset_u;
			if (u <= 0x001f || (0x007f <= u && u <= 0x009f))
				font1 = NOT_AVAILABLE;
			else if (charset && rufl_character_set_test(charset, u))
				font1 = font;
			else if (u < 0x10000)
				font1 = rufl_substitution_table[u];
			else
				font1 = NOT_AVAILABLE;
			if (font1 == font0)
				n++;
		}
		if (n == rufl_PROCESS_CHUNK)
			n--;
		s[n] = 0;
		offset_map[n] = offset_u;
		if (length == 0 && font1 == font0)
			offset_map[n] = string - string0;

		if (font0 == NOT_AVAILABLE)
			code = rufl_process_not_available(action, s, n,
					font_size, &x, y, flags,
					click_x, &offset, callback, context);
		else if (rufl_old_font_manager)
			code = rufl_process_span_old(action, s, n, font0,
					font_size, slant, &x, y, flags,
					click_x, &offset, callback, context);
		else
			code = rufl_process_span(action, s, n, font0,
					font_size, slant, &x, y, flags,
					click_x, &offset, callback, context);

		if ((action == rufl_X_TO_OFFSET || action == rufl_SPLIT) &&
				(offset < n || click_x < x))
			break;
		if (code != rufl_OK)
			return code;

	} while (!(length == 0 && font1 == font0));

	if (action == rufl_WIDTH)
		*width = x;
	else if (action == rufl_X_TO_OFFSET || action == rufl_SPLIT) {
		*char_offset = offset_map[offset];
		*actual_x = x;
	}

	return rufl_OK;
}


/**
 * Render a string of characters from a single RISC OS font.
 */

rufl_code rufl_process_span(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font, unsigned int font_size, unsigned int slant,
		int *x, int y, unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context)
{
	unsigned short *split_point;
	int x_out, y_out;
	unsigned int i;
	char font_name[80];
	bool oblique = slant && !rufl_font_list[font].slant;
	font_f f;
	rufl_code code;

	code = rufl_find_font(font, font_size, "UTF8", &f);
	if (code != rufl_OK)
		return code;

	if (action == rufl_FONT_BBOX) {
		rufl_fm_error = xfont_read_info(f, &x[0], &x[1], &x[2], &x[3]);
		if (rufl_fm_error)
			return rufl_FONT_MANAGER_ERROR;
		return rufl_OK;
	}

	if (action == rufl_PAINT) {
		/* paint span */
		rufl_fm_error = xfont_paint(f, (const char *) s,
				font_OS_UNITS |
				(oblique ? font_GIVEN_TRFM : 0) |
				font_GIVEN_LENGTH |
				font_GIVEN_FONT | font_KERN |
				font_GIVEN16_BIT |
				((flags & rufl_BLEND_FONT) ?
						font_BLEND_FONT : 0),
				*x, y, 0, &trfm_oblique, n * 2);
		if (rufl_fm_error) {
			LOG("xfont_paint: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			for (i = 0; i != n; i++)
				fprintf(stderr, "0x%x ", s[i]);
			fprintf(stderr, " (%u)\n", n);
			return rufl_FONT_MANAGER_ERROR;
		}
	} else if (action == rufl_PAINT_CALLBACK) {
		snprintf(font_name, sizeof font_name, "%s\\EUTF8",
				rufl_font_list[font].identifier);
		callback(context, font_name, font_size, 0, s, n, *x, y);
	}

	/* increment x by width of span */
	if (action == rufl_X_TO_OFFSET || action == rufl_SPLIT) {
		rufl_fm_error = xfont_scan_string(f, (const char *) s,
				font_GIVEN_LENGTH | font_GIVEN_FONT |
				font_KERN | font_GIVEN16_BIT |
				((action == rufl_X_TO_OFFSET) ?
						font_RETURN_CARET_POS : 0),
				(click_x - *x) * 400, 0x7fffffff, 0, 0,
				n * 2,
				(char **)(void *)&split_point, 
				&x_out, &y_out, 0);
		*offset = split_point - s;
	} else {
		rufl_fm_error = xfont_scan_string(f, (const char *) s,
				font_GIVEN_LENGTH | font_GIVEN_FONT |
				font_KERN | font_GIVEN16_BIT,
				0x7fffffff, 0x7fffffff, 0, 0, n * 2,
				0, &x_out, &y_out, 0);
	}
	if (rufl_fm_error) {
		LOG("xfont_scan_string: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}
	*x += x_out / 400;

	return rufl_OK;
}


/**
 * Render a string of characters from a single RISC OS font  (old font manager
 * version).
 */

rufl_code rufl_process_span_old(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font, unsigned int font_size, unsigned int slant,
		int *x, int y, unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context)
{
	char s2[rufl_PROCESS_CHUNK];
	char *split_point;
	int x_out, y_out;
	unsigned int i;
	bool oblique = slant && !rufl_font_list[font].slant;
	font_f f;
	rufl_code code;

	if (action == rufl_FONT_BBOX) {
		/* Don't need encoding for bounding box */
		code = rufl_find_font(font, font_size, NULL, &f);
		if (code != rufl_OK)
			return code;

		rufl_fm_error = xfont_read_info(f, &x[0], &x[1], &x[2], &x[3]);
		if (rufl_fm_error) {
			LOG("xfont_read_info: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
		return rufl_OK;
	}

	if (offset)
		*offset = 0;

	/* Process the span in map-coherent chunks */
	do {
		struct rufl_unicode_map *map = NULL;
		struct rufl_unicode_map_entry *entry = NULL;
		unsigned int j;

		i = 0;

		/* Find map for first character */
		for (j = 0; j < rufl_font_list[font].num_umaps; j++) {
			map = rufl_font_list[font].umap + j;

			entry = bsearch(&s[i], map->map, map->entries,
				sizeof map->map[0],
				rufl_unicode_map_search_cmp);
			if (entry)
				break;
		}
		assert(map != NULL);
		assert(entry != NULL);

		/* Collect characters: s[0..i) use map */
		do {
			entry = bsearch(&s[i], map->map, map->entries,
				sizeof map->map[0],
				rufl_unicode_map_search_cmp);

			if (entry)
				s2[i++] = entry->c;
		} while (i != n && entry != NULL);

		s2[i] = 0;

		code = rufl_find_font(font, font_size, map->encoding, &f);
		if (code != rufl_OK)
			return code;

		if (action == rufl_PAINT) {
			/* paint span */
			/* call Font_SetFont to work around broken PS printer 
			 * driver, which doesn't use the font handle from 
			 * Font_Paint */
			rufl_fm_error = xfont_set_font(f);
			if (rufl_fm_error) {
				LOG("xfont_set_font: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess);
				return rufl_FONT_MANAGER_ERROR;
			}

			rufl_fm_error = xfont_paint(f, s2, font_OS_UNITS |
					(oblique ? font_GIVEN_TRFM : 0) |
					font_GIVEN_LENGTH | font_GIVEN_FONT |
					font_KERN |
					((flags & rufl_BLEND_FONT) ?
							font_BLEND_FONT : 0),
					*x, y, 0, &trfm_oblique, i);
			if (rufl_fm_error) {
				LOG("xfont_paint: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess);
				return rufl_FONT_MANAGER_ERROR;
			}
		} else if (action == rufl_PAINT_CALLBACK) {
			char font_name[80];

			if (map->encoding)
				snprintf(font_name, sizeof font_name, "%s\\E%s",
					rufl_font_list[font].identifier,
					map->encoding);
			else
				snprintf(font_name, sizeof font_name, "%s",
					rufl_font_list[font].identifier);

			callback(context, font_name, font_size, 
					s2, 0, i, *x, y);
		}

		/* increment x by width of span */
		if (action == rufl_X_TO_OFFSET || action == rufl_SPLIT) {
			rufl_fm_error = xfont_scan_string(f, s2,
					font_GIVEN_LENGTH | font_GIVEN_FONT |
					font_KERN |
					((action == rufl_X_TO_OFFSET) ?
						font_RETURN_CARET_POS : 0),
					(click_x - *x) * 400, 0x7fffffff, 
					0, 0, i,
					&split_point, &x_out, &y_out, 0);
			*offset += split_point - s2;
		} else {
			rufl_fm_error = xfont_scan_string(f, s2,
					font_GIVEN_LENGTH | font_GIVEN_FONT | 
					font_KERN,
					0x7fffffff, 0x7fffffff, 0, 0, i,
					0, &x_out, &y_out, 0);
		}
		if (rufl_fm_error) {
			LOG("xfont_scan_string: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
		*x += x_out / 400;

		/* Now update s and n for the next chunk */
		s += i;
		n -= i;
	} while (n != 0);

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


/**
 * Render a string of characters not available in any font as their hex code.
 */

rufl_code rufl_process_not_available(rufl_action action,
		unsigned short *s, unsigned int n,
		unsigned int font_size, int *x, int y,
		unsigned int flags,
		int click_x, size_t *offset,
		rufl_callback_t callback, void *context)
{
	char missing[] = "0000";
	int dx = 7 * font_size / 64;
	int top_y = y + 5 * font_size / 64;
	unsigned int i;
	font_f f;
	rufl_code code;

	if (action == rufl_WIDTH) {
		*x += n * dx;
		return rufl_OK;
	} else if (action == rufl_X_TO_OFFSET || action == rufl_SPLIT) {
		if (click_x - *x < (int) (n * dx))
			*offset = (click_x - *x) / dx;
		else
			*offset = n;
		*x += *offset * dx;
		return rufl_OK;
	}

	code = rufl_find_font(rufl_CACHE_CORPUS, font_size / 2, "Latin1", &f);
	if (code != rufl_OK)
		return code;

	for (i = 0; i != n; i++) {
		missing[0] = "0123456789abcdef"[(s[i] >> 12) & 0xf];
		missing[1] = "0123456789abcdef"[(s[i] >> 8) & 0xf];
		missing[2] = "0123456789abcdef"[(s[i] >> 4) & 0xf];
		missing[3] = "0123456789abcdef"[(s[i] >> 0) & 0xf];

		/* first two characters in top row */
		if (action == rufl_PAINT) {
			rufl_fm_error = xfont_paint(f, missing, font_OS_UNITS |
					font_GIVEN_LENGTH | font_GIVEN_FONT |
					font_KERN |
					((flags & rufl_BLEND_FONT) ?
							font_BLEND_FONT : 0),
					*x, top_y, 0, 0, 2);
			if (rufl_fm_error)
				return rufl_FONT_MANAGER_ERROR;
		} else {
			callback(context, "Corpus.Medium\\ELatin1",
					font_size / 2, missing, 0, 2,
					*x, top_y);
		}

		/* last two characters underneath */
		if (action == rufl_PAINT) {
			rufl_fm_error = xfont_paint(f, missing + 2,
					font_OS_UNITS |
					font_GIVEN_LENGTH | font_GIVEN_FONT |
					font_KERN |
					((flags & rufl_BLEND_FONT) ?
							font_BLEND_FONT : 0),
					*x, y, 0, 0, 2);
			if (rufl_fm_error)
				return rufl_FONT_MANAGER_ERROR;
		} else {
			callback(context, "Corpus.Medium\\ELatin1",
					font_size / 2, missing + 2, 0, 2,
					*x, y);
		}

		*x += dx;
	}

	return rufl_OK;
}
