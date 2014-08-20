/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <joty@netsurf-browser.org>
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
 * Font handling (interface).
 *
 * These functions provide font related services. They all work on UTF-8 strings
 * with lengths given.
 *
 * Note that an interface to painting is not defined here. Painting is
 * redirected through platform-dependent plotters anyway, so there is no gain in
 * abstracting it here.
 */

#ifndef _NETSURF_RENDER_FONT_H_
#define _NETSURF_RENDER_FONT_H_

#include <stdbool.h>
#include <stddef.h>

#include "css/css.h"
#include "desktop/plot_style.h"

struct font_functions
{
	/**
	 * Measure the width of a string.
	 *
	 * \param  fstyle  plot style for this text
	 * \param  string  UTF-8 string to measure
	 * \param  length  length of string, in bytes
	 * \param  width   updated to width of string[0..length)
	 * \return  true on success, false on error and error reported
	 */
	bool (*font_width)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int *width);
	/**
	 * Find the position in a string where an x coordinate falls.
	 *
	 * \param  fstyle       style for this text
	 * \param  string       UTF-8 string to measure
	 * \param  length       length of string, in bytes
	 * \param  x            x coordinate to search for
	 * \param  char_offset  updated to offset in string of actual_x, [0..length]
	 * \param  actual_x     updated to x coordinate of character closest to x
	 * \return  true on success, false on error and error reported
	 */
	bool (*font_position_in_string)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int x, size_t *char_offset, int *actual_x);
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
	bool (*font_split)(const plot_font_style_t *fstyle,
			const char *string, size_t length,
			int x, size_t *char_offset, int *actual_x);
};

extern const struct font_functions nsfont;

void font_plot_style_from_css(const css_computed_style *css, 
		plot_font_style_t *fstyle);

#endif
