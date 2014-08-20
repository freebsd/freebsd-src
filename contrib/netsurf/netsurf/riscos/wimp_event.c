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
 * Automated RISC OS WIMP event handling (implementation).
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "oslib/os.h"
#include "oslib/osbyte.h"
#include "oslib/serviceinternational.h"
#include "oslib/wimp.h"
#include "desktop/gui.h"
#include "riscos/gui.h"
#include "riscos/dialog.h"
#include "riscos/menus.h"
#include "riscos/ucstables.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/utils.h"

#define WIN_HASH_SIZE 32
#define WIN_HASH(w) (((unsigned)(w) >> 5) % WIN_HASH_SIZE)

typedef enum {
	EVENT_NUMERIC_FIELD,
	EVENT_TEXT_FIELD,
	EVENT_UP_ARROW,
	EVENT_DOWN_ARROW,
	EVENT_MENU_GRIGHT,
	EVENT_CHECKBOX,
	EVENT_RADIO,
	EVENT_BUTTON,
	EVENT_CANCEL,
	EVENT_OK
} event_type;

struct event_data_numeric_field {
	int stepping;
	int min;
	int max;
	int decimal_places;
};

struct event_data_menu_gright {
	wimp_i field;
	wimp_menu *menu;
};

struct icon_event {
	event_type type;
	wimp_i i;
	union {
		struct event_data_numeric_field numeric_field;
		struct event_data_menu_gright menu_gright;
		wimp_i linked_icon;
		int radio_group;
		void (*callback)(wimp_pointer *pointer);
	} data;
	union {
		char *textual;
		bool boolean;
	} previous_value;
	bool previous_shaded;
	struct icon_event *next;
};

struct event_window {
	wimp_w w;
	bool (*ok_click)(wimp_w w);
	bool (*mouse_click)(wimp_pointer *pointer);
	bool (*keypress)(wimp_key *key);
	void (*open_window)(wimp_open *open);
	void (*close_window)(wimp_w w);
	void (*redraw_window)(wimp_draw *redraw);
	void (*scroll_window)(wimp_scroll *scroll);
	void (*entering_window)(wimp_entering *entering);
	bool (*menu_prepare)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_pointer *p);
	bool (*menu_selection)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_selection *s, menu_action a);
	void (*menu_warning)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_selection *s, menu_action a);
	void (*menu_close)(wimp_w w, wimp_i i, wimp_menu *m);
	wimp_menu *window_menu;
	bool window_menu_auto;
	bool window_menu_iconbar;
	const char *help_prefix;
	const char *(*get_help_suffix)(wimp_w w, wimp_i i, os_coord *pos,
			wimp_mouse_state buttons);
	void *user_data;
	struct icon_event *first;
	struct event_window *next;
	int max_radio_group;
};

static void ro_gui_wimp_event_ok_click(struct event_window *window,
		wimp_mouse_state state);
static struct event_window *ro_gui_wimp_event_get_window(wimp_w w);
static struct event_window *ro_gui_wimp_event_find_window(wimp_w w);
static struct icon_event *ro_gui_wimp_event_get_event(wimp_w w, wimp_i i,
		event_type type);
static void ro_gui_wimp_event_prepare_gright_menu(wimp_w w, struct icon_event *event);
static struct event_window *ro_gui_wimp_event_remove_window(wimp_w w);

static struct event_window *ro_gui_wimp_event_windows[WIN_HASH_SIZE];

static wimp_w ro_gui_wimp_event_submenu;

/**
 * Memorises the current state of any registered components in a window.
 *
 * \param w	the window to memorise
 * \return true on success, false on memory exhaustion or for an unknown window
 */
bool ro_gui_wimp_event_memorise(wimp_w w)
{
	struct event_window *window;
	struct icon_event *event;
	bool error = false;

	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return false;

	for (event = window->first; event; event = event->next) {
		switch (event->type) {
			case EVENT_NUMERIC_FIELD:
			case EVENT_TEXT_FIELD:
				if (event->previous_value.textual)
					free(event->previous_value.textual);
				event->previous_value.textual = strdup(
					ro_gui_get_icon_string(window->w, event->i));
				if (!event->previous_value.textual) {
					error = true;
					LOG(("Unable to store state for icon %i", event->i));
				}
				break;
			case EVENT_CHECKBOX:
			case EVENT_RADIO:
				event->previous_value.boolean =
					ro_gui_get_icon_selected_state(window->w, event->i);
				break;
			default:
				break;
		}
		if (event->type != EVENT_MENU_GRIGHT)
			event->previous_shaded = ro_gui_get_icon_shaded_state(window->w,
					event->i);
	}
	return !error;
}


/**
 * Restore the state of any registered components in a window to their memorised state.
 *
 * \param w	the window to restore
 * \return true on success, false for an unknown window
 */
bool ro_gui_wimp_event_restore(wimp_w w)
{
	struct event_window *window;
	struct icon_event *event;

	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return false;

	for (event = window->first; event; event = event->next) {
		switch (event->type) {
			case EVENT_NUMERIC_FIELD:
			case EVENT_TEXT_FIELD:
				if (event->previous_value.textual)
					ro_gui_set_icon_string(window->w, event->i,
						event->previous_value.textual, true);
				break;
			case EVENT_CHECKBOX:
			case EVENT_RADIO:
				ro_gui_set_icon_selected_state(window->w, event->i,
					event->previous_value.boolean);
				break;
			default:
				break;
		}
		if (event->type != EVENT_MENU_GRIGHT)
			ro_gui_set_icon_shaded_state(window->w, event->i,
					event->previous_shaded);
	}
	return true;
}


/**
 * Ensures all values are within pre-determined boundaries.
 *
 * \param w	the window to memorise
 * \return true on success, false for an unknown window
 */
