/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include <config.h>

#define HC_DEPRECATED_CRYPTO

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getarg.h>
#include <roken.h>

#include <evp.h>
#include <evp-hcrypto.h>
#include <evp-cc.h>
#include <hex.h>
#include <err.h>

struct tests {
    const char *name;
    void *key;
    size_t keysize;
    void *iv;
    size_t datasize;
    void *indata;
    void *outdata;
    void *outiv;
};

struct tests aes_tests[] = {
    { "aes-256",
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      32,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      "\xdc\x95\xc0\x78\xa2\x40\x89\x89\xad\x48\xa2\x14\x92\x84\x20\x87"
    }
};

struct tests aes_cfb_tests[] = {
    { "aes-cfb8-128",
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      "\x66\xe9\x4b\xd4\xef\x8a\x2c\x3b\x88\x4c\xfa\x59\xca\x34\x2b\x2e"
    }
};

struct tests rc2_40_tests[] = {
    { "rc2-40",
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      "\xc0\xb8\xff\xa5\xd6\xeb\xc9\x62\xcc\x52\x5f\xfe\x9a\x3c\x97\xe6"
    }
};

struct tests des_ede3_tests[] = {
    { "des-ede3",
      "\x19\x17\xff\xe6\xbb\x77\x2e\xfc"
      "\x29\x76\x43\xbc\x63\x56\x7e\x9a"
      "\x00\x2e\x4d\x43\x1d\x5f\xfd\x58",
      24,
      "\xbf\x9a\x12\xb7\x26\x69\xfd\x05",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      "\x55\x95\x97\x76\xa9\x6c\x66\x40\x64\xc7\xf4\x1c\x21\xb7\x14\x1b"
    }
};

struct tests camellia128_tests[] = {
    { "camellia128",
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      16,
      "\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
      "\x07\x92\x3A\x39\xEB\x0A\x81\x7D\x1C\x4D\x87\xBD\xB8\x2D\x1F\x1C",
      NULL
    }
};

