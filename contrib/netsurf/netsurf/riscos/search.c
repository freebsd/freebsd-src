/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
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
 * Free text search (implementation)
 */

#include "utils/config.h"

#include <ctype.h>
#include <string.h>
#include "oslib/hourglass.h"
#include "oslib/wimp.h"
#include "utils/config.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "desktop/browser.h"
#include "desktop/gui.h"
#include "desktop/browser_private.h"
#include "desktop/search.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#define RECENT_SEARCHES 8

struct search_static_data {
	char *recent_searches[RECENT_SEARCHES];
	bool search_insert;
	struct browser_window *search_window;
	search_flags_t flags;
	char *string;
};

static struct search_static_data search_data =
		{{NULL}, false, NULL, 0, NULL};

static wimp_MENU(RECENT_SEARCHES) menu_recent;
wimp_menu *recent_search_menu = (wimp_menu *)&menu_recent;
#define DEFAULT_FLAGS (wimp_ICON_TEXT | wimp_ICON_FILLED | \
		(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) | \
		(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT))


static void ro_gui_search_end(wimp_w w);
static bool ro_gui_search_next(wimp_w w);
static bool ro_gui_search_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer);
static bool ro_gui_search_prepare_menu(void);
static bool ro_gui_search_click(wimp_pointer *pointer);
static bool ro_gui_search_keypress(wimp_key *key);
static search_flags_t ro_gui_search_update_flags(void);
static void ro_gui_search_set_forward_state(bool active, void *p);
static void ro_gui_search_set_back_state(bool active, void *p);
static void ro_gui_search_set_status(bool found, void *p);
static void ro_gui_search_set_hourglass(bool active, void *p);
static void ro_gui_search_add_recent(const char *string, void *p);

static struct gui_search_table search_table = {
	ro_gui_search_set_status,
	ro_gui_search_set_hourglass,
	ro_gui_search_add_recent,
	ro_gui_search_set_forward_state,
	ro_gui_search_set_back_state,
};

struct gui_search_table *riscos_search_table = &search_table;

void ro_gui_search_init(void)
{
	dialog_search = ro_gui_dialog_create("search");
	ro_gui_wimp_event_register_keypress(dialog_search,
			ro_gui_search_keypress);
	ro_gui_wimp_event_register_close_window(dialog_search,
			ro_gui_search_end);
	ro_gui_wimp_event_register_menu_prepare(dialog_search,
			ro_gui_search_menu_prepare);
	ro_gui_wimp_event_register_menu_gright(dialog_search,
			ICON_SEARCH_TEXT, ICON_SEARCH_MENU,
			recent_search_menu);
	ro_gui_wimp_event_register_text_field(dialog_search,
			ICON_SEARCH_STATUS);
	ro_gui_wimp_event_register_checkbox(dialog_search,
			ICON_SEARCH_CASE_SENSITIVE);
	ro_gui_wimp_event_register_mouse_click(dialog_search,
			ro_gui_search_click);
	ro_gui_wimp_event_register_ok(dialog_search, ICON_SEARCH_FIND_NEXT,
			ro_gui_search_next);
	ro_gui_wimp_event_register_cancel(dialog_search, ICON_SEARCH_CANCEL);
	ro_gui_wimp_event_set_help_prefix(dialog_search, "HelpSearch");

	recent_search_menu->title_data.indirected_text.text =
			(char*)messages_get("Search");
	ro_gui_menu_init_structure(recent_search_menu, RECENT_SEARCHES);
}

/**
 * Wrapper for the pressing of an OK button for wimp_event.
 *
 * \return false, to indicate the window should not be closed
 */
bool ro_gui_search_next(wimp_w w)
{
	search_data.search_insert = true;
	search_flags_t flags = SEARCH_FLAG_FORWARDS |
			ro_gui_search_update_flags();
	browser_window_search(search_data.search_window, NULL, flags,
			ro_gui_get_icon_string(dialog_search,
					ICON_SEARCH_TEXT));
	return false;
}


