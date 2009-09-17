/*
 * Test program for MD4 (test vectors from RFC 1320)
 * Copyright (c) 2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto.h"

int main(int argc, char *argv[])
{
	struct {
		char *data;
		u8 *hash;
	} tests[] = {
		{
			"",
			"\x31\xd6\xcf\xe0\xd1\x6a\xe9\x31"
			"\xb7\x3c\x59\xd7\xe0\xc0\x89\xc0"
		},
		{
			"a",
			"\xbd\xe5\x2c\xb3\x1d\xe3\x3e\x46"
			"\x24\x5e\x05\xfb\xdb\xd6\xfb\x24"
		},
		{
			"abc",
			"\xa4\x48\x01\x7a\xaf\x21\xd8\x52"
			"\x5f\xc1\x0a\xe8\x7a\xa6\x72\x9d"
		},
		{
			"message digest",
			"\xd9\x13\x0a\x81\x64\x54\x9f\xe8"
			"\x18\x87\x48\x06\xe1\xc7\x01\x4b"
		},
		{
			"abcdefghijklmnopqrstuvwxyz",
			"\xd7\x9e\x1c\x30\x8a\xa5\xbb\xcd"
			"\xee\xa8\xed\x63\xdf\x41\x2d\xa9"
		},
		{
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"0123456789",
			"\x04\x3f\x85\x82\xf2\x41\xdb\x35"
			"\x1c\xe6\x27\xe1\x53\xe7\xf0\xe4"
		},
		{
			"12345678901234567890123456789012345678901234567890"
			"123456789012345678901234567890",
			"\xe3\x3b\x4d\xdc\x9c\x38\xf2\x19"
			"\x9c\x3e\x7b\x16\x4f\xcc\x05\x36"
		}
	};
	unsigned int i;
	u8 hash[16];
	const u8 *addr[2];
	size_t len[2];
	int errors = 0;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		printf("MD4 test case %d:", i);

		addr[0] = tests[i].data;
		len[0] = strlen(tests[i].data);
		md4_vector(1, addr, len, hash);
		if (memcmp(hash, tests[i].hash, 16) != 0) {
			printf(" FAIL");
			errors++;
		} else
			printf(" OK");

		if (len[0]) {
			addr[0] = tests[i].data;
			len[0] = strlen(tests[i].data);
			addr[1] = tests[i].data + 1;
			len[1] = strlen(tests[i].data) - 1;
			md4_vector(1, addr, len, hash);
			if (memcmp(hash, tests[i].hash, 16) != 0) {
				printf(" FAIL");
				errors++;
			} else
				printf(" OK");
		}

		printf("\n");
	}

	return errors;
}
