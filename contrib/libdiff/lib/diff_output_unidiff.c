/* Produce a unidiff output from a diff_result. */
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <arraylist.h>
#include <diff_main.h>
#include <diff_output.h>

#include "diff_internal.h"
#include "diff_debug.h"

off_t
diff_chunk_get_left_start_pos(const struct diff_chunk *c)
{
	return c->left_start->pos;
}

off_t
diff_chunk_get_right_start_pos(const struct diff_chunk *c)
{
	return c->right_start->pos;
}

bool
diff_chunk_context_empty(const struct diff_chunk_context *cc)
{
	return diff_range_empty(&cc->chunk);
}

int
diff_chunk_get_left_start(const struct diff_chunk *c,
    const struct diff_result *r, int context_lines)
{
	int left_start = diff_atom_root_idx(r->left, c->left_start);
	return MAX(0, left_start - context_lines);
}

int
diff_chunk_get_left_end(const struct diff_chunk *c,
    const struct diff_result *r, int context_lines)
{
	int left_start = diff_chunk_get_left_start(c, r, 0);
	return MIN(r->left->atoms.len,
	    left_start + c->left_count + context_lines);
}

int
diff_chunk_get_right_start(const struct diff_chunk *c,
    const struct diff_result *r, int context_lines)
{
	int right_start = diff_atom_root_idx(r->right, c->right_start);
	return MAX(0, right_start - context_lines);
}

int
diff_chunk_get_right_end(const struct diff_chunk *c,
    const struct diff_result *r, int context_lines)
{
	int right_start = diff_chunk_get_right_start(c, r, 0);
	return MIN(r->right->atoms.len,
	    right_start + c->right_count + context_lines);
}

struct diff_chunk *
diff_chunk_get(const struct diff_result *r, int chunk_idx)
{
	return &r->chunks.head[chunk_idx];
}

int
diff_chunk_get_left_count(struct diff_chunk *c)
{
	return c->left_count;
}

int
diff_chunk_get_right_count(struct diff_chunk *c)
{
	return c->right_count;
}

void
diff_chunk_context_get(struct diff_chunk_context *cc, const struct diff_result *r,
		  int chunk_idx, int context_lines)
{
	const struct diff_chunk *c = &r->chunks.head[chunk_idx];
	int left_start = diff_chunk_get_left_start(c, r, context_lines);
	int left_end = diff_chunk_get_left_end(c, r, context_lines);
	int right_start = diff_chunk_get_right_start(c, r, context_lines);
	int right_end = diff_chunk_get_right_end(c, r,  context_lines);

	*cc = (struct diff_chunk_context){
		.chunk = {
			.start = chunk_idx,
			.end = chunk_idx + 1,
		},
		.left = {
			.start = left_start,
			.end = left_end,
		},
		.right = {
			.start = right_start,
			.end = right_end,
		},
	};
}

bool
diff_chunk_contexts_touch(const struct diff_chunk_context *cc,
			  const struct diff_chunk_context *other)
{
	return diff_ranges_touch(&cc->chunk, &other->chunk)
		|| diff_ranges_touch(&cc->left, &other->left)
		|| diff_ranges_touch(&cc->right, &other->right);
}

void
diff_chunk_contexts_merge(struct diff_chunk_context *cc,
			  const struct diff_chunk_context *other)
{
	diff_ranges_merge(&cc->chunk, &other->chunk);
	diff_ranges_merge(&cc->left, &other->left);
	diff_ranges_merge(&cc->right, &other->right);
}

void
diff_chunk_context_load_change(struct diff_chunk_context *cc,
			       int *nchunks_used,
			       struct diff_result *result,
			       int start_chunk_idx,
			       int context_lines)
{
	int i;
	int seen_minus = 0, seen_plus = 0;

	if (nchunks_used)
		*nchunks_used = 0;

	for (i = start_chunk_idx; i < result->chunks.len; i++) {
		struct diff_chunk *chunk = &result->chunks.head[i];
		enum diff_chunk_type t = diff_chunk_type(chunk);
		struct diff_chunk_context next;

		if (t != CHUNK_MINUS && t != CHUNK_PLUS) {
			if (nchunks_used)
				(*nchunks_used)++;
			if (seen_minus || seen_plus)
				break;
			else
				continue;
		} else if (t == CHUNK_MINUS)
			seen_minus = 1;
		else if (t == CHUNK_PLUS)
			seen_plus = 1;

		if (diff_chunk_context_empty(cc)) {
			/* Note down the start point, any number of subsequent
			 * chunks may be joined up to this chunk by being
			 * directly adjacent. */
			diff_chunk_context_get(cc, result, i, context_lines);
			if (nchunks_used)
				(*nchunks_used)++;
			continue;
		}

		/* There already is a previous chunk noted down for being
		 * printed. Does it join up with this one? */
		diff_chunk_context_get(&next, result, i, context_lines);

		if (diff_chunk_contexts_touch(cc, &next)) {
			/* This next context touches or overlaps the previous
			 * one, join. */
			diff_chunk_contexts_merge(cc, &next);
			if (nchunks_used)
				(*nchunks_used)++;
			continue;
		} else
			break;
	}
}

