/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

#include <rc2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct {
    const void *key;
    const int keylen;
    const int bitsize;
    const void *plain;
    const void *cipher;
} tests[] = {
    {
	"\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	16,
	0,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\x1C\x19\x8A\x83\x8D\xF0\x28\xB7"
    },
    {
	"\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x01",
	16,
	0,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\x21\x82\x9C\x78\xA9\xF9\xC0\x74"
    },
    {
	"\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	16,
	0,
	"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF",
	"\x13\xDB\x35\x17\xD3\x21\x86\x9E"
    },
    {
	"\x00\x01\x02\x03\x04\x05\x06\x07"
	"\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F",
	16,
	0,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\x50\xDC\x01\x62\xBD\x75\x7F\x31"
    },
    {
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	8,
	63,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\xeb\xb7\x73\xf9\x93\x27\x8e\xff"
    },
    {
	"\xff\xff\xff\xff\xff\xff\xff\xff",
	8,
	64,
	"\xff\xff\xff\xff\xff\xff\xff\xff",
	"\x27\x8b\x27\xe4\x2e\x2f\x0d\x49"
    },
    {
	"\x88",
	1,
	64,
	"\x00\x00\x00\x00\x00\x00\x00\x00",
	"\x61\xa8\xa2\x44\xad\xac\xcc\xf0"
    }
};

const unsigned char cbc_key[16] =
"\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00";
const char cbc_iv[8] =
"\x01\x01\x01\x01\x01\x01\x01\x01";
const unsigned char cbc_in_data[32] =
"\x20\x20\x20\x20\x20\x20\x20\x20"
"\x20\x20\x20\x20\x20\x20\x20\x20"
"\x20\x20\x20\x20\x20\x20\x20\x20"
"\x20\x20\x20\x20\x20\x20\x20\x20";

const char out_iv[8] = "\x00\x78\x1b\x6\xff\xb9\xfa\xe";

const char cbc_out_data[32] =
"\xb4\x3f\x89\x15\x69\x68\xda\x79"
"\x29\xab\x5f\x78\xc5\xba\x15\x82"
"\x80\x89\x57\x1b\xbe\x57\x2f\xdc"
"\x00\x78\x1b\x06\xff\xb9\xfa\x0e";

int
main(int argc, char **argv)
{
    RC2_KEY key;
    unsigned char t[8];
    unsigned char out[40];
    int i;

    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
	RC2_set_key(&key, tests[i].keylen, tests[i].key, tests[i].bitsize);

	memcpy(t, tests[i].plain, 8);
	RC2_encryptc(t, t, &key);
	if (memcmp(t, tests[i].cipher, 8) != 0) {
	    printf("encrypt %d\n", i);
	    exit(1);
	}
	RC2_decryptc(t, t, &key);
	if (memcmp(t, tests[i].plain, 8) != 0) {
	    printf("decrypt: %d\n", i);
	    exit(1);
	}
    }

    /* cbc test */

    RC2_set_key(&key, 16, cbc_key, 0);
    memcpy(t, cbc_iv, 8);
    RC2_cbc_encrypt(cbc_in_data, out, 32, &key, t, 1);

    if (memcmp(out_iv, t, 8) != 0)
	abort();

    if (memcmp(out, cbc_out_data, 32) != 0) {
	printf("cbc test encrypt\n");
	exit(1);
    }

    memcpy(t, cbc_iv, 8);
    RC2_cbc_encrypt(out, out, 32, &key, t, 0);

    if (memcmp(cbc_in_data, out, 32) != 0) {
	printf("cbc test decrypt \n");
	exit(1);
    }

    return 0;
}
