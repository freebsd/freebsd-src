/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2010, 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Window toolbars (interface).
 */

#include <stdbool.h>
#include "riscos/theme.h"
#include "riscos/gui/button_bar.h"
#include "riscos/gui/throbber.h"
#include "riscos/gui/url_bar.h"

#ifndef _NETSURF_RISCOS_TOOLBAR_H_
#define _NETSURF_RISCOS_TOOLBAR_H_

typedef enum {
	TOOLBAR_FLAGS_NONE = 0x00,
	TOOLBAR_FLAGS_DISPLAY = 0x01,
	TOOLBAR_FLAGS_EDIT = 0x02,
} toolbar_flags;

/**
 * Widget action types that the toolbar can pass on to clients.
 */

typedef enum {
	TOOLBAR_ACTION_NONE = 0,
	TOOLBAR_ACTION_BUTTON,
	TOOLBAR_ACTION_URL
} toolbar_action_type;

/**
 * Union to hold the different widget action data that can be passed
 * from widget via toolbar to client.
 */

union toolbar_action {
	button_bar_action	button;
	url_bar_action		url;
};

struct toolbar;

struct toolbar_callbacks {
	/** Call on theme update */
	void (*theme_update)(void *, bool);

	/** Call on bar size change */
	void (*change_size)(void *);

	/** Call to update button states */
	void (*update_buttons)(void *);

	/** Call to handle user actions */
	void (*user_action)(void *, toolbar_action_type, union toolbar_action);

	/** Call to handle keypresses. */
	bool (*key_press)(void *, wimp_key *);

	/** Call on change to button order. */
	void (*save_buttons)(void *, char *);
};


#define ro_toolbar_menu_option_shade(toolbar) \
		(((toolbar) == NULL) || ro_toolbar_get_editing(toolbar))

#define ro_toolbar_menu_buttons_tick(toolbar)  \
		(ro_toolbar_get_display_buttons(toolbar) || \
		ro_toolbar_get_editing(toolbar))

#define ro_toolbar_menu_url_tick(toolbar)  \
		(ro_toolbar_get_display_url(toolbar))

#define ro_toolbar_menu_throbber_tick(toolbar)  \
		(ro_toolbar_get_display_throbber(toolbar))

#define ro_toolbar_menu_edit_shade(toolbar) ((toolbar) == NULL)

#define ro_toolbar_menu_edit_tick(toolbar) (ro_toolbar_get_editing(toolbar))


/* The new toolbar API */


/**
 * Initialise the RISC OS toolbar widget.
 */

void ro_toolbar_init(void);


/**
 * Create a new toolbar, ready to have widgets added and to be attached to
 * a window.  If a parent window is supplied, then the toolbar module will
 * handle the window attachments; if NULL, it is up to the client to sort this
 * out for itself.
 *
 * \param *descriptor		The theme to apply, or NULL for the default.
 * \param parent		The window to attach the toolbar to, or NULL.
 * \param style			The theme style to apply.
 * \param bar_flags		Toolbar flags for the new bar.
 * \param *callbacks		A client callback block, or NULL for none.
 * \param *client_data		A data pointer to pass to callbacks, or NULL.
 * \param *help			The Help token prefix for interactive help.
 * \return			The handle of the new bar, or NULL on failure.
 */

struct toolbar *ro_toolbar_create(struct theme_descriptor *descriptor,
		wimp_w parent, theme_style style, toolbar_flags bar_flags,
		const struct toolbar_callbacks *callbacks, void *client_data,
		const char *help);


/**
 * Add a button bar to a toolbar, and configure the buttons.
 *
 * \param *toolbar		The toolbar to take the button bar.
 * \param buttons[]		The button definitions.
 * \param *button_order		The initial button order to use.
 * \return			true if the action completed; else false.
 */

bool ro_toolbar_add_buttons(struct toolbar *toolbar,
		const struct button_bar_buttons buttons[], char *button_order);


/**
 * Add a throbber to a toolbar.
 *
 * \param *toolbar		The toolbar to take the throbber.
 * \return			true if the action completed; else false.
 */

bool ro_toolbar_add_throbber(struct toolbar *toolbar);


/**
 * Add a URL bar to a toolbar.
 *
 * \param *toolbar		The toolbar to take the URL bar.
 * \return			true if the action completed; else false.
 */

bool ro_toolbar_add_url(struct toolbar *toolbar);


/**
 * (Re-)build a toolbar to use the specified (or current) theme.  If false
 * is returned, the toolbar may not be complete and should be deleted.
 *
 * \param *toolbar		The toolbar to rebuild.
 * \return			true if the action was successful; else false.
 */

bool ro_toolbar_rebuild(struct toolbar *toolbar);


