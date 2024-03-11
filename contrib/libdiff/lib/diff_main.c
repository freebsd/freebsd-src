/* Generic infrastructure to implement various diff algorithms (implementation). */
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


#include <sys/queue.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <assert.h>

#include <arraylist.h>
#include <diff_main.h>

#include "diff_internal.h"
#include "diff_debug.h"

inline enum diff_chunk_type
diff_chunk_type(const struct diff_chunk *chunk)
{
	if (!chunk->left_count && !chunk->right_count)
		return CHUNK_EMPTY;
	if (!chunk->solved)
		return CHUNK_ERROR;
	if (!chunk->right_count)
		return CHUNK_MINUS;
	if (!chunk->left_count)
		return CHUNK_PLUS;
	if (chunk->left_count != chunk->right_count)
		return CHUNK_ERROR;
	return CHUNK_SAME;
}

static int
read_at(FILE *f, off_t at_pos, unsigned char *buf, size_t len)
{
	int r;
	if (fseeko(f, at_pos, SEEK_SET) == -1)
		return errno;
	r = fread(buf, sizeof(char), len, f);
	if ((r == 0 || r < len) && ferror(f))
		return EIO;
	if (r != len)
		return EIO;
	return 0;
}

static int
buf_cmp(const unsigned char *left, size_t left_len,
	const unsigned char *right, size_t right_len,
	bool ignore_whitespace)
{
	int cmp;

	if (ignore_whitespace) {
		int il = 0, ir = 0;
		while (il < left_len && ir < right_len) {
			unsigned char cl = left[il];
			unsigned char cr = right[ir];

			if (isspace((unsigned char)cl) && il < left_len) {
				il++;
				continue;
			}
			if (isspace((unsigned char)cr) && ir < right_len) {
				ir++;
				continue;
			}

			if (cl > cr)
				return 1;
			if (cr > cl)
				return -1;
			il++;
			ir++;
		}
		while (il < left_len) {
			unsigned char cl = left[il++];
			if (!isspace((unsigned char)cl))
				return 1;
		}
		while (ir < right_len) {
			unsigned char cr = right[ir++];
			if (!isspace((unsigned char)cr))
				return -1;
		}

		return 0;
	}

	cmp = memcmp(left, right, MIN(left_len, right_len));
	if (cmp)
		return cmp;
	if (left_len == right_len)
		return 0;
	return (left_len > right_len) ? 1 : -1;
}

int
diff_atom_cmp(int *cmp,
	      const struct diff_atom *left,
	      const struct diff_atom *right)
{
	off_t remain_left, remain_right;
	int flags = (left->root->diff_flags | right->root->diff_flags);
	bool ignore_whitespace = (flags & DIFF_FLAG_IGNORE_WHITESPACE);

	if (!left->len && !right->len) {
		*cmp = 0;
		return 0;
	}
	if (!ignore_whitespace) {
		if (!right->len) {
			*cmp = 1;
			return 0;
		}
		if (!left->len) {
			*cmp = -1;
			return 0;
		}
	}

	if (left->at != NULL && right->at != NULL) {
		*cmp = buf_cmp(left->at, left->len, right->at, right->len,
		    ignore_whitespace);
		return 0;
	}

	remain_left = left->len;
	remain_right = right->len;
	while (remain_left > 0 || remain_right > 0) {
		const size_t chunksz = 8192;
		unsigned char buf_left[chunksz], buf_right[chunksz];
		const uint8_t *p_left, *p_right;
		off_t n_left, n_right;
		ssize_t r;

		if (!remain_right) {
			*cmp = 1;
			return 0;
		}
		if (!remain_left) {
			*cmp = -1;
			return 0;
		}

		n_left = MIN(chunksz, remain_left);
		n_right = MIN(chunksz, remain_right);

		if (left->at == NULL) {
			r = read_at(left->root->f,
				    left->pos + (left->len - remain_left),
				    buf_left, n_left);
			if (r) {
				*cmp = 0;
				return r;
			}
			p_left = buf_left;
		} else {
			p_left = left->at + (left->len - remain_left);
		}

		if (right->at == NULL) {
			r = read_at(right->root->f,
				    right->pos + (right->len - remain_right),
				    buf_right, n_right);
			if (r) {
				*cmp = 0;
				return r;
			}
			p_right = buf_right;
		} else {
			p_right = right->at + (right->len - remain_right);
		}

		r = buf_cmp(p_left, n_left, p_right, n_right,
		    ignore_whitespace);
		if (r) {
			*cmp = r;
			return 0;
		}

		remain_left -= n_left;
		remain_right -= n_right;
	}

	*cmp = 0;
	return 0;
}

