/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2011 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Button bars (interface).
 */

#ifndef _NETSURF_RISCOS_BUTTONBAR_H_
#define _NETSURF_RISCOS_BUTTONBAR_H_

#include <stdbool.h>
#include "riscos/theme.h"

/* A list of possible toolbar actions. */

typedef enum {
	TOOLBAR_BUTTON_NONE = 0,		/* Special case: no action */
	TOOLBAR_BUTTON_BACK,
	TOOLBAR_BUTTON_BACK_NEW,
	TOOLBAR_BUTTON_UP,
	TOOLBAR_BUTTON_UP_NEW,
	TOOLBAR_BUTTON_FORWARD,
	TOOLBAR_BUTTON_FORWARD_NEW,
	TOOLBAR_BUTTON_STOP,
	TOOLBAR_BUTTON_RELOAD,
	TOOLBAR_BUTTON_RELOAD_ALL,
	TOOLBAR_BUTTON_HOME,
	TOOLBAR_BUTTON_HISTORY_LOCAL,
	TOOLBAR_BUTTON_HISTORY_GLOBAL,
	TOOLBAR_BUTTON_SAVE_SOURCE,
	TOOLBAR_BUTTON_SAVE_COMPLETE,
	TOOLBAR_BUTTON_PRINT,
	TOOLBAR_BUTTON_BOOKMARK_OPEN,
	TOOLBAR_BUTTON_BOOKMARK_ADD,
	TOOLBAR_BUTTON_SCALE,
	TOOLBAR_BUTTON_SEARCH,
	TOOLBAR_BUTTON_DELETE,
	TOOLBAR_BUTTON_EXPAND,
	TOOLBAR_BUTTON_COLLAPSE,
	TOOLBAR_BUTTON_OPEN,
	TOOLBAR_BUTTON_CLOSE,
	TOOLBAR_BUTTON_LAUNCH,
	TOOLBAR_BUTTON_CREATE
} button_bar_action;

/* Button bar button source definitions.
 *
 * Help tokens are added to the help prefix for the given toolbar by the
 * help system, and correspond to the hard-coded icon numbers that were
 * assigned to the different buttons in the original toolbar implementation.
 * If the Messages file can be updated, these can change to something more
 * meaningful.
 */

struct button_bar_buttons {
	const char		*icon;		/**< The sprite used for the icon.    */
	button_bar_action	select;		/**< The action for select clicks.    */
	button_bar_action	adjust;		/**< The action for Adjust clicks.    */
	const char		opt_key;	/**< The char used in option strings. */
	const char		*help;		/**< The interactive help token.      */
};

/* \TODO -- Move these to the correct modules.
 */

static const struct button_bar_buttons brower_toolbar_buttons[] = {
	{"back", TOOLBAR_BUTTON_BACK, TOOLBAR_BUTTON_BACK_NEW, '0', "0"},
	{"up", TOOLBAR_BUTTON_UP, TOOLBAR_BUTTON_UP_NEW, 'b', "11"},
	{"forward", TOOLBAR_BUTTON_FORWARD, TOOLBAR_BUTTON_FORWARD_NEW, '1', "1"},
	{"stop", TOOLBAR_BUTTON_STOP, TOOLBAR_BUTTON_NONE, '2', "2"},
	{"reload", TOOLBAR_BUTTON_RELOAD, TOOLBAR_BUTTON_RELOAD_ALL, '3', "3"},
	{"home", TOOLBAR_BUTTON_HOME, TOOLBAR_BUTTON_NONE, '4', "4"},
	{"history", TOOLBAR_BUTTON_HISTORY_LOCAL, TOOLBAR_BUTTON_HISTORY_GLOBAL, '5', "5"},
	{"save", TOOLBAR_BUTTON_SAVE_SOURCE, TOOLBAR_BUTTON_SAVE_COMPLETE, '6', "6"},
	{"print", TOOLBAR_BUTTON_PRINT, TOOLBAR_BUTTON_NONE, '7', "7"},
	{"hotlist", TOOLBAR_BUTTON_BOOKMARK_OPEN, TOOLBAR_BUTTON_BOOKMARK_ADD, '8', "8"},
	{"scale", TOOLBAR_BUTTON_SCALE, TOOLBAR_BUTTON_NONE, '9', "9"},
	{"search", TOOLBAR_BUTTON_SEARCH, TOOLBAR_BUTTON_NONE, 'a', "10"},
	{NULL, TOOLBAR_BUTTON_NONE, TOOLBAR_BUTTON_NONE, '\0', ""}
};

