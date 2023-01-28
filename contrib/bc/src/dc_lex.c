/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2023 Gavin D. Howard and contributors.
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
 * The lexer for dc.
 *
 */

#if DC_ENABLED

#include <ctype.h>

#include <dc.h>
#include <vm.h>

bool
dc_lex_negCommand(BcLex* l)
{
	char c = l->buf[l->i];
	return !BC_LEX_NUM_CHAR(c, false, false);
}

/**
 * Processes a dc command that needs a register. This is where the
 * extended-register extension is implemented.
 * @param l  The lexer.
 */
static void
dc_lex_register(BcLex* l)
{
	// If extended register is enabled and the character is whitespace...
	if (DC_X && isspace(l->buf[l->i - 1]))
	{
		char c;

		// Eat the whitespace.
		bc_lex_whitespace(l);
		c = l->buf[l->i];

		// Check for a letter or underscore.
		if (BC_ERR(!isalpha(c) && c != '_'))
		{
			bc_lex_verr(l, BC_ERR_PARSE_CHAR, c);
		}

		// Parse a normal identifier.
		l->i += 1;
		bc_lex_name(l);
	}
	else
	{
		// I don't allow newlines because newlines are used for controlling when
		// execution happens, and allowing newlines would just be complex.
		if (BC_ERR(l->buf[l->i - 1] == '\n'))
		{
			bc_lex_verr(l, BC_ERR_PARSE_CHAR, l->buf[l->i - 1]);
		}

		// Set the lexer string and token.
		bc_vec_popAll(&l->str);
		bc_vec_pushByte(&l->str, (uchar) l->buf[l->i - 1]);
		bc_vec_pushByte(&l->str, '\0');
		l->t = BC_LEX_NAME;
	}
}

/**
 * Parses a dc string. Since dc's strings need to check for balanced brackets,
 * we can't just parse bc and dc strings with different start and end
 * characters. Oh, and dc strings need to check for escaped brackets.
 * @param l  The lexer.
 */
static void
dc_lex_string(BcLex* l)
{
	size_t depth, nls, i;
	char c;
	bool got_more;

	// Set the token and clear the string.
	l->t = BC_LEX_STR;
	bc_vec_popAll(&l->str);

	do
	{
		depth = 1;
		nls = 0;
		got_more = false;

		assert(l->mode != BC_MODE_STDIN || l->buf == vm->buffer.v);

		// This is the meat. As long as we don't run into the NUL byte, and we
		// have "depth", which means we haven't completely balanced brackets
		// yet, we continue eating the string.
		for (i = l->i; (c = l->buf[i]) && depth; ++i)
		{
			// Check for escaped brackets and set the depths as appropriate.
			if (c == '\\')
			{
				c = l->buf[++i];
				if (!c) break;
			}
			else
			{
				depth += (c == '[');
				depth -= (c == ']');
			}

			// We want to adjust the line in the lexer as necessary.
			nls += (c == '\n');

			if (depth) bc_vec_push(&l->str, &c);
		}

		if (BC_ERR(c == '\0' && depth))
		{
			if (!vm->eof && l->mode != BC_MODE_FILE)
			{
				got_more = bc_lex_readLine(l);
			}

			if (got_more)
			{
				bc_vec_popAll(&l->str);
			}
		}
	}
	while (got_more && depth);

	// Obviously, if we didn't balance, that's an error.
	if (BC_ERR(c == '\0' && depth))
	{
		l->i = i;
		bc_lex_err(l, BC_ERR_PARSE_STRING);
	}

	bc_vec_pushByte(&l->str, '\0');

	l->i = i;
	l->line += nls;
}

/**
 * Lexes a dc token. This is the dc implementation of BcLexNext.
 * @param l  The lexer.
 */
void
dc_lex_token(BcLex* l)
{
	char c = l->buf[l->i++], c2;
	size_t i;

	BC_SIG_ASSERT_LOCKED;

	// If the last token was a command that needs a register, we need to parse a
	// register, so do so.
	for (i = 0; i < dc_lex_regs_len; ++i)
	{
		// If the token is a register token, take care of it and return.
		if (l->last == dc_lex_regs[i])
		{
			dc_lex_register(l);
			return;
		}
	}

	// These lines are for tokens that easily correspond to one character. We
	// just set the token.
	if (c >= '"' && c <= '~' &&
	    (l->t = dc_lex_tokens[(c - '"')]) != BC_LEX_INVALID)
	{
		return;
	}

	// This is the workhorse of the lexer when more complicated things are
	// needed.
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

		// We don't have the ! command, so we always expect certain things
		// after the exclamation point.
		case '!':
		{
			c2 = l->buf[l->i];

			if (c2 == '=') l->t = BC_LEX_OP_REL_NE;
			else if (c2 == '<') l->t = BC_LEX_OP_REL_LE;
			else if (c2 == '>') l->t = BC_LEX_OP_REL_GE;
			else bc_lex_invalidChar(l, c);

			l->i += 1;

			break;
		}

		case '#':
		{
			bc_lex_lineComment(l);
			break;
		}

		case '.':
		{
			c2 = l->buf[l->i];

			// If the character after is a number, this dot is part of a number.
			// Otherwise, it's the BSD dot (equivalent to last).
			if (BC_NO_ERR(BC_LEX_NUM_CHAR(c2, true, false)))
			{
				bc_lex_number(l, c);
			}
			else bc_lex_invalidChar(l, c);

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
		{
			bc_lex_number(l, c);
			break;
		}

		case 'g':
		{
			c2 = l->buf[l->i];

			if (c2 == 'l') l->t = BC_LEX_KW_LINE_LENGTH;
			else if (c2 == 'z') l->t = BC_LEX_KW_LEADING_ZERO;
			else bc_lex_invalidChar(l, c2);

			l->i += 1;

			break;
		}

		case '[':
		{
			dc_lex_string(l);
			break;
		}

		default:
		{
			bc_lex_invalidChar(l, c);
		}
	}
}
#endif // DC_ENABLED
