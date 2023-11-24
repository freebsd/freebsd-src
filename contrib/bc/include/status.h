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
 * All bc status codes and cross-platform portability.
 *
 */

#ifndef BC_STATUS_H
#define BC_STATUS_H

#ifdef _WIN32
#include <Windows.h>
#include <BaseTsd.h>
#include <stdio.h>
#include <io.h>
#endif // _WIN32

#include <stdint.h>
#include <sys/types.h>

// This is used by configure.sh to test for OpenBSD.
#ifdef BC_TEST_OPENBSD
#ifdef __OpenBSD__
#error On OpenBSD without _BSD_SOURCE
#endif // __OpenBSD__
#endif // BC_TEST_OPENBSD

// This is used by configure.sh to test for FreeBSD.
#ifdef BC_TEST_FREEBSD
#ifdef __FreeBSD__
#error On FreeBSD with _POSIX_C_SOURCE
#endif // __FreeBSD__
#endif // BC_TEST_FREEBSD

// This is used by configure.sh to test for Mac OSX.
#ifdef BC_TEST_APPLE
#ifdef __APPLE__
#error On Mac OSX without _DARWIN_C_SOURCE
#endif // __APPLE__
#endif // BC_TEST_APPLE

// Windows has deprecated isatty() and the rest of these. Or doesn't have them.
// So these are just fixes for Windows.
#ifdef _WIN32

// This one is special. Windows did not like me defining an
// inline function that was not given a definition in a header
// file. This suppresses that by making inline functions non-inline.
#define inline

#define restrict __restrict
#define strdup _strdup
#define write(f, b, s) _write((f), (b), (unsigned int) (s))
#define read(f, b, s) _read((f), (b), (unsigned int) (s))
#define close _close
#define open(f, n, m) \
	_sopen_s((f), (n), (m) | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE)
#define sigjmp_buf jmp_buf
#define sigsetjmp(j, s) setjmp(j)
#define siglongjmp longjmp
#define isatty _isatty
#define STDIN_FILENO _fileno(stdin)
#define STDOUT_FILENO _fileno(stdout)
#define STDERR_FILENO _fileno(stderr)
#define S_ISDIR(m) ((m) & (_S_IFDIR))
#define O_RDONLY _O_RDONLY
#define stat _stat
#define fstat _fstat
#define BC_FILE_SEP '\\'

#else // _WIN32
#define BC_FILE_SEP '/'
#endif // _WIN32

#ifndef BC_ENABLED
#define BC_ENABLED (1)
#endif // BC_ENABLED

#ifndef DC_ENABLED
#define DC_ENABLED (1)
#endif // DC_ENABLED

#ifndef BC_ENABLE_EXTRA_MATH
#define BC_ENABLE_EXTRA_MATH (1)
#endif // BC_ENABLE_EXTRA_MATH

#ifndef BC_ENABLE_LIBRARY
#define BC_ENABLE_LIBRARY (0)
#endif // BC_ENABLE_LIBRARY

#ifndef BC_ENABLE_HISTORY
#define BC_ENABLE_HISTORY (1)
#endif // BC_ENABLE_HISTORY

#ifndef BC_ENABLE_EDITLINE
#define BC_ENABLE_EDITLINE (0)
#endif // BC_ENABLE_EDITLINE

#ifndef BC_ENABLE_READLINE
#define BC_ENABLE_READLINE (0)
#endif // BC_ENABLE_READLINE

#ifndef BC_ENABLE_NLS
#define BC_ENABLE_NLS (0)
#endif // BC_ENABLE_NLS

#ifdef __OpenBSD__
#if BC_ENABLE_READLINE
#error Cannot use readline on OpenBSD
#endif // BC_ENABLE_READLINE
#endif // __OpenBSD__

#if BC_ENABLE_EDITLINE && BC_ENABLE_READLINE
#error Must enable only one of editline or readline, not both.
#endif // BC_ENABLE_EDITLINE && BC_ENABLE_READLINE

