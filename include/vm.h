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
 * Definitions for bc's VM.
 *
 */

#ifndef BC_VM_H
#define BC_VM_H

#include <assert.h>
#include <stddef.h>
#include <limits.h>

#include <signal.h>

#if BC_ENABLE_NLS

#ifdef _WIN32
#error NLS is not supported on Windows.
#endif // _WIN32

#include <nl_types.h>

#endif // BC_ENABLE_NLS

#include <version.h>
#include <status.h>
#include <num.h>
#include <lex.h>
#include <parse.h>
#include <program.h>
#include <history.h>
#include <bc.h>

// We don't want to include this file for the library because it's unused.
#if !BC_ENABLE_LIBRARY
#include <file.h>
#endif // !BC_ENABLE_LIBRARY

// This should be obvious. If neither calculator is enabled, barf.
#if !BC_ENABLED && !DC_ENABLED
#error Must define BC_ENABLED, DC_ENABLED, or both
#endif

// CHAR_BIT must be at least 6, for various reasons. I might want to bump this
// to 8 in the future.
#if CHAR_BIT < 6
#error CHAR_BIT must be at least 6.
#endif

// Set defaults.

#ifndef MAINEXEC
#define MAINEXEC bc
#endif // MAINEXEC

#ifndef _WIN32
#ifndef EXECPREFIX
#define EXECPREFIX
#endif // EXECPREFIX
#else // _WIN32
#undef EXECPREFIX
#endif // _WIN32

/**
 * Generate a string from text.
 * @parm V  The text to generate a string for.
 */
#define GEN_STR(V) #V

/**
 * Help generate a string from text. The preprocessor requires this two-step
 * process. Trust me.
 * @parm V  The text to generate a string for.
 */
#define GEN_STR2(V) GEN_STR(V)

/// The version as a string. VERSION must be defined previously, usually by the
/// build system.
#define BC_VERSION GEN_STR2(VERSION)

/// The main executable name as a string. MAINEXEC must be defined previously,
/// usually by the build system.
#define BC_MAINEXEC GEN_STR2(MAINEXEC)

/// The build type as a string. BUILD_TYPE must be defined previously, usually
/// by the build system.
#define BC_BUILD_TYPE GEN_STR2(BUILD_TYPE)

// We only allow an empty executable prefix on Windows.
#ifndef _WIN32
#define BC_EXECPREFIX GEN_STR2(EXECPREFIX)
#else // _WIN32
#define BC_EXECPREFIX ""
#endif // _WIN32

#if !BC_ENABLE_LIBRARY

#if DC_ENABLED

/// The flag for the extended register option.
#define DC_FLAG_X (UINTMAX_C(1) << 0)

#endif // DC_ENABLED

#if BC_ENABLED

/// The flag for the POSIX warning option.
#define BC_FLAG_W (UINTMAX_C(1) << 1)

/// The flag for the POSIX error option.
#define BC_FLAG_S (UINTMAX_C(1) << 2)

/// The flag for the math library option.
#define BC_FLAG_L (UINTMAX_C(1) << 3)

/// The flag for the global stacks option.
#define BC_FLAG_G (UINTMAX_C(1) << 4)

#endif // BC_ENABLED

/// The flag for quiet, though this one is reversed; the option clears the flag.
#define BC_FLAG_Q (UINTMAX_C(1) << 5)

/// The flag for interactive.
#define BC_FLAG_I (UINTMAX_C(1) << 6)

/// The flag for prompt. This is also reversed; the option clears the flag.
#define BC_FLAG_P (UINTMAX_C(1) << 7)

/// The flag for read prompt. This is also reversed; the option clears the flag.
#define BC_FLAG_R (UINTMAX_C(1) << 8)

/// The flag for a leading zero.
#define BC_FLAG_Z (UINTMAX_C(1) << 9)

/// The flag for stdin being a TTY.
#define BC_FLAG_TTYIN (UINTMAX_C(1) << 10)

/// The flag for TTY mode.
#define BC_FLAG_TTY (UINTMAX_C(1) << 11)

