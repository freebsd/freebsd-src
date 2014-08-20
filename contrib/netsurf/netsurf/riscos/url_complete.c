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
 * GUI URL auto-completion (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/wimp.h"
#include "content/urldb.h"
#include "utils/log.h"
#include "riscos/global_history.h"
#include "riscos/gui.h"
#include "riscos/mouse.h"
#include "utils/nsoption.h"
#include "riscos/toolbar.h"
#include "riscos/url_complete.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/utils.h"

#define MAXIMUM_VISIBLE_LINES 7

static nsurl **url_complete_matches = NULL;
static int url_complete_matches_allocated = 0;
static int url_complete_matches_available = 0;
static char *url_complete_matched_string = NULL;
static int url_complete_matches_selection = -1;
static int url_complete_keypress_selection = -1;
static wimp_w url_complete_parent = 0;
static bool url_complete_matches_reset = false;
static char *url_complete_original_url = NULL;
static bool url_complete_memory_exhausted = false;

static nsurl *url_complete_redraw[MAXIMUM_VISIBLE_LINES];
static char url_complete_icon_null[] = "";
static char url_complete_icon_sprite[12];
static wimp_icon url_complete_icon;
static wimp_icon url_complete_sprite;
static int mouse_x;
static int mouse_y;

static bool url_complete_callback(nsurl *url,
		const struct url_data *data);
static void ro_gui_url_complete_mouse_at(wimp_pointer *pointer, void *data);


/* This is an exported interface documented in url_complete.h */

void ro_gui_url_complete_start(struct toolbar *toolbar)
{
	const char	*url;
	wimp_w		parent;

	assert(toolbar != NULL);
	parent = ro_toolbar_get_parent_window(toolbar);

	if (!ro_toolbar_get_display_url(toolbar) ||
			(parent == url_complete_parent))
		return;

	ro_gui_url_complete_close();
	url = ro_toolbar_get_url(toolbar);

	url_complete_matched_string = strdup(url);
	if (url_complete_matched_string)
		url_complete_parent = parent;
}


/* This is an exported interface documented in url_complete.h */

bool ro_gui_url_complete_keypress(struct toolbar *toolbar, uint32_t key)
{
	wimp_w			parent;
	wimp_window_state	state;
	char			*match_url;
	const char		*url;
	int			i, lines;
	int			old_selection;
	int			height;
	os_error		*error;
	bool			currently_open;

	assert(toolbar != NULL);
	parent = ro_toolbar_get_parent_window(toolbar);

	/* we must have a toolbar/url bar */
	if (!ro_toolbar_get_display_url(toolbar) ||
	    (!nsoption_bool(url_suggestion))) {
		ro_gui_url_complete_close();
		return false;
	}

	/* if we are currently active elsewhere, remove the previous window */
	currently_open = ((parent == url_complete_parent) &&
			(url_complete_matches_available > 0));
	if (parent != url_complete_parent)
		ro_gui_url_complete_close();

	/* forcibly open on down keys */
	if ((!currently_open) && (url_complete_matched_string)) {
		switch (key) {
			case IS_WIMP_KEY | wimp_KEY_DOWN:
			case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
			case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
				free(url_complete_matched_string);
				url_complete_matched_string = NULL;
		}
	}


	/* get the text to match */
	url_complete_parent = parent;
	url = ro_toolbar_get_url(toolbar);
	match_url = (url != NULL) ? strdup(url) : NULL;
	if (match_url == NULL) {
		ro_gui_url_complete_close();
		return false;
	}

	/* if the text to match has changed then update it */
	if ((!url_complete_matched_string) ||
			(strcmp(match_url, url_complete_matched_string))) {

		/* memorize the current matches */
		lines = MAXIMUM_VISIBLE_LINES;
		if (lines > url_complete_matches_available)
			lines = url_complete_matches_available;
		if (url_complete_matches) {
			for (i = 0; i < MAXIMUM_VISIBLE_LINES; i++) {
				if (i < lines) {
					url_complete_redraw[i] =
						url_complete_matches[i];
				} else {
					url_complete_redraw[i] = NULL;
				}
			}
		}

		/* our selection gets wiped */
		error = xwimp_force_redraw(dialog_url_complete,
				0,
				-(url_complete_matches_selection + 1) * 44,
				65536, -url_complete_matches_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}

		/* clear our state */
		free(url_complete_original_url);
		free(url_complete_matched_string);
		url_complete_matched_string = match_url;
		url_complete_original_url = NULL;
		url_complete_matches_available = 0;
		url_complete_matches_selection = -1;
		url_complete_keypress_selection = -1;

		/* get some initial memory */
		if (!url_complete_matches) {
			url_complete_matches = malloc(64 * sizeof(char *));
			if (!url_complete_matches) {
				ro_gui_url_complete_close();
				return false;
			}
			url_complete_matches_allocated = 64;
		}

		/* find matches */
		url_complete_memory_exhausted = false;
		if (strlen(match_url) == 0)
			urldb_iterate_entries(url_complete_callback);
		else
			urldb_iterate_partial(match_url, url_complete_callback);
		if ((url_complete_memory_exhausted) ||
				(url_complete_matches_available == 0)) {
			ro_gui_url_complete_close();
			return false;
		}

		/* update the window */
		state.w = parent;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return false;
		}
		url_complete_matches_reset = true;
		ro_gui_url_complete_resize(toolbar, PTR_WIMP_OPEN(&state));
		url_complete_matches_reset = false;

		/* redraw the relevant bits of the window */
		lines = MAXIMUM_VISIBLE_LINES;
		if (lines > url_complete_matches_available)
			lines = url_complete_matches_available;
		for (i = 0; i < lines; i++) {
			if (url_complete_redraw[i] !=
					url_complete_matches[i]) {
				error = xwimp_force_redraw(dialog_url_complete,
					0, -(i + 1) * 44, 65536, -i * 44);
				if (error) {
					LOG(("xwimp_force_redraw: 0x%x: %s",
							error->errnum,
							error->errmess));
					warn_user("WimpError",
							error->errmess);
				}
			}
		}
	} else {
		free(match_url);
	}

	/* handle keypresses */
	if (!currently_open)
		return false;

	old_selection = url_complete_matches_selection;

	switch (key) {
		case IS_WIMP_KEY | wimp_KEY_UP:
			url_complete_matches_selection--;
			break;
		case IS_WIMP_KEY | wimp_KEY_DOWN:
			url_complete_matches_selection++;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_UP:
			url_complete_matches_selection -=
					MAXIMUM_VISIBLE_LINES;
			break;
		case IS_WIMP_KEY | wimp_KEY_PAGE_DOWN:
			url_complete_matches_selection +=
					MAXIMUM_VISIBLE_LINES;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_UP:
			url_complete_matches_selection = 0;
			break;
		case IS_WIMP_KEY | wimp_KEY_CONTROL | wimp_KEY_DOWN:
			url_complete_matches_selection = 65536;
			break;
	}

	if (url_complete_matches_selection >
			url_complete_matches_available - 1)
		url_complete_matches_selection =
				url_complete_matches_available - 1;
	else if (url_complete_matches_selection < -1)
		url_complete_matches_selection = -1;

	if (old_selection == url_complete_matches_selection)
		return false;

	error = xwimp_force_redraw(dialog_url_complete,
			0, -(old_selection + 1) * 44,
			65536, -old_selection * 44);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_force_redraw(dialog_url_complete,
			0, -(url_complete_matches_selection + 1) * 44,
			65536, -url_complete_matches_selection * 44);
	if (error) {
		LOG(("xwimp_force_redraw: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (old_selection == -1) {
		free(url_complete_original_url);
		url_complete_original_url = malloc(strlen(url) + 1);
		if (!url_complete_original_url)
			return false;
		strcpy(url_complete_original_url, url);
	}

	if (url_complete_matches_selection == -1) {
		ro_toolbar_set_url(toolbar,
				url_complete_original_url, true, false);
	} else {
		ro_toolbar_set_url(toolbar,
				nsurl_access(url_complete_matches[
					url_complete_matches_selection]),
				true, false);
		free(url_complete_matched_string);
		url_complete_matched_string = strdup(nsurl_access(
					url_complete_matches[
					url_complete_matches_selection]));
	}
	url_complete_keypress_selection = url_complete_matches_selection;

	/* auto-scroll */
	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return true;
	}

	if (state.yscroll < -(url_complete_matches_selection * 44))
		state.yscroll = -(url_complete_matches_selection * 44);
	height = state.visible.y1 - state.visible.y0;
	if (state.yscroll - height >
			-((url_complete_matches_selection + 1) * 44))
		state.yscroll =
			-((url_complete_matches_selection + 1) * 44) + height;

	error = xwimp_open_window(PTR_WIMP_OPEN(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return true;
	}

	return true;
}


/**
 * Callback function for urldb_iterate_partial
 *
 * \param url URL which matches
 * \param data Data associated with URL
 * \return true to continue iteration, false otherwise
 */

bool url_complete_callback(nsurl *url, const struct url_data *data)
{
	nsurl **array_extend;

	/* Ignore unvisited URLs */
	if (data->visits == 0)
		return true;

	url_complete_matches_available++;

	if (url_complete_matches_available >
			url_complete_matches_allocated) {

		array_extend = (nsurl **)realloc(url_complete_matches,
				(url_complete_matches_allocated + 64) *
				sizeof(nsurl *));
		if (!array_extend) {
			url_complete_memory_exhausted = true;
			return false;
		}
		url_complete_matches = array_extend;
		url_complete_matches_allocated += 64;
	}

	url_complete_matches[url_complete_matches_available - 1] = url;

	return true;
}


/* This is an exported interface documented in url_complete.h */

void ro_gui_url_complete_resize(struct toolbar *toolbar, wimp_open *open)
{
	os_box			extent = { 0, 0, 0, 0 };
	os_box			url_extent;
	wimp_window_state	toolbar_state;
	wimp_window_state	state;
	os_error		*error;
	int			lines;
	int			scroll_v = 0;

	/* only react to our window */
	if (open->w != url_complete_parent)
		return;

	/* if there is no toolbar, or there is no URL bar shown,
	 * or there are no URL matches, close it */
	if (!ro_toolbar_get_display_url(toolbar) ||
			(!url_complete_matches) ||
			(url_complete_matches_available == 0)) {
		ro_gui_url_complete_close();
		return;
	}

	/* get our current auto-complete window state for the scroll values */
	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (url_complete_matches_reset)
		state.yscroll = 0;

	/* move the window to the correct position */
	toolbar_state.w = ro_toolbar_get_window(toolbar);
	error = xwimp_get_window_state(&toolbar_state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	if (!ro_toolbar_get_url_field_extent(toolbar, &url_extent)) {
		LOG(("Failed to read URL field extent."));
		return;
	}

	lines = url_complete_matches_available;
	extent.y0 = -(lines * 44);
	extent.x1 = 65536;
	error = xwimp_set_extent(dialog_url_complete, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	state.next = open->next;
	state.flags &= ~wimp_WINDOW_VSCROLL;
	state.flags &= ~(4095 << 16); /* clear bits 16-27 */
	if (lines > MAXIMUM_VISIBLE_LINES) {
		lines = MAXIMUM_VISIBLE_LINES;
		scroll_v = ro_get_vscroll_width(NULL) - 2;
		state.flags |= wimp_WINDOW_VSCROLL;
	}
	state.visible.x0 = open->visible.x0 + 2 + url_extent.x0;
	state.visible.x1 = open->visible.x0 - 2 + url_extent.x1 - scroll_v;
	state.visible.y1 = open->visible.y1 - url_extent.y1 + 2;
	state.visible.y0 = state.visible.y1 - (lines * 44);
	if (state.visible.x1 + scroll_v > toolbar_state.visible.x1)
		state.visible.x1 = toolbar_state.visible.x1 - scroll_v;
	if (state.visible.x1 - state.visible.x0 < 0) {
		error = xwimp_close_window(dialog_url_complete);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	} else {
		error = xwimp_open_window_nested_with_flags(&state,
				(wimp_w)-1, 0);
		if (error) {
			LOG(("xwimp_open_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		open->next = dialog_url_complete;
	}
}


/* This is an exported interface documented in url_complete.h */

bool ro_gui_url_complete_close(void)
{
	os_error	*error;
	bool		currently_open;

	/* There used to be a check here to see if the icon clicked was the
	 * URL text field in the toolbar.  Since this only applied to clicks
	 * originating from the toolbar module following the restructuring,
	 * and this check was better done within the toolbar, it has been
	 * removed from this function and the associated parameters removed.
	 */

	currently_open = ((url_complete_parent != NULL) &&
			(url_complete_matches_available > 0));

	free(url_complete_matches);
	free(url_complete_matched_string);
	free(url_complete_original_url);
	url_complete_matches = NULL;
	url_complete_matched_string = NULL;
	url_complete_original_url = NULL;
	url_complete_matches_allocated = 0;
	url_complete_matches_available = 0;
	url_complete_keypress_selection = -1;
	url_complete_matches_selection = -1;
	url_complete_parent = 0;

	error = xwimp_close_window(dialog_url_complete);
	if (error) {
		LOG(("xwimp_close_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	return currently_open;
}


/* This is an exported interface documented in url_complete.h */

void ro_gui_url_complete_redraw(wimp_draw *redraw)
{
	osbool more;
	os_error *error;
	int clip_y0, clip_y1, origin_y;
	int first_line, last_line, line;
	const struct url_data *data;
	int type;

	/* initialise our icon */
	url_complete_icon.flags = wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
			wimp_ICON_TEXT | wimp_ICON_FILLED |
			(wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT) |
			(wimp_COLOUR_WHITE << wimp_ICON_BG_COLOUR_SHIFT);
	url_complete_icon.extent.x0 = 50;
	url_complete_icon.extent.x1 = 16384;
	url_complete_icon.data.indirected_text.validation =
						url_complete_icon_null;
	url_complete_sprite.flags = wimp_ICON_TEXT | wimp_ICON_SPRITE |
				wimp_ICON_INDIRECTED | wimp_ICON_FILLED |
				wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
	url_complete_sprite.extent.x0 = 0;
	url_complete_sprite.extent.x1 = 50;
	url_complete_sprite.data.indirected_text.text =
						url_complete_icon_null;
	url_complete_sprite.data.indirected_text.validation =
						url_complete_icon_sprite;
	url_complete_sprite.data.indirected_text.size = 1;

	/* no matches? no redraw */
	if (!url_complete_matches) {
		LOG(("Attempt to redraw with no matches made"));
		/* Fill is never used, so make it something obvious */
		ro_gui_user_redraw(redraw, false, os_COLOUR_BLACK);
		return;
	}

	/* redraw */
	more = wimp_redraw_window(redraw);
	while (more) {
		origin_y = redraw->box.y1 - redraw->yscroll;
		clip_y0 = redraw->clip.y0 - origin_y;
		clip_y1 = redraw->clip.y1 - origin_y;

		first_line = (-clip_y1) / 44;
		last_line = (-clip_y0 + 43) / 44;

		for (line = first_line; line < last_line; line++) {
			if (line == url_complete_matches_selection)
				url_complete_icon.flags |=
							wimp_ICON_SELECTED;
			else
				url_complete_icon.flags &=
							~wimp_ICON_SELECTED;
			url_complete_icon.extent.y1 = -line * 44;
			url_complete_icon.extent.y0 = -(line + 1) * 44;
			url_complete_icon.data.indirected_text.text =
					(char *)nsurl_access(
						url_complete_matches[line]);
			url_complete_icon.data.indirected_text.size =
					nsurl_length(
						url_complete_matches[line]);

			error = xwimp_plot_icon(&url_complete_icon);
			if (error) {
				LOG(("xwimp_plot_icon: 0x%x: %s",
						error->errnum,
						error->errmess));
				warn_user("WimpError", error->errmess);
			}

			data = urldb_get_url_data(url_complete_matches[line]);
			if (data)
				type = ro_content_filetype_from_type(
					data->type);
			else
				type = 0;

			sprintf(url_complete_icon_sprite, "Ssmall_%.3x",
					type);

			if (!ro_gui_wimp_sprite_exists(
					url_complete_icon_sprite + 1))
				sprintf(url_complete_icon_sprite,
						"Ssmall_xxx");
			url_complete_sprite.extent.y1 = -line * 44;
			url_complete_sprite.extent.y0 = -(line + 1) * 44;
			error = xwimp_plot_icon(&url_complete_sprite);
			if (error) {
				LOG(("xwimp_plot_icon: 0x%x: %s",
						error->errnum,
						error->errmess));
				warn_user("WimpError", error->errmess);
			}
		}
		more = wimp_get_rectangle(redraw);
	}
}


/* This is an exported interface documented in url_complete.h */

void ro_gui_url_complete_entering(wimp_entering *entering)
{
	ro_mouse_track_start(NULL, ro_gui_url_complete_mouse_at, NULL);
}


/**
 * Handle mouse movement over the URL completion window.
 *
 * \param *pointer	The pointer state
 * \param *data		NULL data pointer expected by mouse tracker
 */

void ro_gui_url_complete_mouse_at(wimp_pointer *pointer, void *data)
{
	wimp_mouse_state current;

	current = pointer->buttons;
	pointer->buttons = 0;
	ro_gui_url_complete_click(pointer);
	pointer->buttons = current;
}


/* This is an exported interface documented in url_complete.h */

bool ro_gui_url_complete_click(wimp_pointer *pointer)
{
	wimp_window_state state;
	os_error *error;
	int selection, old_selection;
	struct gui_window *g;
	const char *url;

	if ((mouse_x == pointer->pos.x) && (mouse_y == pointer->pos.y) &&
			(!pointer->buttons))
		return false;

	mouse_x = pointer->pos.x;
	mouse_y = pointer->pos.y;

	state.w = dialog_url_complete;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	selection = (state.visible.y1 - pointer->pos.y - state.yscroll) / 44;
	if (selection != url_complete_matches_selection) {
		if (url_complete_matches_selection == -1) {
			g = ro_gui_window_lookup(url_complete_parent);
			if (!g)
				return false;
			url = ro_toolbar_get_url(g->toolbar);
			free(url_complete_original_url);
			url_complete_original_url = strdup(url);
			if (!url_complete_original_url)
				return false;
		}
		old_selection = url_complete_matches_selection;
		url_complete_matches_selection = selection;
		error = xwimp_force_redraw(dialog_url_complete,
				0, -(old_selection + 1) * 44,
				65536, -old_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		error = xwimp_force_redraw(dialog_url_complete,
				0, -(url_complete_matches_selection + 1) * 44,
				65536, -url_complete_matches_selection * 44);
		if (error) {
			LOG(("xwimp_force_redraw: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
	}
	if (!pointer->buttons)
		return true;

	/* find owning window */
	g = ro_gui_window_lookup(url_complete_parent);
	if (!g)
		return false;

	/* Select sets the text and launches */
	if (pointer->buttons == wimp_CLICK_SELECT) {
		ro_toolbar_set_url(g->toolbar,
				nsurl_access(url_complete_matches[
					url_complete_matches_selection]),
				true, false);

		/** \todo The interaction of components here is hideous */
		/* Do NOT make any attempt to use any of the global url
		 * completion variables after this call to browser_window_navigate.
		 * They will be invalidated by (at least):
		 *   + gui_window_set_url
		 *   + destruction of (i)frames within the current page
		 * Any attempt to use them will probably result in a crash.
		 */

		browser_window_navigate(g->bw,
			url_complete_matches[url_complete_matches_selection],
			NULL,
			BW_NAVIGATE_HISTORY,
			NULL,
			NULL,
			NULL);

		ro_gui_url_complete_close();

	/* Adjust just sets the text */
	} else if (pointer->buttons == wimp_CLICK_ADJUST) {
		ro_toolbar_set_url(g->toolbar,
				nsurl_access(url_complete_matches[
					url_complete_matches_selection]),
				true, false);
		ro_gui_url_complete_keypress(g->toolbar, 0);
	}
	return true;
}

