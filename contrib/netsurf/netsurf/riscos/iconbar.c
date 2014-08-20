/*
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Iconbar icon and menus (implementation).
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <features.h>

#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/wimp.h"
#include "riscos/configure.h"
#include "riscos/cookies.h"
#include "riscos/dialog.h"
#include "riscos/global_history.h"
#include "riscos/hotlist.h"
#include "riscos/iconbar.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "utils/nsoption.h"
#include "riscos/wimp_event.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

static bool ro_gui_iconbar_click(wimp_pointer *pointer);

static bool ro_gui_iconbar_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static void ro_gui_iconbar_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);


static wimp_menu *ro_gui_iconbar_menu = NULL;	/**< Iconbar menu handle */

/**
 * Initialise the iconbar menus, create an icon and register the necessary
 * handlers to look after them all.
 */

void ro_gui_iconbar_initialise(void)
{
	os_error *error;

	/* Build the iconbar menu */

	static const struct ns_menu iconbar_definition = {
		"NetSurf", {
			{ "Info", NO_ACTION, &dialog_info },
			{ "AppHelp", HELP_OPEN_CONTENTS, 0 },
			{ "Open", BROWSER_NAVIGATE_URL, 0 },
			{ "Open.OpenURL", BROWSER_NAVIGATE_URL, &dialog_openurl },
			{ "Open.HotlistShow", HOTLIST_SHOW, 0 },
			{ "Open.HistGlobal", HISTORY_SHOW_GLOBAL, 0 },
			{ "Open.ShowCookies", COOKIES_SHOW, 0 },
			{ "Choices", CHOICES_SHOW, 0 },
			{ "Quit", APPLICATION_QUIT, 0 },
			{NULL, 0, 0}
		}
	};
	ro_gui_iconbar_menu = ro_gui_menu_define_menu(&iconbar_definition);

	/* Create an iconbar icon. */

	wimp_icon_create icon = {
		wimp_ICON_BAR_RIGHT,
		{ { 0, 0, 68, 68 },
		wimp_ICON_SPRITE | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
				(wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT),
		{ "!netsurf" } } };
	error = xwimp_create_icon(&icon, 0);
	if (error) {
		LOG(("xwimp_create_icon: 0x%x: %s",
				error->errnum, error->errmess));
		die(error->errmess);
	}

	/* Register handlers to look after clicks and menu actions. */

	ro_gui_wimp_event_register_mouse_click(wimp_ICON_BAR,
			ro_gui_iconbar_click);

	ro_gui_wimp_event_register_menu(wimp_ICON_BAR, ro_gui_iconbar_menu,
			true, true);
	ro_gui_wimp_event_register_menu_selection(wimp_ICON_BAR,
			ro_gui_iconbar_menu_select);
	ro_gui_wimp_event_register_menu_warning(wimp_ICON_BAR,
			ro_gui_iconbar_menu_warning);
}


/**
 * Handle Mouse_Click events on the iconbar icon.
 *
 * \param *pointer		The wimp event block to be processed.
 * \return			true if the event was handled; else false.
 */

bool ro_gui_iconbar_click(wimp_pointer *pointer)
{
	int key_down = 0;
	nsurl *url;
	nserror error;

	switch (pointer->buttons) {
	case wimp_CLICK_SELECT:
		if (nsoption_charp(homepage_url) != NULL) {
			error = nsurl_create(nsoption_charp(homepage_url), &url);
		} else {
			error = nsurl_create(NETSURF_HOMEPAGE, &url);
		}

		/* create an initial browser window */
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
					url,
					NULL,
					NULL,
					NULL);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
		break;

	case wimp_CLICK_ADJUST:
		xosbyte1(osbyte_SCAN_KEYBOARD, 0 ^ 0x80, 0, &key_down);
		if (key_down == 0)
			ro_gui_hotlist_open();
		break;
	}

	return true;
}

/**
 * Handle submenu warnings for the iconbar menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_iconbar_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	if (w != wimp_ICON_BAR || i != wimp_ICON_WINDOW)
		return;

	switch (action) {
	case BROWSER_NAVIGATE_URL:
		ro_gui_dialog_prepare_open_url();
		break;
	default:
		break;
	}
}

/**
 * Handle selections from the iconbar menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_iconbar_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	nsurl *url;
	nserror error;

	if (w != wimp_ICON_BAR || i != wimp_ICON_WINDOW)
		return false;

	switch (action) {
	case HELP_OPEN_CONTENTS:
		error = nsurl_create("http://www.netsurf-browser.org/documentation/", &url);
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
					url,
					NULL,
					NULL,
					NULL);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
		return true;
	
	case BROWSER_NAVIGATE_URL:
		ro_gui_dialog_prepare_open_url();
		ro_gui_dialog_open_persistent(NULL, dialog_openurl, true);
		return true;
	case HOTLIST_SHOW:
		ro_gui_hotlist_open();
		return true;
	case HISTORY_SHOW_GLOBAL:
		ro_gui_global_history_open();
		return true;
	case COOKIES_SHOW:
		ro_gui_cookies_open();
		return true;
	case CHOICES_SHOW:
		ro_gui_configure_show();
		return true;
	case APPLICATION_QUIT:
		if (ro_gui_prequit()) {
			LOG(("QUIT in response to user request"));
			netsurf_quit = true;
		}
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Check if a particular menu handle is the iconbar menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the iconbar menu
 */

bool ro_gui_iconbar_check_menu(wimp_menu *menu)
{
	return (ro_gui_iconbar_menu == menu) ? true : false;
}

