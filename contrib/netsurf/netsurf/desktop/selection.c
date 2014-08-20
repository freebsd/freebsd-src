/*
 * Copyright 2005 Adrian Lees <adrianl@users.sourceforge.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
  * Text selection within browser windows (implementation).
  */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dom/dom.h>

#include "desktop/browser_private.h"
#include "desktop/gui_factory.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/save_text.h"
#include "desktop/selection.h"
#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/textplain.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"


/**
 * Text selection works by labelling each node in the box tree with its
 * start index in the textual representation of the tree's content.
 */

#define SPACE_LEN(b) ((b->space == 0) ? 0 : 1)


struct rdw_info {
	bool inited;
	struct rect r;
};

struct selection_string {
	char *buffer;
	size_t buffer_len;
	size_t length;

	int n_styles;
	nsclipboard_styles *styles;
};


typedef bool (*seln_traverse_handler)(const char *text, size_t length,
		struct box *box, void *handle, const char *whitespace_text,
		size_t whitespace_length);


static bool redraw_handler(const char *text, size_t length, struct box *box,
		void *handle, const char *whitespace_text,
		size_t whitespace_length);
static void selection_redraw(struct selection *s, unsigned start_idx,
		unsigned end_idx);
static bool selected_part(struct box *box, unsigned start_idx, unsigned end_idx,
		unsigned *start_offset, unsigned *end_offset);
static bool traverse_tree(struct box *box, unsigned start_idx, unsigned end_idx,
		seln_traverse_handler handler,
		void *handle, save_text_whitespace *before, bool *first,
		bool do_marker);
static unsigned selection_label_subtree(struct box *box, unsigned idx);

/**
 * Get the browser window containing the content a selection object belongs to.
 *
 * \param  s	selection object
 * \return the browser window
 */
static struct browser_window * selection_get_browser_window(struct selection *s)
{
	if (s->is_html)
		return html_get_browser_window(s->c);
	else
		return textplain_get_browser_window(s->c);
}


/**
 * Creates a new selection object associated with a browser window.
 *
 * \return new selection context
 */

struct selection *selection_create(struct content *c, bool is_html)
{
	struct selection *s = calloc(1, sizeof(struct selection));
	if (s) {
		selection_prepare(s, c, is_html);
	}

	return s;
}

/**
 * Prepare a newly created selection object for use.
 *
 * \param  s		selection object
 * \param  c		content
 * \param  is_html	true if content is html false if content is textplain
 */

void selection_prepare(struct selection *s, struct content *c, bool is_html)
{
	if (s) {
		s->c = c;
		s->is_html = is_html;
		s->root = NULL;
		s->drag_state = DRAG_NONE;
		s->max_idx = 0;
		selection_clear(s, false);
	}
}


/**
 * Destroys a selection object, without updating the
 * owning window (caller should call selection_clear()
 * first if update is desired)
 *
 * \param  s       selection object
 */

void selection_destroy(struct selection *s)
{
	if (s != NULL)
		free(s);
}


/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree, whilst maintaining the current
 * selection whenever possible because, for example, it's just the page being
 * resized causing the layout to change.
 *
 * \param  s     selection object
 * \param  root  the root box for html document or NULL for text/plain
 */

void selection_reinit(struct selection *s, struct box *root)
{
	unsigned root_idx;

	assert(s);

	root_idx = 0;

	s->root = root;
	if (root) {
		s->max_idx = selection_label_subtree(root, root_idx);
	} else {
		if (s->is_html == false)
			s->max_idx = textplain_size(s->c);
		else
			s->max_idx = 0;
	}

	if (s->defined) {
		if (s->end_idx > s->max_idx) s->end_idx = s->max_idx;
		if (s->start_idx > s->max_idx) s->start_idx = s->max_idx;
		s->defined = (s->end_idx > s->start_idx);
	}
}


/**
 * Initialise the selection object to use the given box subtree as its root,
 * ie. selections are confined to that subtree.
 *
 * \param  s     selection object
 * \param  root  the root box for html document or NULL for text/plain
 */

