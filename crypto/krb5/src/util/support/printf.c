/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/printf.c */
/*
 * Copyright 2003, 2004, 2005, 2007, 2008 Massachusetts Institute of
 * Technology.  All Rights Reserved.
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

/* Provide {,v}asprintf for platforms that don't have them. */

#include "k5-platform.h"

/* On error: BSD: Set *ret to NULL.  GNU: *ret is undefined.

   Since we want to be able to use the GNU version directly, we need
   provide only the weaker guarantee in this version.  */
int
krb5int_vasprintf(char **ret, const char *format, va_list ap)
{
    va_list ap2;
    char *str = NULL, *nstr;
    size_t len = 80;
    int len2;

    while (1) {
        if (len >= INT_MAX || len == 0)
            goto fail;
        nstr = realloc(str, len);
        if (nstr == NULL)
            goto fail;
        str = nstr;
        va_copy(ap2, ap);
        len2 = vsnprintf(str, len, format, ap2);
        va_end(ap2);
        /* ISO C vsnprintf returns the needed length.  Some old
           vsnprintf implementations return -1 on truncation.  */
        if (len2 < 0) {
            /* Don't know how much space we need, just that we didn't
               supply enough; get a bigger buffer and try again.  */
            if (len <= SIZE_MAX/2)
                len *= 2;
            else if (len < SIZE_MAX)
                len = SIZE_MAX;
            else
                goto fail;
        } else if ((unsigned int) len2 >= SIZE_MAX) {
            /* Need more space than we can request.  */
            goto fail;
        } else if ((size_t) len2 >= len) {
            /* Need more space, but we know how much.  */
            len = (size_t) len2 + 1;
        } else {
            /* Success!  */
            break;
        }
    }
    /* We might've allocated more than we need, if we're still using
       the initial guess, or we got here by doubling.  */
    if ((size_t) len2 < len - 1) {
        nstr = realloc(str, (size_t) len2 + 1);
        if (nstr)
            str = nstr;
    }
    *ret = str;
    return len2;

fail:
    free(str);
    return -1;
}

int
krb5int_asprintf(char **ret, const char *format, ...)
{
    va_list ap;
    int n;

    va_start(ap, format);
    n = krb5int_vasprintf(ret, format, ap);
    va_end(ap);
    return n;
}