/**
 * Attach or re-attach a toolbar to its parent window.
 *
 * \param *toolbar		The toolbar to attach.
 * \param parent		The window to attach the toolbar to.
 * \return			true if the operation succeeded; else false.
 */

bool ro_toolbar_attach(struct toolbar *toolbar, wimp_w parent);


/**
 * Process a toolbar, updating its contents for a size or content change.
 *
 * \param *toolbar		The toolbar to update.
 * \param width			The width to reformat to, or -1 to use parent.
 * \param reformat		true to force a widget reflow; else false.
 * \return			true if the operation succeeded; else false.
 */

bool ro_toolbar_process(struct toolbar *toolbar, int width, bool reformat);


/**
 * Destroy a toolbar after use.
 *
 * \param *toolbar		The toolbar to destroy.
 */

void ro_toolbar_destroy(struct toolbar *toolbar);


/**
 * Change the client data associated with a toolbar's callbacks.
 *
 * \param *toolbar		the toolbar whose data is to be updated.
 * \param *client_data		the new client data, or NULL for none.
 */

void ro_toolbar_update_client_data(struct toolbar *toolbar, void *client_data);


/**
 * Force the update of all toolbars buttons to reflect the current state.
 */

void ro_toolbar_update_all_buttons(void);


/**
 * Refresh a toolbar after it has been updated
 *
 * \param toolbar  the toolbar to update
 */

void ro_toolbar_refresh(struct toolbar *toolbar);


/**
 * Force the update of all toolbars to reflect the application of a new theme.
 */

void ro_toolbar_theme_update(void);


/**
 * Find the toolbar associated with a given RO window handle.
 *
 * \param w		the window handle to look up.
 * \return		the toolbar handle, or NULL if a match wasn't found.
 */

struct toolbar *ro_toolbar_parent_window_lookup(wimp_w w);


/**
 * Find the toolbar using a given RO window handle for its pane.
 *
 * \param w		the window (pane) handle to look up.
 * \return		the toolbar handle, or NULL if a match wasn't found.
 */

struct toolbar *ro_toolbar_window_lookup(wimp_w w);


/**
 * Return the RO window handle of the parent window for a toolbar.
 *
 * \param *toolbar	the toolbar to look up.
 * \return		the RO window handle of the parent.
 */

wimp_w ro_toolbar_get_parent_window(struct toolbar *toolbar);


/**
 * Return the RO window handle of a toolbar.
 *
 * \param *toolbar	the toolbar to look up.
 * \return		the RO window handle of the bar.
 */

wimp_w ro_toolbar_get_window(struct toolbar *toolbar);


/**
 * Return the current height of a toolbar, allowing for available window
 * space.
 *
 * \param *toolbar		The toolbar of interest.
 * \return			The current toolbar height in OS units.
 */

int ro_toolbar_height(struct toolbar *toolbar);


/**
 * Return the full height that a toolbar could grow to, if space is available.
 *
 * \param *toolbar		The toolbar of interest.
 * \return			The full toolbar height in OS units.
 */

int ro_toolbar_full_height(struct toolbar *toolbar);


/**
 * Starts a toolbar throbber, if there is one active.
 *
 * \param *toolbar		the toolbar to start throbbing.
 */

void ro_toolbar_start_throbbing(struct toolbar *toolbar);


/**
 * Stops a toolbar throbber, if there is one active.
 *
 * \param *toolbar		the toolbar to stop throbbing.
 */

void ro_toolbar_stop_throbbing(struct toolbar *toolbar);


/**
 * Animate a toolbar throbber, if there is one active.
 *
 * \param *toolbar		the toolbar to throb.
 */

void ro_toolbar_throb(struct toolbar *toolbar);

/**
 * Change the arrangement of buttons and spacers on a button bar within a
 * toolbar.
 *
 * \param *toolbar		The toolbar to change.
 * \param order[]		The new button configuration.
 * \return			true of the order was updated; else false.
 */

bool ro_toolbar_set_button_order(struct toolbar *toolbar, char order[]);


/**
 * Set the shaded state of a toolbar button.
 *
 * \param *toolbar		the toolbar to update.
 * \param action		the button action to update.
 * \param shaded		true if the button should be shaded; else false.
 */

void ro_toolbar_set_button_shaded_state(struct toolbar *toolbar,
		button_bar_action action, bool shaded);

/**
 * Give a toolbar input focus, placing the caret into the URL bar if one is
 * present.  Currently a toolbar can only accept focus if it has a URL bar.
 *
 * \param *toolbar		The toolbar to take the caret.
 * \return			true if the caret was taken; else false.
 */

bool ro_toolbar_take_caret(struct toolbar *toolbar);