#if BC_ENABLE_EDITLINE || BC_ENABLE_READLINE
#define BC_ENABLE_LINE_LIB (1)
#else // BC_ENABLE_EDITLINE || BC_ENABLE_READLINE
#define BC_ENABLE_LINE_LIB (0)
#endif // BC_ENABLE_EDITLINE || BC_ENABLE_READLINE

// This is error checking for fuzz builds.
#if BC_ENABLE_AFL
#ifndef __AFL_HAVE_MANUAL_CONTROL
#error Must compile with afl-clang-fast or afl-clang-lto for fuzzing
#endif // __AFL_HAVE_MANUAL_CONTROL
#endif // BC_ENABLE_AFL

#ifndef BC_ENABLE_MEMCHECK
#define BC_ENABLE_MEMCHECK (0)
#endif // BC_ENABLE_MEMCHECK

/**
 * Mark a variable as unused.
 * @param e  The variable to mark as unused.
 */
#define BC_UNUSED(e) ((void) (e))

// If users want, they can define this to something like __builtin_expect(e, 1).
// It might give a performance improvement.
#ifndef BC_LIKELY

/**
 * Mark a branch expression as likely.
 * @param e  The expression to mark as likely.
 */
#define BC_LIKELY(e) (e)

#endif // BC_LIKELY

// If users want, they can define this to something like __builtin_expect(e, 0).
// It might give a performance improvement.
#ifndef BC_UNLIKELY

/**
 * Mark a branch expression as unlikely.
 * @param e  The expression to mark as unlikely.
 */
#define BC_UNLIKELY(e) (e)

#endif // BC_UNLIKELY

/**
 * Mark a branch expression as an error, if true.
 * @param e  The expression to mark as an error, if true.
 */
#define BC_ERR(e) BC_UNLIKELY(e)

/**
 * Mark a branch expression as not an error, if true.
 * @param e  The expression to mark as not an error, if true.
 */
#define BC_NO_ERR(s) BC_LIKELY(s)

// Disable extra debug code by default.
#ifndef BC_DEBUG_CODE
#define BC_DEBUG_CODE (0)
#endif // BC_DEBUG_CODE

#if defined(__clang__)
#define BC_CLANG (1)
#else // defined(__clang__)
#define BC_CLANG (0)
#endif // defined(__clang__)

#if defined(__GNUC__) && !BC_CLANG
#define BC_GCC (1)
#else // defined(__GNUC__) && !BC_CLANG
#define BC_GCC (0)
#endif // defined(__GNUC__) && !BC_CLANG

// We want to be able to use _Noreturn on C11 compilers.
#if __STDC_VERSION__ >= 201112L

#include <stdnoreturn.h>
#define BC_NORETURN _Noreturn
#define BC_C11 (1)

#else // __STDC_VERSION__

#if BC_CLANG
#if __has_attribute(noreturn)
#define BC_NORETURN __attribute((noreturn))
#else // __has_attribute(noreturn)
#define BC_NORETURN
#endif // __has_attribute(noreturn)

#else // BC_CLANG

#define BC_NORETURN

#endif // BC_CLANG

#define BC_MUST_RETURN
#define BC_C11 (0)

#endif // __STDC_VERSION__

#define BC_HAS_UNREACHABLE (0)
#define BC_HAS_COMPUTED_GOTO (0)

// GCC and Clang complain if fallthroughs are not marked with their special
// attribute. Jerks. This creates a define for marking the fallthroughs that is
// nothing on other compilers.
#if BC_CLANG || BC_GCC

#if defined(__has_attribute)

#if __has_attribute(fallthrough)
#define BC_FALLTHROUGH __attribute__((fallthrough));
#else // __has_attribute(fallthrough)
#define BC_FALLTHROUGH
#endif // __has_attribute(fallthrough)

#if BC_GCC

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#undef BC_HAS_UNREACHABLE
#define BC_HAS_UNREACHABLE (1)
#endif // __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#else // BC_GCC

#if __clang_major__ >= 4
#undef BC_HAS_UNREACHABLE
#define BC_HAS_UNREACHABLE (1)
#endif // __clang_major__ >= 4

#endif // BC_GCC

