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

#ifndef AMIGA_TREE_H
#define AMIGA_TREE_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include "amiga/os3support.h"
#include "amiga/gui.h"
#include "desktop/tree.h"

struct treeview_window;

enum
{
	AMI_TREE_HOTLIST,
	AMI_TREE_HISTORY,
	AMI_TREE_COOKIES,
	AMI_TREE_SSLCERT
};

struct treeview_window *ami_tree_create(int flags,
			struct sslcert_session_data *ssl_data);
void ami_tree_destroy(struct treeview_window *twin);
struct tree *ami_tree_get_tree(struct treeview_window *twin);

void ami_tree_open(struct treeview_window *twin,int type);
void ami_tree_close(struct treeview_window *twin);
BOOL ami_tree_event(struct treeview_window *twin);

extern const struct treeview_table ami_tree_callbacks;

#endif
