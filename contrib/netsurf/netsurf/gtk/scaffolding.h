/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#ifndef NETSURF_GTK_SCAFFOLDING_H
#define NETSURF_GTK_SCAFFOLDING_H 1

#include <stdbool.h>
#include "utils/errors.h"

struct hlcache_handle;
struct gui_window;
typedef struct gtk_scaffolding nsgtk_scaffolding;

typedef enum {
	BACK_BUTTON = 0,
	HISTORY_BUTTON,
	FORWARD_BUTTON,
	STOP_BUTTON,
	RELOAD_BUTTON,
	HOME_BUTTON,
	URL_BAR_ITEM,
	WEBSEARCH_ITEM,
	THROBBER_ITEM,
	NEWWINDOW_BUTTON,
	NEWTAB_BUTTON,
	OPENFILE_BUTTON,
	CLOSETAB_BUTTON,
	CLOSEWINDOW_BUTTON,
	SAVEPAGE_BUTTON,
	PDF_BUTTON,
	PLAINTEXT_BUTTON,
	DRAWFILE_BUTTON,
	POSTSCRIPT_BUTTON,
	PRINTPREVIEW_BUTTON,
	PRINT_BUTTON,
	QUIT_BUTTON,
	CUT_BUTTON,
	COPY_BUTTON,
	PASTE_BUTTON,
	DELETE_BUTTON,
	SELECTALL_BUTTON,
	FIND_BUTTON,
	PREFERENCES_BUTTON,
	ZOOMPLUS_BUTTON,
	ZOOMMINUS_BUTTON,
	ZOOMNORMAL_BUTTON,
	FULLSCREEN_BUTTON,
	VIEWSOURCE_BUTTON,
	DOWNLOADS_BUTTON,
	SAVEWINDOWSIZE_BUTTON,
	TOGGLEDEBUGGING_BUTTON,
	SAVEBOXTREE_BUTTON,
	SAVEDOMTREE_BUTTON,
	LOCALHISTORY_BUTTON,
	GLOBALHISTORY_BUTTON,
	ADDBOOKMARKS_BUTTON,
	SHOWBOOKMARKS_BUTTON,
 	SHOWCOOKIES_BUTTON,
	OPENLOCATION_BUTTON,
	NEXTTAB_BUTTON,
	PREVTAB_BUTTON,
	CONTENTS_BUTTON,
	GUIDE_BUTTON,
	INFO_BUTTON,
	ABOUT_BUTTON,
	PLACEHOLDER_BUTTON /* size indicator; array maximum indices */
} nsgtk_toolbar_button;    /* PLACEHOLDER_BUTTON - 1 */

struct gtk_history_window {
	struct gtk_scaffolding 	*g;
	GtkWindow		*window;
	GtkScrolledWindow	*scrolled;
	GtkDrawingArea		*drawing_area;
};

struct gtk_search {
	GtkToolbar			*bar;
	GtkEntry			*entry;
	GtkToolButton			*buttons[3]; /* back, forward, */
	GtkCheckButton			*checkAll;	/* close */
	GtkCheckButton			*caseSens;
};

struct nsgtk_button_connect {
	GtkToolItem			*button;
	int				location; /* in toolbar */
	bool				sensitivity;
	GtkImageMenuItem		*main;
	GtkImageMenuItem		*rclick;
	GtkImageMenuItem		*popup;
	void				*mhandler; /* menu item clicked */
	void				*bhandler; /* button clicked */
	void				*dataplus; /* customization -> toolbar */
	void				*dataminus; /* customization -> store */
};

extern nsgtk_scaffolding *scaf_list;

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel);

bool nsgtk_scaffolding_is_busy(nsgtk_scaffolding *g);

GtkWindow *nsgtk_scaffolding_window(nsgtk_scaffolding *g);
GtkNotebook *nsgtk_scaffolding_notebook(nsgtk_scaffolding *g);
GtkWidget *nsgtk_scaffolding_urlbar(nsgtk_scaffolding *g);
GtkWidget *nsgtk_scaffolding_websearch(nsgtk_scaffolding *g);
GtkToolbar *nsgtk_scaffolding_toolbar(nsgtk_scaffolding *g);
struct nsgtk_button_connect *nsgtk_scaffolding_button(nsgtk_scaffolding *g,
		int i);
struct gtk_search *nsgtk_scaffolding_search(nsgtk_scaffolding *g);
GtkMenuBar *nsgtk_scaffolding_menu_bar(nsgtk_scaffolding *g);
struct gtk_history_window *nsgtk_scaffolding_history_window(nsgtk_scaffolding
		*g);
struct gui_window *nsgtk_scaffolding_top_level(nsgtk_scaffolding *g);
void nsgtk_scaffolding_reset_offset(nsgtk_scaffolding *g);
nsgtk_scaffolding *nsgtk_scaffolding_iterate(nsgtk_scaffolding *g);
void nsgtk_scaffolding_toolbar_init(struct gtk_scaffolding *g);
void nsgtk_scaffolding_update_url_bar_ref(nsgtk_scaffolding *g);
void nsgtk_scaffolding_update_throbber_ref(nsgtk_scaffolding *g);
void nsgtk_scaffolding_update_websearch_ref(nsgtk_scaffolding *g);
void nsgtk_scaffolding_set_websearch(nsgtk_scaffolding *g, const char
		*content);
void nsgtk_scaffolding_toggle_search_bar_visibility(nsgtk_scaffolding *g);
void nsgtk_scaffolding_set_top_level(struct gui_window *g);

void nsgtk_scaffolding_destroy(nsgtk_scaffolding *g);

/** update the sensitivity of context sensitive UI elements
 *
 * widgets altered in arrays:
 *   main
 *   right click menu
 *   location
 *   popup
 * current arrays are:
 *   stop
 *   reload
 *   cut
 *   copy
 *   paste
 *   back
 *   forward
 *   nexttab
 *   prevtab
 *   closetab
 */
void nsgtk_scaffolding_set_sensitivity(struct gtk_scaffolding *g);

void nsgtk_scaffolding_initial_sensitivity(struct gtk_scaffolding *g);
void nsgtk_scaffolding_popup_menu(struct gtk_scaffolding *g, gdouble x,
    gdouble y);
void nsgtk_scaffolding_toolbar_size_allocate(GtkWidget *widget,
		GtkAllocation *alloc, gpointer data);
void nsgtk_scaffolding_set_icon(struct gui_window *gw);

gboolean nsgtk_window_url_activate_event(GtkWidget *, gpointer);
gboolean nsgtk_window_url_changed(GtkWidget *, GdkEventKey *, gpointer);

nserror nsgtk_scaffolding_new_tab(struct gui_window *gw);

/* core acessors */
void gui_window_set_title(struct gui_window *g, const char *title);
void gui_window_set_url(struct gui_window *g, const char *url);
void gui_window_start_throbber(struct gui_window *g);
void gui_window_stop_throbber(struct gui_window *g);
void gui_set_search_ico(struct hlcache_handle *ico);

#endif /* NETSURF_GTK_SCAFFOLDING_H */
