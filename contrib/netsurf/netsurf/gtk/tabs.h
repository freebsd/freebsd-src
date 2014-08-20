/*
 * Copyright 2008 Michael Lester <element3260@gmail.com>
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

#ifndef _NETSURF_GTK_TABS_H_
#define _NETSURF_GTK_TABS_H_

struct gui_window;

void nsgtk_tab_init(struct gtk_scaffolding *gs);
void nsgtk_tab_add(struct gui_window *window, GtkWidget *tab_contents, bool background);

/** set the tab title
 *
 * The tab title will be set to the parameter
 *
 * @note currently only called from gui_window_set_title()
 * @param g the gui window to set tab title for.
 * @param title The title text which may not be NULL.
 */
void nsgtk_tab_set_title(struct gui_window *g, const char *title);
void nsgtk_tab_options_changed(GtkNotebook *notebook);
nserror nsgtk_tab_close_current(GtkNotebook *notebook);
nserror nsgtk_tab_prev(GtkNotebook *notebook);
nserror nsgtk_tab_next(GtkNotebook *notebook);

#endif
