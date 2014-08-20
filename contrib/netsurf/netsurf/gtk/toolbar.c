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

#include <gtk/gtk.h>

#include "desktop/browser.h"
#include "desktop/searchweb.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "gtk/toolbar.h"
#include "gtk/gui.h"
#include "gtk/scaffolding.h"
#include "gtk/search.h"
#include "gtk/theme.h"
#include "gtk/throbber.h"
#include "gtk/window.h"
#include "gtk/compat.h"

static GtkTargetEntry entry = {(char *)"nsgtk_button_data",
		GTK_TARGET_SAME_APP, 0};

static bool edit_mode = false;

struct nsgtk_toolbar_custom_store {
	GtkWidget *window;
 	GtkWidget *store_buttons[PLACEHOLDER_BUTTON];
	GtkWidget *widgetvbox;
	GtkWidget *currentbar;
	char numberh; /* current horizontal location while adding */
	GtkBuilder *glade;			/* button widgets to store */
	int buttonlocations[PLACEHOLDER_BUTTON];
	int currentbutton;
	bool fromstore;
};
/* the number of buttons that fit in the width of the store window */
#define NSGTK_STORE_WIDTH 6

/* the 'standard' width of a button that makes sufficient of its label
visible */
#define NSGTK_BUTTON_WIDTH 111

/* the 'standard' height of a button that fits as many toolbars as
possible into the store */
#define NSGTK_BUTTON_HEIGHT 70

/* the 'normal' width of the websearch bar */
#define NSGTK_WEBSEARCH_WIDTH 150

static struct nsgtk_toolbar_custom_store store;
static struct nsgtk_toolbar_custom_store *window = &store;

static void nsgtk_toolbar_close(nsgtk_scaffolding *g);
static void nsgtk_toolbar_window_open(nsgtk_scaffolding *g);
static void nsgtk_toolbar_customization_save(nsgtk_scaffolding *g);
static void nsgtk_toolbar_add_item_to_toolbar(nsgtk_scaffolding *g, int i,
		struct nsgtk_theme *theme);
static bool nsgtk_toolbar_add_store_widget(GtkWidget *widget);
static gboolean nsgtk_toolbar_data(GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, guint time, gpointer data);
static gboolean nsgtk_toolbar_store_return(GtkWidget *widget, GdkDragContext 			*gdc, gint x, gint y, guint time, gpointer data);
static gboolean nsgtk_toolbar_action(GtkWidget *widget, GdkDragContext
		*drag_context, gint x, gint y, guint time, gpointer data);
gboolean nsgtk_toolbar_store_action(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, guint time, gpointer data);
static gboolean nsgtk_toolbar_move_complete(GtkWidget *widget, GdkDragContext
		*gdc, gint x, gint y, GtkSelectionData *selection, guint info,
		guint time, gpointer data);
static void nsgtk_toolbar_clear(GtkWidget *widget, GdkDragContext *gdc, guint
		time, gpointer data);
static gboolean nsgtk_toolbar_delete(GtkWidget *widget, GdkEvent *event,
		gpointer data);
static gboolean nsgtk_toolbar_cancel_clicked(GtkWidget *widget, gpointer data);
static gboolean nsgtk_toolbar_reset(GtkWidget *widget, gpointer data);
static gboolean nsgtk_toolbar_persist(GtkWidget *widget, gpointer data);
static void nsgtk_toolbar_cast(nsgtk_scaffolding *g);
static GtkWidget *nsgtk_toolbar_make_widget(nsgtk_scaffolding *g,
		nsgtk_toolbar_button i,	struct nsgtk_theme *theme);
static void nsgtk_toolbar_set_handler(nsgtk_scaffolding *g,
		nsgtk_toolbar_button i);
static void nsgtk_toolbar_temp_connect(nsgtk_scaffolding *g,
		nsgtk_toolbar_button i);
static void nsgtk_toolbar_clear_toolbar(GtkWidget *widget, gpointer data);
static nsgtk_toolbar_button nsgtk_toolbar_get_id_at_location(
		nsgtk_scaffolding *g, int i);

/**
 * change behaviour of scaffoldings while editing toolbar; all buttons as
 * well as window clicks are desensitized; then buttons in the front window
 * are changed to movable buttons
 */
