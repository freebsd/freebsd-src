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
 * Definitions for bc only.
 *
 */

#ifndef BC_BC_H
#define BC_BC_H

#if BC_ENABLED

#include <limits.h>
#include <stdbool.h>

#include <status.h>
#include <lex.h>
#include <parse.h>

/**
 * The main function for bc. It just sets variables and passes its arguments
 * through to @a bc_vm_boot().
 */
void
bc_main(int argc, char* argv[]);

// These are references to the help text, the library text, and the "filename"
// for the library.
extern const char bc_help[];
extern const char bc_lib[];
extern const char* bc_lib_name;

// These are references to the second math library and its "filename."
#if BC_ENABLE_EXTRA_MATH
extern const char bc_lib2[];
extern const char* bc_lib2_name;
#endif // BC_ENABLE_EXTRA_MATH

/**
 * A struct containing information about a bc keyword.
 */
typedef struct BcLexKeyword
{
	/// Holds the length of the keyword along with a bit that, if set, means the
	/// keyword is used in POSIX bc.
	uchar data;

	/// The keyword text.
	const char name[14];
} BcLexKeyword;

/// Sets the most significant bit. Used for setting the POSIX bit in
/// BcLexKeyword's data field.
#define BC_LEX_CHAR_MSB(bit) ((bit) << (CHAR_BIT - 1))

/// Returns non-zero if the keyword is POSIX, zero otherwise.
#define BC_LEX_KW_POSIX(kw) ((kw)->data & (BC_LEX_CHAR_MSB(1)))

/// Returns the length of the keyword.
#define BC_LEX_KW_LEN(kw) ((size_t) ((kw)->data & ~(BC_LEX_CHAR_MSB(1))))

/// A macro to easily build a keyword entry. See bc_lex_kws in src/data.c.
#define BC_LEX_KW_ENTRY(a, b, c)                                              \
	{                                                                         \
		.data = ((b) & ~(BC_LEX_CHAR_MSB(1))) | BC_LEX_CHAR_MSB(c), .name = a \
	}

#if BC_ENABLE_EXTRA_MATH

/// A macro for the number of keywords bc has. This has to be updated if any are
/// added. This is for the redefined_kws field of the BcVm struct.
#define BC_LEX_NKWS (35)

#else // BC_ENABLE_EXTRA_MATH

/// A macro for the number of keywords bc has. This has to be updated if any are
/// added. This is for the redefined_kws field of the BcVm struct.
#define BC_LEX_NKWS (31)

#endif // BC_ENABLE_EXTRA_MATH

// The array of keywords and its length.
extern const BcLexKeyword bc_lex_kws[];
extern const size_t bc_lex_kws_len;

/**
 * The @a BcLexNext function for bc. (See include/lex.h for a definition of
 * @a BcLexNext.)
 * @param l  The lexer.
 */
void
bc_lex_token(BcLex* l);

// The following section is for flags needed when parsing bc code. These flags
// are complicated, but necessary. Why you ask? Because bc's standard is awful.
//
// If you don't believe me, go read the bc Parsing section of the Development
// manual (manuals/development.md). Then come back.
//
// In other words, these flags are the sign declaring, "Here be dragons."

/**
 * This returns a pointer to the set of flags at the top of the flag stack.
 * @a p is expected to be a BcParse pointer.
 * @param p  The parser.
 * @return   A pointer to the top flag set.
 */
#define BC_PARSE_TOP_FLAG_PTR(p) ((uint16_t*) bc_vec_top(&(p)->flags))

/**
 * This returns the flag set at the top of the flag stack. @a p is expected to
 * be a BcParse pointer.
 * @param p  The parser.
 * @return   The top flag set.
 */
#define BC_PARSE_TOP_FLAG(p) (*(BC_PARSE_TOP_FLAG_PTR(p)))

// After this point, all flag #defines are in sets of 2: one to define the flag,
// and one to define a way to grab the flag from the flag set at the top of the
// flag stack. All `p` arguments are pointers to a BcParse.

// This flag is set if the parser has seen a left brace.
#define BC_PARSE_FLAG_BRACE (UINTMAX_C(1) << 0)
#define BC_PARSE_BRACE(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_BRACE)

// This flag is set if the parser is parsing inside of the braces of a function
// body.
#define BC_PARSE_FLAG_FUNC_INNER (UINTMAX_C(1) << 1)
#define BC_PARSE_FUNC_INNER(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_FUNC_INNER)

