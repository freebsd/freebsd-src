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
 * Definitions for bc's parser.
 *
 */

#ifndef BC_PARSE_H
#define BC_PARSE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <status.h>
#include <vector.h>
#include <lex.h>
#include <lang.h>

// The following are flags that can be passed to @a BcParseExpr functions. They
// define the requirements that the parsed expression must meet to not have an
// error thrown.

/// A flag that requires that the expression is valid for conditionals in for
/// loops, while loops, and if statements. This is because POSIX requires that
/// certain operators are *only* used in those cases. It's whacked, but that's
/// how it is.
#define BC_PARSE_REL (UINTMAX_C(1) << 0)

/// A flag that requires that the expression is valid for a print statement.
#define BC_PARSE_PRINT (UINTMAX_C(1) << 1)

/// A flag that requires that the expression does *not* have any function call.
#define BC_PARSE_NOCALL (UINTMAX_C(1) << 2)

/// A flag that requires that the expression does *not* have a read()
/// expression.
#define BC_PARSE_NOREAD (UINTMAX_C(1) << 3)

/// A flag that *allows* (rather than requires) that an array appear in the
/// expression. This is mostly used as parameters in bc.
#define BC_PARSE_ARRAY (UINTMAX_C(1) << 4)

/// A flag that requires that the expression is not empty and returns a value.
#define BC_PARSE_NEEDVAL (UINTMAX_C(1) << 5)

/**
 * Returns true if the parser has been initialized.
 * @param p    The parser.
 * @param prg  The program.
 * @return     True if @a p has been initialized, false otherwise.
 */
#define BC_PARSE_IS_INITED(p, prg) ((p)->prog == (prg))

/**
 * Returns true if the current parser state allows parsing, false otherwise.
 * @param p  The parser.
 * @return   True if parsing can proceed, false otherwise.
 */
#define BC_PARSE_CAN_PARSE(p) ((p).l.t != BC_LEX_EOF)

/**
 * Pushes the instruction @a i onto the bytecode vector for the current
 * function.
 * @param p  The parser.
 * @param i  The instruction to push onto the bytecode vector.
 */
#define bc_parse_push(p, i) (bc_vec_pushByte(&(p)->func->code, (uchar) (i)))

/**
 * Pushes an index onto the bytecode vector. For more information, see
 * @a bc_vec_pushIndex() in src/vector.c and @a bc_program_index() in
 * src/program.c.
 * @param p    The parser.
 * @param idx  The index to push onto the bytecode vector.
 */
#define bc_parse_pushIndex(p, idx) (bc_vec_pushIndex(&(p)->func->code, (idx)))

/**
 * A convenience macro for throwing errors in parse code. This takes care of
 * plumbing like passing in the current line the lexer is on.
 * @param p  The parser.
 * @param e  The error.
 */
#if BC_DEBUG
#define bc_parse_err(p, e) \
	(bc_vm_handleError((e), __FILE__, __LINE__, (p)->l.line))
#else // BC_DEBUG
#define bc_parse_err(p, e) (bc_vm_handleError((e), (p)->l.line))
#endif // BC_DEBUG

/**
 * A convenience macro for throwing errors in parse code. This takes care of
 * plumbing like passing in the current line the lexer is on.
 * @param p    The parser.
 * @param e    The error.
 * @param ...  The varags that are needed.
 */
#if BC_DEBUG
#define bc_parse_verr(p, e, ...) \
	(bc_vm_handleError((e), __FILE__, __LINE__, (p)->l.line, __VA_ARGS__))
#else // BC_DEBUG
#define bc_parse_verr(p, e, ...) \
	(bc_vm_handleError((e), (p)->l.line, __VA_ARGS__))
#endif // BC_DEBUG

// Forward declarations.
struct BcParse;
struct BcProgram;

/**
 * A function pointer to call when more parsing is needed.
 * @param p  The parser.
 */
typedef void (*BcParseParse)(struct BcParse* p);

/**
 * A function pointer to call when an expression needs to be parsed. This can
 * happen for read() expressions or dc strings.
 * @param p      The parser.
 * @param flags  The flags for what is allowed or required. (See flags above.)
 */
