/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

#include <k5-int.h>
#include "crypto_int.h"

#define ONE_MILLION_A "one million a's"

struct test {
    char *str;
    unsigned char hash[64];
};

struct test sha256_tests[] = {
    { "abc",
      { 0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad }},
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      { 0x24,0x8d,0x6a,0x61,0xd2,0x06,0x38,0xb8,
        0xe5,0xc0,0x26,0x93,0x0c,0x3e,0x60,0x39,
        0xa3,0x3c,0xe4,0x59,0x64,0xff,0x21,0x67,
        0xf6,0xec,0xed,0xd4,0x19,0xdb,0x06,0xc1 }},
    { ONE_MILLION_A,
      { 0xcd,0xc7,0x6e,0x5c,0x99,0x14,0xfb,0x92,
        0x81,0xa1,0xc7,0xe2,0x84,0xd7,0x3e,0x67,
        0xf1,0x80,0x9a,0x48,0xa4,0x97,0x20,0x0e,
        0x04,0x6d,0x39,0xcc,0xc7,0x11,0x2c,0xd0 }},
    { NULL }
};

struct test sha384_tests[] = {
    { "abc",
      { 0xcb,0x00,0x75,0x3f,0x45,0xa3,0x5e,0x8b,
	0xb5,0xa0,0x3d,0x69,0x9a,0xc6,0x50,0x07,
	0x27,0x2c,0x32,0xab,0x0e,0xde,0xd1,0x63,
	0x1a,0x8b,0x60,0x5a,0x43,0xff,0x5b,0xed,
	0x80,0x86,0x07,0x2b,0xa1,0xe7,0xcc,0x23,
	0x58,0xba,0xec,0xa1,0x34,0xc8,0x25,0xa7 }},
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
      "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
      { 0x09,0x33,0x0c,0x33,0xf7,0x11,0x47,0xe8,
	0x3d,0x19,0x2f,0xc7,0x82,0xcd,0x1b,0x47,
	0x53,0x11,0x1b,0x17,0x3b,0x3b,0x05,0xd2,
	0x2f,0xa0,0x80,0x86,0xe3,0xb0,0xf7,0x12,
	0xfc,0xc7,0xc7,0x1a,0x55,0x7e,0x2d,0xb9,
	0x66,0xc3,0xe9,0xfa,0x91,0x74,0x60,0x39 }},
    { ONE_MILLION_A,
      { 0x9d,0x0e,0x18,0x09,0x71,0x64,0x74,0xcb,
	0x08,0x6e,0x83,0x4e,0x31,0x0a,0x4a,0x1c,
	0xed,0x14,0x9e,0x9c,0x00,0xf2,0x48,0x52,
	0x79,0x72,0xce,0xc5,0x70,0x4c,0x2a,0x5b,
	0x07,0xb8,0xb3,0xdc,0x38,0xec,0xc4,0xeb,
	0xae,0x97,0xdd,0xd8,0x7f,0x3d,0x89,0x85 }},
    { NULL }
};

static int
hash_test(const struct krb5_hash_provider *hash, struct test *tests)
{
    struct test *t;
    krb5_crypto_iov iov, *iovs;
    krb5_data hval;
    size_t i;

    if (alloc_data(&hval, hash->hashsize))
	abort();
    for (t = tests; t->str; ++t) {
	if (strcmp(t->str, ONE_MILLION_A) == 0) {
	    /* Hash a million 'a's using a thousand iovs. */
	    iovs = calloc(1000, sizeof(*iovs));
	    assert(iovs != NULL);
	    for (i = 0; i < 1000; i++) {
		iovs[i].flags = KRB5_CRYPTO_TYPE_DATA;
		if (alloc_data(&iovs[i].data, 1000) != 0)
		    abort();
		memset(iovs[i].data.data, 'a', 1000);
	    }
	    if (hash->hash(iovs, 1000, &hval) != 0)
		abort();
	    if (memcmp(hval.data, t->hash, hval.length) != 0)
		abort();
	    for (i = 0; i < 1000; i++)
		free(iovs[i].data.data);
	    free(iovs);
	} else {
	    /* Hash the input in the test. */
	    iov.flags = KRB5_CRYPTO_TYPE_DATA;
	    iov.data = string2data(t->str);
	    if (hash->hash(&iov, 1, &hval) != 0)
		abort();
	    if (memcmp(hval.data, t->hash, hval.length) != 0)
		abort();

	    if (hash == &krb5int_hash_sha256) {
		/* Try again using k5_sha256(). */
		if (k5_sha256(&iov.data, (uint8_t *)hval.data) != 0)
		    abort();
		if (memcmp(hval.data, t->hash, hval.length) != 0)
		    abort();
	    }
	}
    }
    free(hval.data);
    return 0;
}

int
main()
{
    hash_test(&krb5int_hash_sha256, sha256_tests);
    hash_test(&krb5int_hash_sha384, sha384_tests);
    return 0;
}
