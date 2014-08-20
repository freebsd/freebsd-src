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
 * Frame and frameset creation and manipulation (implementation).
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "utils/config.h"
#include "content/hlcache.h"
#include "desktop/browser_private.h"
#include "desktop/frames.h"
#include "desktop/scrollbar.h"
#include "desktop/selection.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "render/html.h"
#include "render/box.h"

/** maximum frame resize margin */
#define FRAME_RESIZE 6

static bool browser_window_resolve_frame_dimension(struct browser_window *bw,
		struct browser_window *sibling, int x, int y, bool width,
		bool height);


/**
 * Callback for (i)frame scrollbars.
 */
void browser_window_scroll_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data)
{
	struct browser_window *bw = client_data;

	switch(scrollbar_data->msg) {
	case SCROLLBAR_MSG_MOVED:
		if (bw->browser_window_type == BROWSER_WINDOW_IFRAME) {
			html_redraw_a_box(bw->parent->current_content, bw->box);
		} else {
			struct rect rect;

			rect.x0 = scrollbar_get_offset(bw->scroll_x);
			rect.y0 = scrollbar_get_offset(bw->scroll_y);
			rect.x1 = rect.x0 + bw->width;
			rect.y1 = rect.y0 + bw->height;

			browser_window_update_box(bw, &rect);
		}
		break;
	case SCROLLBAR_MSG_SCROLL_START:
	{
		struct rect rect = {
			.x0 = scrollbar_data->x0,
			.y0 = scrollbar_data->y0,
			.x1 = scrollbar_data->x1,
			.y1 = scrollbar_data->y1
		};

		if (scrollbar_is_horizontal(scrollbar_data->scrollbar))
			browser_window_set_drag_type(bw, DRAGGING_SCR_X, &rect);
		else
			browser_window_set_drag_type(bw, DRAGGING_SCR_Y, &rect);
	}
		break;
	case SCROLLBAR_MSG_SCROLL_FINISHED:
		browser_window_set_drag_type(bw, DRAGGING_NONE, NULL);

		browser_window_set_pointer(bw, BROWSER_POINTER_DEFAULT);
		break;
	}
}

/* exported interface, documented in browser.h */
void browser_window_handle_scrollbars(struct browser_window *bw)
{
	hlcache_handle *h = bw->current_content;
	bool scroll_x;
	bool scroll_y;
	int c_width = 0;
	int c_height = 0;

	assert(!bw->window); /* Core-handled windows only */

	if (h != NULL) {
		c_width  = content_get_width(h);
		c_height = content_get_height(h);
	}

	if (bw->scrolling == SCROLLING_YES) {
		scroll_x = true;
		scroll_y = true;
	} else if (bw->scrolling == SCROLLING_AUTO &&
			bw->current_content) {
		int bw_width = bw->width;
		int bw_height = bw->height;

		/* subtract existing scrollbar width */
		bw_width -= bw->scroll_y ? SCROLLBAR_WIDTH : 0;
		bw_height -= bw->scroll_x ? SCROLLBAR_WIDTH : 0;

		scroll_y = (c_height > bw_height) ? true : false;
		scroll_x = (c_width > bw_width) ? true : false;
	} else {
		/* No scrollbars */
		scroll_x = false;
		scroll_y = false;
	}

	if (!scroll_x && bw->scroll_x != NULL) {
		scrollbar_destroy(bw->scroll_x);
		bw->scroll_x = NULL;
	}

	if (!scroll_y && bw->scroll_y != NULL) {
		scrollbar_destroy(bw->scroll_y);
		bw->scroll_y = NULL;
	}

	if (scroll_y) {
		int length = bw->height;
		int visible = bw->height - (scroll_x ? SCROLLBAR_WIDTH : 0);

		if (bw->scroll_y == NULL) {
			/* create vertical scrollbar */
			if (!scrollbar_create(false, length, c_height, visible,
					bw, browser_window_scroll_callback,
					&(bw->scroll_y)))
				return;
		} else {
			/* update vertical scrollbar */
			scrollbar_set_extents(bw->scroll_y, length,
					visible, c_height);
		}
	}

	if (scroll_x) {
		int length = bw->width - (scroll_y ? SCROLLBAR_WIDTH : 0);
		int visible = length;

		if (bw->scroll_x == NULL) {
			/* create horizontal scrollbar */
			if (!scrollbar_create(true, length, c_width, visible,
					bw, browser_window_scroll_callback,
					&(bw->scroll_x)))
				return;
		} else {
			/* update horizontal scrollbar */
			scrollbar_set_extents(bw->scroll_x, length,
					visible, c_width);
		}
	}

	if (scroll_x && scroll_y)
		scrollbar_make_pair(bw->scroll_x, bw->scroll_y);
}


