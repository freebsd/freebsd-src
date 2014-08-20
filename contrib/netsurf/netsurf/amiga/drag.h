/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_DRAG_H
#define AMIGA_DRAG_H
#include "amiga/gui.h"

#define AMI_DRAG_THRESHOLD 10

int drag_save;
void *drag_save_data;
struct gui_window *drag_save_gui;

void gui_drag_save_selection(struct gui_window *g, const char *selection);
void gui_drag_save_object(struct gui_window *g, hlcache_handle *c, gui_save_type type);

void ami_drag_save(struct Window *win);
void ami_drag_icon_show(struct Window *win, const char *type);
void ami_drag_icon_close(struct Window *win);
void ami_drag_icon_move(void);
BOOL ami_drag_in_progress(void);

void *ami_window_at_pointer(int type);
#endif
