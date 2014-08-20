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

#include <gtk/gtk.h>
#include "gtk/completion.h"

#include "content/urldb.h"
#include "utils/log.h"
#include "utils/nsoption.h"

GtkListStore *nsgtk_completion_list;

static void nsgtk_completion_empty(void);
static bool nsgtk_completion_udb_callback(nsurl *url,
		const struct url_data *data);

void nsgtk_completion_init(void)
{
	nsgtk_completion_list = gtk_list_store_new(1, G_TYPE_STRING);

}

gboolean nsgtk_completion_match(GtkEntryCompletion *completion,
                                const gchar *key,
                                GtkTreeIter *iter,
                                gpointer user_data)
{
	char *b[4096];		/* no way of finding out its length :( */
	gtk_tree_model_get(GTK_TREE_MODEL(nsgtk_completion_list), iter,
			0, b, -1);

	/* TODO: work out why this works, when there's no code to implement
	 * it.  I boggle. */

	return TRUE;

}

void nsgtk_completion_empty(void)
{
  	gtk_list_store_clear(nsgtk_completion_list);
}

bool nsgtk_completion_udb_callback(nsurl *url, const struct url_data *data)
{
	GtkTreeIter iter;

	if (data->visits != 0) {
		gtk_list_store_append(nsgtk_completion_list, &iter);
		gtk_list_store_set(nsgtk_completion_list, &iter, 0,
				nsurl_access(url), -1);
	}
	return true;
}

void nsgtk_completion_update(const char *prefix)
{
	nsgtk_completion_empty();
	if (nsoption_bool(url_suggestion) == true) {
		urldb_iterate_partial(prefix, nsgtk_completion_udb_callback);
	}
}
