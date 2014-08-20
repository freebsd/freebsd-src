/*
 * Copyright 2004-2010 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Vincent Sanders <vince@debian.org>
 * Copyright 2004-2009 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
 * Copyright 2006-2009 Daniel Silverstone <dsilvers@netsurf-browser.org>
 * Copyright 2006-2008 Rob Kendrick <rjek@netsurf-browser.org>
 * Copyright 2008 John Tytgat <joty@netsurf-browser.org>
 * Copyright 2008 Adam Blokus <adamblokus@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "content/content.h"
#include "content/fetch.h"
#include "content/fetchers/curl.h"
#include "content/fetchers/resource.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "desktop/save_complete.h"
#include "desktop/save_pdf/pdf_plotters.h"
#include "desktop/searchweb.h"
#include "desktop/sslcert_viewer.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "css/utils.h"

#include "gtk/compat.h"
#include "gtk/completion.h"
#include "gtk/cookies.h"
#include "gtk/download.h"
#include "gtk/fetch.h"
#include "gtk/gui.h"
#include "gtk/history.h"
#include "gtk/hotlist.h"
#include "gtk/throbber.h"
#include "gtk/treeview.h"
#include "gtk/window.h"
#include "gtk/schedule.h"
#include "gtk/selection.h"
#include "gtk/search.h"

#include "render/form.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"

char *options_file_location;
char *toolbar_indices_file_location;
char *res_dir_location;
char *print_options_file_location;
char *languages_file_location;
char *themelist_file_location;

GdkPixbuf *favicon_pixbuf; /* favicon default pixbuf */

struct glade_file_location_s *glade_file_location;

static GtkWindow *nsgtk_warning_window;
GtkWidget *widWarning;

static GtkWidget *select_menu;
static struct browser_window *select_menu_bw;
static struct form_control *select_menu_control;

static void nsgtk_ssl_accept(GtkButton *w, gpointer data);
static void nsgtk_ssl_reject(GtkWidget *w, gpointer data);
static gboolean nsgtk_ssl_delete_event(GtkWidget *w, GdkEvent  *event,
		gpointer data);
static void nsgtk_select_menu_clicked(GtkCheckMenuItem *checkmenuitem,
					gpointer user_data);
#ifdef WITH_PDF_EXPORT
static void nsgtk_PDF_set_pass(GtkButton *w, gpointer data);
static void nsgtk_PDF_no_pass(GtkButton *w, gpointer data);
#endif

#define THROBBER_FRAMES 9

char **respaths; /** resource search path vector */

/** Create an array of valid paths to search for resources.
 *
 * The idea is that all the complex path computation to find resources
 * is performed here, once, rather than every time a resource is
 * searched for.
 */
static char **
nsgtk_init_resource(const char *resource_path)
{
	const gchar * const *langv;
	char **pathv; /* resource path string vector */
	char **respath; /* resource paths vector */

	pathv = filepath_path_to_strvec(resource_path);

	langv = g_get_language_names();

	respath = filepath_generate(pathv, langv);

	filepath_free_strvec(pathv);

	return respath;
}

/* This is an ugly hack to just get the new-style throbber going.
 * It, along with the PNG throbber loader, need making more generic.
 */
static bool nsgtk_throbber_init(char **respath, int framec)
{
	char **filenames;
	char targetname[PATH_MAX];
	int frame_num;
	bool ret;

	filenames = calloc(framec, sizeof(char *));
	if (filenames == NULL)
		return false;

	for (frame_num = 0; frame_num < framec; frame_num++) {
		snprintf(targetname, PATH_MAX, "throbber/throbber%d.png", frame_num);
		filenames[frame_num] = filepath_find(respath, targetname);
	}

	ret = nsgtk_throbber_initialise_from_png(frame_num, filenames);

	for (frame_num = 0; frame_num < framec; frame_num++) {
		free(filenames[frame_num]);
	}
	free(filenames);

	return ret;

}

#define NEW_GLADE_ERROR_SIZE 128

