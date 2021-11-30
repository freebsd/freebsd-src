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
 * All bc status codes and cross-platform portability.
 *
 */

#ifndef BC_STATUS_H
#define BC_STATUS_H

#include <stdint.h>

// This is used by configure.sh to test for OpenBSD.
#ifdef BC_TEST_OPENBSD
#ifdef __OpenBSD__
#error On OpenBSD without _BSD_SOURCE
#endif // __OpenBSD__
#endif // BC_TEST_OPENBSD

#ifndef BC_ENABLED
#define BC_ENABLED (1)
#endif // BC_ENABLED

#ifndef DC_ENABLED
#define DC_ENABLED (1)
#endif // DC_ENABLED

#ifndef BC_ENABLE_LIBRARY
#define BC_ENABLE_LIBRARY (0)
#endif // BC_ENABLE_LIBRARY

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

// We want to be able to use _Noreturn on C11 compilers.
#if __STDC_VERSION__ >= 201100L

#include <stdnoreturn.h>
#define BC_NORETURN _Noreturn
#define BC_C11 (1)

#else // __STDC_VERSION__

#define BC_NORETURN
#define BC_MUST_RETURN
#define BC_C11 (0)

#endif // __STDC_VERSION__

#define BC_HAS_UNREACHABLE (0)
#define BC_HAS_COMPUTED_GOTO (0)

// GCC and Clang complain if fallthroughs are not marked with their special
// attribute. Jerks. This creates a define for marking the fallthroughs that is
// nothing on other compilers.
#if defined(__clang__) || defined(__GNUC__)

#if defined(__has_attribute)

#if __has_attribute(fallthrough)
#define BC_FALLTHROUGH __attribute__((fallthrough));
#else // __has_attribute(fallthrough)
#define BC_FALLTHROUGH
#endif // __has_attribute(fallthrough)

#ifdef __GNUC__

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)
#undef BC_HAS_UNREACHABLE
#define BC_HAS_UNREACHABLE (1)
#endif // __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#else // __GNUC__

#if __clang_major__ >= 4
#undef BC_HAS_UNREACHABLE
#define BC_HAS_UNREACHABLE (1)
#endif // __clang_major__ >= 4

#endif // __GNUC__

#else // defined(__has_attribute)
#define BC_FALLTHROUGH
#endif // defined(__has_attribute)
#else // defined(__clang__) || defined(__GNUC__)
#define BC_FALLTHROUGH
#endif // defined(__clang__) || defined(__GNUC__)

#if BC_HAS_UNREACHABLE

#define BC_UNREACHABLE __builtin_unreachable();

#else // BC_HAS_UNREACHABLE

#ifdef _WIN32

#define BC_UNREACHABLE __assume(0);

#else // _WIN32

#define BC_UNREACHABLE

#endif // _WIN32

#endif // BC_HAS_UNREACHABLE

#ifdef __GNUC__

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (1)

#endif // __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5)

#endif // __GNUC__

#ifdef __clang__

#if __clang_major__ >= 4

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (1)

#endif // __clang_major__ >= 4

#endif // __GNUC__

#ifdef BC_NO_COMPUTED_GOTO

#undef BC_HAS_COMPUTED_GOTO
#define BC_HAS_COMPUTED_GOTO (0)

#endif // BC_NO_COMPUTED_GOTO

#ifdef __GNUC__
#ifdef __OpenBSD__
// The OpenBSD GCC doesn't like inline.
#define inline
#endif // __OpenBSD__
#endif // __GNUC__

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