void nsgtk_toolbar_customization_init(nsgtk_scaffolding *g)
{
	int i;
	nsgtk_scaffolding *list = scaf_list;
	edit_mode = true;

	while (list) {
		g_signal_handler_block(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_CLICK));
		g_signal_handler_block(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_REDRAW));
		nsgtk_widget_override_background_color(
			GTK_WIDGET(nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
			GTK_STATE_NORMAL, 0, 0xEEEE, 0xEEEE, 0xEEEE);

		if (list == g) {
			list = nsgtk_scaffolding_iterate(list);
			continue;
		}
		/* set sensitive for all gui_windows save g */
		gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_window(
				list)), FALSE);
		list = nsgtk_scaffolding_iterate(list);
	}
	/* set sensitive for all of g save toolbar */
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_menu_bar(g)),
			FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_notebook(g)),
			FALSE);

	/* set editable aspect for toolbar */
	gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)),
			nsgtk_toolbar_clear_toolbar, g);
	nsgtk_toolbar_set_physical(g);
	/* memorize button locations, set editable */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		window->buttonlocations[i] = nsgtk_scaffolding_button(g, i)
				->location;
		if ((window->buttonlocations[i] == -1) || (i == URL_BAR_ITEM))
			continue;
		gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(
				nsgtk_scaffolding_button(g, i)->button), TRUE);
		gtk_drag_source_set(GTK_WIDGET(nsgtk_scaffolding_button(
				g, i)->button),	GDK_BUTTON1_MASK, &entry, 1,
				GDK_ACTION_COPY);
		nsgtk_toolbar_temp_connect(g, i);
	}

	/* add move button listeners */
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-drop", G_CALLBACK(nsgtk_toolbar_data), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-data-received", G_CALLBACK(
			nsgtk_toolbar_move_complete), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-motion", G_CALLBACK(nsgtk_toolbar_action), g);
	g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			"drag-leave", G_CALLBACK(
			nsgtk_toolbar_clear), g);

	/* set data types */
	gtk_drag_dest_set(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)),
			GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			&entry, 1, GDK_ACTION_COPY);

	/* open toolbar window */
	nsgtk_toolbar_window_open(g);
}

/**
 * create store window
 */
void nsgtk_toolbar_window_open(nsgtk_scaffolding *g)
{
	int x = 0, y = 0;
	GError* error = NULL;
	struct nsgtk_theme *theme =
			nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);
	if (theme == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		return;
	}

	window->glade = gtk_builder_new();
	if (!gtk_builder_add_from_file(window->glade, 
				       glade_file_location->toolbar, 
				       &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		warn_user(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	gtk_builder_connect_signals(window->glade, NULL);

	window->window = GTK_WIDGET(gtk_builder_get_object(window->glade, "toolbarwindow"));
	if (window->window == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	window->widgetvbox = GTK_WIDGET(gtk_builder_get_object(window->glade, "widgetvbox"));
	if (window->widgetvbox == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		nsgtk_toolbar_cancel_clicked(NULL, g);
		free(theme);
		return;
	}

	window->numberh = NSGTK_STORE_WIDTH; /* preset to width [in buttons] of */
				/*  store to cause creation of a new toolbar */
	window->currentbutton = -1;
	/* load toolbuttons */
	/* add toolbuttons to window */
	/* set event handlers */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (i == URL_BAR_ITEM)
			continue;
		window->store_buttons[i] =
				nsgtk_toolbar_make_widget(g, i, theme);
		if (window->store_buttons[i] == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			continue;
		}
		nsgtk_toolbar_add_store_widget(window->store_buttons[i]);
		g_signal_connect(window->store_buttons[i], "drag-data-get",
				G_CALLBACK(
				nsgtk_scaffolding_button(g, i)->dataplus), g);
	}
	free(theme);
	gtk_window_set_transient_for(GTK_WINDOW(window->window),
			nsgtk_scaffolding_window(g));
	gtk_window_set_title(GTK_WINDOW(window->window), messages_get(
			"gtkToolBarTitle"));
	gtk_window_set_accept_focus(GTK_WINDOW(window->window), FALSE);
	gtk_drag_dest_set(GTK_WIDGET(window->window), GTK_DEST_DEFAULT_MOTION |
			GTK_DEST_DEFAULT_DROP, &entry, 1, GDK_ACTION_COPY);
	gtk_widget_show_all(window->window);
	gtk_window_set_position(GTK_WINDOW(window->window),
			GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_get_position(nsgtk_scaffolding_window(g), &x, &y);
	gtk_window_move(GTK_WINDOW(window->window), x, y + 100);
	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(window->glade, "cancelbutton")),
			 "clicked", 
			 G_CALLBACK(nsgtk_toolbar_cancel_clicked), 
			 g);

	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(window->glade, "okbutton")),
			"clicked", G_CALLBACK(nsgtk_toolbar_persist), g);
	g_signal_connect(GTK_WIDGET(gtk_builder_get_object(window->glade, "resetbutton")),
			"clicked", G_CALLBACK(nsgtk_toolbar_reset), g);
	g_signal_connect(window->window, "delete-event",
			G_CALLBACK(nsgtk_toolbar_delete), g);
	g_signal_connect(window->window, "drag-drop",
			G_CALLBACK(nsgtk_toolbar_store_return), g);
	g_signal_connect(window->window, "drag-motion",
			G_CALLBACK(nsgtk_toolbar_store_action), g);
}

/**
 * when titlebar / alt-F4 window close event happens
 */
