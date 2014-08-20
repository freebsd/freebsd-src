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
 * URL bars (interface).
 */

#ifndef _NETSURF_RISCOS_URLBAR_H_
#define _NETSURF_RISCOS_URLBAR_H_

#include <stdbool.h>
#include "riscos/menus.h"
#include "riscos/theme.h"

/* A list of possible URL bar actions. */

typedef enum {
	TOOLBAR_URL_NONE = 0,		/* Special case: no action */
	TOOLBAR_URL_DRAG_URL,
	TOOLBAR_URL_DRAG_FAVICON,
	TOOLBAR_URL_SELECT_HOTLIST,
	TOOLBAR_URL_ADJUST_HOTLIST
} url_bar_action;

struct url_bar;

/**
 * Initialise the url bar module.
 *
 * \return			True iff success, else false.
 */

bool ro_gui_url_bar_init(void);

/**
 * Finalise the url bar module
 */

void ro_gui_url_bar_fini(void);

/**
 * Create a new url bar widget.
 *
 * \param *theme		The theme to apply (or NULL for the default).
 * \return			A url bar handle, or NULL on failure.
 */

struct url_bar *ro_gui_url_bar_create(struct theme_descriptor *theme);


/**
 * Place a URL bar into a toolbar window and initialise any theme-specific
 * settings.  Any previous incarnation of the bar will be forgotten: this
 * is for use when a new toolbar is being created, or when a toolbar has been
 * deleted and rebuilt following a theme change.
 *
 * \param *url_bar		The URL bar to rebuild.
 * \param *theme		The theme to apply (or NULL for current).
 * \param style			The theme style to apply.
 * \param window		The window that the bar is in.
 * \param display		true if the bar should be for display only.
 * \param shaded		true if the bar should be shaded; else false.
 * \return			true on success; else false.
 */

bool ro_gui_url_bar_rebuild(struct url_bar *url_bar,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool display, bool shaded);


/**
 * Destroy a url bar widget.
 *
 * \param *url_bar		The url bar to destroy.
 */

void ro_gui_url_bar_destroy(struct url_bar *url_bar);


/**
 * Return the MINIMUM dimensions required by the URL bar, in RO units,
 * allowing for the current theme.
 *
 * \param *url_bar		The URL bar of interest.
 * \param *width		Return the required width.
 * \param *height		Return the required height.
 * \return			true if values are returned; else false.
 */

bool ro_gui_url_bar_get_dims(struct url_bar *url_bar,
		int *width, int *height);


/**
 * Set or update the dimensions to be used by the URL bar, in RO units.
 * If these are greater than the minimum required, the URL bar will fill
 * the extended space; if less, the call will fail.
 *
 * \param *url_bar		The URL bar to update.
 * \param x0			The minimum X window position.
 * \param y0			The minimum Y window position.
 * \param x1			The maximum X window position.
 * \param y1			The maximum Y window position.
 * \return			true if size updated; else false.
 */

bool ro_gui_url_bar_set_extent(struct url_bar *url_bar,
		int x0, int y0, int x1, int y1);


/**
 * Show or hide a URL bar.
 *
 * \param *url_bar		The URL bar to hide.
 * \param hide			true to hide the bar; false to show it.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_hide(struct url_bar *url_bar, bool hide);


/**
 * Handle redraw event rectangles in a URL bar.
 *
 * \param *url_bar		The URL bar to use.
 * \param *redraw		The Wimp redraw rectangle to process.
 */

void ro_gui_url_bar_redraw(struct url_bar *url_bar, wimp_draw *redraw);


/**
 * Handle mouse clicks in a URL bar.
 *
 * \param *url_bar		The URL bar to use.
 * \param *pointer		The Wimp mouse click event data.
 * \param *state		The toolbar window state.
 * \param *action		Returns the selected action, or
 *				TOOLBAR_URL_NONE.
 * \return			true if the event was handled exclusively;
 *				else false.
 */

bool ro_gui_url_bar_click(struct url_bar *url_bar,
		wimp_pointer *pointer, wimp_window_state *state,
		url_bar_action *action);


/**
 * Process offered menu prepare events from the parent window.
 *
 * \param *url_bar		The URL bar in question.
 * \param i			The icon owning the menu.
 * \param *menu			The menu to be prepared.
 * \param *pointer		The Wimp Pointer data from the event.
 * \return			true if the event is claimed; else false.
 */