#else // defined(__has_attribute)
#define BC_FALLTHROUGH
#endif // defined(__has_attribute)
#else // BC_CLANG || BC_GCC
#define BC_FALLTHROUGH
#endif // BC_CLANG || BC_GCC

#if BC_HAS_UNREACHABLE

#define BC_UNREACHABLE __builtin_unreachable();

#else // BC_HAS_UNREACHABLE

#ifdef _WIN32

#define BC_UNREACHABLE __assume(0);

#else // _WIN32

#define BC_UNREACHABLE

#endif // _WIN32

#endif // BC_HAS_UNREACHABLE

#if BC_GCC

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (1)

#endif // __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#endif // BC_GCC

#if BC_CLANG

#if __clang_major__ >= 4

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (1)

#endif // __clang_major__ >= 4

#endif // BC_CLANG

#ifdef BC_NO_COMPUTED_GOTO

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (0)

#endif // BC_NO_COMPUTED_GOTO

#if BC_GCC
#ifdef __OpenBSD__
// The OpenBSD GCC doesn't like inline.
#define inline
#endif // __OpenBSD__
#endif // BC_GCC

// Workarounds for AIX's POSIX incompatibility.
#ifndef SIZE_MAX
#define SIZE_MAX __SIZE_MAX__
#endif // SIZE_MAX
#ifndef UINTMAX_C
#define UINTMAX_C __UINTMAX_C
#endif // UINTMAX_C
#ifndef UINT32_C
#define UINT32_C __UINT32_C
#endif // UINT32_C
#ifndef UINT_FAST32_MAX
#define UINT_FAST32_MAX __UINT_FAST32_MAX__
#endif // UINT_FAST32_MAX
#ifndef UINT16_MAX
#define UINT16_MAX __UINT16_MAX__
#endif // UINT16_MAX
#ifndef SIG_ATOMIC_MAX
#define SIG_ATOMIC_MAX __SIG_ATOMIC_MAX__
#endif // SIG_ATOMIC_MAX

// Yes, this has to be here.
#include <bcl.h>

// All of these set defaults for settings.

#if BC_ENABLED

#ifndef BC_DEFAULT_BANNER
#define BC_DEFAULT_BANNER (0)
#endif // BC_DEFAULT_BANNER

#endif // BC_ENABLED

#ifndef BC_DEFAULT_SIGINT_RESET
#define BC_DEFAULT_SIGINT_RESET (1)
#endif // BC_DEFAULT_SIGINT_RESET

#ifndef BC_DEFAULT_TTY_MODE
#define BC_DEFAULT_TTY_MODE (1)
#endif // BC_DEFAULT_TTY_MODE

#ifndef BC_DEFAULT_PROMPT
#define BC_DEFAULT_PROMPT BC_DEFAULT_TTY_MODE
#endif // BC_DEFAULT_PROMPT

#ifndef BC_DEFAULT_EXPR_EXIT
#define BC_DEFAULT_EXPR_EXIT (1)
#endif // BC_DEFAULT_EXPR_EXIT

#ifndef BC_DEFAULT_DIGIT_CLAMP
#define BC_DEFAULT_DIGIT_CLAMP (0)
#endif // BC_DEFAULT_DIGIT_CLAMP

// All of these set defaults for settings.
#ifndef DC_DEFAULT_SIGINT_RESET
#define DC_DEFAULT_SIGINT_RESET (1)
#endif // DC_DEFAULT_SIGINT_RESET

#ifndef DC_DEFAULT_TTY_MODE
#define DC_DEFAULT_TTY_MODE (0)
#endif // DC_DEFAULT_TTY_MODE

#ifndef DC_DEFAULT_HISTORY
#define DC_DEFAULT_HISTORY DC_DEFAULT_TTY_MODE
#endif // DC_DEFAULT_HISTORY

#ifndef DC_DEFAULT_PROMPT
#define DC_DEFAULT_PROMPT DC_DEFAULT_TTY_MODE
#endif // DC_DEFAULT_PROMPT

#ifndef DC_DEFAULT_EXPR_EXIT
#define DC_DEFAULT_EXPR_EXIT (1)
#endif // DC_DEFAULT_EXPR_EXIT