gboolean nsgtk_toolbar_delete(GtkWidget *widget, GdkEvent *event,
		gpointer data)
{
	edit_mode = false;
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	/* reset g->buttons->location */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_scaffolding_button(g, i)->location =
				window->buttonlocations[i];
	}
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_toolbar_close(g);
	nsgtk_scaffolding_set_sensitivity(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * when cancel button is clicked
 */
gboolean nsgtk_toolbar_cancel_clicked(GtkWidget *widget, gpointer data)
{
	edit_mode = false;
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	/* reset g->buttons->location */
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		nsgtk_scaffolding_button(g, i)->location =
				window->buttonlocations[i];
	}
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_toolbar_close(g);
	nsgtk_scaffolding_set_sensitivity(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * when 'save settings' button is clicked
 */
gboolean nsgtk_toolbar_persist(GtkWidget *widget, gpointer data)
{
	edit_mode = false;
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	/* save state to file, update toolbars for all windows */
	nsgtk_toolbar_customization_save(g);
	nsgtk_toolbar_cast(g);
	nsgtk_toolbar_set_physical(g);
	nsgtk_toolbar_close(g);
	gtk_widget_destroy(window->window);
	return TRUE;
}

/**
 * when 'reload defaults' button is clicked
 */
gboolean nsgtk_toolbar_reset(GtkWidget *widget, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	int i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
		nsgtk_scaffolding_button(g, i)->location =
				(i <= THROBBER_ITEM) ? i : -1;
	nsgtk_toolbar_set_physical(g);
	for (i = BACK_BUTTON; i <= THROBBER_ITEM; i++) {
		if (i == URL_BAR_ITEM)
			continue;
		gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(
				nsgtk_scaffolding_button(g, i)->button), TRUE);
		gtk_drag_source_set(GTK_WIDGET(
				nsgtk_scaffolding_button(g, i)->button),
				GDK_BUTTON1_MASK, &entry, 1, GDK_ACTION_COPY);
		nsgtk_toolbar_temp_connect(g, i);
	}
	return TRUE;
}

/**
 * set toolbar logical -> physical; physically visible toolbar buttons are made
 * to correspond to the logically stored schema in terms of location
 * visibility etc
 */
void nsgtk_toolbar_set_physical(nsgtk_scaffolding *g)
{
	int i;
	struct nsgtk_theme *theme =
			nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);
	if (theme == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}
	/* simplest is to clear the toolbar then reload it from memory */
	gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)),
			nsgtk_toolbar_clear_toolbar, g);
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
		nsgtk_toolbar_add_item_to_toolbar(g, i, theme);
	gtk_widget_show_all(GTK_WIDGET(nsgtk_scaffolding_toolbar(g)));
	free(theme);
}

/**
 * cleanup code physical update of all toolbars; resensitize
 * \param g the 'front' scaffolding that called customize
 */
void nsgtk_toolbar_close(nsgtk_scaffolding *g)
{
	int i;
	nsgtk_scaffolding *list = scaf_list;
	while (list) {
		struct nsgtk_theme *theme =
				nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);
		if (theme == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			continue;
		}
		/* clear toolbar */
		gtk_container_foreach(GTK_CONTAINER(nsgtk_scaffolding_toolbar(
				list)), nsgtk_toolbar_clear_toolbar, list);
		/* then add items */
		for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
			nsgtk_toolbar_add_item_to_toolbar(list, i, theme);
		}
		nsgtk_toolbar_connect_all(list);
		gtk_widget_show_all(GTK_WIDGET(nsgtk_scaffolding_toolbar(
				list)));
		nsgtk_scaffolding_set_sensitivity(list);
		nsgtk_widget_override_background_color(GTK_WIDGET(nsgtk_window_get_layout(nsgtk_scaffolding_top_level(list))), GTK_STATE_NORMAL, 0, 0xFFFF, 0xFFFF, 0xFFFF);
		g_signal_handler_unblock(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_CLICK));
		g_signal_handler_unblock(GTK_WIDGET(
				nsgtk_window_get_layout(
				nsgtk_scaffolding_top_level(list))),
				nsgtk_window_get_signalhandler(
				nsgtk_scaffolding_top_level(list),
				NSGTK_WINDOW_SIGNAL_REDRAW));
		browser_window_refresh_url_bar(
				nsgtk_get_browser_window(
				nsgtk_scaffolding_top_level(list)));

		if (list != g)
			gtk_widget_set_sensitive(GTK_WIDGET(
					nsgtk_scaffolding_window(list)), TRUE);
		free(theme);
		list = nsgtk_scaffolding_iterate(list);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_notebook(g)),
			TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(nsgtk_scaffolding_menu_bar(g)),
			TRUE);
	/* update favicon etc */
	nsgtk_scaffolding_set_top_level(nsgtk_scaffolding_top_level(g));

	gui_set_search_ico(search_web_ico());
}

/**
 * callback function to iterate toolbar's widgets
 */
void nsgtk_toolbar_clear_toolbar(GtkWidget *widget, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	gtk_container_remove(GTK_CONTAINER(nsgtk_scaffolding_toolbar(g)), widget);
}

/**
 * add item to toolbar
 * \param g the scaffolding whose toolbar an item is added to
 * \param i the location in the toolbar
 * the function should be called, when multiple items are being added,
 * in ascending order
 */
void nsgtk_toolbar_add_item_to_toolbar(nsgtk_scaffolding *g, int i,
		struct nsgtk_theme *theme)
{
	int q;
	for (q = BACK_BUTTON; q < PLACEHOLDER_BUTTON; q++)
		if (nsgtk_scaffolding_button(g, q)->location == i) {
			nsgtk_scaffolding_button(g, q)->button = GTK_TOOL_ITEM(
					nsgtk_toolbar_make_widget(g, q,
					theme));
			gtk_toolbar_insert(nsgtk_scaffolding_toolbar(g),
					nsgtk_scaffolding_button(g, q)->button,
					i);
			break;
		}
}

