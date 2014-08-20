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

#ifndef _NETSURF_RISCOS_MENUS_H_
#define _NETSURF_RISCOS_MENUS_H_

#include <stdbool.h>
#include "oslib/wimp.h"
#include "riscos/gui.h"

extern wimp_menu *image_quality_menu, *proxy_type_menu, *languages_menu;

extern wimp_menu *current_menu;

typedef enum {

	/* no/unknown actions */
	NO_ACTION,

	/* help actions */
	HELP_OPEN_CONTENTS,
	HELP_OPEN_GUIDE,
	HELP_OPEN_INFORMATION,
	HELP_OPEN_CREDITS,
	HELP_OPEN_LICENCE,
	HELP_LAUNCH_INTERACTIVE,

	/* history actions */
	HISTORY_SHOW_LOCAL,
	HISTORY_SHOW_GLOBAL,

	/* hotlist actions */
	HOTLIST_ADD_URL,
	HOTLIST_SHOW,

	/* cookie actions */
	COOKIES_SHOW,
	COOKIES_DELETE,

	/* page actions */
	BROWSER_PAGE,
	BROWSER_PAGE_INFO,
	BROWSER_PRINT,
	BROWSER_NEW_WINDOW,
	BROWSER_VIEW_SOURCE,

	/* object actions */
	BROWSER_OBJECT,
	BROWSER_OBJECT_OBJECT,
	BROWSER_OBJECT_LINK,
	BROWSER_OBJECT_INFO,
	BROWSER_OBJECT_PRINT,
	BROWSER_OBJECT_RELOAD,
	BROWSER_LINK_SAVE,
	BROWSER_LINK_DOWNLOAD,
	BROWSER_LINK_NEW_WINDOW,

	/* save actions */
	BROWSER_OBJECT_SAVE,
	BROWSER_OBJECT_EXPORT,
	BROWSER_OBJECT_EXPORT_SPRITE,
	BROWSER_OBJECT_EXPORT_DRAW,
	BROWSER_OBJECT_SAVE_URL_URI,
	BROWSER_OBJECT_SAVE_URL_URL,
	BROWSER_OBJECT_SAVE_URL_TEXT,
	BROWSER_SAVE,
	BROWSER_SAVE_COMPLETE,
	BROWSER_EXPORT_DRAW,
	BROWSER_EXPORT_PDF,
	BROWSER_EXPORT_TEXT,
	BROWSER_SAVE_URL_URI,
	BROWSER_SAVE_URL_URL,
	BROWSER_SAVE_URL_TEXT,
	BROWSER_LINK_SAVE_URI,
	BROWSER_LINK_SAVE_URL,
	BROWSER_LINK_SAVE_TEXT,
	HOTLIST_EXPORT,
	HISTORY_EXPORT,

	/* selection actions */
	BROWSER_SELECTION,
	BROWSER_SELECTION_SAVE,
	BROWSER_SELECTION_COPY,
	BROWSER_SELECTION_CUT,
	BROWSER_SELECTION_PASTE,
	BROWSER_SELECTION_CLEAR,
	BROWSER_SELECTION_ALL,

	/* navigation actions */
	BROWSER_NAVIGATE_HOME,
	BROWSER_NAVIGATE_BACK,
	BROWSER_NAVIGATE_FORWARD,
	BROWSER_NAVIGATE_UP,
	BROWSER_NAVIGATE_RELOAD,
	BROWSER_NAVIGATE_RELOAD_ALL,
	BROWSER_NAVIGATE_STOP,
	BROWSER_NAVIGATE_URL,

	/* browser window/display actions */
	BROWSER_SCALE_VIEW,
	BROWSER_FIND_TEXT,
	BROWSER_IMAGES_FOREGROUND,
	BROWSER_IMAGES_BACKGROUND,
	BROWSER_BUFFER_ANIMS,
	BROWSER_BUFFER_ALL,
	BROWSER_SAVE_VIEW,
	BROWSER_WINDOW_DEFAULT,
	BROWSER_WINDOW_STAGGER,
	BROWSER_WINDOW_COPY,
	BROWSER_WINDOW_RESET,

	/* tree actions */
	TREE_NEW_FOLDER,
	TREE_NEW_LINK,
	TREE_EXPAND_ALL,
	TREE_EXPAND_FOLDERS,
	TREE_EXPAND_LINKS,
	TREE_COLLAPSE_ALL,
	TREE_COLLAPSE_FOLDERS,
	TREE_COLLAPSE_LINKS,
	TREE_SELECTION,
	TREE_SELECTION_EDIT,
	TREE_SELECTION_LAUNCH,
	TREE_SELECTION_DELETE,
	TREE_SELECT_ALL,
	TREE_CLEAR_SELECTION,

	/* toolbar actions */
	TOOLBAR_BUTTONS,
	TOOLBAR_ADDRESS_BAR,
	TOOLBAR_THROBBER,
	TOOLBAR_EDIT,

	/* misc actions */
	CHOICES_SHOW,
	APPLICATION_QUIT,
} menu_action;


/* Menu entry structures for use when defining menus. */

struct ns_menu_entry {
	const char *text;		/**< menu text (from messages) */
	menu_action action;		/**< associated action */
	wimp_w *sub_window;		/**< sub-window if any */
};

struct ns_menu {
	const char *title;
	struct ns_menu_entry entries[];
};


void ro_gui_menu_init(void);
void ro_gui_menu_create(wimp_menu* menu, int x, int y, wimp_w w);
void ro_gui_menu_destroy(void);
void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i);
void ro_gui_menu_window_changed(wimp_w from, wimp_w to);
void ro_gui_menu_selection(wimp_selection* selection);
void ro_gui_menu_warning(wimp_message_menu_warning *warning);
void ro_gui_menu_message_deleted(wimp_message_menus_deleted *deleted);
void ro_gui_menu_refresh(wimp_menu *menu);
void ro_gui_menu_init_structure(wimp_menu *menu, int entries);
const char *ro_gui_menu_find_menu_entry_key(wimp_menu *menu,
		const char *translated);
wimp_menu *ro_gui_menu_define_menu(const struct ns_menu *menu);
void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded);
void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked);

#endif
