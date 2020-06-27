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
 * Definitions for bc's lexer.
 *
 */

#ifndef BC_LEX_H
#define BC_LEX_H

#include <stdbool.h>
#include <stddef.h>

#include <status.h>
#include <vector.h>
#include <lang.h>

#define bc_lex_err(l, e) (bc_vm_error((e), (l)->line))
#define bc_lex_verr(l, e, ...) (bc_vm_error((e), (l)->line, __VA_ARGS__))

#define BC_LEX_NEG_CHAR (BC_IS_BC ? '-' : '_')
#define BC_LEX_LAST_NUM_CHAR (BC_IS_BC ? 'Z' : 'F')
#define BC_LEX_NUM_CHAR(c, pt, int_only)                          \
	(isdigit(c) || ((c) >= 'A' && (c) <= BC_LEX_LAST_NUM_CHAR) || \
	 ((c) == '.' && !(pt) && !(int_only)))

// BC_LEX_NEG is not used in lexing; it is only for parsing.
typedef enum BcLexType {

	BC_LEX_EOF,
	BC_LEX_INVALID,

#if BC_ENABLED
	BC_LEX_OP_INC,
	BC_LEX_OP_DEC,
#endif // BC_ENABLED

	BC_LEX_NEG,
	BC_LEX_OP_BOOL_NOT,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_TRUNC,
#endif // BC_ENABLE_EXTRA_MATH

	BC_LEX_OP_POWER,
	BC_LEX_OP_MULTIPLY,
	BC_LEX_OP_DIVIDE,
	BC_LEX_OP_MODULUS,
	BC_LEX_OP_PLUS,
	BC_LEX_OP_MINUS,

#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_PLACES,

	BC_LEX_OP_LSHIFT,
	BC_LEX_OP_RSHIFT,
#endif // BC_ENABLE_EXTRA_MATH

	BC_LEX_OP_REL_EQ,
	BC_LEX_OP_REL_LE,
	BC_LEX_OP_REL_GE,
	BC_LEX_OP_REL_NE,
	BC_LEX_OP_REL_LT,
	BC_LEX_OP_REL_GT,

	BC_LEX_OP_BOOL_OR,
	BC_LEX_OP_BOOL_AND,

#if BC_ENABLED
	BC_LEX_OP_ASSIGN_POWER,
	BC_LEX_OP_ASSIGN_MULTIPLY,
	BC_LEX_OP_ASSIGN_DIVIDE,
	BC_LEX_OP_ASSIGN_MODULUS,
	BC_LEX_OP_ASSIGN_PLUS,
	BC_LEX_OP_ASSIGN_MINUS,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_OP_ASSIGN_PLACES,
	BC_LEX_OP_ASSIGN_LSHIFT,
	BC_LEX_OP_ASSIGN_RSHIFT,
#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED
	BC_LEX_OP_ASSIGN,

	BC_LEX_NLINE,
	BC_LEX_WHITESPACE,

	BC_LEX_LPAREN,
	BC_LEX_RPAREN,

	BC_LEX_LBRACKET,
	BC_LEX_COMMA,
	BC_LEX_RBRACKET,

	BC_LEX_LBRACE,
	BC_LEX_SCOLON,
	BC_LEX_RBRACE,

	BC_LEX_STR,
	BC_LEX_NAME,
	BC_LEX_NUMBER,

#if BC_ENABLED
	BC_LEX_KW_AUTO,
	BC_LEX_KW_BREAK,
	BC_LEX_KW_CONTINUE,
	BC_LEX_KW_DEFINE,
	BC_LEX_KW_FOR,
	BC_LEX_KW_IF,
	BC_LEX_KW_LIMITS,
	BC_LEX_KW_RETURN,
	BC_LEX_KW_WHILE,
	BC_LEX_KW_HALT,
	BC_LEX_KW_LAST,
#endif // BC_ENABLED
	BC_LEX_KW_IBASE,
	BC_LEX_KW_OBASE,
	BC_LEX_KW_SCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_SEED,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_LENGTH,
	BC_LEX_KW_PRINT,
	BC_LEX_KW_SQRT,
	BC_LEX_KW_ABS,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_IRAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_QUIT,
	BC_LEX_KW_READ,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_RAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_MAXIBASE,
	BC_LEX_KW_MAXOBASE,
	BC_LEX_KW_MAXSCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_MAXRAND,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_KW_ELSE,

#if DC_ENABLED
	BC_LEX_EQ_NO_REG,
	BC_LEX_OP_MODEXP,
	BC_LEX_OP_DIVMOD,

	BC_LEX_COLON,
	BC_LEX_EXECUTE,
	BC_LEX_PRINT_STACK,
	BC_LEX_CLEAR_STACK,
	BC_LEX_STACK_LEVEL,
	BC_LEX_DUPLICATE,
	BC_LEX_SWAP,
	BC_LEX_POP,

	BC_LEX_ASCIIFY,
	BC_LEX_PRINT_STREAM,

	BC_LEX_STORE_IBASE,
	BC_LEX_STORE_OBASE,
	BC_LEX_STORE_SCALE,
#if BC_ENABLE_EXTRA_MATH
	BC_LEX_STORE_SEED,
#endif // BC_ENABLE_EXTRA_MATH
	BC_LEX_LOAD,
	BC_LEX_LOAD_POP,
	BC_LEX_STORE_PUSH,
	BC_LEX_PRINT_POP,
	BC_LEX_NQUIT,
	BC_LEX_SCALE_FACTOR,
#endif // DC_ENABLED

} BcLexType;

struct BcLex;
typedef void (*BcLexNext)(struct BcLex*);

typedef struct BcLex {

	const char *buf;
	size_t i;
	size_t line;
	size_t len;

	BcLexType t;
	BcLexType last;
	BcVec str;

} BcLex;

void bc_lex_init(BcLex *l);
void bc_lex_free(BcLex *l);
void bc_lex_file(BcLex *l, const char *file);
void bc_lex_text(BcLex *l, const char *text);
void bc_lex_next(BcLex *l);

void bc_lex_lineComment(BcLex *l);
void bc_lex_comment(BcLex *l);
void bc_lex_whitespace(BcLex *l);
void bc_lex_number(BcLex *l, char start);
void bc_lex_name(BcLex *l);
void bc_lex_commonTokens(BcLex *l, char c);

void bc_lex_invalidChar(BcLex *l, char c);

#endif // BC_LEX_H
