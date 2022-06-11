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
 * The lexer for bc.
 *
 */

#if BC_ENABLED

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <bc.h>
#include <vm.h>

/**
 * Lexes an identifier, which may be a keyword.
 * @param l  The lexer.
 */
static void
bc_lex_identifier(BcLex* l)
{
	// We already passed the first character, so we need to be sure to include
	// it.
	const char* buf = l->buf + l->i - 1;
	size_t i;

	// This loop is simply checking for keywords.
	for (i = 0; i < bc_lex_kws_len; ++i)
	{
		const BcLexKeyword* kw = bc_lex_kws + i;
		size_t n = BC_LEX_KW_LEN(kw);

		if (!strncmp(buf, kw->name, n) && !isalnum(buf[n]) && buf[n] != '_')
		{
			// If the keyword has been redefined, and redefinition is allowed
			// (it is not allowed for builtin libraries), break out of the loop
			// and use it as a name. This depends on the argument parser to
			// ensure that only non-POSIX keywords get redefined.
			if (!vm.no_redefine && vm.redefined_kws[i]) break;

			l->t = BC_LEX_KW_AUTO + (BcLexType) i;

			// Warn or error, as appropriate for the mode, if the keyword is not
			// in the POSIX standard.
			if (!BC_LEX_KW_POSIX(kw)) bc_lex_verr(l, BC_ERR_POSIX_KW, kw->name);

			// We minus 1 because the index has already been incremented.
			l->i += n - 1;

			// Already have the token; bail.
			return;
		}
	}

	// If not a keyword, parse the name.
	bc_lex_name(l);

	// POSIX doesn't allow identifiers that are more than one character, so we
	// might have to warn or error here too.
	if (BC_ERR(l->str.len - 1 > 1))
	{
		bc_lex_verr(l, BC_ERR_POSIX_NAME_LEN, l->str.v);
	}
}

/**
 * Parses a bc string. This is separate from dc strings because dc strings need
 * to be balanced.
 * @param l  The lexer.
 */
static void
bc_lex_string(BcLex* l)
{
	// We need to keep track of newlines to increment them properly.
	size_t len, nlines, i;
	const char* buf;
	char c;
	bool got_more;

	l->t = BC_LEX_STR;

	do
	{
		nlines = 0;
		buf = l->buf;
		got_more = false;

		assert(!vm.is_stdin || buf == vm.buffer.v);

		// Fortunately for us, bc doesn't escape quotes. Instead, the equivalent
		// is '\q', which makes this loop simpler.
		for (i = l->i; (c = buf[i]) && c != '"'; ++i)
		{
			nlines += (c == '\n');
		}

		if (BC_ERR(c == '\0') && !vm.eof && (l->is_stdin || l->is_exprs))
		{
			got_more = bc_lex_readLine(l);
		}
	}
	while (got_more && c != '"');

	// If the string did not end properly, barf.
	if (c != '"')
	{
		l->i = i;
		bc_lex_err(l, BC_ERR_PARSE_STRING);
	}

	// Set the temp string to the parsed string.
	len = i - l->i;
	bc_vec_string(&l->str, len, l->buf + l->i);

	l->i = i + 1;
	l->line += nlines;
}

/**
 * This function takes a lexed operator and checks to see if it's the assignment
 * version, setting the token appropriately.
 * @param l        The lexer.
 * @param with     The token to assign if it is an assignment operator.
 * @param without  The token to assign if it is not an assignment operator.
 */
static void
bc_lex_assign(BcLex* l, BcLexType with, BcLexType without)
{
	if (l->buf[l->i] == '=')
	{
		l->i += 1;
		l->t = with;
	}
	else l->t = without;
}

void
bc_lex_token(BcLex* l)
{
	// We increment here. This means that all lexing needs to take that into
	// account, such as when parsing an identifier. If we don't, the first
	// character of every identifier would be missing.
	char c = l->buf[l->i++], c2;

	BC_SIG_ASSERT_LOCKED;

	// This is the workhorse of the lexer.
	switch (c)
	{
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
			// Even though it's not an assignment, we can use this.
			bc_lex_assign(l, BC_LEX_OP_REL_NE, BC_LEX_OP_BOOL_NOT);

			// POSIX doesn't allow boolean not.
			if (l->t == BC_LEX_OP_BOOL_NOT)
			{
				bc_lex_verr(l, BC_ERR_POSIX_BOOL, "!");
			}

			break;
		}

		case '"':
		{
			bc_lex_string(l);
			break;
		}

		case '#':
		{
			// POSIX does not allow line comments.
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

			// Either we have boolean and or an error. And boolean and is not
			// allowed by POSIX.
			if (BC_NO_ERR(c2 == '&'))
			{
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

			// Have to check for increment first.
			if (c2 == '+')
			{
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

			// Have to check for decrement first.
			if (c2 == '-')
			{
				l->i += 1;
				l->t = BC_LEX_OP_DEC;
			}
			else bc_lex_assign(l, BC_LEX_OP_ASSIGN_MINUS, BC_LEX_OP_MINUS);
			break;
		}

		case '.':
		{
			c2 = l->buf[l->i];

			// If it's alone, it's an alias for last.
			if (BC_LEX_NUM_CHAR(c2, true, false)) bc_lex_number(l, c);
			else
			{
				l->t = BC_LEX_KW_LAST;
				bc_lex_err(l, BC_ERR_POSIX_DOT);
			}

			break;
		}

		case '/':
		{
			c2 = l->buf[l->i];
			if (c2 == '*') bc_lex_comment(l);
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

			// Check for shift.
			if (c2 == '<')
			{
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

			// Check for shift.
			if (c2 == '>')
			{
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
			// In bc, a backslash+newline is whitespace.
			if (BC_NO_ERR(l->buf[l->i] == '\n'))
			{
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

			// Once again, boolean or is not allowed by POSIX.
			if (BC_NO_ERR(c2 == '|'))
			{
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
