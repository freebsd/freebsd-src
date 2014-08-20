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

/** \file
 * Automated RISC OS WIMP event handling (interface).
 */


#ifndef _NETSURF_RISCOS_OPTIONS_CONFIGURE_H_
#define _NETSURF_RISCOS_OPTIONS_CONFIGURE_H_

#include <stdbool.h>
#include "oslib/wimp.h"

bool ro_gui_options_cache_initialise(wimp_w w);
bool ro_gui_options_connection_initialise(wimp_w w);
bool ro_gui_options_content_initialise(wimp_w w);
bool ro_gui_options_fonts_initialise(wimp_w w);
bool ro_gui_options_home_initialise(wimp_w w);
bool ro_gui_options_image_initialise(wimp_w w);
void ro_gui_options_image_finalise(wimp_w w);
bool ro_gui_options_interface_initialise(wimp_w w);
bool ro_gui_options_language_initialise(wimp_w w);
bool ro_gui_options_security_initialise(wimp_w w);
bool ro_gui_options_theme_initialise(wimp_w w);
void ro_gui_options_theme_finalise(wimp_w w);

#endif