int
diff_atom_same(bool *same,
	       const struct diff_atom *left,
	       const struct diff_atom *right)
{
	int cmp;
	int r;
	if (left->hash != right->hash) {
		*same = false;
		return 0;
	}
	r = diff_atom_cmp(&cmp, left, right);
	if (r) {
		*same = true;
		return r;
	}
	*same = (cmp == 0);
	return 0;
}

static struct diff_chunk *
diff_state_add_solved_chunk(struct diff_state *state,
			    const struct diff_chunk *chunk)
{
	diff_chunk_arraylist_t *result;
	struct diff_chunk *new_chunk;
	enum diff_chunk_type last_t;
	enum diff_chunk_type new_t;
	struct diff_chunk *last;

	/* Append to solved chunks; make sure that adjacent chunks of same type are combined, and that a minus chunk
	 * never directly follows a plus chunk. */
	result = &state->result->chunks;

	last_t = result->len ? diff_chunk_type(&result->head[result->len - 1])
		: CHUNK_EMPTY;
	new_t = diff_chunk_type(chunk);

	debug("ADD %s chunk #%u:\n", chunk->solved ? "solved" : "UNSOLVED",
	      result->len);
	debug("L\n");
	debug_dump_atoms(&state->left, chunk->left_start, chunk->left_count);
	debug("R\n");
	debug_dump_atoms(&state->right, chunk->right_start, chunk->right_count);

	if (result->len) {
		last = &result->head[result->len - 1];
		assert(chunk->left_start
		       == last->left_start + last->left_count);
		assert(chunk->right_start
		       == last->right_start + last->right_count);
	}

	if (new_t == last_t) {
		new_chunk = &result->head[result->len - 1];
		new_chunk->left_count += chunk->left_count;
		new_chunk->right_count += chunk->right_count;
		debug("  - added chunk touches previous one of same type, joined:\n");
		debug("L\n");
		debug_dump_atoms(&state->left, new_chunk->left_start, new_chunk->left_count);
		debug("R\n");
		debug_dump_atoms(&state->right, new_chunk->right_start, new_chunk->right_count);
	} else if (last_t == CHUNK_PLUS && new_t == CHUNK_MINUS) {
		enum diff_chunk_type prev_last_t =
			result->len > 1 ?
				diff_chunk_type(&result->head[result->len - 2])
				: CHUNK_EMPTY;
		/* If a minus-chunk follows a plus-chunk, place it above the plus-chunk->
		 * Is the one before that also a minus? combine. */
		if (prev_last_t == CHUNK_MINUS) {
			new_chunk = &result->head[result->len - 2];
			new_chunk->left_count += chunk->left_count;
			new_chunk->right_count += chunk->right_count;

			debug("  - added minus-chunk follows plus-chunk,"
			      " put before that plus-chunk and joined"
			      " with preceding minus-chunk:\n");
			debug("L\n");
			debug_dump_atoms(&state->left, new_chunk->left_start, new_chunk->left_count);
			debug("R\n");
			debug_dump_atoms(&state->right, new_chunk->right_start, new_chunk->right_count);
		} else {
			ARRAYLIST_INSERT(new_chunk, *result, result->len - 1);
			if (!new_chunk)
				return NULL;
			*new_chunk = *chunk;

			/* The new minus chunk indicates to which position on
			 * the right it corresponds, even though it doesn't add
			 * any lines on the right. By moving above a plus chunk,
			 * that position on the right has shifted. */
			last = &result->head[result->len - 1];
			new_chunk->right_start = last->right_start;

			debug("  - added minus-chunk follows plus-chunk,"
			      " put before that plus-chunk\n");
		}

		/* That last_t == CHUNK_PLUS indicates to which position on the
		 * left it corresponds, even though it doesn't add any lines on
		 * the left. By inserting/extending the prev_last_t ==
		 * CHUNK_MINUS, that position on the left has shifted. */
		last = &result->head[result->len - 1];
		last->left_start = new_chunk->left_start
			+ new_chunk->left_count;

	} else {
		ARRAYLIST_ADD(new_chunk, *result);
		if (!new_chunk)
			return NULL;
		*new_chunk = *chunk;
	}
	return new_chunk;
}

