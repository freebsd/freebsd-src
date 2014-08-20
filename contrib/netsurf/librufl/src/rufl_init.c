/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2006 James Bursa <james@semichrome.net>
 */

#define _GNU_SOURCE  /* for strndup */
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <search.h>
#include <oslib/font.h>
#include <oslib/hourglass.h>
#include <oslib/os.h>
#include <oslib/osfscontrol.h>
#include <oslib/taskwindow.h>
#include <oslib/wimp.h>
#include <oslib/wimpreadsysinfo.h>
#include "rufl_internal.h"


struct rufl_font_list_entry *rufl_font_list = 0;
size_t rufl_font_list_entries = 0;
const char **rufl_family_list = 0;
unsigned int rufl_family_list_entries = 0;
struct rufl_family_map_entry *rufl_family_map = 0;
os_error *rufl_fm_error = 0;
void *rufl_family_menu = 0;
unsigned short *rufl_substitution_table = 0;
struct rufl_cache_entry rufl_cache[rufl_CACHE_SIZE];
int rufl_cache_time = 0;
bool rufl_old_font_manager = false;
wimp_w rufl_status_w = 0;
char rufl_status_buffer[80];

/** An entry in rufl_weight_table. */
struct rufl_weight_table_entry {
	const char *name;
	unsigned int weight;
};

/** Map from font name part to font weight. Must be case-insensitive sorted by
 * name. */
const struct rufl_weight_table_entry rufl_weight_table[] = {
	{ "Black", 9 },
	{ "Bold", 7 },
	{ "Book", 3 },
	{ "Demi", 6 },
	{ "DemiBold", 6 },
	{ "Extra", 8 },
	{ "ExtraBlack", 9 },
	{ "ExtraBold", 8 },
	{ "ExtraLight", 1 },
	{ "Heavy", 8 },
	{ "Light", 2 },
	{ "Medium", 5 },
	{ "Regular", 4 },
	{ "Semi", 6 },
	{ "SemiBold", 6 },
	{ "SemiLight", 3 },
	{ "UltraBlack", 9 },
	{ "UltraBold", 9 },
};


static rufl_code rufl_init_font_list(void);
static rufl_code rufl_init_add_font(const char *identifier, 
		const char *local_name);
static int rufl_weight_table_cmp(const void *keyval, const void *datum);
static rufl_code rufl_init_scan_font(unsigned int font);
static rufl_code rufl_init_scan_font_no_enumerate(unsigned int font);
static bool rufl_is_space(unsigned int u);
static rufl_code rufl_init_scan_font_old(unsigned int font_index);
static rufl_code rufl_init_scan_font_in_encoding(const char *font_name, 
		const char *encoding, struct rufl_character_set *charset,
		struct rufl_unicode_map *umap, unsigned int *last);
static rufl_code rufl_init_read_encoding(font_f font,
		struct rufl_unicode_map *umap);
static int rufl_glyph_map_cmp(const void *keyval, const void *datum);
static int rufl_unicode_map_cmp(const void *z1, const void *z2);
static rufl_code rufl_init_substitution_table(void);
static rufl_code rufl_save_cache(void);
static rufl_code rufl_load_cache(void);
static int rufl_font_list_cmp(const void *keyval, const void *datum);
static rufl_code rufl_init_family_menu(void);
static void rufl_init_status_open(void);
static void rufl_init_status(const char *status, float progress);
static void rufl_init_status_close(void);


/**
 * Initialise RUfl.
 *
 * All available fonts are scanned. May take some time.
 */

