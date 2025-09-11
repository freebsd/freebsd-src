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
 * Definitions for bc programs.
 *
 */

#ifndef BC_PROGRAM_H
#define BC_PROGRAM_H

#include <assert.h>
#include <stddef.h>

#include <status.h>
#include <parse.h>
#include <lang.h>
#include <num.h>
#include <rand.h>

/// The index of ibase in the globals array.
#define BC_PROG_GLOBALS_IBASE (0)

/// The index of obase in the globals array.
#define BC_PROG_GLOBALS_OBASE (1)

/// The index of scale in the globals array.
#define BC_PROG_GLOBALS_SCALE (2)

#if BC_ENABLE_EXTRA_MATH

/// The index of the rand max in the maxes array.
#define BC_PROG_MAX_RAND (3)

#endif // BC_ENABLE_EXTRA_MATH

/// The length of the globals array.
#define BC_PROG_GLOBALS_LEN (3 + BC_ENABLE_EXTRA_MATH)

typedef struct BcProgram
{
	/// The array of globals values.
	BcBigDig globals[BC_PROG_GLOBALS_LEN];

#if BC_ENABLED
	/// The array of globals stacks.
	BcVec globals_v[BC_PROG_GLOBALS_LEN];
#endif // BC_ENABLED

#if BC_ENABLE_EXTRA_MATH

	/// The pseudo-random number generator.
	BcRNG rng;

#endif // BC_ENABLE_EXTRA_MATH

	/// The results stack.
	BcVec results;

	/// The execution stack.
	BcVec stack;

	/// The constants encountered in the program. They are global to the program
	/// to prevent bad accesses when functions that used non-auto variables are
	/// replaced.
	BcVec consts;

	/// The map of constants to go with consts.
	BcVec const_map;

	/// The strings encountered in the program. They are global to the program
	/// to prevent bad accesses when functions that used non-auto variables are
	/// replaced.
	BcVec strs;

	/// The map of strings to go with strs.
	BcVec str_map;

	/// The array of functions.
	BcVec fns;

	/// The map of functions to go with fns.
	BcVec fn_map;

	/// The array of variables.
	BcVec vars;

	/// The map of variables to go with vars.
	BcVec var_map;

	/// The array of arrays.
	BcVec arrs;

	/// The map of arrays to go with arrs.
	BcVec arr_map;

#if DC_ENABLED

	/// A vector of tail calls. These are just integers, which are the number of
	/// tail calls that have been executed for each function (string) on the
	/// stack for dc. This is to prevent dc from constantly growing memory use
	/// because of pushing more and more string executions on the stack.
	BcVec tail_calls;

#endif // DC_ENABLED

	/// A BcNum that has the proper base for asciify.
	BcNum strmb;

	// A BcNum to run asciify. This is to prevent GCC longjmp() clobbering
	// warnings.
	BcNum asciify;

#if BC_ENABLED

	/// The last printed value for bc.
	BcNum last;

#endif // BC_ENABLED

	/// The number of results that have not been retired.
	size_t nresults;

	// The BcDig array for strmb. This uses BC_NUM_LONG_LOG10 because it is used
	// in bc_num_ulong2num(), which attempts to realloc, unless it is big
	// enough. This is big enough.
	BcDig strmb_num[BC_NUM_BIGDIG_LOG10];

} BcProgram;

/**
 * Returns true if the stack @a s has at least @a n items, false otherwise.
 * @param s  The stack to check.
 * @param n  The number of items the stack must have.
 * @return   True if @a s has at least @a n items, false otherwise.
 */
#define BC_PROG_STACK(s, n) ((s)->len >= ((size_t) (n)))

/**
 * Get a pointer to the top value in a global value stack.
 * @param v  The global value stack.
 * @return   A pointer to the top value in @a v.
 */
#define BC_PROG_GLOBAL_PTR(v) (bc_vec_top(v))

/**
 * Get the top value in a global value stack.
 * @param v  The global value stack.
 * @return   The top value in @a v.
 */
#define BC_PROG_GLOBAL(v) (*((BcBigDig*) BC_PROG_GLOBAL_PTR(v)))

/**
 * Returns the current value of ibase.
 * @param p  The program.
 * @return   The current ibase.
 */
#define BC_PROG_IBASE(p) ((p)->globals[BC_PROG_GLOBALS_IBASE])

/**
 * Returns the current value of obase.
 * @param p  The program.
 * @return   The current obase.
 */
#define BC_PROG_OBASE(p) ((p)->globals[BC_PROG_GLOBALS_OBASE])

/**
 * Returns the current value of scale.
 * @param p  The program.
 * @return   The current scale.
 */
#define BC_PROG_SCALE(p) ((p)->globals[BC_PROG_GLOBALS_SCALE])

/// The index for the main function in the functions array.//
#define BC_PROG_MAIN (0)

/// The index for the read function in the functions array.
#define BC_PROG_READ (1)

/**
 * Retires (completes the execution of) an instruction. Some instructions
 * require special retirement, but most can use this. This basically pops the
 * operands while preserving the result (which we assumed was pushed before the
 * actual operation).
 * @param p     The program.
 * @param nops  The number of operands used by the instruction.
 */
#define bc_program_retire(p, nops)                                \
	do                                                            \
	{                                                             \
		bc_vec_npopAt(&(p)->results, (nops),                      \
		              (p)->results.len - ((p)->nresults + nops)); \
		p->nresults = 0;                                          \
	}                                                             \
	while (0)

#if DC_ENABLED