typedef void (*BcParseExpr)(struct BcParse* p, uint8_t flags);

/// The parser struct.
typedef struct BcParse
{
	/// The lexer.
	BcLex l;

#if BC_ENABLED
	/// The stack of flags for bc. (See comments in include/bc.h.) This stack is
	/// *required* to have one item at all times. Not maintaining that invariant
	/// will cause problems.
	BcVec flags;

	/// The stack of exits. These are indices into the bytecode vector where
	/// blocks for loops and if statements end. Basically, these are the places
	/// to jump to when skipping code.
	BcVec exits;

	/// The stack of conditionals. Unlike exits, which are indices to jump
	/// *forward* to, this is a vector of indices to jump *backward* to, usually
	/// to the conditional of a loop, hence the name.
	BcVec conds;

	/// A stack of operators. When parsing expressions, the bc parser uses the
	/// Shunting-Yard algorithm, which requires a stack of operators. This can
	/// hold the stack for multiple expressions at once because the expressions
	/// stack as well. For more information, see the Expression Parsing section
	/// of the Development manual (manuals/development.md).
	BcVec ops;

	/// A buffer to temporarily store a string in. This is because the lexer
	/// might generate a string as part of its work, and the parser needs that
	/// string, but it also needs the lexer to continue lexing, which might
	/// overwrite the string stored in the lexer. This buffer is for copying
	/// that string from the lexer to keep it safe.
	BcVec buf;
#endif // BC_ENABLED

	/// A reference to the program to grab the current function when necessary.
	struct BcProgram* prog;

	/// A reference to the current function. The function is what holds the
	/// bytecode vector that the parser is filling.
	BcFunc* func;

	/// The index of the function.
	size_t fidx;

#if BC_ENABLED
	/// True if the bc parser just entered a function and an auto statement
	/// would be valid.
	bool auto_part;
#endif // BC_ENABLED

} BcParse;

/**
 * Initializes a parser.
 * @param p     The parser to initialize.
 * @param prog  A referenc to the program.
 * @param func  The index of the current function.
 */
void
bc_parse_init(BcParse* p, struct BcProgram* prog, size_t func);

/**
 * Frees a parser. This is not guarded by #if BC_DEBUG because a separate
 * parser is created at runtime to parse read() expressions and dc strings.
 * @param p  The parser to free.
 */
void
bc_parse_free(BcParse* p);

/**
 * Resets the parser. Resetting means erasing all state to the point that the
 * parser would think it was just initialized.
 * @param p  The parser to reset.
 */
void
bc_parse_reset(BcParse* p);

/**
 * Adds a string. See @a BcProgram in include/program.h for more details.
 * @param p  The parser that parsed the string.
 */
void
bc_parse_addString(BcParse* p);

/**
 * Adds a number. See @a BcProgram in include/program.h for more details.
 * @param p  The parser that parsed the number.
 */
void
bc_parse_number(BcParse* p);

/**
 * Update the current function in the parser.
 * @param p     The parser.
 * @param fidx  The index of the new function.
 */
void
bc_parse_updateFunc(BcParse* p, size_t fidx);

/**
 * Adds a new variable or array. See @a BcProgram in include/program.h for more
 * details.
 * @param p     The parser that parsed the variable or array name.
 * @param name  The name of the variable or array to add.
 * @param var   True if the name is for a variable, false if it's for an array.
 */
void
bc_parse_pushName(const BcParse* p, char* name, bool var);

/**
 * Sets the text that the parser will parse.
 * @param p     The parser.
 * @param text  The text to lex.
 * @param mode  The mode to parse in.
 */
void
bc_parse_text(BcParse* p, const char* text, BcMode mode);

// References to const 0 and 1 strings for special cases. bc and dc have
// specific instructions for 0 and 1 because they pop up so often and (in the
// case of 1), increment/decrement operators.
extern const char bc_parse_zero[2];
extern const char bc_parse_one[2];

#endif // BC_PARSE_H
