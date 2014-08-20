/*
 * Copyright 2008 Mike Lester <element3260@gmail.com>
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

#include <string.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include "utils/log.h"
#include "desktop/browser.h"
#include "desktop/gui.h"

#include "gtk/window.h"
 
static GString *current_selection = NULL;
static GtkClipboard *clipboard;


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
	gchar *gtext;

	*buffer = NULL;
	*length = 0;

	/* get clipboard contents from gtk */
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtext = gtk_clipboard_wait_for_text(clipboard); /* conv to utf-8 */

	if (gtext == NULL)
		return;

	*length = strlen(gtext);
	*buffer = malloc(*length);
	if (*buffer == NULL) {
		*length = 0;
		g_free(gtext);
		return;
	}

	memcpy(*buffer, gtext, *length);

	g_free(gtext);
}


/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
		nsclipboard_styles styles[], int n_styles)
{
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

	if (!current_selection)
		current_selection = g_string_new(NULL);
	else
		g_string_set_size(current_selection, 0);

	current_selection = g_string_append_len(current_selection,
			buffer, length);

	gtk_clipboard_set_text(clipboard, current_selection->str, -1);
}
 
static struct gui_clipboard_table clipboard_table = {
	.get = gui_get_clipboard,
	.set = gui_set_clipboard,
};

struct gui_clipboard_table *nsgtk_clipboard_table = &clipboard_table;
