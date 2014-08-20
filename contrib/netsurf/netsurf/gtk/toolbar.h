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

#ifndef _NETSURF_GTK_TOOLBAR_H_
#define _NETSURF_GTK_TOOLBAR_H_

#include <gtk/gtk.h>

#include "gtk/scaffolding.h"

void nsgtk_toolbar_customization_init(nsgtk_scaffolding *g);
void nsgtk_toolbar_init(nsgtk_scaffolding *g);
void nsgtk_toolbar_customization_load(nsgtk_scaffolding *g);
void nsgtk_toolbar_set_physical(nsgtk_scaffolding *g);
void nsgtk_toolbar_connect_all(nsgtk_scaffolding *g);
int nsgtk_toolbar_get_id_from_widget(GtkWidget *widget, nsgtk_scaffolding
		*g);

#define TOOLPROTO(q) gboolean nsgtk_toolbar_##q##_button_data(\
		GtkWidget *widget, GdkDragContext *cont, GtkSelectionData\
		*selection, guint info, guint time, gpointer data);\
gboolean nsgtk_toolbar_##q##_toolbar_button_data(GtkWidget *widget,\
		GdkDragContext *cont, GtkSelectionData *selection, guint info,\
		guint time, gpointer data)
TOOLPROTO(home);
TOOLPROTO(back);
TOOLPROTO(forward);
TOOLPROTO(reload);
TOOLPROTO(stop);
TOOLPROTO(throbber);
TOOLPROTO(websearch);
TOOLPROTO(history);
TOOLPROTO(newwindow);
TOOLPROTO(newtab);
TOOLPROTO(openfile);
TOOLPROTO(closetab);
TOOLPROTO(closewindow);
TOOLPROTO(savepage);
TOOLPROTO(pdf);
TOOLPROTO(plaintext);
TOOLPROTO(drawfile);
TOOLPROTO(postscript);
TOOLPROTO(printpreview);
TOOLPROTO(print);
TOOLPROTO(quit);
TOOLPROTO(cut);
TOOLPROTO(copy);
TOOLPROTO(paste);
TOOLPROTO(delete);
TOOLPROTO(selectall);
TOOLPROTO(find);
TOOLPROTO(preferences);
TOOLPROTO(zoomplus);
TOOLPROTO(zoomminus);
TOOLPROTO(zoomnormal);
TOOLPROTO(fullscreen);
TOOLPROTO(viewsource);
TOOLPROTO(downloads);
TOOLPROTO(localhistory);
TOOLPROTO(globalhistory);
TOOLPROTO(addbookmarks);
TOOLPROTO(showbookmarks);
TOOLPROTO(showcookies);
TOOLPROTO(openlocation);
TOOLPROTO(nexttab);
TOOLPROTO(prevtab);
TOOLPROTO(savewindowsize);
TOOLPROTO(toggledebugging);
TOOLPROTO(saveboxtree);
TOOLPROTO(savedomtree);
TOOLPROTO(contents);
TOOLPROTO(guide);
TOOLPROTO(info);
TOOLPROTO(about);
#undef TOOLPROTO

#endif