bool ro_gui_url_bar_menu_prepare(struct url_bar *url_bar, wimp_i i,
		wimp_menu *menu, wimp_pointer *pointer);


/**
 * Process offered menu select events from the parent window.
 *
 * \param *url_bar		The URL bar in question.
 * \param i			The icon owning the menu.
 * \param *menu			The menu to be prepared.
 * \param *selection		The wimp menu selection data.
 * \param action		The selected menu action.
 * \return			true if the event is claimed; else false.
 */

bool ro_gui_url_bar_menu_select(struct url_bar *url_bar, wimp_i i,
		wimp_menu *menu, wimp_selection *selection, menu_action action);


/**
 * Translate mouse data into an interactive help message for the URL bar.
 *
 * \param *url_bar		The URL bar to process.
 * \param i			The wimp icon under the pointer.
 * \param *mouse		The mouse position.
 * \param *state		The toolbar window state.
 * \param buttons		The mouse button state.
 * \param **suffix		Return a help token suffix, or "" for none.
 * \return			true if handled exclusively; else false.
 */

bool ro_gui_url_bar_help_suffix(struct url_bar *url_bar, wimp_i i,
		os_coord *mouse, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix);


/**
 * Give a URL bar input focus.
 *
 * \param *url_bar		The URL bar to give focus to.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_take_caret(struct url_bar *url_bar);


/**
 * Set the content of a URL Bar field.
 *
 * \param *url_bar		The URL Bar to update.
 * \param *url			The new url to insert.
 * \param is_utf8		true if the string is in utf8 encoding; false
 *				if it is in local encoding.
 * \param set_caret		true if the caret should be placed in the field;
 *				else false.
 */

void ro_gui_url_bar_set_url(struct url_bar *url_bar, const char *url,
		bool is_utf8, bool set_caret);


/**
 * Update the state of a URL Bar's hotlist icon to reflect any changes to the
 * URL or the contents of the hotlist.
 *
 * \param *url_bar	The URL Bar to update.
 */

void ro_gui_url_bar_update_hotlist(struct url_bar *url_bar);


/**
 * Return a pointer to the URL contained in a URL bar.
 *
 * \param *url_bar		The URL Bar to look up the URL from.
 * \return			Pointer to the URL, or NULL.
 */

const char *ro_gui_url_bar_get_url(struct url_bar *url_bar);


/**
 * Return the current work area coordinates of the URL and favicon field's
 * bounding box.
 *
 * \param *url_bar		The URL bar to check.
 * \param *extent		Returns the field extent.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_get_url_extent(struct url_bar *url_bar, os_box *extent);


/**
 * Test a pointer click to see if it was in the URL bar's text field.
 *
 * \param *url_bar		The URL Bar to test.
 * \param *pointer		The pointer event data to test.
 * \return			true if the click was in the field; else false.
 */

bool ro_gui_url_bar_test_for_text_field_click(struct url_bar *url_bar,
		wimp_pointer *pointer);


/**
 * Test a keypress to see if it was in the URL bar's text field.
 *
 * \param *url_bar		The URL Bar to test.
 * \param *pointer		The pointer event data to test.
 * \return			true if the click was in the field; else false.
 */

bool ro_gui_url_bar_test_for_text_field_keypress(struct url_bar *url_bar,
		wimp_key *key);


/**
 * Set the favicon to a site supplied favicon image, or remove the image
 * and return to using filetype-based icons.
 *
 * \param *url_bar		The URL Bar to update the favicon on.
 * \param *h			The content to use, or NULL to unset.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_set_site_favicon(struct url_bar *url_bar,
		struct hlcache_handle *h);


/**
 * Set the favicon to a RISC OS filetype sprite based on the type of the
 * supplied content.
 *
 * \param *url_bar		The URL Bar to update the favicon on.
 * \param *h			The content to use.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_set_content_favicon(struct url_bar *url_bar,
		struct hlcache_handle *h);


/**
 * Update the state of the URL suggestion pop-up menu icon on a URL bar.
 *
 * \param *url_bar		The URL bar to update.
 * \return			true if successful; else false.
 */

bool ro_gui_url_bar_update_urlsuggest(struct url_bar *url_bar);

#endif

