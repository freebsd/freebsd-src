/*
 * Copyright 2011 Michael Drake <tlsa@netsurf-browser.org>
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
 * Core types.
 */

#ifndef _NETSURF_UTILS_TYPES_H_
#define _NETSURF_UTILS_TYPES_H_

#include <stdbool.h>

struct plotter_table;
struct hlcache_handle;

/* Rectangle coordinates */
struct rect {
	int x0, y0; /* Top left */
	int x1, y1; /* Bottom right */
};


/* Redraw context */
struct redraw_context {
	/** Redraw to show interactive features, such as active selections
	 *  etc.  Should be off for printing. */
	bool interactive;

	/** Render background images.  May want it off for printing. */
	bool background_images;

	/** Current plotters, must be assigned before use. */
	const struct plotter_table *plot;
};


/* Content located at a specific spatial location */
struct contextual_content {
	const char *link_url;
	struct hlcache_handle *object;
	struct hlcache_handle *main;
	enum {
		CTX_FORM_NONE,
		CTX_FORM_TEXT,
		CTX_FORM_FILE
	} form_features;
};

#endif