/// A constant that tells how many functions are required in dc.
#define BC_PROG_REQ_FUNCS (2)

#if !BC_ENABLED

/// Returns true if the calculator should pop after printing.
#define BC_PROGRAM_POP(pop) (pop)

#else // !BC_ENABLED

/// Returns true if the calculator should pop after printing.
#define BC_PROGRAM_POP(pop) (BC_IS_BC || (pop))

#endif // !BC_ENABLED

// This is here to satisfy a clang warning about recursive macros.
#define bc_program_pushVar(p, code, bgn, pop, copy) \
	bc_program_pushVar_impl(p, code, bgn, pop, copy)

#else // DC_ENABLED

// This define disappears pop and copy because for bc, 'pop' and 'copy' are
// always false.
#define bc_program_pushVar(p, code, bgn, pop, copy) \
	bc_program_pushVar_impl(p, code, bgn)

/// Returns true if the calculator should pop after printing.
#define BC_PROGRAM_POP(pop) (BC_IS_BC)

// In debug mode, we want bc to check the stack, but otherwise, we don't because
// the bc language implicitly mandates that the stack should always have enough
// items.
#ifdef BC_DEBUG
#define BC_PROG_NO_STACK_CHECK
#endif // BC_DEBUG

#endif // DC_ENABLED

/**
 * Returns true if the BcNum @a n is acting as a string.
 * @param n  The BcNum to test.
 * @return   True if @a n is acting as a string, false otherwise.
 */
#define BC_PROG_STR(n) ((n)->num == NULL && !(n)->cap)

#if BC_ENABLED

/**
 * Returns true if the result @a r and @a n is a number.
 * @param r  The result.
 * @param n  The number corresponding to the result.
 * @return   True if the result holds a number, false otherwise.
 */
#define BC_PROG_NUM(r, n) \
	((r)->t != BC_RESULT_ARRAY && (r)->t != BC_RESULT_STR && !BC_PROG_STR(n))

#else // BC_ENABLED

/**
 * Returns true if the result @a r and @a n is a number.
 * @param r  The result.
 * @param n  The number corresponding to the result.
 * @return   True if the result holds a number, false otherwise.
 */
#define BC_PROG_NUM(r, n) ((r)->t != BC_RESULT_STR && !BC_PROG_STR(n))

#endif // BC_ENABLED

/**
 * This is a function type for unary operations. Currently, these include
 * boolean not, negation, and truncation with extra math.
 * @param r  The BcResult to store the result into.
 * @param n  The parameter to the unary operation.
 */
typedef void (*BcProgramUnary)(BcResult* r, BcNum* n);

/**
 * Initializes the BcProgram.
 * @param p  The program to initialize.
 */
void
bc_program_init(BcProgram* p);

#if BC_DEBUG

/**
 * Frees a BcProgram. This is only used in debug builds because a BcProgram is
 * only freed on program exit, and we don't care about freeing resources on
 * exit.
 * @param p  The program to initialize.
 */
void
bc_program_free(BcProgram* p);

#endif // BC_DEBUG

/**
 * Prints a stack trace of the bc functions or dc strings currently executing.
 * @param p  The program.
 */
void
bc_program_printStackTrace(BcProgram* p);

#if BC_DEBUG_CODE
#if BC_ENABLED && DC_ENABLED

/**
 * Prints the bytecode in a function. This is a debug-only function.
 * @param p  The program.
 */
void
bc_program_code(const BcProgram* p);

/**
 * Prints an instruction. This is a debug-only function.
 * @param p  The program.
 * @param code  The bytecode array.
 * @param bgn   A pointer to the current index. It is also updated to the next
 *              index.
 */
void
bc_program_printInst(const BcProgram* p, const char* code,
                     size_t* restrict bgn);

/**
 * Prints the stack. This is a debug-only function.
 * @param p  The program.
 */
void
bc_program_printStackDebug(BcProgram* p);

#endif // BC_ENABLED && DC_ENABLED
#endif // BC_DEBUG_CODE

/**
 * Returns the index of the variable or array in their respective arrays.
 * @param p     The program.
 * @param name  The name of the variable or array.
 * @param var   True if the search should be for a variable, false for an array.
 * @return      The index of the variable or array in the correct array.
 */
size_t
bc_program_search(BcProgram* p, const char* name, bool var);

/**
 * Adds a string to the program and returns the string's index in the program.
 * @param p    The program.
 * @param str  The string to add.
 * @return     The string's index in the program.
 */
size_t
bc_program_addString(BcProgram* p, const char* str);

/**
 * Inserts a function into the program and returns the index of the function in
 * the fns array.
 * @param p     The program.
 * @param name  The name of the function.
 * @return      The index of the function after insertion.
 */
size_t
bc_program_insertFunc(BcProgram* p, const char* name);

/**
 * Resets a program, usually because of resetting after an error.
 * @param p  The program to reset.
 */
void
bc_program_reset(BcProgram* p);

/**
 * Executes bc or dc code in the BcProgram.
 * @param p  The program.
 */
void
bc_program_exec(BcProgram* p);

/**
 * Negates a copy of a BcNum. This is a BcProgramUnary function.
 * @param r  The BcResult to store the result into.
 * @param n  The parameter to the unary operation.
 */
void
bc_program_negate(BcResult* r, BcNum* n);

/**
 * Returns a boolean not of a BcNum. This is a BcProgramUnary function.
 * @param r  The BcResult to store the result into.
 * @param n  The parameter to the unary operation.
 */
void
bc_program_not(BcResult* r, BcNum* n);

