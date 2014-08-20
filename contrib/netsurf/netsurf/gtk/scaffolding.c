/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "utils/messages.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/hotlist.h"
#include "desktop/netsurf.h"
#include "desktop/plotters.h"
#include "desktop/print.h"
#include "desktop/save_complete.h"
#ifdef WITH_PDF_EXPORT
#include "desktop/save_pdf/font_haru.h"
#include "desktop/save_pdf/pdf_plotters.h"
#endif
#include "desktop/save_text.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"
#include "render/font.h"
#include "render/html.h"
#include "content/hlcache.h"

#include "gtk/compat.h"
#include "gtk/cookies.h"
#include "gtk/completion.h"
#include "gtk/dialogs/preferences.h"
#include "gtk/dialogs/about.h"
#include "gtk/dialogs/source.h"
#include "gtk/bitmap.h"
#include "gtk/gui.h"
#include "gtk/history.h"
#include "gtk/hotlist.h"
#include "gtk/download.h"
#include "gtk/menu.h"
#include "gtk/plotters.h"
#include "gtk/print.h"
#include "gtk/search.h"
#include "gtk/theme.h"
#include "gtk/throbber.h"
#include "gtk/toolbar.h"
#include "gtk/window.h"
#include "gtk/gdk.h"
#include "gtk/scaffolding.h"
#include "gtk/tabs.h"
#include "gtk/schedule.h"


/** Macro to define a handler for menu, button and activate events. */
#define MULTIHANDLER(q)\
static gboolean nsgtk_on_##q##_activate(struct gtk_scaffolding *g);\
static gboolean nsgtk_on_##q##_activate_menu(GtkMenuItem *widget, gpointer data)\
{\
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
static gboolean nsgtk_on_##q##_activate_button(GtkButton *widget, gpointer data)\
{\
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;\
	return nsgtk_on_##q##_activate(g);\
}\
static gboolean nsgtk_on_##q##_activate(struct gtk_scaffolding *g)

/** Macro to define a handler for menu events. */
#define MENUHANDLER(q)\
static gboolean nsgtk_on_##q##_activate_menu(GtkMenuItem *widget, gpointer data)

/** Macro to define a handler for button events. */
#define BUTTONHANDLER(q)\
static gboolean nsgtk_on_##q##_activate(GtkButton *widget, gpointer data)

/** Core scaffolding structure. */
struct gtk_scaffolding {
	GtkWindow			*window;
	GtkNotebook			*notebook;
	GtkWidget			*url_bar;
	GtkEntryCompletion		*url_bar_completion;

	/** menu bar hierarchy */
	struct nsgtk_bar_submenu        *menu_bar;

	/** right click popup menu hierarchy */
	struct nsgtk_popup_submenu      *menu_popup;

	GtkToolbar			*tool_bar;
	struct nsgtk_button_connect	*buttons[PLACEHOLDER_BUTTON];
	GtkImage			*throbber;
	struct gtk_search		*search;
	GtkWidget			*webSearchEntry;

	int 				offset;
	int				toolbarmem;
	int				toolbarbase;
	int				historybase;

	GtkBuilder			*xml;

	struct gtk_history_window	*history_window;

	int				throb_frame;
	struct gui_window		*top_level;

	bool				fullscreen;

	/* keep global linked list for gui interface adjustments */
	struct gtk_scaffolding 		*next, *prev;
};

/** current window for model dialogue use */
static struct gtk_scaffolding *current_model;

/** global list for interface changes */
nsgtk_scaffolding *scaf_list = NULL;

/** holds the context data for what's under the pointer, when the contextual
 *  menu is opened. */
static struct contextual_content current_menu_ctx;


/**
 * Helper to hide popup menu entries by grouping
 */
static void popup_menu_hide(struct nsgtk_popup_submenu *menu, bool submenu,
		bool link, bool nav, bool cnp, bool custom)
{
	if (submenu){
		gtk_widget_hide(GTK_WIDGET(menu->file_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->edit_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->view_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->nav_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->help_menuitem));

		gtk_widget_hide(menu->first_separator);
	}

	if (link) {
		gtk_widget_hide(GTK_WIDGET(menu->opentab_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->openwin_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->savelink_menuitem));

		gtk_widget_hide(menu->second_separator);
	}

	if (nav) {
		gtk_widget_hide(GTK_WIDGET(menu->back_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->forward_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->stop_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->reload_menuitem));
	}

	if (cnp) {
		gtk_widget_hide(GTK_WIDGET(menu->cut_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->copy_menuitem));
		gtk_widget_hide(GTK_WIDGET(menu->paste_menuitem));
	}

	if (custom) {
		gtk_widget_hide(GTK_WIDGET(menu->customize_menuitem));
	}

}

/**
 * Helper to show popup menu entries by grouping
 */
static void popup_menu_show(struct nsgtk_popup_submenu *menu, bool submenu,
		bool link, bool nav, bool cnp, bool custom)
{
	if (submenu){
		gtk_widget_show(GTK_WIDGET(menu->file_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->edit_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->view_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->nav_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->help_menuitem));

		gtk_widget_show(menu->first_separator);
	}

	if (link) {
		gtk_widget_show(GTK_WIDGET(menu->opentab_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->openwin_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->savelink_menuitem));

		gtk_widget_show(menu->second_separator);
	}

	if (nav) {
		gtk_widget_show(GTK_WIDGET(menu->back_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->forward_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->stop_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->reload_menuitem));
	}

	if (cnp) {
		gtk_widget_show(GTK_WIDGET(menu->cut_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->copy_menuitem));
		gtk_widget_show(GTK_WIDGET(menu->paste_menuitem));
	}

	if (custom) {
		gtk_widget_show(GTK_WIDGET(menu->customize_menuitem));
	}

}


/* event handlers and support functions for them */

/**
 * resource cleanup function for window destruction.
 */
static void scaffolding_window_destroy(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *gs = data;

	LOG(("scaffold:%p", gs));

	if ((gs->history_window) && (gs->history_window->window)) {
		gtk_widget_destroy(GTK_WIDGET(gs->history_window->window));
	}

	if (gs->prev != NULL) {
		gs->prev->next = gs->next;
	} else {
		scaf_list = gs->next;
	}
	if (gs->next != NULL) {
		gs->next->prev = gs->prev;
	}

	LOG(("scaffold list head: %p", scaf_list));

	if (scaf_list == NULL) {
		/* no more open windows */
		netsurf_quit = true;
	}
}

/* signal delivered on window delete event, allowing to halt close if
 * download is in progress
 */
static gboolean scaffolding_window_delete_event(GtkWidget *widget,
		GdkEvent *event, gpointer data)
{
	struct gtk_scaffolding *g = data;

	if (nsgtk_check_for_downloads(GTK_WINDOW(widget)) == false) {
		gtk_widget_destroy(GTK_WIDGET(g->window));
	}
	return TRUE;
}

/* exported interface documented in gtk_scaffold.h */
void nsgtk_scaffolding_destroy(nsgtk_scaffolding *gs)
{
	LOG(("scaffold: %p", gs));

	if (gtk_widget_in_destruction(GTK_WIDGET(gs->window)) != TRUE) {
		gtk_widget_destroy(GTK_WIDGET(gs->window));
	}
}

/**
 * Update the back and forward button sensitivity.
 */
static void nsgtk_window_update_back_forward(struct gtk_scaffolding *g)
{
	int width, height;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	g->buttons[BACK_BUTTON]->sensitivity =
			browser_window_history_back_available(bw);
	g->buttons[FORWARD_BUTTON]->sensitivity =
			browser_window_history_forward_available(bw);

	nsgtk_scaffolding_set_sensitivity(g);

	/* update the url bar, particularly necessary when tabbing */
	browser_window_refresh_url_bar(bw);

	/* update the local history window, as well as queuing a redraw
	 * for it.
	 */
	browser_window_history_size(bw, &width, &height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->drawing_area),
			width, height);
	gtk_widget_queue_draw(GTK_WIDGET(g->history_window->drawing_area));
}

/**
 * Make the throbber run.
 */
static void nsgtk_throb(void *p)
{
	struct gtk_scaffolding *g = p;

	if (g->throb_frame >= (nsgtk_throbber->nframes - 1))
		g->throb_frame = 1;
	else
		g->throb_frame++;

	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[
							g->throb_frame]);

	nsgtk_schedule(100, nsgtk_throb, p);
}

static guint nsgtk_scaffolding_update_edit_actions_sensitivity(
		struct gtk_scaffolding *g)
{
	GtkWidget *widget = gtk_window_get_focus(g->window);
	gboolean has_selection;

	if (GTK_IS_EDITABLE(widget)) {
		has_selection = gtk_editable_get_selection_bounds(
				GTK_EDITABLE (widget), NULL, NULL);

		g->buttons[COPY_BUTTON]->sensitivity = has_selection;
		g->buttons[CUT_BUTTON]->sensitivity = has_selection;
		g->buttons[PASTE_BUTTON]->sensitivity = true;
	} else {
		struct browser_window *bw =
				nsgtk_get_browser_window(g->top_level);
		browser_editor_flags edit_f =
				browser_window_get_editor_flags(bw);

		g->buttons[COPY_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_COPY;
		g->buttons[CUT_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_CUT;
		g->buttons[PASTE_BUTTON]->sensitivity =
				edit_f & BW_EDITOR_CAN_PASTE;
	}

	nsgtk_scaffolding_set_sensitivity(g);
	return ((g->buttons[COPY_BUTTON]->sensitivity) |
			(g->buttons[CUT_BUTTON]->sensitivity) |
			(g->buttons[PASTE_BUTTON]->sensitivity));
}

static void nsgtk_scaffolding_enable_link_operations_sensitivity(
		struct gtk_scaffolding *g)
{

	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_popup->savelink_menuitem), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_popup->opentab_menuitem), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_popup->openwin_menuitem), TRUE);

	popup_menu_show(g->menu_popup, false, true, false, false, false);

}

