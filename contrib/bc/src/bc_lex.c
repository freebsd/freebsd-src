/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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
 * The lexer for bc.
 *
 */

#if BC_ENABLED

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <bc.h>
#include <vm.h>

static void bc_lex_identifier(BcLex *l) {

	size_t i;
	const char *buf = l->buf + l->i - 1;

	for (i = 0; i < bc_lex_kws_len; ++i) {

		const BcLexKeyword *kw = bc_lex_kws + i;
		size_t n = BC_LEX_KW_LEN(kw);

		if (!strncmp(buf, kw->name, n) && !isalnum(buf[n]) && buf[n] != '_') {

			l->t = BC_LEX_KW_AUTO + (BcLexType) i;

			if (!BC_LEX_KW_POSIX(kw))
				bc_lex_verr(l, BC_ERR_POSIX_KW, kw->name);

			// We minus 1 because the index has already been incremented.
			l->i += n - 1;
			return;
		}
	}

	bc_lex_name(l);

	if (BC_ERR(l->str.len - 1 > 1))
		bc_lex_verr(l, BC_ERR_POSIX_NAME_LEN, l->str.v);
}

static void bc_lex_string(BcLex *l) {

	size_t len, nlines = 0, i = l->i;
	const char *buf = l->buf;
	char c;

	l->t = BC_LEX_STR;

	for (; (c = buf[i]) && c != '"'; ++i) nlines += c == '\n';

	if (BC_ERR(c == '\0')) {
		l->i = i;
		bc_lex_err(l, BC_ERR_PARSE_STRING);
	}

	len = i - l->i;
	bc_vec_string(&l->str, len, l->buf + l->i);

	l->i = i + 1;
	l->line += nlines;
}

static void bc_lex_assign(BcLex *l, BcLexType with, BcLexType without) {
	if (l->buf[l->i] == '=') {
		l->i += 1;
		l->t = with;
	}
	else l->t = without;
}

void bc_lex_token(BcLex *l) {

	char c = l->buf[l->i++], c2;

	// This is the workhorse of the lexer.
	switch (c) {

		case '\0':
		case '\n':
		case '\t':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
		{
			bc_lex_commonTokens(l, c);
			break;
		}

		case '!':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);

			if (l->t == BC_LEX_OP_BOOL_NOT)
				bc_lex_verr(l, BC_ERR_POSIX_BOOL, "!");

			break;
		}

		case '"':
		{
			bc_lex_string(l);
			break;
		}

		case '#':
		{
			bc_lex_err(l, BC_ERR_POSIX_COMMENT);
			bc_lex_lineComment(l);
			break;
		}

		case '%':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MODULUS, BC_LEX_OP_MODULUS);
			break;
		}

		case '&':
		{
			c2 = l->buf[l->i];
			if (BC_NO_ERR(c2 == '&')) {

				bc_lex_verr(l, BC_ERR_POSIX_BOOL, "&&");

				l->i += 1;
				l->t = BC_LEX_OP_BOOL_AND;
			}
			else bc_lex_invalidChar(l, c);

			break;
		}
#if BC_ENABLE_EXTRA_MATH
		case '$':
		{
			l->t = BC_LEX_OP_TRUNC;
			break;
		}

		case '@':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_PLACES, BC_LEX_OP_PLACES);
			break;
		}
