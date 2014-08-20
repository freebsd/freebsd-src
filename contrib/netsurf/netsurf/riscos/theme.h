/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Window themes(interface).
 */

#include <stdbool.h>
#include "oslib/osspriteop.h"

#ifndef _NETSURF_RISCOS_THEME_H_
#define _NETSURF_RISCOS_THEME_H_

/** Theme styles, collecting groups of attributes for different locations. */

typedef enum {
	THEME_STYLE_NONE = 0,
	THEME_STYLE_BROWSER_TOOLBAR,
	THEME_STYLE_HOTLIST_TOOLBAR,
	THEME_STYLE_COOKIES_TOOLBAR,
	THEME_STYLE_GLOBAL_HISTORY_TOOLBAR,
	THEME_STYLE_STATUS_BAR
} theme_style;

/** Theme elements, which belong to styles. */

typedef enum {
	THEME_ELEMENT_FOREGROUND,
	THEME_ELEMENT_BACKGROUND
} theme_element;

struct theme_file_header {
	unsigned int magic_value;
	unsigned int parser_version;
	char name[32];
	char author[64];
	char browser_bg;
	char hotlist_bg;
	char status_bg;
	char status_fg;
	char theme_flags;
	char future_expansion_1;
	char future_expansion_2;
	char future_expansion_3;
	unsigned int compressed_sprite_size;
	unsigned int decompressed_sprite_size;
};

struct theme {
	osspriteop_area *sprite_area;		/**< sprite area for theme */
	int throbber_width;			/**< width of the throbber */
	int throbber_height;			/**< height of the throbber */
	int throbber_frames;			/**< frames of animation for the throbber */
	int users;				/**< number of users for the theme */
};

struct theme_descriptor {
	char *leafname;				/**< theme leafname */
	char *filename;				/**< theme filename */
	char name[32];				/**< theme name */
	char author[64];			/**< theme author */
	int browser_background;			/**< background colour of browser toolbar */
	int hotlist_background;			/**< background colour of hotlist toolbar */
	int status_background;			/**< background colour of status window */
	int status_foreground;			/**< colour of status window text */
	bool throbber_right;			/**< throbber is on the right (left otherwise) */
	bool throbber_redraw;			/**< throbber requires forcible updating */
	unsigned int decompressed_size;		/**< decompressed sprite size */
	unsigned int compressed_size;		/**< compressed sprite size */
	struct theme *theme;			/**< corresponding theme (must be opened) */
	struct theme_descriptor *previous;	/**< previous descriptor in the list */
	struct theme_descriptor *next;		/**< next descriptor in the list */
};

void ro_gui_theme_initialise(void);
void ro_gui_theme_finalise(void);
struct theme_descriptor *ro_gui_theme_find(const char *leafname);
struct theme_descriptor *ro_gui_theme_get_available(void);
struct theme_descriptor *ro_gui_theme_get_current(void);
osspriteop_area *ro_gui_theme_get_sprites(struct theme_descriptor *descriptor);
int ro_gui_theme_get_style_element(struct theme_descriptor *descriptor,
		theme_style style, theme_element element);
bool ro_gui_theme_get_throbber_data(struct theme_descriptor *descriptor,
		int *frames, int *width, int *height,
		bool *right, bool *redraw);

bool ro_gui_theme_read_file_header(struct theme_descriptor *descriptor,
		struct theme_file_header *file_header);

bool ro_gui_theme_open(struct theme_descriptor *descriptor, bool list);
bool ro_gui_theme_apply(struct theme_descriptor *descriptor);
void ro_gui_theme_close(struct theme_descriptor *descriptor, bool list);
#endif