bool ro_gui_wimp_event_validate(wimp_w w)
{
	struct event_window *window;
	struct icon_event *event;
	int value;

	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return false;

	for (event = window->first; event; event = event->next) {
		switch (event->type) {
			case EVENT_NUMERIC_FIELD:
				value = ro_gui_get_icon_decimal(window->w, event->i,
						event->data.numeric_field.decimal_places);
				if (value < event->data.numeric_field.min)
					value = event->data.numeric_field.min;
				else if (value > event->data.numeric_field.max)
					value = event->data.numeric_field.max;
				ro_gui_set_icon_decimal(window->w, event->i, value,
						event->data.numeric_field.decimal_places);
				break;
			default:
				break;
		}
	}
	return true;
}

/**
 * Transfer event data from one window to another. This can be used as an
 * alternative to ro_gui_wimp_event_finalise() and re-registering, if
 * events need to continue across a change of window handle.
 *
 * All aspects of the registered events MUST remain the same in the new
 * window!
 *
 * \param from		The current window, which is to be deleted.
 * \param to		The window to which the events should transfer.
 * \return		true on success; false for an unknown window.
 */

bool ro_gui_wimp_event_transfer(wimp_w from, wimp_w to)
{
	struct event_window	*window;
	int			h;

	LOG(("Transferring all events from window 0x%x to window 0x%x",
			(unsigned int) from, (unsigned int) to));

	window = ro_gui_wimp_event_remove_window(from);
	if (window == NULL || window->w != from)
		return false;

	h = WIN_HASH(to);
	window->w = to;
	window->next = ro_gui_wimp_event_windows[h];
	ro_gui_wimp_event_windows[h] = window;

	ro_gui_menu_window_changed(from, to);

	return true;
}

/**
 * Free any resources associated with a window.
 *
 * \param w	the window to free resources for
 */
void ro_gui_wimp_event_finalise(wimp_w w)
{
	struct event_window *window;
	struct icon_event *event;

	LOG(("Removing all events for window 0x%x", (unsigned int)w));
	window = ro_gui_wimp_event_remove_window(w);
	if (!window)
		return;

	while (window->first) {
		event = window->first;
		window->first = event->next;
		switch (event->type) {
			case EVENT_NUMERIC_FIELD:
			case EVENT_TEXT_FIELD:
				if (event->previous_value.textual)
					free(event->previous_value.textual);
				event->previous_value.textual = NULL;
				break;
			default:
				break;
		}
		free(event);
	}
	free(window);
	return;
}


/**
 * Free any resources associated with a specific icon in a window.
 *
 * \param w		The window containing the icon.
 * \param i		The icon to free resources for.
 */

void ro_gui_wimp_event_deregister(wimp_w w, wimp_i i)
{
	struct event_window	*window;
	struct icon_event	*event, *parent, *child;

	LOG(("Removing all events for window 0x%x, icon %d",
			(unsigned int)w, (int)i));
	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return;

	/* Remove any events that apply to the given icon. */

	event = window->first;
	parent = NULL;

	while (event != NULL) {
		child = event->next;

		if (event->i == i) {
			LOG(("Removing event 0x%x", (unsigned int) event));

			if (parent == NULL)
				window->first = child;
			else
				parent->next = child;

			switch (event->type) {
			case EVENT_NUMERIC_FIELD:
			case EVENT_TEXT_FIELD:
				if (event->previous_value.textual)
					free(event->previous_value.textual);
				event->previous_value.textual = NULL;
				break;
			default:
				break;
			}

			free(event);
		} else {
			parent = event;
		}

		event = child;
	}
}


/**
 * Set the associated help prefix for a given window.
 *
 * \param w		the window to get the prefix for
 * \param help_prefix	the prefix to associate with the window (used directly)
 * \return true on success, or NULL for memory exhaustion
 */
bool ro_gui_wimp_event_set_help_prefix(wimp_w w, const char *help_prefix)
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->help_prefix = help_prefix;
	return true;
}


/**
 * Get the associated help prefix.
 *
 * \param w	the window to get the prefix for
 * \return the associated prefix, or NULL
 */
const char *ro_gui_wimp_event_get_help_prefix(wimp_w w)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(w);
	if (window)
		return window->help_prefix;
	return NULL;
}


/**
 * Register a handler to decode help suffixes for a given window.
 *
 */

bool ro_gui_wimp_event_register_help_suffix(wimp_w w,
		const char *(*get_help_suffix)(wimp_w w, wimp_i i,
			os_coord *pos, wimp_mouse_state buttons))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->get_help_suffix = get_help_suffix;
	return true;
}


/**
 * Get the associated help suffix.
 *
 * \param w		The window to get the suffix for
 * \return		The associated prefix, or NULL
 */

const char *ro_gui_wimp_event_get_help_suffix(wimp_w w, wimp_i i,
		os_coord *pos, wimp_mouse_state buttons)
{
	struct		event_window *window;

	window = ro_gui_wimp_event_find_window(w);
	if (window == NULL || window->get_help_suffix == NULL)
		return NULL;

	return window->get_help_suffix(w, i, pos, buttons);
}


/**
 * Sets the user data associated with a window.
 *
 * \param w	the window to associate the data with
 * \param user	the data to associate
 */
bool ro_gui_wimp_event_set_user_data(wimp_w w, void *user)
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->user_data = user;
	return true;

}


/**
 * Gets the user data associated with a window.
 *
 * \param w	the window to retrieve the data for
 * \return the associated data, or NULL
 */
void *ro_gui_wimp_event_get_user_data(wimp_w w)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(w);
	if (window)
		return window->user_data;
	return NULL;
}


