/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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
 * Font handling (RISC OS implementation).
 *
 * The RUfl is used handle and render fonts.
 */

#include <assert.h>
#include <string.h>
#include "oslib/wimp.h"
#include "oslib/wimpreadsysinfo.h"
#include "rufl.h"
#include "css/css.h"
#include "css/utils.h"
#include "render/font.h"
#include "riscos/gui.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

static void nsfont_check_option(char **option, const char *family,
		const char *fallback);
static int nsfont_list_cmp(const void *keyval, const void *datum);
static void nsfont_check_fonts(void);
static void ro_gui_wimp_desktop_font(char *family, size_t bufsize, int *psize,
		rufl_style *pstyle);
static bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width);
static bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);
static bool nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);

/** desktop font, size and style being used */
char ro_gui_desktop_font_family[80];
int ro_gui_desktop_font_size = 12;
rufl_style ro_gui_desktop_font_style = rufl_WEIGHT_400;
bool no_font_blending = false;

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

/**
 * Initialize font handling.
 *
 * Exits through die() on error.
 */

void nsfont_init(void)
{
	const char *fallback;
	rufl_code code;

	nsfont_check_fonts();

	LOG(("Initialise RUfl"));
	code = rufl_init();
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_init: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_init: 0x%x", code));
		die("The Unicode font library could not be initialized. "
				"Please report this to the developers.");
	}
	LOG(("RUfl initialised"));

	if (rufl_family_list_entries == 0)
		die("No fonts could be found. At least one font must be "
				"installed.");

	fallback = nsfont_fallback_font();

	nsfont_check_option(&nsoption_charp(font_sans), "Homerton", fallback);
	nsfont_check_option(&nsoption_charp(font_serif), "Trinity", fallback);
	nsfont_check_option(&nsoption_charp(font_mono), "Corpus", fallback);
	nsfont_check_option(&nsoption_charp(font_cursive), "Churchill", fallback);
	nsfont_check_option(&nsoption_charp(font_fantasy), "Sassoon", fallback);

	if (nsoption_int(font_default) != PLOT_FONT_FAMILY_SANS_SERIF &&
            nsoption_int(font_default) != PLOT_FONT_FAMILY_SERIF &&
            nsoption_int(font_default) != PLOT_FONT_FAMILY_MONOSPACE &&
            nsoption_int(font_default) != PLOT_FONT_FAMILY_CURSIVE &&
            nsoption_int(font_default) != PLOT_FONT_FAMILY_FANTASY) {
            nsoption_set_int(font_default, PLOT_FONT_FAMILY_SANS_SERIF);
        }
}


/**
 * Retrieve the fallback font name
 *
 * \return Fallback font name
 */
const char *nsfont_fallback_font(void)
{
	const char *fallback = "Homerton";

	if (!nsfont_exists(fallback)) {
		LOG(("Homerton not found, dumping RUfl family list"));
		for (unsigned int i = 0; i < rufl_family_list_entries; i++) {
			LOG(("'%s'", rufl_family_list[i]));
		}
		fallback = rufl_family_list[0];
	}

	return fallback;
}

/**
 * Check that a font option is valid, and fix it if not.
 *
 * \param  option    pointer to option, as used by options.[ch]
 * \param  family    font family to use if option is not set, or the set
 *                   family is not available
 * \param  fallback  font family to use if family is not available either
 */

void nsfont_check_option(char **option, const char *family,
		const char *fallback)
{
	if (*option && !nsfont_exists(*option)) {
		free(*option);
		*option = 0;
	}
	if (!*option) {
		if (nsfont_exists(family))
			*option = strdup(family);
		else
			*option = strdup(fallback);
	}
}


/**
 * Check if a font family is available.
 *
 * \param  font_family  name of font family
 * \return  true if the family is available
 */

bool nsfont_exists(const char *font_family)
{
	if (bsearch(font_family, rufl_family_list,
			rufl_family_list_entries, sizeof rufl_family_list[0],
			nsfont_list_cmp))
		return true;
	return false;
}


int nsfont_list_cmp(const void *keyval, const void *datum)
{
	const char *key = keyval;
	const char * const *entry = datum;
	return strcasecmp(key, *entry);
}


/**
 * Check that at least Homerton.Medium is available.
 */

