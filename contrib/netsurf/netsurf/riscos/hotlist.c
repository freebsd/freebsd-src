/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 * Copyright 2010, 2013 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Hotlist (implementation).
 */

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "oslib/osfile.h"
#include "oslib/osmodule.h"
#include "oslib/wimp.h"

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "desktop/hotlist.h"
#include "desktop/tree.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "utils/nsoption.h"

#include "riscos/gui.h"
#include "riscos/dialog.h"
#include "riscos/hotlist.h"
#include "riscos/menus.h"
#include "riscos/message.h"
#include "riscos/save.h"
#include "riscos/toolbar.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/query.h"

static void ro_gui_hotlist_toolbar_update_buttons(void);
static void ro_gui_hotlist_toolbar_save_buttons(char *config);
static bool ro_gui_hotlist_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer);
static void ro_gui_hotlist_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static bool ro_gui_hotlist_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action);
static void ro_gui_hotlist_toolbar_click(button_bar_action action);
static void ro_gui_hotlist_addurl_bounce(wimp_message *message);
static void ro_gui_hotlist_scheduled_callback(void *p);
static void ro_gui_hotlist_remove_confirmed(query_id id,
		enum query_response res, void *p);
static void ro_gui_hotlist_remove_cancelled(query_id id,
		enum query_response res, void *p);

static const query_callback remove_funcs = {
	ro_gui_hotlist_remove_confirmed,
	ro_gui_hotlist_remove_cancelled
};

struct ro_treeview_callbacks ro_hotlist_treeview_callbacks = {
	ro_gui_hotlist_toolbar_click,
	ro_gui_hotlist_toolbar_update_buttons,
	ro_gui_hotlist_toolbar_save_buttons
};

/* Hotlist Protocol Message Blocks, which are currently not in OSLib. */

struct ro_hotlist_message_hotlist_addurl {
	wimp_MESSAGE_HEADER_MEMBERS	/**< The standard message header. */
	char		*url;		/**< Pointer to the URL in RMA.   */
	char		*title;		/**< Pointer to the title in RMA. */
	char		appname[32];	/**< The application name.        */
};

struct ro_hotlist_message_hotlist_changed {
	wimp_MESSAGE_HEADER_MEMBERS	/**< The standard message header. */
};

static char	*hotlist_url = NULL;    /**< URL area claimed from RMA.   */
static char	*hotlist_title = NULL;	/**< Title area claimed from RMA. */

/** Hotlist Query Handler. */

static query_id	hotlist_query = QUERY_INVALID;
static nsurl	*hotlist_delete_url = NULL;

/* The RISC OS hotlist window, toolbar and treeview data. */

static struct ro_hotlist {
	wimp_w		window;		/**< The hotlist RO window handle. */
	struct toolbar	*toolbar;	/**< The hotlist toolbar handle.   */
	ro_treeview	*tv;		/**< The hotlist treeview handle.  */
	wimp_menu	*menu;		/**< The hotlist window menu.      */
} hotlist_window;

/**
 * Pre-Initialise the hotlist tree.  This is called for things that need to
 * be done at the gui_init() stage, such as loading templates.
 */

void ro_gui_hotlist_preinitialise(void)
{
	/* Create our window. */

	hotlist_window.window = ro_gui_dialog_create("tree");
	ro_gui_set_window_title(hotlist_window.window,
			messages_get("Hotlist"));
}

/**
 * Initialise the hotlist tree, at the gui_init2() stage.
 */