#if BC_ENABLE_EXTRA_MATH

/**
 * Truncates a copy of a BcNum. This is a BcProgramUnary function.
 * @param r  The BcResult to store the result into.
 * @param n  The parameter to the unary operation.
 */
void
bc_program_trunc(BcResult* r, BcNum* n);

/**
 * Assigns a value to the seed builtin variable.
 * @param p    The program.
 * @param val  The value to assign to the seed.
 */
void
bc_program_assignSeed(BcProgram* p, BcNum* val);

#endif // BC_ENABLE_EXTRA_MATH

/**
 * Assigns a value to a builtin value that is not seed.
 * @param p      The program.
 * @param scale  True if the builtin is scale.
 * @param obase  True if the builtin is obase. This cannot be true at the same
 *               time @a scale is.
 * @param val    The value to assign to the builtin.
 */
void
bc_program_assignBuiltin(BcProgram* p, bool scale, bool obase, BcBigDig val);

/// A reference to an array of binary operator functions.
extern const BcNumBinaryOp bc_program_ops[];

/// A reference to an array of binary operator allocation request functions.
extern const BcNumBinaryOpReq bc_program_opReqs[];

/// A reference to an array of unary operator functions.
extern const BcProgramUnary bc_program_unarys[];

/// A reference to a filename for command-line expressions.
extern const char bc_program_exprs_name[];

/// A reference to a filename for stdin.
extern const char bc_program_stdin_name[];

/// A reference to the ready message printed on SIGINT.
extern const char bc_program_ready_msg[];

/// A reference to the length of the ready message.
extern const size_t bc_program_ready_msg_len;

/// A reference to an array of escape characters for the print statement.
extern const char bc_program_esc_chars[];

/// A reference to an array of the characters corresponding to the escape
/// characters in bc_program_esc_chars.
extern const char bc_program_esc_seqs[];

#if BC_HAS_COMPUTED_GOTO

#if BC_DEBUG_CODE

// clang-format off
#define BC_PROG_JUMP(inst, code, ip)                                  \
	do                                                                \
	{                                                                 \
		inst = (uchar) (code)[(ip)->idx++];                           \
		bc_file_printf(&vm->ferr, "inst: %s\n", bc_inst_names[inst]); \
		bc_file_flush(&vm->ferr, bc_flush_none);                      \
		goto *bc_program_inst_lbls[inst];                             \
	}                                                                 \
	while (0)
// clang-format on

#else // BC_DEBUG_CODE

// clang-format off
#define BC_PROG_JUMP(inst, code, ip)        \
	do                                      \
	{                                       \
		inst = (uchar) (code)[(ip)->idx++]; \
		goto *bc_program_inst_lbls[inst];   \
	}                                       \
	while (0)
// clang-format on

#endif // BC_DEBUG_CODE

#define BC_PROG_DIRECT_JUMP(l) goto lbl_##l;
#define BC_PROG_LBL(l) lbl_##l
#define BC_PROG_FALLTHROUGH

#if BC_C11

#define BC_PROG_LBLS_SIZE (sizeof(bc_program_inst_lbls) / sizeof(void*))
#define BC_PROG_LBLS_ASSERT                                  \
	_Static_assert(BC_PROG_LBLS_SIZE == BC_INST_INVALID + 1, \
	               "bc_program_inst_lbls[] mismatches the instructions")

#else // BC_C11

#define BC_PROG_LBLS_ASSERT

#endif // BC_C11

#if BC_ENABLED

#if DC_ENABLED

