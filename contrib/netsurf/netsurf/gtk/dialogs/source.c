/*
 * Copyright 2009 Mark Benjamin <MarkBenjamin@dfgh.net>
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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "desktop/netsurf.h"
#include "desktop/browser_private.h"
#include "render/html.h"
#include "content/hlcache.h"
#include "content/content.h"

#include "gtk/dialogs/about.h"
#include "gtk/fetch.h"
#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/dialogs/source.h"

struct nsgtk_source_window {
	gchar *url;
	char *data;
	size_t data_len;
	GtkWindow *sourcewindow;
	GtkTextView *gv;
	struct browser_window *bw;
	struct nsgtk_source_window *next;
	struct nsgtk_source_window *prev;
};

struct menu_events {
	const char *widget;
	GCallback handler;
};

static GtkBuilder *glade_File;
static struct nsgtk_source_window *nsgtk_source_list = 0;
static char source_zoomlevel = 10;

void nsgtk_source_tab_init(GtkWindow *parent, struct browser_window *bw);

#define MENUEVENT(x) { #x, G_CALLBACK(nsgtk_on_##x##_activate) }
#define MENUPROTO(x) static gboolean nsgtk_on_##x##_activate( \
					GtkMenuItem *widget, gpointer g)

MENUPROTO(source_save_as);
MENUPROTO(source_print);
MENUPROTO(source_close);
MENUPROTO(source_select_all);
MENUPROTO(source_cut);
MENUPROTO(source_copy);
MENUPROTO(source_paste);
MENUPROTO(source_delete);
MENUPROTO(source_zoom_in);
MENUPROTO(source_zoom_out);
MENUPROTO(source_zoom_normal);
MENUPROTO(source_about);

struct menu_events source_menu_events[] = {
MENUEVENT(source_save_as),
MENUEVENT(source_print),
MENUEVENT(source_close),
MENUEVENT(source_select_all),
MENUEVENT(source_cut),
MENUEVENT(source_copy),
MENUEVENT(source_paste),
MENUEVENT(source_delete),
MENUEVENT(source_zoom_in),
MENUEVENT(source_zoom_out),
MENUEVENT(source_zoom_normal),
MENUEVENT(source_about),
{NULL, NULL}
};

static void nsgtk_attach_source_menu_handlers(GtkBuilder *xml, gpointer g)
{
	struct menu_events *event = source_menu_events;

	while (event->widget != NULL)
	{
		GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(xml, event->widget));
		g_signal_connect(G_OBJECT(w), "activate", event->handler, g);
		event++;
	}
}

static gboolean nsgtk_source_destroy_event(GtkBuilder *window, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;

	if (nsg->next != NULL)
		nsg->next->prev = nsg->prev;

	if (nsg->prev != NULL)
		nsg->prev->next = nsg->next;
	else
		nsgtk_source_list = nsg->next;

	free(nsg->data);
	free(nsg->url);
	free(g);

	return FALSE;
}

static gboolean nsgtk_source_delete_event(GtkWindow * window, gpointer g)
{
	return FALSE;
}

void nsgtk_source_dialog_init(GtkWindow *parent, struct browser_window *bw)
{
	char glade_Location[strlen(res_dir_location) + SLEN("source.gtk2.ui")
			+ 1];
	if (content_get_type(bw->current_content) != CONTENT_HTML)
		return;

	if (nsoption_bool(source_tab)) {
		nsgtk_source_tab_init(parent, bw);
		return;
	}

	sprintf(glade_Location, "%ssource.gtk2.ui", res_dir_location);

	GError* error = NULL;
	glade_File = gtk_builder_new();
	if (!gtk_builder_add_from_file(glade_File, glade_Location, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		LOG(("error loading glade tree"));
		return;
	}


	const char *source_data;
	unsigned long source_size;
	char *data = NULL;
	size_t data_len;

	source_data = content_get_source_data(bw->current_content,
			&source_size);

	nserror r = utf8_from_enc(
			source_data,
			html_get_encoding(bw->current_content),
			source_size,
			&data,
			&data_len);
	if (r == NSERROR_NOMEM) {
		warn_user("NoMemory",0);
		return;
	} else if (r == NSERROR_BAD_ENCODING) {
		warn_user("EncNotRec",0);
		return;
	}

	GtkWindow *wndSource = GTK_WINDOW(gtk_builder_get_object(
			glade_File, "wndSource"));
	GtkWidget *cutbutton = GTK_WIDGET(gtk_builder_get_object(
						  glade_File, "source_cut"));
	GtkWidget *pastebutton = GTK_WIDGET(gtk_builder_get_object(
						    glade_File, "source_paste"));
	GtkWidget *deletebutton = GTK_WIDGET(gtk_builder_get_object(
						     glade_File, "source_delete"));
	GtkWidget *printbutton = GTK_WIDGET(gtk_builder_get_object(
						    glade_File, "source_print"));
	gtk_widget_set_sensitive(cutbutton, FALSE);
	gtk_widget_set_sensitive(pastebutton, FALSE);
	gtk_widget_set_sensitive(deletebutton, FALSE);
	/* for now */
	gtk_widget_set_sensitive(printbutton, FALSE);

	struct nsgtk_source_window *thiswindow =
			malloc(sizeof(struct nsgtk_source_window));
	if (thiswindow == NULL) {
		free(data);
		warn_user("NoMemory", 0);
		return;
	}

	thiswindow->url = strdup(nsurl_access(browser_window_get_url(bw)));
	if (thiswindow->url == NULL) {
		free(thiswindow);
		free(data);
		warn_user("NoMemory", 0);
		return;
	}

	thiswindow->data = data;
	thiswindow->data_len = data_len;

	thiswindow->sourcewindow = wndSource;
	thiswindow->bw = bw;

	char title[strlen(thiswindow->url) + SLEN("Source of  - NetSurf") + 1];
	sprintf(title, "Source of %s - NetSurf", thiswindow->url);

	thiswindow->next = nsgtk_source_list;
	thiswindow->prev = NULL;
	if (nsgtk_source_list != NULL)
		nsgtk_source_list->prev = thiswindow;
	nsgtk_source_list = thiswindow;

	nsgtk_attach_source_menu_handlers(glade_File, thiswindow);

	gtk_window_set_title(wndSource, title);

	g_signal_connect(G_OBJECT(wndSource), "destroy",
			G_CALLBACK(nsgtk_source_destroy_event),
			thiswindow);
	g_signal_connect(G_OBJECT(wndSource), "delete-event",
			G_CALLBACK(nsgtk_source_delete_event),
			thiswindow);

	GtkTextView *sourceview = GTK_TEXT_VIEW(
			gtk_builder_get_object(glade_File,
			"source_view"));

	PangoFontDescription *fontdesc =
		pango_font_description_from_string("Monospace 8");

	thiswindow->gv = sourceview;
	nsgtk_widget_modify_font(GTK_WIDGET(sourceview), fontdesc);

	GtkTextBuffer *tb = gtk_text_view_get_buffer(sourceview);
	gtk_text_buffer_set_text(tb, thiswindow->data, -1);

	gtk_widget_show(GTK_WIDGET(wndSource));

}

