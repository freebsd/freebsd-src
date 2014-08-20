/*
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net>
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
 * Scrollbar widget (interface).
 */

#ifndef _NETSURF_DESKTOP_SCROLLBAR_H_
#define _NETSURF_DESKTOP_SCROLLBAR_H_

#include <stdbool.h>
#include <limits.h>

#include "desktop/browser.h"

#define SCROLLBAR_WIDTH 16

/* Region dependent values for scrollbar_scroll function */
#define SCROLL_TOP		INT_MIN
#define SCROLL_PAGE_UP		INT_MIN + 1
#define SCROLL_PAGE_DOWN	INT_MAX - 1
#define SCROLL_BOTTOM		INT_MAX

struct scrollbar;

typedef enum {
	SCROLLBAR_MSG_MOVED,		/* the scroll value has changed */
	SCROLLBAR_MSG_SCROLL_START,	/* a scrollbar drag has started, all
 					 * mouse events should be passed to
					 * the scrollbar regardless of the
					 * coordinates
					 */
	SCROLLBAR_MSG_SCROLL_FINISHED,	/* cancel the above */
} scrollbar_msg;

struct scrollbar_msg_data {
	struct scrollbar *scrollbar;
	scrollbar_msg msg;
	int scroll_offset;
	int x0, y0, x1, y1;
};

/**
 * Client callback for the scrollbar.
 * 
 * \param client_data		user data passed at scroll creation
 * \param scrollbar_data	scrollbar message data
 */
typedef void(*scrollbar_client_callback)(void *client_data,
		struct scrollbar_msg_data *scrollbar_data);


/**
 * Create a scrollbar.
 *
 * \param horizontal		true = horizontal scrollbar, false = vertical
 * \param length		length of scrollbar widget
 * \param full_size		length of contained scrollable area
 * \param visible_size		length of visible part of scrollable area
 * \param client_data		data for the client callback
 * \param client_callback	client callback for scrollbar events
 * \param s			updated to point at the newly created scrollbar
 * \return	true if scrollbar has been created succesfully or false on
 *		memory exhaustion
 */
bool scrollbar_create(bool horizontal, int length, int full_size,
		int visible_size, void *client_data,
		scrollbar_client_callback client_callback,
		struct scrollbar **s);

/**
 * Destroy a scrollbar.
 *
 * \param s	the scrollbar to be destroyed
 */
void scrollbar_destroy(struct scrollbar *s);

/**
 * Redraw a part of the scrollbar.
 *
 * \param s	the scrollbar to be redrawn
 * \param x	the X coordinate to draw the scrollbar at
 * \param y	the Y coordinate to draw the scrollbar at
 * \param clip	the clipping rectangle
 * \param scale	scale for the redraw
 * \param ctx	current redraw context
 * \return	true on succes false otherwise
 */
bool scrollbar_redraw(struct scrollbar *s, int x, int y,
		const struct rect *clip, float scale,
		const struct redraw_context *ctx);

/**
 * Set the scroll value of the scrollbar.
 *
 * \param s		the scrollbar to have the value set
 * \param value		the new value to be set
 * \param bar_pos	true if the value is for the scrollbar indication bar
 *			position, false if it is for the scrolled area offset
 */
void scrollbar_set(struct scrollbar *s, int value, bool bar_pos);

/**
 * Scroll the scrollbar by given amount.
 *
 * \param s		the scrollbar to be scrolled
 * \param change	the change in scroll offset required (in px)
 * \return true iff the scrollbar was moved.
 */
bool scrollbar_scroll(struct scrollbar *s, int change);

/**
 * Get the current scroll offset to the visible part of the full area.
 *
 * \param s	the scrollbar to get the scroll offset value from
 * \return	current scroll offset
 */
int scrollbar_get_offset(struct scrollbar *s);

/**
 * Set the length of the scrollbar widget, the size of the visible area, and the
 * size of the full area.
 *
 * \param s		the scrollbar to set the values for
 * \param length	-1 or the new scrollbar widget length
 * \param visible_size	-1 or the new size of the visible area
 * \param full_size	-1 or the new size of the full contained area
 */
