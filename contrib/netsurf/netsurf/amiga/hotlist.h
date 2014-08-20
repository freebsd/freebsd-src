/*
 * Copyright 2008, 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_HOTLIST_H
#define AMIGA_HOTLIST_H
#include "desktop/tree.h"
#include "amiga/tree.h"

void ami_hotlist_initialise(const char *hotlist_file);
void ami_hotlist_free(const char *hotlist_file);
nserror ami_hotlist_scan(void *userdata, int first_item, const char *folder,
	bool (*cb_add_item)(void *userdata, int level, int item, const char *title, nsurl *url, bool folder));

struct treeview_window *hotlist_window;
#endif
