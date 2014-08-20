/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 * Copyright 2005 John-Mark Bell <jmb202@ecs.soton.ac.uk>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "rufl_internal.h"

static int rufl_family_list_cmp(const void *keyval, const void *datum);
static rufl_code rufl_place_in_cache(unsigned int font, unsigned int font_size,
		const char *encoding, font_f f);

/**
 * Find a font family.
 */
rufl_code rufl_find_font_family(const char *font_family,
		rufl_style font_style, unsigned int *font,
		unsigned int *slanted, struct rufl_character_set **charset)
{
	const char **family;
	unsigned int f;
	unsigned int weight, slant, used_weight;
	unsigned int search_direction;

	family = bsearch(font_family, rufl_family_list,
			rufl_family_list_entries,
			sizeof rufl_family_list[0], rufl_family_list_cmp);
	if (!family)
		return rufl_FONT_NOT_FOUND;

	weight = (font_style & 0xf) - 1;
	assert(weight <= 8);
	slant = font_style & rufl_SLANTED ? 1 : 0;

	struct rufl_family_map_entry *e =
			&rufl_family_map[family - rufl_family_list];
	used_weight = weight;
	if (weight <= 2)
		search_direction = -1;
	else
		search_direction = +1;
	while (1) {
		if (e->font[used_weight][slant] != NO_FONT) {
			/* the weight and slant is available */
			f = e->font[used_weight][slant];
			break;
		}
		if (e->font[used_weight][1 - slant] != NO_FONT) {
			/* slanted, and non-slanted weight exists, or vv. */
			f = e->font[used_weight][1 - slant];
			break;
		}
		if (used_weight == 0) {
			/* searched down without finding a weight: search up */
			used_weight = weight + 1;
			search_direction = +1;
		} else if (used_weight == 8) {
			/* searched up without finding a weight: search down */
			used_weight = weight - 1;
			search_direction = -1;
		} else {
			/* try the next weight in the current direction */
			used_weight += search_direction;
		}
	}

	if (font)
		(*font) = f;

	if (slanted)
		(*slanted) = slant;

	if (charset)
		(*charset) = rufl_font_list[f].charset;

	return rufl_OK;
}


/**
 * Find a sized font, placing in the cache if necessary.
 */
rufl_code rufl_find_font(unsigned int font, unsigned int font_size,
		const char *encoding, font_f *fhandle)
{
	font_f f;
	char font_name[80];
	unsigned int i;
	rufl_code code;

	assert(fhandle != NULL);

	for (i = 0; i != rufl_CACHE_SIZE; i++) {
		/* Comparing pointers for the encoding is fine, as the 
		 * encoding string passed to us is either:
		 *
		 *    a) NULL
		 * or b) statically allocated
		 * or c) resides in the font's umap, which is constant
		 *       for the lifetime of the application.
		 */
		if (rufl_cache[i].font == font &&
				rufl_cache[i].size == font_size &&
				rufl_cache[i].encoding == encoding)
			break;
	}
	if (i != rufl_CACHE_SIZE) {
		/* found in cache */
		f = rufl_cache[i].f;
		rufl_cache[i].last_used = rufl_cache_time++;
	} else {
		/* not found */
		if (font == rufl_CACHE_CORPUS) {
			if (encoding)
				snprintf(font_name, sizeof font_name,
					"Corpus.Medium\\E%s", encoding);
			else
				snprintf(font_name, sizeof font_name,
					"Corpus.Medium");
		} else {
			if (encoding)
				snprintf(font_name, sizeof font_name,
					"%s\\E%s",
					rufl_font_list[font].identifier,
					encoding);
			else
				snprintf(font_name, sizeof font_name, "%s",
					rufl_font_list[font].identifier);
		}

		rufl_fm_error = xfont_find_font(font_name,
				font_size, font_size, 0, 0, &f, 0, 0);
		if (rufl_fm_error) {
			LOG("xfont_find_font: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
		/* place in cache */
		code = rufl_place_in_cache(font, font_size, encoding, f);
		if (code != rufl_OK)
			return code;
	}

	(*fhandle) = f;

	return rufl_OK;
}


int rufl_family_list_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const char * const *entry = datum;
	return strcasecmp(key, *entry);
}


/**
 * Place a font into the recent-use cache, making space if necessary.
 */

rufl_code rufl_place_in_cache(unsigned int font, unsigned int font_size,
		const char *encoding, font_f f)
{
	unsigned int i;
	unsigned int max_age = 0;
	unsigned int evict = 0;

	for (i = 0; i != rufl_CACHE_SIZE; i++) {
		if (rufl_cache[i].font == rufl_CACHE_NONE) {
			evict = i;
			break;
		} else if (max_age < rufl_cache_time -
				rufl_cache[i].last_used) {
			max_age = rufl_cache_time -
					rufl_cache[i].last_used;
			evict = i;
		}
	}
	if (rufl_cache[evict].font != rufl_CACHE_NONE) {
		rufl_fm_error = xfont_lose_font(rufl_cache[evict].f);
		if (rufl_fm_error)
			return rufl_FONT_MANAGER_ERROR;
	}
	rufl_cache[evict].font = font;
	rufl_cache[evict].size = font_size;
	rufl_cache[evict].encoding = encoding;
	rufl_cache[evict].f = f;
	rufl_cache[evict].last_used = rufl_cache_time++;

	return rufl_OK;
}