/**
 * Handles a menu selection event.
 *
 * (At present, this is tied to being called from menus.c and relies on that
 * module decoding the menu into an action code.  If menus.c loses its
 * menu handling in the future, such decoding might need to move here.)
 *
 * The order of execution is:
 *
 * 1. Try to match the menu to a pop-up menu.  If successful, handle it as
 *    this.
 * 2. Try to match the menu to a window menu.  If successful, pass control to
 *    the menu's registered _select handler.
 * 3. Return event as unhandled.
 *
 * \param w		the window to owning the menu
 * \param i		the icon owning the menu
 * \param menu		the menu that has been selected
 * \param selection	the selection information
 * \param action	the menu action info from menus.c
 * \return		true if the menu is OK for an Adjust re-open;
 *			else false.
 */
bool ro_gui_wimp_event_menu_selection(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	struct event_window *window;
	struct icon_event *event;
	wimp_menu_entry *menu_entry;
	wimp_key key;
	os_error *error;
	wimp_caret caret;
	wimp_icon_state ic;
	unsigned int button_type;
	bool prepared;

	window = ro_gui_wimp_event_find_window(w);
	if (window == NULL)
		return false;

	/* Start by looking for an icon event that matches.  If there isn't one,
	 * then return details for an unconnected menu.  It's up to the
	 * event recipient to sort out if this is a window menu or not, based
	 * on the menu handle passed back.
	 */

	for (event = window->first; event; event = event->next)
		if ((event->type == EVENT_MENU_GRIGHT) && (event->i == i))
			break;
	if (!event) {
		if (window->menu_selection)
			window->menu_selection(window->w, wimp_ICON_WINDOW,
					menu, selection, action);

		/* Prepare the menu pending a possible Adjust click. */
		if (window->menu_prepare)
			if (!window->menu_prepare(window->w, wimp_ICON_WINDOW,
					menu, NULL))
				return false;

		return true;
	}

	menu_entry = &menu->entries[selection->items[0]];
	for (i = 1; selection->items[i] != -1; i++)
		menu_entry = &menu_entry->sub_menu->
				entries[selection->items[i]];

	/* if the entry is already ticked then we do nothing */
	if (menu_entry->menu_flags & wimp_MENU_TICKED)
		return true;

	ro_gui_set_icon_string(window->w, event->data.menu_gright.field,
			menu_entry->data.indirected_text.text, false);
	if (window->menu_selection)
		window->menu_selection(window->w, event->i, menu,
				selection, action);
	prepared = true;
	if (window->menu_prepare)
		prepared = window->menu_prepare(window->w, event->i,
				menu, NULL);
	if (prepared)
		ro_gui_wimp_event_prepare_gright_menu(window->w, event);

	/* set the caret for writable icons and send a CTRL+U keypress to
	 * stimulate activity if needed */
	ic.w = window->w;
	ic.i = event->data.menu_gright.field;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	button_type = (ic.icon.flags & wimp_ICON_BUTTON_TYPE) >> wimp_ICON_BUTTON_TYPE_SHIFT;
	if ((button_type != wimp_BUTTON_WRITABLE) &&
			(button_type != wimp_BUTTON_WRITE_CLICK_DRAG))
		return prepared;
	error = xwimp_get_caret_position(&caret);
	if (error) {
		LOG(("xwimp_get_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}
	if ((caret.w != window->w) || (caret.i != event->data.menu_gright.field)) {
		error = xwimp_set_caret_position(window->w, event->data.menu_gright.field,
				-1, -1, -1, strlen(menu_entry->data.indirected_text.text));
		if (error) {
			LOG(("xwimp_set_caret_position: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
	if (window->keypress) {
		key.w = window->w;
		key.c = 21;	// ctrl+u
		window->keypress(&key);
	}
	return prepared;
}


/**
 * Handles a mouse click event in a registered window.
 *
 * The order of execution is:
 *
 *  1. If a menu click, and the window has an automatic window menu, this is
 *     processed immediately.
 *  2. Any registered mouse_click routine (see ro_gui_wimp_register_mouse_click())
 *  3. If the current icon is not registered with a type then it is assumed that no
 *     action is necessary, and the click is deemed to have been handled.
 *  4. If the registered mouse_click routine returned false, or there was no registered
 *     routine then the automated action for the registered icon type is performed
 *
 * \param pointer	the current pointer state
 * \return true if the event was handled, false otherwise
 */
bool ro_gui_wimp_event_mouse_click(wimp_pointer *pointer)
{
	struct event_window *window;
	struct icon_event *event;
	wimp_w w;
	struct icon_event *search;
	int current, step, stepping, min, max, decimal_places;
	wimp_window_state open;
	wimp_caret caret;
	os_error *error;
	bool prepared;

	w = pointer->w;
	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return false;

	/* Menu clicks take priority if there is an auto menu defined. */
	if ((pointer->buttons == wimp_CLICK_MENU) &&
			(window->window_menu != NULL) &&
			(window->window_menu_auto)) {
		ro_gui_wimp_event_process_window_menu_click(pointer);
		return true;
	}

	/* registered routines take next priority */
	if ((window->mouse_click) && (window->mouse_click(pointer)))
		return true;

	for (event = window->first; event; event = event->next)
		if (event->i == pointer->i)
			break;
	if (!event)
		return true;

	switch (event->type) {
		case EVENT_NUMERIC_FIELD:
		case EVENT_TEXT_FIELD:
			break;
		case EVENT_UP_ARROW:
		case EVENT_DOWN_ARROW:
			for (search = window->first; search; search = search->next)
				if (search->i == event->data.linked_icon) break;
			if (!search) {
				LOG(("Incorrect reference."));
				return false;
			}
			stepping = search->data.numeric_field.stepping;
			min = search->data.numeric_field.min;
			max = search->data.numeric_field.max;
			decimal_places = search->data.numeric_field.decimal_places;

			if (pointer->buttons & wimp_CLICK_ADJUST)
				step = -stepping;
			else if (pointer->buttons & wimp_CLICK_SELECT)
				step = stepping;
			else
				return true;
			if (event->type == EVENT_DOWN_ARROW)
				step = -step;

			current = ro_gui_get_icon_decimal(pointer->w, event->data.linked_icon,
					decimal_places);
			current += step;
			if (current < min)
				current = min;
			if (current > max)
				current = max;
			ro_gui_set_icon_decimal(pointer->w, event->data.linked_icon, current,
					decimal_places);
			break;
		case EVENT_MENU_GRIGHT:
			/* if there's already a menu open then we assume that we are part of it.
			 * to follow the standard RISC OS behaviour we add a 'send to the back'
			 * button, then close the menu (which closes us) and then finally
			 * re-open ourselves. ugh! */
			if (current_menu != NULL) {
				open.w = pointer->w;
				error = xwimp_get_window_state(&open);
				if (error) {
					LOG(("xwimp_get_window_state: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
					return false;
				}
				error = xwimp_get_caret_position(&caret);
				if (error) {
					LOG(("xwimp_get_caret_position: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
					return false;
				}
				ro_gui_dialog_add_persistent(current_menu_window,
						pointer->w);
				ro_gui_menu_destroy();
				error = xwimp_open_window(PTR_WIMP_OPEN(&open));
				if (error) {
					LOG(("xwimp_open_window: 0x%x: %s",
							error->errnum, error->errmess));
					warn_user("WimpError", error->errmess);
					return false;
				}
				if (caret.w == pointer->w) {
					error = xwimp_set_caret_position(caret.w,
							caret.i,
							caret.pos.x, caret.pos.y,
							-1, caret.index);
					if (error) {
						LOG(("xwimp_set_caret_position: 0x%x: %s",
								error->errnum, error->errmess));
						warn_user("WimpError", error->errmess);
					}
				}
			}
			/* display the menu */

			prepared = true;
			if (window->menu_prepare != NULL)
				prepared = window->menu_prepare(pointer->w, pointer->i,
						event->data.menu_gright.menu, pointer);
			if (prepared) {
				ro_gui_wimp_event_prepare_gright_menu(pointer->w, event);
				ro_gui_popup_menu(event->data.menu_gright.menu, pointer->w, pointer->i);
			}
			break;
		case EVENT_CHECKBOX:
			break;
		case EVENT_RADIO:
			for (search = window->first; search; search = search->next)
				if ((search->type == EVENT_RADIO) &&
						(search->data.radio_group ==
							event->data.radio_group))
					ro_gui_set_icon_selected_state(pointer->w,
							search->i, (search == event));
			break;
		case EVENT_BUTTON:
			if (event->data.callback)
				event->data.callback(pointer);
			break;
		case EVENT_CANCEL:
			if (pointer->buttons & wimp_CLICK_SELECT) {
				ro_gui_dialog_close(pointer->w);
				ro_gui_wimp_event_close_window(pointer->w);
				ro_gui_menu_destroy();
			} else {
				ro_gui_wimp_event_restore(pointer->w);
			}
			break;
		case EVENT_OK:
			ro_gui_wimp_event_ok_click(window, pointer->buttons);
			break;
	}
	return true;
}


/**
 * Prepare a menu ready for use
 *
 * /param w	the window owning the menu
 * /param event	the icon event owning the menu
 */
void ro_gui_wimp_event_prepare_gright_menu(wimp_w w, struct icon_event *event)
{
	int i;
	const char *text;
	unsigned int button_type;
	wimp_icon_state ic;
	wimp_menu *menu;
	os_error *error;

	/* if the linked icon is not writable then we set the ticked state
	 * of the menu item that matches the contents */
	ic.w = w;
	ic.i = event->data.menu_gright.field;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	button_type = (ic.icon.flags & wimp_ICON_BUTTON_TYPE)
			>> wimp_ICON_BUTTON_TYPE_SHIFT;
	if ((button_type == wimp_BUTTON_WRITABLE) ||
			(button_type == wimp_BUTTON_WRITE_CLICK_DRAG))
		return;
	text = ro_gui_get_icon_string(w, event->data.menu_gright.field);
	menu = event->data.menu_gright.menu;
	i = 0;
	do {
		if (!strcmp(menu->entries[i].data.indirected_text.text, text))
			menu->entries[i].menu_flags |= wimp_MENU_TICKED;
		else
			menu->entries[i].menu_flags &= ~wimp_MENU_TICKED;
	} while (!(menu->entries[i++].menu_flags & wimp_MENU_LAST));
}


/**
 * Perform the necessary actions following a click on the OK button.
 *
 * /param window	the window to perform the action on
 * /param state		the mouse button state
 */
void ro_gui_wimp_event_ok_click(struct event_window *window,
		wimp_mouse_state state)
{
	struct icon_event *search;

	for (search = window->first; search; search = search->next)
		if (search->type == EVENT_OK) {
			if (ro_gui_get_icon_shaded_state(window->w, search->i))
				return;
			break;
		}
	ro_gui_wimp_event_validate(window->w);

	if (window->ok_click)
		if (!window->ok_click(window->w))
			return;

	if (state & wimp_CLICK_SELECT) {
		ro_gui_dialog_close(window->w);
		ro_gui_wimp_event_close_window(window->w);
		ro_gui_menu_destroy();
	} else {
		ro_gui_wimp_event_memorise(window->w);
	}
}


/**
 * Handle any registered keypresses, and the standard RISC OS ones
 *
 * \param key	the key state
 * \return true if keypress handled, false otherwise
 */
bool ro_gui_wimp_event_keypress(wimp_key *key)
{
	static const int *ucstable = NULL;
	static int alphabet = 0;
	static uint32_t wc = 0;	/* buffer for UTF8 alphabet */
	static int shift = 0;
	struct event_window *window;
	struct icon_event *event;
	wimp_pointer pointer;
	wimp_key k;
	uint32_t c = (uint32_t) key->c;
	int t_alphabet;
	os_error *error;

	window = ro_gui_wimp_event_find_window(key->w);
	if (!window)
		return false;

	/* copy key state so we can corrupt it safely */
	memcpy(&k, key, sizeof(wimp_key));

	/* In order to make sensible use of the 0x80->0xFF ranges specified
	 * in the RISC OS 8bit alphabets, we must do the following:
	 *
	 * + Read the currently selected alphabet
	 * + Acquire a pointer to the UCS conversion table for this alphabet:
	 *     + Try using ServiceInternational 8 to get the table
	 *     + If that fails, use our internal table (see ucstables.c)
	 * + If the alphabet is not UTF8 and the conversion table exists:
	 *     + Lookup UCS code in the conversion table
	 *     + If code is -1 (i.e. undefined):
	 *         + Use codepoint 0xFFFD instead
	 * + If the alphabet is UTF8, we must buffer input, thus:
	 *     + If the keycode is < 0x80:
	 *         + Handle it directly
	 *     + If the keycode is a UTF8 sequence start:
	 *         + Initialise the buffer appropriately
	 *     + Otherwise:
	 *         + OR in relevant bits from keycode to buffer
	 *         + If we've received an entire UTF8 character:
	 *             + Handle UCS code
	 * + Otherwise:
	 *     + Simply handle the keycode directly, as there's no easy way
	 *       of performing the mapping from keycode -> UCS4 codepoint.
	 */
	error = xosbyte1(osbyte_ALPHABET_NUMBER, 127, 0, &t_alphabet);
	if (error) {
		LOG(("failed reading alphabet: 0x%x: %s",
				error->errnum, error->errmess));
		/* prevent any corruption of ucstable */
		t_alphabet = alphabet;
	}

	if (t_alphabet != alphabet) {
		void *ostable;
		osbool unclaimed;
		/* Alphabet has changed, so read UCS table location */
		alphabet = t_alphabet;

		error = xserviceinternational_get_ucs_conversion_table(
						alphabet, &unclaimed, &ostable);
		if (error != NULL) {
			LOG(("failed reading UCS conversion table: 0x%x: %s",
					error->errnum, error->errmess));
			/* Try using our own table instead */
			ucstable = ucstable_from_alphabet(alphabet);
		} else if (unclaimed) {
			/* Service wasn't claimed so use our own ucstable */
			ucstable = ucstable_from_alphabet(alphabet);
		} else {
			/* Use the table provided by the OS */
			ucstable = ostable;
		}
	}

	if (c < 256) {
		if (alphabet != 111 /* UTF8 */ && ucstable != NULL) {
			/* defined in this alphabet? */
			if (ucstable[c] == -1)
				return true;

			/* read UCS4 value out of table */
			k.c = ucstable[c];
		}
		else if (alphabet == 111 /* UTF8 */) {
			if ((c & 0x80) == 0x00 || (c & 0xC0) == 0xC0) {
				/* UTF8 start sequence */
				if ((c & 0xE0) == 0xC0) {
					wc = ((c & 0x1F) << 6);
					shift = 1;
					return true;
				}
				else if ((c & 0xF0) == 0xE0) {
					wc = ((c & 0x0F) << 12);
					shift = 2;
					return true;
				}
				else if ((c & 0xF8) == 0xF0) {
					wc = ((c & 0x07) << 18);
					shift = 3;
					return true;
				}
				/* These next two have been removed
				 * from RFC3629, but there's no
				 * guarantee that RISC OS won't
				 * generate a UCS4 value outside the
				 * UTF16 plane, so we handle them
				 * anyway. */
				else if ((c & 0xFC) == 0xF8) {
					wc = ((c & 0x03) << 24);
					shift = 4;
				}
				else if ((c & 0xFE) == 0xFC) {
					wc = ((c & 0x01) << 30);
					shift = 5;
				}
				else if (c >= 0x80) {
					/* If this ever happens,
					 * RISC OS' UTF8 keyboard
					 * drivers are broken */
					LOG(("unexpected UTF8 start"
					     " byte %x (ignoring)",
					     c));
					return true;
				}
				/* Anything else is ASCII, so just
				 * handle it directly. */
			}
			else {
				if ((c & 0xC0) != 0x80) {
					/* If this ever happens,
					 * RISC OS' UTF8 keyboard
					 * drivers are broken */
					LOG(("unexpected keycode: "
					     "%x (ignoring)", c));
					return true;
				}

				/* Continuation of UTF8 character */
				wc |= ((c & 0x3F) << (6 * --shift));
				if (shift > 0)
					/* partial character */
					return true;
				else
					/* got entire character, so
					 * fetch from buffer and
					 * handle it */
					k.c = wc;
			}
		}
	} else {
		k.c |= IS_WIMP_KEY;
	}

	/* registered routines take priority */
	if (window->keypress)
		if (window->keypress(&k))
			return true;

	switch (key->c) {
		/* Escape performs the CANCEL action (simulated click) */
		case wimp_KEY_ESCAPE:
			for (event = window->first; event; event = event->next) {
				switch (event->type) {
					case EVENT_CANCEL:
						pointer.w = key->w;
						pointer.i = event->i;
						pointer.buttons = wimp_CLICK_SELECT;
						ro_gui_wimp_event_mouse_click(&pointer);
						return true;
					default:
						break;
				}
			}
			return false;
		/* CTRL+F2 closes a window with a close icon */
		case wimp_KEY_CONTROL + wimp_KEY_F2:
			if (!ro_gui_wimp_check_window_furniture(key->w,
					wimp_WINDOW_CLOSE_ICON))
				return false;
			ro_gui_dialog_close(key->w);
			ro_gui_wimp_event_close_window(key->w);
			ro_gui_menu_destroy();
			return true;
		/* Return performs the OK action */
		case wimp_KEY_RETURN:
			if (!window->ok_click)
				return false;
			/* todo: check we aren't greyed out */
			ro_gui_wimp_event_ok_click(window, wimp_CLICK_SELECT);
			return true;
	}
	return false;
}


/**
 * Handle any open window requests
 *
 * \param open	the window open request
 */
bool ro_gui_wimp_event_open_window(wimp_open *open)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(open->w);
	if ((window) && (window->open_window)) {
		window->open_window(open);
		return true;
	}
	return false;
}


/**
 * Service any close window handlers
 *
 * \param w	the window being closed
 */
bool ro_gui_wimp_event_close_window(wimp_w w)
{
	struct event_window *window;

	LOG(("Close event received for window 0x%x", (unsigned int)w));
	if (w == ro_gui_wimp_event_submenu)
		ro_gui_wimp_event_submenu = 0;
	window = ro_gui_wimp_event_find_window(w);
	if ((window) && (window->close_window)) {
		window->close_window(w);
		return true;
	}
	return false;
}


/**
 * Handle any redraw window requests
 *
 * \param redraw	the window redraw request
 */
bool ro_gui_wimp_event_redraw_window(wimp_draw *redraw)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(redraw->w);
	if ((window) && (window->redraw_window)) {
		window->redraw_window(redraw);
		return true;
	}
	return false;
}


/**
 * Handle any scroll window requests
 *
 * \param scroll	the window scroll request
 */
bool ro_gui_wimp_event_scroll_window(wimp_scroll *scroll)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(scroll->w);
	if ((window) && (window->scroll_window)) {
		window->scroll_window(scroll);
		return true;
	}
	return false;
}


/**
 * Handle any pointer entering window requests
 *
 * \param entering	the pointer entering window request
 */
bool ro_gui_wimp_event_pointer_entering_window(wimp_entering *entering)
{
	struct event_window *window;

	window = ro_gui_wimp_event_find_window(entering->w);
	if ((window) && (window->entering_window)) {
		window->entering_window(entering);
		return true;
	}
	return false;
}


/**
 * Process a Menu click in a window, by checking for a registered window
 * menu and opening it if one is found.
 *
 * \param  *p		The pointer block from the mouse click event.
 * \return		true if the click was actioned; else false.
 */

bool ro_gui_wimp_event_process_window_menu_click(wimp_pointer *pointer)
{
	struct event_window	*window;
	int			xpos, ypos, line_height, gap_height, entry;

	window = ro_gui_wimp_event_find_window(pointer->w);
	if ((window) && (window->window_menu)
			&& (pointer->buttons == wimp_CLICK_MENU)) {
		if (window->menu_prepare)
			if (!window->menu_prepare(window->w, wimp_ICON_WINDOW,
					window->window_menu, pointer))
				return false;

		if (window->window_menu_iconbar) {
			xpos = pointer->pos.x;
			ypos = 96;

			line_height = window->window_menu->height +
					window->window_menu->gap;
			gap_height = 24; /* The fixed dotted line height */

			entry = 0;
			do {
				ypos += line_height;
				if ((window->window_menu->
						entries[entry].menu_flags &
						wimp_MENU_SEPARATE) != 0)
					ypos += gap_height;
			} while ((window->window_menu->
					entries[entry++].menu_flags &
					wimp_MENU_LAST) == 0);
		} else {
			xpos = pointer->pos.x;
			ypos = pointer->pos.y;
		}

		ro_gui_menu_create(window->window_menu, xpos, ypos, window->w);
		return true;
	}
	return false;
}


/**
 * Trigger a window's Prepare Menu event.
 *
 * \param w			The window to use.
 * \param i			The icon to use.
 * \param *menu			The menu handle to use.
 * \return			true if the affected menu was prepared OK; else
 *				false.
 */

bool ro_gui_wimp_event_prepare_menu(wimp_w w, wimp_i i, wimp_menu *menu)
{
	struct event_window	*window;

	window = ro_gui_wimp_event_find_window(w);
	if (window == NULL)
		return false;

	if (window->menu_prepare)
		return window->menu_prepare(w, i, menu, NULL);

	/* The menu is always OK if there's no event handler. */

	return true;
}

/**
 * Register a window menu to be (semi-)automatically handled.
 *
 * \param w			The window to attach the menu to.
 * \param *m			The menu to be attached.
 * \param menu_auto		true if the menu should be opened autimatically
 *				on Menu clicks with no task intervention; false
 *				to pass clicks to the window's Mouse Event
 *				handler and leave that to pass the menu click
 *				back to us for handling and menu opening.
 * \param bool_position_ibar	true if the menu should open in an iconbar
 *				position; false to open at the pointer.
 * \return			true if the menu was registed ok; else false.
 */

bool ro_gui_wimp_event_register_menu(wimp_w w, wimp_menu *m,
		bool menu_auto, bool position_ibar)
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->window_menu = m;
	window->window_menu_auto = menu_auto;
	window->window_menu_iconbar = position_ibar;
	return true;
}

/**
 * Register a numeric field to be automatically handled
 */
bool ro_gui_wimp_event_register_numeric_field(wimp_w w, wimp_i i,
		wimp_i up, wimp_i down,
		int min, int max, int stepping, int decimal_places)
{
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_NUMERIC_FIELD);
	if (!event)
		return false;
	event->data.numeric_field.min = min;
	event->data.numeric_field.max = max;
	event->data.numeric_field.stepping = stepping;
	event->data.numeric_field.decimal_places = decimal_places;

	event = ro_gui_wimp_event_get_event(w, up, EVENT_UP_ARROW);
	if (!event)
		return false;
	event->data.linked_icon = i;

	event = ro_gui_wimp_event_get_event(w, down, EVENT_DOWN_ARROW);
	if (!event)
		return false;
	event->data.linked_icon = i;

	return true;
}


/**
 * Register a text field to be automatically handled
 */
bool ro_gui_wimp_event_register_text_field(wimp_w w, wimp_i i) {
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_TEXT_FIELD);
	if (!event)
		return false;
	return true;
}


/**
 * Register an icon menu to be automatically handled
 */
bool ro_gui_wimp_event_register_menu_gright(wimp_w w, wimp_i i,
		wimp_i gright, wimp_menu *menu)
{
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, gright, EVENT_MENU_GRIGHT);
	if (!event)
		return false;
	event->data.menu_gright.field = i;
	event->data.menu_gright.menu = menu;

	return ro_gui_wimp_event_register_text_field(w, i);
}


/**
 * Register a checkbox to be automatically handled
 */
bool ro_gui_wimp_event_register_checkbox(wimp_w w, wimp_i i)
{
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_CHECKBOX);
	if (!event)
		return false;
	return true;
}


/**
 * Register a group of radio icons to be automatically handled
 */
bool ro_gui_wimp_event_register_radio(wimp_w w, wimp_i *i)
{
	struct event_window *window;
	struct icon_event *event;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->max_radio_group++;

	while (*i != -1) {
		event = ro_gui_wimp_event_get_event(w, *i, EVENT_RADIO);
		if (!event)
			return false;
		event->data.radio_group = window->max_radio_group;
		i++;
	}
	return true;
}


/**
 * Register a function to be called when a particular button is pressed.
 */
bool ro_gui_wimp_event_register_button(wimp_w w, wimp_i i,
		void (*callback)(wimp_pointer *pointer))
{
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_BUTTON);
	if (!event)
		return false;
	event->data.callback = callback;
	return true;
}


/**
 * Register a function to be called for the Cancel action on a window.
 */
bool ro_gui_wimp_event_register_cancel(wimp_w w, wimp_i i)
{
	struct icon_event *event;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_CANCEL);
	if (!event)
		return false;
	return true;
}


/**
 * Register a function to be called for the OK action on a window.
 */
bool ro_gui_wimp_event_register_ok(wimp_w w, wimp_i i,
		bool (*callback)(wimp_w w))
{
	struct event_window *window;
	struct icon_event *event;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->ok_click = callback;

	event = ro_gui_wimp_event_get_event(w, i, EVENT_OK);
	if (!event)
		return false;
	return true;
}


/**
 * Register a function to be called for all mouse-clicks to icons
 * in a window that don't have registered actions.
 */
bool ro_gui_wimp_event_register_mouse_click(wimp_w w,
		bool (*callback)(wimp_pointer *pointer))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->mouse_click = callback;
	return true;
}