static char *
nsgtk_new_ui(char **respath, const char *name, GtkBuilder **pglade)
{
	GtkBuilder *builder;
	GError* error = NULL;
	char *filepath;
	char errorstr[NEW_GLADE_ERROR_SIZE];
	char resname[PATH_MAX];
#if GTK_CHECK_VERSION(3,0,0)
	int gtkv = 3;
#else
	int gtkv = 2;
#endif

	snprintf(resname, PATH_MAX, "%s.gtk%d.ui", name, gtkv);

	filepath = filepath_find(respath, resname);
	if (filepath == NULL) {
		snprintf(errorstr, NEW_GLADE_ERROR_SIZE,
			 "Unable to locate %s glade template file.\n", name);
		die(errorstr);
	}

	builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, filepath, &error))  {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		snprintf(errorstr, NEW_GLADE_ERROR_SIZE,
			 "Unable to load glade %s window definitions.\n", name);

		die(errorstr);
	}

	gtk_builder_connect_signals(builder, NULL);

	LOG(("Using '%s' as %s ui template file", filepath, name));

	if (pglade != NULL) {
		*pglade = builder;
	}

	return filepath;
}

/**
 * Load definitions from glade files.
 */
static void
nsgtk_init_glade(char **respath)
{
	GtkBuilder *gladeWarning;

	glade_file_location = calloc(1, sizeof(struct glade_file_location_s));
	if (glade_file_location == NULL) {
		die("Unable to allocate glade file locations");
	}

	glade_file_location->netsurf = nsgtk_new_ui(respath, "netsurf", NULL);
	glade_file_location->tabcontents = nsgtk_new_ui(respath, "tabcontents", NULL);
	glade_file_location->password = nsgtk_new_ui(respath, "password", NULL);
	glade_file_location->login = nsgtk_new_ui(respath, "login", NULL);
	glade_file_location->ssl = nsgtk_new_ui(respath, "ssl", NULL);
	glade_file_location->toolbar = nsgtk_new_ui(respath, "toolbar", NULL);
	glade_file_location->downloads = nsgtk_new_ui(respath, "downloads", NULL);
	glade_file_location->history = nsgtk_new_ui(respath, "history", NULL);
	glade_file_location->options = nsgtk_new_ui(respath, "options", NULL);
	glade_file_location->hotlist = nsgtk_new_ui(respath, "hotlist", NULL);
	glade_file_location->cookies = nsgtk_new_ui(respath, "cookies", NULL);

	glade_file_location->warning = nsgtk_new_ui(respath, "warning", &gladeWarning);
	nsgtk_warning_window = GTK_WINDOW(gtk_builder_get_object(gladeWarning, "wndWarning"));
	widWarning = GTK_WIDGET(gtk_builder_get_object(gladeWarning, "labelWarning"));
}

/**
 * Set option defaults for gtk frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	char *hdir = getenv("HOME");
	char buf[PATH_MAX];

	/* Set defaults for absent option strings */
	snprintf(buf, PATH_MAX, "%s/.netsurf/Cookies", hdir);
	nsoption_setnull_charp(cookie_file, strdup(buf));
	nsoption_setnull_charp(cookie_jar, strdup(buf));
	if (nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		LOG(("Failed initialising cookie options"));
		return NSERROR_BAD_PARAMETER;
	}

	if (nsoption_charp(downloads_directory) == NULL) {
		snprintf(buf, PATH_MAX, "%s/", hdir);
		nsoption_set_charp(downloads_directory, strdup(buf));
	}

	if (nsoption_charp(url_file) == NULL) {
		snprintf(buf, PATH_MAX, "%s/.netsurf/URLs", hdir);
		nsoption_set_charp(url_file, strdup(buf));
	}

	if (nsoption_charp(hotlist_path) == NULL) {
		snprintf(buf, PATH_MAX, "%s/.netsurf/Hotlist", hdir);
		nsoption_set_charp(hotlist_path, strdup(buf));
	}

	nsoption_setnull_charp(ca_path, strdup("/etc/ssl/certs"));

	if (nsoption_charp(url_file) == NULL ||
			nsoption_charp(ca_path) == NULL ||
			nsoption_charp(downloads_directory) == NULL ||
			nsoption_charp(hotlist_path) == NULL) {
		LOG(("Failed initialising string options"));
		return NSERROR_BAD_PARAMETER;
	}

	/* set default font names */
	nsoption_set_charp(font_sans, strdup("Sans"));
	nsoption_set_charp(font_serif, strdup("Serif"));
	nsoption_set_charp(font_mono, strdup("Monospace"));
	nsoption_set_charp(font_cursive, strdup("Serif"));
	nsoption_set_charp(font_fantasy, strdup("Serif"));

	return NSERROR_OK;
}

