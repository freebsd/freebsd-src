/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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
 * RISC OS specific options.
 */

#ifndef _NETSURF_RISCOS_OPTIONS_H_
#define _NETSURF_RISCOS_OPTIONS_H_

#include "riscos/tinct.h"

/* setup longer default reflow time */
#define DEFAULT_REFLOW_PERIOD 100 /* time in cs */

#define CHOICES_PREFIX "<Choices$Write>.WWW.NetSurf."

#endif

NSOPTION_STRING(theme, "Aletheia")
NSOPTION_STRING(language, NULL)
NSOPTION_INTEGER(plot_fg_quality, tinct_ERROR_DIFFUSE)
NSOPTION_INTEGER(plot_bg_quality, tinct_DITHER)
NSOPTION_BOOL(history_tooltip, true)
NSOPTION_BOOL(toolbar_show_buttons, true)
NSOPTION_BOOL(toolbar_show_address, true)
NSOPTION_BOOL(toolbar_show_throbber, true)
NSOPTION_STRING(toolbar_browser, "0123|58|9")
NSOPTION_STRING(toolbar_hotlist, "40|12|3")
NSOPTION_STRING(toolbar_history, "0|12|3")
NSOPTION_STRING(toolbar_cookies, "0|12")
NSOPTION_BOOL(window_stagger, true)
NSOPTION_BOOL(window_size_clone, true)
NSOPTION_BOOL(buffer_animations, true)
NSOPTION_BOOL(buffer_everything, true)
NSOPTION_BOOL(open_browser_at_startup, false)
NSOPTION_BOOL(no_plugins, false)
NSOPTION_BOOL(block_popups, false)
NSOPTION_BOOL(strip_extensions, false)
NSOPTION_BOOL(confirm_overwrite, true)
NSOPTION_BOOL(confirm_hotlist_remove, true)
NSOPTION_STRING(url_path, "NetSurf:URL")
NSOPTION_STRING(url_save, CHOICES_PREFIX "URL")
NSOPTION_STRING(hotlist_path, "NetSurf:Hotlist")
NSOPTION_STRING(hotlist_save, CHOICES_PREFIX "Hotlist")
NSOPTION_STRING(recent_path, "NetSurf:Recent")
NSOPTION_STRING(recent_save, CHOICES_PREFIX "Recent")
NSOPTION_STRING(theme_path, "NetSurf:Themes")
NSOPTION_STRING(theme_save, CHOICES_PREFIX "Themes")
NSOPTION_BOOL(thumbnail_iconise, true)
NSOPTION_BOOL(interactive_help, true)
NSOPTION_BOOL(external_hotlists, false)
NSOPTION_STRING(external_hotlist_app, NULL)
