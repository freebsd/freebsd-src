/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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
#include <math.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/browser.h"
#include "utils/nsoption.h"
#include "desktop/searchweb.h"

#include "gtk/compat.h"
#include "gtk/window.h"
#include "gtk/gui.h"
#include "gtk/scaffolding.h"
#include "gtk/theme.h"
#include "gtk/dialogs/preferences.h"

/* private prefs */
struct ppref {
	/** dialog handle created when window first accessed */
	GObject *dialog;

	struct browser_window *bw;

	/* widgets which are accessed from outside their own signal handlers */
	GtkEntry *entryHomePageURL;
	GtkEntry *entryProxyHost;
	GtkEntry *entryProxyUser;
	GtkEntry *entryProxyPassword;
	GtkEntry *entryProxyNoproxy;
	GtkSpinButton *spinProxyPort;

	/* dynamic list stores */
	GtkListStore *themes;
	GtkListStore *content_language;
};

static struct ppref ppref;


/* Set netsurf option based on toggle button state
 *
 * This works for any widget which subclasses togglebutton (checkbox,
 * radiobutton etc.)
 */
#define TOGGLEBUTTON_SIGNALS(WIDGET, OPTION)				\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_toggled(GtkToggleButton *togglebutton,	\
				     struct ppref *priv);		\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_toggled(GtkToggleButton *togglebutton,	\
				     struct ppref *priv)		\
{									\
	nsoption_set_bool(OPTION,					\
			  gtk_toggle_button_get_active(togglebutton));	\
}									\
									\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget,			\
				     struct ppref *priv);		\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget,			\
				     struct ppref *priv)		\
{									\
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),		\
				     nsoption_bool(OPTION));		\
}

#define SPINBUTTON_SIGNALS(WIDGET, OPTION, MULTIPLIER)			\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_valuechanged(GtkSpinButton *spinbutton,	\
					  struct ppref *priv);		\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_valuechanged(GtkSpinButton *spinbutton,	\
					  struct ppref *priv)		\
{									\
	nsoption_set_int(OPTION,					\
		round(gtk_spin_button_get_value(spinbutton) * MULTIPLIER)); \
}									\
									\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget, struct ppref *priv); \
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget, struct ppref *priv) \
{									\
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),		\
		((gdouble)nsoption_int(OPTION)) / MULTIPLIER);		\
}

#define ENTRY_SIGNALS(WIDGET, OPTION)					\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_changed(GtkEditable *editable, struct ppref *priv); \
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_changed(GtkEditable *editable, struct ppref *priv)\
{									\
	nsoption_set_charp(OPTION,					\
		strdup(gtk_entry_get_text(GTK_ENTRY(editable))));	\
}									\
									\
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget,	struct ppref *priv); \
G_MODULE_EXPORT void							\
nsgtk_preferences_##WIDGET##_realize(GtkWidget *widget,	struct ppref *priv) \
{									\
	const char *OPTION;						\
	OPTION = nsoption_charp(OPTION);				\
	if (OPTION != NULL) {						\
		gtk_entry_set_text(GTK_ENTRY(widget), OPTION);		\
	}								\
}

/* GTK module requires these to be exported symbols so these all need
 * forward declaring to avoid warnings
 */