static void check_options(char **respath)
{
	char *hdir = getenv("HOME");
	char buf[PATH_MAX];
	nsoption_set_bool(core_select_menu, true);

	/* Attempt to handle nonsense status bar widths.  These may exist
	 * in people's Choices as the GTK front end used to abuse the
	 * status bar width option by using it for an absolute value in px.
	 * The GTK front end now correctly uses it as a proportion of window
	 * width.  Here we assume that a value of less than 15% is wrong
	 * and set to the default two thirds. */
	if (nsoption_int(toolbar_status_size) < 1500) {
		nsoption_set_int(toolbar_status_size, 6667);
	}

	/* user options should be stored in the users home directory */
	snprintf(buf, PATH_MAX, "%s/.netsurf/Choices", hdir);
	options_file_location = strdup(buf);

	filepath_sfinddef(respath, buf, "Print", "~/.netsurf/");
	LOG(("Using '%s' as Print Settings file", buf));
	print_options_file_location = strdup(buf);


}



/**
 * Initialize GTK interface.
 */
static void gui_init(int argc, char** argv, char **respath)
{
	char buf[PATH_MAX];
	char *resource_filename;
	char *addr = NULL;
	nsurl *url;
	nserror error;

	/* find the languages file */
	languages_file_location = filepath_find(respath, "languages");
	if ((languages_file_location == NULL) ||
	    (strlen(languages_file_location) < 10)) {
		die("Unable to find resources.\n");
	}

	/* find the theme list file */
	themelist_file_location = filepath_find(respath, "themelist");
	if ((themelist_file_location != NULL) &&
		(strlen(themelist_file_location) < 10)) {
		free(themelist_file_location);
		themelist_file_location = NULL;
	}
	if (themelist_file_location == NULL) {
		LOG(("Unable to find themelist - disabling"));
	}

	/* Obtain resources path location.
	 *
	 * Uses the directory the languages file was found in,
	 * @todo find and slaughter all references to this!
	 */
	res_dir_location = calloc(1, strlen(languages_file_location) - 8);
	memcpy(res_dir_location,
	       languages_file_location,
	       strlen(languages_file_location) - 9);
	LOG(("Using '%s' for resource path", res_dir_location));

	/* initialise the glade templates */
	nsgtk_init_glade(respath);

	/* set default icon if its available */
	resource_filename = filepath_find(respath, "netsurf.xpm");
	if (resource_filename != NULL) {
		gtk_window_set_default_icon_from_file(resource_filename, NULL);
		free(resource_filename);
	}

	/* Search engine sources */
	search_engines_file_location = filepath_find(respath, "SearchEngines");
	LOG(("Using '%s' as Search Engines file", search_engines_file_location));

	/* Default Icon */
	search_default_ico_location = filepath_find(respath, "default.ico");
	LOG(("Using '%s' as default search ico", search_default_ico_location));

	/* Default favicon */
	resource_filename = filepath_find(respath, "favicon.png");
	if (resource_filename != NULL) {
		favicon_pixbuf = gdk_pixbuf_new_from_file(resource_filename, NULL);
		free(resource_filename);
		if (favicon_pixbuf == NULL) {
			favicon_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, false, 8, 16,16);

		}
	}

	/* Toolbar inicies file */
	toolbar_indices_file_location = filepath_find(respath, "toolbarIndices");
	LOG(("Using '%s' as custom toolbar settings file", toolbar_indices_file_location));

	/* load throbber images */
	if (nsgtk_throbber_init(respath, THROBBER_FRAMES) == false)
		die("Unable to load throbber image.\n");

	/* Initialise completions - cannot fail */
	nsgtk_completion_init();

	filepath_sfinddef(respath, buf, "mime.types", "/etc/");
	gtk_fetch_filetype_init(buf);

	save_complete_init();

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	/* The tree view system needs to know the screen's DPI, so we
	 * find that out here, rather than when we create a first browser
	 * window.
	 */

	nscss_screen_dpi = FLTTOFIX(gdk_screen_get_resolution(
					    gdk_screen_get_default()));
	LOG(("Set CSS DPI to %f", FIXTOFLT(nscss_screen_dpi)));

	if (nsgtk_history_init(glade_file_location->history) == false)
		die("Unable to initialise history window.\n");

	if (nsgtk_download_init(glade_file_location->downloads) == false)
		die("Unable to initialise download window.\n");

	if (nsgtk_cookies_init(glade_file_location->cookies) == false)
		die("Unable to initialise cookies window.\n");

	if (nsgtk_hotlist_init(glade_file_location->hotlist) == false)
		die("Unable to initialise hotlist window.\n");

	/* If there is a url specified on the command line use it */
	if (argc > 1) {
		struct stat fs;
		if (stat(argv[1], &fs) == 0) {
			char *rp = realpath(argv[1], NULL);
			assert(rp != NULL);
			addr = malloc(SLEN("file://") + strlen(rp) + /*\0 */ 1);
			assert(addr != NULL);
			/* These are safe thanks to the above sum */
			strcpy(addr, "file://");
			strcat(addr, rp);
			free(rp);
		} else {
			addr = strdup(argv[1]);
		}
	}
	if (addr != NULL) {
		/* managed to set up based on local launch */
	} else if (nsoption_charp(homepage_url) != NULL) {
		addr = strdup(nsoption_charp(homepage_url));
	} else {
		addr = strdup(NETSURF_HOMEPAGE);
	}

	/* create an initial browser window */
	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}

	free(addr);

	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		netsurf_main_loop();
	}
}