static void nsgtk_scaffolding_enable_edit_actions_sensitivity(
		struct gtk_scaffolding *g)
{

	g->buttons[PASTE_BUTTON]->sensitivity = true;
	g->buttons[COPY_BUTTON]->sensitivity = true;
	g->buttons[CUT_BUTTON]->sensitivity = true;
	nsgtk_scaffolding_set_sensitivity(g);

	popup_menu_show(g->menu_popup, false, false, false, true, false);
}

/* signal handling functions for the toolbar, URL bar, and menu bar */
static gboolean nsgtk_window_edit_menu_clicked(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_update_edit_actions_sensitivity(g);

	return TRUE;
}

static gboolean nsgtk_window_edit_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);

	return TRUE;
}

static gboolean nsgtk_window_popup_menu_hidden(GtkWidget *widget,
		struct gtk_scaffolding *g)
{
	nsgtk_scaffolding_enable_link_operations_sensitivity(g);
	nsgtk_scaffolding_enable_edit_actions_sensitivity(g);
	return TRUE;
}

gboolean nsgtk_window_url_activate_event(GtkWidget *widget, gpointer data)
{
	struct gtk_scaffolding *g = data;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	char *urltxt;
	nsurl *url;
	nserror error;

	if (search_is_url(gtk_entry_get_text(GTK_ENTRY(g->url_bar))) == false) {
		urltxt = search_web_from_term(gtk_entry_get_text(GTK_ENTRY(
				g->url_bar)));
	} else {
		urltxt = strdup(gtk_entry_get_text(GTK_ENTRY(g->url_bar)));
	}

	if (urltxt != NULL) {
		error = nsurl_create(urltxt, &url);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		} else {
			browser_window_navigate(bw, url, NULL,
					BW_NAVIGATE_HISTORY, NULL,
					NULL, NULL);
			nsurl_unref(url);
		}
		free(urltxt);
	}

	return TRUE;
}


gboolean nsgtk_window_url_changed(GtkWidget *widget, GdkEventKey *event,
		gpointer data)
{
	const char *prefix;

	prefix = gtk_entry_get_text(GTK_ENTRY(widget));
	nsgtk_completion_update(prefix);

	return TRUE;
}

/**
 * Event handler for popup menu on toolbar.
 */
static gboolean nsgtk_window_tool_bar_clicked(GtkToolbar *toolbar,
		gint x, gint y,	gint button, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;

	/* set visibility for right-click popup menu */
	popup_menu_hide(g->menu_popup, true, true, false, true, false);
	popup_menu_show(g->menu_popup, false, false, false, false, true);

	gtk_menu_popup(g->menu_popup->popup_menu, NULL, NULL, NULL, NULL, 0,
		       gtk_get_current_event_time());

	return TRUE;
}

/**
 * Update the menus when the number of tabs changes.
 */
static void nsgtk_window_tabs_add(GtkNotebook *notebook,
		GtkWidget *page, guint page_num, struct gtk_scaffolding *g)
{
	gboolean visible = gtk_notebook_get_show_tabs(g->notebook);
	g_object_set(g->menu_bar->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g_object_set(g->menu_popup->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g->buttons[NEXTTAB_BUTTON]->sensitivity = visible;
	g->buttons[PREVTAB_BUTTON]->sensitivity = visible;
	g->buttons[CLOSETAB_BUTTON]->sensitivity = visible;
	nsgtk_scaffolding_set_sensitivity(g);
}

/**
 * Update the menus when the number of tabs changes.
 */
static void
nsgtk_window_tabs_remove(GtkNotebook *notebook,
			 GtkWidget *page,
			 guint page_num,
			 struct gtk_scaffolding *gs)
{
	/* if the scaffold is being destroyed it is not useful to
	 * update the state, futher many of the widgets may have
	 * already been destroyed.
	 */
	if (gtk_widget_in_destruction(GTK_WIDGET(gs->window)) == TRUE) {
		return;
	}

	if (gtk_notebook_get_n_pages(notebook) == 1) {
		nsgtk_scaffolding_destroy(gs);
		return;
	}

	gboolean visible = gtk_notebook_get_show_tabs(gs->notebook);
	g_object_set(gs->menu_bar->view_submenu->tabs_menuitem, "visible", visible, NULL);
	g_object_set(gs->menu_popup->view_submenu->tabs_menuitem, "visible", visible, NULL);
	gs->buttons[NEXTTAB_BUTTON]->sensitivity = visible;
	gs->buttons[PREVTAB_BUTTON]->sensitivity = visible;
	gs->buttons[CLOSETAB_BUTTON]->sensitivity = visible;
	nsgtk_scaffolding_set_sensitivity(gs);
}

/**
 * Handle opening a file path.
 */
static void nsgtk_openfile_open(const char *filename)
{
	struct browser_window *bw;
	char *urltxt;
	nsurl *url;
	nserror error;

	bw = nsgtk_get_browser_window(current_model->top_level);

	urltxt = malloc(strlen(filename) + FILE_SCHEME_PREFIX_LEN + 1);

	if (urltxt != NULL) {
		sprintf(urltxt, FILE_SCHEME_PREFIX"%s", filename);

		error = nsurl_create(urltxt, &url);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		} else {
			browser_window_navigate(bw,
						url,
						NULL,
						BW_NAVIGATE_HISTORY,
						NULL,
						NULL,
						NULL);
			nsurl_unref(url);
		}
		free(urltxt);
	}
}

/* signal handlers for menu entries */

MULTIHANDLER(newwindow)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	const char *addr;
	nsurl *url;
	nserror error;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      bw,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	return TRUE;
}

nserror nsgtk_scaffolding_new_tab(struct gui_window *gw)
{
	struct browser_window *bw = nsgtk_get_browser_window(gw);
	nsurl *url = NULL;
	nserror error;

	if (!nsoption_bool(new_blank)) {
		const char *addr;
		if (nsoption_charp(homepage_url) != NULL) {
			addr = nsoption_charp(homepage_url);
		} else {
			addr = NETSURF_HOMEPAGE;
		}
		error = nsurl_create(addr, &url);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
	}

	error = browser_window_create(BW_CREATE_HISTORY |
				      BW_CREATE_TAB,
				      url,
				      NULL,
				      bw,
				      NULL);
	if (url != NULL) {
		nsurl_unref(url);
	}
	return error;
}

MULTIHANDLER(newtab)
{
	nserror error;

	error = nsgtk_scaffolding_new_tab(g->top_level);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
	return TRUE;
}

MULTIHANDLER(openfile)
{
	current_model = g;
	GtkWidget *dlgOpen = gtk_file_chooser_dialog_new("Open File",
			current_model->window, GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, -6, GTK_STOCK_OPEN, -5, NULL);

	gint response = gtk_dialog_run(GTK_DIALOG(dlgOpen));
	if (response == GTK_RESPONSE_OK) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(dlgOpen));

		nsgtk_openfile_open((const char *) filename);

		g_free(filename);
	}

	gtk_widget_destroy(dlgOpen);
	return TRUE;
}

static gboolean nsgtk_filter_directory(const GtkFileFilterInfo *info,
		gpointer data)
{
	DIR *d = opendir(info->filename);
	if (d == NULL)
		return FALSE;
	closedir(d);
	return TRUE;
}

MULTIHANDLER(savepage)
{
	if (!browser_window_has_content(nsgtk_get_browser_window(g->top_level)))
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkcompleteSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	DIR *d;
	char *path;
	url_func_result res;
	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Directories");
	gtk_file_filter_add_custom(filter, GTK_FILE_FILTER_FILENAME,
			nsgtk_filter_directory, NULL, NULL);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(fc), filter);

	res = url_nice(nsurl_access(browser_window_get_url(
			nsgtk_get_browser_window(g->top_level))), &path, false);
	if (res != URL_FUNC_OK) {
		path = strdup(messages_get("SaveText"));
		if (path == NULL) {
			warn_user("NoMemory", 0);
			return FALSE;
		}
	}

	if (access(path, F_OK) != 0)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), path);
	free(path);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	if (gtk_dialog_run(GTK_DIALOG(fc)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(fc);
		return TRUE;
	}

	path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	d = opendir(path);
	if (d == NULL) {
 		LOG(("Unable to open directory %s for complete save: %s", path,
		     strerror(errno)));
		if (errno == ENOTDIR)
			warn_user("NoDirError", path);
		else
			warn_user("gtkFileError", path);
		gtk_widget_destroy(fc);
		g_free(path);
		return TRUE;
	}
	closedir(d);
	save_complete(nsgtk_get_browser_window(
			g->top_level)->current_content, path, NULL);
	g_free(path);

	gtk_widget_destroy(fc);

	return TRUE;
}


