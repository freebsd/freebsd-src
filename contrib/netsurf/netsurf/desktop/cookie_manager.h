/*
 * Copyright 2013 Michael Drake <tlsa@netsurf-browser.org>
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
 * Cookie Manager (interface).
 */

#ifndef _NETSURF_DESKTOP_COOKIE_MANAGER_H_
#define _NETSURF_DESKTOP_COOKIE_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "desktop/browser.h"
#include "desktop/core_window.h"
#include "desktop/textinput.h"
#include "utils/errors.h"

struct cookie_data;

/**
 * Initialise the cookie manager.
 *
 * This iterates through the URL database, enumerating the cookies and
 * creates a treeview.
 *
 * This must be called before any other cookie_manager_* function.
 *
 * \param cw_t		Callback table for core_window containing the treeview
 * \param cw		The core_window in which the treeview is shown
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror cookie_manager_init(struct core_window_callback_table *cw_t,
		void *core_window_handle);

/**
 * Finalise the cookie manager.
 *
 * This destroys the cookie manager treeview and the cookie manager module's
 * internal data.  After calling this if the cookie manager is required again,
 * cookie_manager_init must be called.
 *
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror cookie_manager_fini(void);

/**
 * Add/update a cookie to the viewer. (Called by urldb.)
 *
 * \param data		Data of cookie being added/updated.
 * \return true (for urldb_iterate_entries)
 */
bool cookie_manager_add(const struct cookie_data *data);

/**
 * Remove a cookie from viewer. (Called by urldb.)
 *
 * \param data Data of cookie being removed.
 */
void cookie_manager_remove(const struct cookie_data *data);

/**
 * Redraw the cookies manager.
 *
 * \param x		X coordinate to render treeview at
 * \param x		Y coordinate to render treeview at
 * \param clip		Current clip rectangle (wrt tree origin)
 * \param ctx		Current redraw context
 */
void cookie_manager_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx);

/**
 * Handles all kinds of mouse action
 *
 * \param mouse		The current mouse state
 * \param x		X coordinate
 * \param y		Y coordinate
 */
void cookie_manager_mouse_action(browser_mouse_state mouse, int x, int y);

/**
 * Key press handling.
 *
 * \param key		The ucs4 character codepoint
 * \return true if the keypress is dealt with, false otherwise.
 */
void cookie_manager_keypress(uint32_t key);

/**
 * Determine whether there is a selection
 *
 * \return true iff there is a selection
 */
bool cookie_manager_has_selection(void);

/**
 * Expand the treeview's nodes
 *
 * \param only_folders	Iff true, only folders are expanded.
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror cookie_manager_expand(bool only_folders);

/**
 * Contract the treeview's nodes
 *
 * \param all		Iff false, only entries are contracted.
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror cookie_manager_contract(bool all);

#endif