#if BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                    \
	static const void* const bc_program_inst_lbls[] = { \
		&&lbl_BC_INST_INC,                              \
		&&lbl_BC_INST_DEC,                              \
		&&lbl_BC_INST_NEG,                              \
		&&lbl_BC_INST_BOOL_NOT,                         \
		&&lbl_BC_INST_TRUNC,                            \
		&&lbl_BC_INST_POWER,                            \
		&&lbl_BC_INST_MULTIPLY,                         \
		&&lbl_BC_INST_DIVIDE,                           \
		&&lbl_BC_INST_MODULUS,                          \
		&&lbl_BC_INST_PLUS,                             \
		&&lbl_BC_INST_MINUS,                            \
		&&lbl_BC_INST_PLACES,                           \
		&&lbl_BC_INST_LSHIFT,                           \
		&&lbl_BC_INST_RSHIFT,                           \
		&&lbl_BC_INST_REL_EQ,                           \
		&&lbl_BC_INST_REL_LE,                           \
		&&lbl_BC_INST_REL_GE,                           \
		&&lbl_BC_INST_REL_NE,                           \
		&&lbl_BC_INST_REL_LT,                           \
		&&lbl_BC_INST_REL_GT,                           \
		&&lbl_BC_INST_BOOL_OR,                          \
		&&lbl_BC_INST_BOOL_AND,                         \
		&&lbl_BC_INST_ASSIGN_POWER,                     \
		&&lbl_BC_INST_ASSIGN_MULTIPLY,                  \
		&&lbl_BC_INST_ASSIGN_DIVIDE,                    \
		&&lbl_BC_INST_ASSIGN_MODULUS,                   \
		&&lbl_BC_INST_ASSIGN_PLUS,                      \
		&&lbl_BC_INST_ASSIGN_MINUS,                     \
		&&lbl_BC_INST_ASSIGN_PLACES,                    \
		&&lbl_BC_INST_ASSIGN_LSHIFT,                    \
		&&lbl_BC_INST_ASSIGN_RSHIFT,                    \
		&&lbl_BC_INST_ASSIGN,                           \
		&&lbl_BC_INST_ASSIGN_POWER_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_MULTIPLY_NO_VAL,           \
		&&lbl_BC_INST_ASSIGN_DIVIDE_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_MODULUS_NO_VAL,            \
		&&lbl_BC_INST_ASSIGN_PLUS_NO_VAL,               \
		&&lbl_BC_INST_ASSIGN_MINUS_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_PLACES_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_LSHIFT_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_RSHIFT_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_NO_VAL,                    \
		&&lbl_BC_INST_NUM,                              \
		&&lbl_BC_INST_VAR,                              \
		&&lbl_BC_INST_ARRAY_ELEM,                       \
		&&lbl_BC_INST_ARRAY,                            \
		&&lbl_BC_INST_ZERO,                             \
		&&lbl_BC_INST_ONE,                              \
		&&lbl_BC_INST_LAST,                             \
		&&lbl_BC_INST_IBASE,                            \
		&&lbl_BC_INST_OBASE,                            \
		&&lbl_BC_INST_SCALE,                            \
		&&lbl_BC_INST_SEED,                             \
		&&lbl_BC_INST_LENGTH,                           \
		&&lbl_BC_INST_SCALE_FUNC,                       \
		&&lbl_BC_INST_SQRT,                             \
		&&lbl_BC_INST_ABS,                              \
		&&lbl_BC_INST_IS_NUMBER,                        \
		&&lbl_BC_INST_IS_STRING,                        \
		&&lbl_BC_INST_IRAND,                            \
		&&lbl_BC_INST_ASCIIFY,                          \
		&&lbl_BC_INST_READ,                             \
		&&lbl_BC_INST_RAND,                             \
		&&lbl_BC_INST_MAXIBASE,                         \
		&&lbl_BC_INST_MAXOBASE,                         \
		&&lbl_BC_INST_MAXSCALE,                         \
		&&lbl_BC_INST_MAXRAND,                          \
		&&lbl_BC_INST_LINE_LENGTH,                      \
		&&lbl_BC_INST_GLOBAL_STACKS,                    \
		&&lbl_BC_INST_LEADING_ZERO,                     \
		&&lbl_BC_INST_PRINT,                            \
		&&lbl_BC_INST_PRINT_POP,                        \
		&&lbl_BC_INST_STR,                              \
		&&lbl_BC_INST_PRINT_STR,                        \
		&&lbl_BC_INST_JUMP,                             \
		&&lbl_BC_INST_JUMP_ZERO,                        \
		&&lbl_BC_INST_CALL,                             \
		&&lbl_BC_INST_RET,                              \
		&&lbl_BC_INST_RET0,                             \
		&&lbl_BC_INST_RET_VOID,                         \
		&&lbl_BC_INST_HALT,                             \
		&&lbl_BC_INST_POP,                              \
		&&lbl_BC_INST_SWAP,                             \
		&&lbl_BC_INST_MODEXP,                           \
		&&lbl_BC_INST_DIVMOD,                           \
		&&lbl_BC_INST_PRINT_STREAM,                     \
		&&lbl_BC_INST_EXTENDED_REGISTERS,               \
		&&lbl_BC_INST_POP_EXEC,                         \
		&&lbl_BC_INST_EXECUTE,                          \
		&&lbl_BC_INST_EXEC_COND,                        \
		&&lbl_BC_INST_PRINT_STACK,                      \
		&&lbl_BC_INST_CLEAR_STACK,                      \
		&&lbl_BC_INST_REG_STACK_LEN,                    \
		&&lbl_BC_INST_STACK_LEN,                        \
		&&lbl_BC_INST_DUPLICATE,                        \
		&&lbl_BC_INST_LOAD,                             \
		&&lbl_BC_INST_PUSH_VAR,                         \
		&&lbl_BC_INST_PUSH_TO_VAR,                      \
		&&lbl_BC_INST_QUIT,                             \
		&&lbl_BC_INST_NQUIT,                            \
		&&lbl_BC_INST_EXEC_STACK_LEN,                   \
		&&lbl_BC_INST_INVALID,                          \
	}

