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

#ifndef _NETSURF_MONKEY_OPTIONS_H_
#define _NETSURF_MONKEY_OPTIONS_H_

/* currently nothing here */

#endif

NSOPTION_BOOL(render_resample, true)
NSOPTION_BOOL(downloads_clear, false)
NSOPTION_BOOL(request_overwrite, true)
NSOPTION_STRING(downloads_directory, NULL)
NSOPTION_STRING(url_file, NULL)
NSOPTION_BOOL(show_single_tab, false)
NSOPTION_INTEGER(button_type, 0)
NSOPTION_BOOL(disable_popups, false)
NSOPTION_BOOL(disable_plugins, false)
NSOPTION_INTEGER(history_age, 0)
NSOPTION_BOOL(hover_urls, false)
NSOPTION_BOOL(focus_new, false)
NSOPTION_BOOL(new_blank, false)
NSOPTION_STRING(hotlist_path, NULL)
NSOPTION_BOOL(source_tab, false)
NSOPTION_INTEGER(current_theme, 0)