/**
 * Check that ~/.netsurf/ exists, and if it doesn't, create it.
 */
static void nsgtk_check_homedir(void)
{
	char *hdir = getenv("HOME");
	char buf[PATH_MAX];

	if (hdir == NULL) {
		/* we really can't continue without a home directory. */
		LOG(("HOME is not set - nowhere to store state!"));
		die("NetSurf requires HOME to be set in order to run.\n");

	}

	snprintf(buf, PATH_MAX, "%s/.netsurf", hdir);
	if (access(buf, F_OK) != 0) {
		LOG(("You don't have a ~/.netsurf - creating one for you."));
		if (mkdir(buf, S_IRWXU) == -1) {
			LOG(("Unable to create %s", buf));
			die("NetSurf requires ~/.netsurf to exist, but it cannot be created.\n");
		}
	} else {
		if (chmod(buf, S_IRWXU) != 0) {
			LOG(("Unable to set permissions on %s", buf));
		}
	}
}

/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
	/* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}



static void nsgtk_poll(bool active)
{
	CURLMcode code;
	fd_set read_fd_set, write_fd_set, exc_fd_set;
	int max_fd;
	GPollFD *fd_list[1000];
	unsigned int fd_count = 0;
	bool block = true;

	schedule_run();

	if (browser_reformat_pending)
		block = false;

	if (active) {
		FD_ZERO(&read_fd_set);
		FD_ZERO(&write_fd_set);
		FD_ZERO(&exc_fd_set);
		code = curl_multi_fdset(fetch_curl_multi,
				&read_fd_set,
				&write_fd_set,
				&exc_fd_set,
				&max_fd);
		assert(code == CURLM_OK);
		for (int i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &read_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_IN | G_IO_HUP | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &write_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_OUT | G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
			if (FD_ISSET(i, &exc_fd_set)) {
				GPollFD *fd = malloc(sizeof *fd);
				fd->fd = i;
				fd->events = G_IO_ERR;
				g_main_context_add_poll(0, fd, 0);
				fd_list[fd_count++] = fd;
			}
		}
	}

	gtk_main_iteration_do(block);

	for (unsigned int i = 0; i != fd_count; i++) {
		g_main_context_remove_poll(0, fd_list[i]);
		free(fd_list[i]);
	}

	schedule_run();

	if (browser_reformat_pending)
		nsgtk_window_process_reformats();
}


static void gui_quit(void)
{
	LOG(("Quitting GUI"));

	/* Ensure all scaffoldings are destroyed before we go into exit */
	while (scaf_list != NULL) {
		nsgtk_scaffolding_destroy(scaf_list);
	}

	nsgtk_download_destroy();
	urldb_save_cookies(nsoption_charp(cookie_jar));
	urldb_save(nsoption_charp(url_file));
	nsgtk_cookies_destroy();
	nsgtk_history_destroy();
	nsgtk_hotlist_destroy();
	free(print_options_file_location);
	free(search_engines_file_location);
	free(search_default_ico_location);
	free(toolbar_indices_file_location);
	gtk_fetch_filetype_fin();
}