/**
 * physically add widgets to store window
 */
bool nsgtk_toolbar_add_store_widget(GtkWidget *widget)
{
	if (window->numberh >= NSGTK_STORE_WIDTH) {
		window->currentbar = gtk_toolbar_new();
		if (window->currentbar == NULL) {
			warn_user("NoMemory", 0);
			return false;
		}
		gtk_toolbar_set_style(GTK_TOOLBAR(window->currentbar),
				GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(window->currentbar),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
		gtk_box_pack_start(GTK_BOX(window->widgetvbox),
			window->currentbar, FALSE, FALSE, 0);
		window->numberh = 0;
	}
	gtk_widget_set_size_request(widget, NSGTK_BUTTON_WIDTH,
			NSGTK_BUTTON_HEIGHT);
	gtk_toolbar_insert(GTK_TOOLBAR(window->currentbar), GTK_TOOL_ITEM(
			widget), window->numberh++);
	gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(widget), TRUE);
	gtk_drag_source_set(widget, GDK_BUTTON1_MASK, &entry, 1,
			GDK_ACTION_COPY);
	gtk_widget_show_all(window->window);
	return true;
}

/**
 * called when a widget is dropped onto the toolbar
 */
gboolean nsgtk_toolbar_data(GtkWidget *widget, GdkDragContext *gdc, gint x,
		gint y, guint time, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	int ind = gtk_toolbar_get_drop_index(nsgtk_scaffolding_toolbar(g),
			x, y);
	int q, i;
	if (window->currentbutton == -1)
		return TRUE;
	struct nsgtk_theme *theme =
			nsgtk_theme_load(GTK_ICON_SIZE_LARGE_TOOLBAR);
	if (theme == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return TRUE;
	}
	if (nsgtk_scaffolding_button(g, window->currentbutton)->location
			!= -1) {
		/* widget was already in the toolbar; so replace */
		if (nsgtk_scaffolding_button(g, window->currentbutton)->
				location < ind)
			ind--;
		gtk_container_remove(GTK_CONTAINER(
				nsgtk_scaffolding_toolbar(g)), GTK_WIDGET(
				nsgtk_scaffolding_button(g,
				window->currentbutton)->button));
		/* 'move' all widgets further right than the original location,
		 * one place to the left in logical schema */
		for (i = nsgtk_scaffolding_button(g, window->currentbutton)->
				location + 1; i < PLACEHOLDER_BUTTON; i++) {
			q = nsgtk_toolbar_get_id_at_location(g, i);
			if (q == -1)
				continue;
			nsgtk_scaffolding_button(g, q)->location--;
		}
		nsgtk_scaffolding_button(g, window->currentbutton)->
				location = -1;
	}
	nsgtk_scaffolding_button(g, window->currentbutton)->button =
			GTK_TOOL_ITEM(nsgtk_toolbar_make_widget(g,
			window->currentbutton, theme));
	free(theme);
	if (nsgtk_scaffolding_button(g, window->currentbutton)->button
			== NULL) {
		warn_user("NoMemory", 0);
		return TRUE;
	}
	/* update logical schema */
	nsgtk_scaffolding_reset_offset(g);
	/* 'move' all widgets further right than the new location, one place to
	 * the right in logical schema */
	for (i = PLACEHOLDER_BUTTON - 1; i >= ind; i--) {
		q = nsgtk_toolbar_get_id_at_location(g, i);
		if (q == -1)
			continue;
		nsgtk_scaffolding_button(g, q)->location++;
	}
	nsgtk_scaffolding_button(g, window->currentbutton)->location = ind;

	/* complete action */
	gtk_toolbar_insert(nsgtk_scaffolding_toolbar(g),
			nsgtk_scaffolding_button(g,
			window->currentbutton)->button, ind);
	gtk_tool_item_set_use_drag_window(GTK_TOOL_ITEM(
			nsgtk_scaffolding_button(g,
			window->currentbutton)->button), TRUE);
	gtk_drag_source_set(GTK_WIDGET(
			nsgtk_scaffolding_button(g,
			window->currentbutton)->button),
			GDK_BUTTON1_MASK, &entry, 1, GDK_ACTION_COPY);
	nsgtk_toolbar_temp_connect(g, window->currentbutton);
	gtk_widget_show_all(GTK_WIDGET(
			nsgtk_scaffolding_button(g,
			window->currentbutton)->button));
	window->currentbutton = -1;
	return TRUE;
}

/**
 * connected to toolbutton drop; perhaps one day it'll work properly so it may
 * replace the global current_button
 */

gboolean nsgtk_toolbar_move_complete(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, GtkSelectionData *selection, guint info,
		guint time, gpointer data)
{
	return FALSE;
}

/**
 * called when a widget is dropped onto the store window
 */
