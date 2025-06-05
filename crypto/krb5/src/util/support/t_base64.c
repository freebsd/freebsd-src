/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_base64.c - base64 encoding and decoding tests */
/*
 * Copyright (c) 1999 - 2001 Kungliga Tekniska Högskolan
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

static struct test {
    void *data;
    size_t len;
    const char *result;
} tests[] = {
    { "", 0 , "" },
    { "1", 1, "MQ==" },
    { "22", 2, "MjI=" },
    { "333", 3, "MzMz" },
    { "4444", 4, "NDQ0NA==" },
    { "55555", 5, "NTU1NTU=" },
    { "abc:def", 7, "YWJjOmRlZg==" },
    { "f", 1, "Zg==" },
    { "fo", 2, "Zm8=" },
    { "foo", 3, "Zm9v" },
    { "foob", 4, "Zm9vYg==" },
    { "fooba", 5, "Zm9vYmE=" },
    { "foobar", 6, "Zm9vYmFy" },
    { NULL, 0, NULL }
};

static char *negative_tests[] = {
    "M=M=",
    "MM=M",
    "MQ===",
    "====",
    "M===",
    NULL
};

int
main(int argc, char **argv)
{
    char *str, **ntest;
    void *data;
    int numerr = 0, numtest = 1;
    const struct test *t;
    size_t len;

    for (t = tests; t->data != NULL; t++) {
        str = k5_base64_encode(t->data, t->len);
        if (strcmp(str, t->result) != 0) {
            fprintf(stderr, "failed test %d: %s != %s\n", numtest,
                    str, t->result);
            numerr++;
        }
        free(str);
        data = k5_base64_decode(t->result, &len);
        if (len != t->len) {
            fprintf(stderr, "failed test %d: len %lu != %lu\n", numtest,
                    (unsigned long)len, (unsigned long)t->len);
            numerr++;
        } else if (memcmp(data, t->data, t->len) != 0) {
            fprintf(stderr, "failed test %d: data\n", numtest);
            numerr++;
        }
        free(data);
        numtest++;
    }

    for (ntest = negative_tests; *ntest != NULL; ntest++) {
        data = k5_base64_decode(*ntest, &len);
        if (data != NULL || len != SIZE_MAX) {
            fprintf(stderr, "failed test %d: successful decode: %s\n",
                    numtest, *ntest);
            numerr++;
        }
        numtest++;
    }

    return numerr ? 1 : 0;
}
