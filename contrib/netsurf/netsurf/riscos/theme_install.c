/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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
 * Theme auto-installing.
 */

#include <assert.h>
#include <stdbool.h>
#include "oslib/osfile.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "utils/nsoption.h"
#include "riscos/theme.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"


static hlcache_handle *theme_install_content = NULL;
static struct theme_descriptor theme_install_descriptor;
wimp_w dialog_theme_install;


static void theme_install_close(wimp_w w);
static nserror theme_install_callback(hlcache_handle *handle,
		const hlcache_event *event, void *pw);
static bool theme_install_read(const char *source_data, 
		unsigned long source_size);


/**
 * Handle a CONTENT_THEME that has started loading.
 */

void theme_install_start(hlcache_handle *c)
{
	assert(c != NULL);
	assert(content_get_type(c) == CONTENT_THEME);

	if (ro_gui_dialog_open_top(dialog_theme_install, NULL, 0, 0)) {
		warn_user("ThemeInstActive", 0);
		return;
	}

	/* stop theme sitting in memory cache */
	content_invalidate_reuse_data(c);

	hlcache_handle_replace_callback(c, theme_install_callback, NULL);

	ro_gui_set_icon_string(dialog_theme_install, ICON_THEME_INSTALL_MESSAGE,
			messages_get("ThemeInstDown"), true);
	ro_gui_set_icon_shaded_state(dialog_theme_install,
			ICON_THEME_INSTALL_INSTALL, true);
	ro_gui_wimp_event_register_close_window(dialog_theme_install,
			theme_install_close);
}


/**
 * Callback for fetchcache() for theme install fetches.
 */

nserror theme_install_callback(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	char buffer[256];
	int author_indent = 0;

	switch (event->type) {

	case CONTENT_MSG_DONE:
	{
		const char *source_data;
		unsigned long source_size;

		theme_install_content = handle;

		source_data = content_get_source_data(handle, &source_size);

		if (!theme_install_read(source_data, source_size)) {
			warn_user("ThemeInvalid", 0);
			theme_install_close(dialog_theme_install);
			break;
		}

		/* remove '© ' from the start of the data */
		if (theme_install_descriptor.author[0] == '©')
			author_indent++;
		while (theme_install_descriptor.author[author_indent] == ' ')
			author_indent++;
		snprintf(buffer, sizeof buffer, messages_get("ThemeInstall"),
				theme_install_descriptor.name,
				&theme_install_descriptor.author[author_indent]);
		buffer[sizeof buffer - 1] = '\0';
		ro_gui_set_icon_string(dialog_theme_install,
				ICON_THEME_INSTALL_MESSAGE,
				buffer, true);
		ro_gui_set_icon_shaded_state(dialog_theme_install,
				ICON_THEME_INSTALL_INSTALL, false);
	}
		break;

	case CONTENT_MSG_ERROR:
		theme_install_close(dialog_theme_install);
		warn_user(event->data.error, 0);
		break;

	default:
		break;
	}

	return NSERROR_OK;
}


/**
 * Fill in theme_install_descriptor from received theme data.
 *
 * \param  source_data  received data
 * \param  source_size  size of data
 * \return  true if data is a correct theme, false on error
 *
 * If the data is a correct theme, theme_install_descriptor is filled in.
 */

bool theme_install_read(const char *source_data, unsigned long source_size)
{
	const void *data = source_data;

	if (source_size < sizeof(struct theme_file_header))
		return false;
	if (!ro_gui_theme_read_file_header(&theme_install_descriptor,
			(struct theme_file_header *) data))
		return false;
	if (source_size - sizeof(struct theme_file_header) !=
			theme_install_descriptor.compressed_size)
		return false;
	return true;
}


/**
 * Install the downloaded theme.
 *
 * \param  w	the theme install window handle
 */

bool ro_gui_theme_install_apply(wimp_w w)
{
	char theme_save[256];
	char *theme_file;
	struct theme_descriptor *theme_install;
	os_error *error;
	char *fix;
	const char *source_data;
	unsigned long source_size;

	assert(theme_install_content);

	/* convert spaces to hard spaces */
	theme_file = strdup(theme_install_descriptor.name);
	if (!theme_file) {
	  	LOG(("malloc failed"));
	  	warn_user("NoMemory", 0);
		return false;
	}
	for (fix = theme_file; *fix != '\0'; fix++)
		if (*fix == ' ')
			*fix = 160;	/* hard space */

	/* simply overwrite previous theme versions */
	snprintf(theme_save, sizeof theme_save, "%s.%s",
                 nsoption_charp(theme_save), theme_file);

	theme_save[sizeof theme_save - 1] = '\0';

	source_data = content_get_source_data(theme_install_content, 
			&source_size);

	error = xosfile_save_stamped(theme_save, 0xffd,
			(byte *) source_data,
			(byte *) source_data + source_size);
	if (error) {
		LOG(("xosfile_save_stamped: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("ThemeInstallErr", 0);
		free(theme_file);
		return false;
	}

	/* apply the new theme */
	ro_gui_theme_get_available();
	theme_install = ro_gui_theme_find(theme_file);
	if (!theme_install || !ro_gui_theme_apply(theme_install)) {
		warn_user("ThemeApplyErr", 0);
	} else {
            nsoption_set_charp(theme, strdup(theme_install->leafname));
	}
	free(theme_file);
	ro_gui_save_options();
	return true;
}


/**
 * Close the theme installer and free resources.
 */

void theme_install_close(wimp_w w)
{
	if (theme_install_content)
		hlcache_handle_release(theme_install_content);

	theme_install_content = NULL;
}
