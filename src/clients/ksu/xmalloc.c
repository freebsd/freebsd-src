/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* clients/ksu/xmalloc.c - Exit-on-failure allocation wrappers */
/*
 * Copyright 1999 by the Massachusetts Institute of Technology.
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

#include "k5-platform.h"
#include "ksu.h"

void *xmalloc (size_t sz)
{
    void *ret = malloc (sz);
    if (ret == 0 && sz != 0) {
        perror (prog_name);
        exit (1);
    }
    return ret;
}

void *xrealloc (void *old, size_t newsz)
{
    void *ret = realloc (old, newsz);
    if (ret == 0 && newsz != 0) {
        perror (prog_name);
        exit (1);
    }
    return ret;
}

void *xcalloc (size_t nelts, size_t eltsz)
{
    void *ret = calloc (nelts, eltsz);
    if (ret == 0 && nelts != 0 && eltsz != 0) {
        perror (prog_name);
        exit (1);
    }
    return ret;
}

char *xstrdup (const char *src)
{
    size_t len = strlen (src) + 1;
    char *dst = xmalloc (len);
    memcpy (dst, src, len);
    return dst;
}

char *xasprintf (const char *format, ...)
{
    char *out;
    va_list args;

    va_start (args, format);
    if (vasprintf(&out, format, args) < 0) {
        perror (prog_name);
        exit (1);
    }
    va_end(args);
    return out;
}