G_MODULE_EXPORT void nsgtk_preferences_comboProxyType_changed(GtkComboBox *combo, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboProxyType_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboboxLoadImages_changed(GtkComboBox *combo, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboboxLoadImages_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboDefault_changed(GtkComboBox *combo, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboDefault_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_fontPreview_clicked(GtkButton *button, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboboxLanguage_changed(GtkComboBox *combo, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboboxLanguage_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboTheme_changed(GtkComboBox *combo, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboTheme_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_buttonAddTheme_clicked(GtkButton *button, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_checkShowSingleTab_toggled(GtkToggleButton *togglebutton, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_checkShowSingleTab_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboTabPosition_changed(GtkComboBox *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboTabPosition_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_sourceButtonWindow_toggled(GtkToggleButton *togglebutton, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_sourceButtonWindow_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboButtonType_changed(GtkComboBox *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboButtonType_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_setCurrentPage_clicked(GtkButton *button, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_setDefaultPage_clicked(GtkButton *button, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboSearch_changed(GtkComboBox *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_comboSearch_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_fileChooserDownloads_selectionchanged(GtkFileChooser *chooser, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_fileChooserDownloads_realize(GtkWidget *widget, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_dialogPreferences_response(GtkDialog *dlg, gint resid);
G_MODULE_EXPORT gboolean nsgtk_preferences_dialogPreferences_deleteevent(GtkDialog *dlg, struct ppref *priv);
G_MODULE_EXPORT void nsgtk_preferences_dialogPreferences_destroy(GtkDialog *dlg, struct ppref *priv);


/********* PDF **********/

/* Appearance */

/* no images in output */
TOGGLEBUTTON_SIGNALS(checkSuppressImages, suppress_images)

/* no background images */
TOGGLEBUTTON_SIGNALS(checkRemoveBackgrounds, remove_backgrounds)

/* scale to fit page */
TOGGLEBUTTON_SIGNALS(checkFitPage, enable_loosening)

/* port */
SPINBUTTON_SIGNALS(spinExportScale, export_scale, 1.0)

/* Margins */
SPINBUTTON_SIGNALS(spinMarginTop, margin_top, 1.0)
SPINBUTTON_SIGNALS(spinMarginBottom, margin_bottom, 1.0)
SPINBUTTON_SIGNALS(spinMarginLeft, margin_left, 1.0)
SPINBUTTON_SIGNALS(spinMarginRight, margin_right, 1.0)


/* Generation */

/* output is compressed */
TOGGLEBUTTON_SIGNALS(checkCompressPDF, enable_PDF_compression)

/* output has a password */
TOGGLEBUTTON_SIGNALS(checkPasswordPDF, enable_PDF_password)

/********* Network **********/

/* HTTP proxy */
static void set_proxy_widgets_sensitivity(int proxyval, struct ppref *priv)
{
	gboolean host;
	gboolean port;
	gboolean user;
	gboolean pass;
	gboolean noproxy;

	switch (proxyval) {
	case 0: /* no proxy */
		host = FALSE;
		port = FALSE;
		user = FALSE;
		pass = FALSE;
		noproxy = FALSE;
		break;

	case 1: /* proxy with no auth */
		host = TRUE;
		port = TRUE;
		user = FALSE;
		pass = FALSE;
		noproxy = TRUE;
		break;

	case 2: /* proxy with basic auth */
		host = TRUE;
		port = TRUE;
		user = TRUE;
		pass = TRUE;
		noproxy = TRUE;
		break;

	case 3: /* proxy with ntlm auth */
		host = TRUE;
		port = TRUE;
		user = TRUE;
		pass = TRUE;
		noproxy = TRUE;
		break;

	case 4: /* system proxy */
		host = FALSE;
		port = FALSE;
		user = FALSE;
		pass = FALSE;
		noproxy = FALSE;
		break;

	default:
		return;
	}

	gtk_widget_set_sensitive(GTK_WIDGET(priv->entryProxyHost), host);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->spinProxyPort), port);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->entryProxyUser), user);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->entryProxyPassword), pass);
	gtk_widget_set_sensitive(GTK_WIDGET(priv->entryProxyNoproxy), noproxy);

}

G_MODULE_EXPORT void
nsgtk_preferences_comboProxyType_changed(GtkComboBox *combo, struct ppref *priv)
{
	int proxy_sel;
	proxy_sel = gtk_combo_box_get_active(combo);

	switch (proxy_sel) {
	case 0: /* no proxy */
		nsoption_set_bool(http_proxy, false);
		break;

	case 1: /* proxy with no auth */
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE);
		break;

	case 2: /* proxy with basic auth */
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_BASIC);
		break;

	case 3: /* proxy with ntlm auth */
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NTLM);
		break;

	case 4: /* system proxy */
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, OPTION_HTTP_PROXY_AUTH_NONE);
		break;
	}

	set_proxy_widgets_sensitivity(proxy_sel, priv);
}

G_MODULE_EXPORT void
nsgtk_preferences_comboProxyType_realize(GtkWidget *widget, struct ppref *priv)
{
	int proxytype = 0; /* no proxy by default */

	if (nsoption_bool(http_proxy) == true) {
		/* proxy type combo box starts with disabled, to allow
		 * for this the http_proxy option needs combining with
		 * the http_proxy_auth option
		 */
		proxytype = nsoption_int(http_proxy_auth) + 1;
		if (nsoption_charp(http_proxy_host) == NULL) {
			/* set to use a proxy without a host, turn proxy off */
			proxytype = 0;
		} else if (((proxytype == 2) ||
			    (proxytype == 3)) &&
			   ((nsoption_charp(http_proxy_auth_user) == NULL) ||
			    (nsoption_charp(http_proxy_auth_pass) == NULL))) {
			/* authentication selected with empty credentials, turn proxy off */
			proxytype = 0;
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), proxytype);

	set_proxy_widgets_sensitivity(proxytype, priv);
}

/* host */
ENTRY_SIGNALS(entryProxyHost, http_proxy_host)

/* port */
SPINBUTTON_SIGNALS(spinProxyPort, http_proxy_port, 1.0)

/* user */
ENTRY_SIGNALS(entryProxyUser, http_proxy_auth_user)

/* password */
ENTRY_SIGNALS(entryProxyPassword, http_proxy_auth_pass)

/* no proxy */
ENTRY_SIGNALS(entryProxyNoproxy, http_proxy_noproxy)

/* Fetching */

/* maximum fetchers */
SPINBUTTON_SIGNALS(spinMaxFetchers, max_fetchers, 1.0)

/* fetches per host */
SPINBUTTON_SIGNALS(spinFetchesPerHost, max_fetchers_per_host, 1.0)

/* cached connections */
SPINBUTTON_SIGNALS(spinCachedConnections, max_cached_fetch_handles, 1.0)


/********* Privacy **********/

/* General */

/* enable referral submission */
TOGGLEBUTTON_SIGNALS(checkSendReferer, send_referer)

/* send do not track */
TOGGLEBUTTON_SIGNALS(checkSendDNT, do_not_track)

/* History */

/* local history shows url tooltips */
TOGGLEBUTTON_SIGNALS(checkHoverURLs, hover_urls)

/* remember browsing history */
SPINBUTTON_SIGNALS(spinHistoryAge, history_age, 1.0)

/* Cache */

/* memory cache size */
SPINBUTTON_SIGNALS(spinMemoryCacheSize, memory_cache_size, (1024*1024))

/* disc cache size */
SPINBUTTON_SIGNALS(spinDiscCacheSize, disc_cache_size, (1024*1024))


/* disc cache age */
SPINBUTTON_SIGNALS(spinDiscCacheAge, disc_cache_age, 1.0)


/********* Content **********/

/* Control */


/* prevent popups */
TOGGLEBUTTON_SIGNALS(checkDisablePopups, disable_popups)

/* hide adverts */
TOGGLEBUTTON_SIGNALS(checkHideAdverts, block_advertisements)

/* enable javascript */
TOGGLEBUTTON_SIGNALS(checkEnableJavascript, enable_javascript)

/* disable plugins */
TOGGLEBUTTON_SIGNALS(checkDisablePlugins, disable_plugins)

/* high quality image scaling */
TOGGLEBUTTON_SIGNALS(checkResampleImages, render_resample)

/* load and display of images */
G_MODULE_EXPORT void
nsgtk_preferences_comboboxLoadImages_changed(GtkComboBox *combo,
					     struct ppref *priv)
{
	int img_sel;
	/* get the row number for the selection */
	img_sel = gtk_combo_box_get_active(combo);
	switch (img_sel) {
	case 0:
		/* background and foreground */
		nsoption_set_bool(foreground_images, true);
		nsoption_set_bool(background_images, true);
		break;

	case 1:
		/* foreground only */
		nsoption_set_bool(foreground_images, true);
		nsoption_set_bool(background_images, false);
		break;

	case 2:
		/* background only */
		nsoption_set_bool(foreground_images, false);
		nsoption_set_bool(background_images, true);
		break;

	case 3:
		/* no images */
		nsoption_set_bool(foreground_images, false);
		nsoption_set_bool(background_images, false);
		break;
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboboxLoadImages_realize(GtkWidget *widget,
					   struct ppref *priv)
{
	if (nsoption_bool(foreground_images)) {
		if (nsoption_bool(background_images)) {
			/* background and foreground */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
		} else {
			/* foreground only */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 1);
		}
	} else {
		if (nsoption_bool(background_images)) {
			/* background only */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 2);
		} else {
			/* no images */
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 3);
		}
	}
}

/* Animation */

/* enable animation */
TOGGLEBUTTON_SIGNALS(checkEnableAnimations, animate_images)

/* frame time */
SPINBUTTON_SIGNALS(spinAnimationSpeed, minimum_gif_delay, 100.0)

/* Fonts */

/* default font */
G_MODULE_EXPORT void
nsgtk_preferences_comboDefault_changed(GtkComboBox *combo, struct ppref *priv)
{
	int font_sel;
	/* get the row number for the selection */
	font_sel = gtk_combo_box_get_active(combo);
	if ((font_sel >= 0) && (font_sel <= 4)) {
		nsoption_set_int(font_default, font_sel);
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboDefault_realize(GtkWidget *widget, struct ppref *priv)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
				 nsoption_int(font_default));
}

/* default font size */
SPINBUTTON_SIGNALS(spinDefaultSize, font_size, 10.0)

/* preview - actually reflow all views */
G_MODULE_EXPORT void
nsgtk_preferences_fontPreview_clicked(GtkButton *button, struct ppref *priv)
{
	nsgtk_reflow_all_windows();
}


/* Language */

/* accept language */
G_MODULE_EXPORT void
nsgtk_preferences_comboboxLanguage_changed(GtkComboBox *combo,
					   struct ppref *priv)
{
	gchar *lang = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* Obtain currently selected item from combo box.
	 * If nothing is selected, do nothing.
	 */
	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		/* Obtain data model from combo box. */
		model = gtk_combo_box_get_model(combo);

		/* Obtain string from model. */
		gtk_tree_model_get(model, &iter, 0, &lang, -1);
	}

	if (lang != NULL) {
		nsoption_set_charp(accept_language, strdup(lang));
		g_free(lang);
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboboxLanguage_realize(GtkWidget *widget,
					   struct ppref *priv)
{
	/* Fill content language list store */
	int active_language = 0;
	GtkTreeIter iter;
	FILE *fp;
	char buf[50];
	const char *default_accept_language = "en";

	if ((priv->content_language != NULL) &&
	    (languages_file_location != NULL) &&
	    ((fp = fopen(languages_file_location, "r")) != NULL)) {
		int combo_row_count = 0;

		gtk_list_store_clear(priv->content_language);
		active_language = -1;

		LOG(("Used %s for languages", languages_file_location));
		while (fgets(buf, sizeof(buf), fp)) {
			/* Ignore blank lines */
			if (buf[0] == '\0')
				continue;

			/* Remove trailing \n */
			buf[strlen(buf) - 1] = '\0';

			gtk_list_store_append(priv->content_language, &iter);
			gtk_list_store_set(priv->content_language,
					   &iter, 0, buf, -1 );

			if (strcmp(buf, default_accept_language) == 0) {
				active_language = combo_row_count;
			}

			combo_row_count++;
		}

		if (active_language == -1) {
			/* configured language was not in list, add it */
			gtk_list_store_append(priv->content_language, &iter);
			gtk_list_store_set(priv->content_language,
					   &iter,
					   0, default_accept_language, -1 );
			active_language = combo_row_count;
		}

		fclose(fp);
	} else {
		LOG(("Failed opening languages file"));
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), active_language);
}


/********* Apperance **********/

/* Themes */

/* select theme */
G_MODULE_EXPORT void
nsgtk_preferences_comboTheme_changed(GtkComboBox *combo, struct ppref *priv)
{
	nsgtk_scaffolding *current = scaf_list;
	int theme = 0;
	gchar *name;
	GtkTreeIter iter;
	GtkTreeModel *model;

	/* Obtain currently selected item from combo box.
	 * If nothing is selected, do nothing.
	 */
	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		/* get the row number for the config */
		theme = gtk_combo_box_get_active(combo);

		nsoption_set_int(current_theme, theme);

		/* retrive the theme name if it is not the default */
		if (theme != 0) {
			/* Obtain data model from combo box. */
			model = gtk_combo_box_get_model(combo);

			/* Obtain string from model. */
			gtk_tree_model_get(model, &iter, 0, &name, -1);
		} else {
			name = NULL;
		}

		nsgtk_theme_set_name(name);

		if (name != NULL) {
			g_free(name);
		}

		while (current)	{
			nsgtk_theme_implement(current);
			current = nsgtk_scaffolding_iterate(current);
		}
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboTheme_realize(GtkWidget *widget, struct ppref *priv)
{
	/* Fill theme list store */
	FILE *fp;
	GtkTreeIter iter;
	char buf[50];
	int combo_row_count = 0;
	int selected_theme = 0;

	if ((priv->themes != NULL) &&
	    (themelist_file_location != NULL) &&
	    ((fp = fopen(themelist_file_location, "r")) != NULL)) {
		gtk_list_store_clear(priv->themes);

		LOG(("Used %s for themelist", themelist_file_location));

		while (fgets(buf, sizeof(buf), fp)) {
			/* Ignore blank lines */
			if (buf[0] == '\0')
				continue;

			/* Remove trailing \n */
			buf[strlen(buf) - 1] = '\0';

			gtk_list_store_append(priv->themes, &iter);
			gtk_list_store_set(priv->themes, &iter, 0, buf, -1);

			combo_row_count++;
		}

		fclose(fp);
	} else {
		LOG(("Failed opening themes file"));
	}

	/* get configured theme and sanity check value */
	selected_theme = nsoption_int(current_theme);
	if (selected_theme > combo_row_count) {
		selected_theme = combo_row_count;
	}
	if (selected_theme < 0) {
		selected_theme = 0;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), selected_theme);
}

/* add theme */
G_MODULE_EXPORT void
nsgtk_preferences_buttonAddTheme_clicked(GtkButton *button, struct ppref *priv)
{
	char *filename, *directory;
	size_t len;
	GtkWidget *fc;
	char *themesfolder;
	gint res;

	fc  = gtk_file_chooser_dialog_new(messages_get("gtkAddThemeTitle"),
					  GTK_WINDOW(priv->dialog),
					  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					  GTK_STOCK_OK,
					  GTK_RESPONSE_ACCEPT,
					  GTK_STOCK_CANCEL,
					  GTK_RESPONSE_CANCEL,
					  NULL);
	len = SLEN("themes") + strlen(res_dir_location) + 1;

	themesfolder = malloc(len);

	snprintf(themesfolder, len, "%sthemes", res_dir_location);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), themesfolder);

	res = gtk_dialog_run(GTK_DIALOG(fc));
	if (res == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(fc));
		if (filename != NULL) {
			if (strcmp(filename, themesfolder) != 0) {
				directory = strrchr(filename, '/');
				*directory = '\0';
				if (strcmp(filename, themesfolder) != 0) {
					warn_user(messages_get(
							  "gtkThemeFolderInstructions"),
						  0);

					if (filename != NULL)
						g_free(filename);

				} else {
					directory++;
					nsgtk_theme_add(directory);
				}
			} else {
				g_free(filename);

				filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));

				if (strcmp(filename, themesfolder) == 0) {
					warn_user(messages_get("gtkThemeFolderSub"),
						  0);
				} else {
					directory = strrchr(filename, '/') + 1;
					nsgtk_theme_add(directory);
				}
			}

			g_free(filename);
		}
	}

	free(themesfolder);

	gtk_widget_destroy(fc);
}