void selection_init(struct selection *s, struct box *root)
{
	if (s->defined)
		selection_clear(s, true);

	s->defined = false;
	s->start_idx = 0;
	s->end_idx = 0;
	s->drag_state = DRAG_NONE;

	selection_reinit(s, root);
}


/**
 * Label each text box in the given box subtree with its position
 * in a textual representation of the content.
 *
 * \param  s     selection object
 * \param  node  box at root of subtree
 * \param  idx   current position within textual representation
 * \return updated position
 */

unsigned selection_label_subtree(struct box *box, unsigned idx)
{
	struct box *child = box->children;

	box->byte_offset = idx;

	if (box->text)
		idx += box->length + SPACE_LEN(box);

	while (child) {
		if (child->list_marker)
			idx = selection_label_subtree(child->list_marker, idx);

		idx = selection_label_subtree(child, idx);
		child = child->next;
	}

	return idx;
}


/**
 * Handles mouse clicks (including drag starts) in or near a selection
 *
 * \param  s      selection object
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  idx    byte offset within textual representation
 *
 * \return true iff the click has been handled by the selection code
 */

bool selection_click(struct selection *s, browser_mouse_state mouse,
		unsigned idx)
{
	browser_mouse_state modkeys =
			(mouse & (BROWSER_MOUSE_MOD_1 | BROWSER_MOUSE_MOD_2));
	int pos = -1;  /* 0 = inside selection, 1 = after it */
	struct browser_window *top = selection_get_browser_window(s);
	top = browser_window_get_root(top);

	if (selection_defined(s)) {
		if (idx > s->start_idx) {
			if (idx <= s->end_idx)
				pos = 0;
			else
				pos = 1;
		}
	}

	if (!pos &&
		((mouse & BROWSER_MOUSE_DRAG_1) ||
		 (modkeys && (mouse & BROWSER_MOUSE_DRAG_2)))) {
		/* drag-saving selection */
		char *sel = selection_get_copy(s);
		guit->window->drag_save_selection(top->window, sel);
		free(sel);
	}
	else if (!modkeys) {
		if (pos && (mouse & BROWSER_MOUSE_PRESS_1)) {
		/* Clear the selection if mouse is pressed outside the
		 * selection, Otherwise clear on release (to allow for drags) */

			selection_clear(s, true);
		} else if (mouse & BROWSER_MOUSE_DRAG_1) {
			/* start new selection drag */

			selection_clear(s, true);
			
			selection_set_start(s, idx);
			selection_set_end(s, idx);

			s->drag_state = DRAG_END;

			guit->window->start_selection(top->window);
		}
		else if (mouse & BROWSER_MOUSE_DRAG_2) {

			/* adjust selection, but only if there is one */
			if (!selection_defined(s))
				return false;	/* ignore Adjust drags */

			if (pos >= 0) {
				selection_set_end(s, idx);

				s->drag_state = DRAG_END;
			}
			else {
				selection_set_start(s, idx);

				s->drag_state = DRAG_START;
			}

			guit->window->start_selection(top->window);
		}
		else if (mouse & BROWSER_MOUSE_CLICK_2) {

			/* ignore Adjust clicks when there's no selection */
			if (!selection_defined(s))
				return false;

			if (pos >= 0)
				selection_set_end(s, idx);
			else
				selection_set_start(s, idx);
			s->drag_state = DRAG_NONE;
		}
		else
			return false;
	}
	else {
		/* not our problem */
		return false;
	}

	/* this mouse click is selection-related */
	return true;
}


/**
 * Handles movements related to the selection, eg. dragging of start and
 * end points.
 *
 * \param  s      selection object
 * \param  mouse  state of mouse buttons and modifier keys
 * \param  idx    byte offset within text representation
 */