/* Even if a left or right side is empty, diff output may need to know the
 * position in that file.
 * So left_start or right_start must never be NULL -- pass left_count or
 * right_count as zero to indicate staying at that position without consuming
 * any lines. */
struct diff_chunk *
diff_state_add_chunk(struct diff_state *state, bool solved,
		     struct diff_atom *left_start, unsigned int left_count,
		     struct diff_atom *right_start, unsigned int right_count)
{
	struct diff_chunk *new_chunk;
	struct diff_chunk chunk = {
		.solved = solved,
		.left_start = left_start,
		.left_count = left_count,
		.right_start = right_start,
		.right_count = right_count,
	};

	/* An unsolved chunk means store as intermediate result for later
	 * re-iteration.
	 * If there already are intermediate results, that means even a
	 * following solved chunk needs to go to intermediate results, so that
	 * it is later put in the final correct position in solved chunks.
	 */
	if (!solved || state->temp_result.len) {
		/* Append to temp_result */
		debug("ADD %s chunk to temp result:\n",
		      chunk.solved ? "solved" : "UNSOLVED");
		debug("L\n");
		debug_dump_atoms(&state->left, left_start, left_count);
		debug("R\n");
		debug_dump_atoms(&state->right, right_start, right_count);
		ARRAYLIST_ADD(new_chunk, state->temp_result);
		if (!new_chunk)
			return NULL;
		*new_chunk = chunk;
		return new_chunk;
	}

	return diff_state_add_solved_chunk(state, &chunk);
}

static void
diff_data_init_root(struct diff_data *d, FILE *f, const uint8_t *data,
    unsigned long long len, int diff_flags)
{
	*d = (struct diff_data){
		.f = f,
		.pos = 0,
		.data = data,
		.len = len,
		.root = d,
		.diff_flags = diff_flags,
	};
}

void
diff_data_init_subsection(struct diff_data *d, struct diff_data *parent,
			  struct diff_atom *from_atom, unsigned int atoms_count)
{
	struct diff_atom *last_atom;

	debug("diff_data %p  parent %p  from_atom %p  atoms_count %u\n",
	      d, parent, from_atom, atoms_count);
	debug("  from_atom ");
	debug_dump_atom(parent, NULL, from_atom);

	if (atoms_count == 0) {
		*d = (struct diff_data){
			.f = NULL,
			.pos = 0,
			.data = NULL,
			.len = 0,
			.root = parent->root,
			.atoms.head = NULL,
			.atoms.len = atoms_count,
		};

		return;
	}

	last_atom = from_atom + atoms_count - 1;
	*d = (struct diff_data){
		.f = NULL,
		.pos = from_atom->pos,
		.data = from_atom->at,
		.len = (last_atom->pos + last_atom->len) - from_atom->pos,
		.root = parent->root,
		.atoms.head = from_atom,
		.atoms.len = atoms_count,
	};

