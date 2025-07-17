/* Output all lines of a diff_result. */
/*
 * Copyright (c) 2020 Neels Hofmeyr <neels@hofmeyr.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <arraylist.h>
#include <diff_main.h>
#include <diff_output.h>

#include "diff_internal.h"

static int
output_plain_chunk(struct diff_output_info *outinfo,
    FILE *dest, const struct diff_input_info *info,
    const struct diff_result *result,
    struct diff_chunk_context *cc, off_t *outoff, bool headers_only)
{
	off_t *offp;
	int left_start, left_len, right_start, right_len;
	int rc;
	bool change = false;

	left_len = cc->left.end - cc->left.start;
	if (left_len < 0)
		return EINVAL;
	else if (result->left->atoms.len == 0)
		left_start = 0;
	else if (left_len == 0 && cc->left.start > 0)
		left_start = cc->left.start;
	else if (cc->left.end > 0)
		left_start = cc->left.start + 1;
	else
		left_start = cc->left.start;

	right_len = cc->right.end - cc->right.start;
	if (right_len < 0)
		return EINVAL;
	else if (result->right->atoms.len == 0)
		right_start = 0;
	else if (right_len == 0 && cc->right.start > 0)
		right_start = cc->right.start;
	else if (cc->right.end > 0)
		right_start = cc->right.start + 1;
	else
		right_start = cc->right.start;

	if (left_len == 0) {
		/* addition */
		if (right_len == 1) {
			rc = fprintf(dest, "%da%d\n", left_start, right_start);
		} else {
			rc = fprintf(dest, "%da%d,%d\n", left_start,
			    right_start, cc->right.end);
		}
	} else if (right_len == 0) {
		/* deletion */
		if (left_len == 1) {
			rc = fprintf(dest, "%dd%d\n", left_start,
			    right_start);
		} else {
			rc = fprintf(dest, "%d,%dd%d\n", left_start,
			    cc->left.end, right_start);
		}
	} else {
		/* change */
		change = true;
		if (left_len == 1 && right_len == 1) {
			rc = fprintf(dest, "%dc%d\n", left_start, right_start);
		} else if (left_len == 1) {
			rc = fprintf(dest, "%dc%d,%d\n", left_start,
			    right_start, cc->right.end);
		} else if (right_len == 1) {
			rc = fprintf(dest, "%d,%dc%d\n", left_start,
			    cc->left.end, right_start);
		} else {
			rc = fprintf(dest, "%d,%dc%d,%d\n", left_start,
			    cc->left.end, right_start, cc->right.end);
		}
	}
	if (rc < 0)
		return errno;
	if (outinfo) {
		ARRAYLIST_ADD(offp, outinfo->line_offsets);
		if (offp == NULL)
			return ENOMEM;
		*outoff += rc;
		*offp = *outoff;
	}

	/*
	 * Now write out all the joined chunks.
	 *
	 * If the hunk denotes a change, it will come in the form of a deletion
	 * chunk followed by a addition chunk. Print a marker to break up the
	 * additions and deletions when this happens.
	 */
	int c_idx;
	for (c_idx = cc->chunk.start; !headers_only && c_idx < cc->chunk.end;
	    c_idx++) {
		const struct diff_chunk *c = &result->chunks.head[c_idx];
		if (c->left_count && !c->right_count)
			rc = diff_output_lines(outinfo, dest,
					  c->solved ? "< " : "?",
					  c->left_start, c->left_count);
		else if (c->right_count && !c->left_count) {
			if (change) {
				rc = fprintf(dest, "---\n");
				if (rc < 0)
					return errno;
				if (outinfo) {
					ARRAYLIST_ADD(offp,
					    outinfo->line_offsets);
					if (offp == NULL)
						return ENOMEM;
					*outoff += rc;
					*offp = *outoff;
				}
			}
			rc = diff_output_lines(outinfo, dest,
					  c->solved ? "> " : "?",
					  c->right_start, c->right_count);
		}
		if (rc)
			return rc;
		if (cc->chunk.end == result->chunks.len) {
			rc = diff_output_trailing_newline_msg(outinfo, dest, c);
			if (rc != DIFF_RC_OK)
				return rc;
		}
	}

	return DIFF_RC_OK;
}

int
diff_output_plain(struct diff_output_info **output_info,
    FILE *dest, const struct diff_input_info *info,
    const struct diff_result *result, int hunk_headers_only)
{
	struct diff_output_info *outinfo = NULL;
	struct diff_chunk_context cc = {};
	int atomizer_flags = (result->left->atomizer_flags|
	    result->right->atomizer_flags);
	int flags = (result->left->root->diff_flags |
	    result->right->root->diff_flags);
	bool force_text = (flags & DIFF_FLAG_FORCE_TEXT_DATA);
	bool have_binary = (atomizer_flags & DIFF_ATOMIZER_FOUND_BINARY_DATA);
	int i, rc;
	off_t outoff = 0, *offp;

	if (!result)
		return EINVAL;
	if (result->rc != DIFF_RC_OK)
		return result->rc;

	if (output_info) {
		*output_info = diff_output_info_alloc();
		if (*output_info == NULL)
			return ENOMEM;
		outinfo = *output_info;
	}

	if (have_binary && !force_text) {
		for (i = 0; i < result->chunks.len; i++) {
			struct diff_chunk *c = &result->chunks.head[i];
			enum diff_chunk_type t = diff_chunk_type(c);

			if (t != CHUNK_MINUS && t != CHUNK_PLUS)
				continue;

			rc = fprintf(dest, "Binary files %s and %s differ\n",
			    diff_output_get_label_left(info),
			    diff_output_get_label_right(info));
			if (rc < 0)
				return errno;
			if (outinfo) {
				ARRAYLIST_ADD(offp, outinfo->line_offsets);
				if (offp == NULL)
					return ENOMEM;
				outoff += rc;
				*offp = outoff;
			}
			break;
		}

		return DIFF_RC_OK;
	}

	for (i = 0; i < result->chunks.len; i++) {
		struct diff_chunk *chunk = &result->chunks.head[i];
		enum diff_chunk_type t = diff_chunk_type(chunk);
		struct diff_chunk_context next;

		if (t != CHUNK_MINUS && t != CHUNK_PLUS)
			continue;

		if (diff_chunk_context_empty(&cc)) {
			/* Note down the start point, any number of subsequent
			 * chunks may be joined up to this chunk by being
			 * directly adjacent. */
			diff_chunk_context_get(&cc, result, i, 0);
			continue;
		}

		/* There already is a previous chunk noted down for being
		 * printed. Does it join up with this one? */
		diff_chunk_context_get(&next, result, i, 0);

		if (diff_chunk_contexts_touch(&cc, &next)) {
			/* This next context touches or overlaps the previous
			 * one, join. */
			diff_chunk_contexts_merge(&cc, &next);
			/* When we merge the last chunk we can end up with one
			 * hanging chunk and have to come back for it after the
			 * loop */
			continue;
		}
		rc = output_plain_chunk(outinfo, dest, info, result, &cc,
		    &outoff, hunk_headers_only);
		if (rc != DIFF_RC_OK)
			return rc;
		cc = next;
	}
	if (!diff_chunk_context_empty(&cc))
		return output_plain_chunk(outinfo, dest, info, result, &cc,
		    &outoff, hunk_headers_only);
	return DIFF_RC_OK;
}