MULTIHANDLER(pdf)
{
#ifdef WITH_PDF_EXPORT

	GtkWidget *save_dialog;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	struct print_settings *settings;
	char filename[PATH_MAX];
	char dirname[PATH_MAX];
	char *url_name;
	url_func_result res;

	LOG(("Print preview (generating PDF)  started."));

	res = url_nice(nsurl_access(browser_window_get_url(bw)),
			&url_name, true);
	if (res != URL_FUNC_OK) {
		warn_user(messages_get(res == URL_FUNC_NOMEM ? "NoMemory"
							     : "URIError"), 0);
		return TRUE;
	}

	strncpy(filename, url_name, PATH_MAX);
	strncat(filename, ".pdf", PATH_MAX - strlen(filename));
	filename[PATH_MAX - 1] = '\0';

	free(url_name);

	strncpy(dirname, option_downloads_directory, PATH_MAX);
	strncat(dirname, "/", PATH_MAX - strlen(dirname));
	dirname[PATH_MAX - 1] = '\0';

	/* this way the scale used by PDF functions is synchronized with that
	 * used by the all-purpose print interface
	 */
	haru_nsfont_set_scale((float)option_export_scale / 100);

	save_dialog = gtk_file_chooser_dialog_new("Export to PDF", g->window,
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			dirname);

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			filename);

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));

		settings = print_make_settings(PRINT_OPTIONS,
				(const char *) filename, &haru_nsfont);
		g_free(filename);

		if (settings == NULL) {
			warn_user(messages_get("NoMemory"), 0);
			gtk_widget_destroy(save_dialog);
			return TRUE;
		}

		/* This will clean up the print_settings object for us */
		print_basic_run(bw->current_content, &pdf_printer, settings);
	}

	gtk_widget_destroy(save_dialog);

#endif /* WITH_PDF_EXPORT */

	return TRUE;
}

MULTIHANDLER(plaintext)
{
	if (!browser_window_has_content(nsgtk_get_browser_window(g->top_level)))
		return FALSE;

	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkplainSave"), g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	char *filename;
	url_func_result res;

	res = url_nice(nsurl_access(browser_window_get_url(
			nsgtk_get_browser_window(g->top_level))),
			&filename, false);
	if (res != URL_FUNC_OK) {
		filename = strdup(messages_get("SaveText"));
		if (filename == NULL) {
			warn_user("NoMemory", 0);
			return FALSE;
		}
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), filename);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
			TRUE);

	free(filename);

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		save_as_text(nsgtk_get_browser_window(
				g->top_level)->current_content, filename);
		g_free(filename);
	}

	gtk_widget_destroy(fc);
	return TRUE;
}

MULTIHANDLER(drawfile)
{
	return TRUE;
}

MULTIHANDLER(postscript)
{
	return TRUE;
}

MULTIHANDLER(printpreview)
{
	return TRUE;
}


MULTIHANDLER(print)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	GtkPrintOperation *print_op;
	GtkPageSetup *page_setup;
	GtkPrintSettings *gtk_print_settings;
	GtkPrintOperationResult res = GTK_PRINT_OPERATION_RESULT_ERROR;
	struct print_settings *settings;

	print_op = gtk_print_operation_new();
	if (print_op == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return TRUE;
	}

	/* use previously saved settings if any */
	gtk_print_settings = gtk_print_settings_new_from_file(
			print_options_file_location, NULL);
	if (gtk_print_settings != NULL) {
		gtk_print_operation_set_print_settings(print_op,
				gtk_print_settings);

		/* We're not interested in the settings any more */
		g_object_unref(gtk_print_settings);
	}

	content_to_print = bw->current_content;

	page_setup = gtk_print_run_page_setup_dialog(g->window, NULL, NULL);
	if (page_setup == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		g_object_unref(print_op);
		return TRUE;
	}
	gtk_print_operation_set_default_page_setup(print_op, page_setup);

	settings = print_make_settings(PRINT_DEFAULT, NULL, &nsfont);

	g_signal_connect(print_op, "begin_print",
			G_CALLBACK(gtk_print_signal_begin_print), settings);
	g_signal_connect(print_op, "draw_page",
			G_CALLBACK(gtk_print_signal_draw_page), NULL);
	g_signal_connect(print_op, "end_print",
			G_CALLBACK(gtk_print_signal_end_print), settings);
	if (content_get_type(bw->current_content) != CONTENT_TEXTPLAIN)
		res = gtk_print_operation_run(print_op,
				GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
    				g->window,
				NULL);

	/* if the settings were used save them for future use */
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		/* Don't ref the settings, as we don't want to own them */
		gtk_print_settings = gtk_print_operation_get_print_settings(
				print_op);

		gtk_print_settings_to_file(gtk_print_settings,
				print_options_file_location, NULL);
	}

	/* Our print_settings object is destroyed by the end print handler */
	g_object_unref(page_setup);
	g_object_unref(print_op);

	return TRUE;
}

MULTIHANDLER(closewindow)
{
	gtk_widget_destroy(GTK_WIDGET(g->window));
	return TRUE;
}

MULTIHANDLER(quit)
{
	if (nsgtk_check_for_downloads(g->window) == false)
		netsurf_quit = true;
	return TRUE;
}

MENUHANDLER(savelink)
{
	nsurl *url;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);

	if (current_menu_ctx.link_url == NULL)
		return FALSE;

	if (nsurl_create(current_menu_ctx.link_url, &url) == NSERROR_OK) {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_DOWNLOAD,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

/**
 * Handler for opening new window from a link. attached to the popup menu.
 */
MENUHANDLER(link_openwin)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);
	nsurl *url;
	nserror error;

	if (current_menu_ctx.link_url == NULL)
		return FALSE;

	error = nsurl_create(current_menu_ctx.link_url, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(
				BW_CREATE_CLONE | BW_CREATE_HISTORY,
				url, NULL, bw, NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	return TRUE;
}

/**
 * Handler for opening new tab from a link. attached to the popup menu.
 */
MENUHANDLER(link_opentab)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *) data;
	struct gui_window *gui = g->top_level;
	struct browser_window *bw = nsgtk_get_browser_window(gui);
	nsurl *url;
	nserror error;

	if (current_menu_ctx.link_url == NULL)
		return FALSE;

	temp_open_background = 1;

	error = nsurl_create(current_menu_ctx.link_url, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_CLONE |
				BW_CREATE_HISTORY | BW_CREATE_TAB,
				url, NULL, bw, NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}

	temp_open_background = -1;

	return TRUE;
}


MULTIHANDLER(cut)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_cut_clipboard (GTK_EDITABLE(g->url_bar));
	else
		browser_window_key_press(bw, KEY_CUT_SELECTION);

	return TRUE;
}

MULTIHANDLER(copy)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_copy_clipboard(GTK_EDITABLE(g->url_bar));
	else
		browser_window_key_press(bw, KEY_COPY_SELECTION);

	return TRUE;
}

MULTIHANDLER(paste)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget *focused = gtk_window_get_focus(g->window);

	/* If the url bar has focus, let gtk handle it */
	if (GTK_IS_EDITABLE (focused))
		gtk_editable_paste_clipboard (GTK_EDITABLE (focused));
	else
		browser_window_key_press(bw, KEY_PASTE);

	return TRUE;
}

MULTIHANDLER(delete)
{
	return TRUE;
}

MENUHANDLER(customize)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	nsgtk_toolbar_customization_init(g);
	return TRUE;
}

MULTIHANDLER(selectall)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	if (nsgtk_widget_has_focus(GTK_WIDGET(g->url_bar))) {
		LOG(("Selecting all URL bar text"));
		gtk_editable_select_region(GTK_EDITABLE(g->url_bar), 0, -1);
	} else {
		LOG(("Selecting all document text"));
		browser_window_key_press(bw, KEY_SELECT_ALL);
	}

	return TRUE;
}

MULTIHANDLER(find)
{
	nsgtk_scaffolding_toggle_search_bar_visibility(g);
	return TRUE;
}

MULTIHANDLER(preferences)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	GtkWidget* wndpreferences;

	wndpreferences = nsgtk_preferences(bw, g->window);
	if (wndpreferences != NULL) {
		gtk_widget_show(GTK_WIDGET(wndpreferences));
	}

	return TRUE;
}

MULTIHANDLER(zoomplus)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	float old_scale = nsgtk_get_scale_for_gui(g->top_level);

	browser_window_set_scale(bw, old_scale + 0.05, true);

	return TRUE;
}

MULTIHANDLER(zoomnormal)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	browser_window_set_scale(bw, 1.0, true);

	return TRUE;
}

MULTIHANDLER(zoomminus)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	float old_scale = nsgtk_get_scale_for_gui(g->top_level);

	browser_window_set_scale(bw, old_scale - 0.05, true);

	return TRUE;
}

MULTIHANDLER(fullscreen)
{
	if (g->fullscreen) {
		gtk_window_unfullscreen(g->window);
	} else {
		gtk_window_fullscreen(g->window);
	}

	g->fullscreen = !g->fullscreen;

	return TRUE;
}

MULTIHANDLER(viewsource)
{
	nsgtk_source_dialog_init(g->window,
			nsgtk_get_browser_window(g->top_level));
	return TRUE;
}