struct diff_output_unidiff_state {
	bool header_printed;
	char prototype[DIFF_FUNCTION_CONTEXT_SIZE];
	int last_prototype_idx;
};

struct diff_output_unidiff_state *
diff_output_unidiff_state_alloc(void)
{
	struct diff_output_unidiff_state *state;

	state = calloc(1, sizeof(struct diff_output_unidiff_state));
	if (state != NULL)
		diff_output_unidiff_state_reset(state);
	return state;
}

void
diff_output_unidiff_state_reset(struct diff_output_unidiff_state *state)
{
	state->header_printed = false;
	memset(state->prototype, 0, sizeof(state->prototype));
	state->last_prototype_idx = 0;
}

void
diff_output_unidiff_state_free(struct diff_output_unidiff_state *state)
{
	free(state);
}

static int
output_unidiff_chunk(struct diff_output_info *outinfo, FILE *dest,
		     struct diff_output_unidiff_state *state,
		     const struct diff_input_info *info,
		     const struct diff_result *result,
		     bool print_header, bool show_function_prototypes,
		     const struct diff_chunk_context *cc)
{
	int rc, left_start, left_len, right_start, right_len;
	off_t outoff = 0, *offp;
	uint8_t *typep;

	if (diff_range_empty(&cc->left) && diff_range_empty(&cc->right))
		return DIFF_RC_OK;

	if (outinfo && outinfo->line_offsets.len > 0) {
		unsigned int idx = outinfo->line_offsets.len - 1;
		outoff = outinfo->line_offsets.head[idx];
	}

	if (print_header && !(state->header_printed)) {
		rc = fprintf(dest, "--- %s\n",
		    diff_output_get_label_left(info));
		if (rc < 0)
			return errno;
		if (outinfo) {
			ARRAYLIST_ADD(offp, outinfo->line_offsets);
			if (offp == NULL)
				return ENOMEM;
			outoff += rc;
			*offp = outoff;
			ARRAYLIST_ADD(typep, outinfo->line_types);
			if (typep == NULL)
				return ENOMEM;
			*typep = DIFF_LINE_MINUS;
		}
		rc = fprintf(dest, "+++ %s\n",
		    diff_output_get_label_right(info));
		if (rc < 0)
			return errno;
		if (outinfo) {
			ARRAYLIST_ADD(offp, outinfo->line_offsets);
			if (offp == NULL)
				return ENOMEM;
			outoff += rc;
			*offp = outoff;
			ARRAYLIST_ADD(typep, outinfo->line_types);
			if (typep == NULL)
				return ENOMEM;
			*typep = DIFF_LINE_PLUS;
		}
		state->header_printed = true;
	}

	left_len = cc->left.end - cc->left.start;
	if (result->left->atoms.len == 0)
		left_start = 0;
	else if (left_len == 0 && cc->left.start > 0)
		left_start = cc->left.start;
	else
		left_start = cc->left.start + 1;

	right_len = cc->right.end - cc->right.start;
	if (result->right->atoms.len == 0)
		right_start = 0;
	else if (right_len == 0 && cc->right.start > 0)
		right_start = cc->right.start;
	else
		right_start = cc->right.start + 1;

	/* Got the absolute line numbers where to start printing, and the index
	 * of the interesting (non-context) chunk.
	 * To print context lines above the interesting chunk, nipping on the
	 * previous chunk index may be necessary.
	 * It is guaranteed to be only context lines where left == right, so it
	 * suffices to look on the left. */
	const struct diff_chunk *first_chunk;
	int chunk_start_line;
	first_chunk = &result->chunks.head[cc->chunk.start];
	chunk_start_line = diff_atom_root_idx(result->left,
					      first_chunk->left_start);
	if (show_function_prototypes) {
		rc = diff_output_match_function_prototype(state->prototype,
		    sizeof(state->prototype), &state->last_prototype_idx,
		    result, chunk_start_line);
		if (rc)
			return rc;
	}

	if (left_len == 1 && right_len == 1) {
		rc = fprintf(dest, "@@ -%d +%d @@%s%s\n",
			left_start, right_start,
			state->prototype[0] ? " " : "",
			state->prototype[0] ? state->prototype : "");
	} else if (left_len == 1 && right_len != 1) {
		rc = fprintf(dest, "@@ -%d +%d,%d @@%s%s\n",
			left_start, right_start, right_len,
			state->prototype[0] ? " " : "",
			state->prototype[0] ? state->prototype : "");
	} else if (left_len != 1 && right_len == 1) {
		rc = fprintf(dest, "@@ -%d,%d +%d @@%s%s\n",
			left_start, left_len, right_start,
			state->prototype[0] ? " " : "",
			state->prototype[0] ? state->prototype : "");
	} else {
		rc = fprintf(dest, "@@ -%d,%d +%d,%d @@%s%s\n",
			left_start, left_len, right_start, right_len,
			state->prototype[0] ? " " : "",
			state->prototype[0] ? state->prototype : "");
	}
	if (rc < 0)
		return errno;
	if (outinfo) {
		ARRAYLIST_ADD(offp, outinfo->line_offsets);
		if (offp == NULL)
			return ENOMEM;
		outoff += rc;
		*offp = outoff;
		ARRAYLIST_ADD(typep, outinfo->line_types);
		if (typep == NULL)
			return ENOMEM;
		*typep = DIFF_LINE_HUNK;
	}

	if (cc->left.start < chunk_start_line) {
		rc = diff_output_lines(outinfo, dest, " ",
				  &result->left->atoms.head[cc->left.start],
				  chunk_start_line - cc->left.start);
		if (rc)
			return rc;
	}

	/* Now write out all the joined chunks and contexts between them */
	int c_idx;
	for (c_idx = cc->chunk.start; c_idx < cc->chunk.end; c_idx++) {
		const struct diff_chunk *c = &result->chunks.head[c_idx];

		if (c->left_count && c->right_count)
			rc = diff_output_lines(outinfo, dest,
					  c->solved ? " " : "?",
					  c->left_start, c->left_count);
		else if (c->left_count && !c->right_count)
			rc = diff_output_lines(outinfo, dest,
					  c->solved ? "-" : "?",
					  c->left_start, c->left_count);
		else if (c->right_count && !c->left_count)
			rc = diff_output_lines(outinfo, dest,
					  c->solved ? "+" : "?",
					  c->right_start, c->right_count);
		if (rc)
			return rc;

		if (cc->chunk.end == result->chunks.len) {
			rc = diff_output_trailing_newline_msg(outinfo, dest, c);
			if (rc != DIFF_RC_OK)
				return rc;
		}
	}

	/* Trailing context? */
	const struct diff_chunk *last_chunk;
	int chunk_end_line;
	last_chunk = &result->chunks.head[cc->chunk.end - 1];
	chunk_end_line = diff_atom_root_idx(result->left,
					    last_chunk->left_start
					    + last_chunk->left_count);
	if (cc->left.end > chunk_end_line) {
		rc = diff_output_lines(outinfo, dest, " ",
				  &result->left->atoms.head[chunk_end_line],
				  cc->left.end - chunk_end_line);
		if (rc)
			return rc;

		if (cc->left.end == result->left->atoms.len) {
			rc = diff_output_trailing_newline_msg(outinfo, dest,
			    &result->chunks.head[result->chunks.len - 1]);
			if (rc != DIFF_RC_OK)
				return rc;
		}
	}

	return DIFF_RC_OK;
}

