/*
 * Copyright 2011 Vincent Sanders <vince@netsurf-browser.org>
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
 * GDK support functions for missing interfaces
 */

#ifndef NETSURF_GTK_GDK_H_
#define NETSURF_GTK_GDK_H_

#include <gtk/gtk.h>

/** obtain a pixbuf of the specified size from a cairo surface.
 *
 * This is the same as the GTK+ 3 gdk_pixbuf_get_from_surface but
 * actually works and is available on gtk 2 
 */
GdkPixbuf *nsgdk_pixbuf_get_from_surface(cairo_surface_t *surface, int width, int height);

#endif /* NETSURF_GTK_GDK_H */
