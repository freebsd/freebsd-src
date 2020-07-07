/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
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

#include <stddef.h>
#include <limits.h>

#include <signal.h>

#if BC_ENABLE_NLS

#	ifdef _WIN32
#	error NLS is not supported on Windows.
#	endif // _WIN32

#include <nl_types.h>

#endif // BC_ENABLE_NLS

#include <status.h>
#include <num.h>
#include <parse.h>
#include <program.h>
#include <history.h>
#include <file.h>

#if !BC_ENABLED && !DC_ENABLED
#error Must define BC_ENABLED, DC_ENABLED, or both
#endif

// CHAR_BIT must be at least 6.
#if CHAR_BIT < 6
#error CHAR_BIT must be at least 6.
#endif

#ifndef BC_ENABLE_NLS
#define BC_ENABLE_NLS (0)
#endif // BC_ENABLE_NLS

#ifndef MAINEXEC
#define MAINEXEC bc
#endif

#ifndef EXECPREFIX
#define EXECPREFIX
#endif

#define GEN_STR(V) #V
#define GEN_STR2(V) GEN_STR(V)

#define BC_VERSION GEN_STR2(VERSION)
#define BC_EXECPREFIX GEN_STR2(EXECPREFIX)
#define BC_MAINEXEC GEN_STR2(MAINEXEC)

// Windows has deprecated isatty().
#ifdef _WIN32
#define isatty _isatty
#endif // _WIN32

#if DC_ENABLED
#define DC_FLAG_X (UINTMAX_C(1)<<0)
#endif // DC_ENABLED

#if BC_ENABLED
#define BC_FLAG_W (UINTMAX_C(1)<<1)
#define BC_FLAG_S (UINTMAX_C(1)<<2)
#define BC_FLAG_L (UINTMAX_C(1)<<3)
#define BC_FLAG_G (UINTMAX_C(1)<<4)
#endif // BC_ENABLED

#define BC_FLAG_Q (UINTMAX_C(1)<<5)
#define BC_FLAG_I (UINTMAX_C(1)<<6)
#define BC_FLAG_P (UINTMAX_C(1)<<7)
#define BC_FLAG_TTYIN (UINTMAX_C(1)<<8)
#define BC_FLAG_TTY (UINTMAX_C(1)<<9)
#define BC_TTYIN (vm.flags & BC_FLAG_TTYIN)
#define BC_TTY (vm.flags & BC_FLAG_TTY)

#if BC_ENABLED

#define BC_S (BC_ENABLED && (vm.flags & BC_FLAG_S))
#define BC_W (BC_ENABLED && (vm.flags & BC_FLAG_W))
#define BC_L (BC_ENABLED && (vm.flags & BC_FLAG_L))
#define BC_G (BC_ENABLED && (vm.flags & BC_FLAG_G))

#endif // BC_ENABLED

#if DC_ENABLED
#define DC_X (vm.flags & DC_FLAG_X)
#endif // DC_ENABLED

#define BC_I (vm.flags & BC_FLAG_I)
#define BC_P (vm.flags & BC_FLAG_P)

#if BC_ENABLED

#define BC_IS_POSIX (BC_S || BC_W)

#if DC_ENABLED
#define BC_IS_BC (vm.name[0] != 'd')
#define BC_IS_DC (vm.name[0] == 'd')
#else // DC_ENABLED
#define BC_IS_BC (1)
#define BC_IS_DC (0)
#endif // DC_ENABLED

#else // BC_ENABLED
#define BC_IS_POSIX (0)
#define BC_IS_BC (0)
#define BC_IS_DC (1)
#endif // BC_ENABLED

#if BC_ENABLED
#define BC_USE_PROMPT (!BC_P && BC_TTY && !BC_IS_POSIX)
#else // BC_ENABLED
#define BC_USE_PROMPT (!BC_P && BC_TTY)
#endif // BC_ENABLED

#define BC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BC_MIN(a, b) ((a) < (b) ? (a) : (b))