// This flag is set if the parser is parsing a function. It is different from
// the one above because it is set if it is parsing a function body *or* header,
// not just if it's parsing a function body.
#define BC_PARSE_FLAG_FUNC (UINTMAX_C(1) << 2)
#define BC_PARSE_FUNC(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_FUNC)

// This flag is set if the parser is expecting to parse a body, whether of a
// function, an if statement, or a loop.
#define BC_PARSE_FLAG_BODY (UINTMAX_C(1) << 3)
#define BC_PARSE_BODY(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_BODY)

// This flag is set if bc is parsing a loop. This is important because the break
// and continue keywords are only valid inside of a loop.
#define BC_PARSE_FLAG_LOOP (UINTMAX_C(1) << 4)
#define BC_PARSE_LOOP(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_LOOP)

// This flag is set if bc is parsing the body of a loop. It is different from
// the one above the same way @a BC_PARSE_FLAG_FUNC_INNER is different from
// @a BC_PARSE_FLAG_FUNC.
#define BC_PARSE_FLAG_LOOP_INNER (UINTMAX_C(1) << 5)
#define BC_PARSE_LOOP_INNER(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_LOOP_INNER)

// This flag is set if bc is parsing an if statement.
#define BC_PARSE_FLAG_IF (UINTMAX_C(1) << 6)
#define BC_PARSE_IF(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_IF)

// This flag is set if bc is parsing an else statement. This is important
// because of "else if" constructions, among other things.
#define BC_PARSE_FLAG_ELSE (UINTMAX_C(1) << 7)
#define BC_PARSE_ELSE(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_ELSE)

// This flag is set if bc just finished parsing an if statement and its body.
// It tells the parser that it can probably expect an else statement next. This
// flag is, thus, one of the most subtle.
#define BC_PARSE_FLAG_IF_END (UINTMAX_C(1) << 8)
#define BC_PARSE_IF_END(p) (BC_PARSE_TOP_FLAG(p) & BC_PARSE_FLAG_IF_END)

/**
 * This returns true if bc is in a state where it should not execute any code
 * at all.
 * @param p  The parser.
 * @return   True if execution cannot proceed, false otherwise.
 */
#define BC_PARSE_NO_EXEC(p) ((p)->flags.len != 1 || BC_PARSE_TOP_FLAG(p) != 0)

/**
 * This returns true if the token @a t is a statement delimiter, which is
 * either a newline or a semicolon.
 * @param t  The token to check.
 * @return   True if t is a statement delimiter token; false otherwise.
 */
#define BC_PARSE_DELIMITER(t) \
	((t) == BC_LEX_SCOLON || (t) == BC_LEX_NLINE || (t) == BC_LEX_EOF)

/**
 * This is poorly named, but it basically returns whether or not the current
 * state is valid for the end of an else statement.
 * @param f  The flag set to be checked.
 * @return   True if the state is valid for the end of an else statement.
 */
#define BC_PARSE_BLOCK_STMT(f) \
	((f) & (BC_PARSE_FLAG_ELSE | BC_PARSE_FLAG_LOOP_INNER))

/**
 * This returns the value of the data for an operator with precedence @a p and
 * associativity @a l (true if left associative, false otherwise). This is used
 * to construct an array of operators, bc_parse_ops, in src/data.c.
 * @param p  The precedence.
 * @param l  True if the operator is left associative, false otherwise.
 * @return   The data for the operator.
 */
#define BC_PARSE_OP(p, l) (((p) & ~(BC_LEX_CHAR_MSB(1))) | (BC_LEX_CHAR_MSB(l)))

/**
 * Returns the operator data for the lex token @a t.
 * @param t  The token to return operator data for.
 * @return   The operator data for @a t.
 */
#define BC_PARSE_OP_DATA(t) bc_parse_ops[((t) -BC_LEX_OP_INC)]

/**
 * Returns non-zero if operator @a op is left associative, zero otherwise.
 * @param op  The operator to test for associativity.
 * @return    Non-zero if the operator is left associative, zero otherwise.
 */
#define BC_PARSE_OP_LEFT(op) (BC_PARSE_OP_DATA(op) & BC_LEX_CHAR_MSB(1))

/**
 * Returns the precedence of operator @a op. Lower number means higher
 * precedence.
 * @param op  The operator to return the precedence of.
 * @return    The precedence of @a op.
 */
#define BC_PARSE_OP_PREC(op) (BC_PARSE_OP_DATA(op) & ~(BC_LEX_CHAR_MSB(1)))