void nsfont_check_fonts(void)
{
	char s[252];
	font_f font;
	os_error *error;

	error = xfont_find_font("Homerton.Medium\\ELatin1",
			160, 160, 0, 0, &font, 0, 0);
	if (error) {
		if (error->errnum == error_FILE_NOT_FOUND) {
			xwimp_start_task("TaskWindow -wimpslot 200K -quit "
					"<NetSurf$Dir>.FixFonts", 0);
			die("FontBadInst");
		} else {
			LOG(("xfont_find_font: 0x%x: %s",
					error->errnum, error->errmess));
			snprintf(s, sizeof s, messages_get("FontError"),
					error->errmess);
			die(s);
		}
	}

	error = xfont_lose_font(font);
	if (error) {
		LOG(("xfont_lose_font: 0x%x: %s",
				error->errnum, error->errmess));
		snprintf(s, sizeof s, messages_get("FontError"),
				error->errmess);
		die(s);
	}
}


/**
 * Measure the width of a string.
 *
 * \param  fstyle  plot style for this text
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(fstyle, &font_family, &font_size, &font_style);
	if (font_size == 0) {
		*width = 0;
		return true;
	}

	code = rufl_width(font_family, font_style, font_size,
			string, length,
			width);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		*width = 0;
		return false;
	}

	*width /= 2;
	return true;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  fstyle       style for this text
 * \param  string       UTF-8 string to measure
 * \param  length       length of string
 * \param  x            x coordinate to search for
 * \param  char_offset  updated to offset in string of actual_x, [0..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(fstyle, &font_family, &font_size, &font_style);
	if (font_size == 0) {
		*char_offset = 0;
		*actual_x = 0;
		return true;
	}

	code = rufl_x_to_offset(font_family, font_style, font_size,
			string, length,
			x * 2, char_offset, actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_x_to_offset: rufl_FONT_MANAGER_ERROR: "
					"0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_x_to_offset: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		*char_offset = 0;
		*actual_x = 0;
		return false;
	}

	*actual_x /= 2;
	return true;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  fstyle       style for this text
 * \param  string       UTF-8 string to measure
 * \param  length       length of string, in bytes
 * \param  x            width available
 * \param  char_offset  updated to offset in string of actual_x, [1..length]
 * \param  actual_x     updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 *
 * On exit, char_offset indicates first character after split point.
 *
 * Note: char_offset of 0 should never be returned.
 *
 *   Returns:
 *     char_offset giving split point closest to x, where actual_x <= x
 *   else
 *     char_offset giving split point closest to x, where actual_x > x
 *
 * Returning char_offset == length means no split possible
 */

bool nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	const char *font_family;
	unsigned int font_size;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(fstyle, &font_family, &font_size, &font_style);
	if (font_size == 0) {
		*char_offset = 0;
		*actual_x = 0;
		return true;
	}

	code = rufl_split(font_family, font_style, font_size,
			string, length,
			x * 2, char_offset, actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_split: rufl_FONT_MANAGER_ERROR: "
					"0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_split: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		*char_offset = 0;
		*actual_x = 0;
		return false;
	}

	if (*char_offset != length) {
		/* we found something to split at */
		size_t orig = *char_offset;

		/* ensure a space at <= the split point we found */
		while (*char_offset && string[*char_offset] != ' ')
			(*char_offset)--;

		/* nothing valid found <= split point, advance to next space */
		if (*char_offset == 0) {
			*char_offset = orig;
			while (*char_offset != length &&
					string[*char_offset] != ' ')
				(*char_offset)++;
		}
	}

	code = rufl_width(font_family, font_style, font_size,
			string, *char_offset,
			actual_x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
/* 		warn_user("MiscError", "font error"); */
		*char_offset = 0;
		*actual_x = 0;
		return false;
	}

	*actual_x /= 2;
	return true;
}


/**
 * Paint a string.
 *
 * \param  fstyle  plot style for this text
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  x       x coordinate
 * \param  y       y coordinate
 * \return  true on success, false on error and error reported
 */

bool nsfont_paint(const plot_font_style_t *fstyle, const char *string,
		size_t length, int x, int y)
{
	const char *font_family;
	unsigned int font_size;
	unsigned int flags = rufl_BLEND_FONT;
	rufl_style font_style;
	rufl_code code;

	nsfont_read_style(fstyle, &font_family, &font_size, &font_style);
	if (font_size == 0)
		return true;

	if (no_font_blending || print_active)
		flags = 0;

	code = rufl_paint(font_family, font_style, font_size,
			string, length, x, y, flags);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_paint: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_paint: 0x%x", code));
	}

	return true;
}


