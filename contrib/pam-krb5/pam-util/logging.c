/*
 * Logging functions for PAM modules.
 *
 * Logs errors and debugging messages from PAM modules.  The debug versions
 * only log anything if debugging was enabled; the crit and err versions
 * always log.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015, 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2005-2007, 2009-2010, 2012-2013
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

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/pam.h>
#include <portable/system.h>

#include <syslog.h>

#include <pam-util/args.h>
#include <pam-util/logging.h>

#ifndef LOG_AUTHPRIV
#    define LOG_AUTHPRIV LOG_AUTH
#endif

/* Used for iterating through arrays. */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/*
 * Mappings of PAM flags to symbolic names for logging when entering a PAM
 * module function.
 */
static const struct {
    int flag;
    const char *name;
} FLAGS[] = {
    /* clang-format off */
    {PAM_CHANGE_EXPIRED_AUTHTOK, "expired"  },
    {PAM_DELETE_CRED,            "delete"   },
    {PAM_DISALLOW_NULL_AUTHTOK,  "nonull"   },
    {PAM_ESTABLISH_CRED,         "establish"},
    {PAM_PRELIM_CHECK,           "prelim"   },
    {PAM_REFRESH_CRED,           "refresh"  },
    {PAM_REINITIALIZE_CRED,      "reinit"   },
    {PAM_SILENT,                 "silent"   },
    {PAM_UPDATE_AUTHTOK,         "update"   },
    /* clang-format on */
};


/*
 * Utility function to format a message into newly allocated memory, reporting
 * an error via syslog if vasprintf fails.
 */
static char *__attribute__((__format__(printf, 1, 0)))
format(const char *fmt, va_list args)
{
    char *msg;

    if (vasprintf(&msg, fmt, args) < 0) {
        syslog(LOG_CRIT | LOG_AUTHPRIV, "vasprintf failed: %m");
        return NULL;
    }
    return msg;
}


/*
 * Log wrapper function that adds the user.  Log a message with the given
 * priority, prefixed by (user <user>) with the account name being
 * authenticated if known.
 */
static void __attribute__((__format__(printf, 3, 0)))
log_vplain(struct pam_args *pargs, int priority, const char *fmt, va_list args)
{
    char *msg;

    if (priority == LOG_DEBUG && (pargs == NULL || !pargs->debug))
        return;
    if (pargs != NULL && pargs->user != NULL) {
        msg = format(fmt, args);
        if (msg == NULL)
            return;
        pam_syslog(pargs->pamh, priority, "(user %s) %s", pargs->user, msg);
        free(msg);
    } else if (pargs != NULL) {
        pam_vsyslog(pargs->pamh, priority, fmt, args);
    } else {
        msg = format(fmt, args);
        if (msg == NULL)
            return;
        syslog(priority | LOG_AUTHPRIV, "%s", msg);
        free(msg);
    }
}


/*
 * Wrapper around log_vplain with variadic arguments.
 */
static void __attribute__((__format__(printf, 3, 4)))
log_plain(struct pam_args *pargs, int priority, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    log_vplain(pargs, priority, fmt, args);
    va_end(args);
}


/*
 * Log wrapper function for reporting a PAM error.  Log a message with the
 * given priority, prefixed by (user <user>) with the account name being
 * authenticated if known, followed by a colon and the formatted PAM error.
 * However, do not include the colon and the PAM error if the PAM status is
 * PAM_SUCCESS.
 */
static void __attribute__((__format__(printf, 4, 0)))
log_pam(struct pam_args *pargs, int priority, int status, const char *fmt,
        va_list args)
{
    char *msg;

    if (priority == LOG_DEBUG && (pargs == NULL || !pargs->debug))
        return;
    msg = format(fmt, args);
    if (msg == NULL)
        return;
    if (pargs == NULL)
        log_plain(NULL, priority, "%s", msg);
    else if (status == PAM_SUCCESS)
        log_plain(pargs, priority, "%s", msg);
    else
        log_plain(pargs, priority, "%s: %s", msg,
                  pam_strerror(pargs->pamh, status));
    free(msg);
}


/*
 * The public interfaces.  For each common log level (crit, err, and debug),
 * generate a putil_<level> function and one for _pam.  Do this with the
 * preprocessor to save duplicate code.
 */
/* clang-format off */
#define LOG_FUNCTION(level, priority)                             \
    void __attribute__((__format__(printf, 2, 3)))                \
    putil_ ## level(struct pam_args *pargs, const char *fmt, ...) \
    {                                                             \
        va_list args;                                             \
                                                                  \
        va_start(args, fmt);                                      \
        log_vplain(pargs, priority, fmt, args);                   \
        va_end(args);                                             \
    }                                                             \
    void __attribute__((__format__(printf, 3, 4)))                \
    putil_ ## level ## _pam(struct pam_args *pargs, int status,   \
                            const char *fmt, ...)                 \
    {                                                             \
        va_list args;                                             \
                                                                  \
        va_start(args, fmt);                                      \
        log_pam(pargs, priority, status, fmt, args);              \
        va_end(args);                                             \
    }