static void nsgtk_select_menu_clicked(GtkCheckMenuItem *checkmenuitem,
					gpointer user_data)
{
	form_select_process_selection(select_menu_control,
			(intptr_t)user_data);
}

static void gui_create_form_select_menu(struct browser_window *bw,
		struct form_control *control)
{

	intptr_t i;
	struct form_option *option;

	GtkWidget *menu_item;

	/* control->data.select.multiple is true if multiple selections
	 * are allowable.  We ignore this, as the core handles it for us.
	 * Yay. \o/
	 */

	if (select_menu != NULL)
		gtk_widget_destroy(select_menu);

	select_menu = gtk_menu_new();
	select_menu_bw = bw;
	select_menu_control = control;

	for (i = 0, option = control->data.select.items; option;
		i++, option = option->next) {
		menu_item = gtk_check_menu_item_new_with_label(option->text);
		if (option->selected)
			gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(menu_item), TRUE);

		g_signal_connect(menu_item, "toggled",
			G_CALLBACK(nsgtk_select_menu_clicked), (gpointer)i);

		gtk_menu_shell_append(GTK_MENU_SHELL(select_menu), menu_item);
	}

	gtk_widget_show_all(select_menu);

	gtk_menu_popup(GTK_MENU(select_menu), NULL, NULL, NULL,
			NULL /* data */, 0, gtk_get_current_event_time());

}

static void gui_launch_url(const char *url)
{
	gboolean ok;
	GError *error = NULL;

	ok = nsgtk_show_uri(NULL, url, GDK_CURRENT_TIME, &error);
	if (ok == TRUE)
		return;

	if (error) {
		warn_user(messages_get("URIOpenError"), error->message);
		g_error_free(error);
	}
}

void warn_user(const char *warning, const char *detail)
{
	char buf[300];	/* 300 is the size the RISC OS GUI uses */

  	LOG(("%s %s", warning, detail ? detail : ""));
	fflush(stdout);

	snprintf(buf, sizeof(buf), "%s %s", messages_get(warning),
			detail ? detail : "");
	buf[sizeof(buf) - 1] = 0;

	gtk_label_set_text(GTK_LABEL(widWarning), buf);

	gtk_widget_show_all(GTK_WIDGET(nsgtk_warning_window));
}

void die(const char * const error)
{
	fprintf(stderr, "%s", error);
	exit(EXIT_FAILURE);
}


static void gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs,
		unsigned long num, nserror (*cb)(bool proceed, void *pw),
		void *cbpw)
{
	static struct nsgtk_treeview *ssl_window;
	struct sslcert_session_data *data;
	GtkButton *accept, *reject;
	void **session;
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	GError* error = NULL;
	GtkBuilder* builder;

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_file(builder, glade_file_location->ssl, &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		return;
	}

	sslcert_viewer_create_session_data(num, url, cb, cbpw, certs, &data);
	ssl_current_session = data;

	window = GTK_WINDOW(gtk_builder_get_object(builder, "wndSSLProblem"));
	scrolled = GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "SSLScrolled"));
	drawing_area = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "SSLDrawingArea"));

	session = calloc(sizeof(void *), 3);

	if (session == NULL) {
		return;
	}

	ssl_window = nsgtk_treeview_create(TREE_SSLCERT, window, scrolled,
			drawing_area);

	if (ssl_window == NULL) {
		free(session);
		return;
	}

	accept = GTK_BUTTON(gtk_builder_get_object(builder, "sslaccept"));
	reject = GTK_BUTTON(gtk_builder_get_object(builder, "sslreject"));

	session[0] = builder;
	session[1] = ssl_window;
	session[2] = data;

#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

	CONNECT(accept, "clicked", nsgtk_ssl_accept, session);
	CONNECT(reject, "clicked", nsgtk_ssl_reject, session);
 	CONNECT(window, "delete_event", G_CALLBACK(nsgtk_ssl_delete_event),
			(gpointer)session);

	gtk_widget_show(GTK_WIDGET(window));
}

void nsgtk_ssl_accept(GtkButton *w, gpointer data)
{
	void **session = data;
	GtkBuilder *x = session[0];
	struct nsgtk_treeview *wnd = session[1];
	struct sslcert_session_data *ssl_data = session[2];

	sslcert_viewer_accept(ssl_data);

	nsgtk_treeview_destroy(wnd);
	g_object_unref(G_OBJECT(x));
	free(session);
}

