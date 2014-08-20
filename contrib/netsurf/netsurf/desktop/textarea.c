/*
 * Copyright 2006 John-Mark Bell <jmb@netsurf-browser.org>
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
 * Single/Multi-line UTF-8 text area (implementation)
 */

#include <stdint.h>
#include <string.h>
#include "css/utils.h"

#include "desktop/mouse.h"
#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "desktop/plotters.h"
#include "desktop/scrollbar.h"
#include "desktop/gui_factory.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"

#define CARET_COLOR 0x0000FF
#define TA_ALLOC_STEP 512

static plot_style_t pstyle_stroke_caret = {
    .stroke_type = PLOT_OP_TYPE_SOLID,
    .stroke_colour = CARET_COLOR,
    .stroke_width = 1,
};

struct line_info {
	unsigned int b_start;		/**< Byte offset of line start */
	unsigned int b_length;		/**< Byte length of line */
	int width;			/**< Width in pixels of line */
};
struct textarea_drag {
	textarea_drag_type type;
	union {
		struct scrollbar* scrollbar;
	} data;
};

struct textarea_utf8 {
	char *data;		/**< UTF-8 text */
	unsigned int alloc;	/**< Size of allocated text */
	unsigned int len;	/**< Length of text, in bytes */
	unsigned int utf8_len;	/**< Length of text, in characters without
				 *   trailing NULL */
};

struct textarea_undo_detail {
	unsigned int b_start;	/**< Offset to detail's text in undo buffer */
	unsigned int b_end;	/**< End of text (exclusive) */
	unsigned int b_limit;	/**< End of detail space (exclusive) */

	unsigned int b_text_start;	/**< Start of textarea text. */
	unsigned int b_text_end;	/**< End of textarea text (exclusive) */
};

struct textarea_undo {
	unsigned int details_alloc;		/**< Details allocated for */
	unsigned int next_detail;		/**< Next detail pos */
	unsigned int last_detail;		/**< Last detail used */
	struct textarea_undo_detail *details;	/**< Array of undo details */

	struct textarea_utf8 text;
};

struct textarea {

	int scroll_x, scroll_y;		/**< scroll offsets for the textarea */

	struct scrollbar *bar_x;	/**< Horizontal scroll. */
	struct scrollbar *bar_y;	/**< Vertical scroll. */

	unsigned int flags;		/**< Textarea flags */
	int vis_width;			/**< Visible width, in pixels */
	int vis_height;			/**< Visible height, in pixels */

	int pad_top;		/**< Top padding, inside border, in pixels */
	int pad_right;		/**< Right padding, inside border, in pixels */
	int pad_bottom;		/**< Bottom padding, inside border, in pixels */
	int pad_left;		/**< Left padding, inside border, in pixels */

	int border_width;	/**< Border width, in pixels */
	colour border_col;	/**< Border colour */

	int text_y_offset;		/**< Vertical dist to 1st line top */
	int text_y_offset_baseline;	/**< Vertical dist to 1st baseline */

	plot_font_style_t fstyle;	/**< Text style, inc. textarea bg col */
	plot_font_style_t sel_fstyle;	/**< Selected text style */
	int line_height;		/**< Line height obtained from style */

	struct textarea_utf8 text;	/**< Textarea text content */
#define PASSWORD_REPLACEMENT "\xe2\x80\xa2"
#define PASSWORD_REPLACEMENT_W (sizeof(PASSWORD_REPLACEMENT) - 1)
	struct textarea_utf8 password;	/**< Text for obscured display */

	struct textarea_utf8 *show;	/**< Points at .text or .password */

	struct {
		int line;		/**< Line caret is on */
		int byte_off;		/**< Character index of caret on line */
	} caret_pos;

	int caret_x;			/**< cached X coordinate of the caret */
	int caret_y;			/**< cached Y coordinate of the caret */

	int sel_start;	/**< Character index of sel start (inclusive) */
	int sel_end;	/**< Character index of sel end (exclusive) */

	int h_extent;			/**< Width of content in px */
	int v_extent;			/**< Height of content in px */

	int line_count;			/**< Count of lines */

#define LINE_CHUNK_SIZE 32
	struct line_info *lines;	/**< Line info array */
	unsigned int lines_alloc_size;	/**< Number of LINE_CHUNK_SIZEs */

	/** Callback function for messages to client */
	textarea_client_callback callback;

	void *data;		/**< Client data for callback */

	int drag_start;		/**< Byte offset of drag start (in ta->show) */
	struct textarea_drag drag_info;	/**< Drag information */

	struct textarea_undo undo; /**< Undo/redo information */
};



/**
 * Normalises any line endings within the text, replacing CRLF or CR with
 * LF as necessary. If the textarea is single line, then all linebreaks are
 * converted into spaces.
 *
 * \param ta		Text area
 * \param b_start	Byte offset to start at
 * \param b_len		Byte length to check
 */
static void textarea_normalise_text(struct textarea *ta,
		unsigned int b_start, unsigned int b_len)
{
	bool multi = (ta->flags & TEXTAREA_MULTILINE) ? true : false;
	struct textarea_msg msg;
	unsigned int index;

	/* Remove CR characters. If it's a CRLF pair delete the CR, or replace
	 * CR with LF otherwise.
	 */
	for (index = 0; index < b_len; index++) {
		if (ta->text.data[b_start + index] == '\r') {
			if (b_start + index + 1 <= ta->text.len &&
					ta->text.data[b_start + index + 1] ==
							'\n') {
				ta->text.len--;
				ta->text.utf8_len--;
				memmove(ta->text.data + b_start + index,
						ta->text.data + b_start +
								index + 1,
						ta->text.len - b_start - index);
			}
			else
				ta->text.data[b_start + index] = '\n';
		}
		/* Replace newlines with spaces if this is a single line
		 * textarea.
		 */
		if (!multi && (ta->text.data[b_start + index] == '\n'))
			ta->text.data[b_start + index] = ' ';
	}

	/* Build text modified message */
	msg.ta = ta;
	msg.type = TEXTAREA_MSG_TEXT_MODIFIED;
	msg.data.modified.text = ta->text.data;
	msg.data.modified.len = ta->text.len;

	/* Pass message to client */
	ta->callback(ta->data, &msg);
}


/**
 * Reset the selection (no redraw)
 *
 * \param ta Text area
 */
static inline void textarea_reset_selection(struct textarea *ta)
{
	ta->sel_start = ta->sel_end = -1;
}


/**
 * Get the caret's position
 *
 * \param ta Text area
 * \return 0-based byte offset of caret location, or -1 on error
 */
static int textarea_get_caret(struct textarea *ta)
{
	/* Ensure caret isn't hidden */
	if (ta->caret_pos.byte_off < 0)
		textarea_set_caret(ta, 0);

	/* If the text is a trailing NULL only */
	if (ta->text.utf8_len == 0)
		return 0;

	/* If caret beyond text */
	if (ta->caret_pos.line >= ta->line_count)
		return ta->show->len - 1;

	/* Byte offset of line, plus byte offset of caret on line */
	return ta->lines[ta->caret_pos.line].b_start + ta->caret_pos.byte_off;
}


/**
 * Scrolls a textarea to make the caret visible (doesn't perform a redraw)
 *
 * \param ta	The text area to be scrolled
 * \return 	true if textarea was scrolled false otherwise
 */