void selection_track(struct selection *s, browser_mouse_state mouse,
		unsigned idx)
{
	if (!mouse) {
		s->drag_state = DRAG_NONE;
	}

	switch (s->drag_state) {

		case DRAG_START:
			if (idx > s->end_idx) {
				unsigned old_end = s->end_idx;
				selection_set_end(s, idx);
				selection_set_start(s, old_end);
				s->drag_state = DRAG_END;
			}
			else
				selection_set_start(s, idx);
			break;

		case DRAG_END:
			if (idx < s->start_idx) {
				unsigned old_start = s->start_idx;
				selection_set_start(s, idx);
				selection_set_end(s, old_start);
				s->drag_state = DRAG_START;
			}
			else
				selection_set_end(s, idx);
			break;

		default:
			break;
	}
}


/**
 * Tests whether a text box lies partially within the given range of
 * byte offsets, returning the start and end indexes of the bytes
 * that are enclosed.
 *
 * \param  box           box to be tested
 * \param  start_idx     byte offset of start of range
 * \param  end_idx       byte offset of end of range
 * \param  start_offset  receives the start offset of the selected part
 * \param  end_offset    receives the end offset of the selected part
 * \return true iff the range encloses at least part of the box
 */

bool selected_part(struct box *box, unsigned start_idx, unsigned end_idx,
		unsigned *start_offset, unsigned *end_offset)
{
	size_t box_length = box->length + SPACE_LEN(box);

	if (box_length > 0) {
		if (box->byte_offset >= start_idx &&
			box->byte_offset + box_length <= end_idx) {

			/* fully enclosed */
			*start_offset = 0;
			*end_offset = box_length;
			return true;
		}
		else if (box->byte_offset + box_length > start_idx &&
			box->byte_offset < end_idx) {
			/* partly enclosed */
			int offset = 0;
			int len;

			if (box->byte_offset < start_idx)
				offset = start_idx - box->byte_offset;

			len = box_length - offset;

			if (box->byte_offset + box_length > end_idx)
				len = end_idx - (box->byte_offset + offset);

			*start_offset = offset;
			*end_offset = offset + len;

			return true;
		}
	}
	return false;
}


/**
 * Traverse the given box subtree, calling the handler function (with its handle)
 * for all boxes that lie (partially) within the given range
 *
 * \param  box        box subtree
 * \param  start_idx  start of range within textual representation (bytes)
 * \param  end_idx    end of range
 * \param  handler    handler function to call
 * \param  handle     handle to pass
 * \param  before     type of whitespace to place before next encountered text
 * \param  first      whether this is the first box with text
 * \param  do_marker  whether deal enter any marker box
 * \return false iff traversal abandoned part-way through
 */

bool traverse_tree(struct box *box, unsigned start_idx, unsigned end_idx,
		seln_traverse_handler handler,
		void *handle, save_text_whitespace *before, bool *first,
		bool do_marker)
{
	struct box *child;
	const char *whitespace_text = "";
	size_t whitespace_length = 0;

	assert(box);

	/* If selection starts inside marker */
	if (box->parent && box->parent->list_marker == box && !do_marker) {
		/* set box to main list element */
		box = box->parent;
	}

	/* If box has a list marker */
	if (box->list_marker) {
		/* do the marker box before continuing with the rest of the
		 * list element */
		if (!traverse_tree(box->list_marker, start_idx, end_idx,
				handler, handle, before, first,
				true))
			return false;
	}

	/* we can prune this subtree, it's after the selection */
	if (box->byte_offset >= end_idx)
		return true;

	/* read before calling the handler in case it modifies the tree */
	child = box->children;

	/* If nicely formatted output of the selected text is required, work
	 * out what whitespace should be placed before the next bit of text */
	if (before) {
		save_text_solve_whitespace(box, first, before, &whitespace_text,
				&whitespace_length);
	}
	else {
		whitespace_text = NULL;
	}
	if (box->type != BOX_BR &&
			!((box->type == BOX_FLOAT_LEFT ||
			box->type == BOX_FLOAT_RIGHT) &&
			!box->text)) {
		unsigned start_offset;
		unsigned end_offset;

		if (selected_part(box, start_idx, end_idx, &start_offset,
				&end_offset)) {
			if (!handler(box->text + start_offset, min(box->length,
					end_offset) - start_offset,
					box, handle, whitespace_text,
					whitespace_length))
				return false;
			if (before) {
				*first = false;
				*before = WHITESPACE_NONE;
			}
		}
	}

	/* find the first child that could lie partially within the selection;
	 * this is important at the top-levels of the tree for pruning subtrees
	 * that lie entirely before the selection */

	if (child) {
		struct box *next = child->next;

		while (next && next->byte_offset < start_idx) {
			child = next;
			next = child->next;
		}

		while (child) {
			/* read before calling the handler in case it modifies
			 * the tree */
			struct box *next = child->next;

			if (!traverse_tree(child, start_idx, end_idx,
					handler, handle, before, first, false))
				return false;

			child = next;
		}
	}

	return true;
}