/* Tabs */

/* always show tab bar */
G_MODULE_EXPORT void
nsgtk_preferences_checkShowSingleTab_toggled(GtkToggleButton *togglebutton,
					     struct ppref *priv)
{
	nsoption_set_bool(show_single_tab,
			  gtk_toggle_button_get_active(togglebutton));
	nsgtk_reflow_all_windows();
}

G_MODULE_EXPORT void
nsgtk_preferences_checkShowSingleTab_realize(GtkWidget *widget,
				     struct ppref *priv)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     nsoption_bool(show_single_tab));
}

/* switch to newly opened tabs immediately */
TOGGLEBUTTON_SIGNALS(checkFocusNew, focus_new)

/* newly opened tabs are blank */
TOGGLEBUTTON_SIGNALS(checkNewBlank, new_blank)

/* tab position */
G_MODULE_EXPORT void
nsgtk_preferences_comboTabPosition_changed(GtkComboBox *widget,
					   struct ppref *priv)
{
	nsgtk_scaffolding *current = scaf_list;

	/* set the option */
	nsoption_set_int(position_tab, gtk_combo_box_get_active(widget));

	/* update all notebooks in all scaffolds */
	while (current)	{
		nsgtk_scaffolding_reset_offset(current);

		nsgtk_reflow_all_windows();

		current = nsgtk_scaffolding_iterate(current);
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboTabPosition_realize(GtkWidget *widget,
					   struct ppref *priv)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
				 nsoption_int(position_tab));
}

