/*
 * Copyright (c) 1999 - 2003 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <roken.h>

#include "check-common.h"

RCSID("$Id: check-common.c,v 1.1 2003/01/23 10:21:36 lha Exp $");

static void
print_bytes (unsigned const char *buf, size_t len)
{
    int i;

    for (i = 0; i < len; ++i)
	printf ("%02x ", buf[i]);
}

int
generic_test (const struct test_case *tests,
	      unsigned ntests,
	      size_t data_size,
	      int (*encode)(unsigned char *, size_t, void *, size_t *),
	      int (*length)(void *),
	      int (*decode)(unsigned char *, size_t, void *, size_t *),
	      int (*cmp)(void *a, void *b))
{
    unsigned char buf[4711];
    int i;
    int failures = 0;
    void *val = malloc (data_size);

    if (data_size != 0 && val == NULL)
	err (1, "malloc");

    for (i = 0; i < ntests; ++i) {
	int ret;
	size_t sz, consumed_sz, length_sz;
	unsigned char *beg;

	ret = (*encode) (buf + sizeof(buf) - 1, sizeof(buf),
			 tests[i].val, &sz);
	beg = buf + sizeof(buf) - sz;
	if (ret != 0) {
	    printf ("encoding of %s failed\n", tests[i].name);
	    ++failures;
	}
	if (sz != tests[i].byte_len) {
	    printf ("encoding of %s has wrong len (%lu != %lu)\n",
		    tests[i].name, 
		    (unsigned long)sz, (unsigned long)tests[i].byte_len);
	    ++failures;
	}

	length_sz = (*length) (tests[i].val);
	if (sz != length_sz) {
	    printf ("length for %s is bad (%lu != %lu)\n",
		    tests[i].name, (unsigned long)length_sz, (unsigned long)sz);
	    ++failures;
	}

	if (memcmp (beg, tests[i].bytes, tests[i].byte_len) != 0) {
	    printf ("encoding of %s has bad bytes:\n"
		    "correct: ", tests[i].name);
	    print_bytes (tests[i].bytes, tests[i].byte_len);
	    printf ("\nactual:  ");
	    print_bytes (beg, sz);
	    printf ("\n");
	    ++failures;
	}
	ret = (*decode) (beg, sz, val, &consumed_sz);
	if (ret != 0) {
	    printf ("decoding of %s failed\n", tests[i].name);
	    ++failures;
	}
	if (sz != consumed_sz) {
	    printf ("different length decoding %s (%ld != %ld)\n",
		    tests[i].name, 
		    (unsigned long)sz, (unsigned long)consumed_sz);
	    ++failures;
	}
	if ((*cmp)(val, tests[i].val) != 0) {
	    printf ("%s: comparison failed\n", tests[i].name);
	    ++failures;
	}
    }
    free (val);
    return failures;
}
