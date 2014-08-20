/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net> 
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
 * Generic tree handling.
 */

#ifndef __NSGTK_TREEVIEW_H__
#define __NSGTK_TREEVIEW_H__

struct nsgtk_treeview;

struct nsgtk_treeview *nsgtk_treeview_create(unsigned int flags,
		GtkWindow *window, GtkScrolledWindow *scrolled,
 		GtkDrawingArea *drawing_area);
void nsgtk_treeview_destroy(struct nsgtk_treeview *tv);

struct tree *nsgtk_treeview_get_tree(struct nsgtk_treeview *tv);

void nsgtk_tree_window_hide(GtkWidget *widget, gpointer g);

#endif /*__NSGTK_TREEVIEW_H__*/