struct tests rc4_tests[] = {
    {
	"rc4 8",
	"\x01\x23\x45\x67\x89\xAB\xCD\xEF",
	8,
	NULL,
	8,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\x74\x94\xC2\xE7\x10\x4B\x08\x79",
	NULL
    },
    {
	"rc4 5",
	"\x61\x8a\x63\xd2\xfb",
	5,
	NULL,
	5,
	"\xdc\xee\x4c\xf9\x2c",
	"\xf1\x38\x29\xc9\xde",
	NULL
    },
    {
	"rc4 309",
	"\x29\x04\x19\x72\xfb\x42\xba\x5f\xc7\x12\x77\x12\xf1\x38\x29\xc9",
	16,
	NULL,
	309,
	"\x52\x75\x69\x73\x6c\x69\x6e\x6e"
	"\x75\x6e\x20\x6c\x61\x75\x6c\x75"
	"\x20\x6b\x6f\x72\x76\x69\x73\x73"
	"\x73\x61\x6e\x69\x2c\x20\x74\xe4"
	"\x68\x6b\xe4\x70\xe4\x69\x64\x65"
	"\x6e\x20\x70\xe4\xe4\x6c\x6c\xe4"
	"\x20\x74\xe4\x79\x73\x69\x6b\x75"
	"\x75\x2e\x20\x4b\x65\x73\xe4\x79"
	"\xf6\x6e\x20\x6f\x6e\x20\x6f\x6e"
	"\x6e\x69\x20\x6f\x6d\x61\x6e\x61"
	"\x6e\x69\x2c\x20\x6b\x61\x73\x6b"
	"\x69\x73\x61\x76\x75\x75\x6e\x20"
	"\x6c\x61\x61\x6b\x73\x6f\x74\x20"
	"\x76\x65\x72\x68\x6f\x75\x75\x2e"
	"\x20\x45\x6e\x20\x6d\x61\x20\x69"
	"\x6c\x6f\x69\x74\x73\x65\x2c\x20"
	"\x73\x75\x72\x65\x20\x68\x75\x6f"
	"\x6b\x61\x61\x2c\x20\x6d\x75\x74"
	"\x74\x61\x20\x6d\x65\x74\x73\xe4"
	"\x6e\x20\x74\x75\x6d\x6d\x75\x75"
	"\x73\x20\x6d\x75\x6c\x6c\x65\x20"
	"\x74\x75\x6f\x6b\x61\x61\x2e\x20"
	"\x50\x75\x75\x6e\x74\x6f\x20\x70"
	"\x69\x6c\x76\x65\x6e\x2c\x20\x6d"
	"\x69\x20\x68\x75\x6b\x6b\x75\x75"
	"\x2c\x20\x73\x69\x69\x6e\x74\x6f"
	"\x20\x76\x61\x72\x61\x6e\x20\x74"
	"\x75\x75\x6c\x69\x73\x65\x6e\x2c"
	"\x20\x6d\x69\x20\x6e\x75\x6b\x6b"
	"\x75\x75\x2e\x20\x54\x75\x6f\x6b"
	"\x73\x75\x74\x20\x76\x61\x6e\x61"
	"\x6d\x6f\x6e\x20\x6a\x61\x20\x76"
	"\x61\x72\x6a\x6f\x74\x20\x76\x65"
	"\x65\x6e\x2c\x20\x6e\x69\x69\x73"
	"\x74\xe4\x20\x73\x79\x64\xe4\x6d"
	"\x65\x6e\x69\x20\x6c\x61\x75\x6c"
	"\x75\x6e\x20\x74\x65\x65\x6e\x2e"
	"\x20\x2d\x20\x45\x69\x6e\x6f\x20"
	"\x4c\x65\x69\x6e\x6f",
	"\x35\x81\x86\x99\x90\x01\xe6\xb5"
	"\xda\xf0\x5e\xce\xeb\x7e\xee\x21"
	"\xe0\x68\x9c\x1f\x00\xee\xa8\x1f"
	"\x7d\xd2\xca\xae\xe1\xd2\x76\x3e"
	"\x68\xaf\x0e\xad\x33\xd6\x6c\x26"
	"\x8b\xc9\x46\xc4\x84\xfb\xe9\x4c"
	"\x5f\x5e\x0b\x86\xa5\x92\x79\xe4"
	"\xf8\x24\xe7\xa6\x40\xbd\x22\x32"
	"\x10\xb0\xa6\x11\x60\xb7\xbc\xe9"
	"\x86\xea\x65\x68\x80\x03\x59\x6b"
	"\x63\x0a\x6b\x90\xf8\xe0\xca\xf6"
	"\x91\x2a\x98\xeb\x87\x21\x76\xe8"
	"\x3c\x20\x2c\xaa\x64\x16\x6d\x2c"
	"\xce\x57\xff\x1b\xca\x57\xb2\x13"
	"\xf0\xed\x1a\xa7\x2f\xb8\xea\x52"
	"\xb0\xbe\x01\xcd\x1e\x41\x28\x67"
	"\x72\x0b\x32\x6e\xb3\x89\xd0\x11"
	"\xbd\x70\xd8\xaf\x03\x5f\xb0\xd8"
	"\x58\x9d\xbc\xe3\xc6\x66\xf5\xea"
	"\x8d\x4c\x79\x54\xc5\x0c\x3f\x34"
	"\x0b\x04\x67\xf8\x1b\x42\x59\x61"
	"\xc1\x18\x43\x07\x4d\xf6\x20\xf2"
	"\x08\x40\x4b\x39\x4c\xf9\xd3\x7f"
	"\xf5\x4b\x5f\x1a\xd8\xf6\xea\x7d"
	"\xa3\xc5\x61\xdf\xa7\x28\x1f\x96"
	"\x44\x63\xd2\xcc\x35\xa4\xd1\xb0"
	"\x34\x90\xde\xc5\x1b\x07\x11\xfb"
	"\xd6\xf5\x5f\x79\x23\x4d\x5b\x7c"
	"\x76\x66\x22\xa6\x6d\xe9\x2b\xe9"
	"\x96\x46\x1d\x5e\x4d\xc8\x78\xef"
	"\x9b\xca\x03\x05\x21\xe8\x35\x1e"
	"\x4b\xae\xd2\xfd\x04\xf9\x46\x73"
	"\x68\xc4\xad\x6a\xc1\x86\xd0\x82"
	"\x45\xb2\x63\xa2\x66\x6d\x1f\x6c"
	"\x54\x20\xf1\x59\x9d\xfd\x9f\x43"
	"\x89\x21\xc2\xf5\xa4\x63\x93\x8c"
	"\xe0\x98\x22\x65\xee\xf7\x01\x79"
	"\xbc\x55\x3f\x33\x9e\xb1\xa4\xc1"
	"\xaf\x5f\x6a\x54\x7f"
    }
};


