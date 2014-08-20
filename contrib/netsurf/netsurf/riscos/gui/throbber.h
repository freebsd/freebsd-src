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
 * Throbber (interface).
 */

#ifndef _NETSURF_RISCOS_THROBBER_H_
#define _NETSURF_RISCOS_THROBBER_H_

#include <stdbool.h>
#include "riscos/theme.h"

struct throbber;


/**
 * Create a new throbber widget.
 *
 * \param *theme		The theme to apply (or NULL for the default).
 * \return			A throbber handle, or NULL on failure.
 */

struct throbber *ro_gui_throbber_create(struct theme_descriptor *theme);

/**
 * Place a throbber into a toolbar window and initialise any theme-specific
 * settings.  Any previous incarnation of the throbber will be forgotten: this
 * is for use when a new toolbar is being created, or when a toolbar has been
 * deleted and rebuilt following a theme change.
 *
 * \param *throbber		The throbber to rebuild.
 * \param *theme		The theme to apply (or NULL for current).
 * \param style			The theme style to apply.
 * \param window		The window that the throbber is in.
 * \param shaded		true if the bar should be throbber; else false.
 * \return			true on success; else false.
 */

bool ro_gui_throbber_rebuild(struct throbber *throbber,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool shaded);


/**
 * Destroy a throbber widget.
 *
 * \param *throbber		The throbber to destroy.
 */

void ro_gui_throbber_destroy(struct throbber *throbber);


/**
 * Return the MINIMUM dimensions required by the throbber, in RO units,
 * allowing for the current theme.
 *
 * \param *throbber		The throbber of interest.
 * \param *width		Return the required width.
 * \param *height		Return the required height.
 * \return			true if values are returned; else false.
 */

bool ro_gui_throbber_get_dims(struct throbber *throbber,
		int *width, int *height);


/**
 * Set or update the dimensions to be used by the throbber, in RO units.
 * If these are greater than the minimum required, the throbber will fill
 * the extended space; if less, the call will fail.
 *
 * \param *throbber		The throbber to update.
 * \param width			The desired width.
 * \param height		The desired height.
 * \return			true if size updated; else false.
 */

bool ro_gui_throbber_set_extent(struct throbber *throbber,
		int x0, int y0, int x1, int y1);


/**
 * Show or hide a throbber.
 *
 * \param *throbber		The throbber to hide.
 * \param hide			true to hide the throbber; false to show it.
 * \return			true if successful; else false.
 */

bool ro_gui_throbber_hide(struct throbber *throbber, bool hide);


/**
 * Translate mouse data into an interactive help message for the throbber.
 *
 * \param *throbber		The throbber to process.
 * \param i			The wimp icon under the pointer.
 * \param *mouse		The mouse position.
 * \param *state		The toolbar window state.
 * \param buttons		The mouse button state.
 * \param **suffix		Return a help token suffix, or "" for none.
 * \return			true if handled exclusively; else false.
 */

bool ro_gui_throbber_help_suffix(struct throbber *throbber, wimp_i i,
		os_coord *screenpos, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix);

/**
 * Start or update the amimation of a throbber.
 *
 * \param *throbber		The throbber to amimate.
 */

bool ro_gui_throbber_animate(struct throbber *throbber);


/**
 * Stop the amimation of a throbber.
 *
 * \param *throbber		The throbber to amimate.
 */

bool ro_gui_throbber_stop(struct throbber *throbber);

#endif