gboolean nsgtk_toolbar_store_return(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, guint time, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	int q, i;

	if ((window->fromstore) || (window->currentbutton == -1)) {
		window->currentbutton = -1;
		return FALSE;
	}
	if (nsgtk_scaffolding_button(g, window->currentbutton)->location
			!= -1) {
		/* 'move' all widgets further right, one place to the left
		 * in logical schema */
		for (i = nsgtk_scaffolding_button(g, window->currentbutton)->
				location + 1; i < PLACEHOLDER_BUTTON; i++) {
			q = nsgtk_toolbar_get_id_at_location(g, i);
			if (q == -1)
				continue;
			nsgtk_scaffolding_button(g, q)->location--;
		}
		gtk_container_remove(GTK_CONTAINER(
				nsgtk_scaffolding_toolbar(g)), GTK_WIDGET(
				nsgtk_scaffolding_button(g,
				window->currentbutton)->button));
		nsgtk_scaffolding_button(g, window->currentbutton)->location
				= -1;
	}
	window->currentbutton = -1;
	gtk_drag_finish(gdc, TRUE, TRUE, time);
	return FALSE;
}
/**
 * called when hovering an item above the toolbar
 */
gboolean nsgtk_toolbar_action(GtkWidget *widget, GdkDragContext *gdc, gint x,
		gint y, guint time, gpointer data)
{
	nsgtk_scaffolding *g = (nsgtk_scaffolding *)data;
	GtkToolItem *item = gtk_tool_button_new(NULL, NULL);
	if (item != NULL)
		gtk_toolbar_set_drop_highlight_item(
				nsgtk_scaffolding_toolbar(g),
				GTK_TOOL_ITEM(item),
				gtk_toolbar_get_drop_index(
				nsgtk_scaffolding_toolbar(g), x, y));
	return FALSE;
}

/**
 * called when hovering above the store
 */
gboolean nsgtk_toolbar_store_action(GtkWidget *widget, GdkDragContext *gdc,
		gint x, gint y, guint time, gpointer data)
{
	return FALSE;
}
/**
 * called when hovering stops
 */
void nsgtk_toolbar_clear(GtkWidget *widget, GdkDragContext *gdc, guint time,
		gpointer data)
{
	gtk_toolbar_set_drop_highlight_item(GTK_TOOLBAR(widget), NULL, 0);
}

/**
 * widget factory for creation of toolbar item widgets
 * \param g the reference scaffolding
 * \param i the id of the widget
 * \param theme the theme to make the widgets from
 */
GtkWidget *nsgtk_toolbar_make_widget(nsgtk_scaffolding *g,
		nsgtk_toolbar_button i,	struct nsgtk_theme *theme)
{
	switch(i) {

/* gtk_tool_button_new() accepts NULL args */
#define MAKE_STOCKBUTTON(p, q) case p##_BUTTON: {\
		GtkStockItem item;\
		char *label = NULL;\
		gtk_stock_lookup(#q, &item);\
		if (item.label != NULL)\
			label = remove_underscores(item.label, false);\
		GtkWidget *w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(\
				theme->image[p##_BUTTON]), label));\
		if (label != NULL) {\
			free(label);\
			label = NULL;\
		}\
		return w;\
	}

	MAKE_STOCKBUTTON(HOME, gtk-home)
	MAKE_STOCKBUTTON(BACK, gtk-go-back)
	MAKE_STOCKBUTTON(FORWARD, gtk-go-forward)
	MAKE_STOCKBUTTON(STOP, gtk-stop)
	MAKE_STOCKBUTTON(RELOAD, gtk-refresh)
#undef MAKE_STOCKBUTTON
	case HISTORY_BUTTON:
		return GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
				theme->image[HISTORY_BUTTON]), ""));
	case URL_BAR_ITEM: {
		GtkWidget *entry = nsgtk_entry_new();
		GtkWidget *w = GTK_WIDGET(gtk_tool_item_new());

		if ((entry == NULL) || (w == NULL)) {
			warn_user(messages_get("NoMemory"), 0);
			return NULL;
		}

		nsgtk_entry_set_icon_from_pixbuf(entry, 
						 GTK_ENTRY_ICON_PRIMARY, 
						 favicon_pixbuf);

		gtk_container_add(GTK_CONTAINER(w), entry);
		gtk_tool_item_set_expand(GTK_TOOL_ITEM(w), TRUE);
		return w;
	}
	case THROBBER_ITEM: {
		if (edit_mode)
			return GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
					gtk_image_new_from_pixbuf(
					nsgtk_throbber->framedata[0])),
					"[throbber]"));
		if ((nsgtk_throbber == NULL) || (nsgtk_throbber->framedata ==
				NULL) || (nsgtk_throbber->framedata[0] ==
				NULL))
			return NULL;
		GtkWidget *image = GTK_WIDGET(gtk_image_new_from_pixbuf(
				nsgtk_throbber->framedata[0]));
		GtkWidget *w = GTK_WIDGET(gtk_tool_item_new());
		GtkWidget *al = GTK_WIDGET(gtk_alignment_new(0.5, 0.5, 1, 1));
		if ((w == NULL) || (al == NULL)) {
			warn_user(messages_get("NoMemory"), 0);
			return NULL;
		}
		gtk_alignment_set_padding(GTK_ALIGNMENT(al), 0, 0, 3, 3);
		if (image != NULL)
			gtk_container_add(GTK_CONTAINER(al), image);
		gtk_container_add(GTK_CONTAINER(w), al);
		return w;
	}
	case WEBSEARCH_ITEM: {
		if (edit_mode)
			return GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(
					gtk_image_new_from_stock("gtk-find",
					GTK_ICON_SIZE_LARGE_TOOLBAR)),
					"[websearch]"));

		GtkWidget *entry = nsgtk_entry_new();

		GtkWidget *w = GTK_WIDGET(gtk_tool_item_new());

		if ((entry == NULL) || (w == NULL)) {
			warn_user(messages_get("NoMemory"), 0);
			return NULL;
		}

		gtk_widget_set_size_request(entry, NSGTK_WEBSEARCH_WIDTH, -1);

		nsgtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_PRIMARY, "gtk-info");

		gtk_container_add(GTK_CONTAINER(w), entry);
		return w;
	}

