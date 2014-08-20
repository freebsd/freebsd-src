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
 * Button bars (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/dragasprite.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "riscos/gui/button_bar.h"
#include "riscos/gui.h"
#include "riscos/mouse.h"
#include "riscos/theme.h"
#include "riscos/wimp.h"
#include "utils/log.h"
#include "utils/utils.h"

#define BUTTONBAR_SPRITE_NAME_LENGTH 12
#define BUTTONBAR_VALIDATION_LENGTH 40

struct button_bar_button {
	wimp_i				icon;
	bool				shaded;
	bool				separator;

	button_bar_action		select_action;
	button_bar_action		adjust_action;

	int				x_pos, y_pos;
	int				x_size, y_size;

	char				sprite[BUTTONBAR_SPRITE_NAME_LENGTH];
	char				validation[BUTTONBAR_VALIDATION_LENGTH];
	char				opt_key;
	const char			*help_suffix;

	struct button_bar_button	*bar_next;
	struct button_bar_button	*next;
};


struct button_bar {
	/** The applied theme (or NULL to use the default) */
	struct theme_descriptor		*theme;

	/** The widget dimensions. */
	int				x_min, y_min;
	int				separator_width;
	int				vertical_offset;

	bool				separators;

	/** The window details and bar position. */
	wimp_w				window;
	os_box				extent;
	osspriteop_area			*sprites;
	int				background;

	bool				hidden;

	bool				edit;
	struct button_bar		*edit_target;
	struct button_bar		*edit_source;
	void				(*edit_refresh)(void *);
	void				*edit_client_data;

	/** The list of all the defined buttons. */

	struct button_bar_button	*buttons;

	/** The list of the buttons in the current bar. */

	struct button_bar_button	*bar;
};

static char			null_text_string[] = "";
static char			separator_name[] = "separator";

static struct button_bar	*drag_start = NULL;
static char			drag_opt = '\0';
static bool			drag_separator = false;

/*
 * Private function prototypes.
 */

static bool ro_gui_button_bar_place_buttons(struct button_bar *button_bar);
static bool ro_gui_button_bar_icon_update(struct button_bar *button_bar);
static bool ro_gui_button_bar_icon_resize(struct button_bar *button_bar);
static void ro_gui_button_bar_drag_end(wimp_dragged *drag, void *data);
static void ro_gui_button_bar_sync_editors(struct button_bar *target,
		struct button_bar *source);
static struct button_bar_button *ro_gui_button_bar_find_icon(
		struct button_bar *button_bar, wimp_i icon);
static struct button_bar_button *ro_gui_button_bar_find_opt_key(
		struct button_bar *button_bar, char opt_key);
static struct button_bar_button *ro_gui_button_bar_find_action(
		struct button_bar *button_bar, button_bar_action action);
static struct button_bar_button *ro_gui_button_bar_find_coords(
		struct button_bar *button_bar, os_coord pos,
		bool *separator, bool *right);

/* This is an exported interface documented in button_bar.h */

struct button_bar *ro_gui_button_bar_create(struct theme_descriptor *theme,
		const struct button_bar_buttons buttons[])
{
	struct button_bar		*button_bar;
	struct button_bar_button	*icon, *new_icon;
	bool				failed;
	int				def;

	/* Allocate memory. */

	button_bar = malloc(sizeof(struct button_bar));
	if (button_bar == NULL) {
		LOG(("No memory for malloc()"));
		return NULL;
	}

	/* Set up default parameters. */

	button_bar->theme = theme;
	button_bar->sprites = ro_gui_theme_get_sprites(theme);
	button_bar->background = wimp_COLOUR_VERY_LIGHT_GREY;

	button_bar->x_min = -1;
	button_bar->y_min = -1;
	button_bar->separator_width = 0;
	button_bar->vertical_offset = 0;

	button_bar->separators = false;

	button_bar->window = NULL;

	button_bar->hidden = false;

	button_bar->edit = false;
	button_bar->edit_target = NULL;
	button_bar->edit_source = NULL;
	button_bar->edit_refresh = NULL;
	button_bar->edit_client_data = NULL;

	button_bar->buttons = NULL;

	/* Process the button icon definitions */

	icon = NULL;
	failed = false;

	for (def = 0; buttons[def].icon != NULL; def++) {
		new_icon = malloc(sizeof(struct button_bar_button));
		if (new_icon == NULL) {
			failed = true;
			break;
		}

		if (icon == NULL) {
			button_bar->buttons = new_icon;
			button_bar->bar = new_icon;
		} else {
			icon->next = new_icon;
			icon->bar_next = new_icon;
		}
		icon = new_icon;
		icon->next = NULL;
		icon->bar_next = NULL;

		strncpy(icon->sprite, buttons[def].icon,
				BUTTONBAR_SPRITE_NAME_LENGTH);
		snprintf(icon->validation, BUTTONBAR_VALIDATION_LENGTH,
				"R5;S%s,p%s", icon->sprite, icon->sprite);

		icon->icon = -1;
		icon->shaded = false;
		icon->separator = false;

		icon->select_action = buttons[def].select;
		icon->adjust_action = buttons[def].adjust;
		icon->opt_key = buttons[def].opt_key;
		icon->help_suffix = buttons[def].help;
	}

	/* Add a separator after the last entry.  This will be lost if the
	 * buttons are subsequently set, but is used for the edit source bar.
	 */

	if (icon != NULL)
		icon->separator = true;

	return button_bar;
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_link_editor(struct button_bar *target,
		struct button_bar *source, void (* refresh)(void *),
		void *client_data)
{
	if (target == NULL || source == NULL ||
			target->edit_target != NULL ||
			target->edit_source != NULL ||
			source->edit_target != NULL ||
			source->edit_source != NULL)
		return false;

	target->edit_source = source;
	source->edit_target = target;

	/* Store the callback data in the editor bar. */

	source->edit_refresh = refresh;
	source->edit_client_data = client_data;

	return true;
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_rebuild(struct button_bar *button_bar,
		struct theme_descriptor *theme, theme_style style,
		wimp_w window, bool edit)
{
	struct button_bar_button	*button;
	os_error			*error;
	int				height;


	if (button_bar == NULL)
		return false;

	button_bar->theme = theme;
	button_bar->window = window;
	button_bar->sprites = ro_gui_theme_get_sprites(theme);
	button_bar->background = ro_gui_theme_get_style_element(theme, style,
			THEME_ELEMENT_BACKGROUND);

	button_bar->edit = edit;

	height = 0;
	button_bar->separator_width = 16;
	ro_gui_wimp_get_sprite_dimensions(button_bar->sprites, separator_name,
			&button_bar->separator_width, &height);

	/* If the separator height is 0, then either the sprite really is
	 * zero pixels high or the default was used as no sprite was found.
	 * Either way, we don't have a separator.
	 */

	button_bar->separators = (height == 0) ? false : true;

	button = button_bar->buttons;
	error = NULL;

	while (button != NULL) {
		button->x_size = 0;
		button->y_size = 0;
		button->icon = -1;

		ro_gui_wimp_get_sprite_dimensions(button_bar->sprites,
				button->sprite,
				&button->x_size, &button->y_size);

		button = button->next;
	}

	if (!ro_gui_button_bar_place_buttons(button_bar))
		return false;

	if (button_bar->edit && button_bar->edit_target != NULL)
		ro_gui_button_bar_sync_editors(button_bar->edit_target,
				button_bar);

	return ro_gui_button_bar_icon_update(button_bar);
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_arrange_buttons(struct button_bar *button_bar,
		char order[])
{
	struct button_bar_button	*button, *new;
	int				i;

	if (button_bar == NULL || order == NULL)
		return false;

	/* Delete any existing button arrangement. */

	button_bar->bar = NULL;

	for (button = button_bar->buttons; button != NULL;
			button = button->next) {
		button->bar_next = NULL;
		button->separator = false;
	}

	/* Parse the config string and link up the new buttons. */

	button = NULL;

	for (i = 0; order[i] != '\0'; i++) {
		if (order[i] != '|') {
			new = ro_gui_button_bar_find_opt_key(button_bar,
					order[i]);

			if (new != NULL) {
				if (button == NULL)
					button_bar->bar = new;
				else
					button->bar_next = new;

				button = new;
			}
		} else {
			if (button != NULL)
				button->separator = true;
		}
	}

	if (!ro_gui_button_bar_place_buttons(button_bar))
		return false;

	return ro_gui_button_bar_place_buttons(button_bar);
}

/**
 * Place the buttons on a button bar, taking into account the button arrangement
 * and the current theme, and update the bar extent details.
 *
 * \param *button_bar		The button bar to update.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_place_buttons(struct button_bar *button_bar)
{
	struct button_bar_button	*button;
	int				x_pos, y_pos, height;

	if (button_bar == NULL)
		return false;

	button = button_bar->bar;
	x_pos = 0;
	y_pos = 0;
	height = 0;

	while (button != NULL) {
		button->x_pos = x_pos;
		button->y_pos = y_pos;

		x_pos += button->x_size;
		if (button->separator)
			x_pos += button_bar->separator_width;

		if (button->y_size > height)
			height = button->y_size;

		button = button->bar_next;
	}

	button_bar->x_min = x_pos;
	button_bar->y_min = height;

	return true;
}


/* This is an exported interface documented in button_bar.h */

void ro_gui_button_bar_destroy(struct button_bar *button_bar)
{
	struct button_bar_button	*button;

	if (button_bar == NULL)
		return;

	/* Free the button definitions. */

	while (button_bar->buttons != NULL) {
		button = button_bar->buttons;
		button_bar->buttons = button->next;
		free(button);
	}

	free(button_bar);
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_get_dims(struct button_bar *button_bar,
		int *width, int *height)
{
	if (button_bar == NULL)
		return false;

	if (button_bar->x_min != -1 && button_bar->y_min != -1) {
		if (width != NULL)
			*width = button_bar->x_min;
		if (height != NULL)
			*height = button_bar->y_min;

		return true;
	}

	return false;
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_set_extent(struct button_bar *button_bar,
		int x0, int y0, int x1, int y1)
{
	if (button_bar == NULL)
		return false;

	if ((x1 - x0) < button_bar->x_min || (y1 - y0) < button_bar->y_min)
		return false;

	if (button_bar->extent.x0 == x0 && button_bar->extent.y0 == y0 &&
			button_bar->extent.x1 == x1 &&
			button_bar->extent.y1 == y1)
		return true;

	/* Redraw the relevant bits of the toolbar. We can't optimise for
	 * stretching the X-extent, as this probably means the button
	 * arrangement has changed which necessitates a full redraw anyway.
	 */

	if (button_bar->window != NULL) {
		xwimp_force_redraw(button_bar->window,
				button_bar->extent.x0, button_bar->extent.y0,
				button_bar->extent.x1, button_bar->extent.y1);
		xwimp_force_redraw(button_bar->window, x0, y0, x1, y1);
	}

	button_bar->extent.x0 = x0;
	button_bar->extent.y0 = y0;
	button_bar->extent.x1 = x1;
	button_bar->extent.y1 = y1;

	if ((y1 - y0) > button_bar->y_min)
		button_bar->vertical_offset =
				((y1 - y0) - button_bar->y_min) / 2;
	else
		button_bar->vertical_offset = 0;

	return ro_gui_button_bar_icon_resize(button_bar);
}


/**
 * Update the icons on a button bar, creating or deleting them from the window
 * as necessary.
 */

bool ro_gui_button_bar_icon_update(struct button_bar *button_bar)
{
	wimp_icon_create		icon;
	struct button_bar_button	*button, *b;
	os_error			*error;
	bool				on_bar;


	if (button_bar == NULL || button_bar->window == NULL)
		return (button_bar == NULL) ? false : true;

	button = button_bar->buttons;

	while (button != NULL) {
		on_bar = false;

		/* Check if the icon is currently on the bar. */

		for (b = button_bar->bar; b != NULL; b = b->bar_next) {
			if (b == button) {
				on_bar = true;
				break;
			}
		}

		if (on_bar && !button_bar->hidden && button->icon == -1) {
			icon.w = button_bar->window;
			icon.icon.extent.x0 = 0;
			icon.icon.extent.y0 = 0;
			icon.icon.extent.x1 = 0;
			icon.icon.extent.y1 = 0;
			icon.icon.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
					wimp_ICON_INDIRECTED |
					wimp_ICON_HCENTRED |
					wimp_ICON_VCENTRED |
					(button_bar->background
						<< wimp_ICON_BG_COLOUR_SHIFT);
			icon.icon.data.indirected_text.size = 1;

			/* We don't actually shade buttons unless there's no
			 * editor active or this is the source bar.
			 */

			if (button->shaded && (!button_bar->edit ||
					button_bar->edit_target != NULL))
				icon.icon.flags |= wimp_ICON_SHADED;

			if (button_bar->edit)
				icon.icon.flags |= (wimp_BUTTON_CLICK_DRAG <<
						wimp_ICON_BUTTON_TYPE_SHIFT);
			else
				icon.icon.flags |= (wimp_BUTTON_CLICK <<
						wimp_ICON_BUTTON_TYPE_SHIFT);

			icon.icon.data.indirected_text.text = null_text_string;
			icon.icon.data.indirected_text.validation =
					button->validation;

			error = xwimp_create_icon(&icon, &button->icon);
			if (error) {
				LOG(("xwimp_create_icon: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				button->icon = -1;
				return false;
			}
		} else if ((!on_bar || button_bar->hidden)
				&& button->icon != -1) {
			error = xwimp_delete_icon(button_bar->window,
					button->icon);
			if (error != NULL) {
				LOG(("xwimp_delete_icon: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return false;
			}

			button->icon = -1;
		}

		button = button->next;
	}

	return ro_gui_button_bar_icon_resize(button_bar);
}


/**
 * Position the icons in the button bar to take account of the currently
 * configured extent.
 *
 * \param *button_bar		The button bar to update.
 * \return			true if successful; else false.
 */

bool ro_gui_button_bar_icon_resize(struct button_bar *button_bar)
{
	os_error			*error;
	struct button_bar_button	*button;

	if (button_bar == NULL || button_bar->hidden)
		return (button_bar == NULL) ? false : true;

	/* Reposition all the icons. */

	button = button_bar->bar;

	while (button != NULL) {
		if(button->icon != -1) {
			error = xwimp_resize_icon(button_bar->window,
					button->icon,
					button_bar->extent.x0 + button->x_pos,
					button_bar->extent.y0 +
						button_bar->vertical_offset +
						button->y_pos,
					button_bar->extent.x0 + button->x_pos +
						button->x_size,
					button_bar->extent.y0 +
						button_bar->vertical_offset +
						button->y_pos +
						button->y_size);
			if (error != NULL) {
				LOG(("xwimp_resize_icon: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				button->icon = -1;
				return false;
			}
		}

		button = button->bar_next;
	}

	return true;
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_hide(struct button_bar *button_bar, bool hide)
{
	if (button_bar == NULL || button_bar->hidden == hide)
		return (button_bar == NULL) ? false : true;

	button_bar->hidden = hide;

	return ro_gui_button_bar_icon_update(button_bar);
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_shade_button(struct button_bar *button_bar,
		button_bar_action action, bool shaded)
{
	struct button_bar_button	*button;

	if (button_bar == NULL)
		return false;

	button = ro_gui_button_bar_find_action(button_bar, action);
	if (button == NULL)
		return false;

	if (button->shaded == shaded)
		return true;

	button->shaded = shaded;

	/* We don't actually shade buttons unless there's no editor active
	 * or this is the source bar.
	 */

	if (button->icon != -1 &&
			(!button_bar->edit || button_bar->edit_target != NULL))
		ro_gui_set_icon_shaded_state(button_bar->window, button->icon,
				shaded);

	return true;
}


/* This is an exported interface documented in button_bar.h */

void ro_gui_button_bar_redraw(struct button_bar *button_bar,
		wimp_draw *redraw)
{
	wimp_icon			icon;
	struct button_bar_button	*button;

	/* Test for a valid button bar, and then check that the redraw box
	 * coincides with the bar's extent.
	 */

	if (button_bar == NULL || button_bar->hidden ||
			(redraw->clip.x0 - (redraw->box.x0 - redraw->xscroll))
					> button_bar->extent.x1 ||
			(redraw->clip.y0 - (redraw->box.y1 - redraw->yscroll))
					> button_bar->extent.y1 ||
			(redraw->clip.x1 - (redraw->box.x0 - redraw->xscroll))
					< button_bar->extent.x0 ||
			(redraw->clip.y1 - (redraw->box.y1 - redraw->yscroll))
					< button_bar->extent.y0 ||
			(!button_bar->edit && !button_bar->separators))
		return;

	icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
			wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
	if (button_bar->edit)
		icon.flags |= wimp_ICON_BORDER | wimp_COLOUR_DARK_GREY <<
				wimp_ICON_FG_COLOUR_SHIFT;
	if (!button_bar->separators)
		icon.flags |= wimp_ICON_FILLED | wimp_COLOUR_LIGHT_GREY <<
				wimp_ICON_BG_COLOUR_SHIFT;
	icon.data.indirected_sprite.id = (osspriteop_id) separator_name;
	icon.data.indirected_sprite.area = button_bar->sprites;
	icon.data.indirected_sprite.size = 12;
	icon.extent.y0 = button_bar->extent.y0 + button_bar->vertical_offset;
	icon.extent.y1 = icon.extent.y0 + button_bar->y_min;

	for (button = button_bar->bar; button != NULL;
			button = button->bar_next) {
		if (button->separator) {
			icon.extent.x0 = button_bar->extent.x0 +
					button->x_pos + button->x_size;
			icon.extent.x1 = icon.extent.x0 +
					button_bar->separator_width;
			xwimp_plot_icon(&icon);
		}
	}
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_click(struct button_bar *button_bar,
		wimp_pointer *pointer, wimp_window_state *state,
		button_bar_action *action)
{
	struct button_bar_button	*button;
	os_coord			pos;
	os_box				box;
	os_error			*error;
	char				*sprite;

	if (button_bar == NULL || button_bar->hidden)
		return false;

	/* Check that the click was within our part of the window. */

	pos.x = pointer->pos.x - state->visible.x0 + state->xscroll;
	pos.y = pointer->pos.y - state->visible.y1 + state->yscroll;

	if (pos.x < button_bar->extent.x0 || pos.x > button_bar->extent.x1 ||
			pos.y < button_bar->extent.y0 ||
			pos.y > button_bar->extent.y1)
		return false;

	if (button_bar->edit && pointer->buttons == wimp_DRAG_SELECT) {
		/* This is an editor click, so we need to check for drags on
		 * icons (buttons) and work area (separators).
		 */

		button = ro_gui_button_bar_find_coords(button_bar, pos,
				&drag_separator, NULL);

		if (button != NULL && (!button->shaded || drag_separator ||
				button_bar->edit_source != NULL)) {

			drag_start = button_bar;
			drag_opt = button->opt_key;

			if (drag_separator) {
				box.x0 = pointer->pos.x -
						button_bar->separator_width / 2;
				box.x1 = box.x0 + button_bar->separator_width;
				sprite = separator_name;
			} else {
				box.x0 = pointer->pos.x - button->x_size / 2;
				box.x1 = box.x0 + button->x_size;
				sprite = button->sprite;
			}

			box.y0 = pointer->pos.y - button->y_size / 2;
			box.y1 = box.y0 + button->y_size;

			error = xdragasprite_start(dragasprite_HPOS_CENTRE |
					dragasprite_VPOS_CENTRE |
					dragasprite_BOUND_SPRITE |
					dragasprite_BOUND_TO_WINDOW |
					dragasprite_DROP_SHADOW,
					button_bar->sprites,
					sprite, &box, NULL);
			if (error)
				LOG(("xdragasprite_start: 0x%x: %s",
						error->errnum, error->errmess));

			ro_mouse_drag_start(ro_gui_button_bar_drag_end,
					NULL, NULL, NULL);


			return true;
		}

	} else if (!button_bar->edit && pointer->i != -1 &&
			(pointer->buttons == wimp_CLICK_SELECT ||
			pointer->buttons == wimp_CLICK_ADJUST)) {
		/* This isn't an editor click, so we're only interested in
		 * Select or Adjust clicks that occur on physical icons.
		 */

		button = ro_gui_button_bar_find_icon(button_bar, pointer->i);

		if (button != NULL) {
			if (action != NULL) {
				switch (pointer->buttons) {
				case wimp_CLICK_SELECT:
					*action = button->select_action;
					break;
				case wimp_CLICK_ADJUST:
					*action = button->adjust_action;
					break;
				default:
					break;
				}
			}
			return true;
		}
	}

	return false;
}


/* This is an exported interface documented in button_bar.h */

bool ro_gui_button_bar_help_suffix(struct button_bar *button_bar, wimp_i i,
		os_coord *mouse, wimp_window_state *state,
		wimp_mouse_state buttons, const char **suffix)
{
	os_coord			pos;
	struct button_bar_button	*button;

	if (button_bar == NULL || button_bar->hidden)
		return false;

	/* Check that the click was within our part of the window. */

	pos.x = mouse->x - state->visible.x0 + state->xscroll;
	pos.y = mouse->y - state->visible.y1 + state->yscroll;

	if (pos.x < button_bar->extent.x0 || pos.x > button_bar->extent.x1 ||
			pos.y < button_bar->extent.y0 ||
			pos.y > button_bar->extent.y1)
		return false;

	/* Look up and return the help suffix assocuated with the button. */

	button = ro_gui_button_bar_find_icon(button_bar, i);

	if (button != NULL)
		*suffix = button->help_suffix;
	else
		*suffix = "";

	return true;
}


/**
 * Terminate a drag event that was initiated by a button bar.
 *
 * \param *drag			The drag event data.
 * \param *data			NULL data to satisfy callback syntax.
 */

void ro_gui_button_bar_drag_end(wimp_dragged *drag, void *data)
{
	struct button_bar		*drag_end = NULL;
	struct button_bar		*source = NULL, *target = NULL;
	struct button_bar_button	*button, *drop, *previous;
	bool				right, separator;
	wimp_window_state		state;
	wimp_pointer			pointer;
	os_coord			pos;
	os_error			*error;

	xdragasprite_stop();

	if (drag_start == NULL)
		return;

	/* Sort out the window coordinates of the drag end. */

	error = xwimp_get_pointer_info(&pointer);
	if (error) {
		LOG(("xwimp_get_pointer_info: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	assert(pointer.w = drag_start->window);

	state.w = drag_start->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	pos.x = pointer.pos.x - state.visible.x0 + state.xscroll;
	pos.y = pointer.pos.y - state.visible.y1 + state.yscroll;

	/* Work out the destination bar, and establish source and target. */

	if (drag_start->edit_target != NULL) {
		source = drag_start;
		target = drag_start->edit_target;
		if (pos.x >= target->extent.x0 && pos.x <= target->extent.x1 &&
				pos.y >= target->extent.y0 &&
				pos.y <= target->extent.y1)
			drag_end = target;
	} else if (drag_start->edit_source != NULL) {
		source = drag_start->edit_source;
		target = drag_start;
		if (pos.x >= target->extent.x0 && pos.x <= target->extent.x1 &&
				pos.y >= target->extent.y0 &&
				pos.y <= target->extent.y1)
			drag_end = target;
		/* drag_end == source and drag_end == NULL are both equivalent
		 * as far as the following code are concerned, and we don't need
		 * to identify either case. */
	}

	button = ro_gui_button_bar_find_opt_key(target, drag_opt);
	assert(button != NULL);

	/* The drag finished in the target bar, so find out where. */

	if (drag_end == target) {
		drop = ro_gui_button_bar_find_coords(target, pos,
				&separator, &right);
	} else {
		drop = NULL;
	}

	/* If the button is dropped on itself, there's no change and it's
	 * less messy to get out now.
	 */

	if (drag_start == target && drag_end == target && button == drop) {
		drag_start = NULL;
		return;
	}

	/* The drag started in the target bar, so remove the dragged button. */

	if (drag_start == target) {
		if (drag_separator) {
			button->separator = false;
		} else if (target->bar == button) {
			target->bar = button->bar_next;
		} else {
			for (previous = target->bar; previous != NULL &&
					previous->bar_next != button;
					previous = previous->bar_next);
			assert(previous != NULL);
			previous->bar_next = button->bar_next;
			if (button->separator)			// ??
				previous->separator = true;	// ??
		}
	}

	/* The drag ended in the target bar, so add the dragged button in. */

	if (drop != NULL) {
		if (right) {
			if (drag_separator) {
				drop->separator = true;
			} else {
				button->bar_next = drop->bar_next;
				drop->bar_next = button;
				if (drop->separator && !separator) {
					drop->separator = false;
					button->separator = true;
				} else {
					button->separator = false;
				}
			}
		} else if (target->bar == drop && !drag_separator) {
			button->separator = false;
			button->bar_next = target->bar;
			target->bar = button;
		} else if (target->bar != drop) {
			for (previous = target->bar; previous != NULL &&
					previous->bar_next != drop;
					previous = previous->bar_next);
			assert(previous != NULL);

			if (drag_separator) {
				previous->separator = true;
			} else {
				if (separator) {
					previous->separator = false;
					button->separator = true;
				} else {
					button->separator = false;
				}
				button->bar_next = previous->bar_next;
				previous->bar_next = button;
			}
		}
	}

	/* Reposition the buttons and force our client to update. */

	ro_gui_button_bar_place_buttons(target);
	ro_gui_button_bar_icon_update(target);
	ro_gui_button_bar_sync_editors(target, source);

	xwimp_force_redraw(target->window,
			target->extent.x0, target->extent.y0,
			target->extent.x1, target->extent.y1);

	if (source->edit_refresh != NULL)
		source->edit_refresh(source->edit_client_data);

	drag_start = NULL;
}


/**
 * Synchronise the shading of a button bar editor source bar with the currently
 * defined buttons in its target bar.
 *
 * \param *target		The editor target bar.
 * \param *source		The editor source bar.
 */

void ro_gui_button_bar_sync_editors(struct button_bar *target,
		struct button_bar *source)
{
	struct button_bar_button	*sb, *tb;

	if (source == NULL || target == NULL)
		return;

	/* Unshade all of the buttons in the source bar. */

	for (sb = source->bar; sb != NULL; sb = sb->bar_next)
		sb->shaded = false;

	/* Step through the target bar and shade each corresponding
	 * button in the source.
	 */

	for (tb = target->bar; tb != NULL; tb = tb->bar_next) {
		sb = ro_gui_button_bar_find_opt_key(source, tb->opt_key);

		if (sb != NULL)
			sb->shaded = true;
	}

	/* Phyically shade the necessary buttons in the toolbar. */

	for (sb = source->bar; sb != NULL; sb = sb->bar_next)
		if (sb->icon != -1)
			ro_gui_set_icon_shaded_state(source->window, sb->icon,
					sb->shaded);
}


/* This is an exported interface documented in button_bar.h */

char *ro_gui_button_bar_get_config(struct button_bar *button_bar)
{
	struct button_bar_button	*button;
	size_t				size;
	char				*config;
	int				i;

	if (button_bar == NULL)
		return NULL;

	for (size = 1, button = button_bar->bar; button != NULL;
			button = button->bar_next) {
		size++;
		if (button->separator)
			size++;
	}

	config = malloc(size);
	if (config == NULL) {
		LOG(("No memory for malloc()"));
		warn_user("NoMemory", 0);
		return NULL;
	}

	for (i = 0, button = button_bar->bar; button != NULL;
			button = button->bar_next) {
		config[i++] = button->opt_key;
		if (button->separator)
			config[i++] = '|';
	}

	config[i] = '\0';

	return config;
}


/**
 * Find a button bar icon definition from an icon handle.
 *
 * \param *button_bar		The button bar to use.
 * \param icon			The icon handle.
 * \return			Pointer to the button bar icon, or NULL.
 */

struct button_bar_button *ro_gui_button_bar_find_icon(
		struct button_bar *button_bar, wimp_i icon)
{
	struct button_bar_button	*button;

	if (button_bar == NULL || icon == -1)
		return NULL;

	button = button_bar->buttons;

	while (button != NULL && button->icon != icon)
		button = button->next;

	return button;
}


/**
 * Find a button bar icon definition from an options key code.
 *
 * \param *button_bar		The button bar to use.
 * \param opt_key		The option key character code.
 * \return			Pointer to the button bar icon, or NULL.
 */

struct button_bar_button *ro_gui_button_bar_find_opt_key(
		struct button_bar *button_bar, char opt_key)
{
	struct button_bar_button	*button;

	if (button_bar == NULL)
		return NULL;

	button = button_bar->buttons;

	while (button != NULL && button->opt_key != opt_key)
		button = button->next;

	return button;
}


/**
 * Find a button bar icon definition from an action code.
 *
 * \param *button_bar		The button bar to use.
 * \param action		The button action to find.
 * \return			Pointer to the button bar icon, or NULL.
 */

struct button_bar_button *ro_gui_button_bar_find_action(
		struct button_bar *button_bar, button_bar_action action)
{
	struct button_bar_button	*button;

	if (button_bar == NULL)
		return NULL;

	button = button_bar->buttons;

	while (button != NULL &&
			button->select_action != action &&
			button->adjust_action != action)
		button = button->next;

	return button;
}


/**
 * Find a button bar icon definition from coordinates.
 *
 * \param *button_bar		The button bar to use.
 * \param pos			The coordinates to find, work area relative.
 * \param *separator		Returns true if the associated separator was
 *				matched; else false.
 * \param *right		Returns true if the coordinates were in the
 *				right hand side of the target; else false.
 * \return			Pointer to the button bar icon, or NULL.
 */

struct button_bar_button *ro_gui_button_bar_find_coords(
		struct button_bar *button_bar, os_coord pos,
		bool *separator, bool *right)
{
	struct button_bar_button	*button;
	int				x0, y0, x1, y1;

	if (button_bar == NULL)
		return NULL;

	button = button_bar->bar;

	while (button != NULL) {
		/* Match button extents. */

		x0 = button_bar->extent.x0 + button->x_pos;
		y0 = button_bar->extent.y0 + button->y_pos;
		x1 = x0 + button->x_size;
		y1 = y0 + button->y_size;

		if (pos.x > x0 && pos.y > y0 && pos.x < x1 && pos.y < y1) {
			if (separator != NULL)
				*separator = false;

			if (right != NULL)
				*right = (pos.x > x0 + button->x_size/2) ?
						true : false;
			return button;
		}

		x0 = x1;
		x1 = x0 + button_bar->separator_width;

		/* Match separator extents. */

		if (pos.x > x0 && pos.y > y0 && pos.x < x1 && pos.y < y1 &&
				button->separator) {
			if (separator != NULL)
				*separator = true;

			if (right != NULL)
				*right = (x0 + button_bar->separator_width/2) ?
						true : false;
			return button;
		}

		button = button->bar_next;
	}

	return NULL;
}