/// The flag for reset on SIGINT.
#define BC_FLAG_SIGINT (UINTMAX_C(1) << 12)

/// The flag for exiting with expressions.
#define BC_FLAG_EXPR_EXIT (UINTMAX_C(1) << 13)

/// The flag for digit clamping.
#define BC_FLAG_DIGIT_CLAMP (UINTMAX_C(1) << 14)

/// A convenience macro for getting the TTYIN flag.
#define BC_TTYIN (vm->flags & BC_FLAG_TTYIN)

/// A convenience macro for getting the TTY flag.
#define BC_TTY (vm->flags & BC_FLAG_TTY)

/// A convenience macro for getting the SIGINT flag.
#define BC_SIGINT (vm->flags & BC_FLAG_SIGINT)

#if BC_ENABLED

/// A convenience macro for getting the POSIX error flag.
#define BC_S (vm->flags & BC_FLAG_S)

/// A convenience macro for getting the POSIX warning flag.
#define BC_W (vm->flags & BC_FLAG_W)

/// A convenience macro for getting the math library flag.
#define BC_L (vm->flags & BC_FLAG_L)

/// A convenience macro for getting the global stacks flag.
#define BC_G (vm->flags & BC_FLAG_G)

#endif // BC_ENABLED

#if DC_ENABLED

/// A convenience macro for getting the extended register flag.
#define DC_X (vm->flags & DC_FLAG_X)

#endif // DC_ENABLED

/// A convenience macro for getting the interactive flag.
#define BC_I (vm->flags & BC_FLAG_I)

/// A convenience macro for getting the prompt flag.
#define BC_P (vm->flags & BC_FLAG_P)

/// A convenience macro for getting the read prompt flag.
#define BC_R (vm->flags & BC_FLAG_R)

/// A convenience macro for getting the leading zero flag.
#define BC_Z (vm->flags & BC_FLAG_Z)

/// A convenience macro for getting the expression exit flag.
#define BC_EXPR_EXIT (vm->flags & BC_FLAG_EXPR_EXIT)

/// A convenience macro for getting the digit clamp flag.
#define BC_DIGIT_CLAMP (vm->flags & BC_FLAG_DIGIT_CLAMP)

#if BC_ENABLED

/// A convenience macro for checking if bc is in POSIX mode.
#define BC_IS_POSIX (BC_S || BC_W)

#if DC_ENABLED

/// Returns true if bc is running.
#define BC_IS_BC (vm->name[0] != 'd')

/// Returns true if dc is running.
#define BC_IS_DC (vm->name[0] == 'd')

/// Returns the correct read prompt.
#define BC_VM_READ_PROMPT (BC_IS_BC ? "read> " : "?> ")

/// Returns the string for the line length environment variable.
#define BC_VM_LINE_LENGTH_STR (BC_IS_BC ? "BC_LINE_LENGTH" : "DC_LINE_LENGTH")

/// Returns the string for the environment args environment variable.
#define BC_VM_ENV_ARGS_STR (BC_IS_BC ? "BC_ENV_ARGS" : "DC_ENV_ARGS")

/// Returns the string for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_STR (BC_IS_BC ? "BC_EXPR_EXIT" : "DC_EXPR_EXIT")

/// Returns the default for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_DEF \
	(BC_IS_BC ? BC_DEFAULT_EXPR_EXIT : DC_DEFAULT_EXPR_EXIT)

/// Returns the string for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_STR (BC_IS_BC ? "BC_DIGIT_CLAMP" : "DC_DIGIT_CLAMP")

/// Returns the default for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_DEF \
	(BC_IS_BC ? BC_DEFAULT_DIGIT_CLAMP : DC_DEFAULT_DIGIT_CLAMP)

/// Returns the string for the TTY mode environment variable.
#define BC_VM_TTY_MODE_STR (BC_IS_BC ? "BC_TTY_MODE" : "DC_TTY_MODE")