/// Statuses, which mark either which category of error happened, or some other
/// status that matters.
typedef enum BcStatus {

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
typedef enum BcErr {

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

	// Parse (and lex errors).

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

/// Do a longjmp(). This is what to use when activating an "exception", i.e., a
/// longjmp(). With debug code, it will print the name of the function it jumped
/// from.
#if BC_DEBUG_CODE
#define BC_JMP bc_vm_jmp(__func__)
#else // BC_DEBUG_CODE
#define BC_JMP bc_vm_jmp()
#endif // BC_DEBUG_CODE

/// Returns true if an exception is in flight, false otherwise.
#define BC_SIG_EXC \
	BC_UNLIKELY(vm.status != (sig_atomic_t) BC_STATUS_SUCCESS || vm.sig)

/// Returns true if there is *no* exception in flight, false otherwise.
#define BC_NO_SIG_EXC \
	BC_LIKELY(vm.status == (sig_atomic_t) BC_STATUS_SUCCESS && !vm.sig)

#ifndef NDEBUG

/// Assert that signals are locked. There are non-async-signal-safe functions in
/// bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are locked.
#define BC_SIG_ASSERT_LOCKED do { assert(vm.sig_lock); } while (0)

/// Assert that signals are unlocked. There are non-async-signal-safe functions
/// in bc, and they *must* have signals locked. Other functions are expected to
/// *not* have signals locked, for reasons. So this is a pre-built assert
/// (no-op in non-debug mode) that check that signals are unlocked.
#define BC_SIG_ASSERT_NOT_LOCKED do { assert(vm.sig_lock == 0); } while (0)

#else // NDEBUG

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

#endif // NDEBUG

/// Locks signals.
#define BC_SIG_LOCK               \
	do {                          \
		BC_SIG_ASSERT_NOT_LOCKED; \
		vm.sig_lock = 1;          \
	} while (0)

/// Unlocks signals. If a signal happened, then this will cause a jump.
#define BC_SIG_UNLOCK         \
	do {                      \
		BC_SIG_ASSERT_LOCKED; \
		vm.sig_lock = 0;      \
		if (vm.sig) BC_JMP;   \
	} while (0)

/// Locks signals, regardless of if they are already locked. This is really only
/// used after labels that longjmp() goes to after the jump because the cleanup
/// code must have signals locked, and BC_LONGJMP_CONT will unlock signals if it
/// doesn't jump.
#define BC_SIG_MAYLOCK   \
	do {                 \
		vm.sig_lock = 1; \
	} while (0)

/// Unlocks signals, regardless of if they were already unlocked. If a signal
/// happened, then this will cause a jump.
#define BC_SIG_MAYUNLOCK    \
	do {                    \
		vm.sig_lock = 0;    \
		if (vm.sig) BC_JMP; \
	} while (0)

/*
 * Locks signals, but stores the old lock state, to be restored later by
 * BC_SIG_TRYUNLOCK.
 * @param v  The variable to store the old lock state to.
 */
#define BC_SIG_TRYLOCK(v) \
	do {                  \
		v = vm.sig_lock;  \
		vm.sig_lock = 1;  \
	} while (0)

/* Restores the previous state of a signal lock, and if it is now unlocked,
 * initiates an exception/jump.
 * @param v  The old lock state.
 */
#define BC_SIG_TRYUNLOCK(v)         \
	do {                            \
		vm.sig_lock = (v);          \
		if (!(v) && vm.sig) BC_JMP; \
	} while (0)

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
#define BC_SETJMP(l)                     \
	do {                                 \
		sigjmp_buf sjb;                  \
		BC_SIG_LOCK;                     \
		bc_vec_grow(&vm.jmp_bufs, 1);    \
		if (sigsetjmp(sjb, 0)) {         \
			assert(BC_SIG_EXC);          \
			goto l;                      \
		}                                \
		bc_vec_push(&vm.jmp_bufs, &sjb); \
		BC_SIG_UNLOCK;                   \
	} while (0)

/**
 * Sets a jump like BC_SETJMP, but unlike BC_SETJMP, it assumes signals are
 * locked and will just set the jump. This does *not* have a call to
 * bc_vec_grow() because it is assumed that BC_SETJMP_LOCKED(l) is used *after*
 * the initializations that need the setjmp().
 * param l  The label to jump to on a longjmp().
 */
#define BC_SETJMP_LOCKED(l)              \
	do {                                 \
		sigjmp_buf sjb;                  \
		BC_SIG_ASSERT_LOCKED;            \
		if (sigsetjmp(sjb, 0)) {         \
			assert(BC_SIG_EXC);          \
			goto l;                      \
		}                                \
		bc_vec_push(&vm.jmp_bufs, &sjb); \
	} while (0)

/// Used after cleanup labels set by BC_SETJMP and BC_SETJMP_LOCKED to jump to
/// the next place. This is what continues the stack unwinding. This basically
/// copies BC_SIG_UNLOCK into itself, but that is because its condition for
/// jumping is BC_SIG_EXC, not just that a signal happened.
#define BC_LONGJMP_CONT                            \
	do {                                           \
		BC_SIG_ASSERT_LOCKED;                      \
		if (!vm.sig_pop) bc_vec_pop(&vm.jmp_bufs); \
		vm.sig_lock = 0;                           \
		if (BC_SIG_EXC) BC_JMP;                    \
	} while (0)

/// Unsets a jump. It always assumes signals are locked. This basically just
/// pops a jmp_buf off of the stack of jmp_bufs, and since the jump mechanism
/// always jumps to the location at the top of the stack, this effectively
/// undoes a setjmp().
#define BC_UNSETJMP               \
	do {                          \
		BC_SIG_ASSERT_LOCKED;     \
		bc_vec_pop(&vm.jmp_bufs); \
	} while (0)

/// Stops a stack unwinding. Technically, a stack unwinding needs to be done
/// manually, but it will always be done unless certain flags are cleared. This
/// clears the flags.
#define BC_LONGJMP_STOP \
	do {                \
		vm.sig_pop = 0; \
		vm.sig = 0;     \
	} while (0)

// Various convenience macros for calling the bc's error handling routine.
#if BC_ENABLE_LIBRARY

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

/**
 * Call bc's error handling routine.
 * @param e    The error.
 * @param l    The line of the script that the error happened.
 * @param ...  Extra arguments for error messages as necessary.
 */
#define bc_error(e, l, ...) (bc_vm_handleError((e), (l), __VA_ARGS__))

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#define bc_err(e) (bc_vm_handleError((e), 0))

/**
 * Call bc's error handling routine.
 * @param e  The error.
 */
#define bc_verr(e, ...) (bc_vm_handleError((e), 0, __VA_ARGS__))

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
#define BC_FUNC_ENTER                                              \
	do {                                                           \
		size_t bc_func_enter_i;                                    \
		for (bc_func_enter_i = 0; bc_func_enter_i < vm.func_depth; \
		     ++bc_func_enter_i)                                    \
		{                                                          \
			bc_file_puts(&vm.ferr, bc_flush_none, "  ");           \
		}                                                          \
		vm.func_depth += 1;                                        \
		bc_file_printf(&vm.ferr, "Entering %s\n", __func__);       \
		bc_file_flush(&vm.ferr, bc_flush_none);                    \
	} while (0);

#define BC_FUNC_EXIT                                               \
	do {                                                           \
		size_t bc_func_enter_i;                                    \
		vm.func_depth -= 1;                                        \
		for (bc_func_enter_i = 0; bc_func_enter_i < vm.func_depth; \
		     ++bc_func_enter_i)                                    \
		{                                                          \
			bc_file_puts(&vm.ferr, bc_flush_none, "  ");           \
		}                                                          \
		bc_file_printf(&vm.ferr, "Leaving %s\n", __func__);        \
		bc_file_flush(&vm.ferr, bc_flush_none);                    \
	} while (0);
#else // BC_DEBUG_CODE
#define BC_FUNC_ENTER
#define BC_FUNC_EXIT
#endif // BC_DEBUG_CODE

#endif // BC_STATUS_H
