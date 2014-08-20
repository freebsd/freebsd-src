/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/gui.h"

struct hlcache_object;
struct selection;

struct FileRequester *filereq;
struct FileRequester *savereq;

enum {
	AMINS_SAVE_SOURCE,
	AMINS_SAVE_TEXT,
	AMINS_SAVE_COMPLETE,
	AMINS_SAVE_PDF,
	AMINS_SAVE_IFF,
	AMINS_SAVE_SELECTION,
};


void ami_file_req_init(void);
void ami_file_req_free(void);

void ami_file_open(struct gui_window_2 *gwin);
void ami_file_save_req(int type, struct gui_window_2 *gwin,
		struct hlcache_handle *object);
void ami_file_save(int type, char *fname, struct Window *win,
		struct hlcache_handle *object, struct hlcache_handle *favicon,
		struct browser_window *bw);
