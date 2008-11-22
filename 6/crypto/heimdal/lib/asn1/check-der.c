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

#include <asn1-common.h>
#include <asn1_err.h>
#include <der.h>

#include "check-common.h"

RCSID("$Id: check-der.c,v 1.9 2003/01/23 10:19:49 lha Exp $");

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
			 (generic_encode)encode_integer,
			 (generic_length) length_integer,
			 (generic_decode)decode_integer,
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
			 (generic_encode)encode_octet_string,
			 (generic_length)length_octet_string,
			 (generic_decode)decode_octet_string,
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
			 (generic_encode)encode_general_string,
			 (generic_length)length_general_string,
			 (generic_decode)decode_general_string,
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
			 (generic_encode)encode_generalized_time,
			 (generic_length)length_generalized_time,
			 (generic_decode)decode_generalized_time,
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
