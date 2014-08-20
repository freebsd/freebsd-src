/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_PRINT_H
#define AMIGA_PRINT_H
#include <proto/exec.h>

struct content;

struct ami_print_window {
	struct nsObject *node;
	struct Window *win;
	Object *objects[OID_LAST];
	Object *gadgets[GID_LAST];
	struct hlcache_handle *c;
};

void ami_print(struct hlcache_handle *c, int copies);
void ami_print_ui(struct hlcache_handle *c);
BOOL ami_print_event(struct ami_print_window *pw);
bool ami_print_cont(void);
struct MsgPort *ami_print_init(void);
void ami_print_free(void);
struct MsgPort *ami_print_get_msgport(void);
#endif