/**
 * Callback to prepare menus in the Search dialog.  At present, this
 * only has to handle the previous search pop-up.
 *
 * \param w		The window handle owning the menu.
 * \param i		The icon handle owning the menu.
 * \param *menu		The menu to be prepared.
 * \param *pointer	The associated mouse click event block, or NULL
 *			on an Adjust-click re-opening.
 * \return		true if the event was handled; false if not.
 */

bool ro_gui_search_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	if (menu != recent_search_menu || i != ICON_SEARCH_MENU)
		return false;

	if (pointer != NULL)
		return ro_gui_search_prepare_menu();

	return true;
}


bool ro_gui_search_click(wimp_pointer *pointer)
{
	search_flags_t flags;
	switch (pointer->i) {
		case ICON_SEARCH_FIND_PREV:
			search_data.search_insert = true;
			flags = ~SEARCH_FLAG_FORWARDS &
					ro_gui_search_update_flags();
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;
		case ICON_SEARCH_CASE_SENSITIVE:
			flags = SEARCH_FLAG_FORWARDS |
					ro_gui_search_update_flags();
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;
		case ICON_SEARCH_SHOW_ALL:
			flags = ro_gui_get_icon_selected_state(
					pointer->w, pointer->i) ?
					SEARCH_FLAG_SHOWALL : SEARCH_FLAG_NONE;
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;
	}
	return false;
}

/**
 * add search string to recent searches list
 * front is at liberty how to implement the bare notification
 * should normally store a strdup() of the string in
 * search_global_data.recent[];
 * core gives no guarantee of the integrity of the const char *
 * \param string search pattern
 * \param p the pointer sent to search_verify_new()
 */

void ro_gui_search_add_recent(const char *search, void *p)
{
	char *tmp;
	int i;

	if ((search == NULL) || (search[0] == '\0'))
		return;

	if (!search_data.search_insert) {
		free(search_data.recent_searches[0]);
		search_data.recent_searches[0] = strdup(search);
		ro_gui_search_prepare_menu();
		return;
	}

	if ((search_data.recent_searches[0] != NULL) &&
			(!strcmp(search_data.recent_searches[0], search)))
		return;

	tmp = strdup(search);
	if (!tmp) {
		warn_user("NoMemory", 0);
		return;
	}
	free(search_data.recent_searches[RECENT_SEARCHES - 1]);
	for (i = RECENT_SEARCHES - 1; i > 0; i--)
		search_data.recent_searches[i] = search_data.recent_searches[i - 1];
	search_data.recent_searches[0] = tmp;
	search_data.search_insert = false;

	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_MENU, false);
	ro_gui_search_prepare_menu();
}

bool ro_gui_search_prepare_menu(void)
{
	int i;
	int suggestions = 0;

	for (i = 0; i < RECENT_SEARCHES; i++)
		if (search_data.recent_searches[i] != NULL)
			suggestions++;

	if (suggestions == 0)
		return false;

	for (i = 0; i < suggestions; i++) {
		recent_search_menu->entries[i].menu_flags &= ~wimp_MENU_LAST;
		recent_search_menu->entries[i].data.indirected_text.text =
				search_data.recent_searches[i];
		recent_search_menu->entries[i].data.indirected_text.size =
				strlen(search_data.recent_searches[i]) + 1;
	}
	recent_search_menu->entries[suggestions - 1].menu_flags |=
			wimp_MENU_LAST;

	return true;
}

/**
 * Open the search dialog
 *
 * \param bw the browser window to search
 */
void ro_gui_search_prepare(struct browser_window *bw)
{
	hlcache_handle *h;

	assert(bw != NULL);

	h = bw->current_content;

	/* only handle html/textplain contents */
	if ((!h) || (content_get_type(h) != CONTENT_HTML &&
			content_get_type(h) != CONTENT_TEXTPLAIN))
		return;

	/* if the search dialogue is reopened over a new window, we may
	   need to cancel the previous search */
	ro_gui_search_set_forward_state(true, bw);
	ro_gui_search_set_back_state(true, bw);

	search_data.search_window = bw;

	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_TEXT, "", true);
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_CASE_SENSITIVE, false);
	ro_gui_set_icon_selected_state(dialog_search,
				ICON_SEARCH_SHOW_ALL, false);

	ro_gui_search_set_status(true, NULL);

	ro_gui_wimp_event_memorise(dialog_search);
	search_data.search_insert = true;
}

