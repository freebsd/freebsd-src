/*
 * Copyright 2004, 2005 Richard Wilson <info@tinct.net>
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
 * Throbber (implementation).
 */

#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "riscos/gui/throbber.h"
#include "riscos/theme.h"
#include "riscos/wimp.h"
#include "utils/log.h"
#include "utils/utils.h"

#define THROBBER_SPRITE_NAME_LENGTH 12
#define THROBBER_ANIMATE_INTERVAL 10

struct throbber {
	/** The applied theme (or NULL to use the default) */
	struct theme_descriptor	*theme;

	/** The widget dimensions. */
	int			x_min, y_min;

	/** The window and icon details. */
	wimp_w			window;
	wimp_i			icon;
	os_box			extent;
	osspriteop_area		*sprites;
	bool			hidden;
	bool			shaded;

	/** The animation details. */
	int			max_frame;
	int			current_frame;
	os_t			last_update;
	char			sprite_name[THROBBER_SPRITE_NAME_LENGTH];
	bool			force_redraw;
};

/*
 * Private function prototypes.
 */

static bool ro_gui_throbber_icon_update(struct throbber *throbber);
static bool ro_gui_throbber_icon_resize(struct throbber *throbber);

/* This is an exported interface documented in throbber.h */

struct throbber *ro_gui_throbber_create(struct theme_descriptor *theme)
{
	struct throbber		*throbber;

	/* Allocate memory. */

	throbber = malloc(sizeof(struct throbber));
	if (throbber == NULL) {
		LOG(("No memory for malloc()"));
		return NULL;
	}

	/* Set up default parameters. If reading the throbber theme data
	 * fails, we give up and return a failure.
	 */

	if (!ro_gui_theme_get_throbber_data(theme, &throbber->max_frame,
			&throbber->x_min, &throbber->y_min, NULL,
			&throbber->force_redraw)) {
		free(throbber);
		return NULL;
	}

	throbber->sprites = ro_gui_theme_get_sprites(theme);

	throbber->theme = theme;

	throbber->extent.x0 = 0;
	throbber->extent.y0 = 0;
	throbber->extent.x1 = 0;
	throbber->extent.y1 = 0;

	throbber->current_frame = 0;
	throbber->last_update = 0;

	throbber->window = NULL;
	throbber->icon = -1;

	throbber->hidden = false;
	throbber->shaded = false;