/**
 * Create and open a iframes for a browser window.
 *
 * \param  bw	    The browser window to create iframes for
 * \param  iframe   The iframes to create
 */

void browser_window_create_iframes(struct browser_window *bw,
		struct content_html_iframe *iframe)
{
	struct browser_window *window;
	struct content_html_iframe *cur;
	struct rect rect;
	int iframes = 0;
	int index;

	for (cur = iframe; cur; cur = cur->next)
		iframes++;
	bw->iframes = calloc(iframes, sizeof(*bw));
	if (!bw->iframes)
		return;
	bw->iframe_count = iframes;

	index = 0;
	for (cur = iframe; cur; cur = cur->next) {
		window = &(bw->iframes[index++]);

		/* Initialise common parts */
		browser_window_initialise_common(BW_CREATE_NONE,
				window, NULL);

		/* window characteristics */
		window->browser_window_type = BROWSER_WINDOW_IFRAME;
		window->scrolling = cur->scrolling;
		window->border = cur->border;
		window->border_colour = cur->border_colour;
		window->no_resize = true;
		window->margin_width = cur->margin_width;
		window->margin_height = cur->margin_height;
		window->scale = bw->scale;
		if (cur->name) {
			window->name = strdup(cur->name);
			if (!window->name)
				warn_user("NoMemory", 0);
		}

		/* linking */
		window->box = cur->box;
		window->parent = bw;
		window->box->iframe = window;

		/* iframe dimensions */
		box_bounds(window->box, &rect);

		browser_window_set_position(window, rect.x0, rect.y0);
		browser_window_set_dimensions(window, rect.x1 - rect.x0,
				rect.y1 - rect.y0);
	}

	/* calculate dimensions */
	browser_window_update_extent(bw);
	browser_window_recalculate_iframes(bw);

	index = 0;
	for (cur = iframe; cur; cur = cur->next) {
		window = &(bw->iframes[index++]);
		if (cur->url) {
			/* fetch iframe's content */
			browser_window_navigate(window, 
				cur->url,
				hlcache_handle_get_url(bw->current_content),
				BW_NAVIGATE_UNVERIFIABLE,
				NULL,
				NULL,
				bw->current_content);
		}
	}
}


/**
 * Recalculate iframe positions following a resize.
 *
 * \param  bw	    The browser window to reposition iframes for
 */

void browser_window_recalculate_iframes(struct browser_window *bw)
{
	struct browser_window *window;
	int index;

	for (index = 0; index < bw->iframe_count; index++) {
		window = &(bw->iframes[index]);

		if (window != NULL) {
			browser_window_handle_scrollbars(window);
		}
	}
}


/**
 * Create and open a frameset for a browser window.
 *
 * \param  bw	    The browser window to create the frameset for
 * \param  iframe   The frameset to create
 */

void browser_window_create_frameset(struct browser_window *bw,
		struct content_html_frames *frameset)
{
	int row, col, index;
	struct content_html_frames *frame;
	struct browser_window *window;
	hlcache_handle *parent;

	assert(bw && frameset);

	/* 1. Create children */
	assert(bw->children == NULL);
	assert(frameset->cols + frameset->rows != 0);

	bw->children = calloc((frameset->cols * frameset->rows), sizeof(*bw));
	if (!bw->children)
		return;
	bw->cols = frameset->cols;
	bw->rows = frameset->rows;
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			/* Initialise common parts */
			browser_window_initialise_common(BW_CREATE_NONE,
					window, NULL);

			/* window characteristics */
			if (frame->children)
				window->browser_window_type =
						BROWSER_WINDOW_FRAMESET;
			else
				window->browser_window_type =
						BROWSER_WINDOW_FRAME;
			window->scrolling = frame->scrolling;
			window->border = frame->border;
			window->border_colour = frame->border_colour;
			window->no_resize = frame->no_resize;
			window->frame_width = frame->width;
			window->frame_height = frame->height;
			window->margin_width = frame->margin_width;
			window->margin_height = frame->margin_height;
			if (frame->name) {
				window->name = strdup(frame->name);
				if (!window->name)
					warn_user("NoMemory", 0);
			}

			window->scale = bw->scale;

			/* linking */
			window->parent = bw;