#endif // BC_ENABLE_EXTRA_MATH
		case '(':
		case ')':
		{
			l->t = (BcLexType) (c - '(' + BC_LEX_LPAREN);
			break;
		}

		case '*':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_MULTIPLY, BC_LEX_OP_MULTIPLY);
			break;
		}

		case '+':
		{
			c2 = l->buf[l->i];
			if (c2 == '+') {
				l->i += 1;
				l->t = BC_LEX_OP_INC;
			}
			else bc_lex_assign(l, BC_LEX_OP_ASSIGN_PLUS, BC_LEX_OP_PLUS);
			break;
		}

		case ',':
		{
			l->t = BC_LEX_COMMA;
			break;
		}

		case '-':
		{
			c2 = l->buf[l->i];
			if (c2 == '-') {
				l->i += 1;
				l->t = BC_LEX_OP_DEC;
			}
			else bc_lex_assign(l, BC_LEX_OP_ASSIGN_MINUS, BC_LEX_OP_MINUS);
			break;
		}

		case '.':
		{
			c2 = l->buf[l->i];
			if (BC_LEX_NUM_CHAR(c2, true, false)) bc_lex_number(l, c);
			else {
				l->t = BC_LEX_KW_LAST;
				bc_lex_err(l, BC_ERR_POSIX_DOT);
			}
			break;
		}

		case '/':
		{
			c2 = l->buf[l->i];
			if (c2 =='*') bc_lex_comment(l);
			else bc_lex_assign(l, BC_LEX_OP_ASSIGN_DIVIDE, BC_LEX_OP_DIVIDE);
			break;
		}

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		// Apparently, GNU bc (and maybe others) allows any uppercase letter as
		// a number. When single digits, they act like the ones above. When
		// multi-digit, any letter above the input base is automatically set to
		// the biggest allowable digit in the input base.
		case 'G':
		case 'H':
		case 'I':
		case 'J':
		case 'K':
		case 'L':
		case 'M':
		case 'N':
		case 'O':
		case 'P':
		case 'Q':
		case 'R':
		case 'S':
		case 'T':
		case 'U':
		case 'V':
		case 'W':
		case 'X':
		case 'Y':
		case 'Z':
		{
			bc_lex_number(l, c);
			break;
		}

		case ';':
		{
			l->t = BC_LEX_SCOLON;
			break;
		}

		case '<':
		{
#if BC_ENABLE_EXTRA_MATH
			c2 = l->buf[l->i];

			if (c2 == '<') {
				l->i += 1;
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_LSHIFT, BC_LEX_OP_LSHIFT);
				break;
			}
#endif // BC_ENABLE_EXTRA_MATH
			bc_lex_assign(l, BC_LEX_OP_REL_LE, BC_LEX_OP_REL_LT);
			break;
		}

		case '=':
		{
			bc_lex_assign(l, BC_LEX_OP_REL_EQ, BC_LEX_OP_ASSIGN);
			break;
		}

		case '>':
		{
#if BC_ENABLE_EXTRA_MATH
			c2 = l->buf[l->i];

			if (c2 == '>') {
				l->i += 1;
				bc_lex_assign(l, BC_LEX_OP_ASSIGN_RSHIFT, BC_LEX_OP_RSHIFT);
				break;
			}
#endif // BC_ENABLE_EXTRA_MATH
			bc_lex_assign(l, BC_LEX_OP_REL_GE, BC_LEX_OP_REL_GT);
			break;
		}

		case '[':
		case ']':
		{
			l->t = (BcLexType) (c - '[' + BC_LEX_LBRACKET);
			break;
		}

		case '\\':
		{
			if (BC_NO_ERR(l->buf[l->i] == '\n')) {
				l->i += 1;
				l->t = BC_LEX_WHITESPACE;
			}
			else bc_lex_invalidChar(l, c);
			break;
		}

		case '^':
		{
			bc_lex_assign(l, BC_LEX_OP_ASSIGN_POWER, BC_LEX_OP_POWER);
			break;
		}

		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
		case 'i':
		case 'j':
		case 'k':
		case 'l':
		case 'm':
		case 'n':
		case 'o':
		case 'p':
		case 'q':
		case 'r':
		case 's':
		case 't':
		case 'u':
		case 'v':
		case 'w':
		case 'x':
		case 'y':
		case 'z':
		{
			bc_lex_identifier(l);
			break;
		}

		case '{':
		case '}':
		{
			l->t = (BcLexType) (c - '{' + BC_LEX_LBRACE);
			break;
		}

		case '|':
		{
			c2 = l->buf[l->i];

			if (BC_NO_ERR(c2 == '|')) {

				bc_lex_verr(l, BC_ERR_POSIX_BOOL, "||");

				l->i += 1;
				l->t = BC_LEX_OP_BOOL_OR;
			}
			else bc_lex_invalidChar(l, c);

			break;
		}

		default:
		{
			bc_lex_invalidChar(l, c);
		}
	}
}
#endif // BC_ENABLED
