/*
 * Copyright 2006 John-Mark Bell <jmb202@ecs.soton.ac.uk>
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

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "swis.h"
#include "oslib/colourtrans.h"
#include "oslib/osbyte.h"
#include "oslib/serviceinternational.h"
#include "oslib/wimp.h"
#include "oslib/wimpspriteop.h"
#include "rufl.h"
#include "riscos/gui.h"
#include "riscos/oslib_pre7.h"
#include "riscos/textarea.h"
#include "riscos/ucstables.h"
#include "riscos/wimp.h"
#include "riscos/wimp_event.h"
#include "riscos/wimputils.h"
#include "utils/log.h"
#include "utils/utf8.h"

#define MARGIN_LEFT 8
#define MARGIN_RIGHT 8

struct line_info {
	unsigned int b_start;		/**< Byte offset of line start */
	unsigned int b_length;		/**< Byte length of line */
};

struct text_area {
#define MAGIC (('T'<<24) | ('E'<<16) | ('X'<<8) | 'T')
	unsigned int magic;		/**< Magic word, for sanity */

	unsigned int flags;		/**< Textarea flags */
	unsigned int vis_width;		/**< Visible width, in pixels */
	unsigned int vis_height;	/**< Visible height, in pixels */
	wimp_w window;			/**< Window handle */

	char *text;			/**< UTF-8 text */
	unsigned int text_alloc;	/**< Size of allocated text */
	unsigned int text_len;		/**< Length of text, in bytes */
	struct {
		unsigned int line;	/**< Line caret is on */
		unsigned int char_off;	/**< Character index of caret */
	} caret_pos;
//	unsigned int selection_start;	/**< Character index of sel start */
//	unsigned int selection_end;	/**< Character index of sel end */

	wimp_w parent;			/**< Parent window handle */
	wimp_i icon;			/**< Parent icon handle */

	char *font_family;		/**< Font family of text */
	unsigned int font_size;		/**< Font size (16ths/pt) */
	rufl_style font_style;		/**< Font style (rufl) */
	int line_height;		/**< Total height of a line, given font size */
	int line_spacing;		/**< Height of line spacing, given font size */

	unsigned int line_count;	/**< Count of lines */
#define LINE_CHUNK_SIZE 256
	struct line_info *lines;	/**< Line info array */

	struct text_area *next;		/**< Next text area in list */
	struct text_area *prev;		/**< Prev text area in list */
};

static wimp_window text_area_definition = {
	{0, 0, 16, 16},
	0,
	0,
	wimp_TOP,
	wimp_WINDOW_NEW_FORMAT | wimp_WINDOW_NO_BOUNDS,
	wimp_COLOUR_BLACK,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_LIGHT_GREY,
	wimp_COLOUR_VERY_LIGHT_GREY,
	wimp_COLOUR_DARK_GREY,
	wimp_COLOUR_MID_LIGHT_GREY,
	wimp_COLOUR_CREAM,
	0,
	{0, -16384, 16384, 0},
	wimp_ICON_TEXT | wimp_ICON_HCENTRED | wimp_ICON_VCENTRED,
	wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT,
	wimpspriteop_AREA,
	1,
	1,
	{""},
	0,
	{}
};

static void ro_textarea_reflow(struct text_area *ta, unsigned int line);
static bool ro_textarea_mouse_click(wimp_pointer *pointer);
static bool ro_textarea_key_press(wimp_key *key);
static void ro_textarea_redraw(wimp_draw *redraw);
static void ro_textarea_redraw_internal(wimp_draw *redraw, bool update);
static void ro_textarea_open(wimp_open *open);

/**
 * Create a text area
 *
 * \param parent Parent window
 * \param icon Icon in parent window to replace
 * \param flags Text area flags
 * \param font_family RUfl font family to use, or NULL for default
 * \param font_size Font size to use (pt * 16), or 0 for default
 * \param font_style Font style to use, or 0 for default
 * \return Opaque handle for textarea or 0 on error
 */
