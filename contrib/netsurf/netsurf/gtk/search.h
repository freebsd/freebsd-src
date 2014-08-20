/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef _NETSURF_GTK_SEARCH_H_
#define _NETSURF_GTK_SEARCH_H_

struct gui_search_table *nsgtk_search_table;

struct gtk_scaffolding;

void nsgtk_search_bar_toggle_visibility(struct gtk_scaffolding * g);
gboolean nsgtk_search_entry_changed(GtkWidget *widget, gpointer data);
gboolean nsgtk_search_entry_activate(GtkWidget *widget, gpointer data);
gboolean nsgtk_search_entry_key(GtkWidget *widget, GdkEventKey *event, gpointer data);
gboolean nsgtk_search_forward_button_clicked(GtkWidget *widget, gpointer data);
gboolean nsgtk_search_back_button_clicked(GtkWidget *widget, gpointer data);
gboolean nsgtk_search_close_button_clicked(GtkWidget *widget, gpointer data);
gboolean nsgtk_websearch_activate(GtkWidget *widget, gpointer data);
gboolean nsgtk_websearch_clear(GtkWidget *widget, GdkEventFocus *f, gpointer data);
		
#endif
