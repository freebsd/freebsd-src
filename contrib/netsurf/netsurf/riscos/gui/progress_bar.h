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
 * Progress bar (interface).
 */

#ifndef _NETSURF_RISCOS_PROGRESS_BAR_H_
#define _NETSURF_RISCOS_PROGRESS_BAR_H_

#include <stdbool.h>
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"

struct progress_bar;

void ro_gui_progress_bar_init(osspriteop_area *icons);

struct progress_bar *ro_gui_progress_bar_create(void);
void ro_gui_progress_bar_destroy(struct progress_bar *pb);
void ro_gui_progress_bar_update(struct progress_bar *pb, int width, int height);

wimp_w ro_gui_progress_bar_get_window(struct progress_bar *pb);
void ro_gui_progress_bar_set_icon(struct progress_bar *pb, const char *icon);
void ro_gui_progress_bar_set_value(struct progress_bar *pb, unsigned int value);
unsigned int ro_gui_progress_bar_get_value(struct progress_bar *pb);
void ro_gui_progress_bar_set_range(struct progress_bar *pb, unsigned int range);
unsigned int ro_gui_progress_bar_get_range(struct progress_bar *pb);
#endif
