/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
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
 * Browser history window (RISC OS implementation).
 *
 * There is only one history window, not one per browser window.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "oslib/wimp.h"
#include "desktop/browser_history.h"
#include "desktop/plotters.h"
#include "riscos/dialog.h"
#include "utils/nsoption.h"
#include "riscos/gui.h"
#include "riscos/mouse.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/url.h"
#include "utils/utils.h"


static struct browser_window *history_bw;
/* Last position of mouse in window. */
static int mouse_x = 0;
/* Last position of mouse in window. */
static int mouse_y = 0;
wimp_w history_window;

static void ro_gui_history_redraw(wimp_draw *redraw);
static bool ro_gui_history_click(wimp_pointer *pointer);
static void ro_gui_history_pointer_entering(wimp_entering *entering);
static void ro_gui_history_track_end(wimp_leaving *leaving, void *data);
static void ro_gui_history_mouse_at(wimp_pointer *pointer, void *data);


/**
 * Create history window.
 */

void ro_gui_history_init(void)
{
	history_window = ro_gui_dialog_create("history");
	ro_gui_wimp_event_register_redraw_window(history_window,
			ro_gui_history_redraw);
	ro_gui_wimp_event_register_mouse_click(history_window,
			ro_gui_history_click);
	ro_gui_wimp_event_register_pointer_entering_window(history_window,
			ro_gui_history_pointer_entering);
	ro_gui_wimp_event_set_help_prefix(history_window, "HelpHistory");
}


/**
 * Open history window.
 *
 * \param  bw          browser window to open history for
 * \param  history     history to open
 * \param  at_pointer  open the window at the pointer
 */

void ro_gui_history_open(struct gui_window *g, bool at_pointer)
{
	struct browser_window *bw;
	int width, height;
	os_box box = {0, 0, 0, 0};
	wimp_window_state state;
	os_error *error;

	assert(g != NULL);
	assert(g->bw != NULL);
	bw = g->bw;
	history_bw = bw;

	browser_window_history_size(bw, &width, &height);
	width *= 2;
	height *= 2;

	/* set extent */
	box.x1 = width;
	box.y0 = -height;
	error = xwimp_set_extent(history_window, &box);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* open full size */
	state.w = history_window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	state.visible.x0 = 0;
	state.visible.y0 = 0;
	state.visible.x1 = width;
	state.visible.y1 = height;
	state.next = wimp_HIDDEN;
	error = xwimp_open_window(PTR_WIMP_OPEN(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	ro_gui_dialog_open_persistent(g->window, history_window, at_pointer);
}


/**
 * Redraw history window.
 */

void ro_gui_history_redraw(wimp_draw *redraw)
{
	osbool more;
	os_error *error;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &ro_plotters
	};

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	while (more) {
		ro_plot_origin_x = redraw->box.x0 - redraw->xscroll;
		ro_plot_origin_y = redraw->box.y1 - redraw->yscroll;
		browser_window_history_redraw(history_bw, &ctx);
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
 * Handle Pointer Entering Window events the history window.
 *
 * \param *entering		The Wimp_PointerEnteringWindow block.
 */

void ro_gui_history_pointer_entering(wimp_entering *entering)
{
	ro_mouse_track_start(ro_gui_history_track_end,
			ro_gui_history_mouse_at, NULL);
} 


/**
 * Handle Pointer Leaving Window events the history window. These arrive as the
 * termination callback handler from ro_mouse's mouse tracking.
 *
 * \param *leaving		The Wimp_PointerLeavingWindow block.
 * \param *data			NULL data pointer.
 */

void ro_gui_history_track_end(wimp_leaving *leaving, void *data)
{
	ro_gui_dialog_close(dialog_tooltip);
} 


/**
 * Handle mouse movements over the history window.
 */

void ro_gui_history_mouse_at(wimp_pointer *pointer, void *data)
{
	int x, y;
	int width;
	const char *url;
	wimp_window_state state;
	wimp_icon_state ic;
	os_box box = {0, 0, 0, 0};
	os_error *error;
	
	LOG(("Mouse at..."));

	/* If the mouse hasn't moved, or if we don't want tooltips, exit */
	if ((mouse_x == pointer->pos.x && mouse_y == pointer->pos.y) ||
	    !nsoption_bool(history_tooltip))
		return;

	/* Update mouse position */
	mouse_x = pointer->pos.x;
	mouse_y = pointer->pos.y;

	/* Find history tree entry under mouse */
	state.w = history_window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / 2;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / 2;
	url = browser_window_history_position_url(history_bw, x, y);
	if (!url) {
		/* not over a tree entry => close tooltip window. */
		error = xwimp_close_window(dialog_tooltip);
		if (error) {
			LOG(("xwimp_close_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		return;
	}

	/* get width of string */
	error = xwimptextop_string_width(url,
			strlen(url) > 256 ? 256 : strlen(url),
			&width);
	if (error) {
		LOG(("xwimptextop_string_width: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	ro_gui_set_icon_string(dialog_tooltip, 0, url, true);

	/* resize icon appropriately */
	ic.w = dialog_tooltip;
	ic.i = 0;
	error = xwimp_get_icon_state(&ic);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
	error = xwimp_resize_icon(dialog_tooltip, 0,
			ic.icon.extent.x0, ic.icon.extent.y0,
			width + 16, ic.icon.extent.y1);
	if (error) {
		LOG(("xwimp_resize_icon: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	state.w = dialog_tooltip;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* update window extent */
	box.x1 = width + 16;
	box.y0 = -36;
	error = xwimp_set_extent(dialog_tooltip, &box);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* set visible area */
	state.visible.x0 = pointer->pos.x + 24;
	state.visible.y0 = pointer->pos.y - 22 - 36;
	state.visible.x1 = pointer->pos.x + 24 + width + 16;
	state.visible.y1 = pointer->pos.y - 22;
	state.next = wimp_TOP;
	/* open window */
	error = xwimp_open_window(PTR_WIMP_OPEN(&state));
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}
}


/**
 * Handle mouse clicks in the history window.
 *
 * \return true if the event was handled, false to pass it on
 */

bool ro_gui_history_click(wimp_pointer *pointer)
{
	int x, y;
	wimp_window_state state;
	os_error *error;

	if (pointer->buttons != wimp_CLICK_SELECT &&
			pointer->buttons != wimp_CLICK_ADJUST)
		/* return if not select or adjust click */
		return true;

	state.w = history_window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return true;
	}

	x = (pointer->pos.x - (state.visible.x0 - state.xscroll)) / 2;
	y = -(pointer->pos.y - (state.visible.y1 - state.yscroll)) / 2;
	browser_window_history_click(history_bw, x, y,
			pointer->buttons == wimp_CLICK_ADJUST);

	return true;
}