	debug("subsection:\n");
	debug_dump(d);
}

void
diff_data_free(struct diff_data *diff_data)
{
	if (!diff_data)
		return;
	if (diff_data->atoms.allocated)
		ARRAYLIST_FREE(diff_data->atoms);
}

int
diff_algo_none(const struct diff_algo_config *algo_config,
	       struct diff_state *state)
{
	debug("\n** %s\n", __func__);
	debug("left:\n");
	debug_dump(&state->left);
	debug("right:\n");
	debug_dump(&state->right);
	debug_dump_myers_graph(&state->left, &state->right, NULL, NULL, 0, NULL,
			       0);

	/* Add a chunk of equal lines, if any */
	struct diff_atom *l = state->left.atoms.head;
	unsigned int l_len = state->left.atoms.len;
	struct diff_atom *r = state->right.atoms.head;
	unsigned int r_len = state->right.atoms.len;
	unsigned int equal_atoms_start = 0;
	unsigned int equal_atoms_end = 0;
	unsigned int l_idx = 0;
	unsigned int r_idx = 0;

	while (equal_atoms_start < l_len
	       && equal_atoms_start < r_len) {
		int err;
		bool same;
		err = diff_atom_same(&same, &l[equal_atoms_start],
				     &r[equal_atoms_start]);
		if (err)
			return err;
		if (!same)
			break;
		equal_atoms_start++;
	}
	while (equal_atoms_end < (l_len - equal_atoms_start)
	       && equal_atoms_end < (r_len - equal_atoms_start)) {
		int err;
		bool same;
		err = diff_atom_same(&same, &l[l_len - 1 - equal_atoms_end],
				   &r[r_len - 1 - equal_atoms_end]);
		if (err)
			return err;
		if (!same)
			break;
		equal_atoms_end++;
	}

	/* Add a chunk of equal lines at the start */
	if (equal_atoms_start) {
		if (!diff_state_add_chunk(state, true,
					  l, equal_atoms_start,
					  r, equal_atoms_start))
			return ENOMEM;
		l_idx += equal_atoms_start;
		r_idx += equal_atoms_start;
	}

	/* Add a "minus" chunk with all lines from the left. */
	if (equal_atoms_start + equal_atoms_end < l_len) {
		unsigned int add_len = l_len - equal_atoms_start - equal_atoms_end;
		if (!diff_state_add_chunk(state, true,
					  &l[l_idx], add_len,
					  &r[r_idx], 0))
			return ENOMEM;
		l_idx += add_len;
	}

	/* Add a "plus" chunk with all lines from the right. */
	if (equal_atoms_start + equal_atoms_end < r_len) {
		unsigned int add_len = r_len - equal_atoms_start - equal_atoms_end;
		if (!diff_state_add_chunk(state, true,
					  &l[l_idx], 0,
					  &r[r_idx], add_len))
			return ENOMEM;
		r_idx += add_len;
	}

	/* Add a chunk of equal lines at the end */
	if (equal_atoms_end) {
		if (!diff_state_add_chunk(state, true,
					  &l[l_idx], equal_atoms_end,
					  &r[r_idx], equal_atoms_end))
			return ENOMEM;
	}

	return DIFF_RC_OK;
}

static int
diff_run_algo(const struct diff_algo_config *algo_config,
	      struct diff_state *state)
{
	int rc;

	if (!algo_config || !algo_config->impl
	    || !state->recursion_depth_left
	    || !state->left.atoms.len || !state->right.atoms.len) {
		debug("Fall back to diff_algo_none():%s%s%s\n",
		      (!algo_config || !algo_config->impl) ? " no-cfg" : "",
		      (!state->recursion_depth_left) ? " max-depth" : "",
		      (!state->left.atoms.len || !state->right.atoms.len)?
			      " trivial" : "");
		return diff_algo_none(algo_config, state);
	}