void nsgtk_ssl_reject(GtkWidget *w, gpointer data)
{
	void **session = data;
	GtkBuilder *x = session[0];
	struct nsgtk_treeview *wnd = session[1];
	struct sslcert_session_data *ssl_data = session[2];

	sslcert_viewer_reject(ssl_data);

	nsgtk_treeview_destroy(wnd);
	g_object_unref(G_OBJECT(x));
	free(session);
}

gboolean nsgtk_ssl_delete_event(GtkWidget *w, GdkEvent  *event, gpointer data)
{
	nsgtk_ssl_reject(w, data);
	return FALSE;
}

#ifdef WITH_PDF_EXPORT

void PDF_Password(char **owner_pass, char **user_pass, char *path)
{
	GladeXML *x = glade_xml_new(glade_password_file_location, NULL, NULL);
	GtkWindow *wnd = GTK_WINDOW(glade_xml_get_widget(x, "wndPDFPassword"));
	GtkButton *ok, *no;
	void **data = malloc(5 * sizeof(void *));

	*owner_pass = NULL;
	*user_pass = NULL;

	data[0] = owner_pass;
	data[1] = user_pass;
	data[2] = wnd;
	data[3] = x;
	data[4] = path;

	ok = GTK_BUTTON(glade_xml_get_widget(x, "buttonPDFSetPassword"));
	no = GTK_BUTTON(glade_xml_get_widget(x, "buttonPDFNoPassword"));

	g_signal_connect(G_OBJECT(ok), "clicked",
			 G_CALLBACK(nsgtk_PDF_set_pass), (gpointer)data);
	g_signal_connect(G_OBJECT(no), "clicked",
			 G_CALLBACK(nsgtk_PDF_no_pass), (gpointer)data);

	gtk_widget_show(GTK_WIDGET(wnd));
}

static void nsgtk_PDF_set_pass(GtkButton *w, gpointer data)
{
	char **owner_pass = ((void **)data)[0];
	char **user_pass = ((void **)data)[1];
	GtkWindow *wnd = ((void **)data)[2];
	GladeXML *x = ((void **)data)[3];
	char *path = ((void **)data)[4];

	char *op, *op1;
	char *up, *up1;

	op = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
			"entryPDFOwnerPassword"))));
	op1 = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
			"entryPDFOwnerPassword1"))));
	up = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
		    	"entryPDFUserPassword"))));
	up1 = strdup(gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(x,
		     	"entryPDFUserPassword1"))));


	if (op[0] == '\0') {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
				"labelInfo")),
       				"Owner password must be at least 1 character long:");
		free(op);
		free(up);
	}
	else if (!strcmp(op, up)) {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
				"labelInfo")),
       				"User and owner passwords must be different:");
		free(op);
		free(up);
	}
	else if (!strcmp(op, op1) && !strcmp(up, up1)) {

		*owner_pass = op;
		if (up[0] == '\0')
			free(up);
		else
			*user_pass = up;

		free(data);
		gtk_widget_destroy(GTK_WIDGET(wnd));
		g_object_unref(G_OBJECT(x));

		save_pdf(path);
		free(path);
	}
	else {
		gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(x,
				"labelInfo")), "Passwords not confirmed:");
		free(op);
		free(up);
	}

	free(op1);
	free(up1);
}

static void nsgtk_PDF_no_pass(GtkButton *w, gpointer data)
{
	GtkWindow *wnd = ((void **)data)[2];
	GladeXML *x = ((void **)data)[3];
	char *path = ((void **)data)[4];

	free(data);

	gtk_widget_destroy(GTK_WIDGET(wnd));
	g_object_unref(G_OBJECT(x));

	save_pdf(path);
	free(path);
}
#endif