/* gtk_tool_button_new accepts NULL args */
#define MAKE_MENUBUTTON(p, q) case p##_BUTTON: {\
		char *label = NULL;\
		label = remove_underscores(messages_get(#q), false);\
		GtkWidget *w = GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(\
				theme->image[p##_BUTTON]), label));\
		if (label != NULL)\
			free(label);\
		return w;\
	}

	MAKE_MENUBUTTON(NEWWINDOW, gtkNewWindow)
	MAKE_MENUBUTTON(NEWTAB, gtkNewTab)
	MAKE_MENUBUTTON(OPENFILE, gtkOpenFile)
	MAKE_MENUBUTTON(CLOSETAB, gtkCloseTab)
	MAKE_MENUBUTTON(CLOSEWINDOW, gtkCloseWindow)
	MAKE_MENUBUTTON(SAVEPAGE, gtkSavePage)
	MAKE_MENUBUTTON(PRINTPREVIEW, gtkPrintPreview)
	MAKE_MENUBUTTON(PRINT, gtkPrint)
	MAKE_MENUBUTTON(QUIT, gtkQuitMenu)
	MAKE_MENUBUTTON(CUT, gtkCut)
	MAKE_MENUBUTTON(COPY, gtkCopy)
	MAKE_MENUBUTTON(PASTE, gtkPaste)
	MAKE_MENUBUTTON(DELETE, gtkDelete)
	MAKE_MENUBUTTON(SELECTALL, gtkSelectAll)
	MAKE_MENUBUTTON(PREFERENCES, gtkPreferences)
	MAKE_MENUBUTTON(ZOOMPLUS, gtkZoomPlus)
	MAKE_MENUBUTTON(ZOOMMINUS, gtkZoomMinus)
	MAKE_MENUBUTTON(ZOOMNORMAL, gtkZoomNormal)
	MAKE_MENUBUTTON(FULLSCREEN, gtkFullScreen)
	MAKE_MENUBUTTON(VIEWSOURCE, gtkViewSource)
	MAKE_MENUBUTTON(CONTENTS, gtkContents)
	MAKE_MENUBUTTON(ABOUT, gtkAbout)
	MAKE_MENUBUTTON(PDF, gtkPDF)
	MAKE_MENUBUTTON(PLAINTEXT, gtkPlainText)
	MAKE_MENUBUTTON(DRAWFILE, gtkDrawFile)
	MAKE_MENUBUTTON(POSTSCRIPT, gtkPostScript)
	MAKE_MENUBUTTON(FIND, gtkFind)
	MAKE_MENUBUTTON(DOWNLOADS, gtkDownloads)
	MAKE_MENUBUTTON(SAVEWINDOWSIZE, gtkSaveWindowSize)
	MAKE_MENUBUTTON(TOGGLEDEBUGGING, gtkToggleDebugging)
	MAKE_MENUBUTTON(SAVEBOXTREE, gtkSaveBoxTree)
	MAKE_MENUBUTTON(SAVEDOMTREE, gtkSaveDomTree)
	MAKE_MENUBUTTON(LOCALHISTORY, gtkLocalHistory)
	MAKE_MENUBUTTON(GLOBALHISTORY, gtkGlobalHistory)
	MAKE_MENUBUTTON(ADDBOOKMARKS, gtkAddBookMarks)
	MAKE_MENUBUTTON(SHOWBOOKMARKS, gtkShowBookMarks)
	MAKE_MENUBUTTON(SHOWCOOKIES, gtkShowCookies)
	MAKE_MENUBUTTON(OPENLOCATION, gtkOpenLocation)
	MAKE_MENUBUTTON(NEXTTAB, gtkNextTab)
	MAKE_MENUBUTTON(PREVTAB, gtkPrevTab)
	MAKE_MENUBUTTON(GUIDE, gtkGuide)
	MAKE_MENUBUTTON(INFO, gtkUserInformation)
	default:
		return NULL;
#undef MAKE_MENUBUTTON
	}
}

/**
 * \return toolbar item id when a widget is an element of the scaffolding
 * else -1
 */
int nsgtk_toolbar_get_id_from_widget(GtkWidget *widget, nsgtk_scaffolding *g)
{
	int i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if ((nsgtk_scaffolding_button(g, i)->location != -1)
				&& (widget == GTK_WIDGET(
				nsgtk_scaffolding_button(g, i)->button))) {
			return i;
		}
	}
	return -1;
}

/**
 * \return toolbar item id from location when there is an item at that logical
 * location; else -1
 */
