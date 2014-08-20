/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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
  * Text export of HTML (implementation).
  */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <dom/dom.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "render/box.h"
#include "render/html.h"

#include "desktop/gui_factory.h"
#include "desktop/save_text.h"

static void extract_text(struct box *box, bool *first,
		save_text_whitespace *before, struct save_text_state *save);
static bool save_text_add_to_buffer(const char *text, size_t length,
		struct box *box, const char *whitespace_text,
		size_t whitespace_length, struct save_text_state *save);


/**
 * Extract the text from an HTML content and save it as a text file. Text is
 * converted to the local encoding.
 *
 * \param  c		An HTML content.
 * \param  path		Path to save text file too.
 */

void save_as_text(hlcache_handle *c, char *path)
{
	FILE *out;
	struct save_text_state save = { NULL, 0, 0 };
	save_text_whitespace before = WHITESPACE_NONE;
	bool first = true;
	nserror ret;
	char *result;

	if (!c || content_get_type(c) != CONTENT_HTML) {
		return;
	}

	extract_text(html_get_box_tree(c), &first, &before, &save);
	if (!save.block)
		return;

	ret = guit->utf8->utf8_to_local(save.block, save.length, &result);
	free(save.block);

	if (ret != NSERROR_OK) {
		LOG(("failed to convert to local encoding, return %d", ret));
		return;
	}

	out = fopen(path, "w");
	if (out) {
		int res = fputs(result, out);

		if (res < 0) {
			LOG(("Warning: write failed"));
		}

		res = fputs("\n", out);
		if (res < 0) {
			LOG(("Warning: failed writing trailing newline"));
		}

		fclose(out);
	}

	free(result);
}


/**
 * Decide what whitespace to place before the next bit of content-related text
 * that is saved. Any existing whitespace is overridden if the whitespace for
 * this box is more "significant".
 *
 * \param  box		Pointer to box.
 * \param  first	Whether this is before the first bit of content-related
 *			text to be saved.
 * \param  before	Type of whitespace currently intended to be placed
 *			before the next bit of content-related text to be saved.
 *			Updated if this box is worthy of more significant
 *			whitespace.
 * \param  whitespace_text    Whitespace to place before next bit of
 *			      content-related text to be saved.
 *			      Updated if this box is worthy of more significant
 *			      whitespace.
 * \param  whitespace_length  Length of whitespace_text.
 *			      Updated if this box is worthy of more significant
 *			      whitespace.
 */

void save_text_solve_whitespace(struct box *box, bool *first,
		save_text_whitespace *before, const char **whitespace_text,
		size_t *whitespace_length)
{
	/* work out what whitespace should be placed before the next bit of
	 * text */
	if (*before < WHITESPACE_TWO_NEW_LINES &&
			/* significant box type */
			(box->type == BOX_BLOCK ||
			 box->type == BOX_TABLE ||
			 box->type == BOX_FLOAT_LEFT ||
			 box->type == BOX_FLOAT_RIGHT) &&
			/* and not a list element */
			!box->list_marker &&
			/* and not a marker... */
			(!(box->parent && box->parent->list_marker == box) ||
			 /* ...unless marker follows WHITESPACE_TAB */
			 ((box->parent && box->parent->list_marker == box) &&
			  *before == WHITESPACE_TAB))) {
		*before = WHITESPACE_TWO_NEW_LINES;
	} else if (*before <= WHITESPACE_ONE_NEW_LINE &&
			(box->type == BOX_TABLE_ROW ||
			 box->type == BOX_BR ||
			 (box->type != BOX_INLINE &&
			 (box->parent && box->parent->list_marker == box)) ||
			 (box->parent && box->parent->style &&
			  (css_computed_white_space(box->parent->style) ==
			   CSS_WHITE_SPACE_PRE ||
			   css_computed_white_space(box->parent->style) ==
			   CSS_WHITE_SPACE_PRE_WRAP) &&
			  box->type == BOX_INLINE_CONTAINER))) {
		if (*before == WHITESPACE_ONE_NEW_LINE)
			*before = WHITESPACE_TWO_NEW_LINES;
		else
			*before = WHITESPACE_ONE_NEW_LINE;
	}
	else if (*before < WHITESPACE_TAB &&
			(box->type == BOX_TABLE_CELL ||
			 box->list_marker)) {
		*before = WHITESPACE_TAB;
	}

	if (*first) {
		/* before the first bit of text to be saved; there is
		 * no preceding whitespace */
		*whitespace_text = "";
		*whitespace_length = 0;
	} else {
		/* set the whitespace that has been decided on */
		switch (*before) {
			case WHITESPACE_TWO_NEW_LINES:
				*whitespace_text = "\n\n";
				*whitespace_length = 2;
				break;
			case WHITESPACE_ONE_NEW_LINE:
				*whitespace_text = "\n";
				*whitespace_length = 1;
				break;
			case WHITESPACE_TAB:
				*whitespace_text = "\t";
				*whitespace_length = 1;
				break;
			case WHITESPACE_NONE:
				*whitespace_text = "";
				*whitespace_length = 0;
				break;
			default:
				*whitespace_text = "";
				*whitespace_length = 0;
				break;
		}
	}
}