#ifndef DC_DEFAULT_DIGIT_CLAMP
#define DC_DEFAULT_DIGIT_CLAMP (0)
#endif // DC_DEFAULT_DIGIT_CLAMP

/// Statuses, which mark either which category of error happened, or some other
/// status that matters.
typedef enum BcStatus
{
	/// Normal status.
	BC_STATUS_SUCCESS = 0,

	/// Math error.
	BC_STATUS_ERROR_MATH,

	/// Parse (and lex) error.
	BC_STATUS_ERROR_PARSE,

	/// Runtime error.
	BC_STATUS_ERROR_EXEC,

	/// Fatal error.
	BC_STATUS_ERROR_FATAL,

	/// EOF status.
	BC_STATUS_EOF,

	/// Quit status. This means that bc/dc is in the process of quitting.
	BC_STATUS_QUIT,

} BcStatus;

/// Errors, which are more specific errors.
typedef enum BcErr
{
	// Math errors.

	/// Negative number used when not allowed.
	BC_ERR_MATH_NEGATIVE,

	/// Non-integer used when not allowed.
	BC_ERR_MATH_NON_INTEGER,

	/// Conversion to a hardware integer would overflow.
	BC_ERR_MATH_OVERFLOW,

	/// Divide by zero.
	BC_ERR_MATH_DIVIDE_BY_ZERO,

	// Fatal errors.

	/// An allocation or reallocation failed.
	BC_ERR_FATAL_ALLOC_ERR,

	/// I/O failure.
	BC_ERR_FATAL_IO_ERR,

	/// File error, such as permissions or file does not exist.
	BC_ERR_FATAL_FILE_ERR,

	/// File is binary, not text, error.
	BC_ERR_FATAL_BIN_FILE,

	/// Attempted to read a directory as a file error.
	BC_ERR_FATAL_PATH_DIR,

	/// Invalid option error.
	BC_ERR_FATAL_OPTION,

	/// Option with required argument not given an argument.
	BC_ERR_FATAL_OPTION_NO_ARG,

	/// Option with no argument given an argument.
	BC_ERR_FATAL_OPTION_ARG,

	/// Option argument is invalid.
	BC_ERR_FATAL_ARG,

	// Runtime errors.

	/// Invalid ibase value.
	BC_ERR_EXEC_IBASE,

	/// Invalid obase value.
	BC_ERR_EXEC_OBASE,

	/// Invalid scale value.
	BC_ERR_EXEC_SCALE,

	/// Invalid expression parsed by read().
	BC_ERR_EXEC_READ_EXPR,

	/// read() used within an expression given to a read() call.
	BC_ERR_EXEC_REC_READ,

	/// Type error.
	BC_ERR_EXEC_TYPE,

	/// Stack has too few elements error.
	BC_ERR_EXEC_STACK,

	/// Register stack has too few elements error.
	BC_ERR_EXEC_STACK_REGISTER,

	/// Wrong number of arguments error.
	BC_ERR_EXEC_PARAMS,

	/// Undefined function error.
	BC_ERR_EXEC_UNDEF_FUNC,

	/// Void value used in an expression error.
	BC_ERR_EXEC_VOID_VAL,

	// Parse (and lex) errors.

	/// EOF encountered when not expected error.
	BC_ERR_PARSE_EOF,

	/// Invalid character error.
	BC_ERR_PARSE_CHAR,

	/// Invalid string (no ending quote) error.
	BC_ERR_PARSE_STRING,

	/// Invalid comment (no end found) error.
	BC_ERR_PARSE_COMMENT,

	/// Invalid token encountered error.
	BC_ERR_PARSE_TOKEN,

#if BC_ENABLED

	/// Invalid expression error.
	BC_ERR_PARSE_EXPR,

	/// Expression is empty error.
	BC_ERR_PARSE_EMPTY_EXPR,

	/// Print statement is invalid error.
	BC_ERR_PARSE_PRINT,

	/// Function definition is invalid error.
	BC_ERR_PARSE_FUNC,

	/// Assignment is invalid error.
	BC_ERR_PARSE_ASSIGN,

	/// No auto identifiers given for an auto statement error.
	BC_ERR_PARSE_NO_AUTO,

	/// Duplicate local (parameter or auto) error.
	BC_ERR_PARSE_DUP_LOCAL,

	/// Invalid block (within braces) error.
	BC_ERR_PARSE_BLOCK,

	/// Invalid return statement for void functions.
	BC_ERR_PARSE_RET_VOID,

	/// Reference attached to a variable, not an array, error.
	BC_ERR_PARSE_REF_VAR,

	// POSIX-only errors.

	/// Name length greater than 1 error.
	BC_ERR_POSIX_NAME_LEN,

	/// Non-POSIX comment used error.
	BC_ERR_POSIX_COMMENT,

	/// Non-POSIX keyword error.
	BC_ERR_POSIX_KW,

	/// Non-POSIX . (last) error.
	BC_ERR_POSIX_DOT,

	/// Non-POSIX return error.
	BC_ERR_POSIX_RET,

	/// Non-POSIX boolean operator used error.
	BC_ERR_POSIX_BOOL,

	/// POSIX relation operator used outside if, while, or for statements error.
	BC_ERR_POSIX_REL_POS,

	/// Multiple POSIX relation operators used in an if, while, or for statement
	/// error.
	BC_ERR_POSIX_MULTIREL,

	/// Empty statements in POSIX for loop error.
	BC_ERR_POSIX_FOR,

	/// POSIX's grammar does not allow a function definition right after a
	/// semicolon.
	BC_ERR_POSIX_FUNC_AFTER_SEMICOLON,

	/// Non-POSIX exponential (scientific or engineering) number used error.
	BC_ERR_POSIX_EXP_NUM,

	/// Non-POSIX array reference error.
	BC_ERR_POSIX_REF,

	/// Non-POSIX void error.
	BC_ERR_POSIX_VOID,

	/// Non-POSIX brace position used error.
	BC_ERR_POSIX_BRACE,

	/// String used in expression.
	BC_ERR_POSIX_EXPR_STRING,

#endif // BC_ENABLED

	// Number of elements.
	BC_ERR_NELEMS,

#if BC_ENABLED

	/// A marker for the start of POSIX errors.
	BC_ERR_POSIX_START = BC_ERR_POSIX_NAME_LEN,

	/// A marker for the end of POSIX errors.
	BC_ERR_POSIX_END = BC_ERR_POSIX_EXPR_STRING,

#endif // BC_ENABLED

} BcErr;