static bool textarea_scroll_visible(struct textarea *ta)
{
	int x0, x1, y0, y1; /* area we want caret inside */
	int x, y; /* caret pos */
	int xs = ta->scroll_x;
	int ys = ta->scroll_y;
	int vis;
	int scrollbar_width;
	bool scrolled = false;

	if (ta->caret_pos.byte_off < 0)
		return false;

	scrollbar_width = (ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH;
	x0 = ta->border_width + ta->pad_left;
	x1 = ta->vis_width - (ta->border_width + ta->pad_right);

	/* Adjust scroll pos for reduced extents */
	vis = ta->vis_width - 2 * ta->border_width - scrollbar_width;
	if (ta->h_extent - xs < vis)
		xs -= vis - (ta->h_extent - xs);

	/* Get caret pos on screen */
	x = ta->caret_x - xs;

	/* scroll as required */
	if (x < x0)
		xs += (x - x0);
	else if (x > x1)
		xs += (x - x1);

	if (ta->bar_x == NULL && ta->scroll_x != 0 &&
			ta->flags & TEXTAREA_MULTILINE) {
		/* Scrollbar removed, set to zero */
		ta->scroll_x = 0;
		scrolled = true;

	} else if (xs != ta->scroll_x) {
		/* Scrolled, set new pos. */
		if (ta->bar_x != NULL) {
			scrollbar_set(ta->bar_x, xs, false);
			xs = scrollbar_get_offset(ta->bar_x);
			if (xs != ta->scroll_x) {
				ta->scroll_x = xs;
				scrolled = true;
			}

		} else if (!(ta->flags & TEXTAREA_MULTILINE)) {
			ta->scroll_x = xs;
			scrolled = true;
		}
	}

	/* check and change vertical scroll */
	if (ta->flags & TEXTAREA_MULTILINE) {
		scrollbar_width = (ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH;
		y0 = 0;
		y1 = ta->vis_height - 2 * ta->border_width -
				ta->pad_top - ta->pad_bottom;

		/* Adjust scroll pos for reduced extents */
		vis = ta->vis_height - 2 * ta->border_width - scrollbar_width;
		if (ta->v_extent - ys < vis)
			ys -= vis - (ta->v_extent - ys);

		/* Get caret pos on screen */
		y = ta->caret_y - ys;

		/* scroll as required */
		if (y < y0)
			ys += (y - y0);
		else if (y + ta->line_height > y1)
			ys += (y + ta->line_height - y1);

		if (ys != ta->scroll_y && ta->bar_y != NULL) {
			/* Scrolled, set new pos. */
			scrollbar_set(ta->bar_y, ys, false);
			ys = scrollbar_get_offset(ta->bar_y);
			if (ys != ta->scroll_y) {
				ta->scroll_y = ys;
				scrolled = true;
			}

		} else if (ta->bar_y == NULL && ta->scroll_y != 0) {
			/* Scrollbar removed, set to zero */
			ta->scroll_y = 0;
			scrolled = true;
		}
	}

	return scrolled;
}


/**
 * Set the caret position
 *
 * \param ta		Text area
 * \param caret_b	Byte offset to caret
 * \return true iff caret placement caused a scroll
 */
static bool textarea_set_caret_internal(struct textarea *ta, int caret_b)
{
	unsigned int b_off;
	int i;
	int index;
	int x, y;
	int x0, y0, x1, y1;
	int width, height;
	struct textarea_msg msg;
	bool scrolled = false;

	if (caret_b != -1 && caret_b > (signed)(ta->show->len - 1))
		caret_b = ta->show->len - 1;

	/* Delete the old caret */
	if (ta->caret_pos.byte_off != -1 &&
			ta->flags & TEXTAREA_INTERNAL_CARET) {
		x0 = ta->caret_x - ta->scroll_x;
		y0 = ta->caret_y - ta->scroll_y;
		width = 2;
		height = ta->line_height;

		msg.ta = ta;
		msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
		msg.data.redraw.x0 = x0;
		msg.data.redraw.y0 = y0;
		msg.data.redraw.x1 = x0 + width;
		msg.data.redraw.y1 = y0 + height;

		/* Ensure it is hidden */
		ta->caret_pos.byte_off = -1;

		ta->callback(ta->data, &msg);
	}

	/* check if the caret has to be drawn at all */
	if (caret_b != -1) {
		/* Find byte offset of caret position */
		b_off = caret_b;

		/* Now find line in which byte offset appears */
		for (i = 0; i < ta->line_count - 1; i++)
			if (ta->lines[i + 1].b_start > b_off)
				break;

		/* Set new caret pos */
		ta->caret_pos.line = i;
		ta->caret_pos.byte_off = b_off - ta->lines[i].b_start;

		/* Finally, redraw the caret */
		index = textarea_get_caret(ta);

		/* find byte offset of caret position */
		b_off = index;

		nsfont.font_width(&ta->fstyle,
				ta->show->data +
				ta->lines[ta->caret_pos.line].b_start,
				b_off - ta->lines[ta->caret_pos.line].b_start,
				&x);

		x += ta->border_width + ta->pad_left;
		ta->caret_x = x;
		y = ta->line_height * ta->caret_pos.line + ta->text_y_offset;
		ta->caret_y = y;

		scrolled = textarea_scroll_visible(ta);

		if (!scrolled && ta->flags & TEXTAREA_INTERNAL_CARET) {
			/* Didn't scroll, just moved caret.
			 * Caret is internal caret, redraw it */
			x -= ta->scroll_x;
			y -= ta->scroll_y;
			x0 = max(x - 1, ta->border_width);
			y0 = max(y, 0);
			x1 = min(x + 1, ta->vis_width - ta->border_width);
			y1 = min(y + ta->line_height,
					ta->vis_height);

			width = x1 - x0;
			height = y1 - y0;

			if (width > 0 && height > 0) {
				msg.ta = ta;
				msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
				msg.data.redraw.x0 = x0;
				msg.data.redraw.y0 = y0;
				msg.data.redraw.x1 = x0 + width;
				msg.data.redraw.y1 = y0 + height;

				ta->callback(ta->data, &msg);
			}
		} else if (scrolled && !(ta->flags & TEXTAREA_MULTILINE)) {
			/* Textarea scrolled, whole area needs redraw */
			/* With multi-line textareas, the scrollbar
			 * callback will have requested redraw. */
			msg.ta = ta;
			msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
			msg.data.redraw.x0 = 0;
			msg.data.redraw.y0 = 0;
			msg.data.redraw.x1 = ta->vis_width;
			msg.data.redraw.y1 = ta->vis_height;

			ta->callback(ta->data, &msg);
		}

		if (!(ta->flags & TEXTAREA_INTERNAL_CARET)) {
			/* Tell client where caret should be placed */
			struct rect cr = {
				.x0 = ta->border_width,
				.y0 = ta->border_width,
				.x1 = ta->vis_width - ta->border_width -
						((ta->bar_y == NULL) ?
						0 : SCROLLBAR_WIDTH),
				.y1 = ta->vis_height - ta->border_width -
						((ta->bar_x == NULL) ?
						0 : SCROLLBAR_WIDTH)
			};
			msg.ta = ta;
			msg.type = TEXTAREA_MSG_CARET_UPDATE;
			msg.data.caret.type = TEXTAREA_CARET_SET_POS;
			msg.data.caret.pos.x = x - ta->scroll_x;
			msg.data.caret.pos.y = y - ta->scroll_y;
			msg.data.caret.pos.height = ta->line_height;
			msg.data.caret.pos.clip = &cr;

			ta->callback(ta->data, &msg);
		}

	} else if (!(ta->flags & TEXTAREA_INTERNAL_CARET)) {
		/* Caret hidden, and client is responsible: tell client */
		msg.ta = ta;
		msg.type = TEXTAREA_MSG_CARET_UPDATE;
		msg.data.caret.type = TEXTAREA_CARET_HIDE;

		ta->callback(ta->data, &msg);
	}

	return scrolled;
}


/**
 * Selects a character range in the textarea and redraws it
 *
 * \param ta		Text area
 * \param b_start	First character (inclusive) byte offset
 * \param b_end		Last character (exclusive) byte offset
 * \parm  force_redraw  Redraw whether selection changed or not
 * \return 		true on success false otherwise
 */
static bool textarea_select(struct textarea *ta, int b_start, int b_end,
		bool force_redraw)
{
	int swap;
	bool pre_existing_selection = (ta->sel_start != -1);
	struct textarea_msg msg;

	if (b_start == b_end) {
		textarea_clear_selection(ta);
		return true;
	}

	/* Ensure start is the beginning of the selection */
	if (b_start > b_end) {
		swap = b_start;
		b_start = b_end;
		b_end = swap;
	}

	if (ta->sel_start == b_start && ta->sel_end == b_end &&
			!force_redraw)
		return true;

	msg.ta = ta;
	msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
	msg.data.redraw.x0 = ta->border_width;
	msg.data.redraw.x1 = ta->vis_width - ta->border_width -
			((ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH);

	if (force_redraw) {
		/* Asked to redraw everything */
		msg.data.redraw.y0 = ta->border_width;
		msg.data.redraw.y1 = ta->vis_height - ta->border_width -
				((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH);
	} else {
		/* Try to minimise redraw region */
		unsigned int b_low, b_high;
		int line_start = 0, line_end = 0;

		if (!pre_existing_selection) {
			/* There's a new selection */
			b_low = b_start;
			b_high = b_end;

		} else if (ta->sel_start != b_start && ta->sel_end != b_end) {
			/* Both ends of the selection have moved */
			b_low = (ta->sel_start < b_start) ?
					ta->sel_start : b_start;
			b_high = (ta->sel_end > b_end) ?
					ta->sel_end : b_end;

		} else if (ta->sel_start != b_start) {
			/* Selection start changed */
			if ((signed)ta->sel_start < b_start) {
				b_low = ta->sel_start;
				b_high = b_start;
			} else {
				b_low = b_start;
				b_high = ta->sel_start;
			}

		} else {
			/* Selection end changed */
			if ((signed)ta->sel_end < b_end) {
				b_low = ta->sel_end;
				b_high = b_end;
			} else {
				b_low = b_end;
				b_high = ta->sel_end;
			}
		}

		/* Find redraw start/end lines */
		for (line_end = 0; line_end < ta->line_count - 1; line_end++)
			if (ta->lines[line_end + 1].b_start > b_low) {
				line_start = line_end;
				break;
			}
		for (; line_end < ta->line_count - 1; line_end++)
			if (ta->lines[line_end + 1].b_start > b_high)
				break;

		/* Set vertical redraw range */
		msg.data.redraw.y0 = max(ta->border_width,
				ta->line_height * line_start +
				ta->text_y_offset - ta->scroll_y);
		msg.data.redraw.y1 = min(ta->vis_height - ta->border_width -
				((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH),
				ta->line_height * line_end + ta->text_y_offset +
				ta->line_height - ta->scroll_y);
	}

	ta->callback(ta->data, &msg);

	ta->sel_start = b_start;
	ta->sel_end = b_end;

	if (!pre_existing_selection && ta->sel_start != -1) {
		/* Didn't have a selection before, but do now */
		msg.type = TEXTAREA_MSG_SELECTION_REPORT;

		msg.data.selection.have_selection = true;
		msg.data.selection.read_only = (ta->flags & TEXTAREA_READONLY);

		ta->callback(ta->data, &msg);

		if (!(ta->flags & TEXTAREA_INTERNAL_CARET)) {
			/* Caret hidden, and client is responsible */
			msg.type = TEXTAREA_MSG_CARET_UPDATE;
			msg.data.caret.type = TEXTAREA_CARET_HIDE;

			ta->callback(ta->data, &msg);
		}
	}

	return true;
}


/**
 * Selects a text fragment, relative to current caret position.
 *
 * \param ta  Text area
 * \return True on success, false otherwise
 */
static bool textarea_select_fragment(struct textarea * ta)
{
	int caret_pos;
	size_t sel_start, sel_end;

	/* Fragment separators must be suitable for URLs and ordinary text */
	static const char *sep = " /:.\r\n";

	caret_pos = textarea_get_caret(ta);
	if (caret_pos < 0) {
		return false;
	}

	if (ta->show->len == 0) {
		return false;
	}

	/* Compute byte offset of caret position */
	for (sel_start = (caret_pos > 0 ? caret_pos - 1 : caret_pos);
			sel_start > 0; sel_start--) {
		/* Cache the character offset of the last separator */
		if (strchr(sep, ta->show->data[sel_start]) != NULL) {
			/* Found start,
			 * add one to start to skip over separator */
			sel_start++;
			break;
		}
	}

	/* Search for next separator, if any */
	for (sel_end = caret_pos; sel_end < ta->show->len - 1; sel_end++) {
		if (strchr(sep, ta->show->data[sel_end]) != NULL) {
			break;
		}
	}

	if (sel_start < sel_end) {
		textarea_select(ta, sel_start, sel_end, false);
		return true;
	}

	return false;
}


/**
 * Selects paragraph, at current caret position.
 *
 * \param ta  textarea widget
 * \return True on success, false otherwise
 */
static bool textarea_select_paragraph(struct textarea * ta)
{
	int caret_pos;
	size_t sel_start, sel_end;

	caret_pos = textarea_get_caret(ta);
	if (caret_pos < 0) {
		return false;
	}

	/* Work back from caret, looking for a place to start selection */
	for (sel_start = (caret_pos > 0 ? caret_pos - 1 : caret_pos);
			sel_start > 0; sel_start--) {
		/* Set selection start as character after any new line found */
		if (ta->show->data[sel_start] == '\n') {
			/* Add one to start to skip over separator */
			sel_start++;
			break;
		}
	}

	/* Search for end of selection */
	for (sel_end = caret_pos; sel_end < ta->show->len - 1; sel_end++) {
		if (ta->show->data[sel_end] == '\n') {
			break;
		}
	}

	if (sel_start < sel_end) {
		textarea_select(ta, sel_start, sel_end, false);
		return true;
	}

	return false;
}


/**
 * Callback for scrollbar widget.
 */
static void textarea_scrollbar_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data)
{
	struct textarea *ta = client_data;
	struct textarea_msg msg;

	switch(scrollbar_data->msg) {
	case SCROLLBAR_MSG_MOVED:
		/* Scrolled; redraw everything */
		ta->scroll_x = scrollbar_get_offset(ta->bar_x);
		ta->scroll_y = scrollbar_get_offset(ta->bar_y);

		msg.ta = ta;
		msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
		msg.data.redraw.x0 = 0;
		msg.data.redraw.y0 = 0;
		msg.data.redraw.x1 = ta->vis_width;
		msg.data.redraw.y1 = ta->vis_height;

		ta->callback(ta->data, &msg);

		if (!(ta->flags & TEXTAREA_INTERNAL_CARET) &&
				ta->sel_start < 0 &&
				ta->caret_pos.byte_off >= 0) {
			/* Tell client where caret should be placed */
			int x = ta->caret_x - ta->scroll_x;
			int y = ta->caret_y - ta->scroll_y;
			int h = ta->line_height;
			struct rect cr = {
				.x0 = ta->border_width,
				.y0 = ta->border_width,
				.x1 = ta->vis_width - ta->border_width -
						((ta->bar_y == NULL) ?
						0 : SCROLLBAR_WIDTH),
				.y1 = ta->vis_height - ta->border_width -
						((ta->bar_x == NULL) ?
						0 : SCROLLBAR_WIDTH)
			};

			msg.ta = ta;
			msg.type = TEXTAREA_MSG_CARET_UPDATE;

			if ((x >= cr.x0 && x < cr.x1) &&
					(y + h >= cr.y0 && y < cr.y1)) {
				/* Caret inside textarea */
				msg.data.caret.type = TEXTAREA_CARET_SET_POS;
				msg.data.caret.pos.x = x;
				msg.data.caret.pos.y = y;
				msg.data.caret.pos.height = h;
				msg.data.caret.pos.clip = &cr;
			} else {
				/* Caret fully outside textarea */
				msg.data.caret.type = TEXTAREA_CARET_HIDE;
			}

			ta->callback(ta->data, &msg);
		}
		break;

	case SCROLLBAR_MSG_SCROLL_START:
		ta->drag_info.type = TEXTAREA_DRAG_SCROLLBAR;
		ta->drag_info.data.scrollbar = scrollbar_data->scrollbar;

		msg.ta = ta;
		msg.type = TEXTAREA_MSG_DRAG_REPORT;
		msg.data.drag = ta->drag_info.type;

		/* Tell client we're handling a drag */
		ta->callback(ta->data, &msg);
		break;

	case SCROLLBAR_MSG_SCROLL_FINISHED:
		ta->drag_info.type = TEXTAREA_DRAG_NONE;

		msg.ta = ta;
		msg.type = TEXTAREA_MSG_DRAG_REPORT;
		msg.data.drag = ta->drag_info.type;

		/* Tell client we finished handling the drag */
		ta->callback(ta->data, &msg);
		break;

	default:
		break;
	}
}



/**
 * Reflow a single line textarea
 *
 * \param ta 	Textarea widget to reflow
 * \param b_off	0-based byte offset in ta->show's text to start of modification
 * \param r	Modified/reduced to area where redraw is required
 * \return true on success false otherwise
 */
static bool textarea_reflow_singleline(struct textarea *ta, size_t b_off,
		struct rect *r)
{
	int x;
	int shift;
	int retained_width = 0;
	int w = ta->vis_width - 2 * ta->border_width -
			ta->pad_left - ta->pad_right;

	assert(!(ta->flags & TEXTAREA_MULTILINE));

	if (ta->lines == NULL) {
		ta->lines =
			malloc(LINE_CHUNK_SIZE * sizeof(struct line_info));
		if (ta->lines == NULL) {
			LOG(("malloc failed"));
			return false;
		}
		ta->lines_alloc_size = LINE_CHUNK_SIZE;

		ta->lines[0].b_start = 0;
		ta->lines[0].b_length = 0;
		ta->lines[0].width = 0;
	}

	if (ta->flags & TEXTAREA_PASSWORD &&
			ta->text.utf8_len != ta->password.utf8_len) {
		/* Make password-obscured text have same number of
		 * characters as underlying text */
		unsigned int c, b;
		int diff = ta->text.utf8_len - ta->password.utf8_len;
		unsigned int rep_len = PASSWORD_REPLACEMENT_W;
		unsigned int b_len = ta->text.utf8_len * rep_len + 1;

		if (diff > 0 && b_len > ta->password.alloc) {
			/* Increase password alloaction */
			char *temp = realloc(ta->password.data,
					b_len + TA_ALLOC_STEP);
			if (temp == NULL) {
				LOG(("realloc failed"));
				return false;
			}

			ta->password.data = temp;
			ta->password.alloc = b_len + TA_ALLOC_STEP;
		}

		b_len--;
		for (c = 0; c < b_len; c += rep_len) {
			for (b = 0; b < rep_len; b++) {
				ta->password.data[c + b] =
						PASSWORD_REPLACEMENT[b];
			}
		}
		ta->password.data[b_len] = '\0';
		ta->password.len = b_len + 1;
		ta->password.utf8_len = ta->text.utf8_len;
	}

	/* Measure new width */
	nsfont.font_width(&ta->fstyle, ta->show->data,
			ta->show->len - 1, &x);

	/* Get width of retained text */
	if (b_off != ta->lines[0].b_length) {
		nsfont.font_width(&ta->fstyle, ta->show->data,
				b_off, &retained_width);
	} else {
		retained_width = ta->lines[0].width;
	}

	shift = ta->border_width + ta->pad_left - ta->scroll_x;

	r->x0 = max(r->x0, retained_width + shift - 1);
	r->x1 = min(r->x1, max(x, ta->lines[0].width) + shift + 1);

	ta->lines[0].b_start = 0;
	ta->lines[0].b_length = ta->show->len - 1;
	ta->lines[0].width = x;

	if (x > w)
		w = x;

	ta->h_extent = w + ta->pad_left + ta->pad_right;
	ta->line_count = 1;

	return true;
}



/**
 * Reflow a multiline textarea from the given line onwards
 *
 * \param ta		Textarea to reflow
 * \param b_start	0-based byte offset in ta->text to start of modification
 * \param b_length	Byte length of change in textarea text
 * \return true on success false otherwise
 */
static bool textarea_reflow_multiline(struct textarea *ta,
		const size_t b_start, const int b_length, struct rect *r)
{
	char *text;
	unsigned int len;
	unsigned int start;
	size_t b_off;
	int x;
	char *space, *para_end;
	unsigned int line; /* line count */
	unsigned int scroll_lines;
	int avail_width;
	int h_extent; /* horizontal extent */
	int v_extent; /* vertical extent */
	bool restart = false;
	bool skip_line = false;

	assert(ta->flags & TEXTAREA_MULTILINE);

	if (ta->lines == NULL) {
		ta->lines =
			malloc(LINE_CHUNK_SIZE * sizeof(struct line_info));
		if (ta->lines == NULL) {
			LOG(("malloc failed"));
			return false;
		}
		ta->lines_alloc_size = LINE_CHUNK_SIZE;
	}

	/* Get line of start of changes */
	for (start = 0; (signed) start < ta->line_count - 1; start++)
		if (ta->lines[start + 1].b_start > b_start)
			break;

	/* Find max number of lines before vertical scrollbar is required */
	scroll_lines = (ta->vis_height - 2 * ta->border_width -
				ta->pad_top - ta->pad_bottom) /
				ta->line_height;

	/* Have to start on line before where the changes are in case an
	 * added space makes the text before the space on a soft-wrapped line
	 * fit on the line above */
	if (start != 0)
		start--;

	do {
		/* Set line count to start point */
		if (restart)
			start = 0;

		line = start;

		/* Find available width */
		avail_width = ta->vis_width - 2 * ta->border_width -
				ta->pad_left - ta->pad_right;
		if (avail_width < 0)
			avail_width = 0;
		h_extent = avail_width;

		/* Set up initial length and text offset */
		if (line == 0) {
			len = ta->text.len - 1;
			text = ta->text.data;
		} else {
			unsigned int i;
			len = ta->text.len - 1 - ta->lines[line].b_start;
			text = ta->text.data + ta->lines[line].b_start;

			for (i = 0; i < line; i++) {
				if (ta->lines[i].width > h_extent) {
					h_extent = ta->lines[i].width;
				}
			}
		}

		if (ta->text.len == 1) {
			/* Handle empty textarea */
			assert(ta->text.data[0] == '\0');
			ta->lines[line].b_start = 0;
			ta->lines[line].b_length = 0;
			ta->lines[line++].width = 0;
			ta->line_count = 1;
		}

		restart = false;
		for (; len > 0; len -= b_off, text += b_off) {
			/* Find end of paragraph */
			for (para_end = text; para_end < text + len;
					para_end++) {
				if (*para_end == '\n')
					break;
			}

			/* Wrap current line in paragraph */
			nsfont.font_split(&ta->fstyle, text, para_end - text,
					avail_width, &b_off, &x);
			/* b_off now marks space, or end of paragraph */

			if (x > h_extent) {
				h_extent = x;
			}
			if (x > avail_width && ta->bar_x == NULL) {
				/* We need to insert a horizontal scrollbar */
				int w = ta->vis_width - 2 * ta->border_width;
				if (!scrollbar_create(true, w, w, w,
						ta, textarea_scrollbar_callback,
						&(ta->bar_x)))
					return false;
				if (ta->bar_y != NULL)
					scrollbar_make_pair(ta->bar_x,
							ta->bar_y);
				ta->pad_bottom += SCROLLBAR_WIDTH;

				/* Find new max visible lines */
				scroll_lines = (ta->vis_height -
						2 * ta->border_width -
						ta->pad_top - ta->pad_bottom) /
						ta->line_height;
			}

			/* Ensure enough storage for lines data */
			if (line > ta->lines_alloc_size - 2) {
				/* Up to two lines my be added in a pass */
				struct line_info *temp = realloc(ta->lines,
						(line + 2 + LINE_CHUNK_SIZE) *
						sizeof(struct line_info));
				if (temp == NULL) {
					LOG(("realloc failed"));
					return false;
				}

				ta->lines = temp;
				ta->lines_alloc_size = line + 2 +
						LINE_CHUNK_SIZE;
			}

			if (para_end == text + b_off && *para_end == '\n') {
				/* Not found any spaces to wrap at, and we
				 * have a newline char */
				ta->lines[line].b_start = text - ta->text.data;
				ta->lines[line].b_length = para_end - text;
				ta->lines[line++].width = x;

				/* Jump newline */
				b_off++;

				if (len - b_off == 0) {
					/* reached end of input;
					 * add last line */
					ta->lines[line].b_start = text +
							b_off - ta->text.data;
					ta->lines[line].b_length = 0;
					ta->lines[line++].width = x;
				}

				if (line > scroll_lines && ta->bar_y == NULL)
					break;

				continue;

			} else if (len - b_off > 0) {
				/* soft wraped, find last space (if any) */
				for (space = text + b_off; space > text;
						space--) {
					if (*space == ' ')
						break;
				}

				if (space != text)
					b_off = space + 1 - text;
			}

			ta->lines[line].b_start = text - ta->text.data;
			ta->lines[line].b_length = b_off;
			ta->lines[line++].width = x;

			if (line > scroll_lines && ta->bar_y == NULL)
				break;
		}

		if (h_extent <= avail_width && ta->bar_x != NULL) {
			/* We need to remove a horizontal scrollbar */
			scrollbar_destroy(ta->bar_x);
			ta->bar_x = NULL;
			ta->pad_bottom -= SCROLLBAR_WIDTH;

			/* Find new max visible lines */
			scroll_lines = (ta->vis_height - 2 * ta->border_width -
					ta->pad_top - ta->pad_bottom) /
					ta->line_height;
		}

		if (line > scroll_lines && ta->bar_y == NULL) {
			/* Add vertical scrollbar */
			int h = ta->vis_height - 2 * ta->border_width;
			if (!scrollbar_create(false, h, h, h,
					ta, textarea_scrollbar_callback,
					&(ta->bar_y)))
				return false;
			if (ta->bar_x != NULL)
				scrollbar_make_pair(ta->bar_x,
						ta->bar_y);
			ta->pad_right += SCROLLBAR_WIDTH;
			restart = true;

		} else if (line <= scroll_lines && ta->bar_y != NULL) {
			/* Remove vertical scrollbar */
			scrollbar_destroy(ta->bar_y);
			ta->bar_y = NULL;
			ta->pad_right -= SCROLLBAR_WIDTH;
			restart = true;
		}
	} while (restart);

	h_extent += ta->pad_left + ta->pad_right -
			(ta->bar_y != NULL ? SCROLLBAR_WIDTH : 0);
	v_extent = line * ta->line_height + ta->pad_top +
				ta->pad_bottom -
				(ta->bar_x != NULL ? SCROLLBAR_WIDTH : 0);

	if (ta->bar_x != NULL) {
		/* Set horizontal scrollbar extents */
		int w = ta->vis_width - 2 * ta->border_width -
				(ta->bar_y != NULL ? SCROLLBAR_WIDTH : 0);
		scrollbar_set_extents(ta->bar_x, w, w, h_extent);
	}

	if (ta->bar_y != NULL) {
		/* Set vertical scrollbar extents */
		int h = ta->vis_height - 2 * ta->border_width;
		scrollbar_set_extents(ta->bar_y, h,
				h - (ta->bar_x != NULL ? SCROLLBAR_WIDTH : 0),
				v_extent);
	}

	ta->h_extent = h_extent;
	ta->v_extent = v_extent;
	ta->line_count = line;

	/* Don't need to redraw above changes, so update redraw request rect*/
	if (ta->lines[start].b_start + ta->lines[start].b_length < b_start &&
			restart == false) {
		/* Start line is unchanged */
		start++;
		skip_line = true;
	}

	r->y0 = max(r->y0, (signed)(ta->line_height * start +
			ta->text_y_offset - ta->scroll_y));

	/* Reduce redraw region to single line if possible */
	if ((skip_line || start == 0) &&
			ta->lines[start].b_start + ta->lines[start].b_length >=
			b_start + b_length) {
		size_t b_line_end = ta->lines[start].b_start +
				ta->lines[start].b_length;
		text = ta->text.data + b_line_end;
		if (*text == '\0' || *text == '\n') {
			r->y1 = min(r->y1, (signed)
					(ta->line_height * (start + 1) +
					ta->text_y_offset - ta->scroll_y));
			if (b_start > ta->lines[start].b_start &&
					b_start <= b_line_end) {
				/* Remove unchanged text at start of line
				 * from redraw region */
				int retained_width = 0;
				size_t retain_end = b_start -
						ta->lines[start].b_start;
				text = ta->text.data + ta->lines[start].b_start;

				nsfont.font_width(&ta->fstyle, text,
						retain_end, &retained_width);

				r->x0 = max(r->x0,
						retained_width +
						ta->border_width +
						ta->pad_left -
						ta->scroll_x - 1);
			}
		}
	}

	return true;
}


/**
 * get byte offset from the beginning of the text for some coordinates
 *
 * \param ta		textarea widget
 * \param x		X coordinate
 * \param y		Y coordinate
 * \param visible	true iff (x,y) is wrt visiable area, false for global
 * \return byte offset
 */
static size_t textarea_get_b_off_xy(struct textarea *ta, int x, int y,
		bool visible)
{
	size_t bpos; /* Byte position in utf8 string */
	int line;

	if (!ta->line_count) {
		return 0;
	}

	x = x - ta->border_width - ta->pad_left +
			(visible ? ta->scroll_x : 0);
	y = y - ta->border_width - ta->pad_top +
			(visible ? ta->scroll_y : 0);

	if (x < 0)
		x = 0;

	line = y / ta->line_height;

	if (ta->line_count - 1 < line)
		line = ta->line_count - 1;
	if (line < 0)
		line = 0;

	/* Get byte position */
	nsfont.font_position_in_string(&ta->fstyle,
			ta->show->data + ta->lines[line].b_start,
			ta->lines[line].b_length, x, &bpos, &x);


	/* If the calculated byte offset corresponds with the number of bytes
	 * in the line, and the line has been soft-wrapped, then ensure the
	 * caret offset is before the trailing space character, rather than
	 * after it. Otherwise, the caret will be placed at the start of the
	 * following line, which is undesirable.
	 */
	if (ta->flags & TEXTAREA_MULTILINE && ta->lines[line].b_length > 1 &&
			bpos == (unsigned)ta->lines[line].b_length &&
			ta->show->data[ta->lines[line].b_start +
					ta->lines[line].b_length - 1] == ' ')
		bpos--;

	/* Set the return byte offset */
	return bpos + ta->lines[line].b_start;
}


/**
 * Set the caret's position
 *
 * \param ta		textarea widget
 * \param x		X coordinate
 * \param y		Y coordinate
 * \param visible	true iff (x,y) is wrt visiable area, false for global
 * \return true iff caret placement caused a scroll
 */
static bool textarea_set_caret_xy(struct textarea *ta, int x, int y,
		bool visible)
{
	unsigned int b_off = textarea_get_b_off_xy(ta, x, y, visible);

	return textarea_set_caret_internal(ta, b_off);
}


/**
 * Insert text into the textarea
 *
 * \param ta		Textarea widget
 * \param text		UTF-8 text to insert
 * \param b_off		0-based byte offset in ta->show's text to insert at
 * \param b_len		Byte length of UTF-8 text
 * \param byte_delta	Updated to change in byte count in textarea (ta->show)
 * \param r		Modified/reduced to area where redraw is required
 * \return false on memory exhaustion, true otherwise
 *
 * Note: b_off must be for ta->show
 */
static bool textarea_insert_text(struct textarea *ta, const char *text,
		size_t b_off, size_t b_len, int *byte_delta, struct rect *r)
{
	int char_delta;
	const size_t show_b_off = b_off;

	if (ta->flags & TEXTAREA_READONLY)
		return true;

	/* If password field, we must convert from ta->password byte offset to
	 * ta->text byte offset */
	if (ta->flags & TEXTAREA_PASSWORD) {
		size_t c_off;

		c_off = utf8_bounded_length(ta->password.data, b_off);
		b_off = utf8_bounded_byte_length(ta->text.data,
				ta->text.len - 1, c_off);
	}

	/* Find insertion point */
	if (b_off > ta->text.len - 1)
		b_off = ta->text.len - 1;

	if (b_len + ta->text.len >= ta->text.alloc) {
		char *temp = realloc(ta->text.data, b_len + ta->text.len +
				TA_ALLOC_STEP);
		if (temp == NULL) {
			LOG(("realloc failed"));
			return false;
		}

		ta->text.data = temp;
		ta->text.alloc = b_len + ta->text.len + TA_ALLOC_STEP;
	}

	/* Shift text following up */
	memmove(ta->text.data + b_off + b_len, ta->text.data + b_off,
			ta->text.len - b_off);
	/* Insert new text */
	memcpy(ta->text.data + b_off, text, b_len);

	char_delta = ta->text.utf8_len;
	*byte_delta = ta->text.len;

	/* Update lengths, and normalise */
	ta->text.len += b_len;
	ta->text.utf8_len += utf8_bounded_length(text, b_len);
	textarea_normalise_text(ta, b_off, b_len);

	/* Get byte delta */
	if (ta->flags & TEXTAREA_PASSWORD) {
		char_delta = ta->text.utf8_len - char_delta;
		*byte_delta = char_delta * PASSWORD_REPLACEMENT_W;
	} else {
		*byte_delta = ta->text.len - *byte_delta;
	}

	/* See to reflow */
	if (ta->flags & TEXTAREA_MULTILINE) {
		if (!textarea_reflow_multiline(ta, show_b_off, b_len, r))
			return false;
	} else {
		if (!textarea_reflow_singleline(ta, show_b_off, r))
			return false;
	}

	return true;
}


/**
 * Helper for replace_text function converts character offset to byte offset
 *
 * text		utf8 textarea text object
 * start	start character offset
 * end		end character offset
 * b_start	updated to byte offset of start in text
 * b_end	updated to byte offset of end in text
 */
static inline void textarea_char_to_byte_offset(struct textarea_utf8 *text,
		unsigned int start, unsigned int end,
		size_t *b_start, size_t *b_end)
{
	size_t diff = end - start;
	/* find byte offset of replace start */
	for (*b_start = 0; start-- > 0;
			*b_start = utf8_next(text->data, text->len - 1,
					*b_start))
		; /* do nothing */

	/* find byte length of replaced text */
	for (*b_end = *b_start; diff-- > 0;
			*b_end = utf8_next(text->data, text->len - 1, *b_end))
		; /* do nothing */
}


/**
 * Perform actual text replacment in a textarea
 *
 * \param ta		Textarea widget
 * \param b_start	Start byte index of replaced section (inclusive)
 * \param b_end		End byte index of replaced section (exclusive)
 * \param rep		Replacement UTF-8 text to insert
 * \param rep_len	Byte length of replacement UTF-8 text
 * \param add_to_clipboard	True iff replaced text to be added to clipboard
 * \param byte_delta	Updated to change in byte count in textarea (ta->show)
 * \param r		Updated to area where redraw is required
 * \return false on memory exhaustion, true otherwise
 *
 * Note, b_start and b_end must be the byte offsets in ta->show, so in the
 * password textarea case, they are for ta->password.
 */
static bool textarea_replace_text_internal(struct textarea *ta, size_t b_start,
		size_t b_end, const char *rep, size_t rep_len,
		bool add_to_clipboard, int *byte_delta, struct rect *r)
{
	int char_delta;
	const size_t show_b_off = b_start;
	*byte_delta = 0;

	if ((ta->flags & TEXTAREA_READONLY) &&
			 !(rep == NULL && rep_len == 0 && add_to_clipboard))
		/* Can't edit if readonly, and we're not just copying */
		return true;

	if (b_start > ta->show->len - 1)
		b_start = ta->show->len - 1;
	if (b_end > ta->show->len - 1)
		b_end = ta->show->len - 1;

	/* Set up initial redraw rect */
	r->x0 = ta->border_width;
	r->y0 = ta->border_width;
	r->x1 = ta->vis_width - ta->border_width -
			((ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH);
	r->y1 = ta->vis_height - ta->border_width -
			((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH);

	/* Early exit if just inserting */
	if (b_start == b_end && rep != NULL)
		return textarea_insert_text(ta, rep, b_start, rep_len,
				byte_delta, r);

	if (b_start > b_end)
		return false;

	/* Place CUTs on clipboard */
	if (add_to_clipboard) {
		guit->clipboard->set(ta->show->data + b_start, b_end - b_start,
				NULL, 0);
	}

	if (rep == NULL) {
		/* No replacement text */
		return true;
	}

	/* If password field, we must convert from ta->password byte offset to
	 * ta->text byte offset */
	if (ta->flags & TEXTAREA_PASSWORD) {
		size_t c_start, c_end;

		c_start = utf8_bounded_length(ta->password.data, b_start);
		c_end = c_start;
		c_end += utf8_bounded_length(ta->password.data + b_start,
				b_end - b_start);
		textarea_char_to_byte_offset(&ta->text, c_start, c_end,
				&b_start, &b_end);
	}

	/* Ensure textarea's text buffer is large enough */
	if (rep_len + ta->text.len - (b_end - b_start) >= ta->text.alloc) {
		char *temp = realloc(ta->text.data,
				rep_len + ta->text.len - (b_end - b_start) +
					TA_ALLOC_STEP);
		if (temp == NULL) {
			LOG(("realloc failed"));
			return false;
		}

		ta->text.data = temp;
		ta->text.alloc = rep_len + ta->text.len - (b_end - b_start) +
				TA_ALLOC_STEP;
	}

	/* Shift text following to new position */
	memmove(ta->text.data + b_start + rep_len, ta->text.data + b_end,
			ta->text.len - b_end);

	/* Insert new text */
	memcpy(ta->text.data + b_start, rep, rep_len);

	char_delta = ta->text.utf8_len;
	*byte_delta = ta->text.len;

	/* Update lengths, and normalise */
	ta->text.len += (int)rep_len - (b_end - b_start);
	ta->text.utf8_len = utf8_length(ta->text.data);
	textarea_normalise_text(ta, b_start, rep_len);

	/* Get byte delta */
	if (ta->flags & TEXTAREA_PASSWORD) {
		char_delta = ta->text.utf8_len - char_delta;
		*byte_delta = char_delta * PASSWORD_REPLACEMENT_W;
	} else {
		*byte_delta = ta->text.len - *byte_delta;
	}

	/* See to reflow */
	if (ta->flags & TEXTAREA_MULTILINE) {
		if (!textarea_reflow_multiline(ta, b_start, *byte_delta, r))
			return false;
	} else {
		if (!textarea_reflow_singleline(ta, show_b_off, r))
			return false;
	}

	return true;
}


/**
 * Update undo buffer by adding any text to be replaced, and allocating
 * space as appropriate.
 *
 * \param ta		Textarea widget
 * \param b_start	Start byte index of replaced section (inclusive)
 * \param b_end		End byte index of replaced section (exclusive)
 * \param rep_len	Byte length of replacement UTF-8 text
 * \return false on memory exhaustion, true otherwise
 */
static bool textarea_copy_to_undo_buffer(struct textarea *ta,
		size_t b_start, size_t b_end, size_t rep_len)
{
	struct textarea_undo *undo;
	size_t b_offset;
	unsigned int len = b_end - b_start;

	undo = &ta->undo;

	if (undo->next_detail == 0)
		b_offset = 0;
	else
		b_offset = undo->details[undo->next_detail - 1].b_start +
			   undo->details[undo->next_detail - 1].b_limit;

	len = len > rep_len ? len : rep_len;

	if (b_offset + len >= undo->text.alloc) {
		/* Need more memory for undo buffer */
		char *temp = realloc(undo->text.data,
				b_offset + len + TA_ALLOC_STEP);
		if (temp == NULL) {
			LOG(("realloc failed"));
			return false;
		}

		undo->text.data = temp;
		undo->text.alloc = b_offset + len + TA_ALLOC_STEP;
	}

	if (undo->next_detail >= undo->details_alloc) {
		/* Need more memory for undo details */
		struct textarea_undo_detail *temp = realloc(undo->details,
				(undo->next_detail + 128) *
				sizeof(struct textarea_undo_detail));
		if (temp == NULL) {
			LOG(("realloc failed"));
			return false;
		}

		undo->details = temp;
		undo->details_alloc = undo->next_detail + 128;
	}

	/* Put text into buffer */
	memcpy(undo->text.data + b_offset, ta->text.data + b_start,
			b_end - b_start);

	/* Update next_detail */
	undo->details[undo->next_detail].b_start = b_offset;
	undo->details[undo->next_detail].b_end = b_offset + b_end - b_start;
	undo->details[undo->next_detail].b_limit = len;

	undo->details[undo->next_detail].b_text_start = b_start;

	return true;
}


/**
 * Replace text in a textarea, updating undo buffer.
 *
 * \param ta		Textarea widget
 * \param b_start	Start byte index of replaced section (inclusive)
 * \param b_end		End byte index of replaced section (exclusive)
 * \param rep		Replacement UTF-8 text to insert
 * \param rep_len	Byte length of replacement UTF-8 text
 * \param add_to_clipboard	True iff replaced text to be added to clipboard
 * \param byte_delta	Updated to change in byte count in textarea (ta->show)
 * \param r		Updated to area where redraw is required
 * \return false on memory exhaustion, true otherwise
 *
 * Note, b_start and b_end must be the byte offsets in ta->show, so in the
 * password textarea case, they are for ta->password.
 */
static bool textarea_replace_text(struct textarea *ta, size_t b_start,
		size_t b_end, const char *rep, size_t rep_len,
		bool add_to_clipboard, int *byte_delta, struct rect *r)
{
	if (!(b_start != b_end && rep == NULL && add_to_clipboard) &&
			!(ta->flags & TEXTAREA_PASSWORD)) {
		/* Not just copying to clipboard, and not a password field;
		 * Sort out undo buffer. */
		if (textarea_copy_to_undo_buffer(ta, b_start, b_end,
				rep_len) == false)
			return false;
	}

	/* Replace the text in the textarea, and reflow it */
	if (textarea_replace_text_internal(ta, b_start, b_end, rep, rep_len,
			add_to_clipboard, byte_delta, r) == false) {
		return false;
	}

	if (!(b_start != b_end && rep == NULL && add_to_clipboard) &&
			!(ta->flags & TEXTAREA_PASSWORD)) {
		/* Not just copying to clipboard, and not a password field;
		 * Update UNDO buffer */
		ta->undo.details[ta->undo.next_detail].b_text_end =
				b_end + *byte_delta;
		ta->undo.last_detail = ta->undo.next_detail;
		ta->undo.next_detail++;
	}

	return true;
}


/**
 * Undo or redo previous change.
 *
 * \param ta		Textarea widget
 * \param forward	Iff true, redo, else undo
 * \param caret		Updated to new caret pos in textarea (ta->show)
 * \param r		Updated to area where redraw is required
 * \return false if nothing to undo/redo, true otherwise
 */
static bool textarea_undo(struct textarea *ta, bool forward,
		unsigned int *caret, struct rect *r)
{
	unsigned int detail_n;
	struct textarea_undo_detail *detail;
	char *temp = NULL;
	unsigned int b_len;
	unsigned int b_text_len;
	int byte_delta;

	if (ta->flags & TEXTAREA_PASSWORD || ta->flags & TEXTAREA_READONLY)
		/* No undo/redo for password or readonly fields */
		return false;

	if (forward) {
		/* Redo */
		if (ta->undo.next_detail > ta->undo.last_detail)
			/* Nothing to redo */
			return false;

		detail_n = ta->undo.next_detail;
	} else {
		/* Undo */
		if (ta->undo.next_detail == 0)
			/* Nothing to undo */
			return false;

		detail_n = ta->undo.next_detail - 1;
	}

	detail = &(ta->undo.details[detail_n]);

	b_len = detail->b_end - detail->b_start;
	b_text_len = detail->b_text_end - detail->b_text_start;

	/* Take copy of any current textarea text that undo/redo will remove */
	if (detail->b_text_end > detail->b_text_start) {
		temp = malloc(b_text_len);
		if (temp == NULL) {
			/* TODO */
			return false;
		}

		memcpy(temp, ta->text.data + detail->b_text_start, b_text_len);
	}

	/* Replace textarea text with undo buffer text */
	textarea_replace_text_internal(ta,
			detail->b_text_start, detail->b_text_end,
			ta->undo.text.data + detail->b_start, b_len,
			false, &byte_delta, r);

	/* Update undo buffer for redo */
	if (temp != NULL)
		memcpy(ta->undo.text.data + detail->b_start, temp, b_text_len);

	detail->b_text_end = detail->b_text_start + b_len;
	detail->b_end = detail->b_start + b_text_len;

	*caret = detail->b_text_end;

	if (forward) {
		/* Redo */
		ta->undo.next_detail++;
	} else {
		/* Undo */
		ta->undo.next_detail--;
	}

	free(temp);

	return true;
}


/**
 * Handles the end of a drag operation
 *
 * \param ta	Text area
 * \param mouse	the mouse state at drag end moment
 * \param x	X coordinate
 * \param y	Y coordinate
 * \return true if drag end was handled false otherwise
 */
static bool textarea_drag_end(struct textarea *ta, browser_mouse_state mouse,
		int x, int y)
{
	size_t b_end;
	struct textarea_msg msg;

	assert(ta->drag_info.type != TEXTAREA_DRAG_NONE);

	switch (ta->drag_info.type) {
	case TEXTAREA_DRAG_SCROLLBAR:
		if (ta->drag_info.data.scrollbar == ta->bar_x) {
			x -= ta->border_width;
			y -= ta->vis_height - ta->border_width -
					SCROLLBAR_WIDTH;
		} else {
			x -= ta->vis_width - ta->border_width -
					SCROLLBAR_WIDTH;
			y -= ta->border_width;
		}
		scrollbar_mouse_drag_end(ta->drag_info.data.scrollbar,
				mouse, x, y);
		assert(ta->drag_info.type == TEXTAREA_DRAG_NONE);

		/* Return, since drag end already reported to textarea client */
		return true;

	case TEXTAREA_DRAG_SELECTION:
		ta->drag_info.type = TEXTAREA_DRAG_NONE;

		b_end = textarea_get_b_off_xy(ta, x, y, true);

		if (!textarea_select(ta, ta->drag_start, b_end, false))
			return false;

		break;

	default:
		return false;
	}

	/* Report drag end to client, if not already reported */
	assert(ta->drag_info.type == TEXTAREA_DRAG_NONE);

	msg.ta = ta;
	msg.type = TEXTAREA_MSG_DRAG_REPORT;
	msg.data.drag = ta->drag_info.type;

	ta->callback(ta->data, &msg);

	return true;
}


/**
 * Setup text offsets after height / border / padding change
 *
 * \param ta	Textarea widget
 */
static void textarea_setup_text_offsets(struct textarea *ta)
{
	int text_y_offset, text_y_offset_baseline;

	text_y_offset = text_y_offset_baseline = ta->border_width;
	if (ta->flags & TEXTAREA_MULTILINE) {
		/* Multiline textarea */
		text_y_offset += ta->pad_top;
		text_y_offset_baseline +=
				(ta->line_height * 3 + 2) / 4 + ta->pad_top;
	} else {
		/* Single line text area; text is vertically centered */
		int vis_height = ta->vis_height - 2 * ta->border_width;
		text_y_offset += (vis_height - ta->line_height + 1) / 2;
		text_y_offset_baseline +=
				(2 * vis_height + ta->line_height + 2) / 4;
	}

	ta->text_y_offset = text_y_offset;
	ta->text_y_offset_baseline = text_y_offset_baseline;
}



/* exported interface, documented in textarea.h */
struct textarea *textarea_create(const textarea_flags flags,
		const textarea_setup *setup,
		textarea_client_callback callback, void *data)
{
	struct textarea *ret;
	struct rect r = {0, 0, 0, 0};

	/* Sanity check flags */
	assert(!(flags & TEXTAREA_MULTILINE &&
			flags & TEXTAREA_PASSWORD));

	if (callback == NULL) {
		LOG(("no callback provided"));
		return NULL;
	}

	ret = malloc(sizeof(struct textarea));
	if (ret == NULL) {
		LOG(("malloc failed"));
		return NULL;
	}

	ret->callback = callback;
	ret->data = data;

	ret->flags = flags;
	ret->vis_width = setup->width;
	ret->vis_height = setup->height;

	ret->pad_top = setup->pad_top;
	ret->pad_right = setup->pad_right;
	ret->pad_bottom = setup->pad_bottom;
	ret->pad_left = setup->pad_left;

	ret->border_width = setup->border_width;
	ret->border_col = setup->border_col;

	ret->fstyle = setup->text;

	ret->sel_fstyle = setup->text;
	ret->sel_fstyle.foreground = setup->selected_text;
	ret->sel_fstyle.background = setup->selected_bg;

	ret->scroll_x = 0;
	ret->scroll_y = 0;
	ret->bar_x = NULL;
	ret->bar_y = NULL;
	ret->h_extent = setup->width;
	ret->v_extent = setup->height;
	ret->drag_start = 0;
	ret->drag_info.type = TEXTAREA_DRAG_NONE;

	ret->undo.details_alloc = 0;
	ret->undo.next_detail = 0;
	ret->undo.last_detail = 0;
	ret->undo.details = NULL;

	ret->undo.text.data = NULL;
	ret->undo.text.alloc = 0;
	ret->undo.text.len = 0;
	ret->undo.text.utf8_len = 0;


	ret->text.data = malloc(TA_ALLOC_STEP);
	if (ret->text.data == NULL) {
		LOG(("malloc failed"));
		free(ret);
		return NULL;
	}
	ret->text.data[0] = '\0';
	ret->text.alloc = TA_ALLOC_STEP;
	ret->text.len = 1;
	ret->text.utf8_len = 0;

	if (flags & TEXTAREA_PASSWORD) {
		ret->password.data = malloc(TA_ALLOC_STEP);
		if (ret->password.data == NULL) {
			LOG(("malloc failed"));
			free(ret->text.data);
			free(ret);
			return NULL;
		}
		ret->password.data[0] = '\0';
		ret->password.alloc = TA_ALLOC_STEP;
		ret->password.len = 1;
		ret->password.utf8_len = 0;

		ret->show = &ret->password;

	} else {
		ret->password.data = NULL;
		ret->password.alloc = 0;
		ret->password.len = 0;
		ret->password.utf8_len = 0;

		ret->show = &ret->text;
	}

	ret->line_height = FIXTOINT(FMUL(FLTTOFIX(1.3), FDIV(FMUL(
			nscss_screen_dpi, FDIV(INTTOFIX(setup->text.size),
			INTTOFIX(FONT_SIZE_SCALE))), F_72)));

	ret->caret_pos.line = ret->caret_pos.byte_off = -1;
	ret->caret_x = 0;
	ret->caret_y = 0;
	ret->sel_start = -1;
	ret->sel_end = -1;

	ret->line_count = 0;
	ret->lines = NULL;
	ret->lines_alloc_size = 0;

	textarea_setup_text_offsets(ret);

	if (flags & TEXTAREA_MULTILINE)
		 textarea_reflow_multiline(ret, 0, 0, &r);
	else
		 textarea_reflow_singleline(ret, 0, &r);

	return ret;
}


/* exported interface, documented in textarea.h */
void textarea_destroy(struct textarea *ta)
{
	if (ta->bar_x)
		scrollbar_destroy(ta->bar_x);
	if (ta->bar_y)
		scrollbar_destroy(ta->bar_y);

	if (ta->flags & TEXTAREA_PASSWORD)
		free(ta->password.data);

	free(ta->undo.text.data);
	free(ta->undo.details);

	free(ta->text.data);
	free(ta->lines);
	free(ta);
}


/* exported interface, documented in textarea.h */
bool textarea_set_text(struct textarea *ta, const char *text)
{
	unsigned int len = strlen(text) + 1;
	struct rect r = {0, 0, 0, 0};

	if (len >= ta->text.alloc) {
		char *temp = realloc(ta->text.data, len + TA_ALLOC_STEP);
		if (temp == NULL) {
			LOG(("realloc failed"));
			return false;
		}
		ta->text.data = temp;
		ta->text.alloc = len + TA_ALLOC_STEP;
	}

	memcpy(ta->text.data, text, len);
	ta->text.len = len;
	ta->text.utf8_len = utf8_length(ta->text.data);

	ta->undo.next_detail = 0;
	ta->undo.last_detail = 0;

	textarea_normalise_text(ta, 0, len);

	if (ta->flags & TEXTAREA_MULTILINE) {
		 if (!textarea_reflow_multiline(ta, 0, len - 1, &r))
		 	return false;
	} else {
		 if (!textarea_reflow_singleline(ta, 0, &r))
		 	return false;
	}

	return true;
}


/* exported interface, documented in textarea.h */
bool textarea_drop_text(struct textarea *ta, const char *text,
		size_t text_length)
{
	struct textarea_msg msg;
	struct rect r;	/**< Redraw rectangle */
	unsigned int caret_pos;
	int byte_delta;

	if (ta->flags & TEXTAREA_READONLY)
		return false;

	if (text == NULL)
		return false;

	caret_pos = textarea_get_caret(ta);

	if (ta->sel_start != -1) {
		if (!textarea_replace_text(ta, ta->sel_start, ta->sel_end,
				text, text_length, false, &byte_delta, &r))
			return false;

		caret_pos = ta->sel_end;
		ta->sel_start = ta->sel_end = -1;
	} else {
		if (!textarea_replace_text(ta, caret_pos, caret_pos,
				text, text_length, false, &byte_delta, &r))
			return false;
	}

	caret_pos += byte_delta;
	textarea_set_caret_internal(ta, caret_pos);

	msg.ta = ta;
	msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
	msg.data.redraw.x0 = 0;
	msg.data.redraw.y0 = 0;
	msg.data.redraw.x1 = ta->vis_width;
	msg.data.redraw.y1 = ta->vis_height;

	ta->callback(ta->data, &msg);

	return true;
}


/* exported interface, documented in textarea.h */
int textarea_get_text(struct textarea *ta, char *buf, unsigned int len)
{
	if (buf == NULL && len == 0) {
		/* want length */
		return ta->text.len;

	} else if (buf == NULL) {
		/* Can't write to NULL */
		return -1;
	}

	if (len < ta->text.len) {
		LOG(("buffer too small"));
		return -1;
	}

	memcpy(buf, ta->text.data, ta->text.len);

	return ta->text.len;
}


/* exported interface, documented in textarea.h */
bool textarea_set_caret(struct textarea *ta, int caret)
{
	int b_off;

	if (caret < 0) {
		textarea_set_caret_internal(ta, -1);
	} else if (caret == 0) {
		textarea_set_caret_internal(ta, 0);
	} else {
		b_off = utf8_bounded_byte_length(ta->show->data,
				ta->show->len - 1, caret);
		textarea_set_caret_internal(ta, b_off);
	}

	return true;
}


/* exported interface, documented in textarea.h */
void textarea_redraw(struct textarea *ta, int x, int y, colour bg, float scale,
		const struct rect *clip, const struct redraw_context *ctx)
{
	const struct plotter_table *plot = ctx->plot;
	int line0, line1, line, left, right, line_y;
	int text_y_offset, text_y_offset_baseline;
	unsigned int b_pos, b_len, b_len_part, b_end;
	unsigned int sel_start, sel_end;
	char *line_text;
	struct rect r, s;
	bool selected = false;
	plot_font_style_t fstyle;
	int fsize = ta->fstyle.size;
	int line_height = ta->line_height;
	plot_style_t plot_style_fill_bg = {
		.stroke_type = PLOT_OP_TYPE_NONE,
		.stroke_width = 0,
		.stroke_colour = NS_TRANSPARENT,
		.fill_type = PLOT_OP_TYPE_SOLID,
		.fill_colour = ta->border_col
	};

	r = *clip;

	/* Nothing to render if textarea is outside clip rectangle */
	if (r.x1 < x || r.y1 < y)
		return;
	if (scale == 1.0) {
		if (r.x0 > x + ta->vis_width || r.y0 > y + ta->vis_height)
			return;
	} else {
		if (r.x0 > x + ta->vis_width * scale ||
				r.y0 > y + ta->vis_height * scale)
			return;
	}

	if (ta->lines == NULL)
		/* Nothing to redraw */
		return;

	line0 = (r.y0 - y + ta->scroll_y) / ta->line_height - 1;
	line1 = (r.y1 - y + ta->scroll_y) / ta->line_height + 1;

	if (line0 < 0)
		line0 = 0;
	if (line1 < 0)
		line1 = 0;
	if (ta->line_count - 1 < line0)
		line0 = ta->line_count - 1;
	if (ta->line_count - 1 < line1)
		line1 = ta->line_count - 1;
	if (line1 < line0)
		line1 = line0;

	if (r.x0 < x)
		r.x0 = x;
	if (r.y0 < y)
		r.y0 = y;
	if (scale == 1.0) {
		if (r.x1 > x + ta->vis_width)
			r.x1 = x + ta->vis_width;
		if (r.y1 > y + ta->vis_height)
			r.y1 = y + ta->vis_height;
	} else {
		if (r.x1 > x + ta->vis_width * scale)
			r.x1 = x + ta->vis_width * scale;
		if (r.y1 > y + ta->vis_height * scale)
			r.y1 = y + ta->vis_height * scale;
	}

	plot->clip(&r);
	if (ta->border_col != NS_TRANSPARENT &&
			ta->border_width > 0) {
		/* Plot border */
		plot->rectangle(x, y, x + ta->vis_width, y + ta->vis_height,
				&plot_style_fill_bg);
	}
	if (ta->fstyle.background != NS_TRANSPARENT) {
		/* Plot background */
		plot_style_fill_bg.fill_colour = ta->fstyle.background;
		plot->rectangle(x + ta->border_width, y + ta->border_width,
				x + ta->vis_width - ta->border_width,
				y + ta->vis_height - ta->border_width,
				&plot_style_fill_bg);
	}

	if (scale == 1.0) {
		if (r.x0 < x + ta->border_width)
			r.x0 = x + ta->border_width;
		if (r.x1 > x + ta->vis_width - ta->border_width)
			r.x1 = x + ta->vis_width - ta->border_width;
		if (r.y0 < y + ta->border_width)
			r.y0 = y + ta->border_width;
		if (r.y1 > y + ta->vis_height - ta->border_width -
				(ta->bar_x != NULL ? SCROLLBAR_WIDTH : 0))
			r.y1 = y + ta->vis_height - ta->border_width -
					(ta->bar_x != NULL ? SCROLLBAR_WIDTH :
							0);
	} else {
		if (r.x0 < x + ta->border_width * scale)
			r.x0 = x + ta->border_width * scale;
		if (r.x1 > x + (ta->vis_width - ta->border_width) * scale)
			r.x1 = x + (ta->vis_width - ta->border_width) * scale;
		if (r.y0 < y + ta->border_width * scale)
			r.y0 = y + ta->border_width * scale;
		if (r.y1 > y + (ta->vis_height - ta->border_width -
				(ta->bar_x != NULL ? SCROLLBAR_WIDTH : 0)) *
				scale)
			r.y1 = y + (ta->vis_height - ta->border_width -
					(ta->bar_x != NULL ? SCROLLBAR_WIDTH :
							0)) * scale;
	}

	if (line0 > 0)
		b_pos = ta->lines[line0].b_start;
	else
		b_pos = 0;

	text_y_offset = ta->text_y_offset;
	text_y_offset_baseline = ta->text_y_offset_baseline;

	if (scale != 1.0) {
		text_y_offset *= scale;
		text_y_offset_baseline *= scale;

		fsize *= scale;
		line_height *= scale;
	}

	plot_style_fill_bg.fill_colour = ta->sel_fstyle.background;

	for (line = line0; (line <= line1) &&
			(y + line * ta->line_height <= r.y1 + ta->scroll_y);
			line++) {
		if (ta->lines[line].b_length == 0) {
			b_pos++;
			continue;
		}

		/* reset clip rectangle */
		plot->clip(&r);

		b_len = ta->lines[line].b_length;

		b_end = 0;
		right = x + ta->border_width + ta->pad_left - ta->scroll_x;

		line_y = line * ta->line_height - ta->scroll_y;

		if (scale != 1.0) {
			line_y *= scale;
		}

		sel_start = ta->sel_start;
		sel_end = ta->sel_end;

		if (ta->sel_end == -1 || ta->sel_end == ta->sel_start ||
				sel_end < ta->lines[line].b_start ||
				sel_start > ta->lines[line].b_start +
						ta->lines[line].b_length) {
			/* Simple case; no selection on this line */
			fstyle = ta->fstyle;
			fstyle.size = fsize;

			plot->text(x + ta->border_width + ta->pad_left -
					ta->scroll_x,
					y + line_y + text_y_offset_baseline,
					ta->show->data +
							ta->lines[line].b_start,
					ta->lines[line].b_length, &fstyle);

			b_pos += b_len;

		} else do {
			/* get length of part of line */
			if (sel_end <= b_pos || sel_start > b_pos + b_len) {
				/* rest of line unselected */
				selected = false;
				b_len_part = b_len;
				fstyle = ta->fstyle;

			} else if (sel_start <= b_pos &&
					sel_end > b_pos + b_len) {
				/* rest of line selected */
				selected = true;
				b_len_part = b_len;
				fstyle = ta->sel_fstyle;

			} else if (sel_start > b_pos) {
				/* next part of line unselected */
				selected = false;
				b_len_part = sel_start - b_pos;
				fstyle = ta->fstyle;

			} else if (sel_end > b_pos) {
				/* next part of line selected */
				selected = true;
				b_len_part = sel_end - b_pos;
				fstyle = ta->sel_fstyle;

			} else {
				assert(0);
			}
			fstyle.size = fsize;

			line_text = &(ta->show->data[ta->lines[line].b_start]);

			/* find b_end for this part of the line */
			b_end += b_len_part;

			/* find clip left/right for this part of line */
			left = right;
			if (b_len_part != b_len) {
				nsfont.font_width(&fstyle, line_text, b_end,
						&right);
			} else {
				right = ta->lines[line].width;
				if (scale != 1.0)
					right *= scale;
			}
			right += x + ta->border_width + ta->pad_left -
					ta->scroll_x;

			/* set clip rectangle for line part */
			s = r;

			if (s.x1 <= left || s.x0 > right) {
				/* Skip this span, it's outside the visible */
				b_pos += b_len_part;
				b_len -= b_len_part;
				continue;
			}

			/* Adjust clip rectangle to span limits */
			if (s.x0 < left)
				s.x0 = left;
			if (s.x1 > right)
				s.x1 = right;

			if (right <= left) {
				/* Skip this span, it's outside the visible */
				b_pos += b_len_part;
				b_len -= b_len_part;
				continue;
			}

			plot->clip(&s);

			if (selected) {
				/* draw selection fill */
				plot->rectangle(s.x0, y + line_y +
					text_y_offset,
					s.x1, y + line_y + line_height +
							text_y_offset,
					&plot_style_fill_bg);
			}

			/* draw text */
			plot->text(x + ta->border_width + ta->pad_left -
					ta->scroll_x,
					y + line_y + text_y_offset_baseline,
					ta->show->data +
							ta->lines[line].b_start,
					ta->lines[line].b_length, &fstyle);

			b_pos += b_len_part;
			b_len -= b_len_part;

		} while (b_pos < b_pos + b_len);

		/* if there is a newline between the lines, skip it */
		if (line < ta->line_count - 1 &&
				ta->lines[line + 1].b_start !=
						ta->lines[line].b_start +
						ta->lines[line].b_length)
			b_pos++;
	}

	if (ta->flags & TEXTAREA_INTERNAL_CARET &&
			(ta->sel_end == -1 || ta->sel_start == ta->sel_end) &&
			ta->caret_pos.byte_off >= 0) {
		/* No native caret, there is no selection, and caret visible */
		int caret_y = y - ta->scroll_y + ta->caret_y;

		plot->clip(&r);

		/* Render our own caret */
		plot->line(x - ta->scroll_x + ta->caret_x, caret_y,
				x - ta->scroll_x + ta->caret_x,
				caret_y + ta->line_height,
				&pstyle_stroke_caret);
	}

	plot->clip(clip);

	if (ta->bar_x != NULL)
		scrollbar_redraw(ta->bar_x,
				x / scale + ta->border_width,
				y / scale + ta->vis_height - ta->border_width -
						SCROLLBAR_WIDTH,
				clip, scale, ctx);

	if (ta->bar_y != NULL)
		scrollbar_redraw(ta->bar_y,
				x / scale + ta->vis_width - ta->border_width -
						SCROLLBAR_WIDTH,
				y / scale + ta->border_width,
				clip, scale, ctx);
}


/* exported interface, documented in textarea.h */
bool textarea_keypress(struct textarea *ta, uint32_t key)
{
	struct textarea_msg msg;
	struct rect r;	/**< Redraw rectangle */
	char utf8[6];
	unsigned int caret, length, b_off, b_len;
	int h_extent = ta->h_extent;
	int v_extent = ta->v_extent;
	int line;
	int byte_delta = 0;
	int x, y;
	bool redraw = false;
	bool readonly;
	bool bar_x = ta->bar_x;
	bool bar_y = ta->bar_y;

	/* Word separators */
	static const char *sep = " .\n";

	caret = textarea_get_caret(ta);
	line = ta->caret_pos.line;
	readonly = (ta->flags & TEXTAREA_READONLY ? true : false);

	if (!(key <= 0x001F || (0x007F <= key && key <= 0x009F))) {
		/* normal character insertion */
		length = utf8_from_ucs4(key, utf8);
		utf8[length] = '\0';

		if (ta->sel_start != -1) {
			if (!textarea_replace_text(ta,
					ta->sel_start, ta->sel_end, utf8,
					length, false, &byte_delta, &r))
				return false;

			redraw = true;
			caret = ta->sel_end;
			textarea_reset_selection(ta);
		} else {
			if (!textarea_replace_text(ta, caret, caret,
					utf8, length, false, &byte_delta, &r))
				return false;
			redraw = true;
		}
		caret += byte_delta;

	} else switch (key) {
		case KEY_SELECT_ALL:
			textarea_select(ta, 0, ta->show->len - 1, true);
			return true;
		case KEY_COPY_SELECTION:
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						NULL, 0, true, &byte_delta, &r))
					return false;
			}
			break;
		case KEY_DELETE_LEFT:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, false, &byte_delta, &r))
					return false;

				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else if (caret > 0) {
				b_off = utf8_prev(ta->show->data, caret);
				if (!textarea_replace_text(ta, b_off, caret,
						"", 0, false, &byte_delta, &r))
					return false;
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_DELETE_RIGHT:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, false, &byte_delta, &r))
					return false;

				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else if (caret < ta->show->len - 1) {
				b_off = utf8_next(ta->show->data,
						ta->show->len - 1, caret);
				if (!textarea_replace_text(ta, caret, b_off,
						"", 0, false, &byte_delta, &r))
					return false;
				caret = b_off;
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_CR:
		case KEY_NL:
			if (readonly)
				break;

			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"\n", 1, false,
						&byte_delta, &r))
					return false;

				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else {
				if (!textarea_replace_text(ta, caret, caret,
						"\n", 1, false,
						&byte_delta, &r))
					return false;
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_PASTE:
		{
			char *clipboard = NULL;
			size_t clipboard_length;

			if (readonly)
				break;

			guit->clipboard->get(&clipboard, &clipboard_length);
			if (clipboard == NULL)
				return false;

			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						clipboard, clipboard_length,
						false, &byte_delta, &r))
					return false;

				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else {
				if (!textarea_replace_text(ta,
						caret, caret,
						clipboard, clipboard_length,
						false, &byte_delta, &r))
					return false;
				redraw = true;
			}
			caret += byte_delta;

			free(clipboard);
		}
			break;
		case KEY_CUT_SELECTION:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, true, &byte_delta, &r))
					return false;

				redraw = true;
				caret = ta->sel_end;
				caret += byte_delta;
				textarea_reset_selection(ta);
			}
			break;
		case KEY_ESCAPE:
			/* Fall through to KEY_CLEAR_SELECTION */
		case KEY_CLEAR_SELECTION:
			return textarea_clear_selection(ta);
		case KEY_LEFT:
			if (readonly)
				break;
			if (caret > 0)
				caret = utf8_prev(ta->show->data, caret);
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_RIGHT:
			if (readonly)
				break;
			if (caret < ta->show->len - 1)
				caret = utf8_next(ta->show->data,
						ta->show->len - 1, caret);
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_UP:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			if (!(ta->flags & TEXTAREA_MULTILINE))
				break;

			line--;
			if (line < 0)
				line = 0;
			if (line == ta->caret_pos.line)
				break;

			x = ta->caret_x;
			y = ta->text_y_offset_baseline + line * ta->line_height;
			textarea_set_caret_xy(ta, x, y, false);

			return true;
		case KEY_DOWN:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			if (!(ta->flags & TEXTAREA_MULTILINE))
				break;

			line++;
			if (line > ta->line_count - 1)
				line = ta->line_count - 1;
			if (line == ta->caret_pos.line)
				break;

			x = ta->caret_x;
			y = ta->text_y_offset_baseline + line * ta->line_height;
			textarea_set_caret_xy(ta, x, y, false);

			return true;
		case KEY_PAGE_UP:
			if (!(ta->flags & TEXTAREA_MULTILINE))
				break;
			y = ta->vis_height - 2 * ta->border_width -
					ta->pad_top - ta->pad_bottom -
					ta->line_height;
			textarea_scroll(ta, 0, -y);
			return true;
		case KEY_PAGE_DOWN:
			if (!(ta->flags & TEXTAREA_MULTILINE))
				break;
			y = ta->vis_height - 2 * ta->border_width -
					ta->pad_top - ta->pad_bottom -
					ta->line_height;
			textarea_scroll(ta, 0, y);
			return true;
		case KEY_LINE_START:
			if (readonly)
				break;
			caret -= ta->caret_pos.byte_off;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_LINE_END:
			if (readonly)
				break;

			caret = ta->lines[line].b_start +
					ta->lines[line].b_length;

			if (!(ta->flags & TEXTAREA_PASSWORD) &&
					caret > 0 &&
					ta->text.data[caret - 1] == ' ')
				caret--;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_TEXT_START:
			if (readonly)
				break;
			caret = 0;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_TEXT_END:
			if (readonly)
				break;
			caret = ta->show->len - 1;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_WORD_LEFT:
			if (readonly)
				break;
			if (caret == 0)
				break;
			caret--;
			while (strchr(sep, ta->show->data[caret]) != NULL &&
					caret > 0)
				caret--;
			for (; caret > 0; caret--) {
				if (strchr(sep, ta->show->data[caret])
						!= NULL) {
					caret++;
					break;
				}
			}
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_WORD_RIGHT:
			if (readonly)
				break;
			if (strchr(sep, ta->show->data[caret]) != NULL &&
					caret < ta->show->len - 1) {
				while (strchr(sep, ta->show->data[caret]) !=
						NULL &&
						caret < ta->show->len - 1) {
					caret++;
				}
				break;
			}
			for (; caret < ta->show->len - 1; caret++) {
				if (strchr(sep, ta->show->data[caret]) != NULL)
					break;
			}
			while (strchr(sep, ta->show->data[caret]) != NULL &&
					caret < ta->show->len - 1)
				caret++;
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			break;
		case KEY_DELETE_LINE:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, false, &byte_delta, &r))
					return false;
				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else {
				if (ta->lines[line].b_length != 0) {
					/* Delete line */
					caret = ta->lines[line].b_start;
					b_len = ta->lines[line].b_length;
					if (!textarea_replace_text(ta, caret,
							caret + b_len, "", 0,
							false, &byte_delta, &r))
						return false;
					caret = caret + b_len;
				} else if (caret < ta->show->len - 1) {
					/* Delete blank line */
					if (!textarea_replace_text(ta,
							caret, caret + 1, "", 0,
							false, &byte_delta, &r))
						return false;
					caret++;
				}
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_DELETE_LINE_END:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, false, &byte_delta, &r))
					return false;
				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else {
				b_len = ta->lines[line].b_length;
				b_off = ta->lines[line].b_start + b_len;
				if (!textarea_replace_text(ta, caret, b_off,
						"", 0, false, &byte_delta, &r))
					return false;
				caret = b_off;
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_DELETE_LINE_START:
			if (readonly)
				break;
			if (ta->sel_start != -1) {
				if (!textarea_replace_text(ta,
						ta->sel_start, ta->sel_end,
						"", 0, false, &byte_delta, &r))
					return false;
				redraw = true;
				caret = ta->sel_end;
				textarea_reset_selection(ta);
			} else {
				if (!textarea_replace_text(ta,
						caret - ta->caret_pos.byte_off,
						caret, "", 0, false,
						&byte_delta, &r))
					return false;
				redraw = true;
			}
			caret += byte_delta;
			break;
		case KEY_UNDO:
			if (!textarea_undo(ta, false, &caret, &r)) {
				/* We consume the UNDO, even if we can't act
				 * on it. */
				return true;
			}
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			redraw = true;
			break;
		case KEY_REDO:
			if (!textarea_undo(ta, true, &caret, &r)) {
				/* We consume the REDO, even if we can't act
				 * on it. */
				return true;
			}
			if (ta->sel_start != -1) {
				textarea_clear_selection(ta);
			}
			redraw = true;
			break;
		default:
			return false;
	}

	redraw &= ~textarea_set_caret_internal(ta, caret);

	/* TODO: redraw only the bit that changed */
	msg.ta = ta;
	msg.type = TEXTAREA_MSG_REDRAW_REQUEST;

	if (bar_x != (ta->bar_x != NULL) || bar_y != (ta->bar_y != NULL) ||
			h_extent != ta->h_extent || v_extent != ta->v_extent) {
		/* Must redraw since scrollbars have changed */
		msg.data.redraw.x0 = ta->border_width;
		msg.data.redraw.y0 = ta->border_width;
		msg.data.redraw.x1 = ta->vis_width - ta->border_width;
		msg.data.redraw.y1 = ta->vis_height - ta->border_width;
		ta->callback(ta->data, &msg);

	} else if (redraw) {
		msg.data.redraw = r;
		ta->callback(ta->data, &msg);
	}

	return true;
}


