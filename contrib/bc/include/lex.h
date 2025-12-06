/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2025 Gavin D. Howard and contributors.
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

/**
 * A convenience macro for throwing errors in lex code. This takes care of
 * plumbing like passing in the current line the lexer is on.
 * @param l  The lexer.
 * @param e  The error.
 */
#if BC_DEBUG
#define bc_lex_err(l, e) (bc_vm_handleError((e), __FILE__, __LINE__, (l)->line))
#else // BC_DEBUG
#define bc_lex_err(l, e) (bc_vm_handleError((e), (l)->line))
#endif // BC_DEBUG

/**
 * A convenience macro for throwing errors in lex code. This takes care of
 * plumbing like passing in the current line the lexer is on.
 * @param l  The lexer.
 * @param e  The error.
 */
#if BC_DEBUG
#define bc_lex_verr(l, e, ...) \
	(bc_vm_handleError((e), __FILE__, __LINE__, (l)->line, __VA_ARGS__))
#else // BC_DEBUG
#define bc_lex_verr(l, e, ...) (bc_vm_handleError((e), (l)->line, __VA_ARGS__))
#endif // BC_DEBUG

// BC_LEX_NEG_CHAR returns the char that corresponds to negative for the
// current calculator.
//
// BC_LEX_LAST_NUM_CHAR returns the char that corresponds to the last valid
// char for numbers. In bc and dc, capital letters are part of numbers, to a
// point. (dc only goes up to hex, so its last valid char is 'F'.)
#if BC_ENABLED

#if DC_ENABLED
#define BC_LEX_NEG_CHAR (BC_IS_BC ? '-' : '_')
#define BC_LEX_LAST_NUM_CHAR (BC_IS_BC ? 'Z' : 'F')
#else // DC_ENABLED
#define BC_LEX_NEG_CHAR ('-')
#define BC_LEX_LAST_NUM_CHAR ('Z')
#endif // DC_ENABLED

#else // BC_ENABLED

#define BC_LEX_NEG_CHAR ('_')
#define BC_LEX_LAST_NUM_CHAR ('F')

#endif // BC_ENABLED

/**
 * Returns true if c is a valid number character.
 * @param c         The char to check.
 * @param pt        If a decimal point has already been seen.
 * @param int_only  True if the number is expected to be an int only, false if
 *                  non-integers are allowed.
 * @return          True if @a c is a valid number character.
 */
#define BC_LEX_NUM_CHAR(c, pt, int_only)                               \
	(isdigit(c) != 0 || ((c) >= 'A' && (c) <= BC_LEX_LAST_NUM_CHAR) || \
	 ((c) == '.' && !(pt) && !(int_only)))

