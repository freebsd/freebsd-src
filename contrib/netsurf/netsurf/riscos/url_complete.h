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

/** \file
 * Central repository for URL data (interface).
 */

#ifndef _NETSURF_RISCOS_URLCOMPLETE_H_
#define _NETSURF_RISCOS_URLCOMPLETE_H_

#include <inttypes.h>
#include <stdbool.h>
#include "oslib/wimp.h"

struct gui_window;

/**
 * Should be called when the caret is placed into a URL completion icon.
 *
 * \param *toolbar		The toolbar to initialise URL completion for.
 */

void ro_gui_url_complete_start(struct toolbar *toolbar);


/**
 * Handles a keypress for URL completion
 *
 * \param *toolbar		The toolbar to be updated.
 * \param key			the key pressed (as UTF32 code or
 *				wimp key + bit31 set)
 * \return			true to indicate keypress handled; else false.
 */

bool ro_gui_url_complete_keypress(struct toolbar *toolbar, uint32_t key);


/**
 * Move and resize the url completion window to match the toolbar.
 *
 * \param *toolbar		The toolbar to update
 * \param *open			the wimp_open request (updated on exit)
 */

void ro_gui_url_complete_resize(struct toolbar *toolbar, wimp_open *open);


/**
 * Try to close the current url completion window
 *
 * \return whether the window was closed
 */

bool ro_gui_url_complete_close(void);


/**
 * Redraws a section of the URL completion window
 *
 * \param redraw  the area to redraw
 */

void ro_gui_url_complete_redraw(wimp_draw *redraw);


/**
 * Handle the pointer entering the URL completion window.
 *
 * \param *entering	The pointer entering data block.
 */ 

void ro_gui_url_complete_entering(wimp_entering *entering);


/**
 * Handle mouse clicks in the URL completion window.
 *
 * \param pointer  the pointer state
 * \return whether the click was handled
 */

bool ro_gui_url_complete_click(wimp_pointer *pointer);

#endif