/**
 * Register a function to be called for all keypresses within a
 * particular window.
 *
 * Important: the character code passed to the callback in key->c
 * is UTF-32 (i.e. in the range [0, &10ffff]). WIMP keys (e.g. F1)
 * will have bit 31 set.
 *
 */
bool ro_gui_wimp_event_register_keypress(wimp_w w,
		bool (*callback)(wimp_key *key))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->keypress = callback;
	return true;
}


/**
 * Register a function to be called for all window opening requests.
 */
bool ro_gui_wimp_event_register_open_window(wimp_w w,
		void (*callback)(wimp_open *open))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->open_window = callback;
	return true;
}

/**
 * Register a function to be called after the window has been closed.
 */
bool ro_gui_wimp_event_register_close_window(wimp_w w,
		void (*callback)(wimp_w w))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->close_window = callback;
	return true;
}

/**
 * Register a function to be called for all window redraw operations.
 */
bool ro_gui_wimp_event_register_redraw_window(wimp_w w,
		void (*callback)(wimp_draw *redraw))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->redraw_window = callback;
	return true;
}

/**
 * Register a function to be called for all window scroll requests.
 */

bool ro_gui_wimp_event_register_scroll_window(wimp_w w,
		void (*callback)(wimp_scroll *scroll))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->scroll_window = callback;
	return true;
}