nsgtk_toolbar_button nsgtk_toolbar_get_id_at_location(nsgtk_scaffolding *g,
		int i)
{
	int q;
	for (q = BACK_BUTTON; q < PLACEHOLDER_BUTTON; q++)
		if (nsgtk_scaffolding_button(g, q)->location == i)
			return q;
	return -1;
}

/**
 * connect 'normal' handlers to toolbar buttons
 */

void nsgtk_toolbar_connect_all(nsgtk_scaffolding *g)
{
	int q, i;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		q = nsgtk_toolbar_get_id_at_location(g, i);
		if (q == -1)
			continue;
		if (nsgtk_scaffolding_button(g, q)->button != NULL)
			g_signal_connect(
					nsgtk_scaffolding_button(g, q)->button,
					"size-allocate", G_CALLBACK(
					nsgtk_scaffolding_toolbar_size_allocate
					), g);
		nsgtk_toolbar_set_handler(g, q);
	}
}

/**
 * add handlers to factory widgets
 * \param g the scaffolding to attach handlers to
 * \param i the toolbar item id
 */
void nsgtk_toolbar_set_handler(nsgtk_scaffolding *g, nsgtk_toolbar_button i)
{
	switch(i){
	case URL_BAR_ITEM:
		nsgtk_scaffolding_update_url_bar_ref(g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_urlbar(g)),
				"activate", G_CALLBACK(
				nsgtk_window_url_activate_event), g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_urlbar(g)),
				"changed", G_CALLBACK(
				nsgtk_window_url_changed), g);
		break;
	case THROBBER_ITEM:
		nsgtk_scaffolding_update_throbber_ref(g);
		break;
	case WEBSEARCH_ITEM:
		nsgtk_scaffolding_update_websearch_ref(g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_websearch(g)),
				"activate", G_CALLBACK(
				nsgtk_websearch_activate), g);
		g_signal_connect(GTK_WIDGET(nsgtk_scaffolding_websearch(g)),
				"button-press-event", G_CALLBACK(
				nsgtk_websearch_clear), g);
		break;
	default:
		if ((nsgtk_scaffolding_button(g, i)->bhandler != NULL) &&
				(nsgtk_scaffolding_button(g, i)->button
				!= NULL))
			g_signal_connect(nsgtk_scaffolding_button(g, i)->
					button, "clicked",
					G_CALLBACK(nsgtk_scaffolding_button(g,
					i)->bhandler), g);
	break;
	}
}

#define DATAHANDLER(p, q, r)\
gboolean nsgtk_toolbar_##p##_button_data(GtkWidget *widget, GdkDragContext\
		*cont, GtkSelectionData	*selection, guint info, guint time,\
		gpointer data)\
{\
	r->currentbutton = q##_BUTTON;\
	r->fromstore = true;\
	return TRUE;\
}\
gboolean nsgtk_toolbar_##p##_toolbar_button_data(GtkWidget *widget,\
		GdkDragContext *cont, GtkSelectionData *selection, guint info,\
		guint time, gpointer data)\
{\
	r->currentbutton = q##_BUTTON;\
	r->fromstore = false;\
	return TRUE;\
}

DATAHANDLER(home, HOME, window)
DATAHANDLER(forward, FORWARD, window)
DATAHANDLER(back, BACK, window)
DATAHANDLER(stop, STOP, window)
DATAHANDLER(reload, RELOAD, window)
DATAHANDLER(history, HISTORY, window)
DATAHANDLER(newwindow, NEWWINDOW, window)
DATAHANDLER(newtab, NEWTAB, window)
DATAHANDLER(openfile, OPENFILE, window)
DATAHANDLER(closetab, CLOSETAB, window)
DATAHANDLER(closewindow, CLOSEWINDOW, window)
DATAHANDLER(savepage, SAVEPAGE, window)
DATAHANDLER(printpreview, PRINTPREVIEW, window)
DATAHANDLER(print, PRINT, window)
DATAHANDLER(quit, QUIT, window)
DATAHANDLER(cut, CUT, window)
DATAHANDLER(copy, COPY, window)
DATAHANDLER(paste, PASTE, window)
DATAHANDLER(delete, DELETE, window)
DATAHANDLER(selectall, SELECTALL, window)
DATAHANDLER(preferences, PREFERENCES, window)
DATAHANDLER(zoomplus, ZOOMPLUS, window)
DATAHANDLER(zoomminus, ZOOMMINUS, window)
DATAHANDLER(zoomnormal, ZOOMNORMAL, window)
DATAHANDLER(fullscreen, FULLSCREEN, window)
DATAHANDLER(viewsource, VIEWSOURCE, window)
DATAHANDLER(contents, CONTENTS, window)
DATAHANDLER(about, ABOUT, window)
DATAHANDLER(pdf, PDF, window)
DATAHANDLER(plaintext, PLAINTEXT, window)
DATAHANDLER(drawfile, DRAWFILE, window)
DATAHANDLER(postscript, POSTSCRIPT, window)
DATAHANDLER(find, FIND, window)
DATAHANDLER(downloads, DOWNLOADS, window)
DATAHANDLER(savewindowsize, SAVEWINDOWSIZE, window)
DATAHANDLER(toggledebugging, TOGGLEDEBUGGING, window)
DATAHANDLER(saveboxtree, SAVEBOXTREE, window)
DATAHANDLER(savedomtree, SAVEDOMTREE, window)
DATAHANDLER(localhistory, LOCALHISTORY, window)
DATAHANDLER(globalhistory, GLOBALHISTORY, window)
DATAHANDLER(addbookmarks, ADDBOOKMARKS, window)
DATAHANDLER(showbookmarks, SHOWBOOKMARKS, window)
DATAHANDLER(showcookies, SHOWCOOKIES, window)
DATAHANDLER(openlocation, OPENLOCATION, window)
DATAHANDLER(nexttab, NEXTTAB, window)
DATAHANDLER(prevtab, PREVTAB, window)
DATAHANDLER(guide, GUIDE, window)
DATAHANDLER(info, INFO, window)
#undef DATAHANDLER
#define DATAHANDLER(p, q, r)\
gboolean nsgtk_toolbar_##p##_button_data(GtkWidget *widget, GdkDragContext\
		*cont, GtkSelectionData	*selection, guint info, guint time,\
		gpointer data)\
{\
	r->currentbutton = q##_ITEM;\
	r->fromstore = true;\
	return TRUE;\
}\
gboolean nsgtk_toolbar_##p##_toolbar_button_data(GtkWidget *widget,\
		GdkDragContext *cont, GtkSelectionData *selection, guint info,\
		guint time, gpointer data)\
{\
	r->currentbutton = q##_ITEM;\
	r->fromstore = false;\
	return TRUE;\
}

