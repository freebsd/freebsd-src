/* Common parts for printing diff output */
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

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arraylist.h>
#include <diff_main.h>
#include <diff_output.h>

#include "diff_internal.h"

static int
get_atom_byte(int *ch, struct diff_atom *atom, off_t off)
{
	off_t cur;

	if (atom->at != NULL) {
		*ch = atom->at[off];
		return 0;
	}

	cur = ftello(atom->root->f);
	if (cur == -1)
		return errno;

	if (cur != atom->pos + off &&
	    fseeko(atom->root->f, atom->pos + off, SEEK_SET) == -1)
		return errno;

	*ch = fgetc(atom->root->f);
	if (*ch == EOF && ferror(atom->root->f))
		return errno;

	return 0;
}

#define DIFF_OUTPUT_BUF_SIZE	512

int
diff_output_lines(struct diff_output_info *outinfo, FILE *dest,
		  const char *prefix, struct diff_atom *start_atom,
		  unsigned int count)
{
	struct diff_atom *atom;
	off_t outoff = 0, *offp;
	uint8_t *typep;
	int rc;

	if (outinfo && outinfo->line_offsets.len > 0) {
		unsigned int idx = outinfo->line_offsets.len - 1;
		outoff = outinfo->line_offsets.head[idx];
	}

	foreach_diff_atom(atom, start_atom, count) {
		off_t outlen = 0;
		int i, ch, nbuf = 0;
		unsigned int len = atom->len;
		unsigned char buf[DIFF_OUTPUT_BUF_SIZE + 1 /* '\n' */];
		size_t n;

		n = strlcpy(buf, prefix, sizeof(buf));
		if (n >= DIFF_OUTPUT_BUF_SIZE) /* leave room for '\n' */
			return ENOBUFS;
		nbuf += n;

		if (len) {
			rc = get_atom_byte(&ch, atom, len - 1);
			if (rc)
				return rc;
			if (ch == '\n')
				len--;
		}

		for (i = 0; i < len; i++) {
			rc = get_atom_byte(&ch, atom, i);
			if (rc)
				return rc;
			if (nbuf >= DIFF_OUTPUT_BUF_SIZE) {
				rc = fwrite(buf, 1, nbuf, dest);
				if (rc != nbuf)
					return errno;
				outlen += rc;
				nbuf = 0;
			}
			buf[nbuf++] = ch;
		}
		buf[nbuf++] = '\n';
		rc = fwrite(buf, 1, nbuf, dest);
		if (rc != nbuf)
			return errno;
		outlen += rc;
		if (outinfo) {
			ARRAYLIST_ADD(offp, outinfo->line_offsets);
			if (offp == NULL)
				return ENOMEM;
			outoff += outlen;
			*offp = outoff;
			ARRAYLIST_ADD(typep, outinfo->line_types);
			if (typep == NULL)
				return ENOMEM;
			*typep = *prefix == ' ' ? DIFF_LINE_CONTEXT :
			    *prefix == '-' ? DIFF_LINE_MINUS :
			    *prefix == '+' ? DIFF_LINE_PLUS : DIFF_LINE_NONE;
		}
	}

	return DIFF_RC_OK;
}

int
diff_output_chunk_left_version(struct diff_output_info **output_info,
			       FILE *dest,
			       const struct diff_input_info *info,
			       const struct diff_result *result,
			       const struct diff_chunk_context *cc)
{
	int rc, c_idx;
	struct diff_output_info *outinfo = NULL;

	if (diff_range_empty(&cc->left))
		return DIFF_RC_OK;

	if (output_info) {
		*output_info = diff_output_info_alloc();
		if (*output_info == NULL)
			return ENOMEM;
		outinfo = *output_info;
	}

	/* Write out all chunks on the left side. */
	for (c_idx = cc->chunk.start; c_idx < cc->chunk.end; c_idx++) {
		const struct diff_chunk *c = &result->chunks.head[c_idx];

		if (c->left_count) {
			rc = diff_output_lines(outinfo, dest, "",
			    c->left_start, c->left_count);
			if (rc)
				return rc;
		}
	}

	return DIFF_RC_OK;
}

int
diff_output_chunk_right_version(struct diff_output_info **output_info,
				FILE *dest,
				const struct diff_input_info *info,
				const struct diff_result *result,
				const struct diff_chunk_context *cc)
{
	int rc, c_idx;
	struct diff_output_info *outinfo = NULL;

	if (diff_range_empty(&cc->right))
		return DIFF_RC_OK;

	if (output_info) {
		*output_info = diff_output_info_alloc();
		if (*output_info == NULL)
			return ENOMEM;
		outinfo = *output_info;
	}

	/* Write out all chunks on the right side. */
	for (c_idx = cc->chunk.start; c_idx < cc->chunk.end; c_idx++) {
		const struct diff_chunk *c = &result->chunks.head[c_idx];

		if (c->right_count) {
			rc = diff_output_lines(outinfo, dest, "", c->right_start,
			    c->right_count);
			if (rc)
				return rc;
		}
	}

	return DIFF_RC_OK;
}