rufl_code rufl_init(void)
{
	bool rufl_broken_font_enumerate_characters = false;
	unsigned int changes = 0;
	unsigned int i;
	int fm_version;
	rufl_code code;
	font_f font;
	os_colour old_sand, old_glass;

	if (rufl_font_list_entries)
		/* already initialized */
		return rufl_OK;

	xhourglass_on();

	rufl_init_status_open();

	/* determine if the font manager supports Unicode */
	rufl_fm_error = xfont_find_font("Homerton.Medium\\EUTF8", 160, 160,
			0, 0, &font, 0, 0);
	if (rufl_fm_error) {
		if (rufl_fm_error->errnum == error_FONT_ENCODING_NOT_FOUND) {
			rufl_old_font_manager = true;
		} else {
			LOG("xfont_find_font: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			rufl_quit();
			xhourglass_off();
			return rufl_FONT_MANAGER_ERROR;
		}
	} else {
		/* New font manager; see if character enumeration works */
		int next;

		rufl_fm_error = xfont_enumerate_characters(font, 0, 
				&next, NULL);
		/* Broken if SWI fails or it doesn't return 0x20 as the first
		 * character to process. Font Managers earlier than 3.64 have
		 * a bug that means they do not return the first available
		 * range of characters in a font. We detect this by asking
		 * for the first character in Homerton.Medium, which we know
		 * is 0x20 (i.e. space). If the value returned is not this,
		 * then we assume the font manager is broken and fall back to
		 * the old code which is significantly slower.
		 */
		if (rufl_fm_error || next != 0x20)
			rufl_broken_font_enumerate_characters = true;

		xfont_lose_font(font);
	}
	LOG("%s font manager", rufl_old_font_manager ? "old" : "new");

	/* test if the font manager supports background blending */
	rufl_fm_error = xfont_cache_addr(&fm_version, 0, 0);
	if (rufl_fm_error)
		return rufl_FONT_MANAGER_ERROR;
	if (fm_version >= 335)
		rufl_can_background_blend = true;

	code = rufl_init_font_list();
	if (code != rufl_OK) {
		rufl_quit();
		xhourglass_off();
		return code;
	}
	LOG("%zu faces, %u families", rufl_font_list_entries,
			rufl_family_list_entries);

	code = rufl_load_cache();
	if (code != rufl_OK) {
		LOG("rufl_load_cache: 0x%x", code);
		rufl_quit();
		xhourglass_off();
		return code;
	}

	xhourglass_leds(1, 0, 0);
	for (i = 0; i != rufl_font_list_entries; i++) {
		if (rufl_font_list[i].charset) {
			/* character set loaded from cache */
			continue;
		}
		LOG("scanning %u \"%s\"", i, rufl_font_list[i].identifier);
		xhourglass_percentage(100 * i / rufl_font_list_entries);
		rufl_init_status(rufl_font_list[i].identifier,
				(float) i / rufl_font_list_entries);
		if (rufl_old_font_manager)
			code = rufl_init_scan_font_old(i);
		else if (rufl_broken_font_enumerate_characters)
			code = rufl_init_scan_font_no_enumerate(i);
		else
			code = rufl_init_scan_font(i);
		if (code != rufl_OK) {
			LOG("rufl_init_scan_font: 0x%x", code);
			rufl_quit();
			xhourglass_off();
			return code;
		}
		changes++;
	}

	xhourglass_leds(2, 0, 0);
	xhourglass_colours(0x0000ff, 0x00ffff, &old_sand, &old_glass);
	code = rufl_init_substitution_table();
	if (code != rufl_OK) {
		LOG("rufl_init_substitution_table: 0x%x", code);
		rufl_quit();
		xhourglass_off();
		return code;
	}
	xhourglass_colours(old_sand, old_glass, 0, 0);

	if (changes) {
		LOG("%u new charsets", changes);
		xhourglass_leds(3, 0, 0);
		code = rufl_save_cache();
		if (code != rufl_OK) {
			LOG("rufl_save_cache: 0x%x", code);
			rufl_quit();
			xhourglass_off();
			return code;
		}
	}

	for (i = 0; i != rufl_CACHE_SIZE; i++)
		rufl_cache[i].font = rufl_CACHE_NONE;

	code = rufl_init_family_menu();
	if (code != rufl_OK) {
		LOG("rufl_init_substitution_table: 0x%x", code);
		rufl_quit();
		xhourglass_off();
		return code;
	}

	rufl_init_status_close();

	xhourglass_off();

	return rufl_OK;
}


/**
 * Build list of font in rufl_font_list and list of font families
 * in rufl_family_list.
 */

rufl_code rufl_init_font_list(void)
{
	rufl_code code;
	font_list_context context = 0;
	char identifier[80], local_name[80];

	while (context != -1) {
		/* read identifier */
		rufl_fm_error = xfont_list_fonts((byte *)identifier,
				font_RETURN_FONT_NAME |
				font_RETURN_LOCAL_FONT_NAME |
				context,
				sizeof identifier,
				(byte *)local_name, sizeof local_name, 0,
				&context, 0, 0);
		if (rufl_fm_error) {
			LOG("xfont_list_fonts: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			return rufl_FONT_MANAGER_ERROR;
		}
		if (context == -1)
			break;

		code = rufl_init_add_font(identifier, local_name);
		if (code != rufl_OK)
			return code;
	}

	return rufl_OK;
}


rufl_code rufl_init_add_font(const char *identifier, const char *local_name)
{
	int size;
	struct rufl_font_list_entry *font_list;
	char *dot;
	const char **family_list;
	const char *family, *part;
	unsigned int weight = 0;
	unsigned int slant = 0;
	bool special = false;
	struct rufl_family_map_entry *family_map;
	unsigned int i;
	struct rufl_weight_table_entry *entry;

	/* Check that:
	 * a) it's not a RiScript generated font
	 * b) it's not a TeX font	 */

	/* Read required buffer size */
	rufl_fm_error = xosfscontrol_canonicalise_path(identifier, 0,
			"Font$Path", 0, 0, &size);
	if (rufl_fm_error) {
		LOG("xosfscontrol_canonicalise_path(\"%s\", ...): 0x%x: %s",
				identifier,
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_OK;
	}
	/* size is -(space required - 1) so negate and add 1 */
	size = -size + 1;

	/* Create buffer and canonicalise path */
	char fullpath[size];
	rufl_fm_error = xosfscontrol_canonicalise_path(identifier,
			fullpath, "Font$Path", 0, size, 0);
	if (rufl_fm_error) {
		LOG("xosfscontrol_canonicalise_path(\"%s\", ...): 0x%x: %s",
				identifier,
				rufl_fm_error->errnum,
				rufl_fm_error->errmess);
		return rufl_OK;
	}

	/* LOG("%s", fullpath); */

	if (strstr(fullpath, "RiScript") || strstr(fullpath, "!TeXFonts"))
		/* Ignore this font */
		return rufl_OK;

	/* add identifier to rufl_font_list */
	font_list = realloc(rufl_font_list, sizeof rufl_font_list[0] *
			(rufl_font_list_entries + 1));
	if (!font_list)
		return rufl_OUT_OF_MEMORY;
	rufl_font_list = font_list;
	rufl_font_list[rufl_font_list_entries].identifier = strdup(identifier);
	if (!rufl_font_list[rufl_font_list_entries].identifier)
		return rufl_OUT_OF_MEMORY;
	rufl_font_list[rufl_font_list_entries].charset = 0;
	rufl_font_list[rufl_font_list_entries].umap = 0;
	rufl_font_list_entries++;

	/* determine family, weight, and slant */
	dot = strchr(local_name, '.');
	family = local_name;
	if (dot)
		*dot = 0;
	while (dot) {
		part = dot + 1;
		dot = strchr(part, '.');
		if (dot)
			*dot = 0;
		if (strcasecmp(part, "Italic") == 0 ||
				strcasecmp(part, "Oblique") == 0) {
			slant = 1;
			continue;
		}
		entry = bsearch(part, rufl_weight_table,
				sizeof rufl_weight_table /
				sizeof rufl_weight_table[0],
				sizeof rufl_weight_table[0],
				rufl_weight_table_cmp);
		if (entry)
			weight = entry->weight;
		else
			special = true;  /* unknown weight or style */
	}
	if (!weight)
		weight = 4;
	weight--;

	if (rufl_family_list_entries == 0 || strcasecmp(family,
			rufl_family_list[rufl_family_list_entries - 1]) != 0) {
		/* new family */
		family_list = realloc(rufl_family_list,
				sizeof rufl_family_list[0] *
				(rufl_family_list_entries + 1));
		if (!family_list)
			return rufl_OUT_OF_MEMORY;
		rufl_family_list = family_list;

		family_map = realloc(rufl_family_map,
				sizeof rufl_family_map[0] *
				(rufl_family_list_entries + 1));
		if (!family_map)
			return rufl_OUT_OF_MEMORY;
		rufl_family_map = family_map;

		family = strdup(family);
		if (!family)
			return rufl_OUT_OF_MEMORY;

		rufl_family_list[rufl_family_list_entries] = family;
		for (i = 0; i != 9; i++)
			rufl_family_map[rufl_family_list_entries].font[i][0] =
			rufl_family_map[rufl_family_list_entries].font[i][1] =
					NO_FONT;
		rufl_family_list_entries++;
	}

	struct rufl_family_map_entry *e =
			&rufl_family_map[rufl_family_list_entries - 1];
	/* prefer fonts with no unknown weight or style in their name, so that,
	 * for example, Alps.Light takes priority over Alps.Cond.Light */
	if (e->font[weight][slant] == NO_FONT || !special)
		e->font[weight][slant] = rufl_font_list_entries - 1;

	rufl_font_list[rufl_font_list_entries - 1].family =
			rufl_family_list_entries - 1;
	rufl_font_list[rufl_font_list_entries - 1].weight = weight;
	rufl_font_list[rufl_font_list_entries - 1].slant = slant;

	return rufl_OK;
}


int rufl_weight_table_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const struct rufl_weight_table_entry *entry = datum;
	return strcasecmp(key, entry->name);
}

/**
 * Scan a font for available characters.
 */

rufl_code rufl_init_scan_font(unsigned int font_index)
{
	char font_name[80];
	int x_out, y_out;
	unsigned int byte, bit;
	unsigned int last_used = 0;
	unsigned int string[2] = { 0, 0 };
	unsigned int u, next;
	struct rufl_character_set *charset;
	struct rufl_character_set *charset2;
	font_f font;
	font_scan_block block = { { 0, 0 }, { 0, 0 }, -1, { 0, 0, 0, 0 } };

	/*LOG("font %u \"%s\"", font_index,
			rufl_font_list[font_index].identifier);*/

	charset = calloc(1, sizeof *charset);
	if (!charset)
		return rufl_OUT_OF_MEMORY;
	for (u = 0; u != 256; u++)
		charset->index[u] = BLOCK_EMPTY;

	snprintf(font_name, sizeof font_name, "%s\\EUTF8",
			rufl_font_list[font_index].identifier);

	rufl_fm_error = xfont_find_font(font_name, 160, 160, 0, 0, &font, 0, 0);
	if (rufl_fm_error) {
		LOG("xfont_find_font(\"%s\"): 0x%x: %s", font_name,
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		free(charset);
		return rufl_OK;
	}

	/* Scan through mapped characters */
	for (u = 0; u != (unsigned int) -1; u = next) {
		unsigned int internal;

		rufl_fm_error = xfont_enumerate_characters(font, u, 
				(int *) &next, (int *) &internal);
		if (rufl_fm_error) {
			LOG("xfont_enumerate_characters: 0x%x: %s",
					rufl_fm_error->errnum, 
					rufl_fm_error->errmess);
			xfont_lose_font(font);
			free(charset);
			return rufl_OK;
		}

		/* Skip DELETE and C0/C1 controls */
		if (u < 0x0020 || (0x007f <= u && u <= 0x009f))
			continue;

		/* Skip astral characters */
		if (u > 0xffff)
			continue;

		/* Skip unmapped characters */
		if (internal == (unsigned int) -1)
			continue;

		if (u % 0x200 == 0)
			rufl_init_status(0, 0);

		/* Character is mapped, let's see if it's really there */
		string[0] = u;
		rufl_fm_error = xfont_scan_string(font, (char *) string,
				font_RETURN_BBOX | font_GIVEN32_BIT |
				font_GIVEN_FONT | font_GIVEN_LENGTH |
				font_GIVEN_BLOCK,
				0x7fffffff, 0x7fffffff,
				&block, 0, 4,
				0, &x_out, &y_out, 0);
		if (rufl_fm_error)
			break;

		if (block.bbox.x0 == 0x20000000) {
			/* absent (no definition) */
		} else if (x_out == 0 && y_out == 0 &&
				block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0) {
			/* absent (empty) */
                } else if (block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0 &&
				!rufl_is_space(u)) {
			/* absent (space but not a space character - some 
			 * fonts do this) */
		} else {
			/* present */
			if (charset->index[u >> 8] == BLOCK_EMPTY) {
				charset->index[u >> 8] = last_used;
				last_used++;
				if (last_used == 254)
					/* too many characters */
					break;
			}

			byte = (u >> 3) & 31;
			bit = u & 7;
			charset->block[charset->index[u >> 8]][byte] |= 
					1 << bit;
		}
	}

	xfont_lose_font(font);

	if (rufl_fm_error) {
		free(charset);
		LOG("xfont_scan_string: 0x%x: %s",
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	/* Determine which blocks are full, and mark them as such */
	for (u = 0; u != 256; u++) {
		if (charset->index[u] == BLOCK_EMPTY)
			continue;

		bit = 0xff;

		for (byte = 0; byte != 32; byte++)
			bit &= charset->block[u][byte];

		if (bit == 0xff) {
			/* Block is full */
			charset->index[u] = BLOCK_FULL;

			for (byte = 0; byte != 32; byte++)
				charset->block[u][byte] = 0;
		}
	}

	/* shrink-wrap */
	charset->size = offsetof(struct rufl_character_set, block) +
			32 * last_used;
	charset2 = realloc(charset, charset->size);
	if (!charset2) {
		free(charset);
		return rufl_OUT_OF_MEMORY;
	}

	rufl_font_list[font_index].charset = charset;

	return rufl_OK;
}

/**
 * Scan a font for available characters (version without character enumeration)
 */

rufl_code rufl_init_scan_font_no_enumerate(unsigned int font_index)
{
	char font_name[80];
	int x_out, y_out;
	unsigned int byte, bit;
	unsigned int block_count = 0;
	unsigned int last_used = 0;
	unsigned int string[2] = { 0, 0 };
	unsigned int u;
	struct rufl_character_set *charset;
	struct rufl_character_set *charset2;
	font_f font;
	font_scan_block block = { { 0, 0 }, { 0, 0 }, -1, { 0, 0, 0, 0 } };

	/*LOG("font %u \"%s\"", font_index,
 			rufl_font_list[font_index].identifier);*/

	charset = calloc(1, sizeof *charset);
	if (!charset)
		return rufl_OUT_OF_MEMORY;

	snprintf(font_name, sizeof font_name, "%s\\EUTF8",
			rufl_font_list[font_index].identifier);

	rufl_fm_error = xfont_find_font(font_name, 160, 160, 0, 0, &font, 0, 0);
	if (rufl_fm_error) {
		LOG("xfont_find_font(\"%s\"): 0x%x: %s", font_name,
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		free(charset);
		return rufl_OK;
	}

	/* scan through all characters */
	for (u = 0x0020; u != 0x10000; u++) {
		if (u == 0x007f) {
			/* skip DELETE and C1 controls */
			u = 0x009f;
			continue;
		}

		if (u % 0x200 == 0)
			rufl_init_status(0, 0);

		string[0] = u;
		rufl_fm_error = xfont_scan_string(font, (char *) string,
				font_RETURN_BBOX | font_GIVEN32_BIT |
				font_GIVEN_FONT | font_GIVEN_LENGTH |
				font_GIVEN_BLOCK,
				0x7fffffff, 0x7fffffff,
				&block, 0, 4,
				0, &x_out, &y_out, 0);
		if (rufl_fm_error)
			break;

		if (block.bbox.x0 == 0x20000000) {
			/* absent (no definition) */
		} else if (x_out == 0 && y_out == 0 &&
				block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0) {
			/* absent (empty) */
                } else if (block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0 &&
				!rufl_is_space(u)) {
			/* absent (space but not a space character - some
			 * fonts do this) */
		} else {
			/* present */
			byte = (u >> 3) & 31;
			bit = u & 7;
			charset->block[last_used][byte] |= 1 << bit;

			block_count++;
		}

		if ((u + 1) % 256 == 0) {
			/* end of block */
			if (block_count == 0)
				charset->index[u >> 8] = BLOCK_EMPTY;
			else if (block_count == 256) {
				charset->index[u >> 8] = BLOCK_FULL;
				for (byte = 0; byte != 32; byte++)
					charset->block[last_used][byte] = 0;
			} else {
				charset->index[u >> 8] = last_used;
				last_used++;
				if (last_used == 254)
					/* too many characters */
					break;
			}
			block_count = 0;
		}
	}

	xfont_lose_font(font);

	if (rufl_fm_error) {
		free(charset);
		LOG("xfont_scan_string: 0x%x: %s",
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	/* shrink-wrap */
	charset->size = offsetof(struct rufl_character_set, block) +
			32 * last_used;
	charset2 = realloc(charset, charset->size);
	if (!charset2) {
		free(charset);
		return rufl_OUT_OF_MEMORY;
	}

	rufl_font_list[font_index].charset = charset;

	return rufl_OK;
}

/**
 * A character is one of the Unicode space characters.
 */

bool rufl_is_space(unsigned int u)
{
	return u == 0x0020 || u == 0x00a0 ||
			(0x2000 <= u && u <= 0x200b) ||
			u == 0x202f || u == 0x3000;
}


/**
 * Scan a font for available characters (old font manager version).
 */

rufl_code rufl_init_scan_font_old(unsigned int font_index)
{
	const char *font_name = rufl_font_list[font_index].identifier;
	struct rufl_character_set *charset;
	struct rufl_character_set *charset2;
	struct rufl_unicode_map *umap = NULL;
	unsigned int num_umaps = 0;
	unsigned int i;
	unsigned int last_used = 0;
	rufl_code code;
	font_list_context context = 0;
	char encoding[80];

	/*LOG("font %u \"%s\"", font_index, font_name);*/

	charset = calloc(1, sizeof *charset);
	if (!charset)
		return rufl_OUT_OF_MEMORY;
	for (i = 0; i != 256; i++)
		charset->index[i] = BLOCK_EMPTY;

	/* Firstly, search through available encodings (Symbol fonts fail) */
	while (context != -1) {
		struct rufl_unicode_map *temp;

		rufl_fm_error = xfont_list_fonts((byte *) encoding, 
				font_RETURN_FONT_NAME |
				0x400000 /* Return encoding name, instead */ |
				context, 
				sizeof(encoding), NULL, 0, NULL, 
				&context, NULL, NULL);
		if (rufl_fm_error) {
			LOG("xfont_list_fonts: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess);
			free(charset);
			for (i = 0; i < num_umaps; i++)
				free((umap + i)->encoding);
			free(umap);
			return rufl_FONT_MANAGER_ERROR;
		}
		if (context == -1)
			break;

		temp = realloc(umap, (num_umaps + 1) * sizeof *umap);
		if (!temp) {
			free(charset);
			for (i = 0; i < num_umaps; i++)
				free((umap + i)->encoding);
			free(umap);
			return rufl_OUT_OF_MEMORY;
		}

		memset(temp + num_umaps, 0, sizeof *umap);

		umap = temp;
		num_umaps++;

		code = rufl_init_scan_font_in_encoding(font_name, encoding,
				charset, umap + (num_umaps - 1), &last_used);
		if (code != rufl_OK) {
			/* Not finding the font isn't fatal */
			if (code != rufl_FONT_MANAGER_ERROR ||
				(rufl_fm_error->errnum != 
						error_FONT_NOT_FOUND &&
				rufl_fm_error->errnum !=
						error_FILE_NOT_FOUND)) {
				free(charset);
				for (i = 0; i < num_umaps; i++)
					free((umap + i)->encoding);
				free(umap);
				return code;
			}

			/* Ensure we reuse the currently allocated umap */
			num_umaps--;
		} else {
			/* If this mapping is identical to an existing one, 
			 * then we can discard it */
			for (i = 0; i != num_umaps - 1; i++) {
				const struct rufl_unicode_map *a = (umap + i);
				const struct rufl_unicode_map *b = 
							(umap + num_umaps - 1);

				if (a->entries == b->entries &&
						memcmp(a->map, b->map, 
							sizeof a->map) == 0) {
					/* Found identical map; discard */
					num_umaps--;
					break;
				}
			}
		}
	}

	if (num_umaps == 0) {
		/* This is a symbol font and can only be used 
		 * without an encoding */
		struct rufl_unicode_map *temp;

		temp = realloc(umap, (num_umaps + 1) * sizeof *umap);
		if (!temp) {
			free(charset);
			free(umap);
			return rufl_OUT_OF_MEMORY;
		}

		memset(temp + num_umaps, 0, sizeof *umap);

		umap = temp;
		num_umaps++;

		code = rufl_init_scan_font_in_encoding(font_name, NULL,
				charset, umap, &last_used);
		if (code != rufl_OK) {
			/* Not finding the font isn't fatal */
			if (code != rufl_FONT_MANAGER_ERROR ||
				(rufl_fm_error->errnum != 
						error_FONT_NOT_FOUND &&
				rufl_fm_error->errnum !=
						error_FILE_NOT_FOUND)) {
				free(charset);
				for (i = 0; i < num_umaps; i++)
					free((umap + i)->encoding);
				free(umap);
				return code;
			}

			num_umaps--;
		}
	}

	/* shrink-wrap */
	charset->size = offsetof(struct rufl_character_set, block) +
			32 * last_used;
	charset2 = realloc(charset, charset->size);
	if (!charset2) {
		for (i = 0; i < num_umaps; i++)
			free((umap + i)->encoding);
		free(umap);
		free(charset);
		return rufl_OUT_OF_MEMORY;
	}

	rufl_font_list[font_index].charset = charset;
	rufl_font_list[font_index].umap = umap;
	rufl_font_list[font_index].num_umaps = num_umaps;

	return rufl_OK;
}

/**
 * Helper function for rufl_init_scan_font_old.
 * Scans the given font using the given font encoding (or none, if NULL)
 */

rufl_code rufl_init_scan_font_in_encoding(const char *font_name, 
		const char *encoding, struct rufl_character_set *charset,
		struct rufl_unicode_map *umap, unsigned int *last)
{
	char string[2] = { 0, 0 };
	int x_out, y_out;
	unsigned int byte, bit;
	unsigned int i;
	unsigned int last_used = *last;
	unsigned int u;
	rufl_code code;
	font_f font;
	font_scan_block block = { { 0, 0 }, { 0, 0 }, -1, { 0, 0, 0, 0 } };
	char buf[80];

	if (encoding)
		snprintf(buf, sizeof buf, "%s\\E%s", font_name, encoding);
	else
		snprintf(buf, sizeof buf, "%s", font_name);

	rufl_fm_error = xfont_find_font(buf, 160, 160, 0, 0, &font, 0, 0);
	if (rufl_fm_error) {
		LOG("xfont_find_font(\"%s\"): 0x%x: %s", buf,
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	code = rufl_init_read_encoding(font, umap);
	if (code != rufl_OK) {
		xfont_lose_font(font);
		return code;
	}

	for (i = 0; i != umap->entries; i++) {
		u = umap->map[i].u;
		string[0] = umap->map[i].c;
		rufl_fm_error = xfont_scan_string(font, (char *) string,
				font_RETURN_BBOX | font_GIVEN_FONT |
				font_GIVEN_LENGTH | font_GIVEN_BLOCK,
				0x7fffffff, 0x7fffffff,
				&block, 0, 1,
				0, &x_out, &y_out, 0);
		if (rufl_fm_error)
			break;

		if (block.bbox.x0 == 0x20000000) {
			/* absent (no definition) */
		} else if (x_out == 0 && y_out == 0 &&
				block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0) {
			/* absent (empty) */
                } else if (block.bbox.x0 == 0 && block.bbox.y0 == 0 &&
				block.bbox.x1 == 0 && block.bbox.y1 == 0 &&
				!rufl_is_space(u)) {
			/* absent (space but not a space character - some
			 * fonts do this) */
		} else {
			/* present */
			if (charset->index[u >> 8] == BLOCK_EMPTY) {
				charset->index[u >> 8] = last_used;
				last_used++;
				if (last_used == 254)
					/* too many characters */
					break;
			}

			byte = (u >> 3) & 31;
			bit = u & 7;
			charset->block[charset->index[u >> 8]][byte] |= 
					1 << bit;
		}
	}

	xfont_lose_font(font);

	if (rufl_fm_error) {
		LOG("xfont_scan_string: 0x%x: %s",
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	if (encoding) {
		umap->encoding = strdup(encoding);
		if (!umap->encoding)
			return rufl_OUT_OF_MEMORY;
	}

	*last = last_used;

	return rufl_OK;
}


/**
 * Parse an encoding file and fill in a rufl_unicode_map.
 */

rufl_code rufl_init_read_encoding(font_f font,
		struct rufl_unicode_map *umap)
{
	unsigned int u = 0;
	unsigned int i = 0;
	int c;
	int n;
	char filename[200];
	char s[200];
	struct rufl_glyph_map_entry *entry;
	FILE *fp;

	rufl_fm_error = xfont_read_encoding_filename(font, filename,
			sizeof filename, 0);
	if (rufl_fm_error) {
		LOG("xfont_read_encoding_filename: 0x%x: %s",
				rufl_fm_error->errnum, rufl_fm_error->errmess);
		return rufl_FONT_MANAGER_ERROR;
	}

	fp = fopen(filename, "r");
	if (!fp)
		/* many "symbol" fonts have no encoding file: assume Latin 1 */
		fp = fopen("Resources:$.Fonts.Encodings.Latin1", "r");
	if (!fp)
		return rufl_IO_ERROR;

	while (!feof(fp) && u != 256) {
		c = fgetc(fp);
		if (c == '%') {
			/* comment line */
			fgets(s, sizeof s, fp);
		} else if (c == '/') {
			/* character definition */
			if (i++ < 32)
				continue;
			n = fscanf(fp, "%100s", s);
			if (n != 1)
				break;
			entry = bsearch(s, rufl_glyph_map,
					rufl_glyph_map_size,
					sizeof rufl_glyph_map[0],
					rufl_glyph_map_cmp);
			if (entry) {
				/* may be more than one unicode for the glyph
				 * sentinels stop overshooting array */
				while (strcmp(s, (entry - 1)->glyph_name) == 0)
					entry--;
				for (; strcmp(s, entry->glyph_name) == 0;
						entry++) {
					umap->map[u].u = entry->u;
					umap->map[u].c = i - 1;
					u++;
					if (u == 256)
						break;
				}
			}
		}
	}

	if (fclose(fp) == EOF)
		return rufl_IO_ERROR;

	/* sort by unicode */
	qsort(umap->map, u, sizeof umap->map[0], rufl_unicode_map_cmp);
	umap->entries = u;

	return rufl_OK;
}


int rufl_glyph_map_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const struct rufl_glyph_map_entry *entry = datum;
	return strcmp(key, entry->glyph_name);
}


int rufl_unicode_map_cmp(const void *z1, const void *z2)
{
	const struct rufl_unicode_map_entry *entry1 = z1;
	const struct rufl_unicode_map_entry *entry2 = z2;
	if (entry1->u < entry2->u)
		return -1;
	else if (entry2->u < entry1->u)
		return 1;
	return 0;
}


/**
 * Construct the font substitution table.
 */

rufl_code rufl_init_substitution_table(void)
{
	unsigned char z;
	unsigned int i;
	unsigned int block, byte, bit;
	unsigned int u;
	unsigned int index;
	const struct rufl_character_set *charset;

	rufl_substitution_table = malloc(65536 *
			sizeof rufl_substitution_table[0]);
	if (!rufl_substitution_table) {
		LOG("malloc(%zu) failed", 65536 *
				sizeof rufl_substitution_table[0]);
		return rufl_OUT_OF_MEMORY;
	}

	for (u = 0; u != 0x10000; u++)
		rufl_substitution_table[u] = NOT_AVAILABLE;

	for (i = 0; i != rufl_font_list_entries; i++) {
		charset = rufl_font_list[i].charset;
		if (!charset)
			continue;
		for (block = 0; block != 256; block++) {
			if (charset->index[block] == BLOCK_EMPTY)
				continue;
			if (charset->index[block] == BLOCK_FULL) {
				for (u = block << 8; u != (block << 8) + 256;
						u++) {
					if (rufl_substitution_table[u] ==
							NOT_AVAILABLE)
						rufl_substitution_table[u] = i;
				}
				continue;
			}
			index = charset->index[block];
			for (byte = 0; byte != 32; byte++) {
				z = charset->block[index][byte];
				if (z == 0)
					continue;
				u = (block << 8) | (byte << 3);
				for (bit = 0; bit != 8; bit++, u++) {
					if (rufl_substitution_table[u] ==
							NOT_AVAILABLE &&
							z & (1 << bit))
						rufl_substitution_table[u] = i;
				}
			}
		}
	}

	return rufl_OK;
}


/**
 * Save character sets to cache.
 */

rufl_code rufl_save_cache(void)
{
	unsigned int i;
	const unsigned int version = rufl_CACHE_VERSION;
	size_t len;
	FILE *fp;

	fp = fopen(rufl_CACHE, "wb");
	if (!fp) {
		LOG("fopen: 0x%x: %s", errno, strerror(errno));
		return rufl_OK;
	}

	/* cache format version */
	if (fwrite(&version, sizeof version, 1, fp) != 1) {
		LOG("fwrite: 0x%x: %s", errno, strerror(errno));
		fclose(fp);
		return rufl_OK;
	}

	/* font manager type flag */
	if (fwrite(&rufl_old_font_manager, sizeof rufl_old_font_manager, 1,
			fp) != 1) {
		LOG("fwrite: 0x%x: %s", errno, strerror(errno));
		fclose(fp);
		return rufl_OK;
	}

	for (i = 0; i != rufl_font_list_entries; i++) {
		if (!rufl_font_list[i].charset)
			continue;

		/* length of font identifier */
		len = strlen(rufl_font_list[i].identifier);
		if (fwrite(&len, sizeof len, 1, fp) != 1) {
			LOG("fwrite: 0x%x: %s", errno, strerror(errno));
			fclose(fp);
			return rufl_OK;
		}

		/* font identifier */
		if (fwrite(rufl_font_list[i].identifier, len, 1, fp) != 1) {
			LOG("fwrite: 0x%x: %s", errno, strerror(errno));
			fclose(fp);
			return rufl_OK;
		}

		/* character set */
		if (fwrite(rufl_font_list[i].charset,
				rufl_font_list[i].charset->size, 1, fp) != 1) {
			LOG("fwrite: 0x%x: %s", errno, strerror(errno));
			fclose(fp);
			return rufl_OK;
		}

		/* unicode map */
		if (rufl_old_font_manager) {
			unsigned int j;

			if (fwrite(&rufl_font_list[i].num_umaps,
					sizeof rufl_font_list[i].num_umaps, 1,
					fp) != 1) {
				LOG("fwrite: 0x%x: %s", errno, strerror(errno));
				fclose(fp);
				return rufl_OK;
			}

			for (j = 0; j < rufl_font_list[i].num_umaps; j++) {
				const struct rufl_unicode_map *umap = 
						rufl_font_list[i].umap + j;

				len = umap->encoding ? 
						strlen(umap->encoding) : 0;

				if (fwrite(&len, sizeof len, 1, fp) != 1) {
					LOG("fwrite: 0x%x: %s", 
							errno, strerror(errno));
					fclose(fp);
					return rufl_OK;
				}

				if (umap->encoding) {
					if (fwrite(umap->encoding, len, 1, 
							fp) != 1) {
						LOG("fwrite: 0x%x: %s",
							errno, strerror(errno));
						fclose(fp);
						return rufl_OK;
					}
				}

				if (fwrite(&umap->entries, sizeof umap->entries,
						1, fp) != 1) {
					LOG("fwrite: 0x%x: %s", 
							errno, strerror(errno));
					fclose(fp);
					return rufl_OK;
				}

				if (fwrite(umap->map, umap->entries * 
					sizeof(struct rufl_unicode_map_entry), 
						1, fp) != 1) {
					LOG("fwrite: 0x%x: %s", 
							errno, strerror(errno));
					fclose(fp);
					return rufl_OK;
				}
			}
		}
	}

	if (fclose(fp) == EOF) {
		LOG("fclose: 0x%x: %s", errno, strerror(errno));
		return rufl_OK;
	}

	LOG("%u charsets saved", i);

	return rufl_OK;
}


/**
 * Load character sets from cache.
 */

rufl_code rufl_load_cache(void)
{
	unsigned int version;
	unsigned int i = 0;
	bool old_font_manager;
	char *identifier;
	size_t len, size;
	FILE *fp;
	struct rufl_font_list_entry *entry;
	struct rufl_character_set *charset;
	struct rufl_unicode_map *umap = NULL;
	unsigned int num_umaps = 0;

	fp = fopen(rufl_CACHE, "rb");
	if (!fp) {
		LOG("fopen: 0x%x: %s", errno, strerror(errno));
		return rufl_OK;
	}

	/* cache format version */
	if (fread(&version, sizeof version, 1, fp) != 1) {
		if (feof(fp))
			LOG("fread: %s", "unexpected eof");
		else
			LOG("fread: 0x%x: %s", errno, strerror(errno));
		fclose(fp);
		return rufl_OK;
	}
	if (version != rufl_CACHE_VERSION) {
		/* incompatible cache format */
		LOG("cache version %u (now %u)", version, rufl_CACHE_VERSION);
		fclose(fp);
		return rufl_OK;
	}

	/* font manager type flag */
	if (fread(&old_font_manager, sizeof old_font_manager, 1, fp) != 1) {
		if (feof(fp))
			LOG("fread: %s", "unexpected eof");
		else
			LOG("fread: 0x%x: %s", errno, strerror(errno));
		fclose(fp);
		return rufl_OK;
	}
	if (old_font_manager != rufl_old_font_manager) {
		/* font manager type has changed */
		LOG("font manager %u (now %u)", old_font_manager,
				rufl_old_font_manager);
		fclose(fp);
		return rufl_OK;
	}

	while (!feof(fp)) {
		/* length of font identifier */
		if (fread(&len, sizeof len, 1, fp) != 1) {
			/* eof at this point simply means that the whole cache
			 * file has been loaded */
			if (!feof(fp))
				LOG("fread: 0x%x: %s", errno, strerror(errno));
			break;
		}

		identifier = malloc(len + 1);
		if (!identifier) {
			LOG("malloc(%zu) failed", len + 1);
			fclose(fp);
			return rufl_OUT_OF_MEMORY;
		}

		/* font identifier */
		if (fread(identifier, len, 1, fp) != 1) {
			if (feof(fp))
				LOG("fread: %s", "unexpected eof");
			else
				LOG("fread: 0x%x: %s", errno, strerror(errno));
			free(identifier);
			break;
		}
		identifier[len] = 0;

		/* character set */
		if (fread(&size, sizeof size, 1, fp) != 1) {
			if (feof(fp))
				LOG("fread: %s", "unexpected eof");
			else
				LOG("fread: 0x%x: %s", errno, strerror(errno));
			free(identifier);
			break;
		}

		charset = malloc(size);
		if (!charset) {
			LOG("malloc(%zu) failed", size);
			free(identifier);
			fclose(fp);
			return rufl_OUT_OF_MEMORY;
		}

		charset->size = size;
		if (fread(charset->index, size - sizeof size, 1, fp) != 1) {
			if (feof(fp))
				LOG("fread: %s", "unexpected eof");
			else
				LOG("fread: 0x%x: %s", errno, strerror(errno));
			free(charset);
			free(identifier);
			break;
		}

		/* unicode map */
		if (rufl_old_font_manager) {
			rufl_code code = rufl_OK;
			unsigned int entry;

			/* Number of maps */
			if (fread(&num_umaps, sizeof num_umaps, 1, fp) != 1) {
				if (feof(fp))
					LOG("fread: %s", "unexpected eof");
				else
					LOG("fread: 0x%x: %s", errno,
							strerror(errno));
				free(charset);
				free(identifier);
				break;
			}

			umap = calloc(num_umaps, sizeof *umap);
			if (!umap) {
				LOG("malloc(%zu) failed", sizeof *umap);
				free(charset);
				free(identifier);
				fclose(fp);
				return rufl_OUT_OF_MEMORY;
			}

			/* Load them */
			for (entry = 0; entry < num_umaps; entry++) {
				struct rufl_unicode_map *map = umap + entry;

				if (fread(&len, sizeof(len), 1, fp) != 1) {
					if (feof(fp))
						LOG("fread: %s", 
							"unexpected eof");
					else
						LOG("fread: 0x%x: %s", errno,
							strerror(errno));
					break;
				}

				if (len > 0) {
					map->encoding = malloc(len + 1);
					if (!map->encoding) {
						LOG("malloc(%zu) failed", 
								len + 1);
						code = rufl_OUT_OF_MEMORY;
						break;
					}

					if (fread(map->encoding, len, 1, 
							fp) != 1) {
						if (feof(fp))
							LOG("fread: %s", 
							"unexpected eof");
						else
							LOG("fread: 0x%x: %s", 
							errno,
							strerror(errno));
						break;
					}
					map->encoding[len] = 0;
				}

				if (fread(&map->entries, sizeof(map->entries),
						1, fp) != 1) {
					if (feof(fp))
						LOG("fread: %s", 
							"unexpected eof");
					else
						LOG("fread: 0x%x: %s", errno,
							strerror(errno));
					break;
				}

				if (fread(map->map, map->entries * 
					sizeof(struct rufl_unicode_map_entry), 
						1, fp) != 1) {
					if (feof(fp))
						LOG("fread: %s", 
							"unexpected eof");
					else
						LOG("fread: 0x%x: %s", errno,
							strerror(errno));
					break;
				}
			}

			/* Clean up if loading failed */
			if (entry != num_umaps) {
				for (num_umaps = 0; num_umaps <= entry; 
						num_umaps++) {
					struct rufl_unicode_map *map =
							umap + num_umaps;

					free(map->encoding);
				}
				free(umap);
				free(charset);
				free(identifier);

				if (code != rufl_OK)
					return code;

				break;
			}
		}

		/* put in rufl_font_list */
		entry = lfind(identifier, rufl_font_list,
				&rufl_font_list_entries,
				sizeof rufl_font_list[0], rufl_font_list_cmp);
		if (entry) {
			entry->charset = charset;
			entry->umap = umap;
			entry->num_umaps = num_umaps;
	                i++;
		} else {
			LOG("\"%s\" not in font list", identifier);
			while (num_umaps > 0) {
				struct rufl_unicode_map *map = 
						umap + num_umaps - 1;

				free(map->encoding);

				num_umaps--;
			}
			free(umap);
			free(charset);
		}

                free(identifier);
	}
	fclose(fp);

	LOG("%u charsets loaded", i);

	return rufl_OK;
}


int rufl_font_list_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const struct rufl_font_list_entry *entry = datum;
	return strcasecmp(key, entry->identifier);
}


/**
 * Create a menu of font families.
 */

rufl_code rufl_init_family_menu(void)
{
	wimp_menu *menu;
	unsigned int i;

	menu = malloc(wimp_SIZEOF_MENU(rufl_family_list_entries));
	if (!menu)
		return rufl_OUT_OF_MEMORY;
	menu->title_data.indirected_text.text = (char *) "Fonts";
	menu->title_fg = wimp_COLOUR_BLACK;
	menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	menu->work_fg = wimp_COLOUR_BLACK;
	menu->work_bg = wimp_COLOUR_WHITE;
	menu->width = 200;
	menu->height = wimp_MENU_ITEM_HEIGHT;
	menu->gap = wimp_MENU_ITEM_GAP;
	for (i = 0; i != rufl_family_list_entries; i++) {
		menu->entries[i].menu_flags = 0;
		menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		menu->entries[i].icon_flags = wimp_ICON_TEXT |
			wimp_ICON_INDIRECTED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
		menu->entries[i].data.indirected_text.text =
				(char *) rufl_family_list[i];
		menu->entries[i].data.indirected_text.validation = (char *) -1;
		menu->entries[i].data.indirected_text.size =
				strlen(rufl_family_list[i]);
	}
	menu->entries[0].menu_flags = wimp_MENU_TITLE_INDIRECTED;
	menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;

	rufl_family_menu = menu;

	return rufl_OK;
}


/**
 * Create and open the init status window.
 */

void rufl_init_status_open(void)
{
	int xeig_factor, yeig_factor, xwind_limit, ywind_limit, width, height;
	wimp_t task;
	osbool window_task;
	wimp_WINDOW(4) window = { { 0, 0, 0, 0 }, 0, 0, wimp_TOP,
			wimp_WINDOW_AUTO_REDRAW | wimp_WINDOW_NEW_FORMAT,
			wimp_COLOUR_BLACK, wimp_COLOUR_LIGHT_GREY,
			wimp_COLOUR_BLACK, wimp_COLOUR_VERY_LIGHT_GREY,
			wimp_COLOUR_DARK_GREY, wimp_COLOUR_LIGHT_GREY,
			wimp_COLOUR_CREAM, 0,
			{ 0, -128, 800, 0 }, 0, 0, 0, 0, 0, { "" }, 4,
			{ { { 12, -56, 788, -12 }, wimp_ICON_TEXT |
			wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
			wimp_ICON_INDIRECTED |
			wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT |
			wimp_COLOUR_VERY_LIGHT_GREY <<wimp_ICON_BG_COLOUR_SHIFT,
			{ "" } },
			{ { 12, -116, 788, -64 }, wimp_ICON_TEXT |
			wimp_ICON_FILLED | wimp_ICON_BORDER |
			wimp_ICON_INDIRECTED |
			wimp_COLOUR_VERY_LIGHT_GREY <<wimp_ICON_BG_COLOUR_SHIFT,
			{ "" } },
			{ { 16, -112, 16, -68 }, wimp_ICON_FILLED |
			wimp_COLOUR_ORANGE << wimp_ICON_BG_COLOUR_SHIFT,
			{ "" } },
			{ { 16, -112, 784, -68 }, wimp_ICON_TEXT |
			wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
			wimp_ICON_INDIRECTED |
			wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT |
			wimp_COLOUR_MID_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT,
			{ "" } } },
			};
	wimp_window_state state;
	os_error *error;

	window.icons[0].data.indirected_text.text =
			(char *) "Scanning fonts - please wait";
	window.icons[0].data.indirected_text.validation = (char *) "";
	window.icons[1].data.indirected_text.text = (char *) "";
	window.icons[1].data.indirected_text.validation = (char *) "r2";
	window.icons[3].data.indirected_text.text = rufl_status_buffer;
	window.icons[3].data.indirected_text.validation = (char *) "";

	xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XEIG_FACTOR,
			&xeig_factor, 0);
	xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YEIG_FACTOR,
			&yeig_factor, 0);
	xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_XWIND_LIMIT,
			&xwind_limit, 0);
	xos_read_mode_variable(os_CURRENT_MODE, os_MODEVAR_YWIND_LIMIT,
			&ywind_limit, 0);
	width = (xwind_limit + 1) << xeig_factor;
	height = (ywind_limit + 1) << yeig_factor;

	window.visible.x0 = width / 2 - 400;
	window.visible.y0 = height / 2 - 64;
	window.visible.x1 = window.visible.x0 + 800;
	window.visible.y1 = window.visible.y0 + 128;

	error = xwimpreadsysinfo_task(&task, 0);
	if (error) {
		LOG("xwimpreadsysinfo_task: 0x%x: %s",
				error->errnum, error->errmess);
		return;
	}
	if (!task)
		return;  /* not a Wimp task */

	error = xtaskwindowtaskinfo_window_task(&window_task);
	if (error) {
		LOG("xtaskwindowtaskinfo_window_task: 0x%x: %s",
				error->errnum, error->errmess);
		return;
	}
	if (window_task)
		return;  /* in a TaskWindow */

	xwimp_create_window((const wimp_window *) &window, &rufl_status_w);
	state.w = rufl_status_w;
	xwimp_get_window_state(&state);
	xwimp_open_window((wimp_open *) (void *) &state);
}


/**
 * Update the status window and multitask.
 */

void rufl_init_status(const char *status, float progress)
{
	wimp_block block;
	static os_t last_t = 0;
	os_t t;

	if (!rufl_status_w)
		return;

	if (status) {
		strncpy(rufl_status_buffer, status, sizeof rufl_status_buffer);
		rufl_status_buffer[sizeof rufl_status_buffer - 1] = 0;
		xwimp_set_icon_state(rufl_status_w, 3, 0, 0);
	}
	if (progress)
		xwimp_resize_icon(rufl_status_w, 2, 16, -112,
				16 + 768 * progress, -68);
	xos_read_monotonic_time(&t);
	if (last_t == t)
		return;
	xwimp_poll(wimp_QUEUE_REDRAW | wimp_MASK_LEAVING | wimp_MASK_ENTERING |
			wimp_MASK_LOSE | wimp_MASK_GAIN | wimp_MASK_MESSAGE |
			wimp_MASK_RECORDED | wimp_MASK_ACKNOWLEDGE,
			&block, 0, 0);
	last_t = t;
}


/**
 * Close and delete the status window.
 */

void rufl_init_status_close(void)
{
	if (!rufl_status_w)
		return;

	xwimp_delete_window(rufl_status_w);
	rufl_status_w = 0;
}