			if (window->name)
				LOG(("Created frame '%s'", window->name));
			else
				LOG(("Created frame (unnamed)"));
		}
	}

	/* 2. Calculate dimensions */
	browser_window_update_extent(bw);
	browser_window_recalculate_frameset(bw);

	/* 3. Recurse for grandchildren */
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			if (frame->children)
				browser_window_create_frameset(window, frame);
		}
	}

	/* Use the URL of the first ancestor window containing html content
	 * as the referer */
	for (window = bw; window->parent; window = window->parent) {
		if (window->current_content && 
				content_get_type(window->current_content) == 
				CONTENT_HTML)
			break;
	}

	parent = window->current_content;

	/* 4. Launch content */
	for (row = 0; row < bw->rows; row++) {
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			frame = &frameset->children[index];
			window = &bw->children[index];

			if (frame->url) {
				browser_window_navigate(window,
					frame->url,
					hlcache_handle_get_url(parent),
					BW_NAVIGATE_HISTORY |
					BW_NAVIGATE_UNVERIFIABLE,
					NULL,
					NULL,
					parent);
			}
		}
	}
}


/**
 * Recalculate frameset positions following a resize.
 *
 * \param  bw	    The browser window to reposition framesets for
 */

void browser_window_recalculate_frameset(struct browser_window *bw)
{
	int widths[bw->cols][bw->rows];
	int heights[bw->cols][bw->rows];
	int bw_width, bw_height;
	int avail_width, avail_height;
	int row, row2, col, index;
	struct browser_window *window;
	float relative;
	int size, extent, applied;
	int x, y;
	int new_width, new_height;

	assert(bw);

	/* window dimensions */
	if (!bw->parent) {
		browser_window_get_dimensions(bw, &bw_width, &bw_height, true);
		bw->x = 0;
		bw->y = 0;
		bw->width = bw_width;
		bw->height = bw_height;
	} else {
		bw_width = bw->width;
		bw_height = bw->height;
	}
	bw_width++;
	bw_height++;

	/* widths */
	for (row = 0; row < bw->rows; row++) {
		avail_width = bw_width;
		relative = 0;
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			switch (window->frame_width.unit) {
			case FRAME_DIMENSION_PIXELS:
				widths[col][row] = window->frame_width.value *
						window->scale;
				if (window->border) {
					if (col != 0)
						widths[col][row] += 1;
					if (col != bw->cols - 1)
						widths[col][row] += 1;
				}
				break;
			case FRAME_DIMENSION_PERCENT:
				widths[col][row] = bw_width *
						window->frame_width.value / 100;
				break;
			case FRAME_DIMENSION_RELATIVE:
				widths[col][row] = 0;
				relative += window->frame_width.value;
				break;
			default:
				/* unknown frame dimension unit */
				assert(window->frame_width.unit ==
						FRAME_DIMENSION_PIXELS ||
						window->frame_width.unit ==
						FRAME_DIMENSION_PERCENT ||
						window->frame_width.unit ==
						FRAME_DIMENSION_RELATIVE);
				break;
			}
			avail_width -= widths[col][row];
		}

		/* Redistribute to fit window */
		if ((relative > 0) && (avail_width > 0)) {
			/* Expand the relative sections to fill remainder */
			for (col = 0; col < bw->cols; col++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (window->frame_width.unit ==
						FRAME_DIMENSION_RELATIVE) {
					size = avail_width * window->
							frame_width.value /
							relative;
					avail_width -= size;
					relative -= window->frame_width.value;
					widths[col][row] += size;
				}
			}
		} else if (bw_width != avail_width) {
			/* proportionally distribute error */
			extent = avail_width;
			applied = 0;
			for (col = 0; col < bw->cols; col++) {
				if (col == bw->cols - 1) {
					/* Last cell, use up remainder */
					widths[col][row] += extent - applied;
					widths[col][row] =
							widths[col][row] < 0 ?
							0 : widths[col][row];
				} else {
					/* Find size of cell adjustment */
					size = (widths[col][row] * extent) /
							(bw_width - extent);
					/* Modify cell */
					widths[col][row] += size;
					applied += size;
				}
			}
		}
	}

	/* heights */
	for (col = 0; col < bw->cols; col++) {
		avail_height = bw_height;
		relative = 0;
		for (row = 0; row < bw->rows; row++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			switch (window->frame_height.unit) {
			case FRAME_DIMENSION_PIXELS:
				heights[col][row] = window->frame_height.value *
						window->scale;
				if (window->border) {
					if (row != 0)
						heights[col][row] += 1;
					if (row != bw->rows - 1)
						heights[col][row] += 1;
				}
				break;
			case FRAME_DIMENSION_PERCENT:
				heights[col][row] = bw_height *
						window->frame_height.value / 100;
				break;
			case FRAME_DIMENSION_RELATIVE:
				heights[col][row] = 0;
				relative += window->frame_height.value;
				break;
			default:
				/* unknown frame dimension unit */
				assert(window->frame_height.unit ==
						FRAME_DIMENSION_PIXELS ||
						window->frame_height.unit ==
						FRAME_DIMENSION_PERCENT ||
						window->frame_height.unit ==
						FRAME_DIMENSION_RELATIVE);
				break;
			}
			avail_height -= heights[col][row];
		}

		if (avail_height == 0)
			continue;

		/* Redistribute to fit window */
		if ((relative > 0) && (avail_height > 0)) {
			/* Expand the relative sections to fill remainder */
			for (row = 0; row < bw->rows; row++) {
				index = (row * bw->cols) + col;
				window = &bw->children[index];

				if (window->frame_height.unit ==
						FRAME_DIMENSION_RELATIVE) {
					size = avail_height * window->
							frame_height.value /
							relative;
					avail_height -= size;
					relative -= window->frame_height.value;
					heights[col][row] += size;
				}
			}
		} else if (bw_height != avail_height) {
			/* proportionally distribute error */
			extent = avail_height;
			applied = 0;
			for (row = 0; row < bw->rows; row++) {
				if (row == bw->rows - 1) {
					/* Last cell, use up remainder */
					heights[col][row] += extent - applied;
					heights[col][row] =
							heights[col][row] < 0 ?
							0 : heights[col][row];
				} else {
					/* Find size of cell adjustment */
					size = (heights[col][row] * extent) /
							(bw_height - extent);
					/* Modify cell */
					heights[col][row] += size;
					applied += size;
				}
			}
		}
	}

	/* position frames and calculate children */
	for (row = 0; row < bw->rows; row++) {
		x = 0;
		for (col = 0; col < bw->cols; col++) {
			index = (row * bw->cols) + col;
			window = &bw->children[index];

			y = 0;
			for (row2 = 0; row2 < row; row2++)
				y+= heights[col][row2];

			window->x = x;
			window->y = y;

			new_width = widths[col][row] - 1;
			new_height = heights[col][row] - 1;

			if (window->width != new_width ||
					window->height != new_height) {
				/* Change in frame size */
				browser_window_reformat(window, false,
						new_width * bw->scale,
						new_height * bw->scale);
				window->width = new_width;
				window->height = new_height;

				browser_window_handle_scrollbars(window);
			}

			x += widths[col][row];

			if (window->children)
				browser_window_recalculate_frameset(window);
		}
	}
}