static int
test_cipher(int i, const EVP_CIPHER *c, struct tests *t)
{
    EVP_CIPHER_CTX ectx;
    EVP_CIPHER_CTX dctx;
    void *d;

    if (c == NULL) {
	printf("%s not supported\n", t->name);
	return 0;
    }

    EVP_CIPHER_CTX_init(&ectx);
    EVP_CIPHER_CTX_init(&dctx);

    if (EVP_CipherInit_ex(&ectx, c, NULL, NULL, NULL, 1) != 1)
	errx(1, "%s: %d EVP_CipherInit_ex einit", t->name, i);
    if (EVP_CipherInit_ex(&dctx, c, NULL, NULL, NULL, 0) != 1)
	errx(1, "%s: %d EVP_CipherInit_ex dinit", t->name, i);

    EVP_CIPHER_CTX_set_key_length(&ectx, t->keysize);
    EVP_CIPHER_CTX_set_key_length(&dctx, t->keysize);

    if (EVP_CipherInit_ex(&ectx, NULL, NULL, t->key, t->iv, 1) != 1)
	errx(1, "%s: %d EVP_CipherInit_ex encrypt", t->name, i);
    if (EVP_CipherInit_ex(&dctx, NULL, NULL, t->key, t->iv, 0) != 1)
	errx(1, "%s: %d EVP_CipherInit_ex decrypt", t->name, i);

    d = emalloc(t->datasize);

    if (!EVP_Cipher(&ectx, d, t->indata, t->datasize))
	return 1;

    if (memcmp(d, t->outdata, t->datasize) != 0) {
	char *s, *s2;
	hex_encode(d, t->datasize, &s);
	hex_encode(t->outdata, t->datasize, &s2);
	errx(1, "%s: %d encrypt not the same: %s != %s", t->name, i, s, s2);
    }

    if (!EVP_Cipher(&dctx, d, d, t->datasize))
	return 1;

    if (memcmp(d, t->indata, t->datasize) != 0) {
	char *s;
	hex_encode(d, t->datasize, &s);
	errx(1, "%s: %d decrypt not the same: %s", t->name, i, s);
    }
    if (t->outiv)
	/* XXXX check  */;

    EVP_CIPHER_CTX_cleanup(&ectx);
    EVP_CIPHER_CTX_cleanup(&dctx);
    free(d);

    return 0;
}

static int version_flag;
static int help_flag;

static struct getargs args[] = {
    { "version",	0,	arg_flag,	&version_flag,
      "print version", NULL },
    { "help",		0,	arg_flag,	&help_flag,
      NULL, 	NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int ret = 0;
    int i, idx = 0;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &idx))
	usage(1);

    if (help_flag)
	usage(0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    argc -= idx;
    argv += idx;

    /* hcrypto */
    for (i = 0; i < sizeof(aes_tests)/sizeof(aes_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_aes_256_cbc(), &aes_tests[i]);
    for (i = 0; i < sizeof(aes_cfb_tests)/sizeof(aes_cfb_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_aes_128_cfb8(), &aes_cfb_tests[i]);

    for (i = 0; i < sizeof(rc2_40_tests)/sizeof(rc2_40_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_rc2_40_cbc(), &rc2_40_tests[i]);
    for (i = 0; i < sizeof(des_ede3_tests)/sizeof(des_ede3_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_des_ede3_cbc(), &des_ede3_tests[i]);
    for (i = 0; i < sizeof(camellia128_tests)/sizeof(camellia128_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_camellia_128_cbc(),
			   &camellia128_tests[i]);
    for (i = 0; i < sizeof(rc4_tests)/sizeof(rc4_tests[0]); i++)
	ret += test_cipher(i, EVP_hcrypto_rc4(), &rc4_tests[i]);

    /* Common Crypto */
#ifdef __APPLE__
    for (i = 0; i < sizeof(aes_tests)/sizeof(aes_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_aes_256_cbc(), &aes_tests[i]);
#if 0
    for (i = 0; i < sizeof(aes_cfb_tests)/sizeof(aes_cfb_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_aes_128_cfb8(), &aes_cfb_tests[i]);
#endif
    for (i = 0; i < sizeof(rc2_40_tests)/sizeof(rc2_40_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_rc2_40_cbc(), &rc2_40_tests[i]);
    for (i = 0; i < sizeof(des_ede3_tests)/sizeof(des_ede3_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_des_ede3_cbc(), &des_ede3_tests[i]);
    for (i = 0; i < sizeof(camellia128_tests)/sizeof(camellia128_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_camellia_128_cbc(),
			   &camellia128_tests[i]);
    for (i = 0; i < sizeof(rc4_tests)/sizeof(rc4_tests[0]); i++)
	ret += test_cipher(i, EVP_cc_rc4(), &rc4_tests[i]);
#endif

    return ret;
}