/// Returns the default for the TTY mode environment variable.
#define BC_VM_TTY_MODE_DEF \
	(BC_IS_BC ? BC_DEFAULT_TTY_MODE : DC_DEFAULT_TTY_MODE)

/// Returns the string for the prompt environment variable.
#define BC_VM_PROMPT_STR (BC_IS_BC ? "BC_PROMPT" : "DC_PROMPT")

/// Returns the default for the prompt environment variable.
#define BC_VM_PROMPT_DEF (BC_IS_BC ? BC_DEFAULT_PROMPT : DC_DEFAULT_PROMPT)

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_STR \
	(BC_IS_BC ? "BC_SIGINT_RESET" : "DC_SIGINT_RESET")

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_DEF \
	(BC_IS_BC ? BC_DEFAULT_SIGINT_RESET : DC_DEFAULT_SIGINT_RESET)

/// Returns true if the calculator should run stdin.
#define BC_VM_RUN_STDIN(has_file) (BC_IS_BC || !(has_file))

#else // DC_ENABLED

/// Returns true if bc is running.
#define BC_IS_BC (1)

/// Returns true if dc is running.
#define BC_IS_DC (0)

/// Returns the correct read prompt.
#define BC_VM_READ_PROMPT ("read> ")

/// Returns the string for the line length environment variable.
#define BC_VM_LINE_LENGTH_STR ("BC_LINE_LENGTH")

/// Returns the string for the environment args environment variable.
#define BC_VM_ENV_ARGS_STR ("BC_ENV_ARGS")

/// Returns the string for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_STR ("BC_EXPR_EXIT")

/// Returns the default for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_DEF (BC_DEFAULT_EXPR_EXIT)

/// Returns the string for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_STR ("BC_DIGIT_CLAMP")

/// Returns the default for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_DEF (BC_DEFAULT_DIGIT_CLAMP)

/// Returns the string for the TTY mode environment variable.
#define BC_VM_TTY_MODE_STR ("BC_TTY_MODE")

/// Returns the default for the TTY mode environment variable.
#define BC_VM_TTY_MODE_DEF (BC_DEFAULT_TTY_MODE)

/// Returns the string for the prompt environment variable.
#define BC_VM_PROMPT_STR ("BC_PROMPT")

/// Returns the default for the SIGINT reset environment variable.
#define BC_VM_PROMPT_DEF (BC_DEFAULT_PROMPT)

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_STR ("BC_SIGINT_RESET")

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_DEF (BC_DEFAULT_SIGINT_RESET)

/// Returns true if the calculator should run stdin.
#define BC_VM_RUN_STDIN(has_file) (BC_IS_BC)

#endif // DC_ENABLED

#else // BC_ENABLED

/// A convenience macro for checking if bc is in POSIX mode.
#define BC_IS_POSIX (0)

/// Returns true if bc is running.
#define BC_IS_BC (0)

/// Returns true if dc is running.
#define BC_IS_DC (1)

/// Returns the correct read prompt.
#define BC_VM_READ_PROMPT ("?> ")

/// Returns the string for the line length environment variable.
#define BC_VM_LINE_LENGTH_STR ("DC_LINE_LENGTH")

/// Returns the string for the environment args environment variable.
#define BC_VM_ENV_ARGS_STR ("DC_ENV_ARGS")

/// Returns the string for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_STR ("DC_EXPR_EXIT")

/// Returns the default for the expression exit environment variable.
#define BC_VM_EXPR_EXIT_DEF (DC_DEFAULT_EXPR_EXIT)

/// Returns the string for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_STR ("DC_DIGIT_CLAMP")

/// Returns the default for the digit clamp environment variable.
#define BC_VM_DIGIT_CLAMP_DEF (DC_DEFAULT_DIGIT_CLAMP)

/// Returns the string for the TTY mode environment variable.
#define BC_VM_TTY_MODE_STR ("DC_TTY_MODE")

/// Returns the default for the TTY mode environment variable.
#define BC_VM_TTY_MODE_DEF (DC_DEFAULT_TTY_MODE)