/* Handle textarea scrollbar mouse action
 * Helper for textarea_mouse_action()
 *
 * \param ta	Text area
 * \param mouse	the mouse state at action moment
 * \param x	X coordinate
 * \param y	Y coordinate
 * \return textarea mouse state
 */
static textarea_mouse_status textarea_mouse_scrollbar_action(
		struct textarea *ta, browser_mouse_state mouse, int x, int y)
{
	int sx, sy; /* xy coord offset for scrollbar */
	int sl; /* scrollbar length */

	assert(SCROLLBAR_MOUSE_USED == (1 << 0));
	assert(TEXTAREA_MOUSE_SCR_USED == (1 << 3));

	/* Existing scrollbar drag */
	if (ta->drag_info.type == TEXTAREA_DRAG_SCROLLBAR) {
		/* Scrollbar drag in progress; pass input to scrollbar */
		if (ta->drag_info.data.scrollbar == ta->bar_x) {
			x -= ta->border_width;
			y -= ta->vis_height - ta->border_width -
					SCROLLBAR_WIDTH;
		} else {
			x -= ta->vis_width - ta->border_width -
					SCROLLBAR_WIDTH;
			y -= ta->border_width;
		}
		return (scrollbar_mouse_action(ta->drag_info.data.scrollbar,
				mouse, x, y) << 3);
	}

