/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * UTF8 status bar (interface).
 */

#ifndef _NETSURF_RISCOS_STATUS_BAR_H_
#define _NETSURF_RISCOS_STATUS_BAR_H_

#include <stdbool.h>

struct status_bar;

struct status_bar *ro_gui_status_bar_create(wimp_w parent, unsigned int width);
void ro_gui_status_bar_destroy(struct status_bar *sb);

wimp_w ro_gui_status_bar_get_window(struct status_bar *sb);
unsigned int ro_gui_status_bar_get_width(struct status_bar *sb);
void ro_gui_status_bar_resize(struct status_bar *sb);
void ro_gui_status_bar_set_visible(struct status_bar *pb, bool visible);
bool ro_gui_status_bar_get_visible(struct status_bar *sb);
void ro_gui_status_bar_set_text(struct status_bar *sb, const char *text);
void ro_gui_status_bar_set_progress_value(struct status_bar *sb,
		unsigned int value);
void ro_gui_status_bar_set_progress_range(struct status_bar *sb,
		unsigned int range);
void ro_gui_status_bar_set_progress_icon(struct status_bar *sb,
		const char *icon);
#endif