/* Source */

/* source view opening */
TOGGLEBUTTON_SIGNALS(sourceButtonTab, source_tab)

G_MODULE_EXPORT void
nsgtk_preferences_sourceButtonWindow_toggled(GtkToggleButton *togglebutton,
				     struct ppref *priv)
{
	nsoption_set_bool(source_tab,
			  !gtk_toggle_button_get_active(togglebutton));
}

G_MODULE_EXPORT void
nsgtk_preferences_sourceButtonWindow_realize(GtkWidget *widget,
					   struct ppref *priv)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				     !nsoption_bool(source_tab));
}


/* URLbar */

/* show recently visited urls as you type */
TOGGLEBUTTON_SIGNALS(checkDisplayRecentURLs, url_suggestion)

/* Toolbar */

/* button position */
G_MODULE_EXPORT void
nsgtk_preferences_comboButtonType_changed(GtkComboBox *widget,
					   struct ppref *priv)
{
	nsgtk_scaffolding *current = scaf_list;
	nsoption_set_int(button_type, gtk_combo_box_get_active(widget) + 1);

	/* value of 0 is reserved for 'unset' */
	while (current)	{
		nsgtk_scaffolding_reset_offset(current);
		switch(nsoption_int(button_type)) {
		case 1:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case 2:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_ICONS);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 3:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_BOTH);
			gtk_toolbar_set_icon_size(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		case 4:
			gtk_toolbar_set_style(
				GTK_TOOLBAR(nsgtk_scaffolding_toolbar(current)),
				GTK_TOOLBAR_TEXT);
		default:
			break;
		}
		current = nsgtk_scaffolding_iterate(current);
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboButtonType_realize(GtkWidget *widget,
					   struct ppref *priv)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
				 nsoption_int(button_type) - 1);
}



