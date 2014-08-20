/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2010, 2013 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Hotlist (interface).
 */

#ifndef _NETSURF_RISCOS_HOTLIST_H_
#define _NETSURF_RISCOS_HOTLIST_H_

/* Hotlist Protocol Messages, which are currently not in OSLib. */

#ifndef message_HOTLIST_ADD_URL
#define message_HOTLIST_ADD_URL 0x4af81
#endif

#ifndef message_HOTLIST_CHANGED
#define message_HOTLIST_CHANGED 0x4af82
#endif

#include "riscos/menus.h"

struct nsurl;

void ro_gui_hotlist_preinitialise(void);
void ro_gui_hotlist_postinitialise(void);
void ro_gui_hotlist_destroy(void);
void ro_gui_hotlist_open(void);
void ro_gui_hotlist_save(void);
bool ro_gui_hotlist_check_window(wimp_w window);
bool ro_gui_hotlist_check_menu(wimp_menu *menu);
void ro_gui_hotlist_add_page(nsurl *url);
void ro_gui_hotlist_add_cleanup(void);
void ro_gui_hotlist_remove_page(nsurl *url);
bool ro_gui_hotlist_has_page(nsurl *url);

#endif
