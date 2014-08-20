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


#include "desktop/hotlist.h"
#include "utils/nsoption.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "gtk/gui.h"
#include "gtk/hotlist.h"
#include "gtk/plotters.h"
#include "gtk/scaffolding.h"
#include "gtk/treeview.h"
#include "utils/log.h"

#define GLADE_NAME "hotlist.glade"

#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
		GtkMenuItem *widget, gpointer g)
#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }		
#define MENUHANDLER(x) gboolean nsgtk_on_##x##_activate(GtkMenuItem *widget, \
		gpointer g)

struct menu_events {
	const char *widget;
	GCallback handler;
};

static void nsgtk_hotlist_init_menu(void);

/* file menu*/
MENUPROTO(export);
MENUPROTO(new_folder);
MENUPROTO(new_entry);

/* edit menu */
MENUPROTO(edit_selected);
MENUPROTO(delete_selected);
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
	MENUEVENT(new_folder),
	MENUEVENT(new_entry),
	
	/* edit menu */
	MENUEVENT(edit_selected),
	MENUEVENT(delete_selected),
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

static struct nsgtk_treeview *hotlist_window;
static GtkBuilder *gladeFile;
GtkWindow *wndHotlist;


/* exported interface docuemnted in gtk_hotlist.h */
bool nsgtk_hotlist_init(const char *glade_file_location)
{
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;

	GError* error = NULL;
	gladeFile = gtk_builder_new();
	if (!gtk_builder_add_from_file(gladeFile, glade_file_location, &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		return false;
	}
	
	gtk_builder_connect_signals(gladeFile, NULL);
	
	wndHotlist = GTK_WINDOW(gtk_builder_get_object(gladeFile, "wndHotlist"));
	window = wndHotlist;
	
	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(gladeFile,
			"hotlistScrolled"));

	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(gladeFile,
			"hotlistDrawingArea"));

	
	tree_hotlist_path = nsoption_charp(hotlist_path);
	hotlist_window = nsgtk_treeview_create(TREE_HOTLIST, window,
			scrolled, drawing_area);
	
	if (hotlist_window == NULL)
		return false;
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))	
	
	CONNECT(window, "delete_event", gtk_widget_hide_on_delete, NULL);
	CONNECT(window, "hide", nsgtk_tree_window_hide, hotlist_window);
		
	nsgtk_hotlist_init_menu();

	return true;
}


/**
 * Connects menu events in the hotlist window.
 */
void nsgtk_hotlist_init_menu(void)
{
	struct menu_events *event = menu_events;
	GtkWidget *w;

	while (event->widget != NULL) {
		w = GTK_WIDGET(gtk_builder_get_object(gladeFile, event->widget));
		if (w == NULL) {
			LOG(("Unable to connect menu widget ""%s""", event->widget));		} else {
			g_signal_connect(G_OBJECT(w), "activate", event->handler, hotlist_window);
		}
		event++;
	}
}


/**
 * Destroys the hotlist window and performs any other necessary cleanup actions.
 */
void nsgtk_hotlist_destroy(void)
{
	/* TODO: what about gladeFile? */
	nsgtk_treeview_destroy(hotlist_window);
}


/* file menu*/
MENUHANDLER(export)
{
	GtkWidget *save_dialog;
	save_dialog = gtk_file_chooser_dialog_new("Save File",
			wndHotlist,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");
	
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"hotlist.html");
	
	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		
		hotlist_export(filename, NULL);		
		g_free(filename);
	}
	
	gtk_widget_destroy(save_dialog);

	return TRUE;
}

MENUHANDLER(new_folder)
{
	hotlist_add_folder(NULL, false, 0);
	return TRUE;
}

MENUHANDLER(new_entry)
{
	hotlist_add_entry(NULL, NULL, false, 0);
	return TRUE;
}

/* edit menu */
MENUHANDLER(edit_selected)
{
	hotlist_edit_selection();
	return TRUE;
}

MENUHANDLER(delete_selected)
{
	hotlist_keypress(KEY_DELETE_LEFT);
	return TRUE;
}

MENUHANDLER(select_all)
{
	hotlist_keypress(KEY_SELECT_ALL);
	return TRUE;
}

MENUHANDLER(clear_selection)
{
	hotlist_keypress(KEY_CLEAR_SELECTION);
	return TRUE;
}

/* view menu*/
MENUHANDLER(expand_all)
{
	hotlist_expand(false);
	return TRUE;
}

MENUHANDLER(expand_directories)
{
	hotlist_expand(true);
	return TRUE;
}

MENUHANDLER(expand_addresses)
{
	hotlist_expand(false);
	return TRUE;
}

MENUHANDLER(collapse_all)
{
	hotlist_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_directories)
{
	hotlist_contract(true);
	return TRUE;
}

MENUHANDLER(collapse_addresses)
{
	hotlist_contract(false);
	return TRUE;
}

MENUHANDLER(launch)
{
	hotlist_keypress(KEY_CR);
	return TRUE;
}