int
diff_output_unidiff_chunk(struct diff_output_info **output_info, FILE *dest,
			  struct diff_output_unidiff_state *state,
			  const struct diff_input_info *info,
			  const struct diff_result *result,
			  const struct diff_chunk_context *cc)
{
	struct diff_output_info *outinfo = NULL;
	int flags = (result->left->root->diff_flags |
	    result->right->root->diff_flags);
	bool show_function_prototypes = (flags & DIFF_FLAG_SHOW_PROTOTYPES);

	if (output_info) {
		*output_info = diff_output_info_alloc();
		if (*output_info == NULL)
			return ENOMEM;
		outinfo = *output_info;
	}

	return output_unidiff_chunk(outinfo, dest, state, info,
	    result, false, show_function_prototypes, cc);
}

int
diff_output_unidiff(struct diff_output_info **output_info,
		    FILE *dest, const struct diff_input_info *info,
		    const struct diff_result *result,
		    unsigned int context_lines)
{
	struct diff_output_unidiff_state *state;
	struct diff_chunk_context cc = {};
	struct diff_output_info *outinfo = NULL;
	int atomizer_flags = (result->left->atomizer_flags|
	    result->right->atomizer_flags);
	int flags = (result->left->root->diff_flags |
	    result->right->root->diff_flags);
	bool show_function_prototypes = (flags & DIFF_FLAG_SHOW_PROTOTYPES);
	bool force_text = (flags & DIFF_FLAG_FORCE_TEXT_DATA);
	bool have_binary = (atomizer_flags & DIFF_ATOMIZER_FOUND_BINARY_DATA);
	off_t outoff = 0, *offp;
	uint8_t *typep;
	int rc, i;

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

			if (outinfo && outinfo->line_offsets.len > 0) {
				unsigned int idx =
				    outinfo->line_offsets.len - 1;
				outoff = outinfo->line_offsets.head[idx];
			}

			rc = fprintf(dest, "Binary files %s and %s differ\n",
			    diff_output_get_label_left(info),
			    diff_output_get_label_right(info));
			if (outinfo) {
				ARRAYLIST_ADD(offp, outinfo->line_offsets);
				if (offp == NULL)
					return ENOMEM;
				outoff += rc;
				*offp = outoff;
				ARRAYLIST_ADD(typep, outinfo->line_types);
				if (typep == NULL)
					return ENOMEM;
				*typep = DIFF_LINE_NONE;
			}
			break;
		}

		return DIFF_RC_OK;
	}

	state = diff_output_unidiff_state_alloc();
	if (state == NULL) {
		if (output_info) {
			diff_output_info_free(*output_info);
			*output_info = NULL;
		}
		return ENOMEM;
	}