/************ Main ************/

/* Startup */

/* entry HomePageURL widget */
ENTRY_SIGNALS(entryHomePageURL, homepage_url)

/* put current page into homepage url */
G_MODULE_EXPORT void
nsgtk_preferences_setCurrentPage_clicked(GtkButton *button, struct ppref *priv)
{
	const gchar *url = nsurl_access(browser_window_get_url(priv->bw));

	if (priv->entryHomePageURL != NULL) {
		gtk_entry_set_text(GTK_ENTRY(priv->entryHomePageURL), url);
		nsoption_set_charp(homepage_url, strdup(url));
	}
}

/* put default page into homepage */
G_MODULE_EXPORT void
nsgtk_preferences_setDefaultPage_clicked(GtkButton *button, struct ppref *priv)
{
	const gchar *url = NETSURF_HOMEPAGE;

	if (priv->entryHomePageURL != NULL) {
		gtk_entry_set_text(GTK_ENTRY(priv->entryHomePageURL), url);
		nsoption_set_charp(homepage_url, strdup(url));
	}
}

/* Search */

/* Url Search widget */
TOGGLEBUTTON_SIGNALS(checkUrlSearch, search_url_bar)

/* provider combo */
G_MODULE_EXPORT void
nsgtk_preferences_comboSearch_changed(GtkComboBox *widget, struct ppref *priv)
{
	nsgtk_scaffolding *current = scaf_list;
	char *name;
	int provider;

	provider = gtk_combo_box_get_active(widget);

	/* set the option */
	nsoption_set_int(search_provider, provider);

	/* refresh web search prefs from file */
	search_web_provider_details(provider);

	/* retrieve ico */
	search_web_retrieve_ico(false);

	/* callback may handle changing gui */
	gui_set_search_ico(search_web_ico());

	/* set entry */
	name = search_web_provider_name();
	if (name != NULL) {
		char content[strlen(name) + SLEN("Search ") + 1];

		sprintf(content, "Search %s", name);
		free(name);
		while (current) {
			nsgtk_scaffolding_set_websearch(current, content);
			current = nsgtk_scaffolding_iterate(current);
		}
	}
}