DATAHANDLER(throbber, THROBBER, window)
DATAHANDLER(websearch, WEBSEARCH, window)
#undef DATAHANDLER

/**
 * connect temporary handler for toolbar edit events
 */
void nsgtk_toolbar_temp_connect(nsgtk_scaffolding *g, nsgtk_toolbar_button i)
{
	if ((i == URL_BAR_ITEM) ||
			(nsgtk_scaffolding_button(g, i)->button == NULL) ||
			(nsgtk_scaffolding_button(g, i)->dataminus == NULL))
		return;
	g_signal_connect(nsgtk_scaffolding_button(g, i)->button,
			"drag-data-get", G_CALLBACK(nsgtk_scaffolding_button(
			g, i)->dataminus), g);
}

/**
 * load toolbar settings from file; file is a set of fields arranged as
 * [itemreference];[itemlocation]|[itemreference];[itemlocation]| etc
 */
void nsgtk_toolbar_customization_load(nsgtk_scaffolding *g)
{
	int i, ii;
	char *val;
	char buffer[SLEN("11;|") * 2 * PLACEHOLDER_BUTTON]; /* numbers 0-99 */
	buffer[0] = '\0';
	char *buffer1, *subbuffer, *ptr = NULL, *pter = NULL;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
		nsgtk_scaffolding_button(g, i)->location =
		(i <= THROBBER_ITEM) ? i : -1;
	FILE *f = fopen(toolbar_indices_file_location, "r");
	if (f == NULL) {
		warn_user(messages_get("gtkFileError"),
				toolbar_indices_file_location);
		return;
	}
	val = fgets(buffer, sizeof buffer, f);
	if (val == NULL) {
		LOG(("empty read toolbar settings"));
	}
	fclose(f);
	i = BACK_BUTTON;
	ii = BACK_BUTTON;
	buffer1 = strtok_r(buffer, "|", &ptr);
	while (buffer1 != NULL) {
		subbuffer = strtok_r(buffer1, ";", &pter);
		i = atoi(subbuffer);
		subbuffer = strtok_r(NULL, ";", &pter);
		ii = atoi(subbuffer);
		if ((i >= BACK_BUTTON) && (i < PLACEHOLDER_BUTTON) &&
				(ii >= -1) && (ii < PLACEHOLDER_BUTTON)) {
			nsgtk_scaffolding_button(g, i)->location = ii;
		}
		buffer1 = strtok_r(NULL, "|", &ptr);
	}
}

/**
 * cast toolbar settings to all scaffoldings referenced from the global linked
 * list of gui_windows
 */
void nsgtk_toolbar_cast(nsgtk_scaffolding *g)
{
	int i;
	nsgtk_scaffolding *list = scaf_list;
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
		window->buttonlocations[i] =
				((nsgtk_scaffolding_button(g, i)->location
				>= -1) &&
				(nsgtk_scaffolding_button(g, i)->location
				< PLACEHOLDER_BUTTON)) ?
				nsgtk_scaffolding_button(g, i)->location : -1;
	while (list) {
		if (list != g)
			for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++)
				nsgtk_scaffolding_button(list, i)->location =
						window->buttonlocations[i];
		list = nsgtk_scaffolding_iterate(list);
	}
}

/**
 * save toolbar settings to file
 */
void nsgtk_toolbar_customization_save(nsgtk_scaffolding *g)
{
	int i;
	FILE *f = fopen(toolbar_indices_file_location, "w");
	if (f == NULL){
		warn_user("gtkFileError", toolbar_indices_file_location);
		return;
	}
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		fprintf(f, "%d;%d|", i, nsgtk_scaffolding_button(g, i)->location);
	}
	fclose(f);
}