/**
 * Resize a browser window that is a frame.
 *
 * \param  bw	    The browser window to resize
 */

void browser_window_resize_frame(struct browser_window *bw, int x, int y)
{
	struct browser_window *parent;
	struct browser_window *sibling;
	int col = -1, row = -1, i;
	bool change = false;

	parent = bw->parent;
	assert(parent);

	/* get frame location */
	for (i = 0; i < (parent->cols * parent->rows); i++) {
		if (&parent->children[i] == bw) {
			col = i % parent->cols;
			row = i / parent->cols;
		 }
	}
	assert((col >= 0) && (row >= 0));

	sibling = NULL;
	if (bw->drag_resize_left)
		sibling = &parent->children[row * parent->cols + (col - 1)];
	else if (bw->drag_resize_right)
		sibling = &parent->children[row * parent->cols + (col + 1)];
	if (sibling)
		change |= browser_window_resolve_frame_dimension(bw, sibling,
				x, y, true, false);

	sibling = NULL;
	if (bw->drag_resize_up)
		sibling = &parent->children[(row - 1) * parent->cols + col];
	else if (bw->drag_resize_down)
		sibling = &parent->children[(row + 1) * parent->cols + col];
	if (sibling)
		change |= browser_window_resolve_frame_dimension(bw, sibling,
				x, y, false, true);

	if (change)
		browser_window_recalculate_frameset(parent);
}


