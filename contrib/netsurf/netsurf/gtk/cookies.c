/*
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
 * Cookies (implementation).
 */


#include "desktop/cookie_manager.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "desktop/textinput.h"
#include "utils/log.h"
#include "gtk/gui.h"
#include "gtk/cookies.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"

#define GLADE_NAME "cookies.glade"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_cookies_init_menu(void);

/* edit menu */
MENUPROTO(delete_selected);
MENUPROTO(delete_all);
MENUPROTO(select_all);
MENUPROTO(clear_selection);

/* view menu*/
MENUPROTO(expand_all);
MENUPROTO(expand_domains);
MENUPROTO(expand_cookies);
MENUPROTO(collapse_all);
MENUPROTO(collapse_domains);
MENUPROTO(collapse_cookies);


static struct menu_events menu_events[] = {
	
	/* edit menu */
	MENUEVENT(delete_selected),
	MENUEVENT(delete_all),
	MENUEVENT(select_all),
	MENUEVENT(clear_selection),
	
	/* view menu*/
	MENUEVENT(expand_all),
	MENUEVENT(expand_domains),
	MENUEVENT(expand_cookies),		  
	MENUEVENT(collapse_all),
	MENUEVENT(collapse_domains),
	MENUEVENT(collapse_cookies),
		  
	{NULL, NULL}
};

static struct nsgtk_treeview *cookies_window;
static GtkBuilder *gladeFile;
GtkWindow *wndCookies;

/**
 * Creates the window for the cookies tree.
 */
bool nsgtk_cookies_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;

	GError* error = NULL;
	gladeFile = gtk_builder_new ();
	if (!gtk_builder_add_from_file(gladeFile, glade_file_location, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return false;
	}
	
	gtk_builder_connect_signals(gladeFile, NULL);
	
	wndCookies = GTK_WINDOW(gtk_builder_get_object(gladeFile, "wndCookies"));
	window = wndCookies;
	
	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(gladeFile,
			"cookiesScrolled"));

	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(gladeFile,
			"cookiesDrawingArea"));
	
	cookies_window = nsgtk_treeview_create(TREE_COOKIES, window,
			scrolled, drawing_area);
	
	if (cookies_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, cookies_window);
		
	nsgtk_cookies_init_menu();

	return true;
}

/**
 * Connects menu events in the cookies window.
 */
void nsgtk_cookies_init_menu()
{
	struct menu_events *event = menu_events;
	GtkWidget *w;

	while (event->widget != NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(gladeFile, event->widget));
		if (w == NULL) {
			LOG(("Unable to connect menu widget ""%s""", event->widget));		} else {
			g_signal_connect(G_OBJECT(w), "activate", event->handler, cookies_window);
		}
		event++;
	}
}

/**
 * Destroys the cookies window and performs any other necessary cleanup actions.
 */
void nsgtk_cookies_destroy(void)
{
	/* TODO: what about gladeFile? */
	nsgtk_treeview_destroy(cookies_window);
}


/* edit menu */
MENUHANDLER(delete_selected)
{
	cookie_manager_keypress(KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(delete_all)
{
	cookie_manager_keypress(KEY_SELECT_ALL);
	cookie_manager_keypress(KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(select_all)
{
	cookie_manager_keypress(KEY_SELECT_ALL);
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	cookie_manager_keypress(KEY_CLEAR_SELECTION);
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	cookie_manager_expand(false);
	return TRUE;
}

MENUHANDLER(expand_domains)
{
	cookie_manager_expand(true);
	return TRUE;
}

MENUHANDLER(expand_cookies)
{
	cookie_manager_expand(false);
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	cookie_manager_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_domains)
{
	cookie_manager_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_cookies)
{
	cookie_manager_contract(false);
	return TRUE;
}
