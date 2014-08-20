/*
 * Copyright 2010, 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Browser window handling (interface).
 */

#include <stdbool.h>

#ifndef _NETSURF_RISCOS_WINDOW_H_
#define _NETSURF_RISCOS_WINDOW_H_

extern struct gui_window_table *riscos_window_table;

void ro_gui_window_initialise(void);

bool ro_gui_window_check_menu(wimp_menu *menu);

void gui_window_set_url(struct gui_window *g, const char *url);

#endif

