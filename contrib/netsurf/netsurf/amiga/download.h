/*
 * Copyright 2008-9 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_DOWNLOAD_H
#define AMIGA_DOWNLOAD_H

#include "amiga/os3support.h"

#include "amiga/gui.h"

extern struct gui_download_table *amiga_download_table;

struct download_context;
struct gui_download_window;

struct dlnode
{
	struct Node node;
	char *filename;
};

void ami_download_window_abort(struct gui_download_window *dw);
BOOL ami_download_window_event(struct gui_download_window *dw);
void ami_free_download_list(struct List *dllist);
BOOL ami_download_check_overwrite(const char *file, struct Window *win, ULONG size);

void gui_window_save_link(struct gui_window *g, const char *url, const char *title);

#endif
