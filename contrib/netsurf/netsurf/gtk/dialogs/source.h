/*
 * Copyright 2009 Mark Benjamin <netsurfbrowser.org.MarkBenjamin@dfgh.net>
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

#ifndef netsurf_gtk_dialogs_source_h_
#define netsurf_gtk_dialogs_source_h_

#include <gtk/gtk.h>
#include "desktop/browser.h"

void nsgtk_source_dialog_init(GtkWindow *parent, struct browser_window *bw);

#endif