#else // BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                    \
	static const void* const bc_program_inst_lbls[] = { \
		&&lbl_BC_INST_INC,                              \
		&&lbl_BC_INST_DEC,                              \
		&&lbl_BC_INST_NEG,                              \
		&&lbl_BC_INST_BOOL_NOT,                         \
		&&lbl_BC_INST_POWER,                            \
		&&lbl_BC_INST_MULTIPLY,                         \
		&&lbl_BC_INST_DIVIDE,                           \
		&&lbl_BC_INST_MODULUS,                          \
		&&lbl_BC_INST_PLUS,                             \
		&&lbl_BC_INST_MINUS,                            \
		&&lbl_BC_INST_REL_EQ,                           \
		&&lbl_BC_INST_REL_LE,                           \
		&&lbl_BC_INST_REL_GE,                           \
		&&lbl_BC_INST_REL_NE,                           \
		&&lbl_BC_INST_REL_LT,                           \
		&&lbl_BC_INST_REL_GT,                           \
		&&lbl_BC_INST_BOOL_OR,                          \
		&&lbl_BC_INST_BOOL_AND,                         \
		&&lbl_BC_INST_ASSIGN_POWER,                     \
		&&lbl_BC_INST_ASSIGN_MULTIPLY,                  \
		&&lbl_BC_INST_ASSIGN_DIVIDE,                    \
		&&lbl_BC_INST_ASSIGN_MODULUS,                   \
		&&lbl_BC_INST_ASSIGN_PLUS,                      \
		&&lbl_BC_INST_ASSIGN_MINUS,                     \
		&&lbl_BC_INST_ASSIGN,                           \
		&&lbl_BC_INST_ASSIGN_POWER_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_MULTIPLY_NO_VAL,           \
		&&lbl_BC_INST_ASSIGN_DIVIDE_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_MODULUS_NO_VAL,            \
		&&lbl_BC_INST_ASSIGN_PLUS_NO_VAL,               \
		&&lbl_BC_INST_ASSIGN_MINUS_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_NO_VAL,                    \
		&&lbl_BC_INST_NUM,                              \
		&&lbl_BC_INST_VAR,                              \
		&&lbl_BC_INST_ARRAY_ELEM,                       \
		&&lbl_BC_INST_ARRAY,                            \
		&&lbl_BC_INST_ZERO,                             \
		&&lbl_BC_INST_ONE,                              \
		&&lbl_BC_INST_LAST,                             \
		&&lbl_BC_INST_IBASE,                            \
		&&lbl_BC_INST_OBASE,                            \
		&&lbl_BC_INST_SCALE,                            \
		&&lbl_BC_INST_LENGTH,                           \
		&&lbl_BC_INST_SCALE_FUNC,                       \
		&&lbl_BC_INST_SQRT,                             \
		&&lbl_BC_INST_ABS,                              \
		&&lbl_BC_INST_IS_NUMBER,                        \
		&&lbl_BC_INST_IS_STRING,                        \
		&&lbl_BC_INST_ASCIIFY,                          \
		&&lbl_BC_INST_READ,                             \
		&&lbl_BC_INST_MAXIBASE,                         \
		&&lbl_BC_INST_MAXOBASE,                         \
		&&lbl_BC_INST_MAXSCALE,                         \
		&&lbl_BC_INST_LINE_LENGTH,                      \
		&&lbl_BC_INST_GLOBAL_STACKS,                    \
		&&lbl_BC_INST_LEADING_ZERO,                     \
		&&lbl_BC_INST_PRINT,                            \
		&&lbl_BC_INST_PRINT_POP,                        \
		&&lbl_BC_INST_STR,                              \
		&&lbl_BC_INST_PRINT_STR,                        \
		&&lbl_BC_INST_JUMP,                             \
		&&lbl_BC_INST_JUMP_ZERO,                        \
		&&lbl_BC_INST_CALL,                             \
		&&lbl_BC_INST_RET,                              \
		&&lbl_BC_INST_RET0,                             \
		&&lbl_BC_INST_RET_VOID,                         \
		&&lbl_BC_INST_HALT,                             \
		&&lbl_BC_INST_POP,                              \
		&&lbl_BC_INST_SWAP,                             \
		&&lbl_BC_INST_MODEXP,                           \
		&&lbl_BC_INST_DIVMOD,                           \
		&&lbl_BC_INST_PRINT_STREAM,                     \
		&&lbl_BC_INST_EXTENDED_REGISTERS,               \
		&&lbl_BC_INST_POP_EXEC,                         \
		&&lbl_BC_INST_EXECUTE,                          \
		&&lbl_BC_INST_EXEC_COND,                        \
		&&lbl_BC_INST_PRINT_STACK,                      \
		&&lbl_BC_INST_CLEAR_STACK,                      \
		&&lbl_BC_INST_REG_STACK_LEN,                    \
		&&lbl_BC_INST_STACK_LEN,                        \
		&&lbl_BC_INST_DUPLICATE,                        \
		&&lbl_BC_INST_LOAD,                             \
		&&lbl_BC_INST_PUSH_VAR,                         \
		&&lbl_BC_INST_PUSH_TO_VAR,                      \
		&&lbl_BC_INST_QUIT,                             \
		&&lbl_BC_INST_NQUIT,                            \
		&&lbl_BC_INST_EXEC_STACK_LEN,                   \
		&&lbl_BC_INST_INVALID,                          \
	}

#endif // BC_ENABLE_EXTRA_MATH

#else // DC_ENABLED

