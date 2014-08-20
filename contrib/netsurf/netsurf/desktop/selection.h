/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
  * Text selection within browser windows (interface).
  */

#ifndef _NETSURF_DESKTOP_SELECTION_H_
#define _NETSURF_DESKTOP_SELECTION_H_

#include <stdbool.h>
#include "desktop/mouse.h"

struct box;

typedef enum {
	DRAG_NONE,
	DRAG_START,
	DRAG_END
} seln_drag_state;


/* this structure should be treated as opaque outside selection.c
   (it's defined here to accelerate selection_defined(s) for reduced
   impact on redraw code) */

struct selection
{
	struct content *c;
	struct box *root;

	unsigned max_idx;  /* total bytes in text representation */

	unsigned start_idx;  /* offset in bytes within text representation */
	unsigned end_idx;

	bool defined;
	bool is_html;

	seln_drag_state drag_state;
};


struct selection *selection_create(struct content *c, bool is_html);
void selection_prepare(struct selection *s, struct content *c, bool is_html);
void selection_destroy(struct selection *s);

void selection_init(struct selection *s, struct box *root);
void selection_reinit(struct selection *s, struct box *root);

/* struct box *selection_root(struct selection *s); */
#define selection_root(s) ((s)->root)

/* bool selection_defined(struct selection *s); */
#define selection_defined(s) ((s)->defined)

/* bool selection_dragging(struct selection *s); */
#define selection_dragging(s) ((s)->drag_state != DRAG_NONE)

/* bool selection_dragging_start(struct selection *s); */
#define selection_dragging_start(s) ((s)->drag_state == DRAG_START)

void selection_clear(struct selection *s, bool redraw);
void selection_select_all(struct selection *s);

void selection_set_start(struct selection *s, unsigned idx);
void selection_set_end(struct selection *s, unsigned idx);

bool selection_click(struct selection *s, browser_mouse_state mouse,
		unsigned idx);
void selection_track(struct selection *s, browser_mouse_state mouse,
		unsigned idx);

bool selection_copy_to_clipboard(struct selection *s);
char * selection_get_copy(struct selection *s);

/** Handles completion of a drag operation */
/* void selection_drag_end(struct selection *s); */
#define selection_drag_end(s) ((s)->drag_state = DRAG_NONE)

bool selection_highlighted(const struct selection *s,
		unsigned start, unsigned end,
		unsigned *start_idx, unsigned *end_idx);

#endif