MENUHANDLER(menubar)
{
	GtkWidget *w;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;

	/* if the menubar is not being shown the popup menu shows the
	 * menubar entries instead.
	 */
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		/* need to synchronise menus as gtk grumbles when one menu
		 * is attached to both headers */
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		gtk_widget_show(GTK_WIDGET(g->menu_bar->bar_menu));

		popup_menu_show(g->menu_popup, false, true, true, true, true);
		popup_menu_hide(g->menu_popup, true, false, false, false, false);
	} else {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->menubar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);

		gtk_widget_hide(GTK_WIDGET(g->menu_bar->bar_menu));

		popup_menu_show(g->menu_popup, true, true, true, true, true);

	}
	return TRUE;
}

MENUHANDLER(toolbar)
{
	GtkWidget *w;
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget))) {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);

		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w))
				== FALSE)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					TRUE);
		gtk_widget_show(GTK_WIDGET(g->tool_bar));
	} else {
		w = GTK_WIDGET(g->menu_popup->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		w = GTK_WIDGET(g->menu_bar->view_submenu->toolbars_submenu->toolbar_menuitem);
		if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)))
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),
					FALSE);
		gtk_widget_hide(GTK_WIDGET(g->tool_bar));
	}

	return TRUE;
}

MULTIHANDLER(downloads)
{
	nsgtk_download_show(g->window);

	return TRUE;
}

MULTIHANDLER(savewindowsize)
{
	int x,y,w,h;

	gtk_window_get_position(g->window, &x, &y);
	gtk_window_get_size(g->window, &w, &h);

	nsoption_set_int(window_width, w);
	nsoption_set_int(window_height, h);
	nsoption_set_int(window_x, x);
	nsoption_set_int(window_y, y);

	nsoption_write(options_file_location, NULL, NULL);

	return TRUE;
}

MULTIHANDLER(toggledebugging)
{
	html_redraw_debug = !html_redraw_debug;
	nsgtk_reflow_all_windows();
	return TRUE;
}

MULTIHANDLER(saveboxtree)
{
	GtkWidget *save_dialog;

	save_dialog = gtk_file_chooser_dialog_new("Save File", g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"boxtree.txt");

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		FILE *fh;

		LOG(("Saving box tree dump to %s...\n", filename));

		fh = fopen((const char *) filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = nsgtk_get_browser_window(g->top_level);

			browser_window_debug_dump(bw, fh);

			fclose(fh);
		}

		g_free(filename);
	}

	gtk_widget_destroy(save_dialog);

	return TRUE;
}

MULTIHANDLER(savedomtree)
{
	GtkWidget *save_dialog;

	save_dialog = gtk_file_chooser_dialog_new("Save File", g->window,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(save_dialog),
			getenv("HOME") ? getenv("HOME") : "/");

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_dialog),
			"domtree.txt");

	if (gtk_dialog_run(GTK_DIALOG(save_dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename(
				GTK_FILE_CHOOSER(save_dialog));
		FILE *fh;
		LOG(("Saving dom tree to %s...\n", filename));

		fh = fopen((const char *) filename, "w");
		if (fh == NULL) {
			warn_user("Error saving box tree dump.",
				"Unable to open file for writing.");
		} else {
			struct browser_window *bw;
			bw = nsgtk_get_browser_window(g->top_level);

			if (bw->current_content &&
					content_get_type(bw->current_content) ==
					CONTENT_HTML) {
#ifdef FIXME
				xmlDebugDumpDocument(fh,
					html_get_document(bw->current_content));
#endif
			}

			fclose(fh);
		}

		g_free(filename);
	}

	gtk_widget_destroy(save_dialog);

	return TRUE;
}


MULTIHANDLER(stop)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	browser_window_stop(bw);

	return TRUE;
}

MULTIHANDLER(reload)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);
	if (bw == NULL)
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_reload(bw, true);

	return TRUE;
}

MULTIHANDLER(back)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	if ((bw == NULL) || (!browser_window_history_back_available(bw)))
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_history_back(bw, false);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

MULTIHANDLER(forward)
{
	struct browser_window *bw =
			nsgtk_get_browser_window(g->top_level);

	if ((bw == NULL) || (!browser_window_history_forward_available(bw)))
		return TRUE;

	/* clear potential search effects */
	browser_window_search_clear(bw);

	browser_window_history_forward(bw, false);
	nsgtk_window_update_back_forward(g);

	return TRUE;
}

MULTIHANDLER(home)
{
	static const char *addr = NETSURF_HOMEPAGE;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;
	nserror error;

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	}

	error = nsurl_create(addr, &url);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(localhistory)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	int x,y, width, height, mainwidth, mainheight, margin = 20;
	/* if entries of the same url but different frag_ids have been added
	 * the history needs redrawing (what throbber code normally does)
	 */
	browser_window_history_size(bw, &width, &height);
	nsgtk_window_update_back_forward(g);
	gtk_window_get_position(g->window, &x, &y);
	gtk_window_get_size(g->window, &mainwidth, &mainheight);
	width = (width + g->historybase + margin > mainwidth) ?
			mainwidth - g->historybase : width + margin;
	height = (height + g->toolbarbase + margin > mainheight) ?
			mainheight - g->toolbarbase : height + margin;
	gtk_window_set_default_size(g->history_window->window, width, height);
	gtk_widget_set_size_request(GTK_WIDGET(g->history_window->window),
			-1, -1);
	gtk_window_resize(g->history_window->window, width, height);
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_opacity(g->history_window->window, 0.9);
	gtk_widget_show(GTK_WIDGET(g->history_window->window));
	gtk_window_move(g->history_window->window, x + g->historybase, y +
			g->toolbarbase);
	gdk_window_raise(nsgtk_widget_get_window(GTK_WIDGET(g->history_window->window)));

	return TRUE;
}

MULTIHANDLER(globalhistory)
{
	gtk_widget_show(GTK_WIDGET(wndHistory));
	gdk_window_raise(nsgtk_widget_get_window(GTK_WIDGET(wndHistory)));

	return TRUE;
}

MULTIHANDLER(addbookmarks)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	if (bw == NULL || !browser_window_has_content(bw))
		return TRUE;
	hotlist_add_url(browser_window_get_url(bw));
	return TRUE;
}

MULTIHANDLER(showbookmarks)
{
	gtk_widget_show(GTK_WIDGET(wndHotlist));
	gdk_window_raise(nsgtk_widget_get_window(GTK_WIDGET(wndHotlist)));
	gtk_window_set_focus(wndHotlist, NULL);

	return TRUE;
}

MULTIHANDLER(showcookies)
{
	gtk_widget_show(GTK_WIDGET(wndCookies));
	gdk_window_raise(nsgtk_widget_get_window(GTK_WIDGET(wndCookies)));

	return TRUE;
}

MULTIHANDLER(openlocation)
{
	gtk_widget_grab_focus(GTK_WIDGET(g->url_bar));
	return TRUE;
}

MULTIHANDLER(nexttab)
{
	nsgtk_tab_next(g->notebook);

	return TRUE;
}

MULTIHANDLER(prevtab)
{

	nsgtk_tab_prev(g->notebook);

	return TRUE;
}

MULTIHANDLER(closetab)
{
	nsgtk_tab_close_current(g->notebook);

	return TRUE;
}

MULTIHANDLER(contents)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;
	nserror error;

	error = nsurl_create("http://www.netsurf-browser.org/documentation/", &url);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(guide)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;

	if (nsurl_create("http://www.netsurf-browser.org/documentation/guide", &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(info)
{
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);
	nsurl *url;

	if (nsurl_create("http://www.netsurf-browser.org/documentation/info", &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
		nsurl_unref(url);
	}

	return TRUE;
}

MULTIHANDLER(about)
{
	nsgtk_about_dialog_init(g->window,
			nsgtk_get_browser_window(g->top_level),
			netsurf_version);
	return TRUE;
}

BUTTONHANDLER(history)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	return nsgtk_on_localhistory_activate(g);
}

#undef MULTIHANDLER
#undef CHECKHANDLER
#undef BUTTONHANDLER

#if GTK_CHECK_VERSION(3,0,0)

static gboolean
nsgtk_history_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct rect clip;
	struct gtk_history_window *hw = (struct gtk_history_window *)data;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};
	double x1;
	double y1;
	double x2;
	double y2;

	current_widget = widget;
	current_cr = cr;

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

	clip.x0 = x1;
	clip.y0 = y1;
	clip.x1 = x2;
	clip.y1 = y2;

	ctx.plot->clip(&clip);

	browser_window_history_redraw(bw, &ctx);

	current_widget = NULL;

	return FALSE;
}
#else

/* signal handler functions for the local history window */
static gboolean
nsgtk_history_draw_event(GtkWidget *widget, GdkEventExpose *event, gpointer g)
{
	struct rect clip;
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	current_widget = widget;

	current_cr = gdk_cairo_create(nsgtk_widget_get_window(widget));

	clip.x0 = event->area.x;
	clip.y0 = event->area.y;
	clip.x1 = event->area.x + event->area.width;
	clip.y1 = event->area.y + event->area.height;
	ctx.plot->clip(&clip);

	browser_window_history_redraw(bw, &ctx);

	cairo_destroy(current_cr);

	current_widget = NULL;

	return FALSE;
}