void ro_gui_hotlist_postinitialise(void)
{
	/* Create our toolbar. */

	hotlist_window.toolbar = ro_toolbar_create(NULL, hotlist_window.window,
			THEME_STYLE_HOTLIST_TOOLBAR, TOOLBAR_FLAGS_NONE,
			ro_treeview_get_toolbar_callbacks(), NULL,
			"HelpHotToolbar");
	if (hotlist_window.toolbar != NULL) {
		ro_toolbar_add_buttons(hotlist_window.toolbar,
				hotlist_toolbar_buttons,
				       nsoption_charp(toolbar_hotlist));
		ro_toolbar_rebuild(hotlist_window.toolbar);
	}

	/* Create the treeview with the window and toolbar. */
	tree_hotlist_path = nsoption_charp(hotlist_path);
	hotlist_window.tv = ro_treeview_create(hotlist_window.window,
			hotlist_window.toolbar, &ro_hotlist_treeview_callbacks,
			TREE_HOTLIST);
	if (hotlist_window.tv == NULL) {
		LOG(("Failed to allocate treeview"));
		return;
	}

	ro_toolbar_update_client_data(hotlist_window.toolbar,
			hotlist_window.tv);

	/* Build the hotlist window menu. */

	static const struct ns_menu hotlist_definition = {
		"Hotlist", {
			{ "Hotlist", NO_ACTION, 0 },
			{ "Hotlist.New", NO_ACTION, 0 },
			{ "Hotlist.New.Folder", TREE_NEW_FOLDER, 0 },
			{ "Hotlist.New.Link", TREE_NEW_LINK, 0 },
			{ "_Hotlist.Export", HOTLIST_EXPORT, &dialog_saveas },
			{ "Hotlist.Expand", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.All", TREE_EXPAND_ALL, 0 },
			{ "Hotlist.Expand.Folders", TREE_EXPAND_FOLDERS, 0 },
			{ "Hotlist.Expand.Links", TREE_EXPAND_LINKS, 0 },
			{ "Hotlist.Collapse", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.All", TREE_COLLAPSE_ALL, 0 },
			{ "Hotlist.Collapse.Folders", TREE_COLLAPSE_FOLDERS, 0 },
			{ "Hotlist.Collapse.Links", TREE_COLLAPSE_LINKS, 0 },
			{ "Hotlist.Toolbars", NO_ACTION, 0 },
			{ "_Hotlist.Toolbars.ToolButtons", TOOLBAR_BUTTONS, 0 },
			{ "Hotlist.Toolbars.EditToolbar", TOOLBAR_EDIT, 0 },
			{ "Selection", TREE_SELECTION, 0 },
			{ "Selection.Edit", TREE_SELECTION_EDIT, 0 },
			{ "Selection.Launch", TREE_SELECTION_LAUNCH, 0 },
			{ "Selection.Delete", TREE_SELECTION_DELETE, 0 },
			{ "SelectAll", TREE_SELECT_ALL, 0 },
			{ "Clear", TREE_CLEAR_SELECTION, 0 },
			{NULL, 0, 0}
		}
	};

	hotlist_window.menu = ro_gui_menu_define_menu(&hotlist_definition);

	ro_gui_wimp_event_register_menu(hotlist_window.window,
			hotlist_window.menu, false, false);
	ro_gui_wimp_event_register_menu_prepare(hotlist_window.window,
			ro_gui_hotlist_menu_prepare);
	ro_gui_wimp_event_register_menu_selection(hotlist_window.window,
			ro_gui_hotlist_menu_select);
	ro_gui_wimp_event_register_menu_warning(hotlist_window.window,
			ro_gui_hotlist_menu_warning);
}

/**
 * Destroy the hotlist window.
 */

void ro_gui_hotlist_destroy(void)
{
	if (hotlist_window.tv == NULL)
		return;

	tree_hotlist_path = nsoption_charp(hotlist_save);
	ro_treeview_destroy(hotlist_window.tv);
}


/**
 * Open the hotlist window.
 *
 */

void ro_gui_hotlist_open(void)
{
	os_error	*error;
	char		command[2048];

	if (nsoption_bool(external_hotlists) &&
	    nsoption_charp(external_hotlist_app) != NULL &&
	    *nsoption_charp(external_hotlist_app) != '\0') {
		snprintf(command, sizeof(command), "Filer_Run %s",
			 nsoption_charp(external_hotlist_app));
		error = xos_cli(command);

		if (error == NULL)
			return;

		LOG(("xos_cli: 0x%x: %s", error->errnum, error->errmess));
		warn_user("Failed to launch external hotlist: %s",
				error->errmess);
	}

	ro_gui_hotlist_toolbar_update_buttons();

	if (!ro_gui_dialog_open_top(hotlist_window.window,
			hotlist_window.toolbar, 600, 800)) {
		ro_treeview_set_origin(hotlist_window.tv, 0,
				-(ro_toolbar_height(hotlist_window.toolbar)));
	}
}

/**
 * Handle toolbar button clicks.
 *
 * \param  action		The action to handle
 */

void ro_gui_hotlist_toolbar_click(button_bar_action action)
{
	switch (action) {
	case TOOLBAR_BUTTON_DELETE:
		hotlist_keypress(KEY_DELETE_LEFT);
		ro_toolbar_update_all_hotlists();
		break;

	case TOOLBAR_BUTTON_EXPAND:
		hotlist_expand(false);
		break;

	case TOOLBAR_BUTTON_COLLAPSE:
		hotlist_contract(false);
		break;

	case TOOLBAR_BUTTON_OPEN:
		hotlist_expand(true);
		break;

	case TOOLBAR_BUTTON_CLOSE:
		hotlist_contract(true);
		break;

	case TOOLBAR_BUTTON_LAUNCH:
		hotlist_keypress(KEY_CR);
		break;

	case TOOLBAR_BUTTON_CREATE:
		hotlist_add_folder(NULL, false, 0);
		break;

	default:
		break;
	}
}


/**
 * Update the button state in the hotlist toolbar.
 */

void ro_gui_hotlist_toolbar_update_buttons(void)
{
	ro_toolbar_set_button_shaded_state(hotlist_window.toolbar,
			TOOLBAR_BUTTON_DELETE,
			!hotlist_has_selection());

	ro_toolbar_set_button_shaded_state(hotlist_window.toolbar,
			TOOLBAR_BUTTON_LAUNCH,
			!hotlist_has_selection());
}


/**
 * Save a new button arrangement in the hotlist toolbar.
 *
 * \param *config		The new button configuration string.
 */

void ro_gui_hotlist_toolbar_save_buttons(char *config)
{
	nsoption_set_charp(toolbar_hotlist, config);
	ro_gui_save_options();
}


/**
 * Prepare the hotlist menu for opening
 *
 * \param  window		The window owning the menu.
 * \param  *menu		The menu about to be opened.
 * \param  *pointer		Pointer to the relevant wimp event block, or
 *				NULL for an Adjust click.
 * \return			true if the event was handled; else false.
 */

bool ro_gui_hotlist_menu_prepare(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_pointer *pointer)
{
	bool selection;

	if (menu != hotlist_window.menu)
		return false;

	selection = hotlist_has_selection();

	ro_gui_menu_set_entry_shaded(hotlist_window.menu,
			TREE_SELECTION, !selection);
	ro_gui_menu_set_entry_shaded(hotlist_window.menu,
			TREE_CLEAR_SELECTION, !selection);

	ro_gui_save_prepare(GUI_SAVE_HOTLIST_EXPORT_HTML,
			NULL, NULL, NULL, NULL);

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_option_shade(hotlist_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_BUTTONS,
			ro_toolbar_menu_buttons_tick(hotlist_window.toolbar));

	ro_gui_menu_set_entry_shaded(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_shade(hotlist_window.toolbar));
	ro_gui_menu_set_entry_ticked(menu, TOOLBAR_EDIT,
			ro_toolbar_menu_edit_tick(hotlist_window.toolbar));

	return true;
}


/**
 * Handle submenu warnings for the hotlist menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu to which the warning applies.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 */

void ro_gui_hotlist_menu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	/* Do nothing */
}

/**
 * Handle selections from the hotlist menu
 *
 * \param  w			The window owning the menu.
 * \param  i			The icon owning the menu.
 * \param  *menu		The menu from which the selection was made.
 * \param  *selection		The wimp menu selection data.
 * \param  action		The selected menu action.
 * \return			true if action accepted; else false.
 */

bool ro_gui_hotlist_menu_select(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	switch (action) {
	case HOTLIST_EXPORT:
		ro_gui_dialog_open_persistent(w, dialog_saveas, true);
		return true;
	case TREE_NEW_FOLDER:
		hotlist_add_folder(NULL, false, 0);
		return true;
	case TREE_NEW_LINK:
		hotlist_add_entry(NULL, NULL, false, 0);
		return true;
	case TREE_EXPAND_ALL:
		hotlist_expand(false);
		return true;
	case TREE_EXPAND_FOLDERS:
		hotlist_expand(true);
		return true;
	case TREE_EXPAND_LINKS:
		hotlist_expand(false);
		return true;
	case TREE_COLLAPSE_ALL:
		hotlist_contract(true);
		return true;
	case TREE_COLLAPSE_FOLDERS:
		hotlist_contract(true);
		return true;
	case TREE_COLLAPSE_LINKS:
		hotlist_contract(false);
		return true;
	case TREE_SELECTION_EDIT:
		hotlist_edit_selection();
		return true;
	case TREE_SELECTION_LAUNCH:
		hotlist_keypress(KEY_CR);
		return true;
	case TREE_SELECTION_DELETE:
		hotlist_keypress(KEY_DELETE_LEFT);
		ro_toolbar_update_all_hotlists();
		return true;
	case TREE_SELECT_ALL:
		hotlist_keypress(KEY_SELECT_ALL);
		return true;
	case TREE_CLEAR_SELECTION:
		hotlist_keypress(KEY_CLEAR_SELECTION);
		return true;
	case TOOLBAR_BUTTONS:
		ro_toolbar_set_display_buttons(hotlist_window.toolbar,
				!ro_toolbar_get_display_buttons(
					hotlist_window.toolbar));
		return true;
	case TOOLBAR_EDIT:
		ro_toolbar_toggle_edit(hotlist_window.toolbar);
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * Check if a particular window handle is the hotlist window
 *
 * \param window	The window in question
 * \return		true if this window is the hotlist
 */
bool ro_gui_hotlist_check_window(wimp_w window)
{
	if (hotlist_window.window == window)
		return true;
	else
		return false;
}

/**
 * Check if a particular menu handle is the hotlist menu
 *
 * \param  *menu		The menu in question.
 * \return			true if this menu is the hotlist menu
 */

bool ro_gui_hotlist_check_menu(wimp_menu *menu)
{
	if (hotlist_window.menu == menu)
		return true;
	else
		return false;
}


/**
 * Add a URL to the hotlist.  This will be passed on to the core hotlist, then
 * Message_HotlistAddURL will broadcast to any bookmark applications via the
 * Hotlist Protocol.
 *
 * \param *url	The URL to be added.
 */

void ro_gui_hotlist_add_page(nsurl *url)
{
	const struct url_data				*data;
	wimp_message					message;
	struct ro_hotlist_message_hotlist_addurl	*add_url =
			(struct ro_hotlist_message_hotlist_addurl *) &message;

	if (url == NULL)
		return;

	/* If we're not using external hotlists, add the page to NetSurf's
	 * own hotlist and return...
	 */

	if (!nsoption_bool(external_hotlists)) {
		hotlist_add_url(url);
		return;
	}

	/* ...otherwise try broadcasting the details to any other
	 * interested parties.  If no-one answers, we'll fall back to
	 * NetSurf's hotlist anyway when the message bounces.
	 */

	ro_gui_hotlist_add_cleanup();

	data = urldb_get_url_data(url);
	if (data == NULL)
		return;

	hotlist_url = osmodule_alloc(nsurl_length(url) + 1);
	hotlist_title = osmodule_alloc(strlen(data->title) + 1);

	if (hotlist_url == NULL || hotlist_title == NULL) {
		ro_gui_hotlist_add_cleanup();
		return;
	}

	strcpy(hotlist_url, nsurl_access(url));
	strcpy(hotlist_title, data->title);

	add_url->size = 60;
	add_url->your_ref = 0;
	add_url->action = message_HOTLIST_ADD_URL;
	add_url->url = hotlist_url;
	add_url->title = hotlist_title;
	strcpy(add_url->appname, "NetSurf");

	if (!ro_message_send_message(wimp_USER_MESSAGE_RECORDED, &message, 0,
			ro_gui_hotlist_addurl_bounce))
		ro_gui_hotlist_add_cleanup();

	/* Listen for the next Null poll, as an indication that the
	 * message didn't bounce.
	 */

	riscos_schedule(0, ro_gui_hotlist_scheduled_callback, NULL);
}


/**
 * Handle bounced Message_HotlistAddURL, so that RMA storage can be freed.
 *
 * \param *message		The bounced message content.
 */

static void ro_gui_hotlist_addurl_bounce(wimp_message *message)
{
	if (hotlist_url != NULL) {
		nsurl *nsurl;

		if (nsurl_create(hotlist_url, &nsurl) != NSERROR_OK)
			return;

		hotlist_add_url(nsurl);
		nsurl_unref(nsurl);
	}

	ro_gui_hotlist_add_cleanup();

	/* There's no longer any need to listen for the next Null poll. */

	riscos_schedule(-1, ro_gui_hotlist_scheduled_callback, NULL);
}


/**
 * Callback to schedule for the next available Null poll, by which point
 * a hotlist client will have claimed the Message_HotlistAddURL and any
 * details in RMA can safely be discarded.
 *
 * \param *p			Unused data pointer.
 */

static void ro_gui_hotlist_scheduled_callback(void *p)
{
	ro_gui_hotlist_add_cleanup();
}


/**
 * Clean up RMA storage used by the Message_HotlistAddURL protocol.
 */

void ro_gui_hotlist_add_cleanup(void)
{
	if (hotlist_url != NULL) {
		osmodule_free(hotlist_url);
		hotlist_url = NULL;
	}

	if (hotlist_title != NULL) {
		osmodule_free(hotlist_title);
		hotlist_title = NULL;
	}
}


/**
 * Remove a URL from the hotlist.  This will be passed on to the core hotlist,
 * unless we're configured to use external hotlists in which case we ignore it.
 *
 * \param *url	The URL to be removed.
 */

void ro_gui_hotlist_remove_page(nsurl *url)
{
	if (url == NULL || nsoption_bool(external_hotlists) ||
			!hotlist_has_url(url))
		return;

	/* Clean up any existing delete attempts before continuing. */

	if (hotlist_query != QUERY_INVALID) {
		query_close(hotlist_query);
		hotlist_query = QUERY_INVALID;
	}

	if (hotlist_delete_url != NULL) {
		nsurl_unref(hotlist_delete_url);
		hotlist_delete_url = NULL;
	}

	/* Check with the user before removing the URL, unless they don't
	 * want us to be careful in which case just do it.
	 */

	if (nsoption_bool(confirm_hotlist_remove)) {
		hotlist_query = query_user("RemoveHotlist", NULL,
				&remove_funcs, NULL,
				messages_get("Remove"),
				messages_get("DontRemove"));

		hotlist_delete_url = nsurl_ref(url);
	} else {
		hotlist_remove_url(url);
		ro_toolbar_update_all_hotlists();
	}
}


/**
 * Callback confirming a URL delete query.
 *
 * \param id		The ID of the query calling us.
 * \param res		The user's response to the query.
 * \param *p		Callback data (always NULL).
 */

static void ro_gui_hotlist_remove_confirmed(query_id id,
		enum query_response res, void *p)
{
	hotlist_remove_url(hotlist_delete_url);
	ro_toolbar_update_all_hotlists();

	nsurl_unref(hotlist_delete_url);
	hotlist_delete_url = NULL;
	hotlist_query = QUERY_INVALID;
}


/**
 * Callback cancelling a URL delete query.
 *
 * \param id		The ID of the query calling us.
 * \param res		The user's response to the query.
 * \param *p		Callback data (always NULL).
 */

static void ro_gui_hotlist_remove_cancelled(query_id id,
		enum query_response res, void *p)
{
	nsurl_unref(hotlist_delete_url);
	hotlist_delete_url = NULL;
	hotlist_query = QUERY_INVALID;
}


/**
 * Report whether the hotlist contains a given URL. This will be passed on to
 * the core hotlist, unless we're configured to use an external hotlist in which
 * case we always report false.
 *
 * \param *url	The URL to be tested.
 * \return	true if the hotlist contains the URL; else false.
 */

bool ro_gui_hotlist_has_page(nsurl *url)
{
	if (url == NULL || nsoption_bool(external_hotlists))
		return false;

	return hotlist_has_url(url);
}


#if 0
/**
 * Handle URL dropped on hotlist
 *
 * \param message  the wimp message we're acting on
 * \param url	   the URL to add
 */
void ro_gui_hotlist_url_drop(wimp_message *message, const char *url)
{
	int x, y;
	nsurl *nsurl;

	if (hotlist_window.window != message->data.data_xfer.w)
		return;

	if (url == NULL)
		return;

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	ro_gui_tree_get_tree_coordinates(hotlist_window.tree,
				message->data.data_xfer.pos.x,
				message->data.data_xfer.pos.y,
				&x, &y);

	hotlist_add_entry(nsurl, NULL, true, y);
	nsurl_unref(nsurl);
}
#endif
