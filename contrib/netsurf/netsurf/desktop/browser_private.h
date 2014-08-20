/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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
 * Browser window private structure.
 */

#ifndef _NETSURF_DESKTOP_BROWSER_PRIVATE_H_
#define _NETSURF_DESKTOP_BROWSER_PRIVATE_H_

#include <stdbool.h>

#include "desktop/browser.h"


struct box;
struct hlcache_handle;
struct gui_window;
struct history;
struct selection;

/** Browser window data. */
struct browser_window {
	/** Page currently displayed, or 0. Must have status READY or DONE. */
	struct hlcache_handle *current_content;
	/** Page being loaded, or 0. */
	struct hlcache_handle *loading_content;

	/** Page Favicon */
	struct hlcache_handle *current_favicon;
	/** handle for favicon which we started loading early */
	struct hlcache_handle *loading_favicon;
	/** favicon fetch already failed - prevents infinite error looping */
	bool failed_favicon;

	/** Window history structure. */
	struct history *history;

	/** Platform specific window data. */
	struct gui_window *window;

	/** Busy indicator is active. */
	bool throbbing;
	/** Add loading_content to the window history when it loads. */
	bool history_add;

	/** Fragment identifier for current_content. */
	lwc_string *frag_id;

	/** Current drag status. */
	browser_drag_type drag_type;

	/** Current drag's browser window, when not in root bw. */
	struct browser_window *drag_window;

	/** Mouse position at start of current scroll drag. */
	int drag_start_x;
	int drag_start_y;
	/** Scroll offsets at start of current scroll draw. */
	int drag_start_scroll_x;
	int drag_start_scroll_y;
	/** Frame resize directions for current frame resize drag. */
	unsigned int drag_resize_left : 1;
	unsigned int drag_resize_right : 1;
	unsigned int drag_resize_up : 1;
	unsigned int drag_resize_down : 1;

	/** Current fetch is download */
	bool download;

	/** Refresh interval (-1 if undefined) */
	int refresh_interval;

	/** Window has been resized, and content needs reformatting. */
	bool reformat_pending;

	/** Window dimensions */
	int x;
	int y;
	int width;
	int height;

	struct scrollbar *scroll_x;  /**< Horizontal scroll. */
	struct scrollbar *scroll_y;  /**< Vertical scroll. */

	/** scale of window contents */
	float scale;

	/** Window characteristics */
	enum {
		BROWSER_WINDOW_NORMAL,
		BROWSER_WINDOW_IFRAME,
		BROWSER_WINDOW_FRAME,
		BROWSER_WINDOW_FRAMESET,
	} browser_window_type;

	/** frameset characteristics */
	int rows;
	int cols;

	/** frame dimensions */
	struct frame_dimension frame_width;
	struct frame_dimension frame_height;
	int margin_width;
	int margin_height;

	/** frame name for targetting */
	char *name;

	/** frame characteristics */
	bool no_resize;
	frame_scrolling scrolling;
	bool border;
	colour border_colour;

	/** iframe parent box */
	struct box *box;

	/** [cols * rows] children */
	struct browser_window *children;
	struct browser_window *parent;

	/** [iframe_count] iframes */
	int iframe_count;
	struct browser_window *iframes;

	/** browser window child of root browser window which has input focus */
	struct browser_window *focus;

	/** Last time a link was followed in this window */
	unsigned int last_action;

	/** Current selection */
	struct {
		struct browser_window *bw;
		bool read_only;
	} selection;
	bool can_edit;

	/** current javascript context */
	struct jscontext *jsctx;

	/** cache of the currently displayed status text. */
	char *status_text; /**< Current status bar text. */
	int status_text_len; /**< Length of the browser_window::status_text buffer. */
	int status_match; /**< Number of times an idempotent status-set operation was performed. */
	int status_miss; /**< Number of times status was really updated. */
};



nserror browser_window_initialise_common(enum browser_window_create_flags flags,
		struct browser_window *bw, struct browser_window *existing);

/**
 * Update the extent of the inside of a browser window to that of the current
 * content
 *
 * \param  bw	browser_window to update the extent of
 */
void browser_window_update_extent(struct browser_window *bw);

#endif
