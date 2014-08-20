/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Global history (interface).
 */

#ifndef _NETSURF_RISCOS_GLOBALHISTORY_H_
#define _NETSURF_RISCOS_GLOBALHISTORY_H_

#include "riscos/menus.h"

void ro_gui_global_history_preinitialise(void);
void ro_gui_global_history_postinitialise(void);
void ro_gui_global_history_destroy(void);
void ro_gui_global_history_open(void);
void ro_gui_global_history_save(void);
bool ro_gui_global_history_check_window(wimp_w window);
bool ro_gui_global_history_check_menu(wimp_menu *menu);

#endif