uintptr_t ro_textarea_create(wimp_w parent, wimp_i icon, unsigned int flags,
		const char *font_family, unsigned int font_size,
		rufl_style font_style)
{
	struct text_area *ret;
	os_error *error;

	ret = malloc(sizeof(struct text_area));
	if (!ret) {
		LOG(("malloc failed"));
		return 0;
	}

	ret->parent = parent;
	ret->icon = icon;
	ret->magic = MAGIC;
	ret->flags = flags;
	ret->text = malloc(64);
	if (!ret->text) {
		LOG(("malloc failed"));
		free(ret);
		return 0;
	}
	ret->text[0] = '\0';
	ret->text_alloc = 64;
	ret->text_len = 1;
	ret->caret_pos.line = ret->caret_pos.char_off = (unsigned int)-1;
//	ret->selection_start = (unsigned int)-1;
//	ret->selection_end = (unsigned int)-1;
	ret->font_family = strdup(font_family ? font_family : "Corpus");
	if (!ret->font_family) {
		LOG(("strdup failed"));
		free(ret->text);
		free(ret);
		return 0;
	}
	ret->font_size = font_size ? font_size : 192 /* 12pt */;
	ret->font_style = font_style ? font_style : rufl_WEIGHT_400;

	/** \todo Better line height calculation */
	ret->line_height = (int)(((ret->font_size * 1.3) / 16) * 2.0) + 1;
	ret->line_spacing = ret->line_height / 8;

	ret->line_count = 0;
	ret->lines = 0;

	if (flags & TEXTAREA_READONLY)
		text_area_definition.title_fg = 0xff;
	else
		text_area_definition.title_fg = wimp_COLOUR_BLACK;
	error = xwimp_create_window(&text_area_definition, &ret->window);
	if (error) {
		LOG(("xwimp_create_window: 0x%x: %s",
				error->errnum, error->errmess));
		free(ret->font_family);
		free(ret->text);
		free(ret);
		return 0;
	}

	/* set the window dimensions */
	if (!ro_textarea_update((uintptr_t)ret)) {
	 	ro_textarea_destroy((uintptr_t)ret);
		return 0;
	}

	/* and register our event handlers */
	ro_gui_wimp_event_set_user_data(ret->window, ret);
	ro_gui_wimp_event_register_mouse_click(ret->window,
			ro_textarea_mouse_click);
	ro_gui_wimp_event_register_keypress(ret->window,
			ro_textarea_key_press);
	ro_gui_wimp_event_register_redraw_window(ret->window,
			ro_textarea_redraw);
	ro_gui_wimp_event_register_open_window(ret->window,
			ro_textarea_open);

	return (uintptr_t)ret;
}

/**
 * Update the a text area following a change in the parent icon
 *
 * \param self Text area to update
 */
bool ro_textarea_update(uintptr_t self)
{
	struct text_area *ta;
	wimp_window_state state;
	wimp_icon_state istate;
	os_box extent;
	os_error *error;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC)
		return false;

	state.w = ta->parent;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	istate.w = ta->parent;
	istate.i = ta->icon;
	error = xwimp_get_icon_state(&istate);
	if (error) {
		LOG(("xwimp_get_icon_state: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	state.w = ta->window;
	state.visible.x1 = state.visible.x0 + istate.icon.extent.x1 -
			ro_get_vscroll_width(ta->window) - state.xscroll;
	state.visible.x0 += istate.icon.extent.x0 + 2 - state.xscroll;
	state.visible.y0 = state.visible.y1 + istate.icon.extent.y0 +
			ro_get_hscroll_height(ta->window) - state.yscroll;
	state.visible.y1 += istate.icon.extent.y1 - 2 - state.yscroll;

	if (ta->flags & TEXTAREA_READONLY) {
		state.visible.x0 += 2;
		state.visible.x1 -= 4;
		state.visible.y0 += 2;
		state.visible.y1 -= 4;
	}

	/* set our width/height */
	ta->vis_width = state.visible.x1 - state.visible.x0;
	ta->vis_height = state.visible.y1 - state.visible.y0;

	/* Set window extent to visible area */
	extent.x0 = 0;
	extent.y0 = -ta->vis_height;
	extent.x1 = ta->vis_width;
	extent.y1 = 0;

	error = xwimp_set_extent(ta->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	/* and open the window */
	error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state), ta->parent,
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_XORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_TOP_OR_RIGHT
					<< wimp_CHILD_YORIGIN_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_LS_EDGE_SHIFT |
			wimp_CHILD_LINKS_PARENT_VISIBLE_BOTTOM_OR_LEFT
					<< wimp_CHILD_RS_EDGE_SHIFT);
	if (error) {
		LOG(("xwimp_open_window_nested: 0x%x: %s",
				error->errnum, error->errmess));
		return false;
	}

	/* reflow the text */
	ro_textarea_reflow(ta, 0);
	return true;
}

/**
 * Destroy a text area
 *
 * \param self Text area to destroy
 */
void ro_textarea_destroy(uintptr_t self)
{
	struct text_area *ta;
	os_error *error;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC)
		return;

	error = xwimp_delete_window(ta->window);
	if (error) {
		LOG(("xwimp_delete_window: 0x%x: %s",
				error->errnum, error->errmess));
	}

	ro_gui_wimp_event_finalise(ta->window);

	free(ta->font_family);
	free(ta->text);
	free(ta);
}

