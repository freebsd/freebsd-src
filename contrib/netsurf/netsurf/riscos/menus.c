/*
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
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
 * Menu creation and handling (implementation).
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/osgbpb.h"
#include "oslib/territory.h"
#include "oslib/wimp.h"

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "utils/utils.h"
#include "utils/utf8.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/cookie_manager.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/textinput.h"

#include "riscos/dialog.h"
#include "riscos/configure.h"
#include "riscos/cookies.h"
#include "riscos/gui.h"
#include "riscos/global_history.h"
#include "riscos/help.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "utils/nsoption.h"
#include "riscos/save.h"
#include "riscos/tinct.h"
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/url_suggest.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/ucstables.h"

struct menu_definition_entry {
	menu_action action;			/**< menu action */
	wimp_menu_entry *menu_entry;		/**< corresponding menu entry */
	const char *entry_key;			/**< Messages key for entry text */
	struct menu_definition_entry *next;	/**< next menu entry */
};

struct menu_definition {
	wimp_menu *menu;			/**< corresponding menu */
	const char *title_key;			/**< Messages key for title text */
	int current_encoding;			/**< Identifier for current text encoding of menu text (as per OS_Byte,71,127) */
	struct menu_definition_entry *entries;	/**< menu entries */
	struct menu_definition *next;		/**< next menu */
};

static void ro_gui_menu_closed(void);
static void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		const struct ns_menu *menu, int depth,
		wimp_menu_entry *parent_entry,
		int first, int last, const char *prefix, int prefix_length);
static struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu);
static struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action);
static menu_action ro_gui_menu_find_action(wimp_menu *menu,
		wimp_menu_entry *menu_entry);
static int ro_gui_menu_get_checksum(void);
static bool ro_gui_menu_translate(struct menu_definition *menu);


/* default menu item flags */
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))

/** The currently defined menus to perform actions for */
static struct menu_definition *ro_gui_menu_definitions;
/** The current menu being worked with (may not be open) */
wimp_menu *current_menu;
/** Whether a menu is currently open */
bool current_menu_open = false;
/** Window that owns the current menu */
wimp_w current_menu_window;
/** Icon that owns the current menu (only valid for popup menus) */
static wimp_i current_menu_icon;
/** The available menus */
wimp_menu *image_quality_menu, *proxy_type_menu, *languages_menu;

/* the values given in PRM 3-157 for how to check menus/windows are
 * incorrect so we use a hack of checking if the sub-menu has bit 0
 * set which is undocumented but true of window handles on
 * all target OS versions */
#define IS_MENU(menu) !((int)(menu) & 1)

/**
 * Create menu structures.
 */

