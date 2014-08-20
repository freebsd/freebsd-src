/*
 * Copyright 2008 Michael Lester <element3260@gmail.com>
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

#include <stdint.h>
#include <string.h>

#include "gtk/compat.h"
#include "gtk/window.h"
#include "gtk/gui.h"
#include "desktop/browser.h"
#include "content/content.h"
#include "utils/nsoption.h"
#include "desktop/search.h"
#include "utils/utils.h"
#include "gtk/search.h"
#include "gtk/tabs.h"

#define TAB_WIDTH_N_CHARS 15

/** callback to update sizes when style-set gtk signal */
static void nsgtk_tab_update_size(GtkWidget *hbox, GtkStyle *previous_style,
		GtkWidget *close_button)
{
	PangoFontMetrics *metrics;
	PangoContext *context;
	int char_width, h, w;
	GtkStyleContext *style;
	GtkStateFlags state;

	state = nsgtk_widget_get_state_flags(hbox);
	style = nsgtk_widget_get_style_context(hbox);

	context = gtk_widget_get_pango_context(hbox);
	metrics = pango_context_get_metrics(context,
				nsgtk_style_context_get_font(style, state),
				pango_context_get_language(context));

	char_width = pango_font_metrics_get_approximate_digit_width(metrics);
	pango_font_metrics_unref(metrics);

	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings (hbox),
			GTK_ICON_SIZE_MENU, &w, &h);

	gtk_widget_set_size_request(hbox,
			TAB_WIDTH_N_CHARS * PANGO_PIXELS(char_width) + 2 * w,
			-1);

	gtk_widget_set_size_request(close_button, w + 4, h + 4);
}

/** Create a notebook tab label */
static GtkWidget *nsgtk_tab_label_setup(struct gui_window *window)
{
	GtkWidget *hbox, *label, *button, *close;

	hbox = nsgtk_hbox_new(FALSE, 2);

	if (nsoption_bool(new_blank) == true)
		label = gtk_label_new("New Tab");
	else
		label = gtk_label_new("Loading...");
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC(label), 0, 0);
	gtk_widget_show(label);

	button = gtk_button_new();

	close = gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU);
	gtk_container_add(GTK_CONTAINER(button), close);
	gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(button, "Close this tab.");

#ifdef FIXME
	GtkRcStyle *rcstyle;
	rcstyle = gtk_rc_style_new();
	rcstyle->xthickness = rcstyle->ythickness = 0;
	gtk_widget_modify_style(button, rcstyle);
	g_object_unref(rcstyle);
#endif

	g_signal_connect_swapped(button, "clicked",
			G_CALLBACK(nsgtk_window_destroy_browser), window);
	g_signal_connect(hbox, "style-set",
			G_CALLBACK(nsgtk_tab_update_size), button);

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(hbox), "label", label);
	g_object_set_data(G_OBJECT(hbox), "close-button", button);


	gtk_widget_show_all(hbox);
	return hbox;
}
#include "utils/log.h"

/** callback when page is switched */

static gint srcpagenum;

/** The switch-page signal handler
 *
 * This signal is handled both before and after delivery to work round
 * issue that setting the selected tab during the switch-page signal
 * fails
 */
static void
nsgtk_tab_switch_page(GtkNotebook *notebook,
		      GtkWidget *page,
		      guint selpagenum,
		      gpointer user_data)
{
	srcpagenum = gtk_notebook_get_current_page(notebook);
}

static void
nsgtk_tab_switch_page_after(GtkNotebook *notebook,
			    GtkWidget *selpage,
			    guint selpagenum,
			    gpointer user_data)
{
	GtkWidget *srcpage;
	GtkWidget *addpage;
	struct gui_window *gw;
	nserror error;

	addpage = g_object_get_data(G_OBJECT(notebook), "addtab");

	if (selpage == addpage) {
		if ((srcpagenum != -1) &&
		    (srcpagenum != (gint)selpagenum)) {
			/* ensure the add tab is not actually selected */
			LOG(("src %d sel %d",srcpagenum,selpagenum ));
			srcpage = gtk_notebook_get_nth_page(notebook, srcpagenum);
			gw = g_object_get_data(G_OBJECT(srcpage), "gui_window");
			if ((gw != NULL) && (nsgtk_get_scaffold(gw) != NULL)) {
				error = nsgtk_scaffolding_new_tab(gw);
			}
		}
	} else {
		LOG(("sel %d", selpagenum ));
		/* tab with page in it */
		gw = g_object_get_data(G_OBJECT(selpage), "gui_window");
		if (gw != NULL) {
			nsgtk_scaffolding_set_top_level(gw);
		}
	}
}

