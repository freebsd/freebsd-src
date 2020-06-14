/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* encode/decode functions.
 *
 * These functions perform various encoding operations, and are provided in
 * pairs, a function to query the length of and encode existing buffers, as
 * well as companion functions to perform the same process to memory
 * allocated from a pool.
 *
 * The API is designed to have the smallest possible RAM footprint, and so
 * will only allocate the exact amount of RAM needed for each conversion.
 */

#include "apr_encode.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_encode_private.h"

/* lookup table: fast and const should make it shared text page. */
static const unsigned char pr2six[256] =
{
#if !APR_CHARSET_EBCDIC
    /* ASCII table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 62, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 128, 64, 64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 63,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
#else                           /* APR_CHARSET_EBCDIC */
    /* EBCDIC table */
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    62, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 63, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 128, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 64, 64, 64, 64, 64, 64,
    64, 35, 36, 37, 38, 39, 40, 41, 42, 43, 64, 64, 64, 64, 64, 64,
    64, 64, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 64, 64, 64, 64, 64, 64,
    64, 9, 10, 11, 12, 13, 14, 15, 16, 17, 64, 64, 64, 64, 64, 64,
    64, 64, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 64,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64
#endif                          /* APR_CHARSET_EBCDIC */
};

static const unsigned char pr2five[256] =
{
#if !APR_CHARSET_EBCDIC
    /* ASCII table */
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 26, 27, 28, 29, 30, 31, 32, 32, 32, 32, 32, 128, 32, 32,
    32, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
#else                           /* APR_CHARSET_EBCDIC */
    /* EBCDIC table */
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 128, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 0, 1, 2, 3, 4, 5, 6, 7, 8, 32, 32, 32, 32, 32, 32,
    32, 9, 10, 11, 12, 13, 14, 15, 16, 17, 32, 32, 32, 32, 32, 32,
    32, 32, 18, 19, 20, 21, 22, 23, 24, 25, 32, 32, 32, 32, 32, 32,
    32, 32, 26, 27, 28, 29, 30, 31, 32, 32, 32, 32, 32, 32, 32, 32
#endif                          /* APR_CHARSET_EBCDIC */
};

static const unsigned char pr2fivehex[256] =
{
#if !APR_CHARSET_EBCDIC
    /* ASCII table */
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 32, 32, 128, 32, 32,
    32, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32
#else                           /* APR_CHARSET_EBCDIC */
    /* EBCDIC table */
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 128, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    32, 10, 11, 12, 13, 14, 15, 16, 17, 18, 32, 32, 32, 32, 32, 32,
    32, 19, 20, 21, 22, 23, 24, 25, 26, 27, 32, 32, 32, 32, 32, 32,
    32, 32, 28, 29, 30, 31, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 32, 32, 32, 32, 32
#endif                          /* APR_CHARSET_EBCDIC */
};

static const unsigned char pr2two[256] =
{
#if !APR_CHARSET_EBCDIC
    /* ASCII table */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 32, 16, 16, 16, 16, 16,
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
#else                           /* APR_CHARSET_EBCDIC */
    /* EBCDIC table */
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 16, 16, 16, 16, 16,
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 16, 16, 16, 16, 16
#endif                          /* APR_CHARSET_EBCDIC */
};

