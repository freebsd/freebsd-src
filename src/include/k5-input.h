/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-input.h - k5input helper functions */
/*
 * Copyright (C) 2014 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef K5_INPUT_H
#define K5_INPUT_H

#include "k5-int.h"

/*
 * The k5input module defines helpers for safely consuming a fixed-sized block
 * of memory.  If an overrun or allocation failure occurs at any step,
 * subsequent functions will return default values until the error is detected
 * by looking at the status field.
 */

struct k5input {
    const unsigned char *ptr;
    size_t len;
    krb5_error_code status;
};

static inline void
k5_input_init(struct k5input *in, const void *ptr, size_t len)
{
    in->ptr = ptr;
    in->len = len;
    in->status = 0;
}

/* Only set the status value of in if it hasn't already been set, so status
 * reflects the first thing to go wrong. */
static inline void
k5_input_set_status(struct k5input *in, krb5_error_code status)
{
    if (!in->status)
        in->status = status;
}

static inline const unsigned char *
k5_input_get_bytes(struct k5input *in, size_t len)
{
    if (in->len < len)
        k5_input_set_status(in, EINVAL);
    if (in->status)
        return NULL;
    in->len -= len;
    in->ptr += len;
    return in->ptr - len;
}

static inline unsigned char
k5_input_get_byte(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 1);
    return (ptr == NULL) ? '\0' : *ptr;
}

static inline uint16_t
k5_input_get_uint16_be(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 2);
    return (ptr == NULL) ? 0 : load_16_be(ptr);
}

static inline uint16_t
k5_input_get_uint16_le(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 2);
    return (ptr == NULL) ? 0 : load_16_le(ptr);
}

static inline uint16_t
k5_input_get_uint16_n(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 2);
    return (ptr == NULL) ? 0 : load_16_n(ptr);
}

static inline uint32_t
k5_input_get_uint32_be(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 4);
    return (ptr == NULL) ? 0 : load_32_be(ptr);
}

static inline uint32_t
k5_input_get_uint32_le(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 4);
    return (ptr == NULL) ? 0 : load_32_le(ptr);
}

static inline uint32_t
k5_input_get_uint32_n(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 4);
    return (ptr == NULL) ? 0 : load_32_n(ptr);
}

static inline uint64_t
k5_input_get_uint64_be(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 8);
    return (ptr == NULL) ? 0 : load_64_be(ptr);
}

static inline uint64_t
k5_input_get_uint64_le(struct k5input *in)
{
    const unsigned char *ptr = k5_input_get_bytes(in, 8);
    return (ptr == NULL) ? 0 : load_64_le(ptr);
}

#endif /* K5_BUF_H */