/**
 * A macro to easily define a series of bits for whether a lex token is an
 * expression token or not. It takes 8 expression bits, corresponding to the 8
 * bits in a uint8_t. You can see this in use for bc_parse_exprs in src/data.c.
 * @param e1  The first bit.
 * @param e2  The second bit.
 * @param e3  The third bit.
 * @param e4  The fourth bit.
 * @param e5  The fifth bit.
 * @param e6  The sixth bit.
 * @param e7  The seventh bit.
 * @param e8  The eighth bit.
 * @return    An expression entry for bc_parse_exprs[].
 */
#define BC_PARSE_EXPR_ENTRY(e1, e2, e3, e4, e5, e6, e7, e8)               \
	((UINTMAX_C(e1) << 7) | (UINTMAX_C(e2) << 6) | (UINTMAX_C(e3) << 5) | \
	 (UINTMAX_C(e4) << 4) | (UINTMAX_C(e5) << 3) | (UINTMAX_C(e6) << 2) | \
	 (UINTMAX_C(e7) << 1) | (UINTMAX_C(e8) << 0))

/**
 * Returns true if token @a i is a token that belongs in an expression.
 * @param i  The token to test.
 * @return   True if i is an expression token, false otherwise.
 */
#define BC_PARSE_EXPR(i) \
	(bc_parse_exprs[(((i) & (uchar) ~(0x07)) >> 3)] & (1 << (7 - ((i) &0x07))))

/**
 * Returns the operator (by lex token) that is at the top of the operator
 * stack.
 * @param p  The parser.
 * @return   The operator that is at the top of the operator stack, as a lex
 *           token.
 */
#define BC_PARSE_TOP_OP(p) (*((BcLexType*) bc_vec_top(&(p)->ops)))

/**
 * Returns true if bc has a "leaf" token. A "leaf" token is one that can stand
 * alone in an expression. For example, a number by itself can be an expression,
 * but a binary operator, while valid for an expression, cannot be alone in the
 * expression. It must have an expression to the left and right of itself. See
 * the documentation for @a bc_parse_expr_err() in src/bc_parse.c.
 * @param prev      The previous token as an instruction.
 * @param bin_last  True if that last operator was a binary operator, false
 *                  otherwise.
 * @param rparen    True if the last operator was a right paren.
 * return           True if the last token was a leaf token, false otherwise.
 */
#define BC_PARSE_LEAF(prev, bin_last, rparen) \
	(!(bin_last) && ((rparen) || bc_parse_inst_isLeaf(prev)))

/**
 * This returns true if the token @a t should be treated as though it's a
 * variable. This goes for actual variables, array elements, and globals.
 * @param t  The token to test.
 * @return   True if @a t should be treated as though it's a variable, false
 *           otherwise.
 */
#if BC_ENABLE_EXTRA_MATH
#define BC_PARSE_INST_VAR(t) \
	((t) >= BC_INST_VAR && (t) <= BC_INST_SEED && (t) != BC_INST_ARRAY)
#else // BC_ENABLE_EXTRA_MATH
#define BC_PARSE_INST_VAR(t) \
	((t) >= BC_INST_VAR && (t) <= BC_INST_SCALE && (t) != BC_INST_ARRAY)
#endif // BC_ENABLE_EXTRA_MATH

/**
 * Returns true if the previous token @a p (in the form of a bytecode
 * instruction) is a prefix operator. The fact that it is for bytecode
 * instructions is what makes it different from @a BC_PARSE_OP_PREFIX below.
 * @param p  The previous token.
 * @return   True if @a p is a prefix operator.
 */
#define BC_PARSE_PREV_PREFIX(p) ((p) >= BC_INST_NEG && (p) <= BC_INST_BOOL_NOT)

/**
 * Returns true if token @a t is a prefix operator.
 * @param t  The token to test.
 * @return   True if @a t is a prefix operator, false otherwise.
 */
#define BC_PARSE_OP_PREFIX(t) ((t) == BC_LEX_OP_BOOL_NOT || (t) == BC_LEX_NEG)

/**
 * We can calculate the conversion between tokens and bytecode instructions by
 * subtracting the position of the first operator in the lex enum and adding the
 * position of the first in the instruction enum. Note: This only works for
 * binary operators.
 * @param t  The token to turn into an instruction.
 * @return   The token as an instruction.
 */
#define BC_PARSE_TOKEN_INST(t) ((uchar) ((t) -BC_LEX_NEG + BC_INST_NEG))