static const char base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char base64url[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const char base32[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static const char base32hex[] =
"0123456789ABCDEFGHIJKLMNOPQRSTUV";

static const char base16[] = "0123456789ABCDEF";
static const char base16lower[] = "0123456789abcdef";

APR_DECLARE(apr_status_t) apr_encode_base64(char *dest, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    const char *base;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register char *bufout = dest;
        int i;

        if (0 == ((flags & APR_ENCODE_BASE64URL))) {
            base = base64;
        }
        else {
            base = base64url;
        }

        for (i = 0; i < slen - 2; i += 3) {
            *bufout++ = base[ENCODE_TO_ASCII(((src[i]) >> 2) & 0x3F)];
            *bufout++ = base[ENCODE_TO_ASCII((((src[i]) & 0x3) << 4)
                                      | ((int)((src[i + 1]) & 0xF0) >> 4))];
            *bufout++ = base[ENCODE_TO_ASCII((((src[i + 1]) & 0xF) << 2)
                       | ((int)(ENCODE_TO_ASCII(src[i + 2]) & 0xC0) >> 6))];
            *bufout++ = base[ENCODE_TO_ASCII((src[i + 2]) & 0x3F)];
        }
        if (i < slen) {
            *bufout++ = base[ENCODE_TO_ASCII(((src[i]) >> 2) & 0x3F)];
            if (i == (slen - 1)) {
                *bufout++ = base[ENCODE_TO_ASCII((((src[i]) & 0x3) << 4))];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                }
            }
            else {
                *bufout++ = base[ENCODE_TO_ASCII((((src[i]) & 0x3) << 4)
                                      | ((int)((src[i + 1]) & 0xF0) >> 4))];
                *bufout++ = base[ENCODE_TO_ASCII(((src[i + 1]) & 0xF) << 2)];
            }
            if (!(flags & APR_ENCODE_NOPADDING)) {
                *bufout++ = '=';
            }
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout++ = '\0';

        return APR_SUCCESS;
    }

    if (len) {
        *len = ((slen + 2) / 3 * 4) + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_encode_base64_binary(char *dest, const unsigned char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    const char *base;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (dest) {
        register char *bufout = dest;
        int i;

        if (0 == ((flags & APR_ENCODE_BASE64URL))) {
            base = base64;
        }
        else {
            base = base64url;
        }

        for (i = 0; i < slen - 2; i += 3) {
            *bufout++ = base[(src[i] >> 2) & 0x3F];
            *bufout++ = base[((src[i] & 0x3) << 4)
                             | ((int)(src[i + 1] & 0xF0) >> 4)];
            *bufout++ = base[((src[i + 1] & 0xF) << 2)
                             | ((int)(src[i + 2] & 0xC0) >> 6)];
            *bufout++ = base[src[i + 2] & 0x3F];
        }
        if (i < slen) {
            *bufout++ = base[(src[i] >> 2) & 0x3F];
            if (i == (slen - 1)) {
                *bufout++ = base[((src[i] & 0x3) << 4)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                }
            }
            else {
                *bufout++ = base[((src[i] & 0x3) << 4)
                                 | ((int)(src[i + 1] & 0xF0) >> 4)];
                *bufout++ = base[((src[i + 1] & 0xF) << 2)];
            }
            if (!(flags & APR_ENCODE_NOPADDING)) {
                *bufout++ = '=';
            }
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout++ = '\0';

        return APR_SUCCESS;
    }

    if (len) {
        *len = ((slen + 2) / 3 * 4) + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *)apr_pencode_base64(apr_pool_t * p, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base64(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base64(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const char *)apr_pencode_base64_binary(apr_pool_t * p, const unsigned char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base64_binary(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base64_binary(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_decode_base64(char *dest, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register apr_size_t nprbytes;
        register apr_size_t count = slen;

        apr_status_t status;

        bufin = (const unsigned char *)src;
        while (pr2six[*(bufin++)] < 64 && count)
            count--;
        nprbytes = (bufin - (const unsigned char *)src) - 1;
        while (pr2six[*(bufin++)] > 64 && count)
            count--;

        status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
            count ? APR_BADCH : APR_SUCCESS;

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes > 4) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2six[bufin[0]] << 2
                                                   | pr2six[bufin[1]] >> 4);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                             pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                                  pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
            bufin += 4;
            nprbytes -= 4;
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }
        if (nprbytes > 1) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                               pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4);
        }
        if (nprbytes > 2) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                             pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2);
        }
        if (nprbytes > 3) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                                  pr2six[bufin[2]] << 6 | pr2six[bufin[3]]);
        }

        if (len) {
            *len = bufout - (unsigned char *)dest;
        }

        *(bufout++) = 0;

        return status;
    }

    if (len) {
        *len = (((int)slen + 3) / 4) * 3 + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_decode_base64_binary(unsigned char *dest,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register apr_size_t nprbytes;
        register apr_size_t count = slen;

        apr_status_t status;

        bufin = (const unsigned char *)src;
        while (pr2six[*(bufin++)] < 64 && count)
            count--;
        nprbytes = (bufin - (const unsigned char *)src) - 1;
        while (pr2six[*(bufin++)] > 64 && count)
            count--;

        status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
            count ? APR_BADCH : APR_SUCCESS;

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes > 4) {
            *(bufout++) = (unsigned char)(pr2six[bufin[0]] << 2
                                          | pr2six[bufin[1]] >> 4);
            *(bufout++) = (unsigned char)(pr2six[bufin[1]] << 4
                                          | pr2six[bufin[2]] >> 2);
            *(bufout++) = (unsigned char)(pr2six[bufin[2]] << 6
                                          | pr2six[bufin[3]]);
            bufin += 4;
            nprbytes -= 4;
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }
        if (nprbytes > 1) {
            *(bufout++) = (unsigned char)(pr2six[bufin[0]] << 2
                                          | pr2six[bufin[1]] >> 4);
        }
        if (nprbytes > 2) {
            *(bufout++) = (unsigned char)(pr2six[bufin[1]] << 4
                                          | pr2six[bufin[2]] >> 2);
        }
        if (nprbytes > 3) {
            *(bufout++) = (unsigned char)(pr2six[bufin[2]] << 6
                                          | pr2six[bufin[3]]);
        }

        if (len) {
            *len = bufout - dest;
        }

        return status;
    }

    if (len) {
        *len = (((int)slen + 3) / 4) * 3;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *)apr_pdecode_base64(apr_pool_t * p, const char *str,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base64(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            void *cmd = apr_palloc(p, size);
            apr_decode_base64(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const unsigned char *)apr_pdecode_base64_binary(apr_pool_t * p,
             const char *str, apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base64_binary(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            unsigned char *cmd = apr_palloc(p, size + 1);
            cmd[size] = 0;
            apr_decode_base64_binary(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_encode_base32(char *dest, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    const char *base;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register char *bufout = dest;
        int i;

        if (!((flags & APR_ENCODE_BASE32HEX))) {
            base = base32;
        }
        else {
            base = base32hex;
        }

        for (i = 0; i < slen - 4; i += 5) {
            *bufout++ = base[ENCODE_TO_ASCII((src[i] >> 3) & 0x1F)];
            *bufout++ = base[ENCODE_TO_ASCII(((src[i] << 2) & 0x1C)
                                             | ((src[i + 1] >> 6) & 0x3))];
            *bufout++ = base[ENCODE_TO_ASCII((src[i + 1] >> 1) & 0x1F)];
            *bufout++ = base[ENCODE_TO_ASCII(((src[i + 1] << 4) & 0x10)
                                             | ((src[i + 2] >> 4) & 0xF))];
            *bufout++ = base[ENCODE_TO_ASCII(((src[i + 2] << 1) & 0x1E)
                                             | ((src[i + 3] >> 7) & 0x1))];
            *bufout++ = base[ENCODE_TO_ASCII((src[i + 3] >> 2) & 0x1F)];
            *bufout++ = base[ENCODE_TO_ASCII(((src[i + 3] << 3) & 0x18)
                                             | ((src[i + 4] >> 5) & 0x7))];
            *bufout++ = base[ENCODE_TO_ASCII(src[i + 4] & 0x1F)];
        }
        if (i < slen) {
            *bufout++ = base[ENCODE_TO_ASCII(src[i] >> 3) & 0x1F];
            if (i == (slen - 1)) {
                *bufout++ = base[ENCODE_TO_ASCII((src[i] << 2) & 0x1C)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else if (i == (slen - 2)) {
                *bufout++ = base[ENCODE_TO_ASCII(((src[i] << 2) & 0x1C)
                                              | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 1] << 4) & 0x10)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else if (i == (slen - 3)) {
                *bufout++ = base[ENCODE_TO_ASCII(((src[i] << 2) & 0x1C)
                                              | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[ENCODE_TO_ASCII(((src[i + 1] << 4) & 0x10)
                                              | ((src[i + 2] >> 4) & 0xF))];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 2] << 1) & 0x1E)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else {
                *bufout++ = base[ENCODE_TO_ASCII(((src[i] << 2) & 0x1C)
                                              | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[ENCODE_TO_ASCII(((src[i + 1] << 4) & 0x10)
                                              | ((src[i + 2] >> 4) & 0xF))];
                *bufout++ = base[ENCODE_TO_ASCII(((src[i + 2] << 1) & 0x1E)
                                              | ((src[i + 3] >> 7) & 0x1))];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 3] >> 2) & 0x1F)];
                *bufout++ = base[ENCODE_TO_ASCII((src[i + 3] << 3) & 0x18)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                }
            }
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout++ = '\0';

        return APR_SUCCESS;
    }

    if (len) {
        *len = ((slen + 2) / 3 * 4) + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_encode_base32_binary(char *dest, const unsigned char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    const char *base;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (dest) {
        register char *bufout = dest;
        int i;

        if (!((flags & APR_ENCODE_BASE32HEX))) {
            base = base32;
        }
        else {
            base = base32hex;
        }

        for (i = 0; i < slen - 4; i += 5) {
            *bufout++ = base[((src[i] >> 3) & 0x1F)];
            *bufout++ = base[(((src[i] << 2) & 0x1C)
                              | ((src[i + 1] >> 6) & 0x3))];
            *bufout++ = base[((src[i + 1] >> 1) & 0x1F)];
            *bufout++ = base[(((src[i + 1] << 4) & 0x10)
                              | ((src[i + 2] >> 4) & 0xF))];
            *bufout++ = base[(((src[i + 2] << 1) & 0x1E)
                              | ((src[i + 3] >> 7) & 0x1))];
            *bufout++ = base[((src[i + 3] >> 2) & 0x1F)];
            *bufout++ = base[(((src[i + 3] << 3) & 0x18)
                              | ((src[i + 4] >> 5) & 0x7))];
            *bufout++ = base[(src[i + 4] & 0x1F)];
        }
        if (i < slen) {
            *bufout++ = base[(src[i] >> 3) & 0x1F];
            if (i == (slen - 1)) {
                *bufout++ = base[((src[i] << 2) & 0x1C)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else if (i == (slen - 2)) {
                *bufout++ = base[(((src[i] << 2) & 0x1C)
                                  | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[((src[i + 1] << 4) & 0x10)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else if (i == (slen - 3)) {
                *bufout++ = base[(((src[i] << 2) & 0x1C)
                                  | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[(((src[i + 1] << 4) & 0x10)
                                  | ((int)(src[i + 2] >> 4) & 0xF))];
                *bufout++ = base[((src[i + 2] << 1) & 0x1E)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                    *bufout++ = '=';
                    *bufout++ = '=';
                }
            }
            else {
                *bufout++ = base[(((src[i] << 2) & 0x1C)
                                  | ((src[i + 1] >> 6) & 0x3))];
                *bufout++ = base[((src[i + 1] >> 1) & 0x1F)];
                *bufout++ = base[(((src[i + 1] << 4) & 0x10)
                                  | ((src[i + 2] >> 4) & 0xF))];
                *bufout++ = base[(((src[i + 2] << 1) & 0x1E)
                                  | ((src[i + 3] >> 7) & 0x1))];
                *bufout++ = base[((src[i + 3] >> 2) & 0x1F)];
                *bufout++ = base[((src[i + 3] << 3) & 0x18)];
                if (!(flags & APR_ENCODE_NOPADDING)) {
                    *bufout++ = '=';
                }
            }
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout++ = '\0';

        return APR_SUCCESS;
    }

    if (len) {
        *len = ((slen + 4) / 5 * 8) + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *)apr_pencode_base32(apr_pool_t * p, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base32(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base32(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const char *)apr_pencode_base32_binary(apr_pool_t * p, const unsigned char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base32_binary(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base32_binary(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_decode_base32(char *dest, const char *src,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register apr_size_t nprbytes;
        register apr_size_t count = slen;

        const unsigned char *pr2;

        apr_status_t status;

        if ((flags & APR_ENCODE_BASE32HEX)) {
            pr2 = pr2fivehex;
        }
        else {
            pr2 = pr2five;
        }

        bufin = (const unsigned char *)src;
        while (pr2[*(bufin++)] < 32 && count)
            count--;
        nprbytes = (bufin - (const unsigned char *)src) - 1;
        while (pr2[*(bufin++)] > 32 && count)
            count--;

        status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
            count ? APR_BADCH : APR_SUCCESS;

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes > 8) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[0]] << 3
                                                      | pr2[bufin[1]] >> 2);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[1]] << 6
                                 | pr2[bufin[2]] << 1 | pr2[bufin[3]] >> 4);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[3]] << 4
                                                      | pr2[bufin[4]] >> 1);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[4]] << 7
                                 | pr2[bufin[5]] << 2 | pr2[bufin[6]] >> 3);
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[6]] << 5
                                                          | pr2[bufin[7]]);
            bufin += 8;
            nprbytes -= 8;
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }
        if (nprbytes >= 2) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                                   pr2[bufin[0]] << 3 | pr2[bufin[1]] >> 2);
        }
        if (nprbytes == 3) {
            status = APR_BADCH;
        }
        if (nprbytes >= 4) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                                     pr2[bufin[1]] << 6 | pr2[bufin[2]] << 1
                                                      | pr2[bufin[3]] >> 4);
        }
        if (nprbytes >= 5) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[3]] << 4
                                                      | pr2[bufin[4]] >> 1);
        }
        if (nprbytes == 6) {
            status = APR_BADCH;
        }
        if (nprbytes >= 7) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[4]] << 7
                                 | pr2[bufin[5]] << 2 | pr2[bufin[6]] >> 3);
        }
        if (nprbytes == 8) {
            *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(pr2[bufin[6]] << 5
                                                          | pr2[bufin[7]]);
        }

        if (len) {
            *len = bufout - (unsigned char *)dest;
        }

        *(bufout++) = 0;

        return status;
    }

    if (len) {
        *len = (((int)slen + 7) / 8) * 5 + 1;
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_decode_base32_binary(unsigned char *dest,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    if (dest) {
        register const unsigned char *bufin;
        register unsigned char *bufout;
        register apr_size_t nprbytes;
        register apr_size_t count = slen;

        const unsigned char *pr2;

        apr_status_t status;

        if ((flags & APR_ENCODE_BASE32HEX)) {
            pr2 = pr2fivehex;
        }
        else {
            pr2 = pr2five;
        }

        bufin = (const unsigned char *)src;
        while (pr2[*(bufin++)] < 32 && count)
            count--;
        nprbytes = (bufin - (const unsigned char *)src) - 1;
        while (pr2[*(bufin++)] > 32 && count)
            count--;

        status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
            count ? APR_BADCH : APR_SUCCESS;

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes > 8) {
            *(bufout++) = (unsigned char)(pr2[bufin[0]] << 3
                                          | pr2[bufin[1]] >> 2);
            *(bufout++) = (unsigned char)(pr2[bufin[1]] << 6
                                 | pr2[bufin[2]] << 1 | pr2[bufin[3]] >> 4);
            *(bufout++) = (unsigned char)(pr2[bufin[3]] << 4
                                          | pr2[bufin[4]] >> 1);
            *(bufout++) = (unsigned char)(pr2[bufin[4]] << 7
                                 | pr2[bufin[5]] << 2 | pr2[bufin[6]] >> 3);
            *(bufout++) = (unsigned char)(pr2[bufin[6]] << 5
                                          | pr2[bufin[7]]);
            bufin += 8;
            nprbytes -= 8;
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }
        if (nprbytes >= 2) {
            *(bufout++) = (unsigned char)(
                                   pr2[bufin[0]] << 3 | pr2[bufin[1]] >> 2);
        }
        if (nprbytes == 3) {
            status = APR_BADCH;
        }
        if (nprbytes >= 4) {
            *(bufout++) = (unsigned char)(
                                     pr2[bufin[1]] << 6 | pr2[bufin[2]] << 1
                                          | pr2[bufin[3]] >> 4);
        }
        if (nprbytes >= 5) {
            *(bufout++) = (unsigned char)(pr2[bufin[3]] << 4
                                          | pr2[bufin[4]] >> 1);
        }
        if (nprbytes == 6) {
            status = APR_BADCH;
        }
        if (nprbytes >= 7) {
            *(bufout++) = (unsigned char)(pr2[bufin[4]] << 7
                                 | pr2[bufin[5]] << 2 | pr2[bufin[6]] >> 3);
        }
        if (nprbytes == 8) {
            *(bufout++) = (unsigned char)(pr2[bufin[6]] << 5
                                          | pr2[bufin[7]]);
        }

        if (len) {
            *len = bufout - dest;
        }

        return status;
    }

    if (len) {
        *len = (((int)slen + 7) / 8) * 5;
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *)apr_pdecode_base32(apr_pool_t * p, const char *str,
                              apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base32(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            void *cmd = apr_palloc(p, size);
            apr_decode_base32(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const unsigned char *)apr_pdecode_base32_binary(apr_pool_t * p,
             const char *str, apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base32_binary(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            unsigned char *cmd = apr_palloc(p, size + 1);
            cmd[size] = 0;
            apr_decode_base32_binary(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_encode_base16(char *dest,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    const char *in = src;
    apr_size_t size;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (dest) {
        register char *bufout = dest;
        const char *base;

        if ((flags & APR_ENCODE_LOWER)) {
            base = base16lower;
        }
        else {
            base = base16;
        }

        for (size = 0; (APR_ENCODE_STRING == slen) ? in[size] : size < slen; size++) {
            if ((flags & APR_ENCODE_COLON) && size) {
                *(bufout++) = ':';
            }
            *(bufout++) = base[(const unsigned char)(ENCODE_TO_ASCII(in[size])) >> 4];
            *(bufout++) = base[(const unsigned char)(ENCODE_TO_ASCII(in[size])) & 0xf];
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout = '\0';

        return APR_SUCCESS;
    }

    if (len) {
        if (APR_ENCODE_STRING == slen) {
            slen = strlen(src);
        }
        if ((flags & APR_ENCODE_COLON) && slen) {
            *len = slen * 3;
        }
        else {
            *len = slen * 2 + 1;
        }
    }

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_encode_base16_binary(char *dest,
    const unsigned char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    const unsigned char *in = src;
    apr_size_t size;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (dest) {
        register char *bufout = dest;
        const char *base;

        if ((flags & APR_ENCODE_LOWER)) {
            base = base16lower;
        }
        else {
            base = base16;
        }

        for (size = 0; size < slen; size++) {
            if ((flags & APR_ENCODE_COLON) && size) {
                *(bufout++) = ':';
            }
            *(bufout++) = base[in[size] >> 4];
            *(bufout++) = base[in[size] & 0xf];
        }

        if (len) {
            *len = bufout - dest;
        }

        *bufout = 0;

        return APR_SUCCESS;
    }

    if (len) {
        if ((flags & APR_ENCODE_COLON) && slen) {
            *len = slen * 3;
        }
        else {
            *len = slen * 2 + 1;
        }
    }

    return APR_SUCCESS;
}

APR_DECLARE(const char *)apr_pencode_base16(apr_pool_t * p,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base16(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base16(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const char *)apr_pencode_base16_binary(apr_pool_t * p,
                      const unsigned char *src, apr_ssize_t slen, int flags,
                                                   apr_size_t * len)
{
    apr_size_t size;

    switch (apr_encode_base16_binary(NULL, src, slen, flags, &size)) {
    case APR_SUCCESS:{
            char *cmd = apr_palloc(p, size);
            apr_encode_base16_binary(cmd, src, slen, flags, len);
            return cmd;
        }
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(apr_status_t) apr_decode_base16(char *dest,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    register const unsigned char *bufin;
    register unsigned char *bufout;
    register apr_size_t nprbytes;
    register apr_size_t count;

    apr_status_t status;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    count = slen;
    bufin = (const unsigned char *)src;
    while (pr2two[*(bufin++)] != 16 && count)
        count--;
    nprbytes = (bufin - (const unsigned char *)src) - 1;
    while (pr2two[*(bufin++)] > 16 && count)
        count--;

    status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
        count ? APR_BADCH : APR_SUCCESS;

    if (dest) {

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes >= 2) {
            if (pr2two[bufin[0]] > 16) {
                bufin += 1;
                nprbytes -= 1;
            }
            else {
                *(bufout++) = (unsigned char)ENCODE_TO_NATIVE(
                                  pr2two[bufin[0]] << 4 | pr2two[bufin[1]]);
                bufin += 2;
                nprbytes -= 2;
            }
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }

        if (len) {
            *len = bufout - (unsigned char *)dest;
        }

        *(bufout++) = 0;

        return status;
    }

    else {

        count = 0;
        bufin = (const unsigned char *)src;

        while (nprbytes >= 2) {
            if (pr2two[bufin[0]] > 16) {
                bufin += 1;
                nprbytes -= 1;
            }
            else {
                count++;
                bufin += 2;
                nprbytes -= 2;
            }
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }

        if (len) {
            *len = count + 1;
        }

        return status;
    }

}

APR_DECLARE(apr_status_t) apr_decode_base16_binary(unsigned char *dest,
             const char *src, apr_ssize_t slen, int flags, apr_size_t * len)
{
    register const unsigned char *bufin;
    register unsigned char *bufout;
    register apr_size_t nprbytes;
    register apr_size_t count;

    apr_status_t status;

    if (!src) {
        return APR_NOTFOUND;
    }

    if (APR_ENCODE_STRING == slen) {
        slen = strlen(src);
    }

    count = slen;
    bufin = (const unsigned char *)src;
    while (pr2two[*(bufin++)] != 16 && count)
        count--;
    nprbytes = (bufin - (const unsigned char *)src) - 1;
    while (pr2two[*(bufin++)] > 16 && count)
        count--;

    status = flags & APR_ENCODE_RELAXED ? APR_SUCCESS :
        count ? APR_BADCH : APR_SUCCESS;

    if (dest) {

        bufout = (unsigned char *)dest;
        bufin = (const unsigned char *)src;

        while (nprbytes >= 2) {
            if (pr2two[bufin[0]] > 16) {
                bufin += 1;
                nprbytes -= 1;
            }
            else {
                *(bufout++) = (unsigned char)(
                                  pr2two[bufin[0]] << 4 | pr2two[bufin[1]]);
                bufin += 2;
                nprbytes -= 2;
            }
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }

        if (len) {
            *len = bufout - (unsigned char *)dest;
        }

        return status;
    }

    else {

        count = 0;
        bufin = (const unsigned char *)src;

        while (nprbytes >= 2) {
            if (pr2two[bufin[0]] > 16) {
                bufin += 1;
                nprbytes -= 1;
            }
            else {
                count++;
                bufin += 2;
                nprbytes -= 2;
            }
        }

        if (nprbytes == 1) {
            status = APR_BADCH;
        }

        if (len) {
            *len = count;
        }

        return status;
    }
}

APR_DECLARE(const char *)apr_pdecode_base16(apr_pool_t * p,
             const char *str, apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base16(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            void *cmd = apr_palloc(p, size);
            apr_decode_base16(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}

APR_DECLARE(const unsigned char *)apr_pdecode_base16_binary(apr_pool_t * p,
             const char *str, apr_ssize_t slen, int flags, apr_size_t * len)
{
    apr_size_t size;

    switch (apr_decode_base16_binary(NULL, str, slen, flags, &size)) {
    case APR_SUCCESS:{
            unsigned char *cmd = apr_palloc(p, size + 1);
            cmd[size] = 0;
            apr_decode_base16_binary(cmd, str, slen, flags, len);
            return cmd;
        }
    case APR_BADCH:
    case APR_NOTFOUND:{
            break;
        }
    }

    return NULL;
}