	ARRAYLIST_FREE(state->temp_result);
	ARRAYLIST_INIT(state->temp_result, DIFF_RESULT_ALLOC_BLOCKSIZE);
	rc = algo_config->impl(algo_config, state);
	switch (rc) {
	case DIFF_RC_USE_DIFF_ALGO_FALLBACK:
		debug("Got DIFF_RC_USE_DIFF_ALGO_FALLBACK (%p)\n",
		      algo_config->fallback_algo);
		rc = diff_run_algo(algo_config->fallback_algo, state);
		goto return_rc;

	case DIFF_RC_OK:
		/* continue below */
		break;

	default:
		/* some error happened */
		goto return_rc;
	}

	/* Pick up any diff chunks that are still unsolved and feed to
	 * inner_algo.  inner_algo will solve unsolved chunks and append to
	 * result, and subsequent solved chunks on this level are then appended
	 * to result afterwards. */
	int i;
	for (i = 0; i < state->temp_result.len; i++) {
		struct diff_chunk *c = &state->temp_result.head[i];
		if (c->solved) {
			diff_state_add_solved_chunk(state, c);
			continue;
		}

		/* c is an unsolved chunk, feed to inner_algo */
		struct diff_state inner_state = {
			.result = state->result,
			.recursion_depth_left = state->recursion_depth_left - 1,
			.kd_buf = state->kd_buf,
			.kd_buf_size = state->kd_buf_size,
		};
		diff_data_init_subsection(&inner_state.left, &state->left,
					  c->left_start, c->left_count);
		diff_data_init_subsection(&inner_state.right, &state->right,
					  c->right_start, c->right_count);

		rc = diff_run_algo(algo_config->inner_algo, &inner_state);
		state->kd_buf = inner_state.kd_buf;
		state->kd_buf_size = inner_state.kd_buf_size;
		if (rc != DIFF_RC_OK)
			goto return_rc;
	}

	rc = DIFF_RC_OK;
return_rc:
	ARRAYLIST_FREE(state->temp_result);
	return rc;
}

int
diff_atomize_file(struct diff_data *d,
		  const struct diff_config *config,
		  FILE *f, const uint8_t *data, off_t len, int diff_flags)
{
	if (!config->atomize_func)
		return EINVAL;

	diff_data_init_root(d, f, data, len, diff_flags);

	return config->atomize_func(config->atomize_func_data, d);

}

struct diff_result *
diff_main(const struct diff_config *config, struct diff_data *left,
	  struct diff_data *right)
{
	struct diff_result *result = malloc(sizeof(struct diff_result));
	if (!result)
		return NULL;

	*result = (struct diff_result){};
	result->left = left;
	result->right = right;

	struct diff_state state = {
		.result = result,
		.recursion_depth_left = config->max_recursion_depth ?
		    config->max_recursion_depth : UINT_MAX,
		.kd_buf = NULL,
		.kd_buf_size = 0,
	};
	diff_data_init_subsection(&state.left, left,
				  left->atoms.head,
				  left->atoms.len);
	diff_data_init_subsection(&state.right, right,
				  right->atoms.head,
				  right->atoms.len);

	result->rc = diff_run_algo(config->algo, &state);
	free(state.kd_buf);

	return result;
}

void
diff_result_free(struct diff_result *result)
{
	if (!result)
		return;
	ARRAYLIST_FREE(result->chunks);
	free(result);
}

int
diff_result_contains_printable_chunks(struct diff_result *result)
{
	struct diff_chunk *c;
	enum diff_chunk_type t;
	int i;

	for (i = 0; i < result->chunks.len; i++) {
		c = &result->chunks.head[i];
		t = diff_chunk_type(c);
		if (t == CHUNK_MINUS || t == CHUNK_PLUS)
			return 1;
	}

	return 0;
}