/// An enum of lex token types.
typedef enum BcLexType
{
	/// End of file.
	BC_LEX_EOF,

	/// Marker for invalid tokens, used by bc and dc for const data.
	BC_LEX_INVALID,

#if BC_ENABLED

	/// Increment operator.
	BC_LEX_OP_INC,

	/// Decrement operator.
	BC_LEX_OP_DEC,

#endif // BC_ENABLED

	/// BC_LEX_NEG is not used in lexing; it is only for parsing. The lexer
	/// marks all '-' characters as BC_LEX_OP_MINUS, but the parser needs to be
	/// able to distinguish them.
	BC_LEX_NEG,

	/// Boolean not.
	BC_LEX_OP_BOOL_NOT,

#if BC_ENABLE_EXTRA_MATH

	/// Truncation operator.
	BC_LEX_OP_TRUNC,

#endif // BC_ENABLE_EXTRA_MATH

	/// Power operator.
	BC_LEX_OP_POWER,

	/// Multiplication operator.
	BC_LEX_OP_MULTIPLY,

	/// Division operator.
	BC_LEX_OP_DIVIDE,

	/// Modulus operator.
	BC_LEX_OP_MODULUS,

	/// Addition operator.
	BC_LEX_OP_PLUS,

	/// Subtraction operator.
	BC_LEX_OP_MINUS,

#if BC_ENABLE_EXTRA_MATH

	/// Places (truncate or extend) operator.
	BC_LEX_OP_PLACES,

	/// Left (decimal) shift operator.
	BC_LEX_OP_LSHIFT,

	/// Right (decimal) shift operator.
	BC_LEX_OP_RSHIFT,

#endif // BC_ENABLE_EXTRA_MATH

	/// Equal operator.
	BC_LEX_OP_REL_EQ,

	/// Less than or equal operator.
	BC_LEX_OP_REL_LE,

	/// Greater than or equal operator.
	BC_LEX_OP_REL_GE,

	/// Not equal operator.
	BC_LEX_OP_REL_NE,

	/// Less than operator.
	BC_LEX_OP_REL_LT,

	/// Greater than operator.
	BC_LEX_OP_REL_GT,

	/// Boolean or operator.
	BC_LEX_OP_BOOL_OR,

	/// Boolean and operator.
	BC_LEX_OP_BOOL_AND,

#if BC_ENABLED

	/// Power assignment operator.
	BC_LEX_OP_ASSIGN_POWER,

	/// Multiplication assignment operator.
	BC_LEX_OP_ASSIGN_MULTIPLY,

	/// Division assignment operator.
	BC_LEX_OP_ASSIGN_DIVIDE,

	/// Modulus assignment operator.
	BC_LEX_OP_ASSIGN_MODULUS,

	/// Addition assignment operator.
	BC_LEX_OP_ASSIGN_PLUS,

	/// Subtraction assignment operator.
	BC_LEX_OP_ASSIGN_MINUS,

#if BC_ENABLE_EXTRA_MATH

	/// Places (truncate or extend) assignment operator.
	BC_LEX_OP_ASSIGN_PLACES,

	/// Left (decimal) shift assignment operator.
	BC_LEX_OP_ASSIGN_LSHIFT,

	/// Right (decimal) shift assignment operator.
	BC_LEX_OP_ASSIGN_RSHIFT,

#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED

	/// Assignment operator.
	BC_LEX_OP_ASSIGN,

	/// Newline.
	BC_LEX_NLINE,

	/// Whitespace.
	BC_LEX_WHITESPACE,

	/// Left parenthesis.
	BC_LEX_LPAREN,

	/// Right parenthesis.
	BC_LEX_RPAREN,

	/// Left bracket.
	BC_LEX_LBRACKET,

	/// Comma.
	BC_LEX_COMMA,

	/// Right bracket.
	BC_LEX_RBRACKET,

	/// Left brace.
	BC_LEX_LBRACE,

	/// Semicolon.
	BC_LEX_SCOLON,

	/// Right brace.
	BC_LEX_RBRACE,

	/// String.
	BC_LEX_STR,

	/// Identifier/name.
	BC_LEX_NAME,

	/// Constant number.
	BC_LEX_NUMBER,

	// These keywords are in the order they are in for a reason. Don't change
	// the order unless you want a bunch of weird failures in the test suite.
	// In fact, almost all of these tokens are in a specific order for a reason.

#if BC_ENABLED

	/// bc auto keyword.
	BC_LEX_KW_AUTO,

	/// bc break keyword.
	BC_LEX_KW_BREAK,

	/// bc continue keyword.
	BC_LEX_KW_CONTINUE,

	/// bc define keyword.
	BC_LEX_KW_DEFINE,

	/// bc for keyword.
	BC_LEX_KW_FOR,

	/// bc if keyword.
	BC_LEX_KW_IF,

	/// bc limits keyword.
	BC_LEX_KW_LIMITS,

	/// bc return keyword.
	BC_LEX_KW_RETURN,

	/// bc while keyword.
	BC_LEX_KW_WHILE,

	/// bc halt keyword.
	BC_LEX_KW_HALT,

	/// bc last keyword.
	BC_LEX_KW_LAST,

#endif // BC_ENABLED

	/// bc ibase keyword.
	BC_LEX_KW_IBASE,

	/// bc obase keyword.
	BC_LEX_KW_OBASE,

	/// bc scale keyword.
	BC_LEX_KW_SCALE,

#if BC_ENABLE_EXTRA_MATH

	/// bc seed keyword.
	BC_LEX_KW_SEED,

#endif // BC_ENABLE_EXTRA_MATH

	/// bc length keyword.
	BC_LEX_KW_LENGTH,

	/// bc print keyword.
	BC_LEX_KW_PRINT,

	/// bc sqrt keyword.
	BC_LEX_KW_SQRT,

	/// bc abs keyword.
	BC_LEX_KW_ABS,

	/// bc is_number keyword.
	BC_LEX_KW_IS_NUMBER,

	/// bc is_string keyword.
	BC_LEX_KW_IS_STRING,

#if BC_ENABLE_EXTRA_MATH

	/// bc irand keyword.
	BC_LEX_KW_IRAND,

#endif // BC_ENABLE_EXTRA_MATH

	/// bc asciffy keyword.
	BC_LEX_KW_ASCIIFY,

	/// bc modexp keyword.
	BC_LEX_KW_MODEXP,

	/// bc divmod keyword.
	BC_LEX_KW_DIVMOD,

	/// bc quit keyword.
	BC_LEX_KW_QUIT,

	/// bc read keyword.
	BC_LEX_KW_READ,

#if BC_ENABLE_EXTRA_MATH

	/// bc rand keyword.
	BC_LEX_KW_RAND,

#endif // BC_ENABLE_EXTRA_MATH

	/// bc maxibase keyword.
	BC_LEX_KW_MAXIBASE,

	/// bc maxobase keyword.
	BC_LEX_KW_MAXOBASE,

	/// bc maxscale keyword.
	BC_LEX_KW_MAXSCALE,

#if BC_ENABLE_EXTRA_MATH

	/// bc maxrand keyword.
	BC_LEX_KW_MAXRAND,

#endif // BC_ENABLE_EXTRA_MATH

	/// bc line_length keyword.
	BC_LEX_KW_LINE_LENGTH,

#if BC_ENABLED

	/// bc global_stacks keyword.
	BC_LEX_KW_GLOBAL_STACKS,

#endif // BC_ENABLED

	/// bc leading_zero keyword.
	BC_LEX_KW_LEADING_ZERO,

	/// bc stream keyword.
	BC_LEX_KW_STREAM,

	/// bc else keyword.
	BC_LEX_KW_ELSE,

#if DC_ENABLED

	/// dc extended registers keyword.
	BC_LEX_EXTENDED_REGISTERS,

	/// A special token for dc to calculate equal without a register.
	BC_LEX_EQ_NO_REG,

	/// Colon (array) operator.
	BC_LEX_COLON,

	/// Execute command.
	BC_LEX_EXECUTE,

	/// Print stack command.
	BC_LEX_PRINT_STACK,

	/// Clear stack command.
	BC_LEX_CLEAR_STACK,

	/// Register stack level command.
	BC_LEX_REG_STACK_LEVEL,

	/// Main stack level command.
	BC_LEX_STACK_LEVEL,

	/// Duplicate command.
	BC_LEX_DUPLICATE,

	/// Swap (reverse) command.
	BC_LEX_SWAP,

	/// Pop (remove) command.
	BC_LEX_POP,

	/// Store ibase command.
	BC_LEX_STORE_IBASE,

	/// Store obase command.
	BC_LEX_STORE_OBASE,

	/// Store scale command.
	BC_LEX_STORE_SCALE,

#if BC_ENABLE_EXTRA_MATH

	/// Store seed command.
	BC_LEX_STORE_SEED,

#endif // BC_ENABLE_EXTRA_MATH

	/// Load variable onto stack command.
	BC_LEX_LOAD,

	/// Pop off of variable stack onto results stack command.
	BC_LEX_LOAD_POP,

	/// Push onto variable stack command.
	BC_LEX_STORE_PUSH,

	/// Print with pop command.
	BC_LEX_PRINT_POP,

	/// Parameterized quit command.
	BC_LEX_NQUIT,

	/// Execution stack depth command.
	BC_LEX_EXEC_STACK_LENGTH,

	/// Scale of number command. This is needed specifically for dc because bc
	/// parses the scale function in parts.
	BC_LEX_SCALE_FACTOR,

	/// Array length command. This is needed specifically for dc because bc
	/// just reuses its length keyword.
	BC_LEX_ARRAY_LENGTH,

#endif // DC_ENABLED

} BcLexType;

