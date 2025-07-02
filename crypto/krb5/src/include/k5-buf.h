/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-buf.h - k5buf interface declarations */
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

#ifndef K5_BUF_H
#define K5_BUF_H

#include <stdarg.h>
#include <string.h>

/*
 * The k5buf module is intended to allow multi-step string construction in a
 * fixed or dynamic buffer without the need to check for a failure at each step
 * (and without aborting on malloc failure).  If an allocation failure occurs
 * or the fixed buffer runs out of room, the buffer will be set to an error
 * state which can be detected with k5_buf_status.  Data in a buffer is not
 * automatically terminated with a zero byte; call k5_buf_cstring() to use the
 * contents as a C string.
 *
 * k5buf structures are usually stack-allocated.  Do not put k5buf structure
 * pointers into public APIs.  It is okay to reference the data and len fields
 * of a buffer (they will be NULL/0 if the buffer is in an error state), but do
 * not change them.
 */

/* Buffer type values */
enum k5buftype { K5BUF_ERROR, K5BUF_FIXED, K5BUF_DYNAMIC, K5BUF_DYNAMIC_ZAP };

struct k5buf {
    enum k5buftype buftype;
    void *data;
    size_t space;
    size_t len;
};

#define EMPTY_K5BUF { K5BUF_ERROR }

/* Initialize a k5buf using a fixed-sized, existing buffer.  SPACE must be
 * more than zero, or an assertion failure will result. */
void k5_buf_init_fixed(struct k5buf *buf, void *data, size_t space);

/* Initialize a k5buf using an internally allocated dynamic buffer. */
void k5_buf_init_dynamic(struct k5buf *buf);

/* Initialize a k5buf using an internally allocated dynamic buffer, zeroing
 * memory when reallocating or freeing. */
void k5_buf_init_dynamic_zap(struct k5buf *buf);

/* Add a C string to BUF. */
void k5_buf_add(struct k5buf *buf, const char *data);

/* Add a counted series of bytes to BUF. */
void k5_buf_add_len(struct k5buf *buf, const void *data, size_t len);

/* Add sprintf-style formatted data to BUF.  For a fixed-length buffer this
 * operation will fail if there isn't room for a zero terminator. */
void k5_buf_add_fmt(struct k5buf *buf, const char *fmt, ...)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

/* Add sprintf-style formatted data to BUF, with a va_list.  The value of ap is
 * undefined after the call. */
void k5_buf_add_vfmt(struct k5buf *buf, const char *fmt, va_list ap)
#if !defined(__cplusplus) && (__GNUC__ > 2)
    __attribute__((__format__(__printf__, 2, 0)))
#endif
    ;

/* Without changing the length of buf, ensure that there is a zero byte after
 * buf.data and return it.  Return NULL on error. */
char *k5_buf_cstring(struct k5buf *buf);

/* Extend the length of buf by len and return a pointer to the reserved space,
 * to be filled in by the caller.  Return NULL on error. */
void *k5_buf_get_space(struct k5buf *buf, size_t len);

/* Truncate BUF.  LEN must be between 0 and the existing buffer
 * length, or an assertion failure will result. */
void k5_buf_truncate(struct k5buf *buf, size_t len);

/* Return ENOMEM if buf is in an error state, 0 otherwise. */
int k5_buf_status(struct k5buf *buf);

/*
 * Free the storage used in the dynamic buffer BUF.  The caller may choose to
 * take responsibility for freeing the data pointer instead of using this
 * function.  If BUF is a fixed buffer, an assertion failure will result.
 * Freeing a buffer in the error state, a buffer initialized with EMPTY_K5BUF,
 * or a zeroed k5buf structure is a no-op.
 */
void k5_buf_free(struct k5buf *buf);

static inline void
k5_buf_add_byte(struct k5buf *buf, uint8_t val)
{
    k5_buf_add_len(buf, &val, 1);
}

static inline void
k5_buf_add_uint16_be(struct k5buf *buf, uint16_t val)
{
    void *p = k5_buf_get_space(buf, 2);

    if (p != NULL)
        store_16_be(val, p);
}

static inline void
k5_buf_add_uint16_le(struct k5buf *buf, uint16_t val)
{
    void *p = k5_buf_get_space(buf, 2);

    if (p != NULL)
        store_16_le(val, p);
}

static inline void
k5_buf_add_uint32_be(struct k5buf *buf, uint32_t val)
{
    void *p = k5_buf_get_space(buf, 4);

    if (p != NULL)
        store_32_be(val, p);
}

static inline void
k5_buf_add_uint32_le(struct k5buf *buf, uint32_t val)
{
    void *p = k5_buf_get_space(buf, 4);

    if (p != NULL)
        store_32_le(val, p);
}

static inline void
k5_buf_add_uint64_be(struct k5buf *buf, uint64_t val)
{
    void *p = k5_buf_get_space(buf, 8);

    if (p != NULL)
        store_64_be(val, p);
}

static inline void
k5_buf_add_uint64_le(struct k5buf *buf, uint64_t val)
{
    void *p = k5_buf_get_space(buf, 8);

    if (p != NULL)
        store_64_le(val, p);
}

#endif /* K5_BUF_H */