	/* Horizontal scrollbar */
	if (ta->bar_x != NULL && ta->drag_info.type == TEXTAREA_DRAG_NONE) {
		/* No drag happening, but mouse input is over scrollbar;
		 * pass input to scrollbar */
		sx = x - ta->border_width;
		sy = y - (ta->vis_height - ta->border_width - SCROLLBAR_WIDTH);
		sl = ta->vis_width - 2 * ta->border_width -
				(ta->bar_y != NULL ? SCROLLBAR_WIDTH : 0);

		if (sx >= 0 && sy >= 0 && sx < sl && sy < SCROLLBAR_WIDTH) {
			return (scrollbar_mouse_action(ta->bar_x, mouse,
					sx, sy) << 3);
		}
	}

	/* Vertical scrollbar */
	if (ta->bar_y != NULL && ta->drag_info.type == TEXTAREA_DRAG_NONE) {
		/* No drag happening, but mouse input is over scrollbar;
		 * pass input to scrollbar */
		sx = x - (ta->vis_width - ta->border_width - SCROLLBAR_WIDTH);
		sy = y - ta->border_width;
		sl = ta->vis_height - 2 * ta->border_width;

		if (sx >= 0 && sy >= 0 && sx < SCROLLBAR_WIDTH && sy < sl) {
			return (scrollbar_mouse_action(ta->bar_y, mouse,
					sx, sy) << 3);
		}
	}