/// Returns the string for the prompt environment variable.
#define BC_VM_PROMPT_STR ("DC_PROMPT")

/// Returns the default for the SIGINT reset environment variable.
#define BC_VM_PROMPT_DEF (DC_DEFAULT_PROMPT)

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_STR ("DC_SIGINT_RESET")

/// Returns the string for the SIGINT reset environment variable.
#define BC_VM_SIGINT_RESET_DEF (DC_DEFAULT_SIGINT_RESET)

/// Returns true if the calculator should run stdin.
#define BC_VM_RUN_STDIN(has_file) (!(has_file))

#endif // BC_ENABLED

/// A convenience macro for checking if the prompt is enabled.
#define BC_PROMPT (BC_P)

#else // !BC_ENABLE_LIBRARY

#define BC_Z (vm->leading_zeroes)

#define BC_DIGIT_CLAMP (vm->digit_clamp)

#endif // !BC_ENABLE_LIBRARY

/**
 * Returns the max of its two arguments. This evaluates arguments twice, so be
 * careful what args you give it.
 * @param a  The first argument.
 * @param b  The second argument.
 * @return   The max of the two arguments.
 */
#define BC_MAX(a, b) ((a) > (b) ? (a) : (b))

/**
 * Returns the min of its two arguments. This evaluates arguments twice, so be
 * careful what args you give it.
 * @param a  The first argument.
 * @param b  The second argument.
 * @return   The min of the two arguments.
 */
#define BC_MIN(a, b) ((a) < (b) ? (a) : (b))

/// Returns the max obase that is allowed.
#define BC_MAX_OBASE ((BcBigDig) (BC_BASE_POW))

/// Returns the max array size that is allowed.
#define BC_MAX_DIM ((BcBigDig) (SIZE_MAX - 1))

/// Returns the max scale that is allowed.
#define BC_MAX_SCALE ((BcBigDig) (BC_NUM_BIGDIG_MAX - 1))

/// Returns the max string length that is allowed.
#define BC_MAX_STRING ((BcBigDig) (BC_NUM_BIGDIG_MAX - 1))

/// Returns the max identifier length that is allowed.
#define BC_MAX_NAME BC_MAX_STRING

/// Returns the max number size that is allowed.
#define BC_MAX_NUM BC_MAX_SCALE

#if BC_ENABLE_EXTRA_MATH

/// Returns the max random integer that can be returned.
#define BC_MAX_RAND ((BcBigDig) (((BcRand) 0) - 1))

#endif // BC_ENABLE_EXTRA_MATH

/// Returns the max exponent that is allowed.
#define BC_MAX_EXP ((ulong) (BC_NUM_BIGDIG_MAX))

/// Returns the max number of variables that is allowed.
#define BC_MAX_VARS ((ulong) (SIZE_MAX - 1))

#if BC_ENABLE_LINE_LIB

/// The size of the global buffer.
#define BC_VM_BUF_SIZE (1 << 10)

/// The amount of the global buffer allocated to stdin.
#define BC_VM_STDIN_BUF_SIZE (BC_VM_BUF_SIZE - 1)

#else // BC_ENABLE_LINE_LIB

/// The size of the global buffer.
#define BC_VM_BUF_SIZE (1 << 12)

/// The amount of the global buffer allocated to stdout.
#define BC_VM_STDOUT_BUF_SIZE (1 << 11)

/// The amount of the global buffer allocated to stderr.
#define BC_VM_STDERR_BUF_SIZE (1 << 10)

/// The amount of the global buffer allocated to stdin.
#define BC_VM_STDIN_BUF_SIZE (BC_VM_STDERR_BUF_SIZE - 1)

#endif // BC_ENABLE_LINE_LIB

/// The max number of temporary BcNums that can be kept.
#define BC_VM_MAX_TEMPS (1 << 9)

/// The capacity of the one BcNum, which is a constant.
#define BC_VM_ONE_CAP (1)