/**
 * Register a function to be called for all pointer entering window requests.
 */

bool ro_gui_wimp_event_register_pointer_entering_window(wimp_w w,
		void (*callback)(wimp_entering *entering))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->entering_window = callback;
	return true;
}

/**
 * Register a function to be called before a menu is (re-)opened.
 *
 * \param *w			The window for which events should be returned.
 * \param *callback		A function to be called beofre the menu is
 *				(re-)opened.
 * \return			true if the menu was registed ok; else false.
 */
bool ro_gui_wimp_event_register_menu_prepare(wimp_w w,
		bool (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_pointer *p))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->menu_prepare = callback;
	return true;
}


/**
 * Register a function to be called following a menu selection.
 *
 * \param *w			The window for which events should be returned.
 * \param *callback		A function to be called when a selection is
 *				made.
 * \return			true if the menu was registed ok; else false.
 */
bool ro_gui_wimp_event_register_menu_selection(wimp_w w,
		bool (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_selection *s, menu_action a))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->menu_selection = callback;
	return true;
}


/**
 * Register a function to be called when a sub-menu warning is received.
 *
 * \param *w			The window for which events should be returned.
 * \param *callback		A function to be called whenever a submenu
 *				warning is received for the menu.
 * \return			true if the menu was registed ok; else false.
 */
