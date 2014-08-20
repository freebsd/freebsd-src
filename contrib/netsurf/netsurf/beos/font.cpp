/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
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
 * Font handling (BeOS implementation).
 * TODO: check for correctness, the code is taken from the GTK one.
 * maybe use the current view instead of constructing a new BFont each time ?
 */


#define __STDBOOL_H__	1
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <Font.h>
#include <String.h>
#include <View.h>
extern "C" {
#include "css/css.h"
#include "render/font.h"
#include "utils/utils.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
}

#include "beos/gui.h"
#include "beos/font.h"
#include "beos/plotters.h"

static bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width);
static bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);
static bool nsfont_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x);

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};


/**
 * Measure the width of a string.
 *
 * \param  fstyle  style for this text
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  width   updated to width of string[0..length)
 * \return  true on success, false on error and error reported
 */

bool nsfont_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	//fprintf(stderr, "%s(, '%s', %d, )\n", __FUNCTION__, string, length);
	BFont font;

	if (length == 0) {
		*width = 0;
		return true;
	}

	nsbeos_style_to_font(font, fstyle);
	*width = (int)font.StringWidth(string, length);
	return true;
}


static int utf8_char_len(const char *c)
{
	uint8 *p = (uint8 *)c;
	uint8 m = 0xE0;
	uint8 v = 0xC0;
	int i;
	if (!*p)
		return 0;
	if ((*p & 0x80) == 0)
		return 1;
	if ((*p & 0xC0) == 0x80)
		return 1; // actually one of the remaining bytes...
	for (i = 2; i < 5; i++) {
		if ((*p & m) == v)
			return i;
		v = (v >> 1) | 0x80;
		m = (m >> 1) | 0x80;
	}
	return i;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  fstyle	style for this text
 * \param  string	UTF-8 string to measure
 * \param  length	length of string
 * \param  x		x coordinate to search for
 * \param  char_offset	updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return  true on success, false on error and error reported
 */

bool nsfont_position_in_string(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	//LOG(("(, '%s', %d, %d, , )", string, length, x));
	//fprintf(stderr, "%s(, '%s', %d, %d, , )\n", __FUNCTION__, string, length, x);
	int index;
	BFont font;

	nsbeos_style_to_font(font, fstyle);
	BString str(string);
	int32 len = str.CountChars();
	float escapements[len];
	float esc = 0.0;
	float current = 0.0;
	int i;
	index = 0;
	font.GetEscapements(string, len, escapements);
	// slow but it should work
	for (i = 0; string[index] && i < len; i++) {
		esc += escapements[i];
		current = font.Size() * esc;
		index += utf8_char_len(&string[index]);
		// is current char already too far away?
		if (x < current)
			break;
	}
	*actual_x = (int)current;
	*char_offset = i; //index;

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
	//fprintf(stderr, "%s(, '%s', %d, %d, , )\n", __FUNCTION__, string, length, x);
	//LOG(("(, '%s', %d, %d, , )", string, length, x));
	int index = 0;
	BFont font;

	nsbeos_style_to_font(font, fstyle);
	BString str(string);
	int32 len = str.CountChars();
	float escapements[len];
	float esc = 0.0;
	float current = 0.0;
	float last_x = 0.0;
	int i;
	int last_space = 0;
	font.GetEscapements(string, len, escapements);
	// very slow but it should work
	for (i = 0; string[index] && i < len; i++) {
		if (string[index] == ' ') {
			last_x = current;
			last_space = index;
		}
		if (x < current && last_space != 0) {
			*actual_x = (int)last_x;
			*char_offset = last_space;
			return true;
		}
		esc += escapements[i];
		current = font.Size() * esc;
		index += utf8_char_len(&string[index]);
	}
	*actual_x = MIN(*actual_x, (int)current);
	*char_offset = index;

	return true;
}


/**
 * Render a string.
 *
 * \param  fstyle  style for this text
 * \param  string  UTF-8 string to measure
 * \param  length  length of string
 * \param  x	   x coordinate
 * \param  y	   y coordinate
 * \return  true on success, false on error and error reported
 */

bool nsfont_paint(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, int y)
{
	//fprintf(stderr, "%s(, '%s', %d, %d, %d, )\n", __FUNCTION__, string, length, x, y);
	//CALLED();
	BFont font;
	rgb_color oldbg;
	rgb_color background;
	rgb_color foreground;
	BView *view;
	float size;

	if (length == 0)
		return true;

	nsbeos_style_to_font(font, fstyle);
	background = nsbeos_rgb_colour(fstyle->background);
	foreground = nsbeos_rgb_colour(fstyle->foreground);

	view = nsbeos_current_gc/*_lock*/();
	if (view == NULL) {
		warn_user("No GC", 0);
		return false;
	}

	oldbg = view->LowColor();
	drawing_mode oldmode = view->DrawingMode();
	view->SetLowColor(B_TRANSPARENT_32_BIT);

	//view->SetScale() XXX

//printf("nsfont_paint: Size: %f\n", font.Size());
	size = (float)font.Size();
#warning XXX use scale

	view->SetFont(&font);
	view->SetHighColor(foreground);
	view->SetDrawingMode(B_OP_OVER);

	BString line(string, length);

	BPoint where(x, y + 1);
	view->DrawString(line.String(), where);
	
	view->SetDrawingMode(oldmode);
	if (memcmp(&oldbg, &background, sizeof(rgb_color)))
		view->SetLowColor(oldbg);

	//nsbeos_current_gc_unlock();

	return true;
}


/**
 * Convert a font style to a PangoFontDescription.
 *
 * \param  fstyle	style for this text
 * \return  a new Pango font description
 */

void nsbeos_style_to_font(BFont &font, const plot_font_style_t *fstyle)
{
	float size;
	uint16 face = 0;
	const char *family;

	switch (fstyle->family) {
	case PLOT_FONT_FAMILY_SERIF:
		family = nsoption_charp(font_serif);
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
		family = nsoption_charp(font_mono);
		break;
	case PLOT_FONT_FAMILY_CURSIVE:
		family = nsoption_charp(font_cursive);
		break;
	case PLOT_FONT_FAMILY_FANTASY:
		family = nsoption_charp(font_fantasy);
		break;
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		family = nsoption_charp(font_sans);
		break;
	}

	if ((fstyle->flags & FONTF_ITALIC)) {
		face = B_ITALIC_FACE;
	} else if ((fstyle->flags & FONTF_OBLIQUE)) {
		face = B_ITALIC_FACE;
		// XXX: no OBLIQUE flag ??
		// maybe find "Oblique" style
		// or use SetShear() ?
	}

#ifndef __HAIKU__XXX
	if (fstyle->weight >= 600) {
		face |= B_BOLD_FACE;
	}
#else
	if (fstyle->weight >= 600) {
		if (fstyle->weight >= 800)
			face |= B_HEAVY_FACE;
		else
			face |= B_BOLD_FACE;
	} else if (fstyle->weight <= 300) {
		face |= B_LIGHT_FACE;
	}
#endif
/*
	case CSS_FONT_WEIGHT_100: weight = 100; break;
	case CSS_FONT_WEIGHT_200: weight = 200; break;
	case CSS_FONT_WEIGHT_300: weight = 300; break;
	case CSS_FONT_WEIGHT_400: weight = 400; break;
	case CSS_FONT_WEIGHT_500: weight = 500; break;
	case CSS_FONT_WEIGHT_600: weight = 600; break;
	case CSS_FONT_WEIGHT_700: weight = 700; break;
	case CSS_FONT_WEIGHT_800: weight = 800; break;
	case CSS_FONT_WEIGHT_900: weight = 900; break;
*/

	if (!face)
		face = B_REGULAR_FACE;

//fprintf(stderr, "nsbeos_style_to_font: %d, %d, %d -> '%s' %04x\n", style->font_family, style->font_style, style->font_weight, family, face);

	if (family) {
		font_family beos_family;

		strncpy(beos_family, family, B_FONT_FAMILY_LENGTH);
		// Ensure it's terminated
		beos_family[B_FONT_FAMILY_LENGTH] = '\0';

		font.SetFamilyAndFace(beos_family, face);
	} else {
		//XXX not used
		font = be_plain_font;
		font.SetFace(face);
	}

//fprintf(stderr, "nsbeos_style_to_font: value %f unit %d\n", style->font_size.value.length.value, style->font_size.value.length.unit);
	size = fstyle->size / FONT_SIZE_SCALE;

//fprintf(stderr, "nsbeos_style_to_font: %f %d\n", size, style->font_size.value.length.unit);

	font.SetSize(size);
}