/**
 * Returns true if a BcResult is safe for garbage collection.
 * @param r  The BcResult to test.
 * @return   True if @a r is safe to garbage collect.
 */
#define BC_VM_SAFE_RESULT(r) ((r)->t >= BC_RESULT_TEMP)

/// The invalid locale catalog return value.
#define BC_VM_INVALID_CATALOG ((nl_catd) -1)

/**
 * Returns true if the *unsigned* multiplication overflows.
 * @param a  The first operand.
 * @param b  The second operand.
 * @param r  The product.
 * @return   True if the multiplication of @a a and @a b overflows.
 */
#define BC_VM_MUL_OVERFLOW(a, b, r) \
	((r) >= SIZE_MAX || ((a) != 0 && (r) / (a) != (b)))

/// The global vm struct. This holds all of the global data besides the file
/// buffers.
typedef struct BcVm
{
	/// The current status. This is volatile sig_atomic_t because it is also
	/// used in the signal handler. See the development manual
	/// (manuals/development.md#async-signal-safe-signal-handling) for more
	/// information.
	volatile sig_atomic_t status;

	/// Non-zero if a jump series is in progress and items should be popped off
	/// the jmp_bufs vector. This is volatile sig_atomic_t because it is also
	/// used in the signal handler. See the development manual
	/// (manuals/development.md#async-signal-safe-signal-handling) for more
	/// information.
	volatile sig_atomic_t sig_pop;

#if !BC_ENABLE_LIBRARY

	/// The parser.
	BcParse prs;

	/// The program.
	BcProgram prog;

	/// A buffer for lines for stdin.
	BcVec line_buf;

	/// A buffer to hold a series of lines from stdin. Sometimes, multiple lines
	/// are necessary for parsing, such as a comment that spans multiple lines.
	BcVec buffer;

	/// A parser to parse read expressions.
	BcParse read_prs;

	/// A buffer for read expressions.
	BcVec read_buf;

#endif // !BC_ENABLE_LIBRARY

	/// A vector of jmp_bufs for doing a jump series. This allows exception-type
	/// error handling, while allowing me to do cleanup on the way.
	BcVec jmp_bufs;

	/// The number of temps in the temps array.
	size_t temps_len;

#if BC_ENABLE_LIBRARY

	/// The vector of contexts for the library.
	BcVec ctxts;

	/// The vector for creating strings to pass to the client.
	BcVec out;

	/// The PRNG.
	BcRNG rng;

	/// The current error.
	BclError err;

	/// Whether or not bcl should abort on fatal errors.
	bool abrt;

	/// Whether or not to print leading zeros.
	bool leading_zeroes;

	/// Whether or not to clamp digits that are greater than or equal to the
	/// current ibase.
	bool digit_clamp;

	/// The number of "references," or times that the library was initialized.
	unsigned int refs;

#else // BC_ENABLE_LIBRARY

	/// A pointer to the filename of the current file. This is not owned by the
	/// BcVm struct.
	const char* file;

	/// The message printed when SIGINT happens.
	const char* sigmsg;

	/// Non-zero when signals are "locked." This is volatile sig_atomic_t
	/// because it is also used in the signal handler. See the development
	/// manual (manuals/development.md#async-signal-safe-signal-handling) for
	/// more information.
	volatile sig_atomic_t sig_lock;

	/// Non-zero when a signal has been received, but not acted on. This is
	/// volatile sig_atomic_t because it is also used in the signal handler. See
	/// the development manual
	/// (manuals/development.md#async-signal-safe-signal-handling) for more
	/// information.
	volatile sig_atomic_t sig;

	/// The length of sigmsg.
	uchar siglen;

	/// The instruction used for returning from a read() call.
	uchar read_ret;

	/// The flags field used by most macros above.
	uint16_t flags;

	/// The number of characters printed in the current line. This is used
	/// because bc has a limit of the number of characters it can print per
	/// line.
	uint16_t nchars;

	/// The length of the line we can print. The user can set this if they wish.
	uint16_t line_len;

	/// True if bc should error if expressions are encountered during option
	/// parsing, false otherwise.
	bool no_exprs;

	/// True if bc should exit if expresions are encountered.
	bool exit_exprs;

	/// True if EOF was encountered.
	bool eof;

	/// The mode that the program is in.
	uchar mode;

#if BC_ENABLED

	/// True if keywords should not be redefined. This is only true for the
	/// builtin math libraries for bc.
	bool no_redefine;

#endif // BC_ENABLED

	/// A vector of filenames to process.
	BcVec files;

	/// A vector of expressions to process.
	BcVec exprs;

	/// The name of the calculator under use. This is used by BC_IS_BC and
	/// BC_IS_DC.
	const char* name;

	/// The help text for the calculator.
	const char* help;

#if BC_ENABLE_HISTORY

	/// The history data.
	BcHistory history;

#endif // BC_ENABLE_HISTORY

	/// The function to call to get the next lex token.
	BcLexNext next;

	/// The function to call to parse.
	BcParseParse parse;

	/// The function to call to parse expressions.
	BcParseExpr expr;

	/// The names of the categories of errors.
	const char* err_ids[BC_ERR_IDX_NELEMS + BC_ENABLED];

	/// The messages for each error.
	const char* err_msgs[BC_ERR_NELEMS];

#if BC_ENABLE_NLS
	/// The locale.
	const char* locale;
#endif // BC_ENABLE_NLS

#endif // BC_ENABLE_LIBRARY

	/// An array of maxes for the globals.
	BcBigDig maxes[BC_PROG_GLOBALS_LEN + BC_ENABLE_EXTRA_MATH];

	/// The last base used to parse.
	BcBigDig last_base;

	/// The last power of last_base used to parse.
	BcBigDig last_pow;

	/// The last exponent of base that equals last_pow.
	BcBigDig last_exp;

	/// BC_BASE_POW - last_pow.
	BcBigDig last_rem;

#if !BC_ENABLE_LIBRARY

	/// A buffer of environment arguments. This is the actual value of the
	/// environment variable.
	char* env_args_buffer;

	/// A vector for environment arguments after parsing.
	BcVec env_args;

	/// A BcNum set to constant 0.
	BcNum zero;

#endif // !BC_ENABLE_LIBRARY

	/// A BcNum set to constant 1.
	BcNum one;

	/// A BcNum holding the max number held by a BcBigDig plus 1.
	BcNum max;

	/// A BcNum holding the max number held by a BcBigDig times 2 plus 1.
	BcNum max2;

	/// The BcDig array for max.
	BcDig max_num[BC_NUM_BIGDIG_LOG10];

	/// The BcDig array for max2.
	BcDig max2_num[BC_NUM_BIGDIG_LOG10];

	// The BcDig array for the one BcNum.
	BcDig one_num[BC_VM_ONE_CAP];

#if !BC_ENABLE_LIBRARY

	// The BcDig array for the zero BcNum.
	BcDig zero_num[BC_VM_ONE_CAP];

	/// The stdout file.
	BcFile fout;

	/// The stderr file.
	BcFile ferr;

#if BC_ENABLE_NLS

	/// The locale catalog.
	nl_catd catalog;

#endif // BC_ENABLE_NLS

	/// A pointer to the stdin buffer.
	char* buf;

	/// The number of items in the input buffer.
	size_t buf_len;

	/// The slabs vector for constants, strings, function names, and other
	/// string-like things.
	BcVec slabs;

#if BC_ENABLED

	/// An array of booleans for which bc keywords have been redefined if
	/// BC_REDEFINE_KEYWORDS is non-zero.
	bool redefined_kws[BC_LEX_NKWS];

#endif // BC_ENABLED
#endif // !BC_ENABLE_LIBRARY

	BcDig* temps_buf[BC_VM_MAX_TEMPS];

#if BC_DEBUG_CODE

	/// The depth for BC_FUNC_ENTER and BC_FUNC_EXIT.
	size_t func_depth;

#endif // BC_DEBUG_CODE

} BcVm;

