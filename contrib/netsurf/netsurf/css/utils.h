/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#ifndef NETSURF_CSS_UTILS_H_
#define NETSURF_CSS_UTILS_H_

#include "css/css.h"
#include "desktop/plot_style.h"

/* DPI of the screen, in fixed point units */
extern css_fixed nscss_screen_dpi;

/**
 * Convert a CSS color to a NetSurf colour primitive
 *
 * ARGB -> (1-A)BGR
 *
 * \param color  The CSS color to convert
 * \return Corresponding NetSurf colour primitive
 */
#define nscss_color_to_ns(c) \
		( ((~c) & 0xff000000)        | \
		 ((( c) & 0xff0000  ) >> 16) | \
		  (( c) & 0xff00    )        | \
		 ((( c) & 0xff      ) << 16))


/**
 * Convert a NetSurf color to a CSS colour primitive
 *
 * (1-A)BGR -> ARGB
 *
 * \param color  The NetSurf color to convert
 * \return Corresponding CSS colour primitive
 */
#define ns_color_to_nscss(c) \
		( ((~c) & 0xff000000)        | \
		 ((( c) & 0xff0000  ) >> 16) | \
		  (( c) & 0xff00    )        | \
		 ((( c) & 0xff      ) << 16))

/**
 * Determine if a CSS color primitive is transparent
 *
 * \param color  The CSS color to consider
 * \return True if the color is transparent, false otherwise
 */
#define nscss_color_is_transparent(color) \
		(((color) >> 24) == 0)

css_fixed nscss_len2pt(css_fixed length, css_unit unit);
css_fixed nscss_len2px(css_fixed length, css_unit unit, 
		const css_computed_style *style);

#endif