static void nsgtk_tab_page_reordered(GtkNotebook *notebook,
				     GtkWidget *child,
				     guint page_num,
				     gpointer user_data)
{
	gint pages;
	GtkWidget *addpage;

	pages = gtk_notebook_get_n_pages(notebook);
	addpage = g_object_get_data(G_OBJECT(notebook), "addtab");

	if (((gint)page_num == (pages - 1)) &&
	    (child != addpage)) {
		/* moved tab to end */
		gtk_notebook_reorder_child(notebook, addpage, -1);
	}
}

static void
nsgtk_tab_orientation(GtkNotebook *notebook)
{
	switch (nsoption_int(position_tab)) {
	case 0:
		gtk_notebook_set_tab_pos(notebook, GTK_POS_TOP);
		break;

	case 1:
		gtk_notebook_set_tab_pos(notebook, GTK_POS_LEFT);
		break;

	case 2:
		gtk_notebook_set_tab_pos(notebook, GTK_POS_RIGHT);
		break;

	case 3:
		gtk_notebook_set_tab_pos(notebook, GTK_POS_BOTTOM);
		break;

	}
}

/** adds a "new tab" tab */
static GtkWidget *
nsgtk_tab_add_newtab(GtkNotebook *notebook)
{
	GtkWidget *tablabel;
	GtkWidget *tabcontents;
	GtkWidget *add;

	tablabel = nsgtk_hbox_new(FALSE, 1);
	tabcontents = nsgtk_hbox_new(FALSE, 1);

	add = gtk_image_new_from_stock("gtk-add", GTK_ICON_SIZE_MENU);

	gtk_box_pack_start(GTK_BOX(tablabel), add, FALSE, FALSE, 0);

	gtk_widget_show_all(tablabel);

	gtk_notebook_append_page(notebook, tabcontents, tablabel);

	gtk_notebook_set_tab_reorderable(notebook, tabcontents, false);

	gtk_widget_show_all(tabcontents);

	g_object_set_data(G_OBJECT(notebook), "addtab", tabcontents);

	return tablabel;
}

/** callback to alter tab visibility when pages are added or removed */
static void
nsgtk_tab_visibility_update(GtkNotebook *notebook, GtkWidget *child, guint page)
{
	gint pagec = gtk_notebook_get_n_pages(notebook);
	GtkWidget *addpage = g_object_get_data(G_OBJECT(notebook), "addtab");

	if (addpage != NULL) {
		pagec--; /* skip the add tab */
		if ((gint)page == pagec) {
			/* ensure the add new tab cannot be current */
			gtk_notebook_set_current_page(notebook, page - 1);
		}
	}

	if ((nsoption_bool(show_single_tab) == true) || (pagec > 1)) {
		gtk_notebook_set_show_tabs(notebook, TRUE);
	} else {
		gtk_notebook_set_show_tabs(notebook, FALSE);
	}
}

/* exported interface documented in gtk/tabs.h */
void nsgtk_tab_options_changed(GtkNotebook *notebook)
{
	nsgtk_tab_orientation(notebook);
	nsgtk_tab_visibility_update(notebook, NULL, 0);
}