void scrollbar_set_extents(struct scrollbar *s, int length,
		int visible_size, int full_size);

/**
 * Check orientation of the scrollbar.
 *
 * \param s	the scrollbar to check the orientation of
 * \return	true for a horizontal scrollbar, else false (vertical)
 */
bool scrollbar_is_horizontal(struct scrollbar *s);

/* Scrollbar mouse input status flags */
typedef enum {
	SCROLLBAR_MOUSE_NONE	= 0,		/**< Not relevant */
	SCROLLBAR_MOUSE_USED	= (1 <<  0),	/**< Took action with input */
	SCROLLBAR_MOUSE_BOTH	= (1 <<  1),	/**< Scrolling both bars */
	SCROLLBAR_MOUSE_UP	= (1 <<  2),	/**< Hover: scroll up */
	SCROLLBAR_MOUSE_PUP	= (1 <<  3),	/**< Hover: scroll page up */
	SCROLLBAR_MOUSE_VRT	= (1 <<  4),	/**< Hover: vert. drag bar */
	SCROLLBAR_MOUSE_PDWN	= (1 <<  5),	/**< Hover: scroll page down */
	SCROLLBAR_MOUSE_DWN	= (1 <<  6),	/**< Hover: scroll down */
	SCROLLBAR_MOUSE_LFT	= (1 <<  7),	/**< Hover: scroll left */
	SCROLLBAR_MOUSE_PLFT	= (1 <<  8),	/**< Hover: scroll page left */
	SCROLLBAR_MOUSE_HRZ	= (1 <<  9),	/**< Hover: horiz. drag bar */
	SCROLLBAR_MOUSE_PRGT	= (1 << 10),	/**< Hover: scroll page right */
	SCROLLBAR_MOUSE_RGT	= (1 << 11)	/**< Hover: scroll right */
} scrollbar_mouse_status;

/**
 * Handle mouse actions other then drag ends.
 *
 * \param s	the scrollbar which gets the mouse action
 * \param mouse	mouse state
 * \param x	X coordinate of the mouse
 * \param y	Y coordinate of the mouse
 * \return	the scrollbar mouse status
 */
scrollbar_mouse_status scrollbar_mouse_action(struct scrollbar *s,
		browser_mouse_state mouse, int x, int y);

/**
 * Get a status bar message from a scrollbar mouse input status.
 *
 * \param status	Status to convert to message
 * \return		Message for the status bar or NULL on failure
 */
const char *scrollbar_mouse_status_to_message(scrollbar_mouse_status status);

/**
 * Handle end of mouse drags.
 *
 * \param s	the scrollbar for which the drag ends
 * \param mouse	mouse state
 * \param x	X coordinate of the mouse
 * \param y	Y coordinate of the mouse
 */
void scrollbar_mouse_drag_end(struct scrollbar *s,
		browser_mouse_state mouse, int x, int y);

/**
 * Called when the content is being dragged to the scrollbars have to adjust.
 * If the content has both scrollbars, and scrollbar_make_pair has beed called
 * before, only the one scroll which will receive further mouse events has to be
 * passed.
 *
 * \param s	one of the the scrollbars owned by the dragged content
 * \param x	X coordinate of mouse during drag start
 * \param y	Y coordinate of mouse during drag start
 */
void scrollbar_start_content_drag(struct scrollbar *s, int x, int y);

/**
 * Connect a horizontal and a vertical scrollbar into a pair so that they
 * co-operate during 2D drags.
 *
 * \param horizontal	the scrollbar used for horizontal scrolling
 * \param vertical	the scrollbar used for vertical scrolling
 */
void scrollbar_make_pair(struct scrollbar *horizontal,
		struct scrollbar *vertical);

/**
 * Get the scrollbar's client data
 *
 * \param s	the scrollbar to get the client data from
 * \return	client data
 */
void *scrollbar_get_data(struct scrollbar *s);

#endif
