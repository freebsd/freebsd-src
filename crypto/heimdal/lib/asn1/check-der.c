/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include <libasn1.h>

RCSID("$Id: check-der.c,v 1.7 1999/12/02 17:05:01 joda Exp $");

static void
print_bytes (unsigned const char *buf, size_t len)
{
    int i;

    for (i = 0; i < len; ++i)
	printf ("%02x ", buf[i]);
}

struct test_case {
    void *val;
    int byte_len;
    const unsigned char *bytes;
    char *name;
};

static int
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

static int
cmp_integer (void *a, void *b)
{
    int *ia = (int *)a;
    int *ib = (int *)b;

    return *ib - *ia;
}

static int
test_integer (void)
{
    struct test_case tests[] = {
	{NULL, 3, "\x02\x01\x00"},
	{NULL, 3, "\x02\x01\x7f"},
	{NULL, 4, "\x02\x02\x00\x80"},
	{NULL, 4, "\x02\x02\x01\x00"},
	{NULL, 3, "\x02\x01\x80"},
	{NULL, 4, "\x02\x02\xff\x7f"},
	{NULL, 3, "\x02\x01\xff"},
	{NULL, 4, "\x02\x02\xff\x01"},
	{NULL, 4, "\x02\x02\x00\xff"},
	{NULL, 6, "\x02\x04\x80\x00\x00\x00"},
	{NULL, 6, "\x02\x04\x7f\xff\xff\xff"}
    };

    int values[] = {0, 127, 128, 256, -128, -129, -1, -255, 255,
		    0x80000000, 0x7fffffff};
    int i;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	asprintf (&tests[i].name, "integer %d", values[i]);
    }

    return generic_test (tests, ntests, sizeof(int),
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))encode_integer,
			 (int (*)(void *))length_integer,
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))decode_integer,
			 cmp_integer);
}

static int
cmp_octet_string (void *a, void *b)
{
    octet_string *oa = (octet_string *)a;
    octet_string *ob = (octet_string *)b;

    if (oa->length != ob->length)
	return ob->length - oa->length;

    return (memcmp (oa->data, ob->data, oa->length));
}

static int
test_octet_string (void)
{
    octet_string s1 = {8, "\x01\x23\x45\x67\x89\xab\xcd\xef"};

    struct test_case tests[] = {
	{NULL, 10, "\x04\x08\x01\x23\x45\x67\x89\xab\xcd\xef"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    tests[0].val = &s1;
    asprintf (&tests[0].name, "a octet string");

    return generic_test (tests, ntests, sizeof(octet_string),
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))encode_octet_string,
			 (int (*)(void *))length_octet_string,
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))decode_octet_string,
			 cmp_octet_string);
}

static int
cmp_general_string (void *a, void *b)
{
    unsigned char **sa = (unsigned char **)a;
    unsigned char **sb = (unsigned char **)b;

    return strcmp (*sa, *sb);
}

static int
test_general_string (void)
{
    unsigned char *s1 = "Test User 1";

    struct test_case tests[] = {
	{NULL, 13, "\x1b\x0b\x54\x65\x73\x74\x20\x55\x73\x65\x72\x20\x31"}
    };
    int ntests = sizeof(tests) / sizeof(*tests);

    tests[0].val = &s1;
    asprintf (&tests[0].name, "the string \"%s\"", s1);

    return generic_test (tests, ntests, sizeof(unsigned char *),
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))encode_general_string,
			 (int (*)(void *))length_general_string,
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))decode_general_string,
			 cmp_general_string);
}

static int
cmp_generalized_time (void *a, void *b)
{
    time_t *ta = (time_t *)a;
    time_t *tb = (time_t *)b;

    return *tb - *ta;
}

static int
test_generalized_time (void)
{
    struct test_case tests[] = {
	{NULL, 17, "\x18\x0f""19700101000000Z"},
	{NULL, 17, "\x18\x0f""19851106210627Z"}
    };
    time_t values[] = {0, 500159187};
    int i;
    int ntests = sizeof(tests) / sizeof(*tests);

    for (i = 0; i < ntests; ++i) {
	tests[i].val = &values[i];
	asprintf (&tests[i].name, "time %d", (int)values[i]);
    }

    return generic_test (tests, ntests, sizeof(time_t),
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))encode_generalized_time,
			 (int (*)(void *))length_generalized_time,
			 (int (*)(unsigned char *, size_t,
				  void *, size_t *))decode_generalized_time,
			 cmp_generalized_time);
}

int
main(int argc, char **argv)
{
    int ret = 0;

    ret += test_integer ();
    ret += test_octet_string ();
    ret += test_general_string ();
    ret += test_generalized_time ();

    return ret;
}
