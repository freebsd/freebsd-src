/*
 * Copyright 2006 Adrian Lees <adrianl@users.sourceforge.net>
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
 * File/object/selection saving (Interface).
 */

#ifndef _NETSURF_RISCOS_SAVE_H_
#define _NETSURF_RISCOS_SAVE_H_

#include <stdbool.h>
#include "oslib/wimp.h"
#include "desktop/gui.h"

void gui_drag_save_object(struct gui_window *g, struct hlcache_handle *c, gui_save_type save_type);
void gui_drag_save_selection(struct gui_window *g, const char *selection);

wimp_w ro_gui_saveas_create(const char *template_name);
void ro_gui_saveas_quit(void);
void ro_gui_save_prepare(gui_save_type save_type, struct hlcache_handle *h,
			char *s, const char *url,
			const char *title);
void ro_gui_save_start_drag(wimp_pointer *pointer);
void ro_gui_drag_save_link(gui_save_type save_type, const char *url,
			const char *title, struct gui_window *g);
void ro_gui_drag_icon(int x, int y, const char *sprite);
void ro_gui_drag_box_cancel(void);
void ro_gui_send_datasave(gui_save_type save_type, wimp_full_message_data_xfer *message, wimp_t to);
void ro_gui_save_datasave_ack(wimp_message *message);
bool ro_gui_save_ok(wimp_w w);
void ro_gui_convert_save_path(char *dp, size_t len, const char *p);

#endif