#define BC_MAX_OBASE ((BcBigDig) (BC_BASE_POW))
#define BC_MAX_DIM ((BcBigDig) (SIZE_MAX - 1))
#define BC_MAX_SCALE ((BcBigDig) (BC_NUM_BIGDIG_MAX - 1))
#define BC_MAX_STRING ((BcBigDig) (BC_NUM_BIGDIG_MAX - 1))
#define BC_MAX_NAME BC_MAX_STRING
#define BC_MAX_NUM BC_MAX_SCALE

#if BC_ENABLE_EXTRA_MATH
#define BC_MAX_RAND ((BcBigDig) (((BcRand) 0) - 1))
#endif // BC_ENABLE_EXTRA_MATH

#define BC_MAX_EXP ((ulong) (BC_NUM_BIGDIG_MAX))
#define BC_MAX_VARS ((ulong) (SIZE_MAX - 1))

#if BC_DEBUG_CODE
#define BC_VM_JMP bc_vm_jmp(__func__)
#else // BC_DEBUG_CODE
#define BC_VM_JMP bc_vm_jmp()
#endif // BC_DEBUG_CODE

#define BC_SIG_EXC \
	BC_UNLIKELY(vm.status != (sig_atomic_t) BC_STATUS_SUCCESS || vm.sig)
#define BC_NO_SIG_EXC \
	BC_LIKELY(vm.status == (sig_atomic_t) BC_STATUS_SUCCESS && !vm.sig)

#ifndef NDEBUG
#define BC_SIG_ASSERT_LOCKED do { assert(vm.sig_lock); } while (0)
#define BC_SIG_ASSERT_NOT_LOCKED do { assert(vm.sig_lock == 0); } while (0)
#else // NDEBUG
#define BC_SIG_ASSERT_LOCKED
#define BC_SIG_ASSERT_NOT_LOCKED
#endif // NDEBUG

#define BC_SIG_LOCK               \
	do {                          \
		BC_SIG_ASSERT_NOT_LOCKED; \
		vm.sig_lock = 1;          \
	} while (0)

#define BC_SIG_UNLOCK              \
	do {                           \
		BC_SIG_ASSERT_LOCKED;      \
		vm.sig_lock = 0;           \
		if (BC_SIG_EXC) BC_VM_JMP; \
	} while (0)

#define BC_SIG_MAYLOCK   \
	do {                 \
		vm.sig_lock = 1; \
	} while (0)

#define BC_SIG_MAYUNLOCK           \
	do {                           \
		vm.sig_lock = 0;           \
		if (BC_SIG_EXC) BC_VM_JMP; \
	} while (0)

#define BC_SIG_TRYLOCK(v) \
	do {                  \
		v = vm.sig_lock;  \
		vm.sig_lock = 1;  \
	} while (0)

#define BC_SIG_TRYUNLOCK(v)                \
	do {                                   \
		vm.sig_lock = (v);                 \
		if (!(v) && BC_SIG_EXC) BC_VM_JMP; \
	} while (0)

#define BC_SETJMP(l)                     \
	do {                                 \
		sigjmp_buf sjb;                  \
		BC_SIG_LOCK;                     \
		if (sigsetjmp(sjb, 0)) {         \
			assert(BC_SIG_EXC);          \
			goto l;                      \
		}                                \
		bc_vec_push(&vm.jmp_bufs, &sjb); \
		BC_SIG_UNLOCK;                   \
	} while (0)

#define BC_SETJMP_LOCKED(l)               \
	do {                                  \
		sigjmp_buf sjb;                   \
		BC_SIG_ASSERT_LOCKED;             \
		if (sigsetjmp(sjb, 0)) {          \
			assert(BC_SIG_EXC);           \
			goto l;                       \
		}                                 \
		bc_vec_push(&vm.jmp_bufs, &sjb);  \
	} while (0)

#define BC_LONGJMP_CONT                             \
	do {                                            \
		BC_SIG_ASSERT_LOCKED;                       \
		if (!vm.sig_pop) bc_vec_pop(&vm.jmp_bufs);  \
		BC_SIG_UNLOCK;                              \
	} while (0)