bool ro_gui_wimp_event_register_menu_warning(wimp_w w,
		void (*callback)(wimp_w w, wimp_i i, wimp_menu *m,
			wimp_selection *s, menu_action a))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->menu_warning = callback;
	return true;
}


/**
 * Register a function to be called before a menu is finally closed.
 *
 * \param *w			The window for which events should be returned.
 * \param *callback		A function to be called when the menu is closed.
 * \return			true if the menu was registed ok; else false.
 */
bool ro_gui_wimp_event_register_menu_close(wimp_w w,
		void (*callback)(wimp_w w, wimp_i i, wimp_menu *m))
{
	struct event_window *window;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return false;
	window->menu_close = callback;
	return true;
}


/**
 * Finds the event data associated with a given window handle, or creates a
 *  new one.
 *
 * \param w	the window to find data for
 */
struct event_window *ro_gui_wimp_event_get_window(wimp_w w)
{
	struct event_window *window;
	int h;

	assert((int)w != 0);
	window = ro_gui_wimp_event_find_window(w);
	if (window)
		return window;

	LOG(("Creating structure for window 0x%x", (unsigned int)w));
	window = calloc(1, sizeof(struct event_window));
	if (!window)
		return NULL;

	h = WIN_HASH(w);
	window->w = w;
	window->next = ro_gui_wimp_event_windows[h];
	ro_gui_wimp_event_windows[h] = window;
	return window;
}


