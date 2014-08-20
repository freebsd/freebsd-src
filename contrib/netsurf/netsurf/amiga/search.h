/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_SEARCH_H
#define AMIGA_SEARCH_H

#include "amiga/gui.h"

struct find_window {
	struct nsObject *node;
	struct Window *win;
	Object *objects[GID_LAST];
	struct gui_window *gwin;
};

struct gui_search_table *amiga_search_table;

void ami_search_open(struct gui_window *gwin);
BOOL ami_search_event(void);
void ami_search_close(void);

char *search_engines_file_location;
char *search_default_ico_location;
#endif