static const struct button_bar_buttons cookies_toolbar_buttons[] = {
	{"delete", TOOLBAR_BUTTON_DELETE, TOOLBAR_BUTTON_NONE, '0', "0"},
	{"expand", TOOLBAR_BUTTON_EXPAND, TOOLBAR_BUTTON_COLLAPSE, '1', "1"},
	{"open", TOOLBAR_BUTTON_OPEN, TOOLBAR_BUTTON_CLOSE, '2', "2"},
	{NULL, TOOLBAR_BUTTON_NONE, TOOLBAR_BUTTON_NONE, '\0', ""}
};

static const struct button_bar_buttons global_history_toolbar_buttons[] = {
	{"delete", TOOLBAR_BUTTON_DELETE, TOOLBAR_BUTTON_NONE, '0', "0"},
	{"expand", TOOLBAR_BUTTON_EXPAND, TOOLBAR_BUTTON_COLLAPSE, '1', "1"},
	{"open", TOOLBAR_BUTTON_OPEN, TOOLBAR_BUTTON_CLOSE, '2', "2"},
	{"launch", TOOLBAR_BUTTON_LAUNCH, TOOLBAR_BUTTON_NONE, '3', "3"},
	{NULL, TOOLBAR_BUTTON_NONE, TOOLBAR_BUTTON_NONE, '\0', ""}
};

static const struct button_bar_buttons hotlist_toolbar_buttons[] = {
	{"delete", TOOLBAR_BUTTON_DELETE, TOOLBAR_BUTTON_NONE, '0', "0"},
	{"expand", TOOLBAR_BUTTON_EXPAND, TOOLBAR_BUTTON_COLLAPSE, '1', "1"},
	{"open", TOOLBAR_BUTTON_OPEN, TOOLBAR_BUTTON_CLOSE, '2', "2"},
	{"launch", TOOLBAR_BUTTON_LAUNCH, TOOLBAR_BUTTON_NONE, '3', "3"},
	{"create", TOOLBAR_BUTTON_CREATE, TOOLBAR_BUTTON_NONE, '4', "4"},
	{NULL, TOOLBAR_BUTTON_NONE, TOOLBAR_BUTTON_NONE, '\0', ""}
};

struct button_bar;


/**
 * Create a new button bar widget.
 *
 * \param *theme		The theme to apply (or NULL for the default).
 * \param buttons[]		An array of button definitions for the bar.
 * \return			A button bar handle, or NULL on failure.
 */

struct button_bar *ro_gui_button_bar_create(struct theme_descriptor *theme,
		const struct button_bar_buttons buttons[]);


/**
 * Link two button bars together: the target being the active bar, and the
 * source being the editing bar used to supply valid buttons.  The bars are
 * checked to ensure that they are not already part of an edit pair, but are
 * not checked for button-compatibility.
 *
 * \param *target		The target button bar.
 * \param *source		The source button bar.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_link_editor(struct button_bar *target,
		struct button_bar *source, void (* refresh)(void *),
		void *client_data);

/**
 * Place a button bar into a toolbar window and initialise any theme-specific
 * settings.  Any previous incarnation of the bar will be forgotten: this
 * is for use when a new toolbar is being created, or when a toolbar has been
 * deleted and rebuilt following a theme change.
 *
 * \param *button_bar		The button bar to rebuild.
 * \param *theme		The theme to apply (or NULL for current).
 * \param style			The theme style to apply.
 * \param window		The window that the bar is in.
 * \param edit			The edit mode of the button bar.
 * \return			true on success; else false.
 */

