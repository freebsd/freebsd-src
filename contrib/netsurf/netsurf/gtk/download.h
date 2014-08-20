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

#ifndef GTK_DOWNLOAD_H
#define GTK_DOWNLOAD_H

#include <gtk/gtk.h>

struct gui_download_table *nsgtk_download_table;

bool nsgtk_download_init(const char *glade_file_location);
void nsgtk_download_destroy (void);
bool nsgtk_check_for_downloads(GtkWindow *parent);
void nsgtk_download_show(GtkWindow *parent);
void nsgtk_download_add(gchar *url, gchar *destination);

#endif
