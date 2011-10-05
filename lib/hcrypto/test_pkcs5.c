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

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include <evp.h>

struct tests {
    const char *password;
    const char *salt;
    int iterations;
    const void *pbkdf2_128;
    const void *pbkdf2_256;
};


const struct tests pkcs5_tests[] = {
    { "password",
      "ATHENA.MIT.EDUraeburn",
      1,
      "\xcd\xed\xb5\x28\x1b\xb2\xf8\x01\x56\x5a\x11\x22\xb2\x56\x35\x15",
      "\xcd\xed\xb5\x28\x1b\xb2\xf8\x01\x56\x5a\x11\x22\xb2\x56\x35\x15"
      "\x0a\xd1\xf7\xa0\x4b\xb9\xf3\xa3\x33\xec\xc0\xe2\xe1\xf7\x08\x37"
    },
    { "password",
      "ATHENA.MIT.EDUraeburn",
      2,
      "\x01\xdb\xee\x7f\x4a\x9e\x24\x3e\x98\x8b\x62\xc7\x3c\xda\x93\x5d",
      "\x01\xdb\xee\x7f\x4a\x9e\x24\x3e\x98\x8b\x62\xc7\x3c\xda\x93\x5d"
      "\xa0\x53\x78\xb9\x32\x44\xec\x8f\x48\xa9\x9e\x61\xad\x79\x9d\x86"
    },
    { "password",
      "ATHENA.MIT.EDUraeburn",
      1200,
      "\x5c\x08\xeb\x61\xfd\xf7\x1e\x4e\x4e\xc3\xcf\x6b\xa1\xf5\x51\x2b",
      "\x5c\x08\xeb\x61\xfd\xf7\x1e\x4e\x4e\xc3\xcf\x6b\xa1\xf5\x51\x2b"
      "\xa7\xe5\x2d\xdb\xc5\xe5\x14\x2f\x70\x8a\x31\xe2\xe6\x2b\x1e\x13"
    },
    {
	"password", "\x12\x34\x56\x78\x78\x56\x34\x12",
	5,
	"\xd1\xda\xa7\x86\x15\xf2\x87\xe6\xa1\xc8\xb1\x20\xd7\x06\x2a\x49",
	"\xd1\xda\xa7\x86\x15\xf2\x87\xe6\xa1\xc8\xb1\x20\xd7\x06\x2a\x49"
	"\x3f\x98\xd2\x03\xe6\xbe\x49\xa6\xad\xf4\xfa\x57\x4b\x6e\x64\xee"
    },
    {
	"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
	"pass phrase equals block size",
	1200,
	"\x13\x9c\x30\xc0\x96\x6b\xc3\x2b\xa5\x5f\xdb\xf2\x12\x53\x0a\xc9",
	"\x13\x9c\x30\xc0\x96\x6b\xc3\x2b\xa5\x5f\xdb\xf2\x12\x53\x0a\xc9"
	"\xc5\xec\x59\xf1\xa4\x52\xf5\xcc\x9a\xd9\x40\xfe\xa0\x59\x8e\xd1"
    },
    {
	"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
	"pass phrase exceeds block size",
	1200,
	"\x9c\xca\xd6\xd4\x68\x77\x0c\xd5\x1b\x10\xe6\xa6\x87\x21\xbe\x61",
	"\x9c\xca\xd6\xd4\x68\x77\x0c\xd5\x1b\x10\xe6\xa6\x87\x21\xbe\x61"
	"\x1a\x8b\x4d\x28\x26\x01\xdb\x3b\x36\xbe\x92\x46\x91\x5e\xc8\x2a"
    },
    {
	"\xf0\x9d\x84\x9e" /* g-clef */,
	"EXAMPLE.COMpianist",
	50,
	"\x6b\x9c\xf2\x6d\x45\x45\x5a\x43\xa5\xb8\xbb\x27\x6a\x40\x3b\x39",
	"\x6b\x9c\xf2\x6d\x45\x45\x5a\x43\xa5\xb8\xbb\x27\x6a\x40\x3b\x39"
	"\xe7\xfe\x37\xa0\xc4\x1e\x02\xc2\x81\xff\x30\x69\xe1\xe9\x4f\x52"
    }
};

static int
test_pkcs5_pbe2(const struct tests *t)
{
    unsigned char key[32];
    int ret, error = 0;

    ret = PKCS5_PBKDF2_HMAC_SHA1(t->password, strlen(t->password),
				 t->salt, strlen(t->salt),
				 t->iterations,
				 16, key);
    if (ret != 1)
	errx(1, "PKCS5_PBKDF2_HMAC_SHA1: %d", ret);

    if (memcmp(t->pbkdf2_128, key, 16) != 0) {
	printf("incorrect 128 key\n");
	error++;
    }

    ret = PKCS5_PBKDF2_HMAC_SHA1(t->password, strlen(t->password),
				 t->salt, strlen(t->salt),
				 t->iterations,
				 32, key);
    if (ret != 1)
	errx(1, "PKCS5_PBKDF2_HMAC_SHA1: %d", ret);

    if (memcmp(t->pbkdf2_256, key, 32) != 0) {
	printf("incorrect 256 key\n");
	error++;
    }

    return error;
}

int
main(int argc, char **argv)
{
    int ret = 0;
    int i;

    for (i = 0; i < sizeof(pkcs5_tests)/sizeof(pkcs5_tests[0]); i++)
	ret += test_pkcs5_pbe2(&pkcs5_tests[i]);

    return ret;
}
