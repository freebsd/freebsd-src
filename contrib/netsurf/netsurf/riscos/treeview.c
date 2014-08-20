/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 * Copyright 2010 Stephen Fryatt <stevef@netsurf-browser.org>
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
 * Generic tree handling (implementation).
 */

#include <oslib/os.h>

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <swis.h>
#include <time.h>
#include "oslib/colourtrans.h"
#include "oslib/dragasprite.h"
#include "oslib/osbyte.h"
#include "oslib/osspriteop.h"
#include "oslib/wimp.h"
#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "desktop/tree.h"
#include "riscos/bitmap.h"
#include "riscos/dialog.h"
#include "riscos/gui.h"
#include "riscos/image.h"
#include "riscos/menus.h"
#include "riscos/mouse.h"
#include "riscos/toolbar.h"
#include "riscos/tinct.h"
#include "riscos/textarea.h"
#include "riscos/treeview.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#ifndef wimp_KEY_END
#define wimp_KEY_END wimp_KEY_COPY
#endif

struct ro_treeview
{
	struct tree *tree;	/*< Pointer to treeview tree block.        */
	wimp_w w;		/*< RO Window Handle for tree window.      */
	struct toolbar	*tb;	/*< Pointer to toolbar block.              */
	struct {
		int x;		/*< X origin of tree, to RO work area.     */
		int y;		/*< Y origin of tree, to RO work area.     */
	} origin;
	struct {
		int x;		/*< X dimension of the tree, in RO units.  */
		int y;		/*< Y dimension of the tree, in RO units.  */
	} size;			/* (Dimensions are 0 until set correctly). */
	struct {
		int x;		/*< X extent of the window, in RO units.   */
		int y;		/*< Y extent of the window, in RO units.   */
	} extent;		/* (Extents are 0 until set correctly).    */
	struct {
		int x;		/*< X coordinate of drag start             */
		int y;		/*< Y coordinate of drag start             */
	} drag_start;
	tree_drag_type drag;	/*< The current drag type for the tree     */
	struct ro_treeview_callbacks *callbacks;	/*< Callback handlers */
};

static void ro_treeview_redraw_request(int x, int y, int width, int height,
		void *pw);
static void ro_treeview_resized(struct tree *tree, int width, int height,
		void *pw);
static void ro_treeview_scroll_visible(int y, int height, void *pw);
static void ro_treeview_get_window_dimensions(int *width, int *height,
		void *pw);

static void ro_treeview_redraw(wimp_draw *redraw);
static void ro_treeview_scroll(wimp_scroll *scroll);
static void ro_treeview_redraw_loop(wimp_draw *redraw, ro_treeview *tv,
		osbool more);
static void ro_treeview_open(wimp_open *open);
static bool ro_treeview_mouse_click(wimp_pointer *pointer);
static void ro_treeview_pointer_entering(wimp_entering *entering);
static void ro_treeview_drag_start(ro_treeview *tv, wimp_pointer *pointer,
		wimp_window_state *state);
static void ro_treeview_drag_end(wimp_dragged *drag, void *data);
static bool ro_treeview_keypress(wimp_key *key);

static void ro_treeview_set_window_extent(ro_treeview *tv,
		int width, int height);

static void ro_treeview_update_theme(void *data, bool ok);
static void ro_treeview_update_toolbar(void *data);
static void ro_treeview_button_update(void *data);
static void ro_treeview_save_toolbar_buttons(void *data, char *config);
static void ro_treeview_button_click(void *data,
		toolbar_action_type action_type, union toolbar_action action);

static const struct treeview_table ro_tree_callbacks = {
	ro_treeview_redraw_request,
	ro_treeview_resized,
	ro_treeview_scroll_visible,
	ro_treeview_get_window_dimensions
};

static const struct toolbar_callbacks ro_treeview_toolbar_callbacks = {
	ro_treeview_update_theme,
	ro_treeview_update_toolbar,
	ro_treeview_button_update,
	ro_treeview_button_click,
	NULL,				/* No toolbar keypress handler    */
	ro_treeview_save_toolbar_buttons
};


/**
 * Create a RISC OS GUI implementation of a treeview tree.
 *
 * \param  window		The window to create the tree in.
 * \param  *toolbar		A toolbar to attach to the window.
 * \param  *callbacks		Callbacks to service the treeview.
 * \param  flags		The treeview flags.
 *
 * \return			The RISC OS treeview pointer.
 */

ro_treeview *ro_treeview_create(wimp_w window, struct toolbar *toolbar,
		struct ro_treeview_callbacks *callbacks, unsigned int flags)
{
	ro_treeview *tv;