/**
 * Traverse though the box tree and add all text to a save buffer.
 *
 * \param  box		Pointer to box.
 * \param  first	Whether this is before the first bit of content-related
 *			text to be saved.
 * \param  before	Type of whitespace currently intended to be placed
 *			before the next bit of content-related text to be saved.
 *			Updated if this box is worthy of more significant
 *			whitespace.
 * \param  save		our save_text_state workspace pointer
 * \return true iff the file writing succeeded and traversal should continue.
 */

void extract_text(struct box *box, bool *first, save_text_whitespace *before,
		struct save_text_state *save)
{
	struct box *child;
	const char *whitespace_text = "";
	size_t whitespace_length = 0;

	assert(box);

	/* If box has a list marker */
	if (box->list_marker) {
		/* do the marker box before continuing with the rest of the
		 * list element */
		extract_text(box->list_marker, first, before, save);
	}

	/* read before calling the handler in case it modifies the tree */
	child = box->children;

	save_text_solve_whitespace(box, first, before, &whitespace_text,
			&whitespace_length);

	if (box->type != BOX_BR && !((box->type == BOX_FLOAT_LEFT ||
			box->type == BOX_FLOAT_RIGHT) && !box->text) &&
			box->length > 0 && box->text) {
		/* Box meets criteria for export; add text to buffer */
		save_text_add_to_buffer(box->text, box->length, box,
				whitespace_text, whitespace_length, save);
		*first = false;
		*before = WHITESPACE_NONE;
	}

	/* Work though the children of this box, extracting any text */
	while (child) {
		extract_text(child, first, before, save);
		child = child->next;
	}

	return;
}


/**
 * Add text to save text buffer. Any preceding whitespace or following space is
 * also added to the buffer.
 *
 * \param  text		Pointer to text being added.
 * \param  length	Length of text to be appended (bytes).
 * \param  box		Pointer to text box.
 * \param  whitespace_text    Whitespace to place before text for formatting
 *                            may be NULL.
 * \param  whitespace_length  Length of whitespace_text.
 * \param  save		Our save_text_state workspace pointer.
 * \return true iff the file writing succeeded and traversal should continue.
 */

bool save_text_add_to_buffer(const char *text, size_t length, struct box *box,
		const char *whitespace_text, size_t whitespace_length,
		struct save_text_state *save)
{
	size_t new_length;
	int space = 0;

	assert(save);

	if (box->space > 0)
		space = 1;

	if (whitespace_text)
		length += whitespace_length;

	new_length = save->length + whitespace_length + length + space;
	if (new_length >= save->alloc) {
		size_t new_alloc = save->alloc + (save->alloc / 4);
		char *new_block;

		if (new_alloc < new_length) new_alloc = new_length;

		new_block = realloc(save->block, new_alloc);
		if (!new_block) return false;

		save->block = new_block;
		save->alloc = new_alloc;
	}
	if (whitespace_text) {
		memcpy(save->block + save->length, whitespace_text,
				whitespace_length);
	}
	memcpy(save->block + save->length + whitespace_length, text, length);
	save->length += length;

	if (space == 1)
		save->block[save->length++] = ' ';

	return true;
}
