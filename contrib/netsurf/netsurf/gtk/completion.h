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

#ifndef _NETSURF_GTK_COMPLETION_H_
#define _NETSURF_GTK_COMPLETION_H_

#include <gtk/gtk.h>

extern GtkListStore *nsgtk_completion_list;

void nsgtk_completion_init(void);
void nsgtk_completion_update(const char *prefix);
gboolean nsgtk_completion_match(GtkEntryCompletion *completion,
				const gchar *key,
				GtkTreeIter *iter,
				gpointer user_data);
#endif