// The indices of each category of error in bc_errs[], and used in bc_err_ids[]
// to associate actual errors with their categories.

/// Math error category.
#define BC_ERR_IDX_MATH (0)

/// Parse (and lex) error category.
#define BC_ERR_IDX_PARSE (1)

/// Runtime error category.
#define BC_ERR_IDX_EXEC (2)

/// Fatal error category.
#define BC_ERR_IDX_FATAL (3)

/// Number of categories.
#define BC_ERR_IDX_NELEMS (4)

// If bc is enabled, we add an extra category for POSIX warnings.
#if BC_ENABLED

/// POSIX warning category.
#define BC_ERR_IDX_WARN (BC_ERR_IDX_NELEMS)

#endif // BC_ENABLED

/**
 * The mode bc is in. This is basically what input it is processing.
 */
typedef enum BcMode
{
	/// Expressions mode.
	BC_MODE_EXPRS,

	/// File mode.
	BC_MODE_FILE,

	/// stdin mode.
	BC_MODE_STDIN,

} BcMode;

/// Do a longjmp(). This is what to use when activating an "exception", i.e., a
/// longjmp(). With debug code, it will print the name of the function it jumped
/// from.
#if BC_DEBUG_CODE
#define BC_JMP bc_vm_jmp(__func__)
#else // BC_DEBUG_CODE
#define BC_JMP bc_vm_jmp()
#endif // BC_DEBUG_CODE

#if !BC_ENABLE_LIBRARY

