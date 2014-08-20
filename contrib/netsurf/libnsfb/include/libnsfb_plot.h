/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the exported plotter interface for the libnsfb graphics library.
 */

#ifndef _LIBNSFB_PLOT_H
#define _LIBNSFB_PLOT_H 1

/** representation of a colour.
 *
 * The colour value comprises of four components arranged in the order ABGR:
 * bits 24-31 are the alpha value and represent the opacity. 0 is
 *   transparent i.e. there would be no change in the target surface if
 *   this colour were to be used and 0xFF is opaque.
 *
 * bits 16-23 are the Blue component of the colour.
 *
 * bits 8-15 are the Green component of the colour.
 *
 * bits 0-7 are the Red component of the colour.
 */
typedef uint32_t nsfb_colour_t;

/**
 * Type of plot operation
 */
typedef enum nsfb_plot_optype_e {
	NFSB_PLOT_OPTYPE_NONE = 0, /**< No operation */
	NFSB_PLOT_OPTYPE_SOLID, /**< Solid colour */
	NFSB_PLOT_OPTYPE_PATTERN, /**< Pattern plot */
} nsfb_plot_optype_t;

/** pen colour and raster operation for plotting primatives. */
typedef struct nsfb_plot_pen_s {
	nsfb_plot_optype_t stroke_type; /**< Stroke plot type */
	int stroke_width; /**< Width of stroke, in pixels */
	nsfb_colour_t stroke_colour; /**< Colour of stroke */
	uint32_t stroke_pattern;
	nsfb_plot_optype_t fill_type; /**< Fill plot type */
	nsfb_colour_t fill_colour; /**< Colour of fill */
} nsfb_plot_pen_t;

/** path operation type. */
typedef enum nsfb_plot_pathop_type_e {
	NFSB_PLOT_PATHOP_MOVE,
	NFSB_PLOT_PATHOP_LINE,
	NFSB_PLOT_PATHOP_QUAD,
	NFSB_PLOT_PATHOP_CUBIC,
} nsfb_plot_pathop_type_t;

/** path element */
typedef struct nsfb_plot_pathop_s {
	nsfb_plot_pathop_type_t operation;
	nsfb_point_t point;
} nsfb_plot_pathop_t;

/** Sets a clip rectangle for subsequent plots.
 *
 * Sets a clipping area which constrains all subsequent plotting operations.
 * The clipping area must lie within the framebuffer visible screen or false
 * will be returned and the new clipping area not set.
 */
bool nsfb_plot_set_clip(nsfb_t *nsfb, nsfb_bbox_t *clip);

/** Get the previously set clipping region.
 */
bool nsfb_plot_get_clip(nsfb_t *nsfb, nsfb_bbox_t *clip);

/** Clears plotting area to a flat colour.
 */
bool nsfb_plot_clg(nsfb_t *nsfb, nsfb_colour_t c);

/** Plots a rectangle outline. 
 *
 * The line can be solid, dotted or dashed. Top left corner at (x0,y0) and
 * rectangle has given width and height.
 */
bool nsfb_plot_rectangle(nsfb_t *nsfb, nsfb_bbox_t *rect, int line_width, nsfb_colour_t c, bool dotted, bool dashed);

/** Plots a filled rectangle. Top left corner at (x0,y0), bottom
 *		  right corner at (x1,y1). Note: (x0,y0) is inside filled area,
 *		  but (x1,y1) is below and to the right. See diagram below.
 */
bool nsfb_plot_rectangle_fill(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t c);

/** Plots a line.
 *
 * Draw a line from (x0,y0) to (x1,y1). Coordinates are at centre of line
 * width/thickness.
 */
bool nsfb_plot_line(nsfb_t *nsfb, nsfb_bbox_t *line, nsfb_plot_pen_t *pen);

/** Plots a number of lines.
 *
 * Draw a series of lines.
 */
bool nsfb_plot_lines(nsfb_t *nsfb, int linec, nsfb_bbox_t *line, nsfb_plot_pen_t *pen);

/** Plots a number of connected lines.
 *
 * Draw a series of connected lines.
 */
bool nsfb_plot_polylines(nsfb_t *nsfb, int pointc, const nsfb_point_t *points, nsfb_plot_pen_t *pen);

/** Plots a filled polygon. 
 *
 * Plots a filled polygon with straight lines between points. The lines around
 * the edge of the ploygon are not plotted. The polygon is filled with a
 * non-zero winding rule.
 *
 *
 */
bool nsfb_plot_polygon(nsfb_t *nsfb, const int *p, unsigned int n, nsfb_colour_t fill);

/** Plot an ellipse.
 */
bool nsfb_plot_ellipse(nsfb_t *nsfb, nsfb_bbox_t *ellipse, nsfb_colour_t c);

/** Plot a filled ellipse.
 */
bool nsfb_plot_ellipse_fill(nsfb_t *nsfb, nsfb_bbox_t *ellipse, nsfb_colour_t c);

/** Plots an arc.
 *
 * around (x,y), from anticlockwise from angle1 to angle2. Angles are measured
 * anticlockwise from horizontal, in degrees.
 */
bool nsfb_plot_arc(nsfb_t *nsfb, int x, int y, int radius, int angle1, int angle2, nsfb_colour_t c);

/** Plots an alpha blended pixel.
 *
 * plots an alpha blended pixel.
 */
bool nsfb_plot_point(nsfb_t *nsfb, int x, int y, nsfb_colour_t c);

bool nsfb_plot_cubic_bezier(nsfb_t *nsfb, nsfb_bbox_t *curve, nsfb_point_t *ctrla, nsfb_point_t *ctrlb, nsfb_plot_pen_t *pen);

bool nsfb_plot_quadratic_bezier(nsfb_t *nsfb, nsfb_bbox_t *curve, nsfb_point_t *ctrla, nsfb_plot_pen_t *pen);

bool nsfb_plot_path(nsfb_t *nsfb, int pathc, nsfb_plot_pathop_t *pathop, nsfb_plot_pen_t *pen);

/** copy an area of screen 
 *
 * Copy an area of the display.
 */
bool nsfb_plot_copy(nsfb_t *srcfb, nsfb_bbox_t *srcbox, nsfb_t *dstfb, nsfb_bbox_t *dstbox);

/** Plot bitmap.
 */
bool nsfb_plot_bitmap(nsfb_t *nsfb, const nsfb_bbox_t *loc, const nsfb_colour_t *pixel, int bmp_width, int bmp_height, int bmp_stride, bool alpha);

/** Plot bitmap.
 */
bool nsfb_plot_bitmap_tiles(nsfb_t *nsfb, const nsfb_bbox_t *loc, int tiles_x, int tiles_y, const nsfb_colour_t *pixel, int bmp_width, int bmp_height, int bmp_stride, bool alpha);

/** Plot an 8 bit glyph.
 */
bool nsfb_plot_glyph8(nsfb_t *nsfb, nsfb_bbox_t *loc, const uint8_t *pixel, int pitch, nsfb_colour_t c);


/** Plot an 1 bit glyph.
 */
bool nsfb_plot_glyph1(nsfb_t *nsfb, nsfb_bbox_t *loc, const uint8_t *pixel, int pitch, nsfb_colour_t c);

/* read rectangle into buffer */
bool nsfb_plot_readrect(nsfb_t *nsfb, nsfb_bbox_t *rect, nsfb_colour_t *buffer);

#endif /* _LIBNSFB_PLOT_H */
