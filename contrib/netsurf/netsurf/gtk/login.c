/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gtk/gtk.h>

#include "utils/log.h"
#include "gtk/gui.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"

struct session_401 {
	nsurl *url;				/**< URL being fetched */
	lwc_string *host;			/**< Host for user display */
	char *realm;				/**< Authentication realm */
	nserror (*cb)(bool proceed, void *pw);	/**< Continuation callback */
	void *cbpw;				/**< Continuation data */
	GtkBuilder *x;				/**< Our glade windows */
	GtkWindow *wnd;				/**< The login window itself */
	GtkEntry *user;				/**< Widget with username */
	GtkEntry *pass;				/**< Widget with password */
};

static void create_login_window(nsurl *url, lwc_string *host,
                const char *realm, nserror (*cb)(bool proceed, void *pw),
		void *cbpw);
static void destroy_login_window(struct session_401 *session);
static void nsgtk_login_next(GtkWidget *w, gpointer data);
static void nsgtk_login_ok_clicked(GtkButton *w, gpointer data);
static void nsgtk_login_cancel_clicked(GtkButton *w, gpointer data);

void gui_401login_open(nsurl *url, const char *realm,
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	lwc_string *host;

	host = nsurl_get_component(url, NSURL_HOST);
	assert(host != NULL);

	create_login_window(url, host, realm, cb, cbpw);

	lwc_string_unref(host);
}

void create_login_window(nsurl *url, lwc_string *host, const char *realm, 
		nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
	struct session_401 *session;

	/* create a new instance of the login window, and get handles to all
	 * the widgets we're interested in.
	 */

	GtkWindow *wnd;
	GtkLabel *lhost, *lrealm;
	GtkEntry *euser, *epass;
	GtkButton *bok, *bcan;
	GError* error = NULL;
	GtkBuilder* builder; 

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_file(builder, glade_file_location->login, &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
	}

	wnd = GTK_WINDOW(gtk_builder_get_object(builder, "wndLogin"));
	lhost = GTK_LABEL(gtk_builder_get_object(builder, "labelLoginHost"));
	lrealm = GTK_LABEL(gtk_builder_get_object(builder, "labelLoginRealm"));
	euser = GTK_ENTRY(gtk_builder_get_object(builder, "entryLoginUser"));
	epass = GTK_ENTRY(gtk_builder_get_object(builder, "entryLoginPass"));
	bok = GTK_BUTTON(gtk_builder_get_object(builder, "buttonLoginOK"));
	bcan = GTK_BUTTON(gtk_builder_get_object(builder, "buttonLoginCan"));

	/* create and fill in our session structure */

	session = calloc(1, sizeof(struct session_401));
	session->url = nsurl_ref(url);
	session->host = lwc_string_ref(host);
	session->realm = strdup(realm ? realm : "Secure Area");
	session->cb = cb;
	session->cbpw = cbpw;
	session->x = builder;
	session->wnd = wnd;
	session->user = euser;
	session->pass = epass;

	/* fill in our new login window */

	gtk_label_set_text(GTK_LABEL(lhost), lwc_string_data(host));
	gtk_label_set_text(lrealm, realm);
	gtk_entry_set_text(euser, "");
	gtk_entry_set_text(epass, "");

	/* attach signal handlers to the Login and Cancel buttons in our new
	 * window to call functions in this file to process the login
	 */
	g_signal_connect(G_OBJECT(bok), "clicked",
			G_CALLBACK(nsgtk_login_ok_clicked), (gpointer)session);
	g_signal_connect(G_OBJECT(bcan), "clicked",
			G_CALLBACK(nsgtk_login_cancel_clicked),
			(gpointer)session);

	/* attach signal handlers to the entry boxes such that pressing
	 * enter in one progresses the focus onto the next widget.
	 */

	g_signal_connect(G_OBJECT(euser), "activate",
			G_CALLBACK(nsgtk_login_next), (gpointer)epass);
	g_signal_connect(G_OBJECT(epass), "activate",
			G_CALLBACK(nsgtk_login_next), (gpointer)bok);

	/* make sure the username entry box currently has the focus */
	gtk_widget_grab_focus(GTK_WIDGET(euser));

	/* finally, show the window */
	gtk_widget_show(GTK_WIDGET(wnd));
}

void destroy_login_window(struct session_401 *session)
{
	nsurl_unref(session->url);
	lwc_string_unref(session->host);
	free(session->realm);
	gtk_widget_destroy(GTK_WIDGET(session->wnd));
	g_object_unref(G_OBJECT(session->x));
	free(session);
}

void nsgtk_login_next(GtkWidget *w, gpointer data)
{
	gtk_widget_grab_focus(GTK_WIDGET(data));
}

void nsgtk_login_ok_clicked(GtkButton *w, gpointer data)
{
	/* close the window and destroy it, having continued the fetch
	 * assoicated with it.
	 */

  	struct session_401 *session = (struct session_401 *)data;
	const gchar *user = gtk_entry_get_text(session->user);
	const gchar *pass = gtk_entry_get_text(session->pass);
	char *auth;

	auth = malloc(strlen(user) + strlen(pass) + 2);
	sprintf(auth, "%s:%s", user, pass);
	urldb_set_auth_details(session->url, session->realm, auth);
	free(auth);

	session->cb(true, session->cbpw);

	destroy_login_window(session);
}

void nsgtk_login_cancel_clicked(GtkButton *w, gpointer data)
{
	struct session_401 *session = (struct session_401 *) data;

	session->cb(false, session->cbpw);

	/* close and destroy the window */
	destroy_login_window(session);
}
