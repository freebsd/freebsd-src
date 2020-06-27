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
 * The lexer for dc.
 *
 */

#if DC_ENABLED

#include <ctype.h>

#include <status.h>
#include <lex.h>
#include <dc.h>
#include <vm.h>

bool dc_lex_negCommand(BcLex *l) {
	char c = l->buf[l->i];
	return !BC_LEX_NUM_CHAR(c, false, false);
}

static void dc_lex_register(BcLex *l) {

	if (DC_X && isspace(l->buf[l->i - 1])) {

		char c;

		bc_lex_whitespace(l);
		c = l->buf[l->i];

		if (!isalnum(c) && c != '_')
			bc_lex_verr(l, BC_ERROR_PARSE_CHAR, c);

		l->i += 1;
		bc_lex_name(l);
	}
	else {
		bc_vec_npop(&l->str, l->str.len);
		bc_vec_pushByte(&l->str, (uchar) l->buf[l->i - 1]);
		bc_vec_pushByte(&l->str, '\0');
		l->t = BC_LEX_NAME;
	}
}

static void dc_lex_string(BcLex *l) {

	size_t depth = 1, nls = 0, i = l->i;
	char c;

	l->t = BC_LEX_STR;
	bc_vec_npop(&l->str, l->str.len);

	for (; (c = l->buf[i]) && depth; ++i) {

		if (c == '\\') {
			c = l->buf[++i];
			if (!c) break;
		}
		else {
			depth += (c == '[');
			depth -= (c == ']');
		}

		nls += (c == '\n');

		if (depth) bc_vec_push(&l->str, &c);
	}

	if (BC_ERR(c == '\0' && depth)) {
		l->i = i;
		bc_lex_err(l, BC_ERROR_PARSE_STRING);
	}

	bc_vec_pushByte(&l->str, '\0');

	l->i = i;
	l->line += nls;
}

void dc_lex_token(BcLex *l) {

	char c = l->buf[l->i++], c2;
	size_t i;

	for (i = 0; i < dc_lex_regs_len; ++i) {
		if (l->last == dc_lex_regs[i]) {
			dc_lex_register(l);
			return;
		}
	}

	if (c >= '"' && c <= '~' &&
	    (l->t = dc_lex_tokens[(c - '"')]) != BC_LEX_INVALID)
	{
		return;
	}

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
			if (BC_NO_ERR(BC_LEX_NUM_CHAR(c2, true, false)))
				bc_lex_number(l, c);
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
