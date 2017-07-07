/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/k5buf.c - string buffer functions */

/*
 * Copyright 2008 Massachusetts Institute of Technology.
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
 * Can't include krb5.h here, or k5-int.h which includes it, because krb5.h
 * needs to be generated with error tables, after util/et, which builds after
 * this directory.
 */
#include "k5-platform.h"
#include "k5-buf.h"
#include <assert.h>

/*
 * Structure invariants:
 *
 * buftype is K5BUF_FIXED, K5BUF_DYNAMIC, or K5BUF_ERROR
 * if buftype is K5BUF_ERROR, the other fields are NULL or 0
 * if buftype is not K5BUF_ERROR:
 *   space > 0
 *   len < space
 *   data[len] = '\0'
 */

/* Return a character pointer to the current end of buf. */
static inline char *
endptr(struct k5buf *buf)
{
    return (char *)buf->data + buf->len;
}

static inline void
set_error(struct k5buf *buf)
{
    buf->buftype = K5BUF_ERROR;
    buf->data = NULL;
    buf->space = buf->len = 0;
}

/*
 * Make sure there is room for LEN more characters in BUF, in addition to the
 * null terminator and what's already in there.  Return true on success.  On
 * failure, set the error flag and return false.
 */
static int
ensure_space(struct k5buf *buf, size_t len)
{
    size_t new_space;
    char *new_data;

    if (buf->buftype == K5BUF_ERROR)
        return 0;
    if (buf->space - 1 - buf->len >= len) /* Enough room already. */
        return 1;
    if (buf->buftype == K5BUF_FIXED) /* Can't resize a fixed buffer. */
        goto error_exit;
    assert(buf->buftype == K5BUF_DYNAMIC);
    new_space = buf->space * 2;
    while (new_space - buf->len - 1 < len) {
        if (new_space > SIZE_MAX / 2)
            goto error_exit;
        new_space *= 2;
    }
    new_data = realloc(buf->data, new_space);
    if (new_data == NULL)
        goto error_exit;
    buf->data = new_data;
    buf->space = new_space;
    return 1;

error_exit:
    if (buf->buftype == K5BUF_DYNAMIC)
        free(buf->data);
    set_error(buf);
    return 0;
}

void
k5_buf_init_fixed(struct k5buf *buf, char *data, size_t space)
{
    assert(space > 0);
    buf->buftype = K5BUF_FIXED;
    buf->data = data;
    buf->space = space;
    buf->len = 0;
    *endptr(buf) = '\0';
}

void
k5_buf_init_dynamic(struct k5buf *buf)
{
    buf->buftype = K5BUF_DYNAMIC;
    buf->space = 128;
    buf->data = malloc(buf->space);
    if (buf->data == NULL) {
        set_error(buf);
        return;
    }
    buf->len = 0;
    *endptr(buf) = '\0';
}

void
k5_buf_add(struct k5buf *buf, const char *data)
{
    k5_buf_add_len(buf, data, strlen(data));
}

void
k5_buf_add_len(struct k5buf *buf, const void *data, size_t len)
{
    if (!ensure_space(buf, len))
        return;
    if (len > 0)
        memcpy(endptr(buf), data, len);
    buf->len += len;
    *endptr(buf) = '\0';
}

void
k5_buf_add_fmt(struct k5buf *buf, const char *fmt, ...)
{
    va_list ap;
    int r;
    size_t remaining;
    char *tmp;

    if (buf->buftype == K5BUF_ERROR)
        return;
    remaining = buf->space - buf->len;

    if (buf->buftype == K5BUF_FIXED) {
        /* Format the data directly into the fixed buffer. */
        va_start(ap, fmt);
        r = vsnprintf(endptr(buf), remaining, fmt, ap);
        va_end(ap);
        if (SNPRINTF_OVERFLOW(r, remaining))
            set_error(buf);
        else
            buf->len += (unsigned int) r;
        return;
    }

    /* Optimistically format the data directly into the dynamic buffer. */
    assert(buf->buftype == K5BUF_DYNAMIC);
    va_start(ap, fmt);
    r = vsnprintf(endptr(buf), remaining, fmt, ap);
    va_end(ap);
    if (!SNPRINTF_OVERFLOW(r, remaining)) {
        buf->len += (unsigned int) r;
        return;
    }

    if (r >= 0) {
        /* snprintf correctly told us how much space is required. */
        if (!ensure_space(buf, r))
            return;
        remaining = buf->space - buf->len;
        va_start(ap, fmt);
        r = vsnprintf(endptr(buf), remaining, fmt, ap);
        va_end(ap);
        if (SNPRINTF_OVERFLOW(r, remaining))  /* Shouldn't ever happen. */
            k5_buf_free(buf);
        else
            buf->len += (unsigned int) r;
        return;
    }

    /* It's a pre-C99 snprintf implementation, or something else went wrong.
     * Fall back to asprintf. */
    va_start(ap, fmt);
    r = vasprintf(&tmp, fmt, ap);
    va_end(ap);
    if (r < 0) {
        k5_buf_free(buf);
        return;
    }
    if (ensure_space(buf, r)) {
        /* Copy the temporary string into buf, including terminator. */
        memcpy(endptr(buf), tmp, r + 1);
        buf->len += r;
    }
    free(tmp);
}

void *
k5_buf_get_space(struct k5buf *buf, size_t len)
{
    if (!ensure_space(buf, len))
        return NULL;
    buf->len += len;
    *endptr(buf) = '\0';
    return endptr(buf) - len;
}

void
k5_buf_truncate(struct k5buf *buf, size_t len)
{
    if (buf->buftype == K5BUF_ERROR)
        return;
    assert(len <= buf->len);
    buf->len = len;
    *endptr(buf) = '\0';
}

int
k5_buf_status(struct k5buf *buf)
{
    return (buf->buftype == K5BUF_ERROR) ? ENOMEM : 0;
}

void
k5_buf_free(struct k5buf *buf)
{
    if (buf->buftype == K5BUF_ERROR)
        return;
    assert(buf->buftype == K5BUF_DYNAMIC);
    free(buf->data);
    set_error(buf);
}