bool browser_window_resolve_frame_dimension(struct browser_window *bw,
		struct browser_window *sibling,
		int x, int y, bool width, bool height)
{
	int bw_dimension, sibling_dimension;
	int bw_pixels, sibling_pixels;
	struct frame_dimension *bw_d, *sibling_d;
	float total_new;
	int frame_size;

	assert(!(width && height));

	/* extend/shrink the box to the pointer */
	if (width) {
		if (bw->drag_resize_left)
			bw_dimension = bw->x + bw->width - x;
		else
			bw_dimension = x - bw->x;
		bw_pixels = bw->width;
		sibling_pixels = sibling->width;
		bw_d = &bw->frame_width;
		sibling_d = &sibling->frame_width;
		frame_size = bw->parent->width;
	} else {
		if (bw->drag_resize_up)
			bw_dimension = bw->y + bw->height - y;
		else
			bw_dimension = y - bw->y;
		bw_pixels = bw->height;
		sibling_pixels = sibling->height;
		bw_d = &bw->frame_height;
		sibling_d = &sibling->frame_height;
		frame_size = bw->parent->height;
	}
	sibling_dimension = bw_pixels + sibling_pixels - bw_dimension;

	/* check for no change or no frame size*/
	if ((bw_dimension == bw_pixels) || (frame_size == 0))
		return false;
	/* check for both being 0 */
	total_new = bw_dimension + sibling_dimension;
	if ((bw_dimension + sibling_dimension) == 0)
		return false;

	/* our frame dimensions are now known to be:
	 *
	 * <--		    frame_size		    --> [VISIBLE PIXELS]
	 * |<--  bw_pixels -->|<--  sibling_pixels -->|	[VISIBLE PIXELS, BEFORE RESIZE]
	 * |<-- bw_d->value-->|<-- sibling_d->value-->| [SPECIFIED UNITS, BEFORE RESIZE]
	 * |<--bw_dimension-->|<--sibling_dimension-->|	[VISIBLE PIXELS, AFTER RESIZE]
	 * |<--		     total_new		   -->|	[VISIBLE PIXELS, AFTER RESIZE]
	 *
	 * when we resize, we must retain the original unit specification such that any
	 * subsequent resizing of the parent window will recalculate the page as the
	 * author specified.
	 *
	 * if the units of both frames are the same then we can resize the values simply
	 * by updating the values to be a percentage of the original widths.
	 */
	if (bw_d->unit == sibling_d->unit) {
		float total_specified = bw_d->value + sibling_d->value;
		bw_d->value = (total_specified * bw_dimension) / total_new;
		sibling_d->value = total_specified - bw_d->value;
		return true;
	}

	/* if one of the sizes is relative then we don't alter the relative width and
	 * just let it reflow across. the non-relative (pixel/percentage) value can
	 * simply be resolved to the specified width that will result in the required
	 * dimension.
	 */
	if (bw_d->unit == FRAME_DIMENSION_RELATIVE) {
		if ((sibling_pixels == 0) && (bw_dimension == 0))
			return false;
		if (fabs(sibling_d->value) < 0.0001)
			bw_d->value = 1;
		if (sibling_pixels == 0)
			sibling_d->value = (sibling_d->value * bw_pixels) / bw_dimension;
		else
			sibling_d->value =
					(sibling_d->value * sibling_dimension) / sibling_pixels;

		/* todo: the availble resize may have changed, update the drag box */
		return true;
	} else if (sibling_d->unit == FRAME_DIMENSION_RELATIVE) {
		if ((bw_pixels == 0) && (sibling_dimension == 0))
			return false;
		if (fabs(bw_d->value) < 0.0001)
			bw_d->value = 1;
		if (bw_pixels == 0)
			bw_d->value = (bw_d->value * sibling_pixels) / sibling_dimension;
		else
			bw_d->value = (bw_d->value * bw_dimension) / bw_pixels;

		/* todo: the availble resize may have changed, update the drag box */
		return true;
	}

	/* finally we have a pixel/percentage mix. unlike relative values, percentages
	 * can easily be backwards-calculated as they can simply be scaled like pixel
	 * values
	 */
	if (bw_d->unit == FRAME_DIMENSION_PIXELS) {
		float total_specified = bw_d->value + frame_size * sibling_d->value / 100;
		bw_d->value = (total_specified * bw_dimension) / total_new;
		sibling_d->value = (total_specified - bw_d->value) * 100 / frame_size;
		return true;
	} else if (sibling_d->unit == FRAME_DIMENSION_PIXELS) {
		float total_specified = bw_d->value * frame_size / 100 + sibling_d->value;
		sibling_d->value = (total_specified * sibling_dimension) / total_new;
		bw_d->value = (total_specified - sibling_d->value) * 100 / frame_size;
		return true;
	}
	assert(!"Invalid frame dimension unit");
	return false;
}


