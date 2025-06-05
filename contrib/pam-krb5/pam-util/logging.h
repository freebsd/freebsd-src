/*
 * Interface to standard PAM logging.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2010, 2012-2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAM_UTIL_LOGGING_H
#define PAM_UTIL_LOGGING_H 1

#include <config.h>
#include <portable/macros.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/pam.h>

#include <stddef.h>
#include <syslog.h>

/* Forward declarations to avoid extra includes. */
struct pam_args;

BEGIN_DECLS

/* Default to a hidden visibility for all internal functions. */
#pragma GCC visibility push(hidden)

/*
 * Error reporting and debugging functions.  For each log level, there are two
 * functions.  The _log function just prints out the message it's given.  The
 * _log_pam function does the same but appends the pam_strerror results for
 * the provided status code if it is not PAM_SUCCESS.
 */
void putil_crit(struct pam_args *, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
void putil_crit_pam(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_err(struct pam_args *, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
void putil_err_pam(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_notice(struct pam_args *, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
void putil_notice_pam(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_debug(struct pam_args *, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
void putil_debug_pam(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));

/*
 * The Kerberos versions of the PAM logging and debugging functions, which
 * report the last Kerberos error.  These are only available if built with
 * Kerberos support.
 */
#ifdef HAVE_KRB5
void putil_crit_krb5(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_err_krb5(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_notice_krb5(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
void putil_debug_krb5(struct pam_args *, int, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif

/* Log entry to a PAM function. */
void putil_log_entry(struct pam_args *, const char *, int flags)
    __attribute__((__nonnull__));

/* Log an authentication failure. */
void putil_log_failure(struct pam_args *, const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 2, 3)));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

/* __func__ is C99, but not provided by all implementations. */
#if (__STDC_VERSION__ < 199901L) && !defined(__func__)
#    if (__GNUC__ >= 2)
#        define __func__ __FUNCTION__
#    else
#        define __func__ "<unknown>"
#    endif
#endif

/* Macros to record entry and exit from the main PAM functions. */
#define ENTRY(args, flags)                              \
    do {                                                \
        if (args->debug)                                \
            putil_log_entry((args), __func__, (flags)); \
    } while (0)
#define EXIT(args, pamret)                                                \
    do {                                                                  \
        if (args != NULL && args->debug)                                  \
            pam_syslog(                                                   \
                (args)->pamh, LOG_DEBUG, "%s: exit (%s)", __func__,       \
                ((pamret) == PAM_SUCCESS)                                 \
                    ? "success"                                           \
                    : (((pamret) == PAM_IGNORE) ? "ignore" : "failure")); \
    } while (0)

#endif /* !PAM_UTIL_LOGGING_H */