G_MODULE_EXPORT void
nsgtk_preferences_comboSearch_realize(GtkWidget *widget, struct ppref *priv)
{
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget),
				 nsoption_int(search_provider));
}


/* Downloads */

/* clear downloads */
TOGGLEBUTTON_SIGNALS(checkClearDownloads, downloads_clear)

/* request overwite */
TOGGLEBUTTON_SIGNALS(checkRequestOverwrite, request_overwrite)

/* download location
 *
 * note selection-changed is used instead of file-set as the returned
 * filename when that signal are used is incorrect. Though this signal
 * does update frequently often with the same data.
 */
G_MODULE_EXPORT void
nsgtk_preferences_fileChooserDownloads_selectionchanged(GtkFileChooser *chooser,
					       struct ppref *priv)
{
	gchar *dir;
	dir = gtk_file_chooser_get_filename(chooser);
	nsoption_set_charp(downloads_directory, strdup(dir));
	g_free(dir);
}

G_MODULE_EXPORT void
nsgtk_preferences_fileChooserDownloads_realize(GtkWidget *widget,
					       struct ppref *priv)
{
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widget),
					   nsoption_charp(downloads_directory));
}


/************* Dialog window ***********/

/* dialog close and destroy events */
G_MODULE_EXPORT void
nsgtk_preferences_dialogPreferences_response(GtkDialog *dlg, gint resid)
{
	if (resid == GTK_RESPONSE_CLOSE) {
		nsoption_write(options_file_location, NULL, NULL);
		gtk_widget_hide(GTK_WIDGET(dlg));
	}
}