static bool browser_window_resize_frames(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y,
		browser_pointer_shape *pointer)
{
	struct browser_window *parent;
	bool left, right, up, down;
	int i, resize_margin;

	if ((x < bw->x) || (x > bw->x + bw->width) ||
			(y < bw->y) || (y > bw->y + bw->height))
		return false;

	parent = bw->parent;
	if ((!bw->no_resize) && parent) {
		resize_margin = FRAME_RESIZE;
		if (resize_margin * 2 > bw->width)
			resize_margin = bw->width / 2;
		left = (x < bw->x + resize_margin);
		right = (x > bw->x + bw->width - resize_margin);
		resize_margin = FRAME_RESIZE;
		if (resize_margin * 2 > bw->height)
			resize_margin = bw->height / 2;
		up = (y < bw->y + resize_margin);
		down = (y > bw->y + bw-> height - resize_margin);

		/* check if the edges can actually be moved */
		if (left || right || up || down) {
			int row = -1, col = -1;
			switch (bw->browser_window_type) {
				case BROWSER_WINDOW_NORMAL:
				case BROWSER_WINDOW_IFRAME:
					assert(0);
					break;
				case BROWSER_WINDOW_FRAME:
				case BROWSER_WINDOW_FRAMESET:
					break;
			}
			for (i = 0; i < (parent->cols * parent->rows); i++) {
				if (&parent->children[i] == bw) {
					col = i % parent->cols;
					row = i / parent->cols;
					break;
				}
			}
			assert((row >= 0) && (col >= 0));

			/* check the sibling frame is within bounds */
			left &= (col > 0);
			right &= (col < parent->cols - 1);
			up &= (row > 0);
			down &= (row < parent->rows - 1);

			/* check the sibling frames can be resized */
			if (left)
				left &= !parent->children[row *
						parent->cols + (col - 1)].
						no_resize;
			if (right)
				right &= !parent->children[row *
						parent->cols + (col + 1)].
						no_resize;
			if (up)
				up &= !parent->children[(row - 1) *
						parent->cols + col].
						no_resize;
			if (down)
				down &= !parent->children[(row + 1) *
						parent->cols + col].
						no_resize;

			/* can't have opposite directions simultaneously */
			if (up)
				down = false;
			if (left)
				right = false;
		}

		if (left || right || up || down) {
			if (left) {
				if (down)
					*pointer = BROWSER_POINTER_LD;
				else if (up)
					*pointer = BROWSER_POINTER_LU;
				else
					*pointer = BROWSER_POINTER_LEFT;
			} else if (right) {
				if (down)
					*pointer = BROWSER_POINTER_RD;
				else if (up)
					*pointer = BROWSER_POINTER_RU;
				else
					*pointer = BROWSER_POINTER_RIGHT;
			} else if (up) {
				*pointer = BROWSER_POINTER_UP;
			} else {
				*pointer = BROWSER_POINTER_DOWN;
			}
			if (mouse & (BROWSER_MOUSE_DRAG_1 |
					BROWSER_MOUSE_DRAG_2)) {

				/* TODO: Pass appropriate rectangle to allow
				 *	 front end to clamp pointer range */
				browser_window_set_drag_type(bw,
						DRAGGING_FRAME, NULL);
				bw->drag_start_x = x;
				bw->drag_start_y = y;
				bw->drag_resize_left = left;
				bw->drag_resize_right = right;
				bw->drag_resize_up = up;
				bw->drag_resize_down = down;
			}
			return true;
		}
	}

	if (bw->children) {
		for (i = 0; i < (bw->cols * bw->rows); i++)
			if (browser_window_resize_frames(&bw->children[i],
					mouse, x, y, pointer))
				return true;
	}
	if (bw->iframes) {
		for (i = 0; i < bw->iframe_count; i++)
			if (browser_window_resize_frames(&bw->iframes[i],
					mouse, x, y, pointer))
				return true;
	}
	return false;
}


bool browser_window_frame_resize_start(struct browser_window *bw,
		browser_mouse_state mouse, int x, int y,
		browser_pointer_shape *pointer)
{
	struct browser_window *root = browser_window_get_root(bw);
	int offx, offy;

	browser_window_get_position(bw, true, &offx, &offy);

	return browser_window_resize_frames(root, mouse,
			x + offx, y + offy, pointer);
}
