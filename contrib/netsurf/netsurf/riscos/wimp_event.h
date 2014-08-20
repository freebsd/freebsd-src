/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Automated RISC OS WIMP event handling (interface).
 */


#ifndef _NETSURF_RISCOS_WIMP_EVENT_H_
#define _NETSURF_RISCOS_WIMP_EVENT_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "riscos/menus.h"

#define IS_WIMP_KEY (1u<<31)

bool ro_gui_wimp_event_memorise(wimp_w w);
bool ro_gui_wimp_event_restore(wimp_w w);
bool ro_gui_wimp_event_validate(wimp_w w);
bool ro_gui_wimp_event_transfer(wimp_w from, wimp_w to);
void ro_gui_wimp_event_finalise(wimp_w w);
void ro_gui_wimp_event_deregister(wimp_w w, wimp_i i);

bool ro_gui_wimp_event_set_help_prefix(wimp_w w, const char *help_prefix);
const char *ro_gui_wimp_event_get_help_prefix(wimp_w w);
bool ro_gui_wimp_event_register_help_suffix(wimp_w w,
		const char *(*get_help_suffix)(wimp_w w, wimp_i i,
			os_coord *pos, wimp_mouse_state buttons));
const char *ro_gui_wimp_event_get_help_suffix(wimp_w w, wimp_i i,
		os_coord *pos, wimp_mouse_state buttons);
bool ro_gui_wimp_event_set_user_data(wimp_w w, void *user);
void *ro_gui_wimp_event_get_user_data(wimp_w w);

bool ro_gui_wimp_event_menu_selection(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
bool ro_gui_wimp_event_mouse_click(wimp_pointer *pointer);
bool ro_gui_wimp_event_keypress(wimp_key *key);
bool ro_gui_wimp_event_open_window(wimp_open *open);
bool ro_gui_wimp_event_close_window(wimp_w w);
bool ro_gui_wimp_event_redraw_window(wimp_draw *redraw);
bool ro_gui_wimp_event_scroll_window(wimp_scroll *scroll);
bool ro_gui_wimp_event_pointer_entering_window(wimp_entering *entering);

bool ro_gui_wimp_event_process_window_menu_click(wimp_pointer *pointer);
bool ro_gui_wimp_event_prepare_menu(wimp_w w, wimp_i i, wimp_menu *menu);

bool ro_gui_wimp_event_register_menu(wimp_w w, wimp_menu *m,
		bool menu_auto, bool position_ibar);
bool ro_gui_wimp_event_register_numeric_field(wimp_w w, wimp_i i, wimp_i up,
		wimp_i down, int min, int max, int stepping,
		int decimal_places);
bool ro_gui_wimp_event_register_text_field(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_menu_gright(wimp_w w, wimp_i i,
		wimp_i gright, wimp_menu *menu);
bool ro_gui_wimp_event_register_checkbox(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_radio(wimp_w w, wimp_i *i);
bool ro_gui_wimp_event_register_button(wimp_w w, wimp_i i,
		void (*callback)(wimp_pointer *pointer));
bool ro_gui_wimp_event_register_cancel(wimp_w w, wimp_i i);
bool ro_gui_wimp_event_register_ok(wimp_w w, wimp_i i,
		bool (*callback)(wimp_w w));

bool ro_gui_wimp_event_register_mouse_click(wimp_w w,
		bool (*callback)(wimp_pointer *pointer));
bool ro_gui_wimp_event_register_keypress(wimp_w w,
		bool (*callback)(wimp_key *key));
bool ro_gui_wimp_event_register_open_window(wimp_w w,
		void (*callback)(wimp_open *open));
bool ro_gui_wimp_event_register_close_window(wimp_w w,
		void (*callback)(wimp_w w));
bool ro_gui_wimp_event_register_redraw_window(wimp_w w,
		void (*callback)(wimp_draw *redraw));
bool ro_gui_wimp_event_register_scroll_window(wimp_w w,
		void (*callback)(wimp_scroll *scroll));
bool ro_gui_wimp_event_register_pointer_entering_window(wimp_w w,
		void (*callback)(wimp_entering *entering));
bool ro_gui_wimp_event_register_menu_prepare(wimp_w w,
		bool (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_pointer *p));
bool ro_gui_wimp_event_register_menu_selection(wimp_w w,
		bool (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a));
bool ro_gui_wimp_event_register_menu_warning(wimp_w w,
		void (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
		wimp_selection *s, menu_action a));
bool ro_gui_wimp_event_register_menu_close(wimp_w w,
		void (*callback)(wimp_w w, wimp_i i, wimp_menu *m));

bool ro_gui_wimp_event_submenu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
void ro_gui_wimp_event_menus_closed(wimp_w w, wimp_i i, wimp_menu *menu);
void ro_gui_wimp_event_register_submenu(wimp_w w);

#endif