/**
 * Returns true if the token is a bc keyword.
 * @param t  The token to check.
 * @return   True if @a t is a bc keyword, false otherwise.
 */
#define BC_PARSE_IS_KEYWORD(t) ((t) >= BC_LEX_KW_AUTO && (t) <= BC_LEX_KW_ELSE)

/// A struct that holds data about what tokens should be expected next. There
/// are a few instances of these, all named because they are used in specific
/// cases. Basically, in certain situations, it's useful to use the same code,
/// but have a list of valid tokens.
///
/// Obviously, @a len is the number of tokens in the @a tokens array. If more
/// than 4 is needed in the future, @a tokens will have to be changed.
typedef struct BcParseNext
{
	/// The number of tokens in the tokens array.
	uchar len;

	/// The tokens that can be expected next.
	uchar tokens[4];

} BcParseNext;

/// A macro to construct an array literal of tokens from a parameter list.
#define BC_PARSE_NEXT_TOKENS(...) .tokens = { __VA_ARGS__ }

/// A macro to generate a BcParseNext literal from BcParseNext data. See
/// src/data.c for examples.
#define BC_PARSE_NEXT(a, ...)                                 \
	{                                                         \
		.len = (uchar) (a), BC_PARSE_NEXT_TOKENS(__VA_ARGS__) \
	}

/// A status returned by @a bc_parse_expr_err(). It can either return success or
/// an error indicating an empty expression.
typedef enum BcParseStatus
{
	BC_PARSE_STATUS_SUCCESS,
	BC_PARSE_STATUS_EMPTY_EXPR,

} BcParseStatus;

/**
 * The @a BcParseExpr function for bc. (See include/parse.h for a definition of
 * @a BcParseExpr.)
 * @param p      The parser.
 * @param flags  Flags that define the requirements that the parsed code must
 *               meet or an error will result. See @a BcParseExpr for more info.
 */
void
bc_parse_expr(BcParse* p, uint8_t flags);

/**
 * The @a BcParseParse function for bc. (See include/parse.h for a definition of
 * @a BcParseParse.)
 * @param p  The parser.
 */
void
bc_parse_parse(BcParse* p);

/**
 * Ends a series of if statements. This is to ensure that full parses happen
 * when a file finishes or before defining a function. Without this, bc thinks
 * that it cannot parse any further. But if we reach the end of a file or a
 * function definition, we know we can add an empty else clause.
 * @param p  The parser.
 */
void
bc_parse_endif(BcParse* p);

/// References to the signal message and its length.
extern const char bc_sig_msg[];
extern const uchar bc_sig_msg_len;

/// A reference to an array of bits that are set if the corresponding lex token
/// is valid in an expression.
extern const uint8_t bc_parse_exprs[];

/// A reference to an array of bc operators.
extern const uchar bc_parse_ops[];

// References to the various instances of BcParseNext's.

/// A reference to what tokens are valid as next tokens when parsing normal
/// expressions. More accurately. these are the tokens that are valid for
/// *ending* the expression.
extern const BcParseNext bc_parse_next_expr;

/// A reference to what tokens are valid as next tokens when parsing function
/// parameters (well, actually arguments).
extern const BcParseNext bc_parse_next_arg;

/// A reference to what tokens are valid as next tokens when parsing a print
/// statement.
extern const BcParseNext bc_parse_next_print;

/// A reference to what tokens are valid as next tokens when parsing things like
/// loop headers and builtin functions where the only thing expected is a right
/// paren.
///
/// The name is an artifact of history, and is related to @a BC_PARSE_REL (see
/// include/parse.h). It refers to how POSIX only allows some operators as part
/// of the conditional of for loops, while loops, and if statements.
extern const BcParseNext bc_parse_next_rel;

// What tokens are valid as next tokens when parsing an array element
// expression.
extern const BcParseNext bc_parse_next_elem;

/// A reference to what tokens are valid as next tokens when parsing the first
/// two parts of a for loop header.
extern const BcParseNext bc_parse_next_for;

/// A reference to what tokens are valid as next tokens when parsing a read
/// expression.
extern const BcParseNext bc_parse_next_read;

/// A reference to what tokens are valid as next tokens when parsing a builtin
/// function with multiple arguments.
extern const BcParseNext bc_parse_next_builtin;

#else // BC_ENABLED

// If bc is not enabled, execution is always possible because dc has strict
// rules that ensure execution can always proceed safely.
#define BC_PARSE_NO_EXEC(p) (0)

#endif // BC_ENABLED

#endif // BC_BC_H