LOG_FUNCTION(crit,   LOG_CRIT)
LOG_FUNCTION(err,    LOG_ERR)
LOG_FUNCTION(notice, LOG_NOTICE)
LOG_FUNCTION(debug,  LOG_DEBUG)
/* clang-format on */


/*
 * Report entry into a function.  Takes the PAM arguments, the function name,
 * and the flags and maps the flags to symbolic names.
 */
void
putil_log_entry(struct pam_args *pargs, const char *func, int flags)
{
    size_t i, length, offset;
    char *out = NULL, *nout;

    if (!pargs->debug)
        return;
    if (flags != 0)
        for (i = 0; i < ARRAY_SIZE(FLAGS); i++) {
            if (!(flags & FLAGS[i].flag))
                continue;
            if (out == NULL) {
                out = strdup(FLAGS[i].name);
                if (out == NULL)
                    break;
            } else {
                length = strlen(FLAGS[i].name);
                nout = realloc(out, strlen(out) + length + 2);
                if (nout == NULL) {
                    free(out);
                    out = NULL;
                    break;
                }
                out = nout;
                offset = strlen(out);
                out[offset] = '|';
                memcpy(out + offset + 1, FLAGS[i].name, length);
                out[offset + 1 + length] = '\0';
            }
        }
    if (out == NULL)
        pam_syslog(pargs->pamh, LOG_DEBUG, "%s: entry", func);
    else {
        pam_syslog(pargs->pamh, LOG_DEBUG, "%s: entry (%s)", func, out);
        free(out);
    }
}


/*
 * Report an authentication failure.  This is a separate function since we
 * want to include various PAM metadata in the log message and put it in a
 * standard format.  The format here is modeled after the pam_unix
 * authentication failure message from Linux PAM.
 */
void __attribute__((__format__(printf, 2, 3)))
putil_log_failure(struct pam_args *pargs, const char *fmt, ...)
{
    char *msg;
    va_list args;
    const char *ruser = NULL;
    const char *rhost = NULL;
    const char *tty = NULL;
    const char *name = NULL;

    if (pargs->user != NULL)
        name = pargs->user;
    va_start(args, fmt);
    msg = format(fmt, args);
    va_end(args);
    if (msg == NULL)
        return;
    pam_get_item(pargs->pamh, PAM_RUSER, (PAM_CONST void **) &ruser);
    pam_get_item(pargs->pamh, PAM_RHOST, (PAM_CONST void **) &rhost);
    pam_get_item(pargs->pamh, PAM_TTY, (PAM_CONST void **) &tty);

    /* clang-format off */
    pam_syslog(pargs->pamh, LOG_NOTICE, "%s; logname=%s uid=%ld euid=%ld"
               " tty=%s ruser=%s rhost=%s", msg,
               (name  != NULL) ? name  : "",
               (long) getuid(), (long) geteuid(),
               (tty   != NULL) ? tty   : "",
               (ruser != NULL) ? ruser : "",
               (rhost != NULL) ? rhost : "");
    /* clang-format on */

    free(msg);
}


/*
 * Below are the additional logging functions enabled if built with Kerberos
 * support, used to report Kerberos errors.
 */
#ifdef HAVE_KRB5


/*
 * Log wrapper function for reporting a Kerberos error.  Log a message with
 * the given priority, prefixed by (user <user>) with the account name being
 * authenticated if known, followed by a colon and the formatted Kerberos
 * error.
 */
__attribute__((__format__(printf, 4, 0))) static void
log_krb5(struct pam_args *pargs, int priority, int status, const char *fmt,
         va_list args)
{
    char *msg;
    const char *k5_msg = NULL;

    if (priority == LOG_DEBUG && (pargs == NULL || !pargs->debug))
        return;
    msg = format(fmt, args);
    if (msg == NULL)
        return;
    if (pargs != NULL && pargs->ctx != NULL) {
        k5_msg = krb5_get_error_message(pargs->ctx, status);
        log_plain(pargs, priority, "%s: %s", msg, k5_msg);
    } else {
        log_plain(pargs, priority, "%s", msg);
    }
    free(msg);
    if (k5_msg != NULL)
        krb5_free_error_message(pargs->ctx, k5_msg);
}


/*
 * The public interfaces.  Do this with the preprocessor to save duplicate
 * code.
 */
/* clang-format off */
#define LOG_FUNCTION_KRB5(level, priority)                              \
    void __attribute__((__format__(printf, 3, 4)))                      \
    putil_ ## level ## _krb5(struct pam_args *pargs, int status,        \
                             const char *fmt, ...)                      \
    {                                                                   \
        va_list args;                                                   \
                                                                        \
        va_start(args, fmt);                                            \
        log_krb5(pargs, priority, status, fmt, args);                   \
        va_end(args);                                                   \
    }
LOG_FUNCTION_KRB5(crit,   LOG_CRIT)
LOG_FUNCTION_KRB5(err,    LOG_ERR)
LOG_FUNCTION_KRB5(notice, LOG_NOTICE)
LOG_FUNCTION_KRB5(debug,  LOG_DEBUG)
/* clang-format on */

#endif /* HAVE_KRB5 */
