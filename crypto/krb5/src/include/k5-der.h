/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-der.h - Distinguished Encoding Rules (DER) declarations */
/*
 * Copyright (C) 2023 by the Massachusetts Institute of Technology.
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

/*
 * Most ASN.1 encoding and decoding is done using the table-driven framework in
 * libkrb5.  When that is not an option, these helpers can be used to encode
 * and decode simple types.
 */

#ifndef K5_DER_H
#define K5_DER_H

#include <stdint.h>
#include <stdbool.h>
#include "k5-buf.h"
#include "k5-input.h"

/* Return the number of bytes needed to encode len as a DER encoding length. */
static inline size_t
k5_der_len_len(size_t len)
{
    size_t llen;

    if (len < 128)
        return 1;
    llen = 1;
    while (len > 0) {
        len >>= 8;
        llen++;
    }
    return llen;
}

/* Return the number of bytes needed to encode a DER value (with identifier
 * byte and length) for a given contents length. */
static inline size_t
k5_der_value_len(size_t contents_len)
{
    return 1 + k5_der_len_len(contents_len) + contents_len;
}

/* Add a DER identifier byte (composed by the caller, including the ASN.1
 * class, tag, and constructed bit) and length. */
static inline void
k5_der_add_taglen(struct k5buf *buf, uint8_t idbyte, size_t len)
{
    uint8_t *p;
    size_t llen = k5_der_len_len(len);

    p = k5_buf_get_space(buf, 1 + llen);
    if (p == NULL)
        return;
    *p++ = idbyte;
    if (len < 128) {
        *p = len;
    } else {
        *p = 0x80 | (llen - 1);
        /* Encode the length bytes backwards so the most significant byte is
         * first. */
        p += llen;
        while (len > 0) {
            *--p = len & 0xFF;
            len >>= 8;
        }
    }
}

/* Add a DER value (identifier byte, length, and contents). */
static inline void
k5_der_add_value(struct k5buf *buf, uint8_t idbyte, const void *contents,
                 size_t len)
{
    k5_der_add_taglen(buf, idbyte, len);
    k5_buf_add_len(buf, contents, len);
}

/*
 * If the next byte in in matches idbyte and the subsequent DER length is
 * valid, advance in past the value, set *contents_out to the value contents,
 * and return true.  Otherwise return false.  Only set an error on in if the
 * next bytes matches idbyte but the ensuing length is invalid.  contents_out
 * may be aliased to in; it will only be written to on successful decoding of a
 * value.
 */
static inline bool
k5_der_get_value(struct k5input *in, uint8_t idbyte,
                 struct k5input *contents_out)
{
    uint8_t lenbyte, i;
    size_t len;
    const void *bytes;

    /* Do nothing if in is empty or the next byte doesn't match idbyte. */
    if (in->status || in->len == 0 || *in->ptr != idbyte)
        return false;

    /* Advance past the identifier byte and decode the length. */
    (void)k5_input_get_byte(in);
    lenbyte = k5_input_get_byte(in);
    if (lenbyte < 128) {
        len = lenbyte;
    } else {
        len = 0;
        for (i = 0; i < (lenbyte & 0x7F); i++) {
            if (len > (SIZE_MAX >> 8)) {
                k5_input_set_status(in, EOVERFLOW);
                return false;
            }
            len = (len << 8) | k5_input_get_byte(in);
        }
    }

    bytes = k5_input_get_bytes(in, len);
    if (bytes == NULL)
        return false;
    k5_input_init(contents_out, bytes, len);
    return true;
}

#endif /* K5_DER_H */