#define BC_UNSETJMP               \
	do {                          \
		BC_SIG_ASSERT_LOCKED;     \
		bc_vec_pop(&vm.jmp_bufs); \
	} while (0)

#define BC_LONGJMP_STOP    \
	do {                   \
		vm.sig_pop = 0;    \
		vm.sig = 0;        \
	} while (0)

#define BC_VM_BUF_SIZE (1<<12)
#define BC_VM_STDOUT_BUF_SIZE (1<<11)
#define BC_VM_STDERR_BUF_SIZE (1<<10)
#define BC_VM_STDIN_BUF_SIZE (BC_VM_STDERR_BUF_SIZE - 1)

#define BC_VM_SAFE_RESULT(r) ((r)->t >= BC_RESULT_TEMP)

#define bc_vm_err(e) (bc_vm_error((e), 0))
#define bc_vm_verr(e, ...) (bc_vm_error((e), 0, __VA_ARGS__))

#define BC_STATUS_IS_ERROR(s) \
	((s) >= BC_STATUS_ERROR_MATH && (s) <= BC_STATUS_ERROR_FATAL)

#define BC_VM_INVALID_CATALOG ((nl_catd) -1)

// dc does not use is_stdin.
#if !BC_ENABLED
#define bc_vm_process(text, is_stdin) bc_vm_process(text)
#else // BC_ENABLED
#endif // BC_ENABLED

typedef struct BcVm {

	volatile sig_atomic_t status;
	volatile sig_atomic_t sig_pop;

	BcParse prs;
	BcProgram prog;

	BcVec jmp_bufs;

	BcVec temps;

	const char* file;

	const char *sigmsg;
	volatile sig_atomic_t sig_lock;
	volatile sig_atomic_t sig;
	uchar siglen;

	uchar read_ret;
	uint16_t flags;

	uint16_t nchars;
	uint16_t line_len;

	bool eof;

	BcBigDig maxes[BC_PROG_GLOBALS_LEN + BC_ENABLE_EXTRA_MATH];

	BcVec files;
	BcVec exprs;

	const char *name;
	const char *help;

#if BC_ENABLE_HISTORY
	BcHistory history;
#endif // BC_ENABLE_HISTORY

	BcLexNext next;
	BcParseParse parse;
	BcParseExpr expr;

	const char *func_header;

	const char *err_ids[BC_ERR_IDX_NELEMS + BC_ENABLED];
	const char *err_msgs[BC_ERROR_NELEMS];

	const char *locale;

	BcBigDig last_base;
	BcBigDig last_pow;
	BcBigDig last_exp;
	BcBigDig last_rem;

	char *env_args_buffer;
	BcVec env_args;

	BcNum max;
	BcDig max_num[BC_NUM_BIGDIG_LOG10];

	BcFile fout;
	BcFile ferr;

#if BC_ENABLE_NLS
	nl_catd catalog;
#endif // BC_ENABLE_NLS

	char *buf;
	size_t buf_len;

} BcVm;

void bc_vm_info(const char* const help);
void bc_vm_boot(int argc, char *argv[], const char *env_len,
                const char* const env_args, const char* env_exp_quit);
void bc_vm_shutdown(void);

void bc_vm_printf(const char *fmt, ...);
void bc_vm_putchar(int c);
size_t bc_vm_arraySize(size_t n, size_t size);
size_t bc_vm_growSize(size_t a, size_t b);
void* bc_vm_malloc(size_t n);
void* bc_vm_realloc(void *ptr, size_t n);
char* bc_vm_strdup(const char *str);

#if BC_DEBUG_CODE
void bc_vm_jmp(const char *f);
#else // BC_DEBUG_CODE
void bc_vm_jmp(void);
#endif // BC_DEBUG_CODE

void bc_vm_error(BcError e, size_t line, ...);

extern const char bc_copyright[];
extern const char* const bc_err_line;
extern const char* const bc_err_func_header;
extern const char *bc_errs[];
extern const uchar bc_err_ids[];
extern const char* const bc_err_msgs[];

extern BcVm vm;
extern char output_bufs[BC_VM_BUF_SIZE];

#endif // BC_VM_H