G_MODULE_EXPORT gboolean
nsgtk_preferences_dialogPreferences_deleteevent(GtkDialog *dlg,
						struct ppref *priv)
{
	nsoption_write(options_file_location, NULL, NULL);
	gtk_widget_hide(GTK_WIDGET(dlg));

	/* delt with it by hiding window, no need to destory widget by
	 * default */
	return TRUE;
}

G_MODULE_EXPORT void
nsgtk_preferences_dialogPreferences_destroy(GtkDialog *dlg, struct ppref *priv)
{
	nsoption_write(options_file_location, NULL, NULL);
}


/* exported interface documented in gtk/dialogs/preferences.h */
GtkWidget* nsgtk_preferences(struct browser_window *bw, GtkWindow *parent)
{
	GError *error = NULL;
	GtkBuilder *preferences_builder;
	struct ppref *priv = &ppref;

	priv->bw = bw; /* for setting "current" page */

	/* memoised dialog creation */
	if (priv->dialog != NULL) {
		gtk_window_set_transient_for(GTK_WINDOW(priv->dialog), parent);
		return GTK_WIDGET(priv->dialog);
	}

	/* populate builder object */
	preferences_builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(preferences_builder,
				       glade_file_location->options,
				       &error)) {
		g_warning("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		return NULL;
	}


	priv->dialog = gtk_builder_get_object(preferences_builder,
					       "dialogPreferences");
	if (priv->dialog == NULL) {
		LOG(("Unable to get object for preferences dialog"));
		/* release builder as were done with it */
		g_object_unref(G_OBJECT(preferences_builder));
		return NULL;
	}

	/* need to explicitly obtain handles for some widgets enabling
	 * updates by other widget events
	 */
#define GB(TYPE, NAME) GTK_##TYPE(gtk_builder_get_object(preferences_builder, #NAME))
	priv->entryHomePageURL = GB(ENTRY, entryHomePageURL);
	priv->themes = GB(LIST_STORE, liststore_themes);
	priv->content_language = GB(LIST_STORE, liststore_content_language);
	priv->entryProxyHost = GB(ENTRY, entryProxyHost);
	priv->spinProxyPort = GB(SPIN_BUTTON, spinProxyPort);
	priv->entryProxyUser = GB(ENTRY, entryProxyUser);
	priv->entryProxyPassword = GB(ENTRY, entryProxyPassword);
	priv->entryProxyNoproxy = GB(ENTRY, entryProxyNoproxy);
#undef GB

	/* connect all signals ready to use */
	gtk_builder_connect_signals(preferences_builder, priv);

	/* release builder as were done with it */
	g_object_unref(G_OBJECT(preferences_builder));

	/* mark dialog as transient on parent */
	gtk_window_set_transient_for(GTK_WINDOW(priv->dialog), parent);

	return GTK_WIDGET(priv->dialog);
}

/* exported interface documented in gtk/dialogs/preferences.h */
void nsgtk_preferences_theme_add(const char *themename)
{
	struct ppref *priv = &ppref;
	GtkTreeIter iter;

	gtk_list_store_append(priv->themes, &iter);
	gtk_list_store_set(priv->themes, &iter, 0, themename, -1 );
}