/// Returns true if an exception is in flight, false otherwise.
#define BC_SIG_EXC(vm) \
	BC_UNLIKELY((vm)->status != (sig_atomic_t) BC_STATUS_SUCCESS || (vm)->sig)

/// Returns true if there is *no* exception in flight, false otherwise.
#define BC_NO_SIG_EXC(vm) \
	BC_LIKELY((vm)->status == (sig_atomic_t) BC_STATUS_SUCCESS && !(vm)->sig)

#ifndef _WIN32
#define BC_SIG_INTERRUPT(vm) \
	BC_UNLIKELY((vm)->sig != 0 && (vm)->sig != SIGWINCH)
#else // _WIN32
#define BC_SIG_INTERRUPT(vm) BC_UNLIKELY((vm)->sig != 0)
#endif // _WIN32

#if BC_DEBUG

/// Assert that signals are locked. There are non-async-signal-safe functions in
/// bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are locked.
#define BC_SIG_ASSERT_LOCKED  \
	do                        \
	{                         \
		assert(vm->sig_lock); \
	}                         \
	while (0)

/// Assert that signals are unlocked. There are non-async-signal-safe functions
/// in bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are unlocked.
#define BC_SIG_ASSERT_NOT_LOCKED   \
	do                             \
	{                              \
		assert(vm->sig_lock == 0); \
	}                              \
	while (0)

#else // BC_DEBUG

/// Assert that signals are locked. There are non-async-signal-safe functions in
/// bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are locked.
#define BC_SIG_ASSERT_LOCKED

/// Assert that signals are unlocked. There are non-async-signal-safe functions
/// in bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are unlocked.
#define BC_SIG_ASSERT_NOT_LOCKED

#endif // BC_DEBUG

/// Locks signals.
#define BC_SIG_LOCK               \
	do                            \
	{                             \
		BC_SIG_ASSERT_NOT_LOCKED; \
		vm->sig_lock = 1;         \
	}                             \
	while (0)

/// Unlocks signals. If a signal happened, then this will cause a jump.
#define BC_SIG_UNLOCK         \
	do                        \
	{                         \
		BC_SIG_ASSERT_LOCKED; \
		vm->sig_lock = 0;     \
		if (vm->sig) BC_JMP;  \
	}                         \
	while (0)

/// Locks signals, regardless of if they are already locked. This is really only
/// used after labels that longjmp() goes to after the jump because the cleanup
/// code must have signals locked, and BC_LONGJMP_CONT will unlock signals if it
/// doesn't jump.
#define BC_SIG_MAYLOCK    \
	do                    \
	{                     \
		vm->sig_lock = 1; \
	}                     \
	while (0)

/// Unlocks signals, regardless of if they were already unlocked. If a signal
/// happened, then this will cause a jump.
#define BC_SIG_MAYUNLOCK     \
	do                       \
	{                        \
		vm->sig_lock = 0;    \
		if (vm->sig) BC_JMP; \
	}                        \
	while (0)

/**
 * Locks signals, but stores the old lock state, to be restored later by
 * BC_SIG_TRYUNLOCK.
 * @param v  The variable to store the old lock state to.
 */
#define BC_SIG_TRYLOCK(v) \
	do                    \
	{                     \
		v = vm->sig_lock; \
		vm->sig_lock = 1; \
	}                     \
	while (0)

/**
 * Restores the previous state of a signal lock, and if it is now unlocked,
 * initiates an exception/jump.
 * @param v  The old lock state.
 */
#define BC_SIG_TRYUNLOCK(v)          \
	do                               \
	{                                \
		vm->sig_lock = (v);          \
		if (!(v) && vm->sig) BC_JMP; \
	}                                \
	while (0)

/// Stops a stack unwinding. Technically, a stack unwinding needs to be done
/// manually, but it will always be done unless certain flags are cleared. This
/// clears the flags.
#define BC_LONGJMP_STOP  \
	do                   \
	{                    \
		vm->sig_pop = 0; \
		vm->sig = 0;     \
	}                    \
	while (0)