#if DEBUG
	unsigned int check_left_pos, check_right_pos;
	check_left_pos = 0;
	check_right_pos = 0;
	for (i = 0; i < result->chunks.len; i++) {
		struct diff_chunk *c = &result->chunks.head[i];
		enum diff_chunk_type t = diff_chunk_type(c);

		debug("[%d] %s lines L%d R%d @L %d @R %d\n",
		      i, (t == CHUNK_MINUS ? "minus" :
			  (t == CHUNK_PLUS ? "plus" :
			   (t == CHUNK_SAME ? "same" : "?"))),
		      c->left_count,
		      c->right_count,
		      c->left_start ? diff_atom_root_idx(result->left, c->left_start) : -1,
		      c->right_start ? diff_atom_root_idx(result->right, c->right_start) : -1);
		assert(check_left_pos == diff_atom_root_idx(result->left, c->left_start));
		assert(check_right_pos == diff_atom_root_idx(result->right, c->right_start));
		check_left_pos += c->left_count;
		check_right_pos += c->right_count;

	}
	assert(check_left_pos == result->left->atoms.len);
	assert(check_right_pos == result->right->atoms.len);
#endif

	for (i = 0; i < result->chunks.len; i++) {
		struct diff_chunk *c = &result->chunks.head[i];
		enum diff_chunk_type t = diff_chunk_type(c);
		struct diff_chunk_context next;

		if (t != CHUNK_MINUS && t != CHUNK_PLUS)
			continue;

		if (diff_chunk_context_empty(&cc)) {
			/* These are the first lines being printed.
			 * Note down the start point, any number of subsequent
			 * chunks may be joined up to this unidiff chunk by
			 * context lines or by being directly adjacent. */
			diff_chunk_context_get(&cc, result, i, context_lines);
			debug("new chunk to be printed:"
			      " chunk %d-%d left %d-%d right %d-%d\n",
			      cc.chunk.start, cc.chunk.end,
			      cc.left.start, cc.left.end,
			      cc.right.start, cc.right.end);
			continue;
		}

		/* There already is a previous chunk noted down for being
		 * printed. Does it join up with this one? */
		diff_chunk_context_get(&next, result, i, context_lines);
		debug("new chunk to be printed:"
		      " chunk %d-%d left %d-%d right %d-%d\n",
		      next.chunk.start, next.chunk.end,
		      next.left.start, next.left.end,
		      next.right.start, next.right.end);

		if (diff_chunk_contexts_touch(&cc, &next)) {
			/* This next context touches or overlaps the previous
			 * one, join. */
			diff_chunk_contexts_merge(&cc, &next);
			debug("new chunk to be printed touches previous chunk,"
			      " now: left %d-%d right %d-%d\n",
			      cc.left.start, cc.left.end,
			      cc.right.start, cc.right.end);
			continue;
		}

		/* No touching, so the previous context is complete with a gap
		 * between it and this next one. Print the previous one and
		 * start fresh here. */
		debug("new chunk to be printed does not touch previous chunk;"
		      " print left %d-%d right %d-%d\n",
		      cc.left.start, cc.left.end, cc.right.start, cc.right.end);
		output_unidiff_chunk(outinfo, dest, state, info, result,
		    true, show_function_prototypes, &cc);
		cc = next;
		debug("new unprinted chunk is left %d-%d right %d-%d\n",
		      cc.left.start, cc.left.end, cc.right.start, cc.right.end);
	}

	if (!diff_chunk_context_empty(&cc))
		output_unidiff_chunk(outinfo, dest, state, info, result,
		    true, show_function_prototypes, &cc);
	diff_output_unidiff_state_free(state);
	return DIFF_RC_OK;
}