/**
 * Removes the event data associated with a given handle from the hash tables,
 * but does not delete it.
 *
 * \param   w  the window to be removed
 * \return  pointer to the event data or NULL if not found
 */

struct event_window *ro_gui_wimp_event_remove_window(wimp_w w)
{
	struct event_window **prev;
	int h = WIN_HASH(w);

	/* search hash chain for the window */
	prev = &ro_gui_wimp_event_windows[h];
	while (*prev) {
		struct event_window *window = *prev;

		if (window->w == w) {
			/* remove from chain */
			*prev = window->next;
			return window;
		}
		prev = &window->next;
	}

	/* not found */
	return NULL;
}

/**
 * Find the event data associated with a given window handle
 *
 * \param w	the window to find data for
 */
struct event_window *ro_gui_wimp_event_find_window(wimp_w w)
{
	struct event_window *window;
	int h = WIN_HASH(w);

	/* search hash chain for window */
	for (window = ro_gui_wimp_event_windows[h]; window; window = window->next) {
		if (window->w == w)
			return window;
	}
	return NULL;
}

struct icon_event *ro_gui_wimp_event_get_event(wimp_w w, wimp_i i,
		event_type type)
{
	struct event_window *window;
	struct icon_event *event;

	window = ro_gui_wimp_event_get_window(w);
	if (!window)
		return NULL;

	for (event = window->first; event; event = event->next) {
		if (event->i == i) {
			event->type = type;
			return event;
		}
	}

	event = calloc(1, sizeof(struct icon_event));
	if (!event)
		return NULL;
	event->i = i;
	event->type = type;
	event->next = window->first;
	window->first = event;

	return event;
}