	return TEXTAREA_MOUSE_NONE;
}


/* exported interface, documented in textarea.h */
textarea_mouse_status textarea_mouse_action(struct textarea *ta,
		browser_mouse_state mouse, int x, int y)
{
	int b_start, b_end;
	unsigned int b_off;
	struct textarea_msg msg;
	textarea_mouse_status status = TEXTAREA_MOUSE_NONE;

	if (ta->drag_info.type != TEXTAREA_DRAG_NONE &&
			mouse == BROWSER_MOUSE_HOVER) {
		/* There is a drag that we must end */
		textarea_drag_end(ta, mouse, x, y);
	}

	/* Mouse action might be a scrollbar's responsibility */
	status = textarea_mouse_scrollbar_action(ta, mouse, x, y);
	if (status != TEXTAREA_MOUSE_NONE) {
		/* Mouse action was handled by a scrollbar */
		return status;
	}

	/* Might be outside textarea, and not dragging */
	if ((x >= ta->vis_width || y >= ta->vis_height) &&
			ta->drag_info.type == TEXTAREA_DRAG_NONE &&
			ta->flags & TEXTAREA_MULTILINE) {
		return status;
	}

	status |= TEXTAREA_MOUSE_EDITOR;

	/* Mouse action is textarea's responsibility */
	if (mouse & BROWSER_MOUSE_DOUBLE_CLICK) {
		/* Select word */
		textarea_set_caret_xy(ta, x, y, true);
		textarea_select_fragment(ta);
		status |= TEXTAREA_MOUSE_USED;

	} else if (mouse & BROWSER_MOUSE_TRIPLE_CLICK) {
		/* Select paragraph */
		textarea_set_caret_xy(ta, x, y, true);
		textarea_select_paragraph(ta);
		status |= TEXTAREA_MOUSE_USED;

	} else if (mouse & BROWSER_MOUSE_PRESS_1) {
		/* Place caret */
		b_off = textarea_get_b_off_xy(ta, x, y, true);
		ta->drag_start = b_off;

		textarea_set_caret_internal(ta, b_off);
		if (ta->sel_start != -1) {
			/* Clear selection */
			textarea_clear_selection(ta);
		}
		status |= TEXTAREA_MOUSE_USED;

	} else if (mouse & BROWSER_MOUSE_PRESS_2) {
		b_off = textarea_get_b_off_xy(ta, x, y, true);

		if (ta->sel_start != -1) {
			/* Adjust selection */
			b_start = (ta->sel_end - ta->sel_start) / 2 +
					ta->sel_start;
			b_start = ((unsigned)b_start > b_off) ?
					ta->sel_end : ta->sel_start;
			ta->drag_start = b_start;
			textarea_select(ta, b_start, b_off, false);
		} else {
			/* Select to caret */
			b_start = textarea_get_caret(ta);
			ta->drag_start = b_start;
			textarea_select(ta, b_start, b_off, false);
		}
		status |= TEXTAREA_MOUSE_USED;

	} else if (mouse & (BROWSER_MOUSE_DRAG_1 | BROWSER_MOUSE_DRAG_2)) {
		/* Selection start */
		b_off = textarea_get_b_off_xy(ta, x, y, true);
		b_start = ta->drag_start;
		b_end = b_off;
		ta->drag_info.type = TEXTAREA_DRAG_SELECTION;

		msg.ta = ta;
		msg.type = TEXTAREA_MSG_DRAG_REPORT;
		msg.data.drag = ta->drag_info.type;

		ta->callback(ta->data, &msg);

		textarea_select(ta, b_start, b_end, false);
		status |= TEXTAREA_MOUSE_USED;

	} else if (mouse &
			(BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_HOLDING_2) &&
			ta->drag_info.type == TEXTAREA_DRAG_SELECTION) {
		/* Selection track */
		int scrx = 0;
		int scry = 0;
		int w, h;
		bool need_redraw = false;

		b_off = textarea_get_b_off_xy(ta, x, y, true);
		b_start = ta->drag_start;
		b_end = b_off;

		w = ta->vis_width - ta->border_width -
				((ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH);
		h = ta->vis_height - ta->border_width -
				((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH);

		/* selection auto-scroll */
		if (x < ta->border_width)
			scrx = (x - ta->border_width) / 4;
		else if (x > w)
			scrx = (x - w) / 4;

		if (y < ta->border_width)
			scry = (y - ta->border_width) / 4;
		else if (y > h)
			scry = (y - h) / 4;

		if (scrx || scry)
			need_redraw = textarea_scroll(ta, scrx, scry);

		textarea_select(ta, b_start, b_end, need_redraw);
		status |= TEXTAREA_MOUSE_USED;
	}

	if (ta->sel_start != -1) {
		/* Have selection */
		status |= TEXTAREA_MOUSE_SELECTION;
	}

	return status;
}


/* exported interface, documented in textarea.h */
bool textarea_clear_selection(struct textarea *ta)
{
	struct textarea_msg msg;
	int line_end, line_start = 0;

	if (ta->sel_start == -1)
		/* No selection to clear */
		return false;

	/* Find selection start & end lines */
	for (line_end = 0; line_end < ta->line_count - 1; line_end++)
		if (ta->lines[line_end + 1].b_start > (unsigned)ta->sel_start) {
			line_start = line_end;
			break;
		}
	for (; line_end < ta->line_count - 1; line_end++)
		if (ta->lines[line_end + 1].b_start > (unsigned)ta->sel_end)
			break;

	/* Clear selection and redraw */
	textarea_reset_selection(ta);

	msg.ta = ta;
	msg.type = TEXTAREA_MSG_REDRAW_REQUEST;
	msg.data.redraw.x0 = ta->border_width;
	msg.data.redraw.y0 = max(ta->border_width,
			ta->line_height * line_start +
			ta->text_y_offset - ta->scroll_y);
	msg.data.redraw.x1 = ta->vis_width - ta->border_width -
			((ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH);
	msg.data.redraw.y1 = min(ta->vis_height - ta->border_width -
			((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH),
			ta->line_height * line_end + ta->text_y_offset +
			ta->line_height - ta->scroll_y);

	ta->callback(ta->data, &msg);

	/* No more selection */
	msg.type = TEXTAREA_MSG_SELECTION_REPORT;

	msg.data.selection.have_selection = false;
	msg.data.selection.read_only = (ta->flags & TEXTAREA_READONLY);

	ta->callback(ta->data, &msg);

	if (!(ta->flags & TEXTAREA_INTERNAL_CARET)) {
		/* Tell client where caret should be placed */
		struct rect cr = {
			.x0 = ta->border_width,
			.y0 = ta->border_width,
			.x1 = ta->vis_width - ta->border_width -
					((ta->bar_y == NULL) ?
					0 : SCROLLBAR_WIDTH),
			.y1 = ta->vis_height - ta->border_width -
					((ta->bar_x == NULL) ?
					0 : SCROLLBAR_WIDTH)
		};
		msg.ta = ta;
		msg.type = TEXTAREA_MSG_CARET_UPDATE;
		msg.data.caret.type = TEXTAREA_CARET_SET_POS;
		msg.data.caret.pos.x = ta->caret_x - ta->scroll_x;
		msg.data.caret.pos.y = ta->caret_y - ta->scroll_y;
		msg.data.caret.pos.height = ta->line_height;
		msg.data.caret.pos.clip = &cr;

		ta->callback(ta->data, &msg);
	}

	return true;
}


/* exported interface, documented in textarea.h */
char *textarea_get_selection(struct textarea *ta)
{
	char *ret;
	size_t b_start, b_end, b_len;

	if (ta->sel_start == -1)
		/* No selection get */
		return NULL;

	b_start = ta->sel_start;
	b_end = ta->sel_end;
	b_len = b_end - b_start;

	if (b_len == 0)
		/* No selection get */
		return NULL;

	ret = malloc(b_len + 1); /* Add space for '\0' */
	if (ret == NULL)
		/* Can't get selection; no memory */
		return NULL;

	memcpy(ret, ta->show->data + b_start, b_len);
	ret[b_len] = '\0';

	return ret;
}


/* exported interface, documented in textarea.h */
void textarea_get_dimensions(struct textarea *ta, int *width, int *height)
{
	if (width != NULL)
		*width = ta->vis_width;
	if (height != NULL)
		*height = ta->vis_height;
}


/* exported interface, documented in textarea.h */
void textarea_set_dimensions(struct textarea *ta, int width, int height)
{
	struct rect r = {0, 0, 0, 0};

	ta->vis_width = width;
	ta->vis_height = height;

	textarea_setup_text_offsets(ta);

	if (ta->flags & TEXTAREA_MULTILINE) {
		 textarea_reflow_multiline(ta, 0, ta->show->len -1, &r);
	} else {
		 textarea_reflow_singleline(ta, 0, &r);
	}
}


/* exported interface, documented in textarea.h */
void textarea_set_layout(struct textarea *ta, int width, int height,
		int top, int right, int bottom, int left)
{
	struct rect r = {0, 0, 0, 0};

	ta->vis_width = width;
	ta->vis_height = height;
	ta->pad_top = top;
	ta->pad_right = right + ((ta->bar_y == NULL) ? 0 : SCROLLBAR_WIDTH);
	ta->pad_bottom = bottom + ((ta->bar_x == NULL) ? 0 : SCROLLBAR_WIDTH);
	ta->pad_left = left;

	textarea_setup_text_offsets(ta);

	if (ta->flags & TEXTAREA_MULTILINE) {
		 textarea_reflow_multiline(ta, 0, ta->show->len -1, &r);
	} else {
		 textarea_reflow_singleline(ta, 0, &r);
	}
}


/* exported interface, documented in textarea.h */
bool textarea_scroll(struct textarea *ta, int scrx, int scry)
{
	bool handled_scroll = false;

	if (ta->flags & TEXTAREA_MULTILINE) {
		/* Multi line textareas have scrollbars to handle this */
		if (ta->bar_x != NULL && scrx != 0 &&
				scrollbar_scroll(ta->bar_x, scrx))
			handled_scroll = true;
		if (ta->bar_y != NULL && scry != 0 &&
				scrollbar_scroll(ta->bar_y, scry))
			handled_scroll = true;

	} else {
		/* Single line.  Can only scroll horizontally. */
		int xs = ta->scroll_x;

		/* Apply offset */
		xs += scrx;

		/* Clamp to limits */
		if (xs < 0)
			xs = 0;
		else if (xs > ta->h_extent - ta->vis_width - ta->border_width)
			xs = ta->h_extent - ta->vis_width - ta->border_width;

		if (xs != ta->scroll_x) {
			ta->scroll_x = xs;
			handled_scroll = true;
		}
	}

	return handled_scroll;
}
