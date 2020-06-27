/*
 * *****************************************************************************
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Common code for the lexers.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include <status.h>
#include <lex.h>
#include <vm.h>
#include <bc.h>

void bc_lex_invalidChar(BcLex *l, char c) {
	l->t = BC_LEX_INVALID;
	bc_lex_verr(l, BC_ERROR_PARSE_CHAR, c);
}

void bc_lex_lineComment(BcLex *l) {
	l->t = BC_LEX_WHITESPACE;
	while (l->i < l->len && l->buf[l->i] != '\n') l->i += 1;
}

void bc_lex_comment(BcLex *l) {

	size_t i, nlines = 0;
	const char *buf = l->buf;
	bool end = false;
	char c;

	l->i += 1;
	l->t = BC_LEX_WHITESPACE;

	for (i = l->i; !end; i += !end) {

		for (; (c = buf[i]) && c != '*'; ++i) nlines += (c == '\n');

		if (BC_ERR(!c || buf[i + 1] == '\0')) {
			l->i = i;
			bc_lex_err(l, BC_ERROR_PARSE_COMMENT);
		}

		end = buf[i + 1] == '/';
	}

	l->i = i + 2;
	l->line += nlines;
}

void bc_lex_whitespace(BcLex *l) {
	char c;
	l->t = BC_LEX_WHITESPACE;
	for (c = l->buf[l->i]; c != '\n' && isspace(c); c = l->buf[++l->i]);
}

void bc_lex_commonTokens(BcLex *l, char c) {
	if (!c) l->t = BC_LEX_EOF;
	else if (c == '\n') l->t = BC_LEX_NLINE;
	else bc_lex_whitespace(l);
}

static size_t bc_lex_num(BcLex *l, char start, bool int_only) {

	const char *buf = l->buf + l->i;
	size_t i;
	char c;
	bool last_pt, pt = (start == '.');

	for (i = 0; (c = buf[i]) && (BC_LEX_NUM_CHAR(c, pt, int_only) ||
	                             (c == '\\' && buf[i + 1] == '\n')); ++i)
	{
		if (c == '\\') {

			if (buf[i + 1] == '\n') {

				i += 2;

				// Make sure to eat whitespace at the beginning of the line.
				while(isspace(buf[i]) && buf[i] != '\n') i += 1;

				c = buf[i];

				if (!BC_LEX_NUM_CHAR(c, pt, int_only)) break;
			}
			else break;
		}

		last_pt = (c == '.');
		if (pt && last_pt) break;
		pt = pt || last_pt;

		bc_vec_push(&l->str, &c);
	}

	return i;
}

void bc_lex_number(BcLex *l, char start) {

	l->t = BC_LEX_NUMBER;

	bc_vec_npop(&l->str, l->str.len);
	bc_vec_push(&l->str, &start);

	l->i += bc_lex_num(l, start, false);

#if BC_ENABLE_EXTRA_MATH
	{
		char c = l->buf[l->i];

		if (c == 'e') {

#if BC_ENABLED
			if (BC_IS_POSIX) bc_lex_err(l, BC_ERROR_POSIX_EXP_NUM);
#endif // BC_ENABLED

			bc_vec_push(&l->str, &c);
			l->i += 1;
			c = l->buf[l->i];

			if (c == BC_LEX_NEG_CHAR) {
				bc_vec_push(&l->str, &c);
				l->i += 1;
				c = l->buf[l->i];
			}

			if (BC_ERR(!BC_LEX_NUM_CHAR(c, false, true)))
				bc_lex_verr(l, BC_ERROR_PARSE_CHAR, c);

			l->i += bc_lex_num(l, 0, true);
		}
	}
#endif // BC_ENABLE_EXTRA_MATH

	bc_vec_pushByte(&l->str, '\0');
}

void bc_lex_name(BcLex *l) {

	size_t i = 0;
	const char *buf = l->buf + l->i - 1;
	char c = buf[i];

	l->t = BC_LEX_NAME;

	while ((c >= 'a' && c <= 'z') || isdigit(c) || c == '_') c = buf[++i];

	bc_vec_string(&l->str, i, buf);

	// Increment the index. We minus 1 because it has already been incremented.
	l->i += i - 1;
}

void bc_lex_init(BcLex *l) {
	BC_SIG_ASSERT_LOCKED;
	assert(l != NULL);
	bc_vec_init(&l->str, sizeof(char), NULL);
}

void bc_lex_free(BcLex *l) {
	BC_SIG_ASSERT_LOCKED;
	assert(l != NULL);
	bc_vec_free(&l->str);
}

void bc_lex_file(BcLex *l, const char *file) {
	assert(l != NULL && file != NULL);
	l->line = 1;
	vm.file = file;
}

void bc_lex_next(BcLex *l) {

	assert(l != NULL);

	l->last = l->t;
	l->line += (l->i != 0 && l->buf[l->i - 1] == '\n');

	if (BC_ERR(l->last == BC_LEX_EOF)) bc_lex_err(l, BC_ERROR_PARSE_EOF);

	l->t = BC_LEX_EOF;

	if (l->i == l->len) return;

	// Loop until failure or we don't have whitespace. This
	// is so the parser doesn't get inundated with whitespace.
	do {
		vm.next(l);
	} while (l->t == BC_LEX_WHITESPACE);
}

void bc_lex_text(BcLex *l, const char *text) {
	assert(l != NULL && text != NULL);
	l->buf = text;
	l->i = 0;
	l->len = strlen(text);
	l->t = l->last = BC_LEX_INVALID;
	bc_lex_next(l);
}