/**
 * create a new tab with page source
 */
void nsgtk_source_tab_init(GtkWindow *parent, struct browser_window *bw)
{
	const char *source_data;
	unsigned long source_size;
	char *ndata = 0;
	size_t ndata_len;
	nsurl *url;
	nserror error;
	nserror r;
	gchar *filename;
	char *fileurl;
	gint handle;

	source_data = content_get_source_data(bw->current_content,
					      &source_size);

	r = utf8_from_enc(source_data,
			  html_get_encoding(bw->current_content),
			  source_size,
			  &ndata,
			  &ndata_len);
	if (r == NSERROR_NOMEM) {
		warn_user("NoMemory",0);
		return;
	} else if (r == NSERROR_BAD_ENCODING) {
		warn_user("EncNotRec",0);
		return;
	}

	handle = g_file_open_tmp("nsgtksourceXXXXXX", &filename, NULL);
	if ((handle == -1) || (filename == NULL)) {
		warn_user(messages_get("gtkSourceTabError"), 0);
		free(ndata);
		return;
	}
	close (handle); /* in case it was binary mode */

	FILE *f = fopen(filename, "w");
	if (f == NULL) {
		warn_user(messages_get("gtkSourceTabError"), 0);
		g_free(filename);
		free(ndata);
		return;
	}

	fprintf(f, "%s", ndata);
	fclose(f);
	free(ndata);
	fileurl = path_to_url(filename);
	g_free(filename);
	if (fileurl == NULL) {
		warn_user(messages_get("NoMemory"), 0);
		return;
	}

	/* Open tab */
	error = nsurl_create(fileurl, &url);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		error = browser_window_create(BW_CREATE_TAB,
					      url,
					      NULL,
					      bw,
					      NULL);
		nsurl_unref(url);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
	}
	free(fileurl);
}

static void nsgtk_source_file_save(GtkWindow *parent, const char *filename,
				   const char *data, size_t data_size)
{
	FILE *f;
	GtkWidget *notif;
	GtkWidget *label;

	f = fopen(filename, "w+");
	if (f != NULL) {
		fwrite(data, data_size, 1, f);
		fclose(f);
		return;
	}

	/* inform user of faliure */
	notif = gtk_dialog_new_with_buttons(messages_get("gtkSaveFailedTitle"),
					    parent,
					    GTK_DIALOG_MODAL, GTK_STOCK_OK,
					    GTK_RESPONSE_NONE, NULL);

	g_signal_connect_swapped(notif, "response",
				 G_CALLBACK(gtk_widget_destroy), notif);

	label = gtk_label_new(messages_get("gtkSaveFailed"));
	gtk_container_add(GTK_CONTAINER(nsgtk_dialog_get_content_area(GTK_DIALOG(notif))), label);
	gtk_widget_show_all(notif);

}