void ro_gui_menu_init(void)
{
	/* image quality menu */
	static const struct ns_menu images_definition = {
		"Display", {
			{ "ImgStyle0", NO_ACTION, 0 },
			{ "ImgStyle1", NO_ACTION, 0 },
			{ "ImgStyle2", NO_ACTION, 0 },
			{ "ImgStyle3", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	image_quality_menu = ro_gui_menu_define_menu(&images_definition);

	/* proxy menu */
	static const struct ns_menu proxy_type_definition = {
		"ProxyType", {
			{ "ProxyNone", NO_ACTION, 0 },
			{ "ProxyNoAuth", NO_ACTION, 0 },
			{ "ProxyBasic", NO_ACTION, 0 },
			{ "ProxyNTLM", NO_ACTION, 0 },
			{NULL, 0, 0}
		}
	};
	proxy_type_menu = ro_gui_menu_define_menu(&proxy_type_definition);

	/* special case menus */
	ro_gui_url_suggest_init();

	/* Note: This table *must* be kept in sync with the LangNames file */
	static const struct ns_menu lang_definition = {
		"Languages", {
			{ "lang_af", NO_ACTION, 0 },
			{ "lang_bm", NO_ACTION, 0 },
			{ "lang_ca", NO_ACTION, 0 },
			{ "lang_cs", NO_ACTION, 0 },
			{ "lang_cy", NO_ACTION, 0 },
			{ "lang_da", NO_ACTION, 0 },
			{ "lang_de", NO_ACTION, 0 },
			{ "lang_en", NO_ACTION, 0 },
			{ "lang_es", NO_ACTION, 0 },
			{ "lang_et", NO_ACTION, 0 },
			{ "lang_eu", NO_ACTION, 0 },
			{ "lang_ff", NO_ACTION, 0 },
			{ "lang_fi", NO_ACTION, 0 },
			{ "lang_fr", NO_ACTION, 0 },
			{ "lang_ga", NO_ACTION, 0 },
			{ "lang_gl", NO_ACTION, 0 },
			{ "lang_ha", NO_ACTION, 0 },
			{ "lang_hr", NO_ACTION, 0 },
			{ "lang_hu", NO_ACTION, 0 },
			{ "lang_id", NO_ACTION, 0 },
			{ "lang_is", NO_ACTION, 0 },
			{ "lang_it", NO_ACTION, 0 },
			{ "lang_lt", NO_ACTION, 0 },
			{ "lang_lv", NO_ACTION, 0 },
			{ "lang_ms", NO_ACTION, 0 },
			{ "lang_mt", NO_ACTION, 0 },
			{ "lang_nl", NO_ACTION, 0 },
			{ "lang_no", NO_ACTION, 0 },
			{ "lang_pl", NO_ACTION, 0 },
			{ "lang_pt", NO_ACTION, 0 },
			{ "lang_rn", NO_ACTION, 0 },
			{ "lang_ro", NO_ACTION, 0 },
			{ "lang_rw", NO_ACTION, 0 },
			{ "lang_sk", NO_ACTION, 0 },
			{ "lang_sl", NO_ACTION, 0 },
			{ "lang_so", NO_ACTION, 0 },
			{ "lang_sq", NO_ACTION, 0 },
			{ "lang_sr", NO_ACTION, 0 },
			{ "lang_sv", NO_ACTION, 0 },
			{ "lang_sw", NO_ACTION, 0 },
			{ "lang_tr", NO_ACTION, 0 },
			{ "lang_uz", NO_ACTION, 0 },
			{ "lang_vi", NO_ACTION, 0 },
			{ "lang_wo", NO_ACTION, 0 },
			{ "lang_xs", NO_ACTION, 0 },
			{ "lang_yo", NO_ACTION, 0 },
			{ "lang_zu", NO_ACTION, 0 },
			{ NULL, 0, 0 }
		}
	};
	languages_menu = ro_gui_menu_define_menu(&lang_definition);
}


/**
 * Display a menu.
 *
 * \param *menu			Pointer to the menu to be displayed.
 * \param x			The x position.
 * \param y			The y position.
 * \param w			The window that the menu belongs to.
 */

void ro_gui_menu_create(wimp_menu *menu, int x, int y, wimp_w w)
{
	os_error *error;
	struct menu_definition *definition;

	/* translate menu, if necessary (this returns quickly
	 * if there's nothing to be done) */
	definition = ro_gui_menu_find_menu(menu);
	if (definition) {
		if (!ro_gui_menu_translate(definition)) {
			warn_user("NoMemory", 0);
			return;
		}
	}

	/* store the menu characteristics */
	current_menu = menu;
	current_menu_window = w;
	current_menu_icon = wimp_ICON_WINDOW;

	/* create the menu */
	current_menu_open = true;
	error = xwimp_create_menu(menu, x - 64, y);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		ro_gui_menu_closed();
	}
}


/**
 * Display a pop-up menu next to the specified icon.
 *
 * \param  menu  menu to open
 * \param  w	 window handle
 * \param  i	 icon handle
 */

void ro_gui_popup_menu(wimp_menu *menu, wimp_w w, wimp_i i)
{
	wimp_window_state state;
	wimp_icon_state icon_state;
	os_error *error;

	state.w = w;
	icon_state.w = w;
	icon_state.i = i;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	error = xwimp_get_icon_state(&icon_state);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return;
	}

	ro_gui_menu_create(menu,
			state.visible.x0 + icon_state.icon.extent.x1 + 64,
			state.visible.y1 + icon_state.icon.extent.y1 -
			state.yscroll, w);
	current_menu_icon = i;
}


/**
 * Forcibly close any menu or transient dialogue box that is currently open.
 */

void ro_gui_menu_destroy(void)
{
	os_error *error;

	if (current_menu == NULL)
		return;

	error = xwimp_create_menu(wimp_CLOSE_MENU, 0, 0);
	if (error) {
		LOG(("xwimp_create_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}

	ro_gui_menu_closed();
}


/**
 * Allow the current menu window to change, if the window is deleted and
 * recreated while a menu is active on an Adjust-click.
 *
 * \param from			The original window handle.
 * \param to			The new replacement window handle.
 */

void ro_gui_menu_window_changed(wimp_w from, wimp_w to)
{

	if (from == current_menu_window)
		current_menu_window = to;
}


/**
 * Handle menu selection.
 */

void ro_gui_menu_selection(wimp_selection *selection)
{
	int			i; //, j;
	wimp_menu_entry		*menu_entry;
	menu_action		action;
	wimp_pointer		pointer;
	os_error		*error;
	int			previous_menu_icon = current_menu_icon;

	/* if we are using gui_multitask then menu selection events
	 * may be delivered after the menu has been closed. As such,
	 * we simply ignore these events. */
	if (!current_menu)
		return;

	assert(current_menu_window);

	/* get the menu entry and associated action and definition */
	menu_entry = &current_menu->entries[selection->items[0]];
	for (i = 1; selection->items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[selection->items[i]];
	action = ro_gui_menu_find_action(current_menu, menu_entry);

	/* Deal with the menu action.  If this manages to re-prepare the
	 * menu for re-opening, we test for and act on Adjust clicks.
	 */

	if (!ro_gui_wimp_event_menu_selection(current_menu_window,
			current_menu_icon, current_menu, selection, action))
		return;

	/* re-open the menu for Adjust clicks */
	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		ro_gui_menu_closed();
		return;
	}

	if (pointer.buttons != wimp_CLICK_ADJUST) {
		ro_gui_menu_closed();
		return;
	}

	ro_gui_menu_create(current_menu, 0, 0, current_menu_window);
	current_menu_icon = previous_menu_icon;
}


/**
 * Handle Message_MenuWarning.
 */
void ro_gui_menu_warning(wimp_message_menu_warning *warning)
{
	int i;
	menu_action action;
	wimp_menu_entry *menu_entry;
	os_error *error;

	assert(current_menu);
	assert(current_menu_window);

	/* get the sub-menu of the warning */
	if (warning->selection.items[0] == -1)
		return;
	menu_entry = &current_menu->entries[warning->selection.items[0]];
	for (i = 1; warning->selection.items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[warning->selection.items[i]];
	action = ro_gui_menu_find_action(current_menu, menu_entry);

	/* Process the warning via Wimp_Event, then register the resulting
	 * submenu with the module.
	 */

	ro_gui_wimp_event_submenu_warning(current_menu_window,
			current_menu_icon, current_menu, &(warning->selection),
			action);

	if (IS_MENU(menu_entry->sub_menu)) {
		ro_gui_wimp_event_register_submenu((wimp_w) 0);
	} else {
		ro_gui_wimp_event_register_submenu((wimp_w)
				menu_entry->sub_menu);

		/* If this is a dialogue box, remove the close and back icons.
		 */

		ro_gui_wimp_update_window_furniture((wimp_w)
				menu_entry->sub_menu,
				wimp_WINDOW_CLOSE_ICON | wimp_WINDOW_BACK_ICON,
				0);
	}

	/* open the sub-menu */

	error = xwimp_create_sub_menu(menu_entry->sub_menu,
			warning->pos.x, warning->pos.y);
	if (error) {
		LOG(("xwimp_create_sub_menu: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
	}
}


/**
 * Handle Message_MenusDeleted, removing our current record of an open menu
 * if it matches the deleted menu handle.
 *
 * \param *deleted		The message block.
 */

void ro_gui_menu_message_deleted(wimp_message_menus_deleted *deleted)
{
	if (deleted != NULL && deleted->menu == current_menu)
		ro_gui_menu_closed();
}


/**
 * Clean up after a menu has been closed, or forcibly close an open menu.
  */

static void ro_gui_menu_closed(void)
{
	if (current_menu != NULL)
		ro_gui_wimp_event_menus_closed(current_menu_window,
				current_menu_icon, current_menu);

	current_menu = NULL;
	current_menu_window = NULL;
	current_menu_icon = 0;
	current_menu_open = false;
}


/**
 * Update the current menu by sending it a Menu Prepare event through wimp_event
 * and then reopening it if the contents has changed.
 *
 * \param *menu		The menu to refresh: if 0, the current menu will be
 *			refreshed regardless, otherwise it will be refreshed
 *			only if it matches the supplied handle.
 */

void ro_gui_menu_refresh(wimp_menu *menu)
{
	int checksum = 0;
	os_error *error;

	if (!current_menu_open)
		return;

	checksum = ro_gui_menu_get_checksum();

	if (!ro_gui_wimp_event_prepare_menu(current_menu_window,
			current_menu_icon, current_menu))
		return;

	/* \TODO -- Call the menu's event handler here. */

	if (checksum != ro_gui_menu_get_checksum()) {
		error = xwimp_create_menu(current_menu, 0, 0);
		if (error) {
			LOG(("xwimp_create_menu: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MenuError", error->errmess);
		}
	}
}




/**
 * Creates a wimp_menu and adds it to the list to handle actions for.
 *
 * \param  *menu		The data to create the menu with
 * \param  *callbacks		A callback table for the menu (NULL if to be
 *				handled in the 'old-fashined way' by menus.c).
 * \return			The menu created, or NULL on failure
 */
wimp_menu *ro_gui_menu_define_menu(const struct ns_menu *menu)
{
	struct menu_definition *definition;
	int entry;

	definition = calloc(sizeof(struct menu_definition), 1);
	if (!definition) {
		die("No memory to create menu definition.");
		return NULL; /* For the benefit of scan-build */
	}

	/* link in the menu to our list */
	definition->next = ro_gui_menu_definitions;
	ro_gui_menu_definitions = definition;

	/* count number of menu entries */
	for (entry = 0; menu->entries[entry].text; entry++)
		/* do nothing */;

	/* create our definitions */
	ro_gui_menu_define_menu_add(definition, menu, 0, NULL,
			0, entry, NULL, 0);

	/* and translate menu into current encoding */
	if (!ro_gui_menu_translate(definition))
		die("No memory to translate menu.");

	return definition->menu;
}

/**
 * Create a wimp menu tree from ns_menu data.
 * This function does *not* deal with the menu textual content - it simply
 * creates and populates the appropriate structures. Textual content is
 * generated by ro_gui_menu_translate_menu()
 *
 * \param definition  Top level menu definition
 * \param menu  Menu declaration data
 * \param depth  Depth of menu we're currently building
 * \param parent_entry  Entry in parent menu, or NULL if root menu
 * \param first  First index in declaration data that is used by this menu
 * \param last Last index in declaration data that is used by this menu
 * \param prefix  Prefix pf menu declaration string already seen
 * \param prefix_length  Length of prefix
 */
void ro_gui_menu_define_menu_add(struct menu_definition *definition,
		const struct ns_menu *menu, int depth,
		wimp_menu_entry *parent_entry, int first, int last,
		const char *prefix, int prefix_length)
{
	int entry, id, cur_depth;
	int entries = 0;
	int matches[last - first + 1];
	const char *match;
	const char *text, *menu_text;
	wimp_menu *new_menu;
	struct menu_definition_entry *definition_entry;

	/* step 1: store the matches for depth and subset string */
	for (entry = first; entry < last; entry++) {
		cur_depth = 0;
		match = menu->entries[entry].text;

		/* skip specials at start of string */
		while (!isalnum(*match))
			match++;

		/* attempt prefix match */
		if ((prefix) && (strncmp(match, prefix, prefix_length)))
			continue;

		/* Find depth of this entry */
		while (*match)
			if (*match++ == '.')
				cur_depth++;

		if (depth == cur_depth)
			matches[entries++] = entry;
	}
	matches[entries] = last;

	/* no entries, so exit */
	if (entries == 0)
		return;

	/* step 2: build and link the menu. we must use realloc to stop
	 * our memory fragmenting so we can test for sub-menus easily */
	new_menu = (wimp_menu *)malloc(wimp_SIZEOF_MENU(entries));
	if (!new_menu)
		die("No memory to create menu.");

	if (parent_entry) {
		/* Fix up sub menu pointer */
		parent_entry->sub_menu = new_menu;
	} else {
		/* Root menu => fill in definition struct */
		definition->title_key = menu->title;
		definition->current_encoding = 0;
		definition->menu = new_menu;
	}

	/* this is fixed up in ro_gui_menu_translate() */
	new_menu->title_data.indirected_text.text = NULL;

	/* fill in menu flags */
	ro_gui_menu_init_structure(new_menu, entries);

	/* and then create the entries */
	for (entry = 0; entry < entries; entry++) {
		/* add the entry */
		id = matches[entry];

		text = menu->entries[id].text;

		/* fill in menu flags from specials at start of string */
		new_menu->entries[entry].menu_flags = 0;
		while (!isalnum(*text)) {
			if (*text == '_')
				new_menu->entries[entry].menu_flags |=
							wimp_MENU_SEPARATE;
			text++;
		}

		/* get messages key for menu entry */
		menu_text = strrchr(text, '.');
		if (!menu_text)
			/* no '.' => top-level entry */
			menu_text = text;
		else
			menu_text++; /* and move past the '.' */

		/* fill in submenu data */
		if (menu->entries[id].sub_window)
			new_menu->entries[entry].sub_menu =
				(wimp_menu *) (*menu->entries[id].sub_window);

		/* this is fixed up in ro_gui_menu_translate() */
		new_menu->entries[entry].data.indirected_text.text = NULL;

		/* create definition entry */
		definition_entry =
			malloc(sizeof(struct menu_definition_entry));
		if (!definition_entry)
			die("Unable to create menu definition entry");
		definition_entry->action = menu->entries[id].action;
		definition_entry->menu_entry = &new_menu->entries[entry];
		definition_entry->entry_key = menu_text;
		definition_entry->next = definition->entries;
		definition->entries = definition_entry;

		/* recurse */
		if (new_menu->entries[entry].sub_menu == wimp_NO_SUB_MENU) {
			ro_gui_menu_define_menu_add(definition, menu,
					depth + 1, &new_menu->entries[entry],
					matches[entry], matches[entry + 1],
					text, strlen(text));
		}

		/* give menu warnings */
		if (new_menu->entries[entry].sub_menu != wimp_NO_SUB_MENU)
			new_menu->entries[entry].menu_flags |=
						wimp_MENU_GIVE_WARNING;
	}
	new_menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	new_menu->entries[entries - 1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Initialise the basic state of a menu structure so all entries are
 * indirected text with no flags, no submenu.
 */
void ro_gui_menu_init_structure(wimp_menu *menu, int entries)
{
  	int i;

	menu->title_fg = wimp_COLOUR_BLACK;
	menu->title_bg = wimp_COLOUR_LIGHT_GREY;
	menu->work_fg = wimp_COLOUR_BLACK;
	menu->work_bg = wimp_COLOUR_WHITE;
	menu->width = 200;
	menu->height = wimp_MENU_ITEM_HEIGHT;
	menu->gap = wimp_MENU_ITEM_GAP;

	for (i = 0; i < entries; i++) {
		menu->entries[i].menu_flags = 0;
		menu->entries[i].sub_menu = wimp_NO_SUB_MENU;
		menu->entries[i].icon_flags =
				DEFAULT_FLAGS | wimp_ICON_INDIRECTED;
		menu->entries[i].data.indirected_text.validation =
				(char *)-1;
	}
	menu->entries[0].menu_flags |= wimp_MENU_TITLE_INDIRECTED;
	menu->entries[i - 1].menu_flags |= wimp_MENU_LAST;
}


/**
 * Finds the menu_definition corresponding to a wimp_menu.
 *
 * \param menu  the menu to find the definition for
 * \return the associated definition, or NULL if one could not be found
 */
struct menu_definition *ro_gui_menu_find_menu(wimp_menu *menu)
{
	struct menu_definition *definition;

	if (!menu)
		return NULL;

	for (definition = ro_gui_menu_definitions; definition;
			definition = definition->next)
		if (definition->menu == menu)
			return definition;
	return NULL;
}


/**
 * Finds the key associated with a menu entry translation.
 *
 * \param menu  the menu to search
 * \param translated  the translated text
 * \return the original message key, or NULL if one could not be found
 */
const char *ro_gui_menu_find_menu_entry_key(wimp_menu *menu,
		const char *translated)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NULL;

	for (entry = definition->entries; entry; entry = entry->next)
		if (!strcmp(entry->menu_entry->data.indirected_text.text, translated))
			return entry->entry_key;
	return NULL;
}


/**
 * Finds the menu_definition_entry corresponding to an action for a wimp_menu.
 *
 * \param menu	  the menu to search for an action within
 * \param action  the action to find
 * \return the associated menu entry, or NULL if one could not be found
 */
struct menu_definition_entry *ro_gui_menu_find_entry(wimp_menu *menu,
		menu_action action)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NULL;

	for (entry = definition->entries; entry; entry = entry->next)
		if (entry->action == action)
			return entry;
	return NULL;
}


/**
 * Finds the action corresponding to a wimp_menu_entry for a wimp_menu.
 *
 * \param menu	      the menu to search for an action within
 * \param menu_entry  the menu_entry to find
 * \return the associated action, or 0 if one could not be found
 */
menu_action ro_gui_menu_find_action(wimp_menu *menu, wimp_menu_entry *menu_entry)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return NO_ACTION;

	for (entry = definition->entries; entry; entry = entry->next) {
		if (entry->menu_entry == menu_entry)
			return entry->action;
	}
	return NO_ACTION;
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_shaded(wimp_menu *menu, menu_action action,
		bool shaded)
{
	struct menu_definition_entry *entry;
	struct menu_definition *definition = ro_gui_menu_find_menu(menu);

	if (!definition)
		return;

	/* we can't use find_entry as multiple actions may appear in one menu */
	for (entry = definition->entries; entry; entry = entry->next)
		if (entry->action == action) {
			if (shaded)
				entry->menu_entry->icon_flags |= wimp_ICON_SHADED;
			else
				entry->menu_entry->icon_flags &= ~wimp_ICON_SHADED;
		}
}


/**
 * Sets an action within a menu as having a specific ticked status.
 *
 * \param menu	  the menu containing the action
 * \param action  the action to tick/untick
 * \param ticked  whether to set the item as ticked
 */
void ro_gui_menu_set_entry_ticked(wimp_menu *menu, menu_action action,
		bool ticked)
{
	struct menu_definition_entry *entry =
			ro_gui_menu_find_entry(menu, action);
	if (entry) {
		if (ticked)
			entry->menu_entry->menu_flags |= wimp_MENU_TICKED;
		else
			entry->menu_entry->menu_flags &= ~wimp_MENU_TICKED;
	}
}


/**
 * Calculates a simple checksum for the current menu state
 */
int ro_gui_menu_get_checksum(void)
{
	wimp_selection menu_tree;
	int i = 0, j, checksum = 0;
	os_error *error;
	wimp_menu *menu;

	if (!current_menu_open)
		return 0;

	error = xwimp_get_menu_state((wimp_menu_state_flags)0,
			&menu_tree, 0, 0);
	if (error) {
		LOG(("xwimp_get_menu_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MenuError", error->errmess);
		return 0;
	}

	menu = current_menu;
	do {
		j = 0;
		do {
			if (menu->entries[j].icon_flags & wimp_ICON_SHADED)
				checksum ^= (1 << (i + j * 2));
			if (menu->entries[j].menu_flags & wimp_MENU_TICKED)
				checksum ^= (2 << (i + j * 2));
		} while (!(menu->entries[j++].menu_flags & wimp_MENU_LAST));

		j = menu_tree.items[i++];
		if (j != -1) {
			menu = menu->entries[j].sub_menu;
			if ((!menu) || (menu == wimp_NO_SUB_MENU) || (!IS_MENU(menu)))
				break;
		}
	} while (j != -1);

	return checksum;
}

/**
 * Translate a menu's textual content into the system local encoding
 *
 * \param menu  The menu to translate
 * \return false if out of memory, true otherwise
 */
bool ro_gui_menu_translate(struct menu_definition *menu)
{
	os_error *error;
	int alphabet;
	struct menu_definition_entry *entry;
	char *translated;
	nserror err;

	/* read current alphabet */
	error = xosbyte1(osbyte_ALPHABET_NUMBER, 127, 0, &alphabet);
	if (error) {
		LOG(("failed reading alphabet: 0x%x: %s",
				error->errnum, error->errmess));
		/* assume Latin1 */
		alphabet = territory_ALPHABET_LATIN1;
	}

	if (menu->current_encoding == alphabet)
		/* menu text is already in the correct encoding */
		return true;

	/* translate root menu title text */
	free(menu->menu->title_data.indirected_text.text);
	err = utf8_to_local_encoding(messages_get(menu->title_key),
			0, &translated);
	if (err != NSERROR_OK) {
		assert(err != NSERROR_BAD_ENCODING);
		LOG(("utf8_to_enc failed"));
		return false;
	}

	/* and fill in WIMP menu field */
	menu->menu->title_data.indirected_text.text = translated;

	/* now the menu entries */
	for (entry = menu->entries; entry; entry = entry->next) {
		wimp_menu *submenu = entry->menu_entry->sub_menu;

		/* tranlate menu entry text */
		free(entry->menu_entry->data.indirected_text.text);
		err = utf8_to_local_encoding(messages_get(entry->entry_key),
				0, &translated);
		if (err != NSERROR_OK) {
			assert(err != NSERROR_BAD_ENCODING);
			LOG(("utf8_to_enc failed"));
			return false;
		}

		/* fill in WIMP menu fields */
		entry->menu_entry->data.indirected_text.text = translated;
		entry->menu_entry->data.indirected_text.validation =
				(char *) -1;
		entry->menu_entry->data.indirected_text.size =
				strlen(translated);

		/* child menu title - this is the same as the text of
		 * the parent menu entry, so just copy the pointer */
		if (submenu != wimp_NO_SUB_MENU && IS_MENU(submenu)) {
			submenu->title_data.indirected_text.text =
					translated;
		}
	}

	/* finally, set the current encoding of the menu */
	menu->current_encoding = alphabet;

	return true;
}