#endif /* GTK_CHECK_VERSION(3,0,0) */

static gboolean nsgtk_history_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer g)
{
	struct gtk_history_window *hw = (struct gtk_history_window *)g;
	struct browser_window *bw =
			nsgtk_get_browser_window(hw->g->top_level);

	LOG(("X=%g, Y=%g", event->x, event->y));

	browser_window_history_click(bw, event->x, event->y, false);

	return TRUE;
}



static void nsgtk_attach_menu_handlers(struct gtk_scaffolding *g)
{
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (g->buttons[i]->main != NULL) {
			g_signal_connect(g->buttons[i]->main, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
		}
		if (g->buttons[i]->rclick != NULL) {
			g_signal_connect(g->buttons[i]->rclick, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
		}
		if (g->buttons[i]->popup != NULL) {
			g_signal_connect(g->buttons[i]->popup, "activate",
					G_CALLBACK(g->buttons[i]->mhandler), g);
		}
	}
#define CONNECT_CHECK(q)\
	g_signal_connect(g->menu_bar->view_submenu->toolbars_submenu->q##_menuitem, "toggled", G_CALLBACK(nsgtk_on_##q##_activate_menu), g);\
	g_signal_connect(g->menu_popup->view_submenu->toolbars_submenu->q##_menuitem, "toggled", G_CALLBACK(nsgtk_on_##q##_activate_menu), g)
	CONNECT_CHECK(menubar);
	CONNECT_CHECK(toolbar);
#undef CONNECT_CHECK

}

/**
 * Create and connect handlers to popup menu.
 *
 * \param g scaffoliding to attach popup menu to.
 * \return true on success or false on error.
 */
static bool nsgtk_new_scaffolding_popup(struct gtk_scaffolding *g, GtkAccelGroup *group)
{
	struct nsgtk_popup_submenu *nmenu;

	nmenu = nsgtk_menu_popup_create(group);

	if (nmenu == NULL)
		return false;

/** Connect a GTK signal handler to a widget */
#define SIG_CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	SIG_CONNECT(nmenu->popup_menu, "hide",
		    nsgtk_window_popup_menu_hidden, g);

	g_signal_connect(nmenu->savelink_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_savelink_activate_menu), g);

	g_signal_connect(nmenu->opentab_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_opentab_activate_menu), g);

	g_signal_connect(nmenu->openwin_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_link_openwin_activate_menu), g);

	g_signal_connect(nmenu->cut_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_cut_activate_menu), g);

	g_signal_connect(nmenu->copy_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_copy_activate_menu), g);

	g_signal_connect(nmenu->paste_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_paste_activate_menu), g);

	g_signal_connect(nmenu->customize_menuitem, "activate",
			 G_CALLBACK(nsgtk_on_customize_activate_menu), g);

	/* set initial popup menu visibility */
	popup_menu_hide(nmenu, true, false, false, false, true);

	g->menu_popup = nmenu;

	return true;
}

nsgtk_scaffolding *nsgtk_new_scaffolding(struct gui_window *toplevel)
{
	struct gtk_scaffolding *g;
	char *searchname;
	int i;
	GtkAccelGroup *group;
	GError* error = NULL;

	g = malloc(sizeof(*g));
	if (g == NULL) {
		return NULL;
	}

	LOG(("Constructing a scaffold of %p for gui_window %p", g, toplevel));

	g->top_level = toplevel;

	/* load the window template from the glade xml file, and extract
	 * widget references from it for later use.
	 */
	g->xml = gtk_builder_new();
	if (!gtk_builder_add_from_file(g->xml, glade_file_location->netsurf, &error)) {
		g_warning("Couldn't load builder file: \"%s\"", error->message);
		g_error_free(error);
		free(g);
		return NULL;
	}

	gtk_builder_connect_signals(g->xml, NULL);

/** Obtain a GTK widget handle from glade xml object */
#define GET_WIDGET(x) GTK_WIDGET (gtk_builder_get_object(g->xml, (x)))

	g->window = GTK_WINDOW(GET_WIDGET("wndBrowser"));
	g->notebook = GTK_NOTEBOOK(GET_WIDGET("notebook"));
	g->tool_bar = GTK_TOOLBAR(GET_WIDGET("toolbar"));

	g->search = malloc(sizeof(struct gtk_search));
	if (g->search == NULL) {
		free(g);
		return NULL;
	}

	g->search->bar = GTK_TOOLBAR(GET_WIDGET("searchbar"));
	g->search->entry = GTK_ENTRY(GET_WIDGET("searchEntry"));

	g->search->buttons[0] = GTK_TOOL_BUTTON(GET_WIDGET("searchBackButton"));
	g->search->buttons[1] = GTK_TOOL_BUTTON(GET_WIDGET(
			"searchForwardButton"));
	g->search->buttons[2] = GTK_TOOL_BUTTON(GET_WIDGET(
			"closeSearchButton"));
	g->search->checkAll = GTK_CHECK_BUTTON(GET_WIDGET("checkAllSearch"));
	g->search->caseSens = GTK_CHECK_BUTTON(GET_WIDGET("caseSensButton"));

#undef GET_WIDGET

	/* allocate buttons */
	for (i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		g->buttons[i] = calloc(1, sizeof(struct nsgtk_button_connect));
		if (g->buttons[i] == NULL) {
			for (i-- ; i >= BACK_BUTTON; i--) {
				free(g->buttons[i]);
			}
			free(g);
			return NULL;
		}
		g->buttons[i]->location = -1;
		g->buttons[i]->sensitivity = true;
	}

	/* here custom toolbutton adding code */
	g->offset = 0;
	g->toolbarmem = 0;
	g->toolbarbase = 0;
	g->historybase = 0;
	nsgtk_toolbar_customization_load(g);
	nsgtk_toolbar_set_physical(g);

	group = gtk_accel_group_new();
	gtk_window_add_accel_group(g->window, group);

	g->menu_bar = nsgtk_menu_bar_create(GTK_MENU_SHELL(gtk_builder_get_object(g->xml, "menubar")), group);


	/* set this window's size and position to what's in the options, or
	 * or some sensible default if they're not set yet.
	 */
	if (nsoption_int(window_width) > 0) {
		gtk_window_move(g->window,
				nsoption_int(window_x),
				nsoption_int(window_y));
		gtk_window_resize(g->window,
				  nsoption_int(window_width),
				  nsoption_int(window_height));
	} else {
		/* Set to 1000x700, so we're very likely to fit even on
		 * 1024x768 displays, not being able to take into account
		 * window furniture or panels.
		 */
		gtk_window_set_default_size(g->window, 1000, 700);
	}

	/* Default toolbar button type uses system defaults */
	if (nsoption_int(button_type) == 0) {
		GtkSettings *settings = gtk_settings_get_default();
		GtkIconSize tooliconsize;
		GtkToolbarStyle toolbarstyle;

		g_object_get(settings,
			     "gtk-toolbar-icon-size", &tooliconsize,
			     "gtk-toolbar-style", &toolbarstyle, NULL);

		switch (toolbarstyle) {
		case GTK_TOOLBAR_ICONS:
			if (tooliconsize == GTK_ICON_SIZE_SMALL_TOOLBAR) {
				nsoption_set_int(button_type, 1);
			} else {
				nsoption_set_int(button_type, 2);
			}
			break;

		case GTK_TOOLBAR_TEXT:
			nsoption_set_int(button_type, 4);
			break;

		case GTK_TOOLBAR_BOTH:
		case GTK_TOOLBAR_BOTH_HORIZ:
			/* no labels in default configuration */
		default:
			/* No system default, so use large icons */
			nsoption_set_int(button_type, 2);
			break;
		}
	}

	switch (nsoption_int(button_type)) {
	/* case 0 is 'unset' [from fresh install / clearing options]
	 * see above */

	case 1: /* Small icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_SMALL_TOOLBAR);
		break;
	case 2: /* Large icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_ICONS);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;
	case 3: /* Large icons with text */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_BOTH);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(g->tool_bar),
					  GTK_ICON_SIZE_LARGE_TOOLBAR);
		break;
	case 4: /* Text icons only */
		gtk_toolbar_set_style(GTK_TOOLBAR(g->tool_bar),
				      GTK_TOOLBAR_TEXT);
	default:
		break;
	}

	gtk_toolbar_set_show_arrow(g->tool_bar, TRUE);
	gtk_widget_show_all(GTK_WIDGET(g->tool_bar));
	nsgtk_tab_init(g);

	gtk_widget_set_size_request(GTK_WIDGET(
			g->buttons[HISTORY_BUTTON]->button), 20, -1);

	/* create the local history window to be associated with this browser */
	g->history_window = malloc(sizeof(struct gtk_history_window));
	g->history_window->g = g;
	g->history_window->window =
			GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	gtk_window_set_transient_for(g->history_window->window, g->window);
	gtk_window_set_title(g->history_window->window, "NetSurf History");
	gtk_window_set_type_hint(g->history_window->window,
			GDK_WINDOW_TYPE_HINT_UTILITY);
	g->history_window->scrolled =
			GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
	gtk_container_add(GTK_CONTAINER(g->history_window->window),
			GTK_WIDGET(g->history_window->scrolled));

	gtk_widget_show(GTK_WIDGET(g->history_window->scrolled));
	g->history_window->drawing_area =
			GTK_DRAWING_AREA(gtk_drawing_area_new());

	gtk_widget_set_events(GTK_WIDGET(g->history_window->drawing_area),
			GDK_EXPOSURE_MASK |
			GDK_POINTER_MOTION_MASK |
			GDK_BUTTON_PRESS_MASK);
	nsgtk_widget_override_background_color(GTK_WIDGET(g->history_window->drawing_area),
			GTK_STATE_NORMAL,
			0, 0xffff, 0xffff, 0xffff);
	gtk_scrolled_window_add_with_viewport(g->history_window->scrolled,
			GTK_WIDGET(g->history_window->drawing_area));
	gtk_widget_show(GTK_WIDGET(g->history_window->drawing_area));


	/* set up URL bar completion */
	g->url_bar_completion = gtk_entry_completion_new();
	gtk_entry_completion_set_match_func(g->url_bar_completion,
			nsgtk_completion_match, NULL, NULL);
	gtk_entry_completion_set_model(g->url_bar_completion,
			GTK_TREE_MODEL(nsgtk_completion_list));
	gtk_entry_completion_set_text_column(g->url_bar_completion, 0);
	gtk_entry_completion_set_minimum_key_length(g->url_bar_completion, 1);
	gtk_entry_completion_set_popup_completion(g->url_bar_completion, TRUE);
	g_object_set(G_OBJECT(g->url_bar_completion),
			"popup-set-width", TRUE,
			"popup-single-match", TRUE,
			NULL);

	/* set up the throbber. */
	g->throb_frame = 0;