struct BcLex;

/**
 * A function pointer to call when another token is needed. Mostly called by the
 * parser.
 * @param l  The lexer.
 */
typedef void (*BcLexNext)(struct BcLex* l);

/// The lexer.
typedef struct BcLex
{
	/// A pointer to the text to lex.
	const char* buf;

	/// The current index into buf.
	size_t i;

	/// The current line.
	size_t line;

	/// The length of buf.
	size_t len;

	/// The current token.
	BcLexType t;

	/// The previous token.
	BcLexType last;

	/// A string to store extra data for tokens. For example, the @a BC_LEX_STR
	/// token really needs to store the actual string, and numbers also need the
	/// string.
	BcVec str;

	/// The mode the lexer is in.
	BcMode mode;

} BcLex;

/**
 * Initializes a lexer.
 * @param l  The lexer to initialize.
 */
void
bc_lex_init(BcLex* l);

/**
 * Frees a lexer. This is not guarded by #if BC_DEBUG because a separate
 * parser is created at runtime to parse read() expressions and dc strings, and
 * that parser needs a lexer.
 * @param l  The lexer to free.
 */
void
bc_lex_free(BcLex* l);

/**
 * Sets the filename that the lexer will be lexing.
 * @param l     The lexer.
 * @param file  The filename that the lexer will lex.
 */
