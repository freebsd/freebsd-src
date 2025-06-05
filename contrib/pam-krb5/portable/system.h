/*
 * Standard system includes and portability adjustments.
 *
 * Declarations of routines and variables in the C library.  Including this
 * file is the equivalent of including all of the following headers,
 * portably:
 *
 *     #include <inttypes.h>
 *     #include <limits.h>
 *     #include <stdarg.h>
 *     #include <stdbool.h>
 *     #include <stddef.h>
 *     #include <stdio.h>
 *     #include <stdlib.h>
 *     #include <stdint.h>
 *     #include <string.h>
 *     #include <strings.h>
 *     #include <sys/types.h>
 *     #include <unistd.h>
 *
 * Missing functions are provided via #define or prototyped if available from
 * the portable helper library.  Also provides some standard #defines.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014, 2016, 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2011, 2013-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_SYSTEM_H
#define PORTABLE_SYSTEM_H 1

/* Make sure we have our configuration information. */
#include <config.h>

/* BEGIN_DECL and __attribute__. */
#include <portable/macros.h>

/* A set of standard ANSI C headers.  We don't care about pre-ANSI systems. */
#if HAVE_INTTYPES_H
#    include <inttypes.h>
#endif
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#if HAVE_STDINT_H
#    include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_STRINGS_H
#    include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_UNISTD_H
#    include <unistd.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
#    include <sys/bitypes.h>
#endif

/* Get the bool type. */
#include <portable/stdbool.h>

/* Windows provides snprintf under a different name. */
#ifdef _WIN32
#    define snprintf _snprintf
#endif

/* Windows does not define ssize_t. */
#ifndef HAVE_SSIZE_T
typedef ptrdiff_t ssize_t;
#endif

/*
 * POSIX requires that these be defined in <unistd.h>.  If one of them has
 * been defined, all the rest almost certainly have.
 */
#ifndef STDIN_FILENO
#    define STDIN_FILENO  0
#    define STDOUT_FILENO 1
#    define STDERR_FILENO 2
#endif

/*
 * C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
 * Autoconf manual, memcpy is a generally portable fallback.
 */
#ifndef va_copy
#    ifdef __va_copy
#        define va_copy(d, s) __va_copy((d), (s))
#    else
#        define va_copy(d, s) memcpy(&(d), &(s), sizeof(va_list))
#    endif
#endif

/*
 * If explicit_bzero is not available, fall back on memset.  This does NOT
 * provide any of the security guarantees of explicit_bzero and will probably
 * be optimized away by the compiler.  It just ensures that code will compile
 * and function on systems without explicit_bzero.
 */
#if !HAVE_EXPLICIT_BZERO
#    define explicit_bzero(s, n) memset((s), 0, (n))
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/*
 * Provide prototypes for functions not declared in system headers.  Use the
 * HAVE_DECL macros for those functions that may be prototyped but implemented
 * incorrectly or implemented without a prototype.
 */
#if !HAVE_ASPRINTF
extern int asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
extern int vasprintf(char **, const char *, va_list)
    __attribute__((__format__(printf, 2, 0)));
#endif
#if !HAVE_ISSETUGID
extern int issetugid(void);
#endif
#if !HAVE_MKSTEMP
extern int mkstemp(char *);
#endif
#if !HAVE_DECL_REALLOCARRAY
extern void *reallocarray(void *, size_t, size_t);
#endif
#if !HAVE_STRNDUP
extern char *strndup(const char *, size_t);
#endif

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PORTABLE_SYSTEM_H */
