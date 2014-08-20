/*
 * Copyright 2008-2009, 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_CLIPBOARD_H
#define AMIGA_CLIPBOARD_H
#include <stdbool.h>

struct bitmap;
struct hlcache_handle;
struct selection;
struct gui_window;
struct gui_window_2;
struct gui_clipboard_table;

extern struct gui_clipboard_table *amiga_clipboard_table;

void gui_start_selection(struct gui_window *g);

void ami_clipboard_init(void);
void ami_clipboard_free(void);
void ami_drag_selection(struct gui_window *g);
bool ami_easy_clipboard(char *text);
bool ami_easy_clipboard_bitmap(struct bitmap *bitmap);
#ifdef WITH_NS_SVG
bool ami_easy_clipboard_svg(struct hlcache_handle *c);
#endif
#endif