#if BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                    \
	static const void* const bc_program_inst_lbls[] = { \
		&&lbl_BC_INST_INC,                              \
		&&lbl_BC_INST_DEC,                              \
		&&lbl_BC_INST_NEG,                              \
		&&lbl_BC_INST_BOOL_NOT,                         \
		&&lbl_BC_INST_TRUNC,                            \
		&&lbl_BC_INST_POWER,                            \
		&&lbl_BC_INST_MULTIPLY,                         \
		&&lbl_BC_INST_DIVIDE,                           \
		&&lbl_BC_INST_MODULUS,                          \
		&&lbl_BC_INST_PLUS,                             \
		&&lbl_BC_INST_MINUS,                            \
		&&lbl_BC_INST_PLACES,                           \
		&&lbl_BC_INST_LSHIFT,                           \
		&&lbl_BC_INST_RSHIFT,                           \
		&&lbl_BC_INST_REL_EQ,                           \
		&&lbl_BC_INST_REL_LE,                           \
		&&lbl_BC_INST_REL_GE,                           \
		&&lbl_BC_INST_REL_NE,                           \
		&&lbl_BC_INST_REL_LT,                           \
		&&lbl_BC_INST_REL_GT,                           \
		&&lbl_BC_INST_BOOL_OR,                          \
		&&lbl_BC_INST_BOOL_AND,                         \
		&&lbl_BC_INST_ASSIGN_POWER,                     \
		&&lbl_BC_INST_ASSIGN_MULTIPLY,                  \
		&&lbl_BC_INST_ASSIGN_DIVIDE,                    \
		&&lbl_BC_INST_ASSIGN_MODULUS,                   \
		&&lbl_BC_INST_ASSIGN_PLUS,                      \
		&&lbl_BC_INST_ASSIGN_MINUS,                     \
		&&lbl_BC_INST_ASSIGN_PLACES,                    \
		&&lbl_BC_INST_ASSIGN_LSHIFT,                    \
		&&lbl_BC_INST_ASSIGN_RSHIFT,                    \
		&&lbl_BC_INST_ASSIGN,                           \
		&&lbl_BC_INST_ASSIGN_POWER_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_MULTIPLY_NO_VAL,           \
		&&lbl_BC_INST_ASSIGN_DIVIDE_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_MODULUS_NO_VAL,            \
		&&lbl_BC_INST_ASSIGN_PLUS_NO_VAL,               \
		&&lbl_BC_INST_ASSIGN_MINUS_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_PLACES_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_LSHIFT_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_RSHIFT_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_NO_VAL,                    \
		&&lbl_BC_INST_NUM,                              \
		&&lbl_BC_INST_VAR,                              \
		&&lbl_BC_INST_ARRAY_ELEM,                       \
		&&lbl_BC_INST_ARRAY,                            \
		&&lbl_BC_INST_ZERO,                             \
		&&lbl_BC_INST_ONE,                              \
		&&lbl_BC_INST_LAST,                             \
		&&lbl_BC_INST_IBASE,                            \
		&&lbl_BC_INST_OBASE,                            \
		&&lbl_BC_INST_SCALE,                            \
		&&lbl_BC_INST_SEED,                             \
		&&lbl_BC_INST_LENGTH,                           \
		&&lbl_BC_INST_SCALE_FUNC,                       \
		&&lbl_BC_INST_SQRT,                             \
		&&lbl_BC_INST_ABS,                              \
		&&lbl_BC_INST_IS_NUMBER,                        \
		&&lbl_BC_INST_IS_STRING,                        \
		&&lbl_BC_INST_IRAND,                            \
		&&lbl_BC_INST_ASCIIFY,                          \
		&&lbl_BC_INST_READ,                             \
		&&lbl_BC_INST_RAND,                             \
		&&lbl_BC_INST_MAXIBASE,                         \
		&&lbl_BC_INST_MAXOBASE,                         \
		&&lbl_BC_INST_MAXSCALE,                         \
		&&lbl_BC_INST_MAXRAND,                          \
		&&lbl_BC_INST_LINE_LENGTH,                      \
		&&lbl_BC_INST_GLOBAL_STACKS,                    \
		&&lbl_BC_INST_LEADING_ZERO,                     \
		&&lbl_BC_INST_PRINT,                            \
		&&lbl_BC_INST_PRINT_POP,                        \
		&&lbl_BC_INST_STR,                              \
		&&lbl_BC_INST_PRINT_STR,                        \
		&&lbl_BC_INST_JUMP,                             \
		&&lbl_BC_INST_JUMP_ZERO,                        \
		&&lbl_BC_INST_CALL,                             \
		&&lbl_BC_INST_RET,                              \
		&&lbl_BC_INST_RET0,                             \
		&&lbl_BC_INST_RET_VOID,                         \
		&&lbl_BC_INST_HALT,                             \
		&&lbl_BC_INST_POP,                              \
		&&lbl_BC_INST_SWAP,                             \
		&&lbl_BC_INST_MODEXP,                           \
		&&lbl_BC_INST_DIVMOD,                           \
		&&lbl_BC_INST_PRINT_STREAM,                     \
		&&lbl_BC_INST_INVALID,                          \
	}

