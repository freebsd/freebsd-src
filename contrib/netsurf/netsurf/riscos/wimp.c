/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
 * Copyright 2008 John Tytgat <joty@netsurf-browser.org>
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
 * General RISC OS WIMP/OS library functions (implementation).
 */

#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osfile.h"
#include "oslib/wimp.h"
#include "oslib/wimpextend.h"
#include "oslib/wimpspriteop.h"
#include "desktop/gui.h"
#include "riscos/gui.h"
#include "riscos/oslib_pre7.h"
#include "riscos/wimp.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "riscos/ucstables.h"


static void ro_gui_wimp_cache_furniture_sizes(wimp_w w);
static size_t ro_gui_strlen(const char *str);
static int ro_gui_strncmp(const char *s1, const char *s2, size_t len);

static wimpextend_furniture_sizes furniture_sizes;
static wimp_w furniture_window = NULL;

/**
 * Gets the horizontal scrollbar height
 *
 * \param  w  the window to read (or NULL to read a cached value)
 */
int ro_get_hscroll_height(wimp_w w)
{
	ro_gui_wimp_cache_furniture_sizes(w);
	return furniture_sizes.border_widths.y0;
}


/**
 * Gets the vertical scrollbar width
 *
 * \param  w  the window to read (or NULL to read a cached value)
 */
int ro_get_vscroll_width(wimp_w w)
{
	ro_gui_wimp_cache_furniture_sizes(w);
	return furniture_sizes.border_widths.x1;
}


/**
 * Gets the title bar height
 *
 * \param  w  the window to read (or NULL to read a cached value)
 */
int ro_get_title_height(wimp_w w)
{
	ro_gui_wimp_cache_furniture_sizes(w);
	return furniture_sizes.border_widths.y1;
}

/**
 * Caches window furniture information
 *
 * \param  w  the window to cache information from
 * \return true on success, false on error (default values cached)
 */
