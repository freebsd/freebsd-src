/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * UTF8 status bar (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "swis.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "desktop/plotters.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "riscos/gui.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "riscos/gui/progress_bar.h"
#include "riscos/gui/status_bar.h"

#define ICON_WIDGET 0
#define WIDGET_WIDTH 12
#define PROGRESS_WIDTH 160

struct status_bar {
	wimp_w w;			/**< status bar window handle */
	wimp_w parent;			/**< parent window handle */
	const char *text;		/**< status bar text */
	struct progress_bar *pb;	/**< progress bar */
	unsigned int scale;		/**< current status bar scale */
	int width;			/**< current status bar width */
	bool visible;			/**< status bar is visible? */
};

static char status_widget_text[] = "";
static char status_widget_validation[] = "R5;Pptr_lr,8,6";

wimp_WINDOW(1) status_bar_definition = {
	{0, 0, 1, 1},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_MOVEABLE |
			wimp_WINDOW_FURNITURE_WINDOW |
			wimp_WINDOW_IGNORE_XEXTENT,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	wimp_WINDOW_NEVER3D | 0x16u /* RISC OS 5.03+ */,
	{0, 0, 65535, 65535},
	0,
	0,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	1,
	{
	  	{
			{0, 0, 1, 1},
			wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
					wimp_ICON_BORDER | wimp_ICON_FILLED |
					(wimp_COLOUR_LIGHT_GREY <<
						wimp_ICON_BG_COLOUR_SHIFT) |
					(wimp_BUTTON_CLICK_DRAG <<
						wimp_ICON_BUTTON_TYPE_SHIFT),
			{
				.indirected_text = {
						status_widget_text,
				  		status_widget_validation,
			  			1
			  	}
			}
		}
	}
};

static void ro_gui_status_bar_open(wimp_open *open);
static bool ro_gui_status_bar_click(wimp_pointer *pointer);
static void ro_gui_status_bar_redraw(wimp_draw *redraw);
static void ro_gui_status_bar_redraw_callback(void *handle);
static void ro_gui_status_position_progress_bar(struct status_bar *sb);


/**
 * Create a new status bar
 *
 * \param  parent  the window to contain the status bar
 * \param  width  the proportional width to use (0...10,000)
 */
struct status_bar *ro_gui_status_bar_create(wimp_w parent, unsigned int width)
{
	struct status_bar *sb;
	os_error *error;

	sb = calloc(1, sizeof(*sb));
	if (!sb)
		return NULL;

	sb->pb = ro_gui_progress_bar_create();
	if (!sb->pb)
		return NULL;

	error = xwimp_create_window((wimp_window *)&status_bar_definition,
				&sb->w);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(sb);
		return NULL;
	}
	sb->parent = parent;
	sb->scale = width;
	sb->visible = true;

	ro_gui_wimp_event_set_user_data(sb->w, sb);
	ro_gui_wimp_event_register_open_window(sb->w,
			ro_gui_status_bar_open);
	ro_gui_wimp_event_register_mouse_click(sb->w,
			ro_gui_status_bar_click);
	ro_gui_wimp_event_register_redraw_window(sb->w,
			ro_gui_status_bar_redraw);
	ro_gui_wimp_event_set_help_prefix(sb->w, "HelpStatus");
	ro_gui_status_bar_resize(sb);
	return sb;
}


/**
 * Destroy a status bar and free all associated resources
 *
 * \param  sb  the status bar to destroy
 */
void ro_gui_status_bar_destroy(struct status_bar *sb)
{
	os_error *error;
	assert(sb);

	ro_gui_wimp_event_finalise(sb->w);
	error = xwimp_delete_window(sb->w);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x:%s",
			error->errnum, error->errmess));
	}

	ro_gui_progress_bar_destroy(sb->pb);

	/* Remove any scheduled redraw callbacks */
	riscos_schedule(-1, ro_gui_status_bar_redraw_callback, (void *) sb);

	free(sb);
}


/**
 * Get the handle of the window that represents a status bar
 *
 * \param  sb  the status bar to get the window handle of
 * \return the status bar's window handle
 */
wimp_w ro_gui_status_bar_get_window(struct status_bar *sb)
{
	assert(sb);

	return sb->w;
}


/**
 * Get the proportional width the status bar is currently using
 *
 * \param  sb  the status bar to get the width of
 * \return the status bar's width (0...10,000)
 */
unsigned int ro_gui_status_bar_get_width(struct status_bar *sb)
{
	assert(sb);

	return sb->scale;
}


/**
 * Set the visibility status of the status bar
 *
 * \param  sb  the status bar to check the visiblity of
 * \return whether the status bar is visible
 */
void ro_gui_status_bar_set_visible(struct status_bar *sb, bool visible)
{
	os_error *error;

	assert(sb);

	sb->visible = visible;
	if (visible) {
		ro_gui_status_bar_resize(sb);
	} else {
		error = xwimp_close_window(sb->w);
		if (error) {
			LOG(("xwimp_close_window: 0x%x:%s",
				error->errnum, error->errmess));
		}
	}
}