/**
 * Traverse the current selection, calling the handler function (with its
 * handle) for all boxes that lie (partially) within the given range
 *
 * \param  handler  handler function to call
 * \param  handle   handle to pass
 * \return false iff traversal abandoned part-way through
 */

static bool selection_traverse(struct selection *s,
		seln_traverse_handler handler, void *handle)
{
	save_text_whitespace before = WHITESPACE_NONE;
	bool first = true;
	const char *text;
	size_t length;

	if (!selection_defined(s))
		return true;	/* easy case, nothing to do */

	if (s->root) {
		/* HTML */
		return traverse_tree(s->root, s->start_idx, s->end_idx,
				handler, handle, &before, &first, false);
	}

	/* Text */
	text = textplain_get_raw_data(s->c, s->start_idx, s->end_idx, &length);

	if (text && !handler(text, length, NULL, handle, NULL, 0))
		return false;

	return true;
}


/**
 * Selection traversal handler for redrawing the screen when the selection
 * has been altered.
 *
 * \param  text		pointer to text string
 * \param  length	length of text to be appended (bytes)
 * \param  box		pointer to text box being (partially) added
 * \param  handle	unused handle, we don't need one
 * \param  whitespace_text    whitespace to place before text for formatting
 *                            may be NULL
 * \param  whitespace_length  length of whitespace_text
 * \return true iff successful and traversal should continue
 */

bool redraw_handler(const char *text, size_t length, struct box *box,
		void *handle, const char *whitespace_text,
		size_t whitespace_length)
{
	if (box) {
		struct rdw_info *r = (struct rdw_info*)handle;
		int width, height;
		int x, y;
		plot_font_style_t fstyle;

		font_plot_style_from_css(box->style, &fstyle);

		/* \todo - it should be possible to reduce the redrawn area by
		 * considering the 'text', 'length' and 'space' parameters */
		box_coords(box, &x, &y);

		width = box->padding[LEFT] + box->width + box->padding[RIGHT];
		height = box->padding[TOP] + box->height + box->padding[BOTTOM];

		if (box->type == BOX_TEXT && box->space != 0)
			width += box->space;

		if (r->inited) {
			if (x < r->r.x0) r->r.x0 = x;
			if (y < r->r.y0) r->r.y0 = y;
			if (x + width > r->r.x1) r->r.x1 = x + width;
			if (y + height > r->r.y1) r->r.y1 = y + height;
		}
		else {
			r->inited = true;
			r->r.x0 = x;
			r->r.y0 = y;
			r->r.x1 = x + width;
			r->r.y1 = y + height;
		}
	}
	return true;
}


/**
 * Redraws the given range of text.
 *
 * \param  s          selection object
 * \param  start_idx  start offset (bytes) within the textual representation
 * \param  end_idx    end offset (bytes) within the textual representation
 */