#else // BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                    \
	static const void* const bc_program_inst_lbls[] = { \
		&&lbl_BC_INST_INC,                              \
		&&lbl_BC_INST_DEC,                              \
		&&lbl_BC_INST_NEG,                              \
		&&lbl_BC_INST_BOOL_NOT,                         \
		&&lbl_BC_INST_POWER,                            \
		&&lbl_BC_INST_MULTIPLY,                         \
		&&lbl_BC_INST_DIVIDE,                           \
		&&lbl_BC_INST_MODULUS,                          \
		&&lbl_BC_INST_PLUS,                             \
		&&lbl_BC_INST_MINUS,                            \
		&&lbl_BC_INST_REL_EQ,                           \
		&&lbl_BC_INST_REL_LE,                           \
		&&lbl_BC_INST_REL_GE,                           \
		&&lbl_BC_INST_REL_NE,                           \
		&&lbl_BC_INST_REL_LT,                           \
		&&lbl_BC_INST_REL_GT,                           \
		&&lbl_BC_INST_BOOL_OR,                          \
		&&lbl_BC_INST_BOOL_AND,                         \
		&&lbl_BC_INST_ASSIGN_POWER,                     \
		&&lbl_BC_INST_ASSIGN_MULTIPLY,                  \
		&&lbl_BC_INST_ASSIGN_DIVIDE,                    \
		&&lbl_BC_INST_ASSIGN_MODULUS,                   \
		&&lbl_BC_INST_ASSIGN_PLUS,                      \
		&&lbl_BC_INST_ASSIGN_MINUS,                     \
		&&lbl_BC_INST_ASSIGN,                           \
		&&lbl_BC_INST_ASSIGN_POWER_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_MULTIPLY_NO_VAL,           \
		&&lbl_BC_INST_ASSIGN_DIVIDE_NO_VAL,             \
		&&lbl_BC_INST_ASSIGN_MODULUS_NO_VAL,            \
		&&lbl_BC_INST_ASSIGN_PLUS_NO_VAL,               \
		&&lbl_BC_INST_ASSIGN_MINUS_NO_VAL,              \
		&&lbl_BC_INST_ASSIGN_NO_VAL,                    \
		&&lbl_BC_INST_NUM,                              \
		&&lbl_BC_INST_VAR,                              \
		&&lbl_BC_INST_ARRAY_ELEM,                       \
		&&lbl_BC_INST_ARRAY,                            \
		&&lbl_BC_INST_ZERO,                             \
		&&lbl_BC_INST_ONE,                              \
		&&lbl_BC_INST_LAST,                             \
		&&lbl_BC_INST_IBASE,                            \
		&&lbl_BC_INST_OBASE,                            \
		&&lbl_BC_INST_SCALE,                            \
		&&lbl_BC_INST_LENGTH,                           \
		&&lbl_BC_INST_SCALE_FUNC,                       \
		&&lbl_BC_INST_SQRT,                             \
		&&lbl_BC_INST_ABS,                              \
		&&lbl_BC_INST_IS_NUMBER,                        \
		&&lbl_BC_INST_IS_STRING,                        \
		&&lbl_BC_INST_ASCIIFY,                          \
		&&lbl_BC_INST_READ,                             \
		&&lbl_BC_INST_MAXIBASE,                         \
		&&lbl_BC_INST_MAXOBASE,                         \
		&&lbl_BC_INST_MAXSCALE,                         \
		&&lbl_BC_INST_LINE_LENGTH,                      \
		&&lbl_BC_INST_GLOBAL_STACKS,                    \
		&&lbl_BC_INST_LEADING_ZERO,                     \
		&&lbl_BC_INST_PRINT,                            \
		&&lbl_BC_INST_PRINT_POP,                        \
		&&lbl_BC_INST_STR,                              \
		&&lbl_BC_INST_PRINT_STR,                        \
		&&lbl_BC_INST_JUMP,                             \
		&&lbl_BC_INST_JUMP_ZERO,                        \
		&&lbl_BC_INST_CALL,                             \
		&&lbl_BC_INST_RET,                              \
		&&lbl_BC_INST_RET0,                             \
		&&lbl_BC_INST_RET_VOID,                         \
		&&lbl_BC_INST_HALT,                             \
		&&lbl_BC_INST_POP,                              \
		&&lbl_BC_INST_SWAP,                             \
		&&lbl_BC_INST_MODEXP,                           \
		&&lbl_BC_INST_DIVMOD,                           \
		&&lbl_BC_INST_PRINT_STREAM,                     \
		&&lbl_BC_INST_INVALID,                          \
	}

#endif // BC_ENABLE_EXTRA_MATH

#endif // DC_ENABLED

#else // BC_ENABLED

#if BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                                   \
	static const void* const bc_program_inst_lbls[] = {                \
		&&lbl_BC_INST_NEG,           &&lbl_BC_INST_BOOL_NOT,           \
		&&lbl_BC_INST_TRUNC,         &&lbl_BC_INST_POWER,              \
		&&lbl_BC_INST_MULTIPLY,      &&lbl_BC_INST_DIVIDE,             \
		&&lbl_BC_INST_MODULUS,       &&lbl_BC_INST_PLUS,               \
		&&lbl_BC_INST_MINUS,         &&lbl_BC_INST_PLACES,             \
		&&lbl_BC_INST_LSHIFT,        &&lbl_BC_INST_RSHIFT,             \
		&&lbl_BC_INST_REL_EQ,        &&lbl_BC_INST_REL_LE,             \
		&&lbl_BC_INST_REL_GE,        &&lbl_BC_INST_REL_NE,             \
		&&lbl_BC_INST_REL_LT,        &&lbl_BC_INST_REL_GT,             \
		&&lbl_BC_INST_BOOL_OR,       &&lbl_BC_INST_BOOL_AND,           \
		&&lbl_BC_INST_ASSIGN_NO_VAL, &&lbl_BC_INST_NUM,                \
		&&lbl_BC_INST_VAR,           &&lbl_BC_INST_ARRAY_ELEM,         \
		&&lbl_BC_INST_ARRAY,         &&lbl_BC_INST_ZERO,               \
		&&lbl_BC_INST_ONE,           &&lbl_BC_INST_IBASE,              \
		&&lbl_BC_INST_OBASE,         &&lbl_BC_INST_SCALE,              \
		&&lbl_BC_INST_SEED,          &&lbl_BC_INST_LENGTH,             \
		&&lbl_BC_INST_SCALE_FUNC,    &&lbl_BC_INST_SQRT,               \
		&&lbl_BC_INST_ABS,           &&lbl_BC_INST_IS_NUMBER,          \
		&&lbl_BC_INST_IS_STRING,     &&lbl_BC_INST_IRAND,              \
		&&lbl_BC_INST_ASCIIFY,       &&lbl_BC_INST_READ,               \
		&&lbl_BC_INST_RAND,          &&lbl_BC_INST_MAXIBASE,           \
		&&lbl_BC_INST_MAXOBASE,      &&lbl_BC_INST_MAXSCALE,           \
		&&lbl_BC_INST_MAXRAND,       &&lbl_BC_INST_LINE_LENGTH,        \
		&&lbl_BC_INST_LEADING_ZERO,  &&lbl_BC_INST_PRINT,              \
		&&lbl_BC_INST_PRINT_POP,     &&lbl_BC_INST_STR,                \
		&&lbl_BC_INST_POP,           &&lbl_BC_INST_SWAP,               \
		&&lbl_BC_INST_MODEXP,        &&lbl_BC_INST_DIVMOD,             \
		&&lbl_BC_INST_PRINT_STREAM,  &&lbl_BC_INST_EXTENDED_REGISTERS, \
		&&lbl_BC_INST_POP_EXEC,      &&lbl_BC_INST_EXECUTE,            \
		&&lbl_BC_INST_EXEC_COND,     &&lbl_BC_INST_PRINT_STACK,        \
		&&lbl_BC_INST_CLEAR_STACK,   &&lbl_BC_INST_REG_STACK_LEN,      \
		&&lbl_BC_INST_STACK_LEN,     &&lbl_BC_INST_DUPLICATE,          \
		&&lbl_BC_INST_LOAD,          &&lbl_BC_INST_PUSH_VAR,           \
		&&lbl_BC_INST_PUSH_TO_VAR,   &&lbl_BC_INST_QUIT,               \
		&&lbl_BC_INST_NQUIT,         &&lbl_BC_INST_EXEC_STACK_LEN,     \
		&&lbl_BC_INST_INVALID,                                         \
	}