#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	/* connect history window signals to their handlers */
	nsgtk_connect_draw_event(GTK_WIDGET(g->history_window->drawing_area),
				 G_CALLBACK(nsgtk_history_draw_event),
				 g->history_window);
	/*CONNECT(g->history_window->drawing_area, "motion_notify_event",
			nsgtk_history_motion_notify_event, g->history_window);*/
	CONNECT(g->history_window->drawing_area, "button_press_event",
			nsgtk_history_button_press_event, g->history_window);
	CONNECT(g->history_window->window, "delete_event",
			gtk_widget_hide_on_delete, NULL);

	g_signal_connect_after(g->notebook, "page-added",
			G_CALLBACK(nsgtk_window_tabs_add), g);
	g_signal_connect_after(g->notebook, "page-removed",
			G_CALLBACK(nsgtk_window_tabs_remove), g);

	/* connect main window signals to their handlers. */
	CONNECT(g->window, "delete-event", scaffolding_window_delete_event, g);
	CONNECT(g->window, "destroy", scaffolding_window_destroy, g);

	/* toolbar URL bar menu bar search bar signal handlers */
	CONNECT(g->menu_bar->edit_submenu->edit, "show", nsgtk_window_edit_menu_clicked, g);
	CONNECT(g->menu_bar->edit_submenu->edit, "hide", nsgtk_window_edit_menu_hidden, g);
	CONNECT(g->search->buttons[1], "clicked",
			nsgtk_search_forward_button_clicked, g);
	CONNECT(g->search->buttons[0], "clicked",
			nsgtk_search_back_button_clicked, g);
	CONNECT(g->search->entry, "changed", nsgtk_search_entry_changed, g);
	CONNECT(g->search->entry, "activate", nsgtk_search_entry_activate, g);
	CONNECT(g->search->entry, "key-press-event", nsgtk_search_entry_key, g);
	CONNECT(g->search->buttons[2], "clicked",
			nsgtk_search_close_button_clicked, g);
	CONNECT(g->search->caseSens, "toggled", nsgtk_search_entry_changed, g);

	CONNECT(g->tool_bar, "popup-context-menu",
		nsgtk_window_tool_bar_clicked, g);

	/* create popup menu */
	nsgtk_new_scaffolding_popup(g, group);

	/* set up the menu signal handlers */
	nsgtk_scaffolding_toolbar_init(g);
	nsgtk_toolbar_connect_all(g);
	nsgtk_attach_menu_handlers(g);

	/* prepare to set the web search ico */

	/* init web search prefs from file */
	search_web_provider_details(nsoption_int(search_provider));

	/* potentially retrieve ico */
	if (search_web_ico() == NULL) {
		search_web_retrieve_ico(false);
	}

	/* set entry */
	searchname = search_web_provider_name();
	if (searchname != NULL) {
		char searchcontent[strlen(searchname) + SLEN("Search ")	+ 1];
		sprintf(searchcontent, "Search %s", searchname);
		nsgtk_scaffolding_set_websearch(g, searchcontent);
		free(searchname);
	}

	nsgtk_scaffolding_initial_sensitivity(g);

	g->fullscreen = false;


	/* attach to the list */
	if (scaf_list)
		scaf_list->prev = g;
	g->next = scaf_list;
	g->prev = NULL;
	scaf_list = g;

	/* call functions that need access from the list */
	nsgtk_theme_init();
	nsgtk_theme_implement(g);

	/* set web search ico */
	gui_set_search_ico(search_web_ico());

	/* finally, show the window. */
	gtk_widget_show(GTK_WIDGET(g->window));

	LOG(("creation complete"));

	return g;
}

/** set the title in the window
 *
 * @param gw The gui window to set title on
 * @param title The title to set (may be NULL)
 */
void gui_window_set_title(struct gui_window *gw, const char *title)
{
	struct gtk_scaffolding *gs = nsgtk_get_scaffold(gw);
	int title_len;
	char *newtitle;

	if ((title == NULL) || (title[0] == '\0')) {
		if (gs->top_level != gw) {
			gtk_window_set_title(gs->window, "NetSurf");
		}
		return;
	}

	nsgtk_tab_set_title(gw, title);

	if (gs->top_level != gw) {
		/* not top level window so do not set window title */
		return;
	}

	title_len = strlen(title) + SLEN(" - NetSurf") + 1;
	newtitle = malloc(title_len);
	if (newtitle == NULL) {
		return;
	}

	snprintf(newtitle, title_len, "%s - NetSurf", title);

	gtk_window_set_title(gs->window, newtitle);

	free(newtitle);
}


void gui_window_set_url(struct gui_window *_g, const char *url)
{
	struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	if (g->top_level != _g) return;
	gtk_entry_set_text(GTK_ENTRY(g->url_bar), url);
	gtk_editable_set_position(GTK_EDITABLE(g->url_bar), -1);
}

void gui_window_start_throbber(struct gui_window* _g)
{
	struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	g->buttons[STOP_BUTTON]->sensitivity = true;
	g->buttons[RELOAD_BUTTON]->sensitivity = false;
	nsgtk_scaffolding_set_sensitivity(g);

	nsgtk_window_update_back_forward(g);

	nsgtk_schedule(100, nsgtk_throb, g);
}

void gui_window_stop_throbber(struct gui_window* _g)
{
	struct gtk_scaffolding *g = nsgtk_get_scaffold(_g);
	if (g == NULL)
		return;
	nsgtk_window_update_back_forward(g);
	nsgtk_schedule(-1, nsgtk_throb, g);
	if (g->buttons[STOP_BUTTON] != NULL)
		g->buttons[STOP_BUTTON]->sensitivity = false;
	if (g->buttons[RELOAD_BUTTON] != NULL)
		g->buttons[RELOAD_BUTTON]->sensitivity = true;

	nsgtk_scaffolding_set_sensitivity(g);

	if ((g->throbber == NULL) || (nsgtk_throbber == NULL) ||
			(nsgtk_throbber->framedata == NULL) ||
			(nsgtk_throbber->framedata[0] == NULL))
		return;
	gtk_image_set_from_pixbuf(g->throbber, nsgtk_throbber->framedata[0]);
}


/**
 * set favicon
 */
void
nsgtk_scaffolding_set_icon(struct gui_window *gw)
{
	struct gtk_scaffolding *sc = nsgtk_get_scaffold(gw);
	GdkPixbuf *icon_pixbuf = nsgtk_get_icon(gw);

	/* check icon needs to be shown */
	if ((icon_pixbuf == NULL) ||
	    (sc->top_level != gw)) {
		return;
	}

	nsgtk_entry_set_icon_from_pixbuf(sc->url_bar,
					 GTK_ENTRY_ICON_PRIMARY,
					 icon_pixbuf);

	gtk_widget_show_all(GTK_WIDGET(sc->buttons[URL_BAR_ITEM]->button));
}

void gui_set_search_ico(hlcache_handle *ico)
{
	struct bitmap *srch_bitmap;
	nsgtk_scaffolding *current;
	GdkPixbuf *srch_pixbuf;

	if ((ico == NULL) &&
	    (ico = search_web_ico()) == NULL) {
		return;
	}

	srch_bitmap = content_get_bitmap(ico);
	if (srch_bitmap == NULL) {
		return;
	}

	srch_pixbuf = nsgdk_pixbuf_get_from_surface(srch_bitmap->surface, 16, 16);

	if (srch_pixbuf == NULL) {
		return;
	}

	/* add ico to each window's toolbar */
	for (current = scaf_list; current != NULL; current = current->next) {
		nsgtk_entry_set_icon_from_pixbuf(current->webSearchEntry,
						 GTK_ENTRY_ICON_PRIMARY,
						 srch_pixbuf);
	}

	g_object_unref(srch_pixbuf);
}

