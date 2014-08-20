/*
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
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
 * Core window handling (interface).
 */

#ifndef _NETSURF_DESKTOP_CORE_WINDOW_H_
#define _NETSURF_DESKTOP_CORE_WINDOW_H_

#include "utils/types.h"

struct core_window;

typedef enum {
	CORE_WINDOW_DRAG_NONE,
	CORE_WINDOW_DRAG_SELECTION,
	CORE_WINDOW_DRAG_TEXT_SELECTION,
	CORE_WINDOW_DRAG_MOVE
} core_window_drag_status;

/** Callbacks to achieve various core window functionality. */
struct core_window_callback_table {
	/**
	 * Request a redraw of the window
	 *
	 * \param cw		the core window object
	 * \param r		rectangle to redraw
	 */
	void (*redraw_request)(struct core_window *cw, const struct rect *r);

	/**
	 * Update the limits of the window
	 *
	 * \param cw		the core window object
	 * \param width		the width in px, or negative if don't care
	 * \param height	the height in px, or negative if don't care
	 */
	void (*update_size)(struct core_window *cw, int width, int height);

	/**
	 * Scroll the window to make area visible
	 *
	 * \param cw		the core window object
	 * \param r		rectangle to make visible
	 */
	void (*scroll_visible)(struct core_window *cw, const struct rect *r);

	/**
	 * Get window viewport dimensions
	 *
	 * \param cw		the core window object
	 * \param width		to be set to viewport width in px, if non NULL
	 * \param height	to be set to viewport height in px, if non NULL
	 */
	void (*get_window_dimensions)(struct core_window *cw,
			int *width, int *height);

	/**
	 * Inform corewindow owner of drag status
	 *
	 * \param cw		the core window object
	 * \param ds		the current drag status
	 */
	void (*drag_status)(struct core_window *cw,
			core_window_drag_status ds);
};


#endif