	/* Claim memory for the treeview block, and create a tree. */

	tv = malloc(sizeof(ro_treeview));
	if (tv == NULL)
		return NULL;

	tv->w = window;
	tv->tb = toolbar;

	/* Set the tree redraw origin at a default 0,0 RO units. */

	tv->origin.x = 0;
	tv->origin.y = 0;

	/* Set the tree size as 0,0 to indicate that we don't know. */

	tv->size.x = 0;
	tv->size.y = 0;

	/* Set the tree window extent to 0,0, to indicate that we
	 * don't know. */

	tv->extent.x = 0;
	tv->extent.y = 0;

	/* Set that there is no drag opperation at the moment */

	tv->drag = TREE_NO_DRAG;

	tv->tree = tree_create(flags, &ro_tree_callbacks, tv);
	if (tv->tree == NULL) {
		free(tv);
		return NULL;
	}

	/* Record the callback info. */

	tv->callbacks = callbacks;

	/* Register wimp events to handle the supplied window. */

	ro_gui_wimp_event_register_redraw_window(tv->w, ro_treeview_redraw);
	ro_gui_wimp_event_register_scroll_window(tv->w, ro_treeview_scroll);
	ro_gui_wimp_event_register_pointer_entering_window(tv->w,
			ro_treeview_pointer_entering);
	ro_gui_wimp_event_register_open_window(tv->w, ro_treeview_open);
	ro_gui_wimp_event_register_mouse_click(tv->w, ro_treeview_mouse_click);
	ro_gui_wimp_event_register_keypress(tv->w, ro_treeview_keypress);
	ro_gui_wimp_event_set_user_data(tv->w, tv);

	return tv;
}

/**
 * Delete a RISC OS GUI implementation of a treeview tree.  The window is
 * *not* destroyed -- this must be done by the caller.
 *
 * \param  tv			The RISC OS treeview to delete.
 */

void ro_treeview_destroy(ro_treeview *tv)
{
	ro_gui_wimp_event_finalise(tv->w);

	tree_delete(tv->tree);

	free(tv);
}

/**
 * Return a pointer to a toolbar callbacks structure with the handlers to be
 * used by any treeview window toolbars.
 *
 * \return			A pointer to the callback structure.
 */

const struct toolbar_callbacks *ro_treeview_get_toolbar_callbacks(void)
{
	return &ro_treeview_toolbar_callbacks;
}

/**
 * Change the redraw origin of a treeview tree in RISC OS graphics units.
 *
 * \param  *tv		The ro_treeview object to update.
 * \param  x		The X position, in terms of the RO window work area.
 * \param  y		The Y position, in terms of the RO window work area.
 *
 * \todo -- this probably needs a rework.
 */

void ro_treeview_set_origin(ro_treeview *tv, int x, int y)
{
	if (tv != NULL) {
		tv->origin.x = x;
		tv->origin.y = y;

		/* Assuming that we know how big the tree currently is, then
		 * adjust the window work area extent to match.  If we don't,
		 * then presumably the tree isn't in an open window yet and
		 * a subsequent Open Window Event should pick it up.
		 */

		if (tv->size.x != 0 && tv->size.y != 0)
			ro_treeview_set_window_extent(tv,
					tv->origin.x + tv->size.x,
					tv->origin.y + tv->size.y);
	}
}

/**
 * Return details of the tree block associated with an ro_treeview object.
 *
 * \param  *tv		The ro_treeview object of interest.
 * \return		A pointer to the associated tree block.
 */

struct tree *ro_treeview_get_tree(ro_treeview *tv)
{
	return (tv != NULL) ? (tv->tree) : (NULL);
}

/**
 * Return details of the RISC OS window handle associated with an
 * ro_treeview object.
 *
 * \param  *tv		The ro_treeview object of interest.
 * \return		The associated RISC OS window handle.
 */

wimp_w ro_treeview_get_window(ro_treeview *tv)
{
	return (tv != NULL) ? (tv->w) : (NULL);
}

/**
 * Callback to force a redraw of part of the treeview window.
 *
 * \param  x		Min X Coordinate of area to be redrawn.
 * \param  y		Min Y Coordinate of area to be redrawn.
 * \param  width	Width of area to be redrawn.
 * \param  height	Height of area to be redrawn.
 * \param  pw		The treeview object to be redrawn.
 */