/**
 * Convert a font style to a font family, size and rufl_style.
 *
 * \param  fstyle       plot style for this text
 * \param  font_family  updated to font family
 * \param  font_size    updated to font size
 * \param  font_style   updated to font style
 */

void nsfont_read_style(const plot_font_style_t *fstyle,
		const char **font_family, unsigned int *font_size,
		rufl_style *font_style)
{
	static const rufl_style weight_table[] = {
		rufl_WEIGHT_100,
		rufl_WEIGHT_200,
		rufl_WEIGHT_300,
		rufl_WEIGHT_400,
		rufl_WEIGHT_500,
		rufl_WEIGHT_600,
		rufl_WEIGHT_700,
		rufl_WEIGHT_800,
		rufl_WEIGHT_900
	};

	*font_size = (fstyle->size * 16) / FONT_SIZE_SCALE;
	if (1600 < *font_size)
		*font_size = 1600;

	switch (fstyle->family) {
	case PLOT_FONT_FAMILY_SANS_SERIF:
            *font_family = nsoption_charp(font_sans);
		break;
	case PLOT_FONT_FAMILY_SERIF:
            *font_family = nsoption_charp(font_serif);
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
            *font_family = nsoption_charp(font_mono);
		break;
	case PLOT_FONT_FAMILY_CURSIVE:
            *font_family = nsoption_charp(font_cursive);
		break;
	case PLOT_FONT_FAMILY_FANTASY:
            *font_family = nsoption_charp(font_fantasy);
		break;
	default:
            *font_family = nsoption_charp(font_sans);
		break;
	}

	if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) {
		*font_style = rufl_SLANTED;
	} else {
		*font_style = 0;
	}

	*font_style |= weight_table[(fstyle->weight / 100) - 1];
}


/**
 * Looks up the current desktop font and converts that to a family name,
 * font size and style flags suitable for passing directly to rufl
 *
 * \param  family	buffer to receive font family
 * \param  family_size	buffer size
 * \param  psize	receives the font size in 1/16 points
 * \param  pstyle	receives the style settings to be passed to rufl
 */

void ro_gui_wimp_desktop_font(char *family, size_t family_size, int *psize,
		rufl_style *pstyle)
{
	rufl_style style = rufl_WEIGHT_400;
	os_error *error;
	int ptx, pty;
	font_f font_handle;
	int used;

	assert(family);
	assert(20 < family_size);
	assert(psize);
	assert(pstyle);

	error = xwimpreadsysinfo_font(&font_handle, NULL);
	if (error) {
		LOG(("xwimpreadsysinfo_font: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		goto failsafe;
	}

	if (font_handle == font_SYSTEM) {
		/* Er, yeah; like that's ever gonna work with RUfl */
		goto failsafe;
	}

	error = xfont_read_identifier(font_handle, NULL, &used);
	if (error) {
		LOG(("xfont_read_identifier: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		goto failsafe;
	}

	if (family_size < (size_t) used + 1) {
		LOG(("desktop font name too long"));
		goto failsafe;
	}

	error = xfont_read_defn(font_handle, (byte *) family,
			&ptx, &pty, NULL, NULL, NULL, NULL);
	if (error) {
		LOG(("xfont_read_defn: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		goto failsafe;
	}

	for (size_t i = 0; i != (size_t) used; i++) {
		if (family[i] < ' ') {
			family[i] = 0;
			break;
		}
	}

	LOG(("desktop font \"%s\"", family));

	if (strcasestr(family, ".Medium"))
		style = rufl_WEIGHT_500;
	else if (strcasestr(family, ".Bold"))
		style = rufl_WEIGHT_700;
	if (strcasestr(family, ".Italic") || strcasestr(family, ".Oblique"))
		style |= rufl_SLANTED;

	char *dot = strchr(family, '.');
	if (dot)
		*dot = 0;

	*psize = max(ptx, pty);
	*pstyle = style;

	LOG(("family \"%s\", size %i, style %i", family, *psize, style));

	return;

failsafe:
	strcpy(family, "Homerton");
	*psize = 12*16;
	*pstyle = rufl_WEIGHT_400;
}


/**
 * Retrieve the current desktop font family, size and style from
 * the WindowManager in a form suitable for passing to rufl
 */

void ro_gui_wimp_get_desktop_font(void)
{
	ro_gui_wimp_desktop_font(ro_gui_desktop_font_family,
		sizeof(ro_gui_desktop_font_family),
		&ro_gui_desktop_font_size,
		&ro_gui_desktop_font_style);
}