void selection_redraw(struct selection *s, unsigned start_idx, unsigned end_idx)
{
	struct rdw_info rdw;

	assert(end_idx >= start_idx);
	rdw.inited = false;

	if (s->root) {
		if (!traverse_tree(s->root, start_idx, end_idx,
				redraw_handler, &rdw,
				NULL, NULL, false))
			return;
	}
	else {
		if (s->is_html == false && end_idx > start_idx) {
			textplain_coords_from_range(s->c, start_idx,
							end_idx, &rdw.r);
			rdw.inited = true;
		}
	}

	if (rdw.inited)
		content__request_redraw(s->c, rdw.r.x0, rdw.r.y0,
				rdw.r.x1 - rdw.r.x0, rdw.r.y1 - rdw.r.y0);
}


/**
 * Append text to selection string.
 *
 * \param  text        text to be added
 * \param  length      length of text in bytes
 * \param  space       indicates whether a trailing space should be appended
 * \param  sel_string  string to append to, may be resized
 * \return true iff successful
 */

static bool selection_string_append(const char *text, size_t length, bool space,
		plot_font_style_t *style, struct selection_string *sel_string)
{
	size_t new_length = sel_string->length + length + (space ? 1 : 0) + 1;

	if (style != NULL) {
		/* Add text run style */
		nsclipboard_styles *new_styles;

		if (sel_string->n_styles == 0)
			assert(sel_string->length == 0);

		new_styles = realloc(sel_string->styles,
				(sel_string->n_styles + 1) *
				sizeof(nsclipboard_styles));
		if (new_styles == NULL)
			return false;

		sel_string->styles = new_styles;

		sel_string->styles[sel_string->n_styles].style = *style;
		sel_string->styles[sel_string->n_styles].start =
				sel_string->length;

		sel_string->n_styles++;
	}

	if (new_length > sel_string->buffer_len) {
		/* Need to extend buffer */
		size_t new_alloc = new_length + (new_length / 4);
		char *new_buff;

		new_buff = realloc(sel_string->buffer, new_alloc);
		if (new_buff == NULL)
			return false;

		sel_string->buffer = new_buff;
		sel_string->buffer_len = new_alloc;
	}

	/* Copy text onto end of existing text in buffer */
	memcpy(sel_string->buffer + sel_string->length, text, length);
	sel_string->length += length;

	if (space)
		sel_string->buffer[sel_string->length++] = ' ';

	/* Ensure NULL termination */
	sel_string->buffer[sel_string->length] = '\0';

	return true;
}


/**
 * Selection traversal routine for appending text to a string
 *
 * \param  text		pointer to text being added, or NULL for newline
 * \param  length	length of text to be appended (bytes)
 * \param  box		pointer to text box, or NULL if from textplain
 * \param  handle	selection string to append to
 * \param  whitespace_text    whitespace to place before text for formatting
 *                            may be NULL
 * \param  whitespace_length  length of whitespace_text
 * \return true iff successful and traversal should continue
 */

static bool selection_copy_handler(const char *text, size_t length,
		struct box *box, void *handle, const char *whitespace_text,
		size_t whitespace_length)
{
	bool add_space = false;
	plot_font_style_t style;
	plot_font_style_t *pstyle = NULL;

	/* add any whitespace which precedes the text from this box */
	if (whitespace_text != NULL && whitespace_length > 0) {
		if (!selection_string_append(whitespace_text,
				whitespace_length, false, pstyle, handle)) {
			return false;
		}
	}

	if (box != NULL) {
		/* HTML */
		add_space = (box->space != 0);

		if (box->style != NULL) {
			/* Override default font style */
			font_plot_style_from_css(box->style, &style);
			pstyle = &style;
		} else {
			/* If there's no style, there must be no text */
			assert(box->text == NULL);
		}
	}

	/* add the text from this box */
	if (!selection_string_append(text, length, add_space, pstyle, handle))
		return false;

	return true;
}


/**
 * Get copy of selection as string
 *
 * \param s  selection
 * \return string of selected text, or NULL.  Ownership passed to caller.
 */

char *selection_get_copy(struct selection *s)
{
	struct selection_string sel_string = {
		.buffer = NULL,
		.buffer_len = 0,
		.length = 0,

		.n_styles = 0,
		.styles = NULL
	};

	if (s == NULL || !s->defined)
		return NULL;

	if (!selection_traverse(s, selection_copy_handler, &sel_string)) {
		free(sel_string.buffer);
		free(sel_string.styles);
		return NULL;
	}

	free(sel_string.styles);

	return sel_string.buffer;
}