/**
 * Get the visibility status of the status bar
 *
 * \param  sb  the status bar to check the visiblity of
 * \return whether the status bar is visible
 */
bool ro_gui_status_bar_get_visible(struct status_bar *sb)
{
	assert(sb);

	return sb->visible;
}


/**
 * Set the value of the progress bar
 *
 * \param  pb  the status bar to set the progress of
 * \param  value  the value to use
 */
void ro_gui_status_bar_set_progress_value(struct status_bar *sb,
		unsigned int value)
{
	assert(sb);

	ro_gui_status_bar_set_progress_range(sb,
			max(value, ro_gui_progress_bar_get_range(sb->pb)));
	ro_gui_progress_bar_set_value(sb->pb, value);
}


/**
 * Set the range of the progress bar
 *
 * \param  pb  the status bar to set the range of
 * \param  value  the value to use, or 0 to turn off the progress bar
 */
void ro_gui_status_bar_set_progress_range(struct status_bar *sb,
		unsigned int range)
{
	unsigned int old_range;
	os_error *error;

	assert(sb);

	old_range = ro_gui_progress_bar_get_range(sb->pb);
	ro_gui_progress_bar_set_range(sb->pb, range);

	LOG(("Ranges are %i vs %i", old_range, range));
	if ((old_range == 0) && (range != 0)) {
		ro_gui_status_position_progress_bar(sb);
	} else if ((old_range != 0) && (range == 0)) {
		error = xwimp_close_window(
				ro_gui_progress_bar_get_window(sb->pb));
		if (error) {
			LOG(("xwimp_close_window: 0x%x:%s",
				error->errnum, error->errmess));
		}
	}
}


/**
 * Set the icon for the progress bar
 *
 * \param  pb  the status bar to set the icon for
 * \param  icon  the icon to use, or NULL for no icon
 */
void ro_gui_status_bar_set_progress_icon(struct status_bar *sb,
		const char *icon)
{
	assert(sb);

	ro_gui_progress_bar_set_icon(sb->pb, icon);
}


/**
 * Set the text to display in the status bar
 *
 * \param  text  the UTF8 text to display, or NULL for none
 */
void ro_gui_status_bar_set_text(struct status_bar *sb, const char *text)
{
	assert(sb);

	sb->text = text;

	/* Schedule a window redraw for 1cs' time.
	 * 
	 * We do this to ensure that redraws as a result of text changes
	 * do not prevent other applications obtaining CPU time.
	 *
	 * The scheduled callback will be run when we receive the first 
	 * null poll after 1cs has elapsed. It may then issue a redraw
	 * request to the Wimp.
	 *
	 * The scheduler ensures that only one instance of the 
	 * { callback, handle } pair is registered at once.
	 */
	if (sb->visible && text != NULL) {
		riscos_schedule(10, ro_gui_status_bar_redraw_callback, sb);
	}
}


/**
 * Resize a status bar following a change in the dimensions of the
 * parent window.
 *
 * \param  sb  the status bar to resize
 */
void ro_gui_status_bar_resize(struct status_bar *sb)
{
	int window_width, window_height;
	int status_width, status_height;
	int redraw_left, redraw_right;
	wimp_window_state state;
	os_error *error;
	os_box extent;

	if ((!sb) || (!sb->visible))
		return;

	/* get the window work area dimensions */
	state.w = sb->parent;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}
	window_width = state.visible.x1 - state.visible.x0;
	window_height = state.visible.y1 - state.visible.y0;


	/* recalculate the scaled width */
	status_width = (window_width * sb->scale) / 10000;
	if (status_width < WIDGET_WIDTH)
		status_width = WIDGET_WIDTH;
	status_height = ro_get_hscroll_height(sb->parent);

	/* resize the status/resize icons */
	if (status_width != sb->width) {
		/* update the window extent */
		extent.x0 = 0;
		extent.y0 = 0;
		extent.x1 = status_width;
		extent.y1 = status_height - 4;
		error = xwimp_set_extent(sb->w, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
			return;
		}

		/* re-open the nested window */
		state.w = sb->w;
		state.xscroll = 0;
		state.yscroll = 0;
		state.next = wimp_TOP;
		state.visible.x0 = state.visible.x0;
		state.visible.y1 = state.visible.y0 - 2;
		state.visible.x1 = state.visible.x0 + status_width;
		state.visible.y0 = state.visible.y1 - status_height + 4;
		error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state),
				sb->parent,
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_XORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_YORIGIN_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_LS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_BS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_RS_EDGE_SHIFT |
				wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
						<< wimp_CHILD_TS_EDGE_SHIFT);
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
			return;
		}
		ro_gui_status_position_progress_bar(sb);
		error = xwimp_resize_icon(sb->w, ICON_WIDGET,
				status_width - WIDGET_WIDTH, 0,
				status_width, status_height - 4);
		if (error) {
			LOG(("xwimp_resize_icon: 0x%x: %s",
				error->errnum, error->errmess));
			return;
		}

		redraw_left = min(status_width, sb->width) - WIDGET_WIDTH - 2;
		redraw_right = max(status_width, sb->width);
		xwimp_force_redraw(sb->w, redraw_left, 0,
				redraw_right, status_height);
		sb->width = status_width;
	}
}


