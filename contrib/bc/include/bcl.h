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
 * The public header for the bc library.
 *
 */

#ifndef BC_BCL_H
#define BC_BCL_H

#ifdef _WIN32
#include <Windows.h>
#include <BaseTsd.h>
#include <stdio.h>
#include <io.h>
#endif // _WIN32

#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#define BC_SEED_ULONGS (4)
#define BC_SEED_SIZE (sizeof(long) * BC_SEED_ULONGS)

// For some reason, LONG_BIT is not defined in some versions of gcc.
// I define it here to the minimum accepted value in the POSIX standard.
#ifndef LONG_BIT
#define LONG_BIT (32)
#endif // LONG_BIT

#ifndef BC_LONG_BIT
#define BC_LONG_BIT LONG_BIT
#endif // BC_LONG_BIT

#if BC_LONG_BIT > LONG_BIT
#error BC_LONG_BIT cannot be greater than LONG_BIT
#endif // BC_LONG_BIT > LONG_BIT

#if BC_LONG_BIT >= 64

typedef uint64_t BclBigDig;
typedef uint64_t BclRandInt;

#elif BC_LONG_BIT >= 32

typedef uint32_t BclBigDig;
typedef uint32_t BclRandInt;

#else

#error BC_LONG_BIT must be at least 32

#endif // BC_LONG_BIT >= 64
#define BC_UNUSED(e) ((void) (e))

#ifndef BC_LIKELY
#define BC_LIKELY(e) (e)
#endif // BC_LIKELY

#ifndef BC_UNLIKELY
#define BC_UNLIKELY(e) (e)
#endif // BC_UNLIKELY

#define BC_ERR(e) BC_UNLIKELY(e)
#define BC_NO_ERR(s) BC_LIKELY(s)

#ifndef BC_DEBUG_CODE
#define BC_DEBUG_CODE (0)
#endif // BC_DEBUG_CODE

#if __STDC_VERSION__ >= 201100L
#include <stdnoreturn.h>
#define BC_NORETURN _Noreturn
#else // __STDC_VERSION__
#define BC_NORETURN
#define BC_MUST_RETURN
#endif // __STDC_VERSION__

#if defined(__clang__) || defined(__GNUC__)
#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define BC_FALLTHROUGH __attribute__((fallthrough));
#else // __has_attribute(fallthrough)
#define BC_FALLTHROUGH
#endif // __has_attribute(fallthrough)
#else // defined(__has_attribute)
#define BC_FALLTHROUGH
#endif // defined(__has_attribute)
#else // defined(__clang__) || defined(__GNUC__)
#define BC_FALLTHROUGH
#endif // defined(__clang__) || defined(__GNUC__)

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

// Windows has deprecated isatty() and the rest of these.
// Or doesn't have them.
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
#define open(f, n, m) _sopen_s(f, n, m, _SH_DENYNO, _S_IREAD | _S_IWRITE)
#define sigjmp_buf jmp_buf
#define sigsetjmp(j, s) setjmp(j)
#define siglongjmp longjmp
#define isatty _isatty
#define STDIN_FILENO (0)
#define STDOUT_FILENO (1)
#define STDERR_FILENO (2)
#define ssize_t SSIZE_T
#define S_ISDIR(m) ((m) & _S_IFDIR)
#define O_RDONLY _O_RDONLY
#define stat _stat
#define fstat _fstat
#define BC_FILE_SEP '\\'

#else // _WIN32
#define BC_FILE_SEP '/'
#endif // _WIN32

#if BC_ENABLE_LIBRARY

typedef enum BclError {

	BCL_ERROR_NONE,

	BCL_ERROR_INVALID_NUM,
	BCL_ERROR_INVALID_CONTEXT,
	BCL_ERROR_SIGNAL,

	BCL_ERROR_MATH_NEGATIVE,
	BCL_ERROR_MATH_NON_INTEGER,
	BCL_ERROR_MATH_OVERFLOW,
	BCL_ERROR_MATH_DIVIDE_BY_ZERO,

	BCL_ERROR_PARSE_INVALID_STR,

	BCL_ERROR_FATAL_ALLOC_ERR,
	BCL_ERROR_FATAL_UNKNOWN_ERR,

	BCL_ERROR_NELEMS,

} BclError;

typedef struct BclNumber {

	size_t i;

} BclNumber;

struct BclCtxt;

typedef struct BclCtxt* BclContext;

void bcl_handleSignal(void);
bool bcl_running(void);

BclError bcl_init(void);
void bcl_free(void);

bool bcl_abortOnFatalError(void);
void bcl_setAbortOnFatalError(bool abrt);

void bcl_gc(void);

BclError bcl_pushContext(BclContext ctxt);
void bcl_popContext(void);
BclContext bcl_context(void);

BclContext bcl_ctxt_create(void);
void bcl_ctxt_free(BclContext ctxt);
void bcl_ctxt_freeNums(BclContext ctxt);

size_t bcl_ctxt_scale(BclContext ctxt);
void bcl_ctxt_setScale(BclContext ctxt, size_t scale);
size_t bcl_ctxt_ibase(BclContext ctxt);
void bcl_ctxt_setIbase(BclContext ctxt, size_t ibase);
size_t bcl_ctxt_obase(BclContext ctxt);
void bcl_ctxt_setObase(BclContext ctxt, size_t obase);

BclError bcl_err(BclNumber n);

BclNumber bcl_num_create(void);
void bcl_num_free(BclNumber n);

bool bcl_num_neg(BclNumber n);
void bcl_num_setNeg(BclNumber n, bool neg);
size_t bcl_num_scale(BclNumber n);
BclError bcl_num_setScale(BclNumber n, size_t scale);
size_t bcl_num_len(BclNumber n);

BclError bcl_copy(BclNumber d, BclNumber s);
BclNumber bcl_dup(BclNumber s);

BclError bcl_bigdig(BclNumber n, BclBigDig *result);
BclNumber bcl_bigdig2num(BclBigDig val);

BclNumber bcl_add(BclNumber a, BclNumber b);
BclNumber bcl_sub(BclNumber a, BclNumber b);
BclNumber bcl_mul(BclNumber a, BclNumber b);
BclNumber bcl_div(BclNumber a, BclNumber b);
BclNumber bcl_mod(BclNumber a, BclNumber b);
BclNumber bcl_pow(BclNumber a, BclNumber b);
BclNumber bcl_lshift(BclNumber a, BclNumber b);
BclNumber bcl_rshift(BclNumber a, BclNumber b);
BclNumber bcl_sqrt(BclNumber a);
BclError bcl_divmod(BclNumber a, BclNumber b, BclNumber *c, BclNumber *d);
BclNumber bcl_modexp(BclNumber a, BclNumber b, BclNumber c);

ssize_t bcl_cmp(BclNumber a, BclNumber b);

void bcl_zero(BclNumber n);
void bcl_one(BclNumber n);

BclNumber bcl_parse(const char *restrict val);
char* bcl_string(BclNumber n);

BclNumber bcl_irand(BclNumber a);
BclNumber bcl_frand(size_t places);
BclNumber bcl_ifrand(BclNumber a, size_t places);

BclError bcl_rand_seedWithNum(BclNumber n);
BclError bcl_rand_seed(unsigned char seed[BC_SEED_SIZE]);
void bcl_rand_reseed(void);
BclNumber bcl_rand_seed2num(void);
BclRandInt bcl_rand_int(void);
BclRandInt bcl_rand_bounded(BclRandInt bound);

#endif // BC_ENABLE_LIBRARY

#endif // BC_BCL_H