/**
 * Print the copyright banner and help if it's non-NULL.
 * @param help  The help message to print if it's non-NULL.
 */
void
bc_vm_info(const char* const help);

/**
 * The entrance point for bc/dc together.
 * @param argc  The count of arguments.
 * @param argv  The argument array.
 */
void
bc_vm_boot(int argc, char* argv[]);

/**
 * Initializes some of the BcVm global. This is separate to make things easier
 * on the library code.
 */
void
bc_vm_init(void);

/**
 * Frees the BcVm global.
 */
void
bc_vm_shutdown(void);

/**
 * Add a temp to the temp array.
 * @param num  The BcDig array to add to the temp array.
 */
void
bc_vm_addTemp(BcDig* num);

/**
 * Return the temp on the top of the temp stack, or NULL if there are none.
 * @return  A temp, or NULL if none exist.
 */
BcDig*
bc_vm_takeTemp(void);

/**
 * Gets the top temp of the temp stack. This is separate from bc_vm_takeTemp()
 * to quiet a GCC warning about longjmp() clobbering in bc_num_init().
 * @return  A temp, or NULL if none exist.
 */
BcDig*
bc_vm_getTemp(void);

/**
 * Frees all temporaries.
 */
void
bc_vm_freeTemps(void);

#if !BC_ENABLE_HISTORY || BC_ENABLE_LINE_LIB || BC_ENABLE_LIBRARY