/**
 * Set the text in a text area, discarding any current text
 *
 * \param self Text area
 * \param text UTF-8 text to set text area's contents to
 * \return true on success, false on memory exhaustion
 */
bool ro_textarea_set_text(uintptr_t self, const char *text)
{
	struct text_area *ta;
	unsigned int len = strlen(text) + 1;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return true;
	}

	if (len >= ta->text_alloc) {
		char *temp = realloc(ta->text, len + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return false;
		}
		ta->text = temp;
		ta->text_alloc = len+64;
	}

	memcpy(ta->text, text, len);
	ta->text_len = len;

	ro_textarea_reflow(ta, 0);

	return true;
}

/**
 * Extract the text from a text area
 *
 * \param self Text area
 * \param buf Pointer to buffer to receive data, or NULL
 *            to read length required
 * \param len Length (bytes) of buffer pointed to by buf, or 0 to read length
 * \return Length (bytes) written/required or -1 on error
 */
int ro_textarea_get_text(uintptr_t self, char *buf, unsigned int len)
{
	struct text_area *ta;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return -1;
	}

	if (buf == NULL && len == 0) {
		/* want length */
		return ta->text_len;
	}

	if (len < ta->text_len) {
		LOG(("buffer too small"));
		return -1;
	}

	memcpy(buf, ta->text, ta->text_len);

	return ta->text_len;
}

/**
 * Insert text into the text area
 *
 * \param self Text area
 * \param index 0-based character index to insert at
 * \param text UTF-8 text to insert
 */
void ro_textarea_insert_text(uintptr_t self, unsigned int index,
		const char *text)
{
	struct text_area *ta;
	unsigned int b_len = strlen(text);
	size_t b_off, c_len;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	/* Find insertion point */
	if (index > c_len)
		index = c_len;

	for (b_off = 0; index-- > 0;
			b_off = utf8_next(ta->text, ta->text_len, b_off))
		; /* do nothing */

	if (b_len + ta->text_len >= ta->text_alloc) {
		char *temp = realloc(ta->text, b_len + ta->text_len + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return;
		}

		ta->text = temp;
		ta->text_alloc = b_len + ta->text_len + 64;
	}

	/* Shift text following up */
	memmove(ta->text + b_off + b_len, ta->text + b_off,
			ta->text_len - b_off);
	/* Insert new text */
	memcpy(ta->text + b_off, text, b_len);

	ta->text_len += b_len;

	/** \todo calculate line to reflow from */
	ro_textarea_reflow(ta, 0);
}

/**
 * Replace text in a text area
 *
 * \param self Text area
 * \param start Start character index of replaced section (inclusive)
 * \param end End character index of replaced section (exclusive)
 * \param text UTF-8 text to insert
 */