/**
 * Process a WIMP redraw request
 *
 * \param  redraw  the redraw request to process
 */
void ro_gui_status_bar_redraw(wimp_draw *redraw)
{
	struct status_bar *sb;
	os_error *error;
	osbool more;
	rufl_code code;

	sb = (struct status_bar *)ro_gui_wimp_event_get_user_data(redraw->w);
	assert(sb);

	/* initialise the plotters */
	ro_plot_origin_x = 0;
	ro_plot_origin_y = 0;

	/* redraw the window */
	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
	while (more) {
		/* redraw the status text */
		if (sb->text) {
			error = xcolourtrans_set_font_colours(font_CURRENT,
					0xeeeeee00, 0x00000000, 14, 0, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
				return;
			}
			code = rufl_paint(ro_gui_desktop_font_family,
					ro_gui_desktop_font_style,
					ro_gui_desktop_font_size,
					sb->text, strlen(sb->text),
					redraw->box.x0 + 6, redraw->box.y0 + 8,
					rufl_BLEND_FONT);
			if (code != rufl_OK) {
				if (code == rufl_FONT_MANAGER_ERROR)
					LOG(("rufl_FONT_MANAGER_ERROR: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
				else
					LOG(("rufl_paint: 0x%x", code));
			}
		}

		/* separate the widget from the text with a line */
		ro_plotters.rectangle((redraw->box.x0 + sb->width - WIDGET_WIDTH - 2) >> 1,
				-redraw->box.y0 >> 1,
				(redraw->box.x0 + sb->width - WIDGET_WIDTH) >> 1,
				-redraw->box.y1 >> 1,
				plot_style_fill_black);

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}
}

/**
 * Callback for scheduled redraw
 *
 * \param handle  Callback handle
 */
void ro_gui_status_bar_redraw_callback(void *handle)
{
	struct status_bar *sb = handle;

	wimp_force_redraw(sb->w, 0, 0, sb->width - WIDGET_WIDTH, 65536);
}


/**
 * Process an mouse_click event for a status window.
 *
 * \param  pointer  details of the mouse click
 */
bool ro_gui_status_bar_click(wimp_pointer *pointer)
{
	wimp_drag drag;
	os_error *error;

	switch (pointer->i) {
		case ICON_WIDGET:
			drag.w = pointer->w;
			drag.type = wimp_DRAG_SYSTEM_SIZE;
			drag.initial.x0 = pointer->pos.x;
			drag.initial.x1 = pointer->pos.x;
			drag.initial.y0 = pointer->pos.y;
			drag.initial.y1 = pointer->pos.y;
			error = xwimp_drag_box(&drag);
			if (error) {
				LOG(("xwimp_drag_box: 0x%x: %s",
						error->errnum, error->errmess));
			}
			break;
	}
	return true;
}


/**
 * Process an open_window request for a status window.
 *
 * \param  open  the request to process
 */
void ro_gui_status_bar_open(wimp_open *open)
{
	struct status_bar *sb;
	int window_width, status_width;
	wimp_window_state state;
	os_error *error;

	/* get the parent width for scaling */
	sb = (struct status_bar *)ro_gui_wimp_event_get_user_data(open->w);
	state.w = sb->parent;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}
	window_width = state.visible.x1 - state.visible.x0;
	if (window_width == 0)
		window_width = 1;
	status_width = open->visible.x1 - open->visible.x0;
	if (status_width <= 12)
		status_width = 0;

	/* store the new size */
	sb->scale = (10000 * status_width) / window_width;
	if (sb->scale > 10000)
		sb->scale = 10000;
	ro_gui_status_bar_resize(sb);
}


/**
 * Reposition the progress component following a change in the
 * dimension of the status window.
 *
 * \param  sb  the status bar to update
 */
void ro_gui_status_position_progress_bar(struct status_bar *sb)
{
	wimp_window_state state;
	os_error *error;
	int left, right;

	if (!sb)
		return;
	if (ro_gui_progress_bar_get_range(sb->pb) == 0)
		return;

	/* get the window work area dimensions */
	state.w = sb->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
			error->errnum, error->errmess));
		return;
	}

	/* calculate the dimensions */
	right = state.visible.x1 - WIDGET_WIDTH - 2;
	left = max(state.visible.x0, right - PROGRESS_WIDTH);

	/* re-open the nested window */
	state.w = ro_gui_progress_bar_get_window(sb->pb);
	state.xscroll = 0;
	state.yscroll = 0;
	state.next = wimp_TOP;
	state.visible.x0 = left;
	state.visible.x1 = right;
	error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state),
			sb->w,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_BS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_RS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_TS_EDGE_SHIFT);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	/* update the progress bar display on non-standard width */
	if ((right - left) != PROGRESS_WIDTH)
		ro_gui_progress_bar_update(sb->pb, right - left,
				state.visible.y1 - state.visible.y0);
}