/**
 * Erases the flush argument if history does not exist because it does not
 * matter if history does not exist.
 */
#define bc_vm_putchar(c, t) bc_vm_putchar_impl(c)

#else // !BC_ENABLE_HISTORY || BC_ENABLE_LINE_LIB || BC_ENABLE_LIBRARY

// This is here to satisfy a clang warning about recursive macros.
#define bc_vm_putchar(c, t) bc_vm_putchar_impl(c, t)

#endif // !BC_ENABLE_HISTORY || BC_ENABLE_LINE_LIB || BC_ENABLE_LIBRARY

/**
 * Print to stdout with limited formating.
 * @param fmt  The format string.
 */
void
bc_vm_printf(const char* fmt, ...);

/**
 * Puts a char into the stdout buffer.
 * @param c     The character to put on the stdout buffer.
 * @param type  The flush type.
 */
void
bc_vm_putchar(int c, BcFlushType type);

/**
 * Multiplies @a n and @a size and throws an allocation error if overflow
 * occurs.
 * @param n     The number of elements.
 * @param size  The size of each element.
 * @return      The product of @a n and @a size.
 */
size_t
bc_vm_arraySize(size_t n, size_t size);

/**
 * Adds @a a and @a b and throws an error if overflow occurs.
 * @param a  The first operand.
 * @param b  The second operand.
 * @return   The sum of @a a and @a b.
 */
size_t
bc_vm_growSize(size_t a, size_t b);

/**
 * Allocate @a n bytes and throw an allocation error if allocation fails.
 * @param n  The bytes to allocate.
 * @return   A pointer to the allocated memory.
 */
void*
bc_vm_malloc(size_t n);

/**
 * Reallocate @a ptr to be @a n bytes and throw an allocation error if
 * reallocation fails.
 * @param ptr  The pointer to a memory allocation to reallocate.
 * @param n    The bytes to allocate.
 * @return     A pointer to the reallocated memory.
 */
void*
bc_vm_realloc(void* ptr, size_t n);

/**
 * Allocates space for, and duplicates, @a str.
 * @param str  The string to allocate.
 * @return     The allocated string.
 */
char*
bc_vm_strdup(const char* str);

/**
 * Reads a line from stdin into BcVm's buffer field.
 * @param clear  True if the buffer should be cleared first, false otherwise.
 * @return       True if a line was read, false otherwise.
 */
bool
bc_vm_readLine(bool clear);

/**
 * Reads a line from the command-line expressions into BcVm's buffer field.
 * @param clear  True if the buffer should be cleared first, false otherwise.
 * @return       True if a line was read, false otherwise.
 */
