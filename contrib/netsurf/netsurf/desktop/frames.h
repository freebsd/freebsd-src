/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Frame and frameset creation and manipulation (interface).
 */

#ifndef _NETSURF_DESKTOP_FRAMES_H_
#define _NETSURF_DESKTOP_FRAMES_H_

#include "desktop/browser.h"

struct scrollbar_msg_data;

void browser_window_create_iframes(struct browser_window *bw,
		struct content_html_iframe *iframe);
void browser_window_recalculate_iframes(struct browser_window *bw);
void browser_window_create_frameset(struct browser_window *bw,
		struct content_html_frames *frameset);
void browser_window_recalculate_frameset(struct browser_window *bw);
bool browser_window_frame_resize_start(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y,
		browser_pointer_shape *pointer);
void browser_window_resize_frame(struct browser_window *bw, int x, int y);

void browser_window_scroll_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data);


/**
 * Create, remove, and update browser window scrollbars
 *
 * \param  bw    The browser window
 */
void browser_window_handle_scrollbars(struct browser_window *bw);

#endif
