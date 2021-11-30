/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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

#include <lex.h>
#include <vm.h>
#include <bc.h>

void bc_lex_invalidChar(BcLex *l, char c) {
	l->t = BC_LEX_INVALID;
	bc_lex_verr(l, BC_ERR_PARSE_CHAR, c);
}

void bc_lex_lineComment(BcLex *l) {
	l->t = BC_LEX_WHITESPACE;
	while (l->i < l->len && l->buf[l->i] != '\n') l->i += 1;
}

void bc_lex_comment(BcLex *l) {

	size_t i, nlines = 0;
	const char *buf;
	bool end = false, got_more;
	char c;

	l->i += 1;
	l->t = BC_LEX_WHITESPACE;

	// This loop is complex because it might need to request more data from
	// stdin if the comment is not ended. This loop is taken until the comment
	// is finished or we have EOF.
	do {

		buf = l->buf;
		got_more = false;

		// If we are in stdin mode, the buffer must be the one used for stdin.
		assert(!vm.is_stdin || buf == vm.buffer.v);

		// Find the end of the comment.
		for (i = l->i; !end; i += !end) {

			// While we don't have an asterisk, eat, but increment nlines.
			for (; (c = buf[i]) && c != '*'; ++i) nlines += (c == '\n');

			// If this is true, we need to request more data.
			if (BC_ERR(!c || buf[i + 1] == '\0')) {

				// Read more.
				if (!vm.eof && l->is_stdin) got_more = bc_lex_readLine(l);

				break;
			}

			// If this turns true, we found the end. Yay!
			end = (buf[i + 1] == '/');
		}

	} while (got_more && !end);

	// If we didn't find the end, barf.
	if (!end) {
		l->i = i;
		bc_lex_err(l, BC_ERR_PARSE_COMMENT);
	}

	l->i = i + 2;
	l->line += nlines;
}

void bc_lex_whitespace(BcLex *l) {

	char c;

	l->t = BC_LEX_WHITESPACE;

	// Eat. We don't eat newlines because they can be special.
	for (c = l->buf[l->i]; c != '\n' && isspace(c); c = l->buf[++l->i]);
}

void bc_lex_commonTokens(BcLex *l, char c) {
	if (!c) l->t = BC_LEX_EOF;
	else if (c == '\n') l->t = BC_LEX_NLINE;
	else bc_lex_whitespace(l);
}

/**
 * Parses a number.
 * @param l         The lexer.
 * @param start     The start character.
 * @param int_only  Whether this function should only look for an integer. This
 *                  is used to implement the exponent of scientific notation.
 */
static size_t bc_lex_num(BcLex *l, char start, bool int_only) {

	const char *buf = l->buf + l->i;
	size_t i;
	char c;
	bool last_pt, pt = (start == '.');

	// This loop looks complex. It is not. It is asking if the character is not
	// a nul byte and it if it a valid num character based on what we have found
	// thus far, or whether it is a backslash followed by a newline. I can do
	// i+1 on the buffer because the buffer must have a nul byte.
	for (i = 0; (c = buf[i]) && (BC_LEX_NUM_CHAR(c, pt, int_only) ||
	                             (c == '\\' && buf[i + 1] == '\n')); ++i)
	{
		// I don't need to test that the next character is a newline because
		// the loop condition above ensures that.
		if (c == '\\') {

			i += 2;

			// Make sure to eat whitespace at the beginning of the line.
			while(isspace(buf[i]) && buf[i] != '\n') i += 1;

			c = buf[i];

			// If the next character is not a number character, bail.
			if (!BC_LEX_NUM_CHAR(c, pt, int_only)) break;
		}

		// Did we find the radix point?
		last_pt = (c == '.');

		// If we did, and we already have one, then break because it's not part
		// of this number.
		if (pt && last_pt) break;

		// Set whether we have found a radix point.
		pt = pt || last_pt;

		bc_vec_push(&l->str, &c);
	}

	return i;
}

void bc_lex_number(BcLex *l, char start) {

	l->t = BC_LEX_NUMBER;

	// Make sure the string is clear.
	bc_vec_popAll(&l->str);
	bc_vec_push(&l->str, &start);

	// Parse the number.
	l->i += bc_lex_num(l, start, false);

#if BC_ENABLE_EXTRA_MATH
	{
		char c = l->buf[l->i];

		// Do we have a number in scientific notation?
		if (c == 'e') {

#if BC_ENABLED
			// Barf for POSIX.
			if (BC_IS_POSIX) bc_lex_err(l, BC_ERR_POSIX_EXP_NUM);
#endif // BC_ENABLED

			// Push the e.
			bc_vec_push(&l->str, &c);
			l->i += 1;
			c = l->buf[l->i];

			// Check for negative specifically because bc_lex_num() does not.
			if (c == BC_LEX_NEG_CHAR) {
				bc_vec_push(&l->str, &c);
				l->i += 1;
				c = l->buf[l->i];
			}

			// We must have a number character, so barf if not.
			if (BC_ERR(!BC_LEX_NUM_CHAR(c, false, true)))
				bc_lex_verr(l, BC_ERR_PARSE_CHAR, c);

			// Parse the exponent.
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

	// Should be obvious. It's looking for valid characters.
	while ((c >= 'a' && c <= 'z') || isdigit(c) || c == '_') c = buf[++i];

	// Set the string to the identifier.
	bc_vec_string(&l->str, i, buf);

	// Increment the index. We minus 1 because it has already been incremented.
	l->i += i - 1;
}

void bc_lex_init(BcLex *l) {
	BC_SIG_ASSERT_LOCKED;
	assert(l != NULL);
	bc_vec_init(&l->str, sizeof(char), BC_DTOR_NONE);
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

	BC_SIG_ASSERT_LOCKED;

	assert(l != NULL);

	l->last = l->t;

	// If this wasn't here, the line number would be off.
	l->line += (l->i != 0 && l->buf[l->i - 1] == '\n');

	// If the last token was EOF, someone called this one too many times.
	if (BC_ERR(l->last == BC_LEX_EOF)) bc_lex_err(l, BC_ERR_PARSE_EOF);

	l->t = BC_LEX_EOF;

	// We are done if this is true.
	if (l->i == l->len) return;

	// Loop until failure or we don't have whitespace. This
	// is so the parser doesn't get inundated with whitespace.
	do {
		vm.next(l);
	} while (l->t == BC_LEX_WHITESPACE);
}

/**
 * Updates the buffer and len so that they are not invalidated when the stdin
 * buffer grows.
 * @param l     The lexer.
 * @param text  The text.
 * @param len   The length of the text.
 */
static void bc_lex_fixText(BcLex *l, const char *text, size_t len) {
	l->buf = text;
	l->len = len;
}

bool bc_lex_readLine(BcLex *l) {

	bool good;

	// These are reversed because they should be already locked, but
	// bc_vm_readLine() needs them to be unlocked.
	BC_SIG_UNLOCK;

	good = bc_vm_readLine(false);

	BC_SIG_LOCK;

	bc_lex_fixText(l, vm.buffer.v, vm.buffer.len - 1);

	return good;
}

void bc_lex_text(BcLex *l, const char *text, bool is_stdin) {

	BC_SIG_ASSERT_LOCKED;

	assert(l != NULL && text != NULL);

	bc_lex_fixText(l, text, strlen(text));
	l->i = 0;
	l->t = l->last = BC_LEX_INVALID;
	l->is_stdin = is_stdin;

	bc_lex_next(l);
}