uint32_t gtk_gui_gdkkey_to_nskey(GdkEventKey *key)
{
	/* this function will need to become much more complex to support
	 * everything that the RISC OS version does.  But this will do for
	 * now.  I hope.
	 */
	switch (key->keyval) {

	case GDK_KEY(Tab):
		return KEY_TAB;

	case GDK_KEY(BackSpace):
		if (key->state & GDK_SHIFT_MASK)
			return KEY_DELETE_LINE_START;
		else
			return KEY_DELETE_LEFT;
	case GDK_KEY(Delete):
		if (key->state & GDK_SHIFT_MASK)
			return KEY_DELETE_LINE_END;
		else
			return KEY_DELETE_RIGHT;
	case GDK_KEY(Linefeed):	return 13;
	case GDK_KEY(Return):	return 10;
	case GDK_KEY(Left):		return KEY_LEFT;
	case GDK_KEY(Right):		return KEY_RIGHT;
	case GDK_KEY(Up):		return KEY_UP;
	case GDK_KEY(Down):		return KEY_DOWN;
	case GDK_KEY(Home):
		if (key->state & GDK_CONTROL_MASK)
			return KEY_TEXT_START;
		else
			return KEY_LINE_START;
	case GDK_KEY(End):
		if (key->state & GDK_CONTROL_MASK)
			return KEY_TEXT_END;
		else
			return KEY_LINE_END;
	case GDK_KEY(Page_Up):
		return KEY_PAGE_UP;
	case GDK_KEY(Page_Down):
		return KEY_PAGE_DOWN;
	case 'a':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_SELECT_ALL;
		return gdk_keyval_to_unicode(key->keyval);
	case 'u':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_DELETE_LINE;
		return gdk_keyval_to_unicode(key->keyval);
	case 'c':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_COPY_SELECTION;
		return gdk_keyval_to_unicode(key->keyval);
	case 'v':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_PASTE;
		return gdk_keyval_to_unicode(key->keyval);
	case 'x':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_CUT_SELECTION;
		return gdk_keyval_to_unicode(key->keyval);
	case 'Z':
	case 'y':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_REDO;
		return gdk_keyval_to_unicode(key->keyval);
	case 'z':
		if (key->state & GDK_CONTROL_MASK)
			return KEY_UNDO;
		return gdk_keyval_to_unicode(key->keyval);
	case GDK_KEY(Escape):
		return KEY_ESCAPE;

		/* Modifiers - do nothing for now */
	case GDK_KEY(Shift_L):
	case GDK_KEY(Shift_R):
	case GDK_KEY(Control_L):
	case GDK_KEY(Control_R):
	case GDK_KEY(Caps_Lock):
	case GDK_KEY(Shift_Lock):
	case GDK_KEY(Meta_L):
	case GDK_KEY(Meta_R):
	case GDK_KEY(Alt_L):
	case GDK_KEY(Alt_R):
	case GDK_KEY(Super_L):
	case GDK_KEY(Super_R):
	case GDK_KEY(Hyper_L):
	case GDK_KEY(Hyper_R):
		return 0;

	default:
		return gdk_keyval_to_unicode(key->keyval);
	}
}


static struct gui_browser_table nsgtk_browser_table = {
	.poll = nsgtk_poll,
	.schedule = nsgtk_schedule,

	.quit = gui_quit,
	.set_search_ico = gui_set_search_ico,
	.launch_url = gui_launch_url,
	.create_form_select_menu = gui_create_form_select_menu,
	.cert_verify = gui_cert_verify,
        .login = gui_401login_open,
};

/**
 * Main entry point from OS.
 */
int main(int argc, char** argv)
{
	char *messages;
	char *options;
	nserror ret;
	struct gui_table nsgtk_gui_table = {
		.browser = &nsgtk_browser_table,
		.window = nsgtk_window_table,
		.clipboard = nsgtk_clipboard_table,
		.download = nsgtk_download_table,
		.fetch = nsgtk_fetch_table,
		.search = nsgtk_search_table,
	};

	/* check home directory is available */
	nsgtk_check_homedir();

	respaths = nsgtk_init_resource("${HOME}/.netsurf/:${NETSURFRES}:"GTK_RESPATH":./gtk/res");

	gtk_init(&argc, &argv);

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "Options failed to initialise (%s)\n",
			messages_get_errorcode(ret));
		return 1;
	}
	options = filepath_find(respaths, "Choices");
	nsoption_read(options, nsoptions);
	free(options);
	nsoption_commandline(&argc, argv, nsoptions);
	check_options(respaths); /* check user options */

	/* common initialisation */
	messages = filepath_find(respaths, "Messages");
	ret = netsurf_init(messages, &nsgtk_gui_table);
	free(messages);
	if (ret != NSERROR_OK) {
		fprintf(stderr, "NetSurf core failed to initialise (%s)\n",
			messages_get_errorcode(ret));
		return 1;
	}

	/* run the browser */
	gui_init(argc, argv, respaths);

	/* common finalisation */
	netsurf_exit();

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	return 0;
}