/* exported interface documented in gtk/tabs.h */
void nsgtk_tab_init(struct gtk_scaffolding *gs)
{
	GtkNotebook *notebook;

	notebook = nsgtk_scaffolding_notebook(gs);

	nsgtk_tab_add_newtab(notebook);

	g_signal_connect(notebook, "switch-page",
			 G_CALLBACK(nsgtk_tab_switch_page), NULL);
	g_signal_connect_after(notebook, "switch-page",
			 G_CALLBACK(nsgtk_tab_switch_page_after), NULL);

	g_signal_connect(notebook, "page-removed",
			 G_CALLBACK(nsgtk_tab_visibility_update), NULL);
	g_signal_connect(notebook, "page-added",
			 G_CALLBACK(nsgtk_tab_visibility_update), NULL);
	g_signal_connect(notebook, "page-reordered",
			 G_CALLBACK(nsgtk_tab_page_reordered), NULL);


	nsgtk_tab_options_changed(notebook);
}

/* exported interface documented in gtk/tabs.h */
void nsgtk_tab_add(struct gui_window *gw,
		   GtkWidget *tab_contents,
		   bool background)
{
	GtkNotebook *notebook;
	GtkWidget *tabBox;
	gint remember;
	gint pages;
	gint newpage;

	g_object_set_data(G_OBJECT(tab_contents), "gui_window", gw);

	notebook = nsgtk_scaffolding_notebook(nsgtk_get_scaffold(gw));

	tabBox = nsgtk_tab_label_setup(gw);

	nsgtk_window_set_tab(gw, tabBox);

	remember = gtk_notebook_get_current_page(notebook);

	pages = gtk_notebook_get_n_pages(notebook);

	newpage = gtk_notebook_insert_page(notebook, tab_contents, tabBox, pages - 1);

	gtk_notebook_set_tab_reorderable(notebook, tab_contents, true);

	gtk_widget_show_all(tab_contents);

	if (background) {
		gtk_notebook_set_current_page(notebook, remember);
	} else {
		gtk_notebook_set_current_page(notebook, newpage);
	}

	gtk_widget_grab_focus(GTK_WIDGET(nsgtk_scaffolding_urlbar(
			nsgtk_get_scaffold(gw))));
}

/* exported interface documented in gtk/tabs.h */
void nsgtk_tab_set_title(struct gui_window *g, const char *title)
{
	GtkWidget *label;
	GtkWidget *tab;

	tab = nsgtk_window_get_tab(g);
	if (tab == NULL) {
		return;
	}

	label = g_object_get_data(G_OBJECT(tab), "label");
	gtk_label_set_text(GTK_LABEL(label), title);
	gtk_widget_set_tooltip_text(tab, title);
}

/* exported interface documented in gtk/tabs.h */
nserror nsgtk_tab_close_current(GtkNotebook *notebook)
{
	gint pagen;
	GtkWidget *page;
	struct gui_window *gw;
	GtkWidget *addpage;

	pagen = gtk_notebook_get_current_page(notebook);
	if (pagen == -1) {
		return NSERROR_OK;
	}

	page = gtk_notebook_get_nth_page(notebook, pagen);
	if (page == NULL) {
		return NSERROR_OK;
	}

	addpage = g_object_get_data(G_OBJECT(notebook), "addtab");
	if (page == addpage) {
		/* the add new tab page is current, cannot close that */
		return NSERROR_OK;
	}

	gw = g_object_get_data(G_OBJECT(page), "gui_window");
	if (gw == NULL) {
		return NSERROR_OK;
	}

	nsgtk_window_destroy_browser(gw);

	return NSERROR_OK;
}

nserror nsgtk_tab_prev(GtkNotebook *notebook)
{
	gtk_notebook_prev_page(notebook);

	return NSERROR_OK;

}

nserror nsgtk_tab_next(GtkNotebook *notebook)
{
	gint pagen;
	GtkWidget *page;
	GtkWidget *addpage;

	pagen = gtk_notebook_get_current_page(notebook);
	if (pagen == -1) {
		return NSERROR_OK;
	}

	page = gtk_notebook_get_nth_page(notebook, pagen + 1);
	if (page == NULL) {
		return NSERROR_OK;
	}

	addpage = g_object_get_data(G_OBJECT(notebook), "addtab");
	if (page == addpage) {
		/* cannot make add new tab page current */
		return NSERROR_OK;
	}

	gtk_notebook_set_current_page(notebook, pagen + 1);

	return NSERROR_OK;
}