bool
bc_vm_readBuf(bool clear);

/**
 * A convenience and portability function for OpenBSD's pledge().
 * @param promises      The promises to pledge().
 * @param execpromises  The exec promises to pledge().
 */
void
bc_pledge(const char* promises, const char* execpromises);

/**
 * Returns the value of an environment variable.
 * @param var  The environment variable.
 * @return     The value of the environment variable.
 */
char*
bc_vm_getenv(const char* var);

/**
 * Frees an environment variable value.
 * @param val  The value to free.
 */
void
bc_vm_getenvFree(char* val);

#if BC_DEBUG_CODE

/**
 * Start executing a jump series.
 * @param f  The name of the function that started the jump series.
 */
void
bc_vm_jmp(const char* f);

#else // BC_DEBUG_CODE

/**
 * Start executing a jump series.
 */
void
bc_vm_jmp(void);

#endif // BC_DEBUG_CODE

#if BC_ENABLE_LIBRARY

/**
 * Handle an error. This is the true error handler. It will start a jump series
 * if an error occurred. POSIX errors will not cause jumps when warnings are on
 * or no POSIX errors are enabled.
 * @param e  The error.
 */
void
bc_vm_handleError(BcErr e);

/**
 * Handle a fatal error.
 * @param e  The error.
 */
void
bc_vm_fatalError(BcErr e);

/**
 * A function to call at exit.
 */
void
bc_vm_atexit(void);

#else // BC_ENABLE_LIBRARY

/**
 * Calculates the number of decimal digits in the argument.
 * @param val  The value to calculate the number of decimal digits in.
 * @return     The number of decimal digits in @a val.
 */
size_t
bc_vm_numDigits(size_t val);

#ifndef NDEBUG

/**
 * Handle an error. This is the true error handler. It will start a jump series
 * if an error occurred. POSIX errors will not cause jumps when warnings are on
 * or no POSIX errors are enabled.
 * @param e      The error.
 * @param file   The source file where the error occurred.
 * @param fline  The line in the source file where the error occurred.
 * @param line   The bc source line where the error occurred.
 */
void
bc_vm_handleError(BcErr e, const char* file, int fline, size_t line, ...);

#else // NDEBUG

/**
 * Handle an error. This is the true error handler. It will start a jump series
 * if an error occurred. POSIX errors will not cause jumps when warnings are on
 * or no POSIX errors are enabled.
 * @param e     The error.
 * @param line  The bc source line where the error occurred.
 */
void
bc_vm_handleError(BcErr e, size_t line, ...);

#endif // NDEBUG

/**
 * Handle a fatal error.
 * @param e  The error.
 */
#if !BC_ENABLE_MEMCHECK
BC_NORETURN
#endif // !BC_ENABLE_MEMCHECK
void
bc_vm_fatalError(BcErr e);

/**
 * A function to call at exit.
 * @param status  The exit status.
 */
int
bc_vm_atexit(int status);
#endif // BC_ENABLE_LIBRARY

/// A reference to the copyright header.
extern const char bc_copyright[];

/// A reference to the array of default error category names.
extern const char* bc_errs[];

/// A reference to the array of error category indices for each error.
extern const uchar bc_err_ids[];

/// A reference to the array of default error messages.
extern const char* const bc_err_msgs[];

/// A reference to the pledge() promises at start.
extern const char bc_pledge_start[];

#if BC_ENABLE_HISTORY

/// A reference to the end pledge() promises when using history.
extern const char bc_pledge_end_history[];

#endif // BC_ENABLE_HISTORY

/// A reference to the end pledge() promises when *not* using history.
extern const char bc_pledge_end[];

#if !BC_ENABLE_LIBRARY

/// A reference to the global data.
extern BcVm* vm;

/// The global data.
extern BcVm vm_data;

/// A reference to the global output buffers.
extern char output_bufs[BC_VM_BUF_SIZE];

#endif // !BC_ENABLE_LIBRARY

#endif // BC_VM_H
