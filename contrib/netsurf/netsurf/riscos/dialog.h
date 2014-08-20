/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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

#ifndef _NETSURF_RISCOS_DIALOG_H_
#define _NETSURF_RISCOS_DIALOG_H_

#include <stdbool.h>
#include <stdlib.h>
#include "oslib/wimp.h"
#include "riscos/toolbar.h"
#include "riscos/gui.h"

void ro_gui_dialog_init(void);
wimp_w ro_gui_dialog_create(const char *template_name);
wimp_window * ro_gui_dialog_load_template(const char *template_name);

void ro_gui_dialog_open(wimp_w w);
void ro_gui_dialog_close(wimp_w close);

bool ro_gui_dialog_open_top(wimp_w w, struct toolbar *toolbar,
		int width, int height);
void ro_gui_dialog_open_at_pointer(wimp_w w);
void ro_gui_dialog_open_xy(wimp_w, int x, int y);
void ro_gui_dialog_open_centre_parent(wimp_w parent, wimp_w w);

void ro_gui_dialog_open_persistent(wimp_w parent, wimp_w w, bool pointer);
void ro_gui_dialog_add_persistent(wimp_w parent, wimp_w w);
void ro_gui_dialog_close_persistent(wimp_w parent);




void ro_gui_dialog_click(wimp_pointer *pointer);
void ro_gui_dialog_prepare_zoom(struct gui_window *g);
void ro_gui_dialog_update_zoom(struct gui_window *g);
void ro_gui_dialog_prepare_open_url(void);
void ro_gui_save_options(void);
void ro_gui_dialog_open_config(void);
void ro_gui_dialog_proxyauth_menu_selection(int item);
void ro_gui_dialog_image_menu_selection(int item);
void ro_gui_dialog_languages_menu_selection(const char *lang);
void ro_gui_dialog_font_menu_selection(int item);
#endif
