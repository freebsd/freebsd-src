/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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


#include "desktop/global_history.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "desktop/textinput.h"
#include "gtk/gui.h"
#include "gtk/history.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "utils/log.h"
#include "utils/utils.h"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_history_init_menu(void);

/* file menu*/
MENUPROTO(export);

/* edit menu */
MENUPROTO(delete_selected);
MENUPROTO(delete_all);
MENUPROTO(select_all);
MENUPROTO(clear_selection);

/* view menu*/
MENUPROTO(expand_all);
MENUPROTO(expand_directories);
MENUPROTO(expand_addresses);	  
MENUPROTO(collapse_all);
MENUPROTO(collapse_directories);
MENUPROTO(collapse_addresses);

MENUPROTO(launch);

static struct menu_events menu_events[] = {
	
	/* file menu*/
	MENUEVENT(export),
	
	/* edit menu */
	MENUEVENT(delete_selected),
	MENUEVENT(delete_all),
	MENUEVENT(select_all),
	MENUEVENT(clear_selection),
	
	/* view menu*/
	MENUEVENT(expand_all),
	MENUEVENT(expand_directories),
	MENUEVENT(expand_addresses),		  
	MENUEVENT(collapse_all),
	MENUEVENT(collapse_directories),
	MENUEVENT(collapse_addresses),
		  
	MENUEVENT(launch),
		  {NULL, NULL}
};

static struct nsgtk_treeview *global_history_window;
static GtkBuilder *gladeFile;
GtkWindow *wndHistory;


/* exported interface, documented in gtk_history.h */
bool nsgtk_history_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	GError* error = NULL;

	gladeFile = gtk_builder_new();
	if (!gtk_builder_add_from_file(gladeFile, glade_file_location, &error))  {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return false;
	}

	gtk_builder_connect_signals(gladeFile, NULL);
	
	wndHistory = GTK_WINDOW(gtk_builder_get_object(gladeFile, "wndHistory"));
	
	window = wndHistory;
	
	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(gladeFile, "globalHistoryScrolled"));

	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(gladeFile, "globalHistoryDrawingArea"));

	global_history_window = nsgtk_treeview_create(
		TREE_HISTORY, window, scrolled, drawing_area);
	
	if (global_history_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr)				\
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, global_history_window);
	
	nsgtk_history_init_menu();

	return true;
}


/**
 * Connects menu events in the global history window.
 */
void nsgtk_history_init_menu(void)
{
	struct menu_events *event = menu_events;
	GtkWidget *w;

	while (event->widget != NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(gladeFile, event->widget));
		if (w == NULL) {
			LOG(("Unable to connect menu widget ""%s""", event->widget));
		} else {
			g_signal_connect(G_OBJECT(w), "activate", event->handler, global_history_window);
		}
		event++;
	}
}


/**
 * Destroys the global history window and performs any other necessary cleanup
 * actions.
 */
void nsgtk_history_destroy(void)
{
	/* TODO: what about gladeFile? */
	nsgtk_treeview_destroy(global_history_window);
}


/* file menu*/
MENUHANDLER(export)
{
	GtkWidget *save_dialog;
	save_dialog = gtk_file_chooser_dialog_new("Save File",
			wndHistory,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"history.html");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		
		global_history_export(filename, NULL);		
		g_free(filename);
	}
	
	gtk_widget_destroy(save_dialog);

	return TRUE;
}

/* edit menu */
MENUHANDLER(delete_selected)
{
	global_history_keypress(KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(delete_all)
{
	global_history_keypress(KEY_SELECT_ALL);
	global_history_keypress(KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(select_all)
{
	global_history_keypress(KEY_SELECT_ALL);
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	global_history_keypress(KEY_CLEAR_SELECTION);
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	global_history_expand(false);
	return TRUE;
}

MENUHANDLER(expand_directories)
{
	global_history_expand(true);
	return TRUE;
}

MENUHANDLER(expand_addresses)
{
	global_history_expand(false);
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	global_history_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_directories)
{
	global_history_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_addresses)
{
	global_history_contract(false);
	return TRUE;
}

MENUHANDLER(launch)
{
	global_history_keypress(KEY_CR);
	return TRUE;
}