	return throbber;
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_rebuild(struct throbber *throbber,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool shaded)
{
	if (throbber == NULL)
		return false;

	throbber->theme = theme;
	throbber->window = window;
	throbber->sprites = ro_gui_theme_get_sprites(theme);

	throbber->icon = -1;

	throbber->shaded = shaded;

	strcpy(throbber->sprite_name, "throbber0");

	if (!ro_gui_theme_get_throbber_data(theme, &throbber->max_frame,
			&throbber->x_min, &throbber->y_min, NULL,
			&throbber->force_redraw)) {
		free(throbber);
		return false;
	}

	return ro_gui_throbber_icon_update(throbber);
}


/* This is an exported interface documented in throbber.h */

void ro_gui_throbber_destroy(struct throbber *throbber)
{
	if (throbber == NULL)
		return;

	free(throbber);
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_get_dims(struct throbber *throbber,
		int *width, int *height)
{
	if (throbber == NULL)
		return false;

	if (throbber->x_min != -1 && throbber->y_min != -1) {
		if (width != NULL)
			*width = throbber->x_min;
		if (height != NULL)
			*height = throbber->y_min;

		return true;
	}

	return false;
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_set_extent(struct throbber *throbber,
		int x0, int y0, int x1, int y1)
{
	if (throbber == NULL)
		return false;

	if ((x1 - x0) < throbber->x_min || (y1 - y0) < throbber->y_min)
		return false;

	if (throbber->extent.x0 == x0 && throbber->extent.y0 == y0 &&
			throbber->extent.x1 == x1 &&
			throbber->extent.y1 == y1)
		return true;

	/* Redraw the relevant bits of the toolbar. */

	if (throbber->window != NULL && throbber->icon != -1) {
		xwimp_force_redraw(throbber->window,
				throbber->extent.x0, throbber->extent.y0,
				throbber->extent.x1, throbber->extent.y1);
		xwimp_force_redraw(throbber->window, x0, y0, x1, y1);
	}

	/* Update the throbber position */

	throbber->extent.x0 = x0;
	throbber->extent.y0 = y0;
	throbber->extent.x1 = x1;
	throbber->extent.y1 = y1;

	return ro_gui_throbber_icon_resize(throbber);
}


/**
 * Create or delete a throbber's icon if required to bring it into sync with
 * the current hidden setting.
 *
 * \param *throbber		The throbber to update.
 * \return			true if successful; else false.
 */

bool ro_gui_throbber_icon_update(struct throbber *throbber)
{
	wimp_icon_create	icon;
	os_error		*error;

	if (throbber == NULL || throbber->window == NULL)
		return false;

	if (!throbber->hidden && throbber->icon == -1) {
		icon.w = throbber->window;
		icon.icon.extent.x0 = throbber->extent.x0;
		icon.icon.extent.y0 = throbber->extent.y0;
		icon.icon.extent.x1 = throbber->extent.x1;
		icon.icon.extent.y1 = throbber->extent.y1;
		icon.icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
				wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
		icon.icon.data.indirected_sprite.id =
				(osspriteop_id) throbber->sprite_name;
		icon.icon.data.indirected_sprite.area = throbber->sprites;
		icon.icon.data.indirected_sprite.size =
				THROBBER_SPRITE_NAME_LENGTH;

		error = xwimp_create_icon(&icon, &throbber->icon);
		if (error != NULL) {
			LOG(("xwimp_create_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			throbber->icon = -1;
			return false;
		}

		if (!ro_gui_throbber_icon_resize(throbber))
			return false;
	} else if (throbber->hidden && throbber->icon != -1) {
		error = xwimp_delete_icon(throbber->window, throbber->icon);
		if (error != NULL) {
			LOG(("xwimp_delete_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}

		throbber->icon = -1;
	}

	if (throbber->icon != -1)
		ro_gui_set_icon_shaded_state(throbber->window,
				throbber->icon, throbber->shaded);

	return true;
}


/**
 * Position the icons in the throbber to take account of the currently
 * configured extent.
 *
 * \param *throbber		The throbber to update.
 * \return			true if successful; else false.
 */

bool ro_gui_throbber_icon_resize(struct throbber *throbber)
{
	os_error	*error;

	if (throbber->window == NULL)
		return false;

	if (throbber->icon != -1) {
		error = xwimp_resize_icon(throbber->window, throbber->icon,
				throbber->extent.x0, throbber->extent.y0,
				throbber->extent.x1, throbber->extent.y1);
		if (error != NULL) {
			LOG(("xwimp_resize_icon: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			throbber->icon = -1;
			return false;
		}
	}

	return true;
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_hide(struct throbber *throbber, bool hide)
{
	if (throbber == NULL || throbber->hidden == hide)
		return (throbber == NULL) ? false : true;

	throbber->hidden = hide;

	return ro_gui_throbber_icon_update(throbber);
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_help_suffix(struct throbber *throbber, wimp_i i,
		os_coord *mouse, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix)
{
	os_coord			pos;

	if (throbber == NULL || throbber->hidden)
		return false;

	/* Check that the click was within our part of the window. */

	pos.x = mouse->x - state->visible.x0 + state->xscroll;
	pos.y = mouse->y - state->visible.y1 + state->yscroll;

	if (pos.x < throbber->extent.x0 || pos.x > throbber->extent.x1 ||
			pos.y < throbber->extent.y0 ||
			pos.y > throbber->extent.y1)
		return false;

	/* Return a hard-coded icon number that matches the one that was
	 * always allocated to the throbber in a previous implementation.
	 * If Messages can be updated, this could be changed.
	 */

	if (i == throbber->icon)
		*suffix = "16";
	else
		*suffix = "";

	return true;
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_animate(struct throbber *throbber)
{
	os_t	t;
	char	sprite_name[THROBBER_SPRITE_NAME_LENGTH];

	if (throbber == NULL || throbber->hidden)
		return (throbber == NULL) ? false : true;

	xos_read_monotonic_time(&t);

	/* Drop out if we're not ready for the next frame, unless this
	 * call is to start animation from a stopped throbber (ie. if
	 * the current frame is 0).
	 */

	if ((t < (throbber->last_update + THROBBER_ANIMATE_INTERVAL)) &&
			(throbber->current_frame > 0))
		return true;

	throbber->last_update = t;
	throbber->current_frame++;

	if (throbber->current_frame > throbber->max_frame)
		throbber->current_frame = 1;

	snprintf(sprite_name, THROBBER_SPRITE_NAME_LENGTH,
			"throbber%i", throbber->current_frame);
	ro_gui_set_icon_string(throbber->window, throbber->icon,
			sprite_name, true);

	if (throbber->force_redraw)
		ro_gui_force_redraw_icon(throbber->window, throbber->icon);

	return true;
}


/* This is an exported interface documented in throbber.h */

bool ro_gui_throbber_stop(struct throbber *throbber)
{
	char	sprite_name[THROBBER_SPRITE_NAME_LENGTH];

	if (throbber == NULL || throbber->hidden ||
			throbber->current_frame == 0)
		return (throbber == FALSE) ? false : true;

	throbber->current_frame = 0;
	throbber->last_update = 0;

	strcpy(sprite_name, "throbber0");
	ro_gui_set_icon_string(throbber->window, throbber->icon,
			sprite_name, true);

	if (throbber->force_redraw)
		ro_gui_force_redraw_icon(throbber->window, throbber->icon);

	return true;
}

