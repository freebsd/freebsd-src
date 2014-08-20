/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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
 * deprecated compatibility layer for new treeview modules.  Do not use.
 */

#ifndef _NETSURF_DESKTOP_TREE_H_
#define _NETSURF_DESKTOP_TREE_H_

#include <stdbool.h>
#include <stdint.h>

#include "desktop/mouse.h"

struct sslcert_session_data;
struct tree;
struct redraw_context;

extern struct sslcert_session_data *ssl_current_session;
extern const char *tree_hotlist_path;

/* Tree flags */
enum tree_flags {
	TREE_HISTORY,
	TREE_COOKIES,
	TREE_SSLCERT,
	TREE_HOTLIST
};

typedef enum {
	TREE_NO_DRAG = 0,
	TREE_SELECT_DRAG,
	TREE_MOVE_DRAG,
	TREE_TEXTAREA_DRAG,	/** < A drag that is passed to a textarea */
	TREE_UNKNOWN_DRAG	/** < A drag the tree itself won't handle */
} tree_drag_type;

/** callbacks to perform necessary operations on treeview. */
struct treeview_table {
	void (*redraw_request)(int x, int y, int width, int height,
			       void *data); /**< request a redraw. */
	void (*resized)(struct tree *tree, int width, int height,
			void *data); /**< resize treeview area. */
	void (*scroll_visible)(int y, int height, void *data); /**< scroll visible treeview area. */
	void (*get_window_dimensions)(int *width, int *height, void *data); /**< get dimensions of window */
};

struct tree *tree_create(unsigned int flags,
		const struct treeview_table *callbacks,
  		void *client_data);

/** deprecated compatibility layer for new treeview modules.  Do not use. */
void tree_delete(struct tree *tree);
tree_drag_type tree_drag_status(struct tree *tree);
void tree_draw(struct tree *tree, int x, int y,
		int clip_x, int clip_y, int clip_width, int clip_height,
		const struct redraw_context *ctx);
bool tree_mouse_action(struct tree *tree, browser_mouse_state mouse,
		int x, int y);
void tree_drag_end(struct tree *tree, browser_mouse_state mouse, int x0, int y0,
		int x1, int y1);
bool tree_keypress(struct tree *tree, uint32_t key);


#endif