void
bc_lex_file(BcLex* l, const char* file);

/**
 * Sets the text the lexer will lex.
 * @param l     The lexer.
 * @param text  The text to lex.
 * @param mode  The mode to lex in.
 */
void
bc_lex_text(BcLex* l, const char* text, BcMode mode);

/**
 * Generic next function for the parser to call. It takes care of calling the
 * correct @a BcLexNext function and consuming whitespace.
 * @param l  The lexer.
 */
void
bc_lex_next(BcLex* l);

/**
 * Lexes a line comment (one beginning with '#' and going to a newline).
 * @param l  The lexer.
 */
void
bc_lex_lineComment(BcLex* l);

/**
 * Lexes a general comment (C-style comment).
 * @param l  The lexer.
 */
void
bc_lex_comment(BcLex* l);

/**
 * Lexes whitespace, finding as much as possible.
 * @param l  The lexer.
 */
void
bc_lex_whitespace(BcLex* l);

/**
 * Lexes a number that begins with char @a start. This takes care of parsing
 * numbers in scientific and engineering notations.
 * @param l      The lexer.
 * @param start  The starting char of the number. To detect a number and call
 *               this function, the lexer had to eat the first char. It fixes
 *               that by passing it in.
 */
void
bc_lex_number(BcLex* l, char start);

/**
 * Lexes a name/identifier.
 * @param l  The lexer.
 */
void
bc_lex_name(BcLex* l);

/**
 * Lexes common whitespace characters.
 * @param l  The lexer.
 * @param c  The character to lex.
 */
void
bc_lex_commonTokens(BcLex* l, char c);

/**
 * Throws a parse error because char @a c was invalid.
 * @param l  The lexer.
 * @param c  The problem character.
 */
void
bc_lex_invalidChar(BcLex* l, char c);

/**
 * Reads a line from stdin and puts it into the lexer's buffer.
 * @param l  The lexer.
 */
bool
bc_lex_readLine(BcLex* l);

#endif // BC_LEX_H
