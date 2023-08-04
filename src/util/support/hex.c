/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/hex.c - hex encoding/decoding implementation */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
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

#include <k5-platform.h>
#include <k5-hex.h>
#include <ctype.h>

static inline char
hex_digit(uint8_t bval, int uppercase)
{
    assert(bval <= 0xF);
    if (bval < 10)
        return '0' + bval;
    else if (uppercase)
        return 'A' + (bval - 10);
    else
        return 'a' + (bval - 10);
}

int
k5_hex_encode(const void *bytes, size_t len, int uppercase, char **hex_out)
{
    size_t i;
    const uint8_t *p = bytes;
    char *hex;

    *hex_out = NULL;

    hex = malloc(len * 2 + 1);
    if (hex == NULL)
        return ENOMEM;

    for (i = 0; i < len; i++) {
        hex[i * 2] = hex_digit(p[i] >> 4, uppercase);
        hex[i * 2 + 1] = hex_digit(p[i] & 0xF, uppercase);
    }
    hex[len * 2] = '\0';

    *hex_out = hex;
    return 0;
}

/* Decode a hex digit.  Return 0-15 on success, -1 on invalid input. */
static inline int
decode_hexchar(unsigned char c)
{
    if (isdigit(c))
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

int
k5_hex_decode(const char *hex, uint8_t **bytes_out, size_t *len_out)
{
    size_t hexlen, i;
    int h1, h2;
    uint8_t *bytes;

    *bytes_out = NULL;
    *len_out = 0;

    hexlen = strlen(hex);
    if (hexlen % 2 != 0)
        return EINVAL;
    bytes = malloc(hexlen / 2 + 1);
    if (bytes == NULL)
        return ENOMEM;

    for (i = 0; i < hexlen / 2; i++) {
        h1 = decode_hexchar(hex[i * 2]);
        h2 = decode_hexchar(hex[i * 2 + 1]);
        if (h1 == -1 || h2 == -1) {
            free(bytes);
            return EINVAL;
        }
        bytes[i] = h1 * 16 + h2;
    }
    bytes[i] = 0;

    *bytes_out = bytes;
    *len_out = hexlen / 2;
    return 0;
}