/**
 * Set the content of a toolbar's URL field.
 *
 * \param *toolbar		the toolbar to update.
 * \param *url			the new url to insert.
 * \param is_utf8		true if the string is in utf8 encoding; false
 *				if it is in local encoding.
 * \param set_caret		true if the caret should be placed in the field;
 *				else false.
 */

void ro_toolbar_set_url(struct toolbar *toolbar, const char *url,
		bool is_utf8, bool set_caret);


/**
 * Return a pointer to the URL contained in a browser toolbar.  If the toolbar
 * doesn't have a URL field, then NULL is returned instead.
 *
 * \param *toolbar		The toolbar to look up the URL from.
 * \return			pointer to the URL, or NULL.
 */

const char *ro_toolbar_get_url(struct toolbar *toolbar);


/**
 * Update the state of the URL Bar hotlist icons in all open toolbars.
 */

void ro_toolbar_update_all_hotlists(void);


/**
 * Update the state of a toolbar's URL Bar hotlist icon to reflect any changes
 * to the URL or the hotlist contents.
 *
 * \param *toolbar	The toolbar to update.
 */

void ro_toolbar_update_hotlist(struct toolbar *toolbar);


/**
 * Return the current work area coordinates of the URL and favicon field's
 * bounding box.
 *
 * \param *toolbar		The toolbar to look up.
 * \param *extent		Return the coordinates.
 * \return			true if successful; else false.
 */

bool ro_toolbar_get_url_field_extent(struct toolbar *toolbar, os_box *extent);


/**
 * Update the favicon in a browser window toolbar to the supplied content, or
 * revert to using filetype-based icons.
 *
 * \param *toolbar		The toolbar to refresh.
 * \param *h			The new favicon to use, or NULL for none.
 */

void ro_toolbar_set_site_favicon(struct toolbar *toolbar,
		struct hlcache_handle *h);


/**
 * Update the favicon in a browser window toolbar to reflect the RISC OS
 * filetype of the supplied content.  If the toolbar currently has a
 * site favicon set, then this call will be ignored.
 *
 * \param *toolbar		The toolbar to refresh.
 * \param *h			The page content to reflect.
 */

void ro_toolbar_set_content_favicon(struct toolbar *toolbar,
		struct hlcache_handle *h);


/**
 * Update the state of the URL suggestion pop-up menu icon on a toolbar.
 *
 * \param *toolbar		The toolbar to update.
 */

void ro_toolbar_update_urlsuggest(struct toolbar *toolbar);


/**
 * Set the display button bar state for a toolbar.
 *
 * \param *toolbar		the toolbar to update.
 * \param display		true to display the button bar; else false.
 */

void ro_toolbar_set_display_buttons(struct toolbar *toolbar, bool display);


/**
 * Set the display URL bar state for a toolbar.
 *
 * \param *toolbar		the toolbar to update.
 * \param display		true to display the URL bar; else false.
 */

void ro_toolbar_set_display_url(struct toolbar *toolbar, bool display);


/**
 * Set the display throbber state for a toolbar.
 *
 * \param *toolbar		the toolbar to update.
 * \param display		true to display the throbber; else false.
 */

void ro_toolbar_set_display_throbber(struct toolbar *toolbar, bool display);


/**
 * Return true or false depending on whether the given toolbar is set to
 * display the button bar.
 *
 * \param *toolbar		the toolbar of interest.
 * \return			true if the toolbar exists and the button bar is
 *				shown; else false.
 */

bool ro_toolbar_get_display_buttons(struct toolbar *toolbar);


/**
 * Return true or false depending on whether the given toolbar is set to
 * display the URL bar.
 *
 * \param *toolbar		the toolbar of interest.
 * \return			true if the toolbar exists and the URL bar is
 *				shown; else false.
 */

bool ro_toolbar_get_display_url(struct toolbar *toolbar);


/**
 * Return true or false depending on whether the given toolbar is set to
 * display the throbber.
 *
 * \param *toolbar		the toolbar of interest.
 * \return			true if the toolbar exists and the throbber is
 *				shown; else false.
 */

bool ro_toolbar_get_display_throbber(struct toolbar *toolbar);


/**
 * Return true or false depending on whether the given toolbar is currently
 * being edited.
 *
 * \param *toolbar		the toolbar of interest.
 * \return			true if the toolbar exists and is beng edited;
 *				else false.
 */

bool ro_toolbar_get_editing(struct toolbar *toolbar);


/**
 * Toggle toolbar edit mode on the given toolbar.  Only a button bar can be
 * edited, so edit mode can only be toggled if there's an editor button
 * bar defined.
 *
 * \param *toolbar		The toolbar to be toggled.
 * \return			true if the action was successful; false if
 *				the action failed and the toolbar was destroyed.
 */

bool ro_toolbar_toggle_edit(struct toolbar *toolbar);

#endif