bool nsgtk_scaffolding_is_busy(nsgtk_scaffolding *g)
{
	/* We are considered "busy" if the stop button is sensitive */
	return g->buttons[STOP_BUTTON]->sensitivity;
}

GtkWindow* nsgtk_scaffolding_window(nsgtk_scaffolding *g)
{
	return g->window;
}

GtkNotebook* nsgtk_scaffolding_notebook(nsgtk_scaffolding *g)
{
	return g->notebook;
}

GtkWidget *nsgtk_scaffolding_urlbar(nsgtk_scaffolding *g)
{
	return g->url_bar;
}

GtkWidget *nsgtk_scaffolding_websearch(nsgtk_scaffolding *g)
{
	return g->webSearchEntry;
}


GtkToolbar *nsgtk_scaffolding_toolbar(nsgtk_scaffolding *g)
{
	return g->tool_bar;
}

struct nsgtk_button_connect *nsgtk_scaffolding_button(nsgtk_scaffolding *g,
		int i)
{
	return g->buttons[i];
}

struct gtk_search *nsgtk_scaffolding_search(nsgtk_scaffolding *g)
{
	return g->search;
}

GtkMenuBar *nsgtk_scaffolding_menu_bar(nsgtk_scaffolding *g)
{
	return g->menu_bar->bar_menu;
}

struct gtk_history_window *nsgtk_scaffolding_history_window(nsgtk_scaffolding
		*g)
{
	return g->history_window;
}

nsgtk_scaffolding *nsgtk_scaffolding_iterate(nsgtk_scaffolding *g)
{
	return g->next;
}

void nsgtk_scaffolding_reset_offset(nsgtk_scaffolding *g)
{
	g->offset = 0;
}

void nsgtk_scaffolding_update_url_bar_ref(nsgtk_scaffolding *g)
{
	g->url_bar = GTK_WIDGET(gtk_bin_get_child(GTK_BIN(
			nsgtk_scaffolding_button(g, URL_BAR_ITEM)->button)));

	gtk_entry_set_completion(GTK_ENTRY(g->url_bar),
			g->url_bar_completion);
}
void nsgtk_scaffolding_update_throbber_ref(nsgtk_scaffolding *g)
{
	g->throbber = GTK_IMAGE(gtk_bin_get_child(GTK_BIN(gtk_bin_get_child(
			GTK_BIN(g->buttons[THROBBER_ITEM]->button)))));
}

void nsgtk_scaffolding_update_websearch_ref(nsgtk_scaffolding *g)
{
	g->webSearchEntry = gtk_bin_get_child(GTK_BIN(
			g->buttons[WEBSEARCH_ITEM]->button));
}

void nsgtk_scaffolding_set_websearch(nsgtk_scaffolding *g, const char *content)
{
	/* this code appears technically correct, though currently has no
	 * effect at all - tinkering encouraged */
	PangoLayout *lo = gtk_entry_get_layout(GTK_ENTRY(g->webSearchEntry));
	if (lo != NULL) {
		pango_layout_set_font_description(lo, NULL);
		PangoFontDescription *desc = pango_font_description_new();
		if (desc != NULL) {
			pango_font_description_set_style(desc,
					PANGO_STYLE_ITALIC);
			pango_font_description_set_family(desc, "Arial");
			pango_font_description_set_weight(desc,
					PANGO_WEIGHT_ULTRALIGHT);
			pango_font_description_set_size(desc,
					10 * PANGO_SCALE);
			pango_layout_set_font_description(lo, desc);
		}

		PangoAttrList *list = pango_attr_list_new();
		if (list != NULL) {
			PangoAttribute *italic = pango_attr_style_new(
					PANGO_STYLE_ITALIC);
			if (italic != NULL) {
				italic->start_index = 0;
				italic->end_index = strlen(content);
			}
			PangoAttribute *grey = pango_attr_foreground_new(
					0x7777, 0x7777, 0x7777);
			if (grey != NULL) {
				grey->start_index = 0;
				grey->end_index = strlen(content);
			}
			pango_attr_list_insert(list, italic);
			pango_attr_list_insert(list, grey);
			pango_layout_set_attributes(lo, list);
			pango_attr_list_unref(list);
		}
		pango_layout_set_text(lo, content, -1);
	}
/*	an alternative method */
/*	char *parse = malloc(strlen(content) + 1);
	PangoAttrList *list = pango_layout_get_attributes(lo);
	char *markup = g_strconcat("<span foreground='#777777'><i>", content,
			"</i></span>", NULL);
	pango_parse_markup(markup, -1, 0, &list, &parse, NULL, NULL);
	gtk_widget_show_all(g->webSearchEntry);
*/
	gtk_entry_set_visibility(GTK_ENTRY(g->webSearchEntry), TRUE);
	gtk_entry_set_text(GTK_ENTRY(g->webSearchEntry), content);
}

void nsgtk_scaffolding_toggle_search_bar_visibility(nsgtk_scaffolding *g)
{
	gboolean vis;
	struct browser_window *bw = nsgtk_get_browser_window(g->top_level);

	g_object_get(G_OBJECT(g->search->bar), "visible", &vis, NULL);
	if (vis) {
		if (bw != NULL) {
			browser_window_search_clear(bw);
		}

		gtk_widget_hide(GTK_WIDGET(g->search->bar));
	} else {
		gtk_widget_show(GTK_WIDGET(g->search->bar));
		gtk_widget_grab_focus(GTK_WIDGET(g->search->entry));
	}
}


struct gui_window *nsgtk_scaffolding_top_level(nsgtk_scaffolding *g)
{
	return g->top_level;
}

/* set the current active top level gui window */
void nsgtk_scaffolding_set_top_level(struct gui_window *gw)
{
	struct browser_window *bw;
	nsgtk_scaffolding *sc;

	assert(gw != NULL);

	bw = nsgtk_get_browser_window(gw);

	assert(bw != NULL);

	sc = nsgtk_get_scaffold(gw);
	assert(sc != NULL);

	sc->top_level = gw;

	/* Synchronise the history (will also update the URL bar) */
	nsgtk_window_update_back_forward(sc);

	/* clear effects of potential searches */
	browser_window_search_clear(bw);

	nsgtk_scaffolding_set_icon(gw);

	/* Ensure the window's title bar is updated */
	if (bw->current_content != NULL) {
		gui_window_set_title(gw, content_get_title(bw->current_content));
	}
}

/* exported interface documented in scaffolding.h */
void nsgtk_scaffolding_set_sensitivity(struct gtk_scaffolding *g)
{
	int i;
#define SENSITIVITY(q)\
		i = q##_BUTTON;\
		if (g->buttons[i]->main != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->main),\
					g->buttons[i]->sensitivity);\
		if (g->buttons[i]->rclick != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->rclick),\
					g->buttons[i]->sensitivity);\
		if ((g->buttons[i]->location != -1) && \
				(g->buttons[i]->button != NULL))\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->button),\
					g->buttons[i]->sensitivity);\
		if (g->buttons[i]->popup != NULL)\
			gtk_widget_set_sensitive(GTK_WIDGET(\
					g->buttons[i]->popup),\
					g->buttons[i]->sensitivity);

	SENSITIVITY(STOP)
	SENSITIVITY(RELOAD)
	SENSITIVITY(CUT)
	SENSITIVITY(COPY)
	SENSITIVITY(PASTE)
	SENSITIVITY(BACK)
	SENSITIVITY(FORWARD)
	SENSITIVITY(NEXTTAB)
	SENSITIVITY(PREVTAB)
	SENSITIVITY(CLOSETAB)
#undef SENSITIVITY
}

void nsgtk_scaffolding_initial_sensitivity(struct gtk_scaffolding *g)
{
	for (int i = BACK_BUTTON; i < PLACEHOLDER_BUTTON; i++) {
		if (g->buttons[i]->main != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->main),
					g->buttons[i]->sensitivity);
		if (g->buttons[i]->rclick != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->rclick),
					g->buttons[i]->sensitivity);
		if ((g->buttons[i]->location != -1) &&
				(g->buttons[i]->button != NULL))
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->button),
					g->buttons[i]->sensitivity);
		if (g->buttons[i]->popup != NULL)
			gtk_widget_set_sensitive(GTK_WIDGET(
					g->buttons[i]->popup),
					g->buttons[i]->sensitivity);
	}
	gtk_widget_set_sensitive(GTK_WIDGET(g->menu_bar->view_submenu->images_menuitem), FALSE);
}

/**
 * Checks if a location is over a link.
 *
 * Side effect of this function is to set the global current_menu_ctx
 */
static bool is_menu_over_link(struct gtk_scaffolding *g, gdouble x, gdouble y)
{
	browser_window_get_contextual_content(
			nsgtk_get_browser_window(g->top_level),
			x, y, &current_menu_ctx);

	if (current_menu_ctx.link_url == NULL)
		return false;

	return true;
}

