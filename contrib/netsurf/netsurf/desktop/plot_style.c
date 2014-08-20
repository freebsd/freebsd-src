/*
 * Copyright 2009 Vincent Sanders <vince@kyllikki.org>
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

/** 
 * \file desktop/plot_style.c
 * \brief Plotter global styles.
 *
 * These plot styles are globaly available and used in many places.
 */

#include "desktop/plotters.h"

static plot_style_t plot_style_fill_white_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0xffffff,
};
plot_style_t *plot_style_fill_white = &plot_style_fill_white_static;

static plot_style_t plot_style_fill_black_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0x0,
};
plot_style_t *plot_style_fill_black = &plot_style_fill_black_static;

static plot_style_t plot_style_fill_red_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0x000000ff,
};
plot_style_t *plot_style_fill_red = &plot_style_fill_red_static;

/* Box model debug outline styles for content, padding and margin edges */
static const plot_style_t plot_style_content_edge_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x00ff0000,
	.stroke_width = 1,
};
plot_style_t const * const plot_style_content_edge =
		&plot_style_content_edge_static;

static const plot_style_t plot_style_padding_edge_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x000000ff,
	.stroke_width = 1,
};
plot_style_t const * const plot_style_padding_edge =
		&plot_style_padding_edge_static;

static const plot_style_t plot_style_margin_edge_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x0000ffff,
	.stroke_width = 1,
};
plot_style_t const * const plot_style_margin_edge =
		&plot_style_margin_edge_static;

/* Broken object replacement styles */
static const plot_style_t plot_style_broken_object_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = 0x008888ff,
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x000000ff,
	.stroke_width = 1,
};
plot_style_t const * const plot_style_broken_object =
		&plot_style_broken_object_static;

static const plot_font_style_t plot_fstyle_broken_object_static = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 16 * FONT_SIZE_SCALE,
	.weight = 400,
	.flags = FONTF_NONE,
	.background = 0x8888ff,
	.foreground = 0x000044,
};
plot_font_style_t const * const plot_fstyle_broken_object =
		&plot_fstyle_broken_object_static;

/* caret style used in html_redraw_caret */
static plot_style_t plot_style_caret_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = 0x0000ff,  /* todo - choose a proper colour */
};
plot_style_t *plot_style_caret = &plot_style_caret_static;



/* html redraw widget styles */

/** plot style for filled widget base colour. */
static plot_style_t plot_style_fill_wbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = WIDGET_BASEC,
};
plot_style_t *plot_style_fill_wbasec = &plot_style_fill_wbasec_static;

/** plot style for dark filled widget base colour . */
static plot_style_t plot_style_fill_darkwbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = double_darken_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_fill_darkwbasec = &plot_style_fill_darkwbasec_static;

/** plot style for light filled widget base colour. */
static plot_style_t plot_style_fill_lightwbasec_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = double_lighten_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_fill_lightwbasec = &plot_style_fill_lightwbasec_static;


/** plot style for widget background. */
static plot_style_t plot_style_fill_wblobc_static = {
	.fill_type = PLOT_OP_TYPE_SOLID,
	.fill_colour = WIDGET_BLOBC,
};
plot_style_t *plot_style_fill_wblobc = &plot_style_fill_wblobc_static;

/** plot style for checkbox cross. */
static plot_style_t plot_style_stroke_wblobc_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = WIDGET_BLOBC,
	.stroke_width = 2,
};
plot_style_t *plot_style_stroke_wblobc = &plot_style_stroke_wblobc_static;

/** stroke style for widget double dark colour. */
static plot_style_t plot_style_stroke_darkwbasec_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = double_darken_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_stroke_darkwbasec = &plot_style_stroke_darkwbasec_static;

/** stroke style for widget double light colour. */
static plot_style_t plot_style_stroke_lightwbasec_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = double_lighten_colour(WIDGET_BASEC),
};
plot_style_t *plot_style_stroke_lightwbasec = &plot_style_stroke_lightwbasec_static;

/* history styles */

/** stroke style for history core. */
static plot_style_t plot_style_stroke_history_static = {
	.stroke_type = PLOT_OP_TYPE_SOLID,
	.stroke_colour = HISTORY_COLOUR_LINES,
	.stroke_width = 2,
};
plot_style_t *plot_style_stroke_history = &plot_style_stroke_history_static;

/* Generic font style */
static const plot_font_style_t plot_style_font_static = {
	.family = PLOT_FONT_FAMILY_SANS_SERIF,
	.size = 8 * FONT_SIZE_SCALE,
	.weight = 400,
	.flags = FONTF_NONE,
	.background = 0xffffff,
	.foreground = 0x000000,
};
plot_font_style_t const * const plot_style_font = &plot_style_font_static;