void ro_textarea_replace_text(uintptr_t self, unsigned int start,
		unsigned int end, const char *text)
{
	struct text_area *ta;
	int b_len = strlen(text);
	size_t b_start, b_end, c_len, diff;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	if (start > c_len)
		start = c_len;
	if (end > c_len)
		end = c_len;

	if (start == end)
		return ro_textarea_insert_text(self, start, text);

	if (start > end) {
		int temp = end;
		end = start;
		start = temp;
	}

	diff = end - start;

	for (b_start = 0; start-- > 0;
			b_start = utf8_next(ta->text, ta->text_len, b_start))
		; /* do nothing */

	for (b_end = b_start; diff-- > 0;
			b_end = utf8_next(ta->text, ta->text_len, b_end))
		; /* do nothing */

	if (b_len + ta->text_len - (b_end - b_start) >= ta->text_alloc) {
		char *temp = realloc(ta->text,
			b_len + ta->text_len - (b_end - b_start) + 64);
		if (!temp) {
			LOG(("realloc failed"));
			return;
		}

		ta->text = temp;
		ta->text_alloc =
			b_len + ta->text_len - (b_end - b_start) + 64;
	}

	/* Shift text following to new position */
	memmove(ta->text + b_start + b_len, ta->text + b_end,
			ta->text_len - b_end);

	/* Insert new text */
	memcpy(ta->text + b_start, text, b_len);

	ta->text_len += b_len - (b_end - b_start);

	/** \todo calculate line to reflow from */
	ro_textarea_reflow(ta, 0);
}

/**
 * Set the caret's position
 *
 * \param self Text area
 * \param caret 0-based character index to place caret at
 */