void nsgtk_scaffolding_popup_menu(struct gtk_scaffolding *g, gdouble x, gdouble y)
{
	if (is_menu_over_link(g, x, y)) {
		popup_menu_show(g->menu_popup, false, true, false, false, false);
	} else {
		popup_menu_hide(g->menu_popup, false, true, false, false, false);
	}

	nsgtk_scaffolding_update_edit_actions_sensitivity(g);

	if (!(g->buttons[COPY_BUTTON]->sensitivity))
		gtk_widget_hide(GTK_WIDGET(g->menu_popup->copy_menuitem));
	else
		gtk_widget_show(GTK_WIDGET(g->menu_popup->copy_menuitem));

	if (!(g->buttons[CUT_BUTTON]->sensitivity))
		gtk_widget_hide(GTK_WIDGET(g->menu_popup->cut_menuitem));
	else
		gtk_widget_show(GTK_WIDGET(g->menu_popup->cut_menuitem));

	if (!(g->buttons[PASTE_BUTTON]->sensitivity))
		gtk_widget_hide(GTK_WIDGET(g->menu_popup->paste_menuitem));
	else
		gtk_widget_show(GTK_WIDGET(g->menu_popup->paste_menuitem));

	/* hide customize */
	popup_menu_hide(g->menu_popup, false, false, false, false, true);

	gtk_menu_popup(g->menu_popup->popup_menu, NULL, NULL, NULL, NULL, 0,
			gtk_get_current_event_time());
}

/**
 * reallocate width for history button, reallocate buttons right of history;
 * memorise base of history button / toolbar
 */
void nsgtk_scaffolding_toolbar_size_allocate(GtkWidget *widget,
		GtkAllocation *alloc, gpointer data)
{
	struct gtk_scaffolding *g = (struct gtk_scaffolding *)data;
	int i = nsgtk_toolbar_get_id_from_widget(widget, g);
	if (i == -1)
		return;
	if ((g->toolbarmem == alloc->x) ||
			(g->buttons[i]->location <
			g->buttons[HISTORY_BUTTON]->location))
	/* no reallocation after first adjustment, no reallocation for buttons
	 * left of history button */
		return;
	if (widget == GTK_WIDGET(g->buttons[HISTORY_BUTTON]->button)) {
		if (alloc->width == 20)
			return;

		g->toolbarbase = alloc->y + alloc->height;
		g->historybase = alloc->x + 20;
		if (g->offset == 0)
			g->offset = alloc->width - 20;
		alloc->width = 20;
	} else if (g->buttons[i]->location <=
			g->buttons[URL_BAR_ITEM]->location) {
		alloc->x -= g->offset;
		if (i == URL_BAR_ITEM)
			alloc->width += g->offset;
	}
	g->toolbarmem = alloc->x;
	gtk_widget_size_allocate(widget, alloc);
}




/**
 * init the array g->buttons[]
 */
void nsgtk_scaffolding_toolbar_init(struct gtk_scaffolding *g)
{
#define ITEM_MAIN(p, q, r)\
	g->buttons[p##_BUTTON]->main =\
			g->menu_bar->q->r##_menuitem;\
	g->buttons[p##_BUTTON]->rclick =\
			g->menu_popup->q->r##_menuitem;\
	g->buttons[p##_BUTTON]->mhandler =\
			nsgtk_on_##r##_activate_menu;\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##r##_activate_button;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##r##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##r##_toolbar_button_data

#define ITEM_SUB(p, q, r, s)\
	g->buttons[p##_BUTTON]->main =\
			g->menu_bar->q->r##_submenu->s##_menuitem;\
	g->buttons[p##_BUTTON]->rclick =\
			g->menu_popup->q->r##_submenu->s##_menuitem;\
	g->buttons[p##_BUTTON]->mhandler =\
			nsgtk_on_##s##_activate_menu;\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##s##_activate_button;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##s##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##s##_toolbar_button_data

#define ITEM_BUTTON(p, q)\
	g->buttons[p##_BUTTON]->bhandler =\
			nsgtk_on_##q##_activate;\
	g->buttons[p##_BUTTON]->dataplus =\
			nsgtk_toolbar_##q##_button_data;\
	g->buttons[p##_BUTTON]->dataminus =\
			nsgtk_toolbar_##q##_toolbar_button_data

#define ITEM_POP(p, q)					\
	g->buttons[p##_BUTTON]->popup = GTK_IMAGE_MENU_ITEM(\
			g->menu_popup->q##_menuitem)

#define SENSITIVITY(q)				\
	g->buttons[q##_BUTTON]->sensitivity = false

#define ITEM_ITEM(p, q)\
	g->buttons[p##_ITEM]->dataplus =\
			nsgtk_toolbar_##q##_button_data;\
	g->buttons[p##_ITEM]->dataminus =\
			nsgtk_toolbar_##q##_toolbar_button_data
	ITEM_ITEM(WEBSEARCH, websearch);
	ITEM_ITEM(THROBBER, throbber);
	ITEM_MAIN(NEWWINDOW, file_submenu, newwindow);
	ITEM_MAIN(NEWTAB, file_submenu, newtab);
	ITEM_MAIN(OPENFILE, file_submenu, openfile);
	ITEM_MAIN(PRINT, file_submenu, print);
	ITEM_MAIN(CLOSEWINDOW, file_submenu, closewindow);
	ITEM_MAIN(SAVEPAGE, file_submenu, savepage);
	ITEM_MAIN(PRINTPREVIEW, file_submenu, printpreview);
	ITEM_MAIN(PRINT, file_submenu, print);
	ITEM_MAIN(QUIT, file_submenu, quit);
	ITEM_MAIN(CUT, edit_submenu, cut);
	ITEM_MAIN(COPY, edit_submenu, copy);
	ITEM_MAIN(PASTE, edit_submenu, paste);
	ITEM_MAIN(DELETE, edit_submenu, delete);
	ITEM_MAIN(SELECTALL, edit_submenu, selectall);
	ITEM_MAIN(FIND, edit_submenu, find);
	ITEM_MAIN(PREFERENCES, edit_submenu, preferences);
	ITEM_MAIN(STOP, view_submenu, stop);
	ITEM_POP(STOP, stop);
	ITEM_MAIN(RELOAD, view_submenu, reload);
	ITEM_POP(RELOAD, reload);
	ITEM_MAIN(FULLSCREEN, view_submenu, fullscreen);
	ITEM_MAIN(VIEWSOURCE, view_submenu, viewsource);
	ITEM_MAIN(DOWNLOADS, view_submenu, downloads);
	ITEM_MAIN(SAVEWINDOWSIZE, view_submenu, savewindowsize);
	ITEM_MAIN(BACK, nav_submenu, back);
	ITEM_POP(BACK, back);
	ITEM_MAIN(FORWARD, nav_submenu, forward);
	ITEM_POP(FORWARD, forward);
	ITEM_MAIN(HOME, nav_submenu, home);
	ITEM_MAIN(LOCALHISTORY, nav_submenu, localhistory);
	ITEM_MAIN(GLOBALHISTORY, nav_submenu, globalhistory);
	ITEM_MAIN(ADDBOOKMARKS, nav_submenu, addbookmarks);
	ITEM_MAIN(SHOWBOOKMARKS, nav_submenu, showbookmarks);
	ITEM_MAIN(SHOWCOOKIES, nav_submenu, showcookies);
	ITEM_MAIN(OPENLOCATION, nav_submenu, openlocation);
	ITEM_MAIN(CONTENTS, help_submenu, contents);
	ITEM_MAIN(INFO, help_submenu, info);
	ITEM_MAIN(GUIDE, help_submenu, guide);
	ITEM_MAIN(ABOUT, help_submenu, about);
	ITEM_SUB(PLAINTEXT, file_submenu, export, plaintext);
	ITEM_SUB(PDF, file_submenu, export, pdf);
	ITEM_SUB(DRAWFILE, file_submenu, export, drawfile);
	ITEM_SUB(POSTSCRIPT, file_submenu, export, postscript);
	ITEM_SUB(ZOOMPLUS, view_submenu, scaleview, zoomplus);
	ITEM_SUB(ZOOMMINUS, view_submenu, scaleview, zoomminus);
	ITEM_SUB(ZOOMNORMAL, view_submenu, scaleview, zoomnormal);
	ITEM_SUB(NEXTTAB, view_submenu, tabs, nexttab);
	ITEM_SUB(PREVTAB, view_submenu, tabs, prevtab);
	ITEM_SUB(CLOSETAB, view_submenu, tabs, closetab);
	ITEM_SUB(TOGGLEDEBUGGING, view_submenu, debugging, toggledebugging);
	ITEM_SUB(SAVEBOXTREE, view_submenu, debugging, saveboxtree);
	ITEM_SUB(SAVEDOMTREE, view_submenu, debugging, savedomtree);
	ITEM_BUTTON(HISTORY, history);
	/* disable items that make no sense initially, as well as
	 * as-yet-unimplemented items */
	SENSITIVITY(BACK);
	SENSITIVITY(FORWARD);
	SENSITIVITY(STOP);
	SENSITIVITY(PRINTPREVIEW);
	SENSITIVITY(DELETE);
	SENSITIVITY(DRAWFILE);
	SENSITIVITY(POSTSCRIPT);
	SENSITIVITY(NEXTTAB);
	SENSITIVITY(PREVTAB);
	SENSITIVITY(CLOSETAB);
#ifndef WITH_PDF_EXPORT
	SENSITIVITY(PDF);
#endif

#undef ITEM_MAIN
#undef ITEM_SUB
#undef ITEM_BUTTON
#undef ITEM_POP
#undef SENSITIVITY

}
