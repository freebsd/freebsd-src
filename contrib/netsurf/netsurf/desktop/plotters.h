/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Target independent plotting (interface).
 */

#ifndef _NETSURF_DESKTOP_PLOTTERS_H_
#define _NETSURF_DESKTOP_PLOTTERS_H_

#include <stdbool.h>
#include "css/css.h"
#include "content/content.h"
#include "desktop/plot_style.h"

struct bitmap;

typedef unsigned long bitmap_flags_t;
#define BITMAPF_NONE 0
#define BITMAPF_REPEAT_X 1
#define BITMAPF_REPEAT_Y 2


/** Set of target specific plotting functions.
 *
 * The functions are:
 *  arc		- Plots an arc, around (x,y), from anticlockwise from angle1 to
 *		  angle2. Angles are measured anticlockwise from horizontal, in
 *		  degrees.
 *  disc	- Plots a circle, centered on (x,y), which is optionally filled.
 *  line	- Plots a line from (x0,y0) to (x1,y1). Coordinates are at
 *		  centre of line width/thickness.
 *  path	- Plots a path consisting of cubic Bezier curves. Line colour is
 *		  given by c and fill colour is given by fill.
 *  polygon	- Plots a filled polygon with straight lines between points.
 *		  The lines around the edge of the ploygon are not plotted. The
 *		  polygon is filled with the non-zero winding rule.
 *  rectangle	- Plots a rectangle outline. The line can be solid, dotted or
 *		  dashed. Top left corner at (x0,y0) and rectangle has given
 *		  width and height.
 *  fill	- Plots a filled rectangle. Top left corner at (x0,y0), bottom
 *		  right corner at (x1,y1). Note: (x0,y0) is inside filled area,
 *		  but (x1,y1) is below and to the right. See diagram below.
 *  clip	- Sets a clip rectangle for subsequent plots.
 *  text	- Plots text. (x,y) is the coordinate of the left hand side of
 *		  the text's baseline. The text is UTF-8 encoded. The colour, c,
 *		  is the colour of the text. Background colour, bg, may be used
 *		  optionally to attempt to provide anti-aliased text without
 *		  screen reads. Font information is provided in the style.
 *  bitmap     	- Tiled plot of a bitmap image. (x,y) gives the top left
 *		  coordinate of an explicitly placed tile. From this tile the
 *		  image can repeat in all four directions -- up, down, left and
 *		  right -- to the extents given by the current clip rectangle.
 *		  The bitmap_flags say whether to tile in the x and y
 *		  directions. If not tiling in x or y directions, the single
 *		  image is plotted. The width and height give the dimensions
 *		  the image is to be scaled to.
 *  group_start	- Start of a group of objects. Used when plotter implements
 *		  export to a vector graphics file format. (Optional.)
 *  group_end	- End of the most recently started group. (Optional.)
 *  flush	- Only used internally by the knockout code. Should be NULL in
 *		  any front end display plotters or export plotters.
 *
 * Plotter options:
 *  option_knockout	- Optimisation particularly for unaccelerated screen
 *			  redraw. It tries to avoid plotting to the same area
 *			  more than once. See desktop/knockout.c
 *
 * Coordinates are from top left of canvas and (0,0) is the top left grid
 * denomination. If a "fill" is drawn from (0,0) to (4,3), the result is:
 *
 *     0 1 2 3 4 5
 *    +-+-+-+-+-+-
 *  0 |#|#|#|#| |
 *    +-+-+-+-+-+-
 *  1 |#|#|#|#| |
 *    +-+-+-+-+-+-
 *  2 |#|#|#|#| |
 *    +-+-+-+-+-+-
 *  3 | | | | | |
 */
struct plotter_table {
	/* clipping operations */
	bool (*clip)(const struct rect *clip);

        /* shape primatives */
	bool (*arc)(int x, int y, int radius, int angle1, int angle2, const plot_style_t *pstyle);
	bool (*disc)(int x, int y, int radius, const plot_style_t *pstyle);
	bool (*line)(int x0, int y0, int x1, int y1, const plot_style_t *pstyle);
	bool (*rectangle)(int x0, int y0, int x1, int y1, const plot_style_t *pstyle);
	bool (*polygon)(const int *p, unsigned int n, const plot_style_t *pstyle);

	/* complex path (for SVG) */
	bool (*path)(const float *p, unsigned int n, colour fill, float width,
			colour c, const float transform[6]);

        /* Image */
	bool (*bitmap)(int x, int y, int width, int height,
			struct bitmap *bitmap, colour bg,
			bitmap_flags_t flags);

	/**
	 * Text.
	 *
	 * \param  x       x coordinate
	 * \param  y       y coordinate
	 * \param  text    UTF-8 string to plot
	 * \param  length  length of string, in bytes
	 * \param  fstyle  plot style for this text
	 * \return  true on success, false on error and error reported
	 */
	bool (*text)(int x, int y, const char *text, size_t length,
			const plot_font_style_t *fstyle);

        /* optional callbacks */
	bool (*group_start)(const char *name);  /**< optional, may be NULL */
	bool (*group_end)(void);		/**< optional, may be NULL */
	bool (*flush)(void);			/**< optional, may be NULL */

        /* flags */
	bool option_knockout;	/**< set if knockout rendering is required */
};

enum path_command {
	PLOTTER_PATH_MOVE,
	PLOTTER_PATH_CLOSE,
	PLOTTER_PATH_LINE,
	PLOTTER_PATH_BEZIER,
};


#endif
