/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/base64.c - base64 encoder and decoder */
/*
 * Copyright (c) 1995-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <k5-platform.h>
#include <k5-base64.h>

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *
k5_base64_encode(const void *data, size_t len)
{
    char *s, *p;
    size_t i;
    unsigned int c;
    const unsigned char *q;

    if (len > SIZE_MAX / 4)
        return NULL;

    p = s = malloc(len * 4 / 3 + 4);
    if (p == NULL)
        return NULL;
    q = (const unsigned char *)data;

    for (i = 0; i < len;) {
        c = q[i++];
        c *= 256;
        if (i < len)
            c += q[i];
        i++;
        c *= 256;
        if (i < len)
            c += q[i];
        i++;
        p[0] = base64_chars[(c & 0x00fc0000) >> 18];
        p[1] = base64_chars[(c & 0x0003f000) >> 12];
        p[2] = base64_chars[(c & 0x00000fc0) >> 6];
        p[3] = base64_chars[(c & 0x0000003f) >> 0];
        if (i > len)
            p[3] = '=';
        if (i > len + 1)
            p[2] = '=';
        p += 4;
    }
    *p = '\0';
    return s;
}

#define DECODE_ERROR 0xffffffff

/* Decode token, which must be four bytes long. */
static unsigned int
decode_token(const char *token)
{
    int i, marker = 0;
    unsigned int val = 0;
    const char *p;

    for (i = 0; i < 4; i++) {
        val *= 64;
        if (token[i] == '=') {
            marker++;
        } else if (marker > 0) {
            return DECODE_ERROR;
        } else {
            p = strchr(base64_chars, token[i]);
            if (p == NULL)
                return DECODE_ERROR;
            val += p - base64_chars;
        }
    }
    if (marker > 2)
        return DECODE_ERROR;
    return (marker << 24) | val;
}

void *
k5_base64_decode(const char *str, size_t *len_out)
{
    unsigned char *data, *q;
    unsigned int val, marker;
    size_t len;

    *len_out = SIZE_MAX;

    /* Allocate the output buffer. */
    len = strlen(str);
    if (len % 4)
        return NULL;
    q = data = malloc(len / 4 * 3);
    if (data == NULL) {
        *len_out = 0;
        return NULL;
    }

    /* Decode the string. */
    for (; *str != '\0'; str += 4) {
        val = decode_token(str);
        if (val == DECODE_ERROR) {
            free(data);
            return NULL;
        }
        marker = (val >> 24) & 0xff;
        *q++ = (val >> 16) & 0xff;
        if (marker < 2)
            *q++ = (val >> 8) & 0xff;
        if (marker < 1)
            *q++ = val & 0xff;
    }
    *len_out = q - data;
    return data;
}
