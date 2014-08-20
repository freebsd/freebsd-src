/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
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
 * General RISC OS WIMP/OS library functions (interface).
 */


#ifndef _NETSURF_RISCOS_WIMP_H_
#define _NETSURF_RISCOS_WIMP_H_

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "rufl.h"


int ro_get_hscroll_height(wimp_w w);
int ro_get_vscroll_width(wimp_w w);
int ro_get_title_height(wimp_w w);
bool ro_gui_wimp_read_eig_factors(os_mode mode, int *xeig, int *yeig);
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode);
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode);

#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)
void ro_gui_force_redraw_icon(wimp_w w, wimp_i i);
const char *ro_gui_get_icon_string(wimp_w w, wimp_i i);
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text, bool is_utf8);

void ro_gui_set_icon_integer(wimp_w w, wimp_i i, int value);
void ro_gui_set_icon_decimal(wimp_w w, wimp_i i, int value, int decimal_places);
int ro_gui_get_icon_decimal(wimp_w w, wimp_i i, int decimal_places);

void ro_gui_set_icon_selected_state(wimp_w w, wimp_i i, bool state);
bool ro_gui_get_icon_selected_state(wimp_w w, wimp_i i);
void ro_gui_set_icon_shaded_state(wimp_w w, wimp_i i, bool state);
bool ro_gui_get_icon_shaded_state(wimp_w w, wimp_i i);
void ro_gui_set_icon_deleted_state(wimp_w w, wimp_i i, bool state);
bool ro_gui_get_icon_deleted_state(wimp_w w, wimp_i i);
void ro_gui_set_icon_button_type(wimp_w w, wimp_i i, int type);
void ro_gui_set_icon_sprite(wimp_w w, wimp_i i, osspriteop_area *area,
		const char *name);
void ro_gui_set_window_title(wimp_w w, const char *title);
bool ro_gui_set_caret_first(wimp_w w);
void ro_gui_open_window_centre(wimp_w parent, wimp_w child);

osspriteop_area *ro_gui_load_sprite_file(const char *pathname);
bool ro_gui_wimp_sprite_exists(const char *sprite);
os_error *ro_gui_wimp_get_sprite(const char *name, osspriteop_header **sprite);
bool ro_gui_wimp_get_sprite_dimensions(osspriteop_area *area, char *sprite,
		int *width, int *height);

wimp_w ro_gui_set_window_background_colour(wimp_w window, wimp_colour background);
void ro_gui_set_icon_colours(wimp_w window, wimp_i icon,
		wimp_colour foreground, wimp_colour background);
void ro_gui_user_redraw(wimp_draw *redraw, bool user_fill, os_colour user_colour);
void ro_gui_wimp_update_window_furniture(wimp_w w, wimp_window_flags bic_mask,
		wimp_window_flags xor_mask);
bool ro_gui_wimp_check_window_furniture(wimp_w w, wimp_window_flags mask);

void ro_gui_scroll(wimp_scroll *scroll);

#endif