/* Handle sumbenu warnings.  This is called from ro_gui_menu_warning(), and
 * returns to that function to have the submenu opened correctly.
 *
 * \param w		the window to owning the menu
 * \param i		the icon owning the menu
 * \param menu		the menu that has been selected
 * \param selection	the selection information
 * \param action	the menu action info from menus.c
 * \return		true if the event was handled, false otherwise
 */

bool ro_gui_wimp_event_submenu_warning(wimp_w w, wimp_i i, wimp_menu *menu,
		wimp_selection *selection, menu_action action)
{
	struct event_window *window;
	struct icon_event *event;

	ro_gui_wimp_event_register_submenu(0);

	/* Process the event for any window menus.  Find the window data, then
	 * try and match to an icon event.  If we can, then there isn't anything
	 * to do.
	 */

	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return false;

	for (event = window->first; event; event = event->next)
		if ((event->type == EVENT_MENU_GRIGHT) && (event->i == i))
			break;
	if (event) {
		if (window->menu_close != NULL &&
				event->type == EVENT_MENU_GRIGHT &&
				event->data.menu_gright.menu == menu) {
			window->menu_close(w, i, menu);
			return true;
		}

		return false;
	}

	/* If the warning is for a window menu, then pass the event on to it. */

	if ((window->window_menu) && (window->window_menu == menu)) {
		if (window->menu_warning) {
			window->menu_warning(w, wimp_ICON_WINDOW, menu,
					selection, action);
			return true;
		}
	}

	return false;
}

/**
 * Handle menus being closed.  This is called from the menus modules, in
 * every scenario when one of our own menus is open.
 *
 * \param w		the window to owning the menu
 * \param i		the icon owning the menu
 * \param menu		the menu that has been selected
 */

void ro_gui_wimp_event_menus_closed(wimp_w w, wimp_i i, wimp_menu *menu)
{
	struct event_window *window;
	struct icon_event *event;

	ro_gui_wimp_event_register_submenu(0);

	/* Process the event for any window menus.  Find the window data, then
	 * try and match to an icon event.  If we can, then GRight menus are
	 * sent the event; otherwise, we do nothing.
	 */

	window = ro_gui_wimp_event_find_window(w);
	if (!window)
		return;

	for (event = window->first; event; event = event->next)
		if ((event->type == EVENT_MENU_GRIGHT) && (event->i == i))
			break;
	if (event) {
		if (window->menu_close != NULL &&
				event->type == EVENT_MENU_GRIGHT &&
				event->data.menu_gright.menu == menu)
			window->menu_close(w, i, menu);
		return;
	}

	/* If the close is for a window menu, then pass the event on to it. */

	if ((window->window_menu) && (window->window_menu == menu) &&
			(window->menu_close))
		window->menu_close(w, wimp_ICON_WINDOW, menu);
}

/**
 * Register a submenu as being opened
 */
void ro_gui_wimp_event_register_submenu(wimp_w w)
{
	if (ro_gui_wimp_event_submenu)
		ro_gui_wimp_event_close_window(ro_gui_wimp_event_submenu);
	ro_gui_wimp_event_submenu = w;
}