/**
 * Handle keypresses in the search dialog
 *
 * \param key wimp_key block
 * \return true if keypress handled, false otherwise
 */
bool ro_gui_search_keypress(wimp_key *key)
{
	bool state;
	search_flags_t flags;

	switch (key->c) {
		case 1: {
			flags = ro_gui_search_update_flags()
					^ SEARCH_FLAG_SHOWALL;
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
		}
		break;
		case 9: /* ctrl i */
			state = ro_gui_get_icon_selected_state(dialog_search,
					ICON_SEARCH_CASE_SENSITIVE);
			ro_gui_set_icon_selected_state(dialog_search,
					ICON_SEARCH_CASE_SENSITIVE, !state);
			flags = SEARCH_FLAG_FORWARDS |
					ro_gui_search_update_flags();
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;
		case IS_WIMP_KEY | wimp_KEY_UP:
			search_data.search_insert = true;
			flags = ~SEARCH_FLAG_FORWARDS &
					ro_gui_search_update_flags();
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
			search_data.search_insert = true;
			flags = SEARCH_FLAG_FORWARDS |
					ro_gui_search_update_flags();
			browser_window_search(search_data.search_window, NULL,
					flags,
					ro_gui_get_icon_string(dialog_search,
							ICON_SEARCH_TEXT));
			return true;

		default:
			if (key->c == 21) {
				/* ctrl+u means the user's starting
				 * a new search */
				browser_window_search_clear(
						search_data.search_window);
				search_data.search_insert = true;
			}
			if (key->c == 8  || /* backspace */
			    key->c == 21 || /* ctrl u */
			    (key->c >= 0x20 && key->c <= 0x7f)) {
				flags = SEARCH_FLAG_FORWARDS |
						ro_gui_search_update_flags();
				ro_gui_search_set_forward_state(true,
						search_data.search_window);
				ro_gui_search_set_back_state(true,
						search_data.search_window);
				browser_window_search(search_data.search_window,
						NULL,
						flags,
						ro_gui_get_icon_string(
							dialog_search,
							ICON_SEARCH_TEXT));
				return true;
			}
			break;
	}

	return false;
}

/**
 * Ends the search
 * \param w the search window handle (not used)
 */
void ro_gui_search_end(wimp_w w)
{
	browser_window_search_clear(search_data.search_window);
}

/**
* Change the displayed search status.
* \param found  search pattern matched in text
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ro_gui_search_set_status(bool found, void *p)
{
	ro_gui_set_icon_string(dialog_search, ICON_SEARCH_STATUS, found ? "" :
			messages_get("NotFound"), true);
}

/**
* display hourglass while searching
* \param active start/stop indicator
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ro_gui_search_set_hourglass(bool active, void *p)
{
	if (active)
		xhourglass_on();

	else
		xhourglass_off();
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ro_gui_search_set_forward_state(bool active, void *p)
{
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_NEXT,
			!active);
}

/**
* activate search forwards button in gui
* \param active activate/inactivate
* \param p the pointer sent to search_verify_new() / search_create_context()
*/

void ro_gui_search_set_back_state(bool active, void *p)
{
	ro_gui_set_icon_shaded_state(dialog_search, ICON_SEARCH_FIND_PREV,
			!active);
}

/**
* retrieve state of 'case sensitive', 'show all' checks in gui
*/
search_flags_t ro_gui_search_update_flags(void)
{
	search_flags_t flags;
	flags = 0 | (ro_gui_get_icon_selected_state(dialog_search,
			ICON_SEARCH_CASE_SENSITIVE) ?
			SEARCH_FLAG_CASE_SENSITIVE : 0) |
			(ro_gui_get_icon_selected_state(dialog_search,
			ICON_SEARCH_SHOW_ALL) ? SEARCH_FLAG_SHOWALL : 0);
	return flags;
}

