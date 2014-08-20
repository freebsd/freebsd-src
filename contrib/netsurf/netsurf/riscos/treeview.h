/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Generic tree handling (interface).
 */

#ifndef _NETSURF_RISCOS_TREEVIEW_H_
#define _NETSURF_RISCOS_TREEVIEW_H_

#include <stdbool.h>
#include <oslib/help.h>
#include <oslib/wimp.h>

#include "desktop/tree.h"
#include "riscos/toolbar.h"

typedef struct ro_treeview ro_treeview;

struct ro_treeview_callbacks {
	void (*toolbar_button_click)(button_bar_action action);
	void (*toolbar_button_update)(void);
	void (*toolbar_button_save)(char *);
};

ro_treeview *ro_treeview_create(wimp_w window, struct toolbar *toolbar,
		struct ro_treeview_callbacks *callbacks, unsigned int flags);
void ro_treeview_destroy(ro_treeview *tv);
const struct toolbar_callbacks *ro_treeview_get_toolbar_callbacks(void);

struct tree *ro_treeview_get_tree(ro_treeview *tv);
wimp_w ro_treeview_get_window(ro_treeview *tv);

void ro_treeview_set_origin(ro_treeview *tv, int x, int y);
void ro_treeview_mouse_at(wimp_pointer *pointer, void *data);
int ro_treeview_get_help(help_full_message_request *message_data);

#endif