void ro_treeview_redraw_request(int x, int y, int width, int height,
		void *pw)
{
	if (pw != NULL) {
		ro_treeview		*tv = (ro_treeview *) pw;
		os_error		*error;
		wimp_draw		update;
		osbool			more;

		update.w = tv->w;
		update.box.x0 = (2 * x) + tv->origin.x;
		update.box.y0 = (-2 * (y + height)) + tv->origin.y;
		update.box.x1 = (2 * (x + width)) + tv->origin.x;
		update.box.y1 = (-2 * y) + tv->origin.y;

		error = xwimp_update_window(&update, &more);
		if (error) {
			LOG(("xwimp_update_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
		ro_treeview_redraw_loop(&update, tv, more);
	}
}

/**
 * Pass RISC OS redraw events on to the treeview widget.
 *
 * \param  *redraw		Pointer to Redraw Event block.
 */

void ro_treeview_redraw(wimp_draw *redraw)
{
	osbool		more;
	os_error	*error;
	ro_treeview	*tv;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(redraw->w);
	if (tv == NULL) {
		LOG(("NULL treeview block for window: 0x%x",
				(unsigned int) redraw->w));
		/* Don't return, as not servicing redraw events isn't a good
		 * idea.  The following code must handle (tv == NULL)
		 * gracefully while clearing the redraw queue.
		 */
	}

	error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	ro_treeview_redraw_loop(redraw, tv, more);
}

/**
 * Handle scroll events in treeview windows.
 *
 * \param  *scroll		Pointer to Scroll Event block.
 */

void ro_treeview_scroll(wimp_scroll *scroll)
{
	os_error	*error;
	int		x = scroll->visible.x1 - scroll->visible.x0 - 32;
	int		y = scroll->visible.y1 - scroll->visible.y0 - 32;
	ro_treeview	*tv;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(scroll->w);
	if (tv == NULL)
		return;

	if (tv->tb != NULL)
		y -= ro_toolbar_full_height(tv->tb);

	switch (scroll->xmin) {
	case wimp_SCROLL_PAGE_LEFT:
		scroll->xscroll -= x;
		break;
	case wimp_SCROLL_COLUMN_LEFT:
		scroll->xscroll -= 32;
		break;
	case wimp_SCROLL_COLUMN_RIGHT:
		scroll->xscroll += 32;
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
		scroll->yscroll += 32;
		break;
	case wimp_SCROLL_LINE_DOWN:
		scroll->yscroll -= 32;
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


/**
 * Redraw a treeview window, once the initial readraw block has been collected.
 *
 * /param  *redraw		Pointer to redraw block.
 * /param  *tv			The treeview object being redrawn.
 * /param  more			Flag to show if more actions are required.
 */

void ro_treeview_redraw_loop(wimp_draw *redraw, ro_treeview *tv, osbool more)
{
	os_error *error;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &ro_plotters
	};

	while (more) {

		if (tv != NULL && tv->tree != NULL) {
			struct rect clip;

			ro_plot_origin_x = redraw->box.x0 + tv->origin.x -
					redraw->xscroll;
			ro_plot_origin_y = redraw->box.y1 + tv->origin.y -
					redraw->yscroll;

			clip.x0 = (redraw->clip.x0 - ro_plot_origin_x) / 2;
			clip.y0 = (ro_plot_origin_y - redraw->clip.y1) / 2;

			/* Treeview text alwyas has flat background colour,
			 * so disable unnecessary background blending */
			no_font_blending = true;
			tree_draw(tv->tree, 0, 0,
					clip.x0, clip.y0,
					(redraw->clip.x1 - redraw->clip.x0)/2,
					(redraw->clip.y1 - redraw->clip.y0)/2,
					&ctx);
			no_font_blending = false;
	 	}

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_redraw_window: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}
	}
}

/**
 * Callback to notify us of a new overall tree size.
 *
 * \param  tree		The tree being resized.
 * \param  width	The new width of the window.
 * \param  height	The new height of the window.
 * \param  *pw		The treeview object to be resized.
 */

void ro_treeview_resized(struct tree *tree, int width, int height,
		void *pw)
{
	if (pw != NULL) {
		ro_treeview		*tv = (ro_treeview *) pw;

		/* Store the width and height in terms of RISC OS work area. */

		tv->size.x = width * 2;
		tv->size.y = -(height * 2);

		/* Resize the window. */

		ro_treeview_set_window_extent(tv, tv->size.x, tv->size.y);
	}
}

/**
 * Callback to request that a section of the tree is scrolled into view.
 *
 * \param  y			The Y coordinate of top of the area in NS units.
 * \param  height		The height of the area in NS units.
 * \param  *pw			The treeview object affected.
 */

void ro_treeview_scroll_visible(int y, int height, void *pw)
{
	if (pw != NULL) {
		ro_treeview		*tv = (ro_treeview *) pw;
		os_error		*error;
		wimp_window_state	state;
		int			visible_t, visible_b;
		int			request_t, request_b;

		state.w = tv->w;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		/* Work out top and bottom of both the currently visible and
		 * the required areas, in terms of the RO work area.
		 */

		 visible_t = state.yscroll;
		 visible_b = state.yscroll
		 		- (state.visible.y1 - state.visible.y0);

		 request_t = -(2 * y);// - tv->origin.y;
		 request_b = -(2 * (y + height));// - tv->origin.y;

		 /* If the area is outside the visible window, then scroll it
		  * in to view.
		  */

		 if (request_t > visible_t || request_b < visible_b) {
		 	if (request_t > visible_t) {
		 		state.yscroll = request_t;
		 	} else if (request_b < visible_b) {
		 		state.yscroll = request_b + tv->origin.y
		 			+ (state.visible.y1 - state.visible.y0);

		 		/* If the required area is bigger than the
		 		 * visible extent, then align to the top and
		 		 * let the bottom disappear out of view.
		 		 */

		 		if (state.yscroll < request_t)
		 			state.yscroll = request_t;
		 	}

		 	error = xwimp_open_window((wimp_open *) &state);
		 	if (error) {
				LOG(("xwimp_open_window: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return;
			}
		}
	}
}

/**
 * Callback to return the tree window dimensions to the treeview system.
 *
 * \param  *width		Return the window width.
 * \param  *height		Return the window height.
 * \param  *pw			The treeview object to use.
 */

void ro_treeview_get_window_dimensions(int *width, int *height,
		void *pw)
{
	if (pw != NULL && (width != NULL || height != NULL)) {
		ro_treeview		*tv = (ro_treeview *) pw;
		os_error		*error;
		wimp_window_state	state;

		state.w = tv->w;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		if (width != NULL)
			*width = (state.visible.x1 - state.visible.x0) / 2;

		if (height != NULL)
			*height = (state.visible.y1 - state.visible.y0) / 2;
	}
}

/**
 * Resize the RISC OS window extent of a treeview.
 *
 * \param  *tv			The RISC OS treeview object to resize.
 * \param  width		The new width of the work area, in RO units.
 * \param  height		The new height of the work area, in RO units.
 */

void ro_treeview_set_window_extent(ro_treeview *tv, int width, int height)
{
	if (tv != NULL) {
		os_error		*error;
		os_box			extent;
		wimp_window_state	state;
		int			new_x, new_y;
		int			visible_x, visible_y;
		int			new_x_scroll, new_y_scroll;

		/* Calculate the new window extents, in RISC OS units. */

		new_x = width + tv->origin.x;
		new_y = height + tv->origin.y;

		/* Get details of the existing window, and start to sanity
		 * check the new extents.
		 */

		state.w = tv->w;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		/* If the extent is smaller than the current visible area,
		 * then extend it so that it matches the visible area.
		 */

		if (new_x < (state.visible.x1 - state.visible.x0))
			new_x = state.visible.x1 - state.visible.x0;

		if (new_y > (state.visible.y0 - state.visible.y1))
			new_y = state.visible.y0 - state.visible.y1;

		/* Calculate the maximum visible coordinates of the existing
		 * window.
		 */

		visible_x = state.xscroll +
				(state.visible.x1 - state.visible.x0);
		visible_y = state.yscroll +
				(state.visible.y0 - state.visible.y1);

		/* If the window is currently open, and the exising visible
		 * area is bigger than the new extent, then we need to reopen
		 * the window in an appropriare position before setting the
		 * new extent.
		 */

		if ((state.flags & wimp_WINDOW_OPEN) &&
				(visible_x > new_x || visible_y < new_y)) {
			new_x_scroll = state.xscroll;
			new_y_scroll = state.yscroll;

			if (visible_x > new_x)
				new_x_scroll = new_x - (state.visible.x1
						- state.visible.x0);

			if (visible_y < new_y)
				new_y_scroll = new_y - (state.visible.y0
						- state.visible.y1);

			if (new_x_scroll < 0) {
				state.visible.x1 -= new_x_scroll;
				state.xscroll = 0;
			} else {
				state.xscroll = new_x_scroll;
			}

			if (new_y_scroll > 0) {
				state.visible.y0 += new_y_scroll;
				state.yscroll = 0;
			} else {
				state.yscroll = new_y_scroll;
			}

			error = xwimp_open_window((wimp_open *) &state);
			if (error) {
				LOG(("xwimp_get_window_state: 0x%x: %s",
						error->errnum, error->errmess));
				warn_user("WimpError", error->errmess);
				return;
			}

			/* \todo -- Not sure if we need to reattach the
			 * toolbar here: the nested wimp seems to take care
			 * of it for us?
			 */
		}

		/* Now that the new extent fits into the visible window, we
		 * can resize the work area.  If we succeed, the values are
		 * recorded to save having to ask the Wimp for them
		 * each time.
		 */

		extent.x0 = 0;
		extent.y0 = new_y;
		extent.x1 = new_x;
		extent.y1 = 0;

		error = xwimp_set_extent(tv->w, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		tv->extent.x = new_x;
		tv->extent.y = new_y;
	}
}

/**
 * Handle RISC OS Window Open events for a treeview window.
 *
 * \param  *open		Pointer to the Window Open Event block.
 */

static void ro_treeview_open(wimp_open *open)
{
	ro_treeview	*tv;
	os_error	*error;
	os_box		extent;
	int		width, height;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(open->w);
	if (tv == NULL) {
		LOG(("NULL treeview block for window: ox%x",
				(unsigned int) open->w));
		return;
	}

	/* Calculate the window work area.  It must be at least the same as
	 * the current visible area of the window, and needs to contain the
	 * tree as defined by size.x + offset.x and size.y + offset.y (note
	 * that the offset.y should be set to cover any toolbar, so we can
	 * ignore the size of that).
	 */

	width = open->visible.x1 - open->visible.x0;
	height = open->visible.y0 - open->visible.y1;

	if (tv->size.x != 0 && width < (tv->origin.x + tv->size.x))
		width = (tv->origin.x + tv->size.x);

	if (tv->size.y != 0 && height > (tv->size.y + tv->origin.y))
		height = (tv->size.y + tv->origin.y);

	if (width != tv->extent.x || height != tv->extent.y) {
		extent.x0 = 0;
		extent.y0 = height;
		extent.x1 = width;
		extent.y1 = 0;

		error = xwimp_set_extent(tv->w, &extent);
		if (error) {
			LOG(("xwimp_set_extent: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
			return;
		}

		tv->extent.x = width;
		tv->extent.y = height;
	}

	/* \todo -- Might need to add vertical scrollbar hiding back in here? */

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	if (tv->tb)
		ro_toolbar_process(tv->tb, -1, false);
}


/**
 * Pass RISC OS Mouse Click events on to the treeview widget.
 *
 * \param  *pointer		Pointer to the Mouse Click Event block.
 * \return			Return true if click handled; else false.
 */

static bool ro_treeview_mouse_click(wimp_pointer *pointer)
{
	os_error		*error;
	ro_treeview		*tv;
	wimp_window_state	state;
	int			xpos, ypos;
	browser_mouse_state	mouse;
	bool			handled = false;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(pointer->w);
	if (tv == NULL) {
		LOG(("NULL treeview block for window: 0x%x",
				(unsigned int) pointer->w));
		return false;
	}

	state.w = tv->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return false;
	}

	/* Convert the returned mouse coordinates into NetSurf's internal
	 * units.
	 */

	xpos = ((pointer->pos.x - state.visible.x0) +
			state.xscroll - tv->origin.x) / 2;
	ypos = ((state.visible.y1 - pointer->pos.y) -
			state.yscroll + tv->origin.y) / 2;

	/* Start to process the mouse click.
	 *
	 * Select and Adjust are processed normally. To get filer-like operation
	 * with selections, Menu clicks are passed to the treeview first as
	 * Select if there are no selected nodes in the tree.
	 */

	mouse = 0;

	if (pointer->buttons == wimp_CLICK_MENU) {
		/* TODO: test for no selection, and pass click to select node */
		/* mouse |= BROWSER_MOUSE_CLICK_1; */
	} else {
		mouse = ro_gui_mouse_click_state(pointer->buttons,
				wimp_BUTTON_DOUBLE_CLICK_DRAG);

		/* Give the window input focus on Select-clicks.  This wouldn't
		 * be necessary if the core used the RISC OS caret.
		 */

		if (mouse & BROWSER_MOUSE_CLICK_1)
			xwimp_set_caret_position(tv->w, -1, -100, -100, 32, -1);
	}

	if (mouse != 0) {
		handled = tree_mouse_action(tv->tree, mouse, xpos, ypos);

		tv->drag = tree_drag_status(tv->tree);
		if (tv->drag != TREE_NO_DRAG) {
			tv->drag_start.x = xpos;
			tv->drag_start.y = ypos;
		}

		/* If it's a visible drag, start the RO side of the visible
		 * effects.
		 */

		if (tv->drag == TREE_SELECT_DRAG ||
				tv->drag == TREE_MOVE_DRAG)
			ro_treeview_drag_start(tv, pointer, &state);


		if (tv->callbacks != NULL &&
				tv->callbacks->toolbar_button_update != NULL)
			tv->callbacks->toolbar_button_update();
	}

	/* Special actions for some mouse buttons.  Adjust closes the dialog;
	 * Menu opens a menu.  For the latter, we assume that the owning module
	 * will have attached a window menu to our parent window with the auto
	 * flag unset (so that we can fudge the selection above).  If it hasn't,
	 * the call will quietly fail.
	 *
	 * \TODO -- Adjust-click close isn't a perfect copy of what the RO
	 *          version did: adjust clicks anywhere close the tree, and
	 *          selections persist.
	 */

	switch(pointer->buttons) {
	case wimp_CLICK_ADJUST:
		if (handled)
			ro_gui_dialog_close(tv->w);
		break;

	case wimp_CLICK_MENU:
		ro_gui_wimp_event_process_window_menu_click(pointer);
		break;
	}

	return true;
}


/**
 * Handle Pointer Entering Window events for treeview windows.
 *
 * \param *entering		The Wimp_PointerEnteringWindow block.
 */

void ro_treeview_pointer_entering(wimp_entering *entering)
{
	ro_treeview		*tv;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(entering->w);
	if (tv == NULL)
		return;

	ro_mouse_track_start(NULL, ro_treeview_mouse_at, NULL);
} 

/**
 * Track the mouse under Null Polls from the wimp, to support dragging.
 *
 * \param *pointer		Pointer to a Wimp Pointer block.
 * \param *data			NULL to allow use as a ro_mouse callback.
 */

void ro_treeview_mouse_at(wimp_pointer *pointer, void *data)
{
	os_error		*error;
	ro_treeview		*tv;
	wimp_window_state	state;
	int			xpos, ypos;
	browser_mouse_state	mouse;

	if (pointer->buttons & (wimp_CLICK_MENU))
		return;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(pointer->w);
	if (tv == NULL) {
		LOG(("NULL treeview block for window: 0x%x",
				(unsigned int) pointer->w));
		return;
	}

	if (tv->drag == TREE_NO_DRAG)
		return;

	/* We know now that it's not a Menu click and the treeview thinks
	 * that a drag is in progress.
	 */

	state.w = tv->w;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
		return;
	}

	/* Convert the returned mouse coordinates into NetSurf's internal
	 * units.
	 */

	xpos = ((pointer->pos.x - state.visible.x0) +
			state.xscroll - tv->origin.x) / 2;
	ypos = ((state.visible.y1 - pointer->pos.y) -
			state.yscroll + tv->origin.y) / 2;

	/* Start to process the mouse click. */

	mouse = ro_gui_mouse_drag_state(pointer->buttons,
			wimp_BUTTON_DOUBLE_CLICK_DRAG);

	tree_mouse_action(tv->tree, mouse, xpos, ypos);

	if (!(mouse & BROWSER_MOUSE_DRAG_ON)) {
		tree_drag_end(tv->tree, mouse, tv->drag_start.x,
				tv->drag_start.y, xpos, ypos);
		tv->drag = TREE_NO_DRAG;
	}

	if (tv->callbacks != NULL &&
			tv->callbacks->toolbar_button_update != NULL)
		tv->callbacks->toolbar_button_update();
}


/**
 * Start a RISC OS drag event to reflect on screen what is happening
 * during the core tree drag.
 *
 * \param *tv		The RO treeview to which the drag is attached.
 * \param *pointer	The RO pointer event data block starting the drag.
 * \param *state	The RO window state block for the treeview window.
 */

static void ro_treeview_drag_start(ro_treeview *tv, wimp_pointer *pointer,
		wimp_window_state *state)
{
	os_error		*error;
	wimp_drag		drag;
	wimp_auto_scroll_info	auto_scroll;

	drag.w = tv->w;
	drag.bbox.x0 = state->visible.x0;
	drag.bbox.y0 = state->visible.y0;
	drag.bbox.x1 = state->visible.x1;
	drag.bbox.y1 = state->visible.y1 - ro_toolbar_height(tv->tb) - 2;

	switch (tv->drag) {
	case TREE_SELECT_DRAG:
		drag.type = wimp_DRAG_USER_RUBBER;

		drag.initial.x0 = pointer->pos.x;
		drag.initial.y0 = pointer->pos.y;
		drag.initial.x1 = pointer->pos.x;
		drag.initial.y1 = pointer->pos.y;
		break;

	case TREE_MOVE_DRAG:
		drag.type = wimp_DRAG_USER_POINT;

		drag.initial.x0 = pointer->pos.x - 4;
		drag.initial.y0 = pointer->pos.y - 48;
		drag.initial.x1 = pointer->pos.x + 48;
		drag.initial.y1 = pointer->pos.y + 4;
		break;

	default:
		/* No other drag types are supported. */
		break;
	}

	LOG(("Drag start..."));

	error = xwimp_drag_box_with_flags(&drag,
			wimp_DRAG_BOX_KEEP_IN_LINE | wimp_DRAG_BOX_CLIP);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	} else {
		auto_scroll.w = tv->w;
		auto_scroll.pause_zone_sizes.x0 = 80;
		auto_scroll.pause_zone_sizes.y0 = 80;
		auto_scroll.pause_zone_sizes.x1 = 80;
		auto_scroll.pause_zone_sizes.y1 = 80 +
				ro_toolbar_height(tv->tb);
		auto_scroll.pause_duration = 0;
		auto_scroll.state_change = (void *) 1;

		error = xwimp_auto_scroll(wimp_AUTO_SCROLL_ENABLE_VERTICAL,
				&auto_scroll, NULL);
		if (error) {
			LOG(("xwimp_auto_scroll: 0x%x: %s",
					error->errnum, error->errmess));
			warn_user("WimpError", error->errmess);
		}
		
		ro_mouse_drag_start(ro_treeview_drag_end, ro_treeview_mouse_at,
				NULL, NULL);
	}
}


/**
 * Process RISC OS User Drag Box events which relate to us: in effect, drags
 * started by ro_treeview_drag_start().
 *
 * \param *drag			Pointer to the User Drag Box Event block.
 * \param *data			NULL to allow use as a ro_mouse callback.
 */

static void ro_treeview_drag_end(wimp_dragged *drag, void *data)
{
	os_error		*error;

	error = xwimp_drag_box((wimp_drag *) -1);
	if (error) {
		LOG(("xwimp_drag_box: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}

	error = xwimp_auto_scroll(0, NULL, NULL);
	if (error) {
		LOG(("xwimp_auto_scroll: 0x%x: %s",
				error->errnum, error->errmess));
		warn_user("WimpError", error->errmess);
	}
}


/**
 * Pass RISC OS keypress events on to the treeview widget.
 *
 * \param  *key			Pointer to the Key Pressed Event block.
 * \return			Return true if keypress handled; else false.
 */

static bool ro_treeview_keypress(wimp_key *key)
{
	ro_treeview		*tv;
	uint32_t		c;

	tv = (ro_treeview *) ro_gui_wimp_event_get_user_data(key->w);
	if (tv == NULL) {
		LOG(("NULL treeview block for window: 0x%x",
				(unsigned int) key->w));
		return false;
	}

	c = (uint32_t) key->c;

	if ((unsigned)c < 0x20 || (0x7f <= c && c <= 0x9f) ||
			(c & IS_WIMP_KEY)) {
	/* Munge control keys into unused control chars */
	/* We can't map onto 1->26 (reserved for ctrl+<qwerty>
	   That leaves 27->31 and 128->159 */
		switch (c & ~IS_WIMP_KEY) {
		case wimp_KEY_TAB: c = 9; break;
		case wimp_KEY_SHIFT | wimp_KEY_TAB: c = 11; break;

		/* cursor movement keys */
		case wimp_KEY_HOME:
		case wimp_KEY_CONTROL | wimp_KEY_LEFT:
			c = KEY_LINE_START;
			break;
		case wimp_KEY_END:
			if (os_version >= RISCOS5)
				c = KEY_LINE_END;
			else
				c = KEY_DELETE_RIGHT;
			break;
		case wimp_KEY_CONTROL | wimp_KEY_RIGHT: c = KEY_LINE_END;   break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:	c = KEY_TEXT_START; break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:  c = KEY_TEXT_END;   break;
		case wimp_KEY_SHIFT | wimp_KEY_LEFT:	c = KEY_WORD_LEFT ; break;
		case wimp_KEY_SHIFT | wimp_KEY_RIGHT:	c = KEY_WORD_RIGHT; break;
		case wimp_KEY_SHIFT | wimp_KEY_UP:	c = KEY_PAGE_UP;    break;
		case wimp_KEY_SHIFT | wimp_KEY_DOWN:	c = KEY_PAGE_DOWN;  break;
		case wimp_KEY_LEFT:  c = KEY_LEFT; break;
		case wimp_KEY_RIGHT: c = KEY_RIGHT; break;
		case wimp_KEY_UP:    c = KEY_UP; break;
		case wimp_KEY_DOWN:  c = KEY_DOWN; break;

		/* editing */
		case wimp_KEY_CONTROL | wimp_KEY_END:
			c = KEY_DELETE_LINE_END;
			break;
		case wimp_KEY_DELETE:
			if (ro_gui_ctrl_pressed())
				c = KEY_DELETE_LINE_START;
			else if (os_version < RISCOS5)
				c = KEY_DELETE_LEFT;
			break;

		default:
			break;
		}
	}

	if (!(c & IS_WIMP_KEY)) {
		if (tree_keypress(tv->tree, c)) {
			if (tv->callbacks &&
					tv->callbacks->toolbar_button_update
					!= NULL)
				tv->callbacks->toolbar_button_update();

			return true;
		}
	}

	return false;
}


/**
 * Update a treeview to use a new theme.
 *
 * \param *data			Pointer to the treeview to update.
 * \param ok			true if the bar still exists; else false.
 */

void ro_treeview_update_theme(void *data, bool ok)
{
	ro_treeview *tv = (ro_treeview *) data;

	if (tv != NULL && tv->tb != NULL){
		if (ok) {
			ro_treeview_update_toolbar(tv);
		} else {
			tv->tb = NULL;
		}
	}
}


/**
 * Change the size of a treeview's toolbar and redraw the window.
 *
 * \param *data			The treeview to update.
 */

void ro_treeview_update_toolbar(void *data)
{
	ro_treeview *tv = (ro_treeview *) data;

	if (tv != NULL && tv->tb != NULL) {
		ro_treeview_set_origin(tv, 0,
				-(ro_toolbar_height(tv->tb)));

		xwimp_force_redraw(tv->w, 0, tv->extent.y, tv->extent.x, 0);
	}
}


/**
 * Update the toolbar icons in a treeview window's toolbar.  As we're just
 * an intermediate widget, we pass the details on down the chain.
 *
 * \param *data			The treeview owning the toolbar.
 */

void ro_treeview_button_update(void *data)
{
	ro_treeview *tv = (ro_treeview *) data;

	if (tv == NULL || tv->callbacks == NULL)
		return;

	if (tv->callbacks->toolbar_button_update != NULL)
		tv->callbacks->toolbar_button_update();
}


/**
 * Save a new button configuration from a treeview window's toolbar.  As
 * we're just an intermediate widget, we pass the details on.
 *
 * \param *data			The treeview owning the toolbar.
 * \param *config		The new button config string.
 */

void ro_treeview_save_toolbar_buttons(void *data, char *config)
{
	ro_treeview *tv = (ro_treeview *) data;

	if (tv == NULL || tv->callbacks == NULL)
		return;

	if (tv->callbacks->toolbar_button_save != NULL)
		tv->callbacks->toolbar_button_save(config);
}


/**
 * Process clicks on buttons in a treeview window's toolbar.  As we're just
 * an intermediate widget, we just pass the details on down the chain.
 *
 * \param *data			The treeview owning the click.
 * \param action_type		The action type to be handled.
 * \param action		The action to handle.
 */

void ro_treeview_button_click(void *data,
		toolbar_action_type action_type, union toolbar_action action)
{
	ro_treeview *tv = (ro_treeview *) data;

	if (tv == NULL || tv->callbacks == NULL ||
			action_type != TOOLBAR_ACTION_BUTTON)
		return;

	if (tv->callbacks->toolbar_button_click != NULL)
		tv->callbacks->toolbar_button_click(action.button);

	if (tv->callbacks->toolbar_button_update != NULL)
		tv->callbacks->toolbar_button_update();
}


/**
 * Return a token identifying the interactive help message for a given cursor
 * position.
 *
 * Currently this is inimplemented.
 *
 * \param  *message_data	Pointer to the Wimp's help message block.
 * \return			Token value (-1 indicates no help available).
 */

int ro_treeview_get_help(help_full_message_request *message_data)
{
	return -1;
}