bool ro_gui_button_bar_rebuild(struct button_bar *button_bar,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool edit);


/**
 * Arrange buttons on a button bar, using an order string to specify the
 * required button and separator layout.
 *
 * \param *button_bar		The button bar to update.
 * \param order[]		The button order configuration string.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_arrange_buttons(struct button_bar *button_bar,
		char order[]);


/**
 * Destroy a button bar widget.
 *
 * \param *button_bar		The button bar to destroy.
 */

void ro_gui_button_bar_destroy(struct button_bar *button_bar);


/**
 * Return the MINIMUM dimensions required by the button bar, in RO units,
 * allowing for the current theme.
 *
 * \param *button_bar		The button bar of interest.
 * \param *width		Return the required width.
 * \param *height		Return the required height.
 * \return			true if values are returned; else false.
 */

bool ro_gui_button_bar_get_dims(struct button_bar *button_bar,
		int *width, int *height);


/**
 * Set or update the dimensions to be used by the button bar, in RO units.
 * If these are greater than the minimum required, the button bar will fill
 * the extended space; if less, the call will fail.
 *
 * \param *button_bar		The button bar to update.
 * \param x0			The minimum X window position.
 * \param y0			The minimum Y window position.
 * \param x1			The maximum X window position.
 * \param y1			The maximum Y window position.
 * \return			true if size updated; else false.
 */

bool ro_gui_button_bar_set_extent(struct button_bar *button_bar,
		int x0, int y0, int x1, int y1);


/**
 * Show or hide a button bar.
 *
 * \param *button_bar		The button bar to hide.
 * \param hide			true to hide the bar; false to show it.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_hide(struct button_bar *button_bar, bool hide);


/**
 * Shade or unshade a button in a bar corresponding to the given action.
 *
 * \param *button_bar		The button bar to update.
 * \param action		The action to update.
 * \param shaded		true to shade the button; false to unshade.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_shade_button(struct button_bar *button_bar,
		button_bar_action action, bool shaded);


/**
 * Handle redraw event rectangles in a button bar.
 *
 * \param *button_bar		The button bar to use.
 * \param *redraw		The Wimp redraw rectangle to process.
 */

void ro_gui_button_bar_redraw(struct button_bar *button_bar,
		wimp_draw *redraw);


/**
 * Handle mouse clicks in a button bar.
 *
 * \param *button_bar		The button bar to use.
 * \param *pointer		The Wimp mouse click event data.
 * \param *state		The toolbar window state.
 * \param *action		Returns the selected action, or
 *				TOOLBAR_BUTTON_NONE.
 * \return			true if the event was handled exclusively;
 *				else false.
 */

bool ro_gui_button_bar_click(struct button_bar *button_bar,
		wimp_pointer *pointer, wimp_window_state *state,
		button_bar_action *action);


/**
 * Translate mouse data into an interactive help message for a button bar.
 *
 * \param *button_bar		The button bar to process.
 * \param i			The wimp icon under the pointer.
 * \param *mouse		The mouse position.
 * \param *state		The toolbar window state.
 * \param buttons		The mouse button state.
 * \param **suffix		Return a help token suffix, or "" for none.
 * \return			true if handled exclusively; else false.
 */

bool ro_gui_button_bar_help_suffix(struct button_bar *button_bar, wimp_i i,
		os_coord *mouse, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix);


/**
 * Return a config string reflecting the configured order of buttons
 * and spacers.  The string is allocated with malloc(), and should be
 * free()d after use.
 *
 * \param *button_bar		The button bar of interest.
 * \return			Pointer to a config string, or NULL on failure.
 */

char *ro_gui_button_bar_get_config(struct button_bar *button_bar);

#endif