/**
 * Sets a jump like BC_SETJMP, but unlike BC_SETJMP, it assumes signals are
 * locked and will just set the jump. This does *not* have a call to
 * bc_vec_grow() because it is assumed that BC_SETJMP_LOCKED(l) is used *after*
 * the initializations that need the setjmp().
 * param l  The label to jump to on a longjmp().
 */
#define BC_SETJMP_LOCKED(vm, l)           \
	do                                    \
	{                                     \
		sigjmp_buf sjb;                   \
		BC_SIG_ASSERT_LOCKED;             \
		if (sigsetjmp(sjb, 0))            \
		{                                 \
			assert(BC_SIG_EXC(vm));       \
			goto l;                       \
		}                                 \
		bc_vec_push(&vm->jmp_bufs, &sjb); \
	}                                     \
	while (0)

/// Used after cleanup labels set by BC_SETJMP and BC_SETJMP_LOCKED to jump to
/// the next place. This is what continues the stack unwinding. This basically
/// copies BC_SIG_UNLOCK into itself, but that is because its condition for
/// jumping is BC_SIG_EXC, not just that a signal happened.
#define BC_LONGJMP_CONT(vm)                          \
	do                                               \
	{                                                \
		BC_SIG_ASSERT_LOCKED;                        \
		if (!vm->sig_pop) bc_vec_pop(&vm->jmp_bufs); \
		vm->sig_lock = 0;                            \
		if (BC_SIG_EXC(vm)) BC_JMP;                  \
	}                                                \
	while (0)

#else // !BC_ENABLE_LIBRARY

#define BC_SIG_LOCK
#define BC_SIG_UNLOCK
#define BC_SIG_MAYLOCK
#define BC_SIG_TRYLOCK(lock)
#define BC_SIG_TRYUNLOCK(lock)
#define BC_SIG_ASSERT_LOCKED

/// Returns true if an exception is in flight, false otherwise.
#define BC_SIG_EXC(vm) \
	BC_UNLIKELY(vm->status != (sig_atomic_t) BC_STATUS_SUCCESS)

/// Returns true if there is *no* exception in flight, false otherwise.
#define BC_NO_SIG_EXC(vm) \
	BC_LIKELY(vm->status == (sig_atomic_t) BC_STATUS_SUCCESS)

/// Used after cleanup labels set by BC_SETJMP and BC_SETJMP_LOCKED to jump to
/// the next place. This is what continues the stack unwinding. This basically
/// copies BC_SIG_UNLOCK into itself, but that is because its condition for
/// jumping is BC_SIG_EXC, not just that a signal happened.
#define BC_LONGJMP_CONT(vm)         \
	do                              \
	{                               \
		bc_vec_pop(&vm->jmp_bufs);  \
		if (BC_SIG_EXC(vm)) BC_JMP; \
	}                               \
	while (0)

#endif // !BC_ENABLE_LIBRARY

/**
 * Sets a jump, and sets it up as well so that if a longjmp() happens, bc will
 * immediately goto a label where some cleanup code is. This one assumes that
 * signals are not locked and will lock them, set the jump, and unlock them.
 * Setting the jump also includes pushing the jmp_buf onto the jmp_buf stack.
 * This grows the jmp_bufs vector first to prevent a fatal error from happening
 * after the setjmp(). This is done because BC_SETJMP(l) is assumed to be used
 * *before* the actual initialization calls that need the setjmp().
 * param l  The label to jump to on a longjmp().
 */
#define BC_SETJMP(vm, l)                  \
	do                                    \
	{                                     \
		sigjmp_buf sjb;                   \
		BC_SIG_LOCK;                      \
		bc_vec_grow(&vm->jmp_bufs, 1);    \
		if (sigsetjmp(sjb, 0))            \
		{                                 \
			assert(BC_SIG_EXC(vm));       \
			goto l;                       \
		}                                 \
		bc_vec_push(&vm->jmp_bufs, &sjb); \
		BC_SIG_UNLOCK;                    \
	}                                     \
	while (0)

