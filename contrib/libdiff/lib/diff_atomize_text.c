/* Split source by line breaks, and calculate a simplistic checksum. */
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
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include <arraylist.h>
#include <diff_main.h>

#include "diff_internal.h"
#include "diff_debug.h"

unsigned int
diff_atom_hash_update(unsigned int hash, unsigned char atom_byte)
{
	return hash * 23 + atom_byte;
}

static int
diff_data_atomize_text_lines_fd(struct diff_data *d)
{
	off_t pos = 0;
	const off_t end = pos + d->len;
	unsigned int array_size_estimate = d->len / 50;
	unsigned int pow2 = 1;
	bool ignore_whitespace = (d->diff_flags & DIFF_FLAG_IGNORE_WHITESPACE);
	bool embedded_nul = false;

	while (array_size_estimate >>= 1)
		pow2++;

	ARRAYLIST_INIT(d->atoms, 1 << pow2);

	if (fseek(d->root->f, 0L, SEEK_SET) == -1)
		return errno;

	while (pos < end) {
		off_t line_end = pos;
		unsigned int hash = 0;
		unsigned char buf[512];
		size_t r, i;
		struct diff_atom *atom;
		int eol = 0;

		while (eol == 0 && line_end < end) {
			r = fread(buf, sizeof(char), sizeof(buf), d->root->f);
			if (r == 0 && ferror(d->root->f))
				return EIO;
			i = 0;
			while (eol == 0 && i < r) {
				if (buf[i] != '\r' && buf[i] != '\n') {
					if (!ignore_whitespace
					    || !isspace((unsigned char)buf[i]))
						hash = diff_atom_hash_update(
						    hash, buf[i]);
					if (buf[i] == '\0')
						embedded_nul = true;
					line_end++;
				} else
					eol = buf[i];
				i++;
			}
		}

		/* When not at the end of data, the line ending char ('\r' or
		 * '\n') must follow */
		if (line_end < end)
			line_end++;
		/* If that was an '\r', also pull in any following '\n' */
		if (line_end < end && eol == '\r') {
			if (fseeko(d->root->f, line_end, SEEK_SET) == -1)
				return errno;
			r = fread(buf, sizeof(char), sizeof(buf), d->root->f);
			if (r == 0 && ferror(d->root->f))
				return EIO;
			if (r > 0 && buf[0] == '\n')
				line_end++;
		}

		/* Record the found line as diff atom */
		ARRAYLIST_ADD(atom, d->atoms);
		if (!atom)
			return ENOMEM;

		*atom = (struct diff_atom){
			.root = d,
			.pos = pos,
			.at = NULL,	/* atom data is not memory-mapped */
			.len = line_end - pos,
			.hash = hash,
		};

		/* Starting point for next line: */
		pos = line_end;
		if (fseeko(d->root->f, pos, SEEK_SET) == -1)
			return errno;
	}

	/* File are considered binary if they contain embedded '\0' bytes. */
	if (embedded_nul)
		d->atomizer_flags |= DIFF_ATOMIZER_FOUND_BINARY_DATA;

	return DIFF_RC_OK;
}

static sigjmp_buf diff_data_signal_env;
static void
diff_data_signal_handler(int sig)
{
	siglongjmp(diff_data_signal_env, sig);
}

static int
diff_data_atomize_text_lines_mmap(struct diff_data *d)
{
	struct sigaction act, oact;
	const uint8_t *volatile pos = d->data;
	const uint8_t *end = pos + d->len;
	bool ignore_whitespace = (d->diff_flags & DIFF_FLAG_IGNORE_WHITESPACE);
	bool embedded_nul = false;
	unsigned int array_size_estimate = d->len / 50;
	unsigned int pow2 = 1;
	while (array_size_estimate >>= 1)
		pow2++;

	ARRAYLIST_INIT(d->atoms, 1 << pow2);

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = diff_data_signal_handler;
	sigaction(SIGBUS, &act, &oact);
	if (sigsetjmp(diff_data_signal_env, 0) > 0) {
		/*
		 * The file was truncated while we were reading it.  Set
		 * the end pointer to the beginning of the line we were
		 * trying to read, adjust the file length, and set a flag.
		 */
		end = pos;
		d->len = end - d->data;
		d->atomizer_flags |= DIFF_ATOMIZER_FILE_TRUNCATED;
	}
	while (pos < end) {
		const uint8_t *line_start = pos, *line_end = pos;
		unsigned int hash = 0;

		while (line_end < end && *line_end != '\r' && *line_end != '\n') {
			if (!ignore_whitespace
			    || !isspace((unsigned char)*line_end))
				hash = diff_atom_hash_update(hash, *line_end);
			if (*line_end == '\0')
				embedded_nul = true;
			line_end++;
		}

		/* When not at the end of data, the line ending char ('\r' or
		 * '\n') must follow */
		if (line_end < end && *line_end == '\r')
			line_end++;
		if (line_end < end && *line_end == '\n')
			line_end++;

		/* Record the found line as diff atom */
		struct diff_atom *atom;
		ARRAYLIST_ADD(atom, d->atoms);
		if (!atom)
			return ENOMEM;

		*atom = (struct diff_atom){
			.root = d,
			.pos = (off_t)(line_start - d->data),
			.at = line_start,
			.len = line_end - line_start,
			.hash = hash,
		};

		/* Starting point for next line: */
		pos = line_end;
	}
	sigaction(SIGBUS, &oact, NULL);

	/* File are considered binary if they contain embedded '\0' bytes. */
	if (embedded_nul)
		d->atomizer_flags |= DIFF_ATOMIZER_FOUND_BINARY_DATA;

	return DIFF_RC_OK;
}

static int
diff_data_atomize_text_lines(struct diff_data *d)
{
	if (d->data == NULL)
		return diff_data_atomize_text_lines_fd(d);
	else
		return diff_data_atomize_text_lines_mmap(d);
}

int
diff_atomize_text_by_line(void *func_data, struct diff_data *d)
{
	return diff_data_atomize_text_lines(d);
}
