/*
 * Copyright (c) 2002,2003 Hewlett-Packard Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

struct uwx_bstream {
    copyin_cb copyin;
    intptr_t cb_token;
    uint64_t source;
    uint64_t buf;
    unsigned char *bufp;
    int nbuf;
    unsigned int ntotal;
    int request;
    int peekc;
};

/* uwx_init_bstream: initialize a byte stream for reading */

extern void uwx_init_bstream(
    struct uwx_bstream *bstream,
    struct uwx_env *env,
    uint64_t source,
    unsigned int len,
    int request);


/* uwx_get_byte: read the next byte from the byte stream */

extern int uwx_get_byte(struct uwx_bstream *bstream);


/* uwx_unget_byte: push a byte back onto the byte stream */

extern int uwx_unget_byte(struct uwx_bstream *bstream, int b);


/* uwx_get_uleb128: read a ULEB128 value from the byte stream */

extern int uwx_get_uleb128(struct uwx_bstream *bstream, uint64_t *val);
