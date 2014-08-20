/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_HOTLIST_H
#define NS_ATARI_HOTLIST_H

#include <stdbool.h>

#include "desktop/tree.h"
#include "atari/gemtk/gemtk.h"
#include "atari/treeview.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/* The hotlist window, toolbar and treeview data. */

struct atari_hotlist {
	GUIWIN * window;
	//ATARI_TREEVIEW_PTR tv;/*< The hotlist treeview handle.  */
	struct core_window *tv;
	bool init;
	char path[PATH_MAX];
};

extern struct atari_hotlist hl;

void atari_hotlist_init( void );
void atari_hotlist_open( void );
void atari_hotlist_close( void );
void atari_hotlist_destroy( void );
void atari_hotlist_add_page( const char * url, const char * title );
void atari_hotlist_redraw( void );


#endif