/// Unsets a jump. It always assumes signals are locked. This basically just
/// pops a jmp_buf off of the stack of jmp_bufs, and since the jump mechanism
/// always jumps to the location at the top of the stack, this effectively
/// undoes a setjmp().
#define BC_UNSETJMP(vm)            \
	do                             \
	{                              \
		BC_SIG_ASSERT_LOCKED;      \
		bc_vec_pop(&vm->jmp_bufs); \
	}                              \
	while (0)

#if BC_ENABLE_LIBRARY

#define BC_SETJMP_LOCKED(vm, l) BC_SETJMP(vm, l)

// Various convenience macros for calling the bc's error handling routine.

/**
 * Call bc's error handling routine.
 * @param e    The error.
 * @param l    The line of the script that the error happened.
 * @param ...  Extra arguments for error messages as necessary.
 */
#define bc_error(e, l, ...) (bc_vm_handleError((e)))

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#define bc_err(e) (bc_vm_handleError((e)))

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#define bc_verr(e, ...) (bc_vm_handleError((e)))

#else // BC_ENABLE_LIBRARY

// Various convenience macros for calling the bc's error handling routine.

/**
 * Call bc's error handling routine.
 * @param e    The error.
 * @param l    The line of the script that the error happened.
 * @param ...  Extra arguments for error messages as necessary.
 */
#if BC_DEBUG
#define bc_error(e, l, ...) \
	(bc_vm_handleError((e), __FILE__, __LINE__, (l), __VA_ARGS__))
#else // BC_DEBUG
#define bc_error(e, l, ...) (bc_vm_handleError((e), (l), __VA_ARGS__))
#endif // BC_DEBUG

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#if BC_DEBUG
#define bc_err(e) (bc_vm_handleError((e), __FILE__, __LINE__, 0))
#else // BC_DEBUG
#define bc_err(e) (bc_vm_handleError((e), 0))
#endif // BC_DEBUG

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#if BC_DEBUG
#define bc_verr(e, ...) \
	(bc_vm_handleError((e), __FILE__, __LINE__, 0, __VA_ARGS__))
#else // BC_DEBUG
#define bc_verr(e, ...) (bc_vm_handleError((e), 0, __VA_ARGS__))
#endif // BC_DEBUG

#endif // BC_ENABLE_LIBRARY

/**
 * Returns true if status @a s is an error, false otherwise.
 * @param s  The status to test.
 * @return   True if @a s is an error, false otherwise.
 */
#define BC_STATUS_IS_ERROR(s) \
	((s) >= BC_STATUS_ERROR_MATH && (s) <= BC_STATUS_ERROR_FATAL)

// Convenience macros that can be placed at the beginning and exits of functions
// for easy marking of where functions are entered and exited.
#if BC_DEBUG_CODE
#define BC_FUNC_ENTER                                               \
	do                                                              \
	{                                                               \
		size_t bc_func_enter_i;                                     \
		for (bc_func_enter_i = 0; bc_func_enter_i < vm->func_depth; \
		     ++bc_func_enter_i)                                     \
		{                                                           \
			bc_file_puts(&vm->ferr, bc_flush_none, "  ");           \
		}                                                           \
		vm->func_depth += 1;                                        \
		bc_file_printf(&vm->ferr, "Entering %s\n", __func__);       \
		bc_file_flush(&vm->ferr, bc_flush_none);                    \
	}                                                               \
	while (0);

#define BC_FUNC_EXIT                                                \
	do                                                              \
	{                                                               \
		size_t bc_func_enter_i;                                     \
		vm->func_depth -= 1;                                        \
		for (bc_func_enter_i = 0; bc_func_enter_i < vm->func_depth; \
		     ++bc_func_enter_i)                                     \
		{                                                           \
			bc_file_puts(&vm->ferr, bc_flush_none, "  ");           \
		}                                                           \
		bc_file_printf(&vm->ferr, "Leaving %s\n", __func__);        \
		bc_file_flush(&vm->ferr, bc_flush_none);                    \
	}                                                               \
	while (0);
#else // BC_DEBUG_CODE
#define BC_FUNC_ENTER
#define BC_FUNC_EXIT
#endif // BC_DEBUG_CODE

#endif // BC_STATUS_H
