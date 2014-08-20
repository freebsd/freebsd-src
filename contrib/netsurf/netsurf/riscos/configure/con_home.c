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

#include <stdbool.h>
#include "utils/nsoption.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/menus.h"
#include "riscos/url_suggest.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/configure.h"
#include "riscos/configure/configure.h"
#include "utils/messages.h"
#include "utils/utils.h"

#define HOME_URL_FIELD 3
#define HOME_URL_GRIGHT 4
#define HOME_OPEN_STARTUP 5
#define HOME_DEFAULT_BUTTON 6
#define HOME_CANCEL_BUTTON 7
#define HOME_OK_BUTTON 8

static void ro_gui_options_home_default(wimp_pointer *pointer);
static bool ro_gui_options_home_ok(wimp_w w);
static bool ro_gui_options_home_menu_prepare(wimp_w w, wimp_i i,
		wimp_menu *menu, wimp_pointer *pointer);

bool ro_gui_options_home_initialise(wimp_w w)
{
	/* set the current values */
	ro_gui_set_icon_string(w, HOME_URL_FIELD,
                               nsoption_charp(homepage_url) ? 
                               nsoption_charp(homepage_url) : "", true);

	ro_gui_set_icon_selected_state(w, HOME_OPEN_STARTUP,
                                       nsoption_bool(open_browser_at_startup));

	ro_gui_set_icon_shaded_state(w,
			HOME_URL_GRIGHT, !ro_gui_url_suggest_prepare_menu());

	/* initialise all functions for a newly created window */
	ro_gui_wimp_event_register_menu_gright(w, HOME_URL_FIELD,
			HOME_URL_GRIGHT, ro_gui_url_suggest_menu);
	ro_gui_wimp_event_register_checkbox(w, HOME_OPEN_STARTUP);
	ro_gui_wimp_event_register_button(w, HOME_DEFAULT_BUTTON,
			ro_gui_options_home_default);
	ro_gui_wimp_event_register_cancel(w, HOME_CANCEL_BUTTON);
	ro_gui_wimp_event_register_ok(w, HOME_OK_BUTTON,
			ro_gui_options_home_ok);
	ro_gui_wimp_event_register_menu_prepare(w,
			ro_gui_options_home_menu_prepare);
	ro_gui_wimp_event_set_help_prefix(w, "HelpHomeConfig");
	ro_gui_wimp_event_memorise(w);
	return true;

}

void ro_gui_options_home_default(wimp_pointer *pointer)
{
	/* set the default values */
	ro_gui_set_icon_string(pointer->w, HOME_URL_FIELD, "", true);
	ro_gui_set_icon_selected_state(pointer->w, HOME_OPEN_STARTUP, false);
}

bool ro_gui_options_home_ok(wimp_w w)
{
	nsoption_set_charp(homepage_url,
		       strdup(ro_gui_get_icon_string(w, HOME_URL_FIELD)));

	nsoption_set_bool(open_browser_at_startup,
			  ro_gui_get_icon_selected_state(w, HOME_OPEN_STARTUP));

	ro_gui_save_options();
  	return true;
}


/**
 * Callback to prepare menus in the Configure Home dialog.  At present, this
 * only has to handle the URL Suggestion pop-up.
 *
 * \param w		The window handle owning the menu.
 * \param i		The icon handle owning the menu.
 * \param *menu		The menu to be prepared.
 * \param *pointer	The associated mouse click event block, or NULL
 *			on an Adjust-click re-opening.
 * \return		true if the event was handled; false if not.
 */

bool ro_gui_options_home_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	if (menu != ro_gui_url_suggest_menu || i != HOME_URL_GRIGHT)
		return false;

	if (pointer != NULL)
		ro_gui_url_suggest_prepare_menu();

	return true;
}
