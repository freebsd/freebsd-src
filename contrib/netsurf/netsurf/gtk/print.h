/*
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
 * GTK printing (interface).
 */

#ifndef NETSURF_GTK_PRINT_PLOTTERS_H
#define NETSURF_GTK_PRINT_PLOTTERS_H


#include <gtk/gtk.h>

struct hlcache_handle;

extern cairo_t *gtk_print_current_cr;

extern struct hlcache_handle *content_to_print;


/*handlers for signals from the GTK print operation*/
void gtk_print_signal_begin_print(GtkPrintOperation *operation,
		GtkPrintContext *context,
  		gpointer user_data);
  		
void gtk_print_signal_draw_page(GtkPrintOperation *operation,
		GtkPrintContext *context,
  		gint page_nr,
  		gpointer user_data);
  			
void gtk_print_signal_end_print(GtkPrintOperation *operation,
		GtkPrintContext *context,
  		gpointer user_data);

#endif