gboolean nsgtk_on_source_save_as_activate(GtkMenuItem *widget, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;
	GtkWidget *fc = gtk_file_chooser_dialog_new(
			messages_get("gtkSourceSave"),
			nsg->sourcewindow,
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE,
			GTK_RESPONSE_ACCEPT,
			NULL);
	char *filename;
	url_func_result res;

	res = url_nice(nsg->url, &filename, false);
	if (res != URL_FUNC_OK) {
		filename = strdup(messages_get("SaveSource"));
		if (filename == NULL) {
			warn_user("NoMemory", 0);
			return FALSE;
		}
	}

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), filename);

	free(filename);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc),
						       TRUE);

	if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
		nsgtk_source_file_save(nsg->sourcewindow, filename, nsg->data, nsg->data_len);
		g_free(filename);
	}

	gtk_widget_destroy(fc);

	return TRUE;
}


gboolean nsgtk_on_source_print_activate( GtkMenuItem *widget, gpointer g)
{
	/* correct printing */

	return TRUE;
}

gboolean nsgtk_on_source_close_activate( GtkMenuItem *widget, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;

	gtk_widget_destroy(GTK_WIDGET(nsg->sourcewindow));

	return TRUE;
}



gboolean nsgtk_on_source_select_all_activate (GtkMenuItem *widget, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;
	GtkTextBuffer *buf = gtk_text_view_get_buffer(nsg->gv);
	GtkTextIter start, end;

	gtk_text_buffer_get_bounds(buf, &start, &end);

	gtk_text_buffer_select_range(buf, &start, &end);

	return TRUE;
}

gboolean nsgtk_on_source_cut_activate(GtkMenuItem *widget, gpointer g)
{
	return TRUE;
}

gboolean nsgtk_on_source_copy_activate(GtkMenuItem *widget, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;
	GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(nsg->gv));

	gtk_text_buffer_copy_clipboard(buf,
			gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));

	return TRUE;
}

gboolean nsgtk_on_source_paste_activate(GtkMenuItem *widget, gpointer g)
{
	return TRUE;
}

gboolean nsgtk_on_source_delete_activate(GtkMenuItem *widget, gpointer g)
{
	return TRUE;
}

static void nsgtk_source_update_zoomlevel(gpointer g)
{
	struct nsgtk_source_window *nsg;
	GtkTextBuffer *buf;
	GtkTextTagTable *tab;
	GtkTextTag *tag;

	nsg = nsgtk_source_list;
	while (nsg) {
		if (nsg->gv) {
			buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(nsg->gv));

			tab = gtk_text_buffer_get_tag_table(
					GTK_TEXT_BUFFER(buf));

			tag = gtk_text_tag_table_lookup(tab, "zoomlevel");
			if (!tag) {
				tag = gtk_text_tag_new("zoomlevel");
				gtk_text_tag_table_add(tab, GTK_TEXT_TAG(tag));
			}

			gdouble fscale = ((gdouble) source_zoomlevel) / 10;

			g_object_set(GTK_TEXT_TAG(tag), "scale", fscale, NULL);

			GtkTextIter start, end;

			gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buf),
					&start, &end);
			gtk_text_buffer_remove_all_tags(GTK_TEXT_BUFFER(buf),
					&start,	&end);
			gtk_text_buffer_apply_tag(GTK_TEXT_BUFFER(buf),
					GTK_TEXT_TAG(tag), &start, &end);
		}
		nsg = nsg->next;
	}
}

gboolean nsgtk_on_source_zoom_in_activate(GtkMenuItem *widget, gpointer g)
{
	source_zoomlevel++;
	nsgtk_source_update_zoomlevel(g);

	return TRUE;
}

gboolean nsgtk_on_source_zoom_out_activate(GtkMenuItem *widget, gpointer g)
{
	if (source_zoomlevel > 1) {
		source_zoomlevel--;
		nsgtk_source_update_zoomlevel(g);
	}

	return TRUE;
}


gboolean nsgtk_on_source_zoom_normal_activate(GtkMenuItem *widget, gpointer g)
{
	source_zoomlevel = 10;
	nsgtk_source_update_zoomlevel(g);

	return TRUE;
}

gboolean nsgtk_on_source_about_activate(GtkMenuItem *widget, gpointer g)
{
	struct nsgtk_source_window *nsg = (struct nsgtk_source_window *) g;

	nsgtk_about_dialog_init(nsg->sourcewindow, nsg->bw, netsurf_version);

	return TRUE;
}
