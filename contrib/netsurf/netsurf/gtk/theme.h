/*
 * Copyright 2009 Mark Benjamin <netsurf-browser.org.MarkBenjamin@dfgh.net>
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

#ifndef _NETSURF_GTK_THEME_H_
#define _NETSURF_GTK_THEME_H_

#include <gtk/gtk.h>
#include "gtk/scaffolding.h"

typedef enum search_buttons {
	SEARCH_BACK_BUTTON = 0,
	SEARCH_FORWARD_BUTTON,
	SEARCH_CLOSE_BUTTON,
	SEARCH_BUTTONS_COUNT
} nsgtk_search_buttons;

struct nsgtk_theme {
	GtkImage	*image[PLACEHOLDER_BUTTON];
	GtkImage	*searchimage[SEARCH_BUTTONS_COUNT];
	/* apng throbber element */
};

struct nsgtk_theme *nsgtk_theme_load(GtkIconSize s);
void nsgtk_theme_add(const char *themename);
void nsgtk_theme_init(void);
void nsgtk_theme_prepare(void);
void nsgtk_theme_implement(struct gtk_scaffolding *g);
char *nsgtk_theme_name(void);
void nsgtk_theme_set_name(const char *name);

#endif