void ro_textarea_set_caret(uintptr_t self, unsigned int caret)
{
	struct text_area *ta;
	size_t c_len, b_off;
	unsigned int i;
	size_t index;
	int x;
	os_coord os_line_height;
	rufl_code code;
	os_error *error;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	c_len = utf8_length(ta->text);

	if (caret > c_len)
		caret = c_len;

	/* Find byte offset of caret position */
	for (b_off = 0; caret > 0; caret--)
		b_off = utf8_next(ta->text, ta->text_len, b_off);

	/* Now find line in which byte offset appears */
	for (i = 0; i < ta->line_count - 1; i++)
		if (ta->lines[i + 1].b_start > b_off)
			break;

	ta->caret_pos.line = i;

	/* Now calculate the char. offset of the caret in this line */
	for (c_len = 0, ta->caret_pos.char_off = 0;
			c_len < b_off - ta->lines[i].b_start;
			c_len = utf8_next(ta->text + ta->lines[i].b_start,
					ta->lines[i].b_length, c_len))
		ta->caret_pos.char_off++;


	/* Finally, redraw the WIMP caret */
	index = ro_textarea_get_caret(self);
	os_line_height.x = 0;
	os_line_height.y = (int)((float)(ta->line_height - ta->line_spacing) * 0.62) + 1;
	ro_convert_pixels_to_os_units(&os_line_height, (os_mode)-1);

	for (b_off = 0; index-- > 0; b_off = utf8_next(ta->text, ta->text_len, b_off))
				; /* do nothing */

	code = rufl_width(ta->font_family, ta->font_style, ta->font_size,
			ta->text + ta->lines[ta->caret_pos.line].b_start,
			b_off - ta->lines[ta->caret_pos.line].b_start, &x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_width: 0x%x: %s",
				rufl_fm_error->errnum,
				rufl_fm_error->errmess));
		else
			LOG(("rufl_width: 0x%x", code));
		return;
	}

	error = xwimp_set_caret_position(ta->window, -1, x + MARGIN_LEFT,
			-((ta->caret_pos.line + 1) * ta->line_height) -
				ta->line_height / 4 + ta->line_spacing,
			os_line_height.y, -1);
	if (error) {
		LOG(("xwimp_set_caret_position: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
}

/**
 * Set the caret's position
 *
 * \param self Text area
 * \param x X position of caret on the screen
 * \param y Y position of caret on the screen
 */
void ro_textarea_set_caret_xy(uintptr_t self, int x, int y)
{
	struct text_area *ta;
	wimp_window_state state;
	size_t b_off, c_off, temp;
	int line;
	os_coord os_line_height;
	rufl_code code;
	os_error *error;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return;
	}

	if (ta->flags & TEXTAREA_READONLY)
		return;

	os_line_height.x = 0;
	os_line_height.y = (int)((float)(ta->line_height - ta->line_spacing) * 0.62) + 1;
	ro_convert_pixels_to_os_units(&os_line_height, (os_mode)-1);

	state.w = ta->window;
	error = xwimp_get_window_state(&state);
	if (error) {
		LOG(("xwimp_get_window_state: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	x = x - (state.visible.x0 - state.xscroll) - MARGIN_LEFT;
	y = (state.visible.y1 - state.yscroll) - y;

	line = y / ta->line_height;

	if (line < 0)
		line = 0;
	if (ta->line_count - 1 < (unsigned)line)
		line = ta->line_count - 1;

	code = rufl_x_to_offset(ta->font_family, ta->font_style,
			ta->font_size,
			ta->text + ta->lines[line].b_start,
			ta->lines[line].b_length,
			x, &b_off, &x);
	if (code != rufl_OK) {
		if (code == rufl_FONT_MANAGER_ERROR)
			LOG(("rufl_x_to_offset: 0x%x: %s",
					rufl_fm_error->errnum,
					rufl_fm_error->errmess));
		else
			LOG(("rufl_x_to_offset: 0x%x", code));
		return;
	}

	for (temp = 0, c_off = 0; temp < b_off + ta->lines[line].b_start;
			temp = utf8_next(ta->text, ta->text_len, temp))
		c_off++;

	ro_textarea_set_caret((uintptr_t)ta, c_off);
}

/**
 * Get the caret's position
 *
 * \param self Text area
 * \return 0-based character index of caret location, or -1 on error
 */
unsigned int ro_textarea_get_caret(uintptr_t self)
{
	struct text_area *ta;
	size_t c_off = 0, b_off;

	ta = (struct text_area *)self;
	if (!ta || ta->magic != MAGIC) {
		LOG(("magic doesn't match"));
		return -1;
	}

	/* Calculate character offset of this line's start */
	for (b_off = 0; b_off < ta->lines[ta->caret_pos.line].b_start;
			b_off = utf8_next(ta->text, ta->text_len, b_off))
		c_off++;

	return c_off + ta->caret_pos.char_off;
}

/** \todo Selection handling */

/**
 * Reflow a text area from the given line onwards
 *
 * \param ta Text area to reflow
 * \param line Line number to begin reflow on
 */
void ro_textarea_reflow(struct text_area *ta, unsigned int line)
{
	rufl_code code;
	char *text;
	unsigned int len;
	size_t b_off;
	int x;
	char *space;
	unsigned int line_count = 0;
	os_box extent;
	os_error *error;

	/** \todo pay attention to line parameter */
	/** \todo create horizontal scrollbar if needed */

	ta->line_count = 0;

	if (!ta->lines) {
		ta->lines =
			malloc(LINE_CHUNK_SIZE * sizeof(struct line_info));
		if (!ta->lines) {
			LOG(("malloc failed"));
			return;
		}
	}

	if (!(ta->flags & TEXTAREA_MULTILINE)) {
		/* Single line */
		ta->lines[line_count].b_start = 0;
		ta->lines[line_count++].b_length = ta->text_len - 1;

		ta->line_count = line_count;

		return;
	}

	for (len = ta->text_len - 1, text = ta->text; len > 0;
			len -= b_off, text += b_off) {
		code = rufl_split(ta->font_family, ta->font_style,
				ta->font_size, text, len,
				ta->vis_width - MARGIN_LEFT - MARGIN_RIGHT,
				&b_off, &x);
		if (code != rufl_OK) {
			if (code == rufl_FONT_MANAGER_ERROR)
				LOG(("rufl_x_to_offset: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
			else
				LOG(("rufl_x_to_offset: 0x%x", code));
			return;
		}

		if (line_count > 0 && line_count % LINE_CHUNK_SIZE == 0) {
			struct line_info *temp = realloc(ta->lines,
					(line_count + LINE_CHUNK_SIZE) *
					sizeof(struct line_info));
			if (!temp) {
				LOG(("realloc failed"));
				return;
			}

			ta->lines = temp;
		}

		/* handle CR/LF */
		for (space = text; space < text + b_off; space++) {
			if (*space == '\r' || *space == '\n')
				break;
		}

		if (space != text + b_off) {
			/* Found newline; use it */
			ta->lines[line_count].b_start = text - ta->text;
			ta->lines[line_count++].b_length = space - text;

			/* CRLF / LFCR pair */
			if (*space == '\r' && *(space + 1) == '\n')
				space++;
			else if (*space == '\n' && *(space + 1) == '\r')
				space++;

			b_off = space + 1 - text;

			if (len - b_off == 0) {
				/* reached end of input => add last line */
				ta->lines[line_count].b_start =
						text + b_off - ta->text;
				ta->lines[line_count++].b_length = 0;
			}

			continue;
		}

		if (len - b_off > 0) {
			/* find last space (if any) */
			for (space = text + b_off; space > text; space--)
				if (*space == ' ')
					break;

			if (space != text)
				b_off = space + 1 - text;
		}

		ta->lines[line_count].b_start = text - ta->text;
		ta->lines[line_count++].b_length = b_off;
	}

	ta->line_count = line_count;

	/* and now update extent */
	extent.x0 = 0;
	extent.y1 = 0;
	extent.x1 = ta->vis_width;
	extent.y0 = -ta->line_height * line_count - ta->line_spacing;

	if (extent.y0 > (int)-ta->vis_height)
		/* haven't filled window yet */
		return;

	error = xwimp_set_extent(ta->window, &extent);
	if (error) {
		LOG(("xwimp_set_extent: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	/* Create vertical scrollbar if we don't already have one */
	if (!ro_gui_wimp_check_window_furniture(ta->window,
			wimp_WINDOW_VSCROLL)) {
		wimp_window_state state;
		wimp_w parent;
		bits linkage;
		unsigned int vscroll_width;

		/* Save window parent & linkage flags */
		state.w = ta->window;
		error = xwimp_get_window_state_and_nesting(&state,
				&parent, &linkage);
		if (error) {
			LOG(("xwimp_get_window_state_and_nesting: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Now, attempt to create vertical scrollbar */
		ro_gui_wimp_update_window_furniture(ta->window,
				wimp_WINDOW_VSCROLL,
				wimp_WINDOW_VSCROLL);

		/* Get new window state */
		state.w = ta->window;
		error = xwimp_get_window_state(&state);
		if (error) {
			LOG(("xwimp_get_window_state: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* Get scroll width */
		vscroll_width = ro_get_vscroll_width(NULL);

		/* Shrink width by difference */
		state.visible.x1 -= vscroll_width;

		/* and reopen window */
		error = xwimp_open_window_nested(PTR_WIMP_OPEN(&state),
				parent, linkage);
		if (error) {
			LOG(("xwimp_open_window_nested: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		/* finally, update visible width */
		ta->vis_width -= vscroll_width;

		/* Now we've done that, we have to reflow the text area */
		ro_textarea_reflow(ta, 0);
	}
}

/**
 * Handle mouse clicks in a text area
 *
 * \param pointer Mouse click state block
 * \return true if click handled, false otherwise
 */
bool ro_textarea_mouse_click(wimp_pointer *pointer)
{
	struct text_area *ta;

	ta = (struct text_area *)ro_gui_wimp_event_get_user_data(pointer->w);

	ro_textarea_set_caret_xy((uintptr_t)ta, pointer->pos.x, pointer->pos.y);
	return true;
}

/**
 * Handle key presses in a text area
 *
 * \param key Key pressed state block
 * \param true if press handled, false otherwise
 */
bool ro_textarea_key_press(wimp_key *key)
{
	uint32_t c = (uint32_t) key->c;
	wimp_key keypress;
	struct text_area *ta;
	char utf8[7];
	size_t utf8_len;
	bool redraw = false;
	unsigned int c_pos;
	os_error *error;

	ta = (struct text_area *)ro_gui_wimp_event_get_user_data(key->w);

	if (ta->flags & TEXTAREA_READONLY)
		return true;

	if (!(c & IS_WIMP_KEY ||
			(c <= 0x001f || (0x007f <= c && c <= 0x009f)))) {
		/* normal character - insert */
		utf8_len = utf8_from_ucs4(c, utf8);
		utf8[utf8_len] = '\0';

		c_pos = ro_textarea_get_caret((uintptr_t)ta);
		ro_textarea_insert_text((uintptr_t)ta, c_pos, utf8);
		ro_textarea_set_caret((uintptr_t)ta, ++c_pos);

		redraw = true;
	} else {
		/** \todo handle command keys */
		switch (c & ~IS_WIMP_KEY) {
		case 8: /* Backspace */
			c_pos = ro_textarea_get_caret((uintptr_t)ta);
			if (c_pos > 0) {
				ro_textarea_replace_text((uintptr_t)ta,
					c_pos - 1, c_pos, "");
				ro_textarea_set_caret((uintptr_t)ta, c_pos - 1);
				redraw = true;
			}
			break;
		case 21: /* Ctrl + U */
			ro_textarea_set_text((uintptr_t)ta, "");
			ro_textarea_set_caret((uintptr_t)ta, 0);
			redraw = true;
			break;
		case wimp_KEY_DELETE:
			c_pos = ro_textarea_get_caret((uintptr_t)ta);
			if (os_version < RISCOS5 && c_pos > 0) {
				ro_textarea_replace_text((uintptr_t)ta,
						c_pos - 1, c_pos, "");
				ro_textarea_set_caret((uintptr_t)ta, c_pos - 1);
			} else {
				ro_textarea_replace_text((uintptr_t)ta, c_pos,
						c_pos + 1, "");
			}
			redraw = true;
			break;

		case wimp_KEY_LEFT:
			c_pos = ro_textarea_get_caret((uintptr_t)ta);
			if (c_pos > 0)
				ro_textarea_set_caret((uintptr_t)ta, c_pos - 1);
			break;
		case wimp_KEY_RIGHT:
			c_pos = ro_textarea_get_caret((uintptr_t)ta);
			ro_textarea_set_caret((uintptr_t)ta, c_pos + 1);
			break;
		case wimp_KEY_UP:
			/** \todo Move caret up a line */
			break;
		case wimp_KEY_DOWN:
			/** \todo Move caret down a line */
			break;

		case wimp_KEY_HOME:
		case wimp_KEY_CONTROL | wimp_KEY_LEFT:
			/** \todo line start */
			break;
		case wimp_KEY_CONTROL | wimp_KEY_RIGHT:
			/** \todo line end */
			break;
		case wimp_KEY_CONTROL | wimp_KEY_UP:
			ro_textarea_set_caret((uintptr_t)ta, 0);
			break;
		case wimp_KEY_CONTROL | wimp_KEY_DOWN:
			ro_textarea_set_caret((uintptr_t)ta,
					utf8_length(ta->text));
			break;

		case wimp_KEY_COPY:
			if (os_version < RISCOS5) {
				c_pos = ro_textarea_get_caret((uintptr_t)ta);
				ro_textarea_replace_text((uintptr_t)ta, c_pos,
						c_pos + 1, "");
			} else {
				/** \todo line end */
			}
			break;

		/** pass on RETURN and ESCAPE to the parent icon */
		case wimp_KEY_RETURN:
			if (ta->flags & TEXTAREA_MULTILINE) {
				/* Insert newline */
				c_pos = ro_textarea_get_caret((uintptr_t)ta);
				ro_textarea_insert_text((uintptr_t)ta, c_pos,
						"\n");
				ro_textarea_set_caret((uintptr_t)ta, ++c_pos);

				redraw = true;

				break;
			}
			/* fall through */
		case wimp_KEY_ESCAPE:
			keypress = *key;
			keypress.w = ta->parent;
			keypress.i = ta->icon;
			keypress.index = 0; /* undefined if not in an icon */
			error = xwimp_send_message_to_window(wimp_KEY_PRESSED,
					(wimp_message*)&keypress, ta->parent,
					ta->icon, 0);
			if (error) {
				LOG(("xwimp_send_message: 0x%x:%s",
					error->errnum, error->errmess));
			}
			break;
		}
	}

	if (redraw) {
		wimp_draw update;

		update.w = ta->window;
		update.box.x0 = 0;
		update.box.y1 = 0;
		update.box.x1 = ta->vis_width;
		update.box.y0 = -ta->line_height * (ta->line_count + 1);
		ro_textarea_redraw_internal(&update, true);
	}

	return true;
}

/**
 * Handle WIMP redraw requests for text areas
 *
 * \param redraw Redraw request block
 */
void ro_textarea_redraw(wimp_draw *redraw)
{
	ro_textarea_redraw_internal(redraw, false);
}

/**
 * Internal textarea redraw routine
 *
 * \param redraw Redraw/update request block
 * \param update True if update, false if full redraw
 */
void ro_textarea_redraw_internal(wimp_draw *redraw, bool update)
{
	struct text_area *ta;
	int clip_x0, clip_y0, clip_x1, clip_y1;
	int line0, line1, line;
	osbool more;
	rufl_code code;
	os_error *error;

	ta = (struct text_area *)ro_gui_wimp_event_get_user_data(redraw->w);

	if (update)
		error = xwimp_update_window(redraw, &more);
	else
		error = xwimp_redraw_window(redraw, &more);
	if (error) {
		LOG(("xwimp_redraw_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}

	while (more) {
		clip_x0 = redraw->clip.x0 - (redraw->box.x0-redraw->xscroll);
		clip_y0 = (redraw->box.y1-redraw->yscroll) - redraw->clip.y1;
		clip_x1 = redraw->clip.x1 - (redraw->box.x0-redraw->xscroll);
		clip_y1 = (redraw->box.y1-redraw->yscroll) - redraw->clip.y0;

		error = xcolourtrans_set_gcol(
				(ta->flags & TEXTAREA_READONLY) ? 0xD9D9D900
								: 0xFFFFFF00,
				colourtrans_SET_BG_GCOL | colourtrans_USE_ECFS_GCOL,
				os_ACTION_OVERWRITE, 0, 0);
		if (error) {
			LOG(("xcolourtrans_set_gcol: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		error = xos_clg();
		if (error) {
			LOG(("xos_clg: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}

		if (!ta->lines)
			/* Nothing to redraw */
			return;

		line0 = clip_y0 / ta->line_height - 1;
		line1 = clip_y1 / ta->line_height + 1;

		if (line0 < 0)
			line0 = 0;
		if (line1 < 0)
			line1 = 0;
		if (ta->line_count - 1 < (unsigned)line0)
			line0 = ta->line_count - 1;
		if (ta->line_count - 1 < (unsigned)line1)
			line1 = ta->line_count - 1;
		if (line1 < line0)
			line1 = line0;

		for (line = line0; line <= line1; line++) {
			if (ta->lines[line].b_length == 0)
				continue;

			error = xcolourtrans_set_font_colours(font_CURRENT,
					(ta->flags & TEXTAREA_READONLY) ?
						0xD9D9D900 : 0xFFFFFF00,
					0x00000000, 14, 0, 0, 0);
			if (error) {
				LOG(("xcolourtrans_set_font_colours: 0x%x: %s",
					error->errnum, error->errmess));
				return;
			}

			code = rufl_paint(ta->font_family, ta->font_style,
					ta->font_size,
					ta->text + ta->lines[line].b_start,
					ta->lines[line].b_length,
					redraw->box.x0 - redraw->xscroll + MARGIN_LEFT,
					redraw->box.y1 - redraw->yscroll -
						((line + 1) *
						ta->line_height - ta->line_spacing),
					rufl_BLEND_FONT);
			if (code != rufl_OK) {
				if (code == rufl_FONT_MANAGER_ERROR)
					LOG(("rufl_paint: rufl_FONT_MANAGER_ERROR: 0x%x: %s",
						rufl_fm_error->errnum,
						rufl_fm_error->errmess));
				else
					LOG(("rufl_paint: 0x%x", code));
			}
		}

		error = xwimp_get_rectangle(redraw, &more);
		if (error) {
			LOG(("xwimp_get_rectangle: 0x%x: %s",
					error->errnum, error->errmess));
			return;
		}
	}
}

/**
 * Handle a WIMP open window request
 *
 * \param open OpenWindow block
 */
void ro_textarea_open(wimp_open *open)
{
	os_error *error;

	error = xwimp_open_window(open);
	if (error) {
		LOG(("xwimp_open_window: 0x%x: %s",
				error->errnum, error->errmess));
		return;
	}
}