int
diff_output_trailing_newline_msg(struct diff_output_info *outinfo, FILE *dest,
				 const struct diff_chunk *c)
{
	enum diff_chunk_type chunk_type = diff_chunk_type(c);
	struct diff_atom *atom, *start_atom;
	unsigned int atom_count;
	int rc, ch;
	off_t outoff = 0, *offp;
	uint8_t *typep;


	if (chunk_type == CHUNK_MINUS || chunk_type == CHUNK_SAME) {
		start_atom = c->left_start;
		atom_count = c->left_count;
	} else if (chunk_type == CHUNK_PLUS) {
		start_atom = c->right_start;
		atom_count = c->right_count;
	} else
		return EINVAL;

	/* Locate the last atom. */
	if (atom_count == 0)
		return EINVAL;
	atom = &start_atom[atom_count - 1];

	rc = get_atom_byte(&ch, atom, atom->len - 1);
	if (rc != DIFF_RC_OK)
		return rc;

	if (ch != '\n') {
		if (outinfo && outinfo->line_offsets.len > 0) {
			unsigned int idx = outinfo->line_offsets.len - 1;
			outoff = outinfo->line_offsets.head[idx];
		}
		rc = fprintf(dest, "\\ No newline at end of file\n");
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
			*typep = DIFF_LINE_NONE;
		}
	}

	return DIFF_RC_OK;
}

static bool
is_function_prototype(unsigned char ch)
{
	return (isalpha((unsigned char)ch) || ch == '_' || ch == '$' ||
	    ch == '-' || ch == '+');
}

#define begins_with(s, pre) (strncmp(s, pre, sizeof(pre)-1) == 0)

int
diff_output_match_function_prototype(char *prototype, size_t prototype_size,
    int *last_prototype_idx, const struct diff_result *result,
    int chunk_start_line)
{
	struct diff_atom *start_atom, *atom;
	const struct diff_data *data;
	unsigned char buf[DIFF_FUNCTION_CONTEXT_SIZE];
	const char *state = NULL;
	int rc, i, ch;

	if (result->left->atoms.len > 0 && chunk_start_line > 0) {
		data = result->left;
		start_atom = &data->atoms.head[chunk_start_line - 1];
	} else
		return DIFF_RC_OK;

	diff_data_foreach_atom_backwards_from(start_atom, atom, data) {
		int atom_idx = diff_atom_root_idx(data, atom);
		if (atom_idx < *last_prototype_idx)
			break;
		rc = get_atom_byte(&ch, atom, 0);
		if (rc)
			return rc;
		buf[0] = (unsigned char)ch;
		if (!is_function_prototype(buf[0]))
			continue;
		for (i = 1; i < atom->len && i < sizeof(buf) - 1; i++) {
			rc = get_atom_byte(&ch, atom, i);
			if (rc)
				return rc;
			if (ch == '\n')
				break;
			buf[i] = (unsigned char)ch;
		}
		buf[i] = '\0';
		if (begins_with(buf, "private:")) {
			if (!state)
				state = " (private)";
		} else if (begins_with(buf, "protected:")) {
			if (!state)
				state = " (protected)";
		} else if (begins_with(buf, "public:")) {
			if (!state)
				state = " (public)";
		} else {
			if (state)  /* don't care about truncation */
				strlcat(buf, state, sizeof(buf));
			strlcpy(prototype, buf, prototype_size);
			break;
		}
	}

	*last_prototype_idx = diff_atom_root_idx(data, start_atom);
	return DIFF_RC_OK;
}

struct diff_output_info *
diff_output_info_alloc(void)
{
	struct diff_output_info *output_info;
	off_t *offp;
	uint8_t *typep;

	output_info = malloc(sizeof(*output_info));
	if (output_info != NULL) {
		ARRAYLIST_INIT(output_info->line_offsets, 128);
		ARRAYLIST_ADD(offp, output_info->line_offsets);
		if (offp == NULL) {
			diff_output_info_free(output_info);
			return NULL;
		}
		*offp = 0;
		ARRAYLIST_INIT(output_info->line_types, 128);
		ARRAYLIST_ADD(typep, output_info->line_types);
		if (typep == NULL) {
			diff_output_info_free(output_info);
			return NULL;
		}
		*typep = DIFF_LINE_NONE;
	}
	return output_info;
}

void
diff_output_info_free(struct diff_output_info *output_info)
{
	ARRAYLIST_FREE(output_info->line_offsets);
	ARRAYLIST_FREE(output_info->line_types);
	free(output_info);
}

const char *
diff_output_get_label_left(const struct diff_input_info *info)
{
	if (info->flags & DIFF_INPUT_LEFT_NONEXISTENT)
		return "/dev/null";

	return info->left_path ? info->left_path : "a";
}

const char *
diff_output_get_label_right(const struct diff_input_info *info)
{
	if (info->flags & DIFF_INPUT_RIGHT_NONEXISTENT)
		return "/dev/null";

	return info->right_path ? info->right_path : "b";
}