void ro_gui_wimp_cache_furniture_sizes(wimp_w w)
{
	os_error *error;

	if (furniture_window == w)
		return;
	furniture_window = w;
	furniture_sizes.w = w;
	furniture_sizes.border_widths.y0 = 40;
	furniture_sizes.border_widths.x1 = 40;
	error = xwimpextend_get_furniture_sizes(&furniture_sizes);
	if (error) {
		LOG(("xwimpextend_get_furniture_sizes: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Reads a modes EIG factors.
 *
 * \param  mode  mode to read EIG factors for, or -1 for current
 */
bool ro_gui_wimp_read_eig_factors(os_mode mode, int *xeig, int *yeig)
{
	os_error *error;

	error = xos_read_mode_variable(mode, os_MODEVAR_XEIG_FACTOR, xeig, 0);
	if (error) {
		LOG(("xos_read_mode_variable: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return false;
	}
	error = xos_read_mode_variable(mode, os_MODEVAR_YEIG_FACTOR, yeig, 0);
	if (error) {
		LOG(("xos_read_mode_variable: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return false;
	}
	return true;
}


/**
 * Converts the supplied os_coord from OS units to pixels.
 *
 * \param  os_units  values to convert
 * \param  mode	     mode to use EIG factors for, or -1 for current
 */
void ro_convert_os_units_to_pixels(os_coord *os_units, os_mode mode)
{
	int xeig = 1, yeig = 1;

	ro_gui_wimp_read_eig_factors(mode, &xeig, &yeig);
	os_units->x = ((os_units->x + (1 << xeig) - 1) >> xeig);
	os_units->y = ((os_units->y + (1 << yeig) - 1) >> yeig);
}


/**
 * Converts the supplied os_coord from pixels to OS units.
 *
 * \param  pixels  values to convert
 * \param  mode	   mode to use EIG factors for, or -1 for current
 */
void ro_convert_pixels_to_os_units(os_coord *pixels, os_mode mode)
{
	int xeig = 1, yeig = 1;

	ro_gui_wimp_read_eig_factors(mode, &xeig, &yeig);
	pixels->x = (pixels->x << xeig);
	pixels->y = (pixels->y << yeig);
}


/**
 * Redraws an icon
 *
 * \param  w  window handle
 * \param  i  icon handle
 */

#define ro_gui_redraw_icon(w, i) xwimp_set_icon_state(w, i, 0, 0)


/**
 * Forces an icon to be redrawn entirely (ie not just updated).
 *
 * \param  w  window handle
 * \param  i  icon handle
 */
void ro_gui_force_redraw_icon(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	os_error *error;

	/*	Get the icon data
	*/
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	error = xwimp_force_redraw(w, ic.icon.extent.x0, ic.icon.extent.y0,
			ic.icon.extent.x1, ic.icon.extent.y1);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Read the contents of a text or sprite icon.
 *
 * \param  w  window handle
 * \param  i  icon handle
 * \return NUL terminated string in icon
 *
 * If the icon contains direct text then the returned data will
 * be invalidated by the next call to this function. Therefore,
 * all client calls to this function must either copy the string or
 * ensure that this function is not called again until they are
 * finished with the string data returned.
 *
 * \todo this doesn't do local encoding -> UTF-8 to match what is done in
 * ro_gui_set_icon_string.
 */
const char *ro_gui_get_icon_string(wimp_w w, wimp_i i)
{
	static wimp_icon_state ic;
	os_error *error;
	char *itext;

	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return NULL;
	}
	itext = (ic.icon.flags & wimp_ICON_INDIRECTED)
			? ic.icon.data.indirected_text.text : ic.icon.data.text;
	/* Guarantee NUL termination.  */
	itext[ro_gui_strlen(itext)] = '\0';

	return itext;
}


/**
 * Set the contents of a text or sprite icon to a string.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  text  NUL terminated string (copied)
 * \param  is_utf8 When true, the given string is UTF-8 encoded and will be
 * converted to local encoding currently used by the Wimp. When false, the
 * given string is assumed to be in local encoding in use by the Wimp.
 */
void ro_gui_set_icon_string(wimp_w w, wimp_i i, const char *text, bool is_utf8)
{
	wimp_caret caret;
	wimp_icon_state ic;
	os_error *error;
	size_t old_len, new_len;
	char *local_text = NULL;
	const char *text_for_icon;
	char *dst_text;
	size_t dst_max_len;
	unsigned int button_type;

	if (is_utf8) {
		nserror err;
		/* convert text to local encoding */
		err = utf8_to_local_encoding(text, 0, &local_text);
		if (err != NSERROR_OK) {
			/* A bad encoding should never happen, so assert this */
			assert(err != NSERROR_BAD_ENCODING);
			LOG(("utf8_to_enc failed"));
			/* Paranoia */
			local_text = NULL;
		}
		text_for_icon = local_text ? local_text : text;
	}
	else
		text_for_icon = text;
	new_len = strlen(text_for_icon);

	/* get the icon data */
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		goto exit;
	}

	if (ic.icon.flags & wimp_ICON_INDIRECTED) {
		dst_text = ic.icon.data.indirected_text.text;
		dst_max_len = ic.icon.data.indirected_text.size;
	}
	else {
		dst_text = ic.icon.data.text;
		dst_max_len = sizeof(ic.icon.data.text);
	}
	old_len = ro_gui_strlen(dst_text);
	assert(old_len < dst_max_len);

	/* check that the existing text is not the same as the updated text
	 * to stop flicker */
	if (dst_max_len) {
		if (!ro_gui_strncmp(dst_text, text_for_icon, dst_max_len))
			goto exit;

		/* copy the text across */
		strncpy(dst_text, text_for_icon, dst_max_len - 1);
		dst_text[dst_max_len - 1] = '\0';

		/* handle the caret being in the icon */
		button_type = (ic.icon.flags & wimp_ICON_BUTTON_TYPE)
				>> wimp_ICON_BUTTON_TYPE_SHIFT;
		if ((button_type == wimp_BUTTON_WRITABLE) ||
				(button_type == wimp_BUTTON_WRITE_CLICK_DRAG)) {
			error = xwimp_get_caret_position(&caret);
			if (error) {
				LOG(("xwimp_get_caret_position: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				goto exit;
			}
			if ((caret.w == w) && (caret.i == i)) {
				if ((size_t)caret.index > new_len
						|| (size_t)caret.index == old_len)
					caret.index = new_len;
				error = xwimp_set_caret_position(w, i, caret.pos.x,
						caret.pos.y, -1, caret.index);
				if (error) {
					LOG(("xwimp_set_caret_position: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
				}
			}
		}
		ro_gui_redraw_icon(w, i);
	}

exit:
	free(local_text);
}


/**
 * Set the contents of an icon to a number.
 *
 * \param  w	  window handle
 * \param  i	  icon handle
 * \param  value  value
 */
void ro_gui_set_icon_integer(wimp_w w, wimp_i i, int value)
{
	char buffer[20]; // Big enough for 64-bit int

	setlocale(LC_NUMERIC, "");

	sprintf(buffer, "%d", value);

	setlocale(LC_NUMERIC, "C");

	ro_gui_set_icon_string(w, i, buffer, true);
}


/**
 * Set the contents of an icon to a number.
 *
 * \param  w	  window handle
 * \param  i	  icon handle
 * \param  value  value
 */
void ro_gui_set_icon_decimal(wimp_w w, wimp_i i, int value, int decimal_places)
{
	char buffer[20]; // Big enough for 64-bit int

	setlocale(LC_NUMERIC, "");

	switch (decimal_places) {
		case 0:
			sprintf(buffer, "%d", value);
			break;
		case 1:
			sprintf(buffer, "%.1f", (float)value / 10);
			break;
		case 2:
			sprintf(buffer, "%.2f", (float)value / 100);
			break;
		default:
			assert(!"Unsupported decimal format");
			break;
	}

	setlocale(LC_NUMERIC, "C");

	ro_gui_set_icon_string(w, i, buffer, true);
}


/**
 * Get the contents of an icon as a number.
 *
 * \param  w	  window handle
 * \param  i	  icon handle
 * \param  value  value
 */
int ro_gui_get_icon_decimal(wimp_w w, wimp_i i, int decimal_places)
{
	double value;
	int multiple = 1;

	for (; decimal_places > 0; decimal_places--)
		multiple *= 10;

	setlocale(LC_NUMERIC, "");

	value = atof(ro_gui_get_icon_string(w, i)) * multiple;

	setlocale(LC_NUMERIC, "C");

	return (int)value;
}


/**
 * Set the selected state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  state selected state
 */
void ro_gui_set_icon_selected_state(wimp_w w, wimp_i i, bool state)
{
	os_error *error;
	if (ro_gui_get_icon_selected_state(w, i) == state) return;
	error = xwimp_set_icon_state(w, i,
			(state ? wimp_ICON_SELECTED : 0), wimp_ICON_SELECTED);
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}

/**
 * Gets the selected state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 */
bool ro_gui_get_icon_selected_state(wimp_w w, wimp_i i)
{
	os_error *error;
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	return ((ic.icon.flags & wimp_ICON_SELECTED) != 0);
}


/**
 * Set the shaded state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  state shaded state
 */
void ro_gui_set_icon_shaded_state(wimp_w w, wimp_i i, bool state)
{
	wimp_caret caret;
	os_error *error;

	/* update the state */
	if (ro_gui_get_icon_shaded_state(w, i) == state)
		return;
	error = xwimp_set_icon_state(w, i,
			(state ? wimp_ICON_SHADED : 0), wimp_ICON_SHADED);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	if (!state)
		return;

	/* ensure the caret is not in a shaded icon */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if ((caret.w != w) || (caret.i != i))
		return;
	/* move the caret to the first avaiable writable */
	if (ro_gui_set_caret_first(w))
		return;
	/* lose the caret */
	error = xwimp_set_caret_position((wimp_w)-1, (wimp_i)-1, -1, -1, -1, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Gets the shaded state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 */
bool ro_gui_get_icon_shaded_state(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_SHADED) != 0;
}


/**
 * Set the deleted state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 * \param  state shaded state
 */
void ro_gui_set_icon_deleted_state(wimp_w w, wimp_i i, bool state)
{
	wimp_caret caret;
	os_error *error;

	/* update the state */
	if (ro_gui_get_icon_deleted_state(w, i) == state)
		return;
	error = xwimp_set_icon_state(w, i,
			(state ? wimp_ICON_DELETED : 0), wimp_ICON_DELETED);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
	if (!state)
		return;

	/* ensure the caret is not in a shaded icon */
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	if ((caret.w != w) || (caret.i != i))
		return;
	/* move the caret to the first avaiable writable */
	if (ro_gui_set_caret_first(w))
		return;
	/* lose the caret */
	error = xwimp_set_caret_position((wimp_w)-1, (wimp_i)-1, -1, -1, -1, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Gets the deleted state of an icon.
 *
 * \param  w	 window handle
 * \param  i	 icon handle
 */
bool ro_gui_get_icon_deleted_state(wimp_w w, wimp_i i)
{
	wimp_icon_state ic;
	ic.w = w;
	ic.i = i;
	xwimp_get_icon_state(&ic);
	return (ic.icon.flags & wimp_ICON_DELETED) != 0;
}


/**
 * Set the button type of an icon.
 *
 * \param  w	window handle
 * \param  i	icon handle
 * \param  type button type
 */
void ro_gui_set_icon_button_type(wimp_w w, wimp_i i, int type)
{
	os_error *error;
	error = xwimp_set_icon_state(w, i, wimp_ICON_BUTTON_TYPE,
			(type << wimp_ICON_BUTTON_TYPE_SHIFT));
	if (error) {
		LOG(("xwimp_set_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Set an icon's sprite
 *
 * \param  w	window handle
 * \param  i	icon handle
 * \param  area sprite area containing sprite
 * \param  name name of sprite in area (in local encoding)
 */
void ro_gui_set_icon_sprite(wimp_w w, wimp_i i, osspriteop_area *area,
		const char *name)
{
	wimp_icon_state ic;
	os_error *error;

	/* get the icon data */
	ic.w = w;
	ic.i = i;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* copy the name across */
	if (ic.icon.data.indirected_text.size) {
		strncpy(ic.icon.data.indirected_text.text, name,
			(unsigned int)ic.icon.data.indirected_text.size - 1);
		ic.icon.data.indirected_text.text[
				ic.icon.data.indirected_text.size - 1] = '\0';
	}

	ic.icon.data.indirected_sprite.area = area;

	ro_gui_redraw_icon(w, i);
}


/**
 * Set a window title
 *
 * \param  w	 window handle
 * \param  text  new title (copied)
 */
void ro_gui_set_window_title(wimp_w w, const char *text)
{
	wimp_window_info_base window;
	os_error *error;
	char *title_local_enc;
	nserror err;

	/*	Get the window details
	*/
	window.w = w;
	error = xwimp_get_window_info_header_only((wimp_window_info *)&window);
	if (error) {
		LOG(("xwimp_get_window_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* convert text to local encoding */
	err = utf8_to_local_encoding(text, 0, &title_local_enc);
	if (err != NSERROR_OK) {
		/* A bad encoding should never happen,
		 * so assert this */
		assert(err != NSERROR_BAD_ENCODING);
		LOG(("utf8_to_enc failed"));
		return;
	}

	/*	Set the title string
	*/
	strncpy(window.title_data.indirected_text.text, title_local_enc,
			(unsigned int)window.title_data.indirected_text.size
					- 1);
	window.title_data.indirected_text.text[
			window.title_data.indirected_text.size - 1] = '\0';

	/*	Redraw accordingly
	*/
	error = xwimp_force_redraw_title(w);
	if (error) {
		LOG(("xwimp_force_redraw_title: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	free(title_local_enc);
}


/**
 * Places the caret in the first available icon
 *
 * \w	the window to place the caret in
 * \return true if the caret was placed, false otherwise
 */
bool ro_gui_set_caret_first(wimp_w w)
{
	int icon, b;
	wimp_window_state win_state;
	wimp_window_info_base window;
	wimp_icon_state state;
	os_error *error;

	/* check the window is open */
	win_state.w = w;
	error = xwimp_get_window_state(&win_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	if (!(win_state.flags & wimp_WINDOW_OPEN))
		return false;

	/* get the window details for the icon count */
	window.w = w;
	error = xwimp_get_window_info_header_only((wimp_window_info *)&window);
	if (error) {
		LOG(("xwimp_get_window_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* work through all the icons */
	state.w = w;
	for (icon = 0; icon < window.icon_count; icon++) {
		state.i = icon;
		error = xwimp_get_icon_state(&state);
		if (error) {
			LOG(("xwimp_get_icon_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		/* ignore if it's shaded or not writable */
		if (state.icon.flags & wimp_ICON_SHADED)
			continue;
		b = (state.icon.flags >> wimp_ICON_BUTTON_TYPE_SHIFT) & 0xf;
		if ((b != wimp_BUTTON_WRITE_CLICK_DRAG) &&
				(b != wimp_BUTTON_WRITABLE))
			continue;

		/* move the caret */
		error = xwimp_set_caret_position(w, icon, 0, 0, -1,
				strlen(state.icon.data.indirected_text.text));
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		return true;
	}
	return false;
}


/**
 * Load a sprite file into memory.
 *
 * \param  pathname  file to load
 * \return  sprite area, or 0 on memory exhaustion or error and error reported
 */

osspriteop_area *ro_gui_load_sprite_file(const char *pathname)
{
	int len;
	fileswitch_object_type obj_type;
	osspriteop_area *area;
	os_error *error;

	error = xosfile_read_stamped_no_path(pathname,
			&obj_type, 0, 0, &len, 0, 0);
	if (error) {
		LOG(("xosfile_read_stamped_no_path: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return 0;
	}
	if (obj_type != fileswitch_IS_FILE) {
		warn_user("FileError", pathname);
		return 0;
	}

	area = malloc(len + 4);
	if (!area) {
		warn_user("NoMemory", 0);
		return 0;
	}

	area->size = len + 4;
	area->sprite_count = 0;
	area->first = 16;
	area->used = 16;

	error = xosspriteop_load_sprite_file(osspriteop_USER_AREA,
			area, pathname);
	if (error) {
		LOG(("xosspriteop_load_sprite_file: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		free(area);
		return 0;
	}

	return area;
}


/**
 * Check if a sprite is present in the Wimp sprite pool.
 *
 * \param  sprite  name of sprite
 * \return  true if the sprite is present
 */

bool ro_gui_wimp_sprite_exists(const char *sprite)
{
	static char last_sprite_found[16];
	os_error *error;

	/* make repeated calls fast */
	if (!strncmp(sprite, last_sprite_found, sizeof(last_sprite_found)))
		return true;

	/* fallback if not known to exist */
	error = xwimpspriteop_select_sprite(sprite, 0);
	if (error) {
		if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
			LOG(("xwimpspriteop_select_sprite: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("MiscError", error->errmess);
		}
		return false;
	}
  	snprintf(last_sprite_found, sizeof(last_sprite_found), sprite);
	return true;
}


/**
 * Locate a sprite in the Wimp sprite pool, returning a pointer to it.
 *
 * \param  name	   sprite name
 * \param  sprite  receives pointer to sprite if found
 * \return error ptr iff not found
 */

os_error *ro_gui_wimp_get_sprite(const char *name, osspriteop_header **sprite)
{
	osspriteop_area *rom_base, *ram_base;
	os_error *error;

	error = xwimp_base_of_sprites(&rom_base, &ram_base);
	if (error) return error;

	error = xosspriteop_select_sprite(osspriteop_USER_AREA,
			ram_base, (osspriteop_id)name, sprite);

	if (error && error->errnum == error_SPRITE_OP_DOESNT_EXIST)
		error = xosspriteop_select_sprite(osspriteop_USER_AREA,
				rom_base, (osspriteop_id)name, sprite);

	return error;
}


/**
 * Get the dimensions of a sprite
 *
 * \param *area			The sprite area to use.
 * \param *sprite		Pointer to the sprite name.
 * \param *width		Return the sprite width.
 * \param *height		Return the sprite height.
 * \return			true if successful; else false.
 */

bool ro_gui_wimp_get_sprite_dimensions(osspriteop_area *area, char *sprite,
		int *width, int *height)
{
	os_error			*error = NULL;
	os_mode				mode;
	os_coord			dimensions;

	dimensions.x = 0;
	dimensions.y = 0;

	if (area != (osspriteop_area *) -1)
		error = xosspriteop_read_sprite_info(osspriteop_USER_AREA,
				area, (osspriteop_id) sprite,
				&dimensions.x, &dimensions.y, 0, &mode);

	if (error != NULL || area == (osspriteop_area *) -1)
		error = xwimpspriteop_read_sprite_info(sprite,
				&dimensions.x, &dimensions.y, 0, &mode);

	if (error == NULL) {
		ro_convert_pixels_to_os_units(&dimensions, mode);
		if (width != NULL)
			*width = dimensions.x;
		if (height != NULL)
			*height = dimensions.y;
	} else if (error->errnum != error_SPRITE_OP_DOESNT_EXIST) {
		LOG(("xosspriteop_read_sprite_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("MiscError", error->errmess);
		return false;
	}

	return true;
}


/**
 * Performs simple user redraw for a window.
 *
 * \param  user_fill	whether to fill the redraw area
 * \param  user_colour  the colour to use when filling
 */

void ro_gui_user_redraw(wimp_draw *redraw, bool user_fill,
		os_colour user_colour)
{
	os_error *error;
	osbool more;

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		if (user_fill) {
			error = xcolourtrans_set_gcol(user_colour,
					colourtrans_SET_BG_GCOL,
					os_ACTION_OVERWRITE, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_gcol: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("MiscError", error->errmess);
			}
			os_clg();
		}
		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Sets whether a piece of window furniture is present for a window.
 *
 * \param  w	     the window to modify
 * \param  bic_mask  the furniture flags to clear
 * \param  xor_mask  the furniture flags to toggle
 */
void ro_gui_wimp_update_window_furniture(wimp_w w, wimp_window_flags bic_mask,
		wimp_window_flags xor_mask)
{
	wimp_window_state state;
	wimp_w parent;
	bits linkage;
	os_error *error;
	bool open;

	state.w = w;
	error = xwimp_get_window_state_and_nesting(&state, &parent, &linkage);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	open = state.flags & wimp_WINDOW_OPEN;
	state.flags &= ~(63 << 16); /* clear bits 16-21 */
	state.flags &= ~bic_mask;
	state.flags ^= xor_mask;
	if (!open)
		state.next = wimp_HIDDEN;
	error = xwimp_open_window_nested_with_flags(&state, parent, linkage);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!open) {
		error = xwimp_close_window(w);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}


/**
 * Checks whether a piece of window furniture is present for a window.
 *
 * \param  w	     the window to modify
 * \param  mask	     the furniture flags to check
 */
bool ro_gui_wimp_check_window_furniture(wimp_w w, wimp_window_flags mask)
{
	wimp_window_state state;
	os_error *error;

	state.w = w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	return state.flags & mask;
}

/**
 * RO GUI-specific strlen, for control character terminated strings
 *
 * \param str  The string to measure the length of
 * \return The length of the string
 */
size_t ro_gui_strlen(const char *str)
{
	const char *str_begin;

	if (str == NULL)
		return 0;

	for (str_begin = str; *str++ >= ' '; /* */)
		/* */;

	return str - str_begin - 1;
}

/**
 * RO GUI-specific strncmp, for control character terminated strings
 *
 * \param s1 The first string for comparison
 * \param s2 The second string for comparison
 * \param len Maximum number of bytes to be checked
 * \return 0 for equal strings up to len bytes; pos for s1 being bigger than
 * s2; neg for s1 being smaller than s2.
 */
int ro_gui_strncmp(const char *s1, const char *s2, size_t len)
{
	while (len--) {
		char c1 = *s1++;
		char c2 = *s2++;
		if (c1 < ' ' || c2 < ' ')
			return (c1 < ' ' ? 0 : c1) - (c2 < ' ' ? 0 : c2);
		int diff = c1 - c2;
		if (diff)
			return diff;
	}
	return 0;
}


/**
 * Generic window scroll event handler.
 *
 * \param  *scroll		Pointer to Scroll Event block.
 */

void ro_gui_scroll(wimp_scroll *scroll)
{
	os_error	*error;
	int		x = scroll->visible.x1 - scroll->visible.x0 - 32;
	int		y = scroll->visible.y1 - scroll->visible.y0 - 32;

	switch (scroll->xmin) {
	case wimp_SCROLL_PAGE_LEFT:
		scroll->xscroll -= x;
		break;
	case wimp_SCROLL_COLUMN_LEFT:
		scroll->xscroll -= 100;
		break;
	case wimp_SCROLL_COLUMN_RIGHT:
		scroll->xscroll += 100;
		break;
	case wimp_SCROLL_PAGE_RIGHT:
		scroll->xscroll += x;
		break;
	default:
		scroll->xscroll += (x * (scroll->xmin>>2)) >> 2;
		break;
	}

	switch (scroll->ymin) {
	case wimp_SCROLL_PAGE_UP:
		scroll->yscroll += y;
		break;
	case wimp_SCROLL_LINE_UP:
		scroll->yscroll += 100;
		break;
	case wimp_SCROLL_LINE_DOWN:
		scroll->yscroll -= 100;
		break;
	case wimp_SCROLL_PAGE_DOWN:
		scroll->yscroll -= y;
		break;
	default:
		scroll->yscroll += (y * (scroll->ymin>>2)) >> 2;
		break;
	}

	error = xwimp_open_window((wimp_open *) scroll);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
	}
}