/**
 * Copy the selected contents to the clipboard
 *
 * \param s  selection
 * \return true iff successful
 */
bool selection_copy_to_clipboard(struct selection *s)
{
	struct selection_string sel_string = {
		.buffer = NULL,
		.buffer_len = 0,
		.length = 0,

		.n_styles = 0,
		.styles = NULL
	};

	if (s == NULL || !s->defined)
		return false;

	if (!selection_traverse(s, selection_copy_handler, &sel_string)) {
		free(sel_string.buffer);
		free(sel_string.styles);
		return false;
	}

	guit->clipboard->set(sel_string.buffer, sel_string.length,
			sel_string.styles, sel_string.n_styles);

	free(sel_string.buffer);
	free(sel_string.styles);

	return true;
}


/**
 * Clears the current selection, optionally causing the screen to be updated.
 *
 * \param  s       selection object
 * \param  redraw  true iff the previously selected region of the browser
 *                window should be redrawn
 */

void selection_clear(struct selection *s, bool redraw)
{
	int old_start, old_end;
	bool was_defined;

	assert(s);
	was_defined = selection_defined(s);
	old_start = s->start_idx;
	old_end = s->end_idx;

	s->defined = false;
	s->start_idx = 0;
	s->end_idx = 0;

	if (redraw && was_defined)
		selection_redraw(s, old_start, old_end);
}


/**
 * Selects all the text within the box subtree controlled by
 * this selection object, updating the screen accordingly.
 *
 * \param  s  selection object
 */

void selection_select_all(struct selection *s)
{
	assert(s);
	s->defined = true;
	
	selection_set_start(s, 0);
	selection_set_end(s, s->max_idx);
}


/**
 * Set the start position of the current selection, updating the screen.
 *
 * \param  s       selection object
 * \param  offset  byte offset within textual representation
 */

void selection_set_start(struct selection *s, unsigned offset)
{
	bool was_defined = selection_defined(s);
	unsigned old_start = s->start_idx;
	
	s->start_idx = offset;
	s->defined = (s->start_idx < s->end_idx);
	
	if (was_defined) {
		if (offset < old_start)
			selection_redraw(s, s->start_idx, old_start);
		else
			selection_redraw(s, old_start, s->start_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
}


/**
 * Set the end position of the current selection, updating the screen.
 *
 * \param  s       selection object
 * \param  offset  byte offset within textual representation
 */

void selection_set_end(struct selection *s, unsigned offset)
{
	bool was_defined = selection_defined(s);
	unsigned old_end = s->end_idx;

	s->end_idx = offset;
	s->defined = (s->start_idx < s->end_idx);

	if (was_defined) {
		if (offset < old_end)
			selection_redraw(s, s->end_idx, old_end);
		else
			selection_redraw(s, old_end, s->end_idx);
	}
	else if (selection_defined(s))
		selection_redraw(s, s->start_idx, s->end_idx);
}


/**
 * Tests whether a text range lies partially within the selection, if there is
 * a selection defined, returning the start and end indexes of the bytes
 * that should be selected.
 *
 * \param  s          the selection object
 * \param  start      byte offset of start of text
 * \param  start_idx  receives the start index (in bytes) of the highlighted portion
 * \param  end_idx    receives the end index (in bytes)
 * \return true iff part of the given box lies within the selection
 */

bool selection_highlighted(const struct selection *s,
		unsigned start, unsigned end,
		unsigned *start_idx, unsigned *end_idx)
{
	/* caller should have checked first for efficiency */
	assert(s);
	assert(selection_defined(s));

	if (end <= s->start_idx || start >= s->end_idx)
		return false;

	*start_idx = (s->start_idx >= start) ? (s->start_idx - start) : 0;
	*end_idx = min(end, s->end_idx) - start;

	return true;
}