#else // BC_ENABLE_EXTRA_MATH

#define BC_PROG_LBLS                                                   \
	static const void* const bc_program_inst_lbls[] = {                \
		&&lbl_BC_INST_NEG,           &&lbl_BC_INST_BOOL_NOT,           \
		&&lbl_BC_INST_POWER,         &&lbl_BC_INST_MULTIPLY,           \
		&&lbl_BC_INST_DIVIDE,        &&lbl_BC_INST_MODULUS,            \
		&&lbl_BC_INST_PLUS,          &&lbl_BC_INST_MINUS,              \
		&&lbl_BC_INST_REL_EQ,        &&lbl_BC_INST_REL_LE,             \
		&&lbl_BC_INST_REL_GE,        &&lbl_BC_INST_REL_NE,             \
		&&lbl_BC_INST_REL_LT,        &&lbl_BC_INST_REL_GT,             \
		&&lbl_BC_INST_BOOL_OR,       &&lbl_BC_INST_BOOL_AND,           \
		&&lbl_BC_INST_ASSIGN_NO_VAL, &&lbl_BC_INST_NUM,                \
		&&lbl_BC_INST_VAR,           &&lbl_BC_INST_ARRAY_ELEM,         \
		&&lbl_BC_INST_ARRAY,         &&lbl_BC_INST_ZERO,               \
		&&lbl_BC_INST_ONE,           &&lbl_BC_INST_IBASE,              \
		&&lbl_BC_INST_OBASE,         &&lbl_BC_INST_SCALE,              \
		&&lbl_BC_INST_LENGTH,        &&lbl_BC_INST_SCALE_FUNC,         \
		&&lbl_BC_INST_SQRT,          &&lbl_BC_INST_ABS,                \
		&&lbl_BC_INST_IS_NUMBER,     &&lbl_BC_INST_IS_STRING,          \
		&&lbl_BC_INST_ASCIIFY,       &&lbl_BC_INST_READ,               \
		&&lbl_BC_INST_MAXIBASE,      &&lbl_BC_INST_MAXOBASE,           \
		&&lbl_BC_INST_MAXSCALE,      &&lbl_BC_INST_LINE_LENGTH,        \
		&&lbl_BC_INST_LEADING_ZERO,  &&lbl_BC_INST_PRINT,              \
		&&lbl_BC_INST_PRINT_POP,     &&lbl_BC_INST_STR,                \
		&&lbl_BC_INST_POP,           &&lbl_BC_INST_SWAP,               \
		&&lbl_BC_INST_MODEXP,        &&lbl_BC_INST_DIVMOD,             \
		&&lbl_BC_INST_PRINT_STREAM,  &&lbl_BC_INST_EXTENDED_REGISTERS, \
		&&lbl_BC_INST_POP_EXEC,      &&lbl_BC_INST_EXECUTE,            \
		&&lbl_BC_INST_EXEC_COND,     &&lbl_BC_INST_PRINT_STACK,        \
		&&lbl_BC_INST_CLEAR_STACK,   &&lbl_BC_INST_REG_STACK_LEN,      \
		&&lbl_BC_INST_STACK_LEN,     &&lbl_BC_INST_DUPLICATE,          \
		&&lbl_BC_INST_LOAD,          &&lbl_BC_INST_PUSH_VAR,           \
		&&lbl_BC_INST_PUSH_TO_VAR,   &&lbl_BC_INST_QUIT,               \
		&&lbl_BC_INST_NQUIT,         &&lbl_BC_INST_EXEC_STACK_LEN,     \
		&&lbl_BC_INST_INVALID,                                         \
	}

#endif // BC_ENABLE_EXTRA_MATH

#endif // BC_ENABLED

#else // BC_HAS_COMPUTED_GOTO

#define BC_PROG_JUMP(inst, code, ip) break
#define BC_PROG_DIRECT_JUMP(l)
#define BC_PROG_LBL(l) case l
#define BC_PROG_FALLTHROUGH BC_FALLTHROUGH

#define BC_PROG_LBLS

#endif // BC_HAS_COMPUTED_GOTO

#endif // BC_PROGRAM_H
