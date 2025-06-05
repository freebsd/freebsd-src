/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-err.h */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 *
 * Error-message handling
 */

#ifndef K5_ERR_H
#define K5_ERR_H

#if defined(_MSDOS) || defined(_WIN32)
#include <win-mac.h>
#endif
#ifndef KRB5_CALLCONV
#define KRB5_CALLCONV
#define KRB5_CALLCONV_C
#endif

#include <stdarg.h>

struct errinfo {
    long code;
    char *msg;
};
#define EMPTY_ERRINFO { 0, NULL }

void k5_set_error(struct errinfo *ep, long code, const char *fmt, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 3, 4)))
#endif
    ;

void k5_vset_error(struct errinfo *ep, long code, const char *fmt,
                   va_list args)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 3, 0)))
#endif
    ;

const char *k5_get_error(struct errinfo *ep, long code);
void k5_free_error(struct errinfo *ep, const char *msg);
void k5_clear_error(struct errinfo *ep);
void k5_set_error_info_callout_fn(const char *(KRB5_CALLCONV *f)(long));

#endif /* K5_ERR_H */
