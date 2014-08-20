/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net> 
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

#ifndef __NSGTK_HISTORY_H__
#define __NSGTK_HISTORY_H__

#include <gtk/gtk.h>

extern GtkWindow *wndHistory;

/**
 * Creates the window for the global history tree.
 *
 * \param glade_file_location The glade file to use for the window.
 * \return true on success false on faliure.
 */
bool nsgtk_history_init(const char *glade_file_location);

/**
 * Free global resources associated with the gtk history window.
 */
void nsgtk_history_destroy(void);

#endif /* __NSGTK_HISTORY_H__ */
