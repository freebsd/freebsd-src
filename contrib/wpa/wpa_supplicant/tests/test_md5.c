/*
 * Test program for MD5 (test vectors from RFC 1321)
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
			"\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04"
			"\xe9\x80\x09\x98\xec\xf8\x42\x7e"
		},
		{
			"a",
			"\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8"
			"\x31\xc3\x99\xe2\x69\x77\x26\x61"
		},
		{
			"abc",
			"\x90\x01\x50\x98\x3c\xd2\x4f\xb0"
			"\xd6\x96\x3f\x7d\x28\xe1\x7f\x72"
		},
		{
			"message digest",
			"\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d"
			"\x52\x5a\x2f\x31\xaa\xf1\x61\xd0"
		},
		{
			"abcdefghijklmnopqrstuvwxyz",
			"\xc3\xfc\xd3\xd7\x61\x92\xe4\x00"
			"\x7d\xfb\x49\x6c\xca\x67\xe1\x3b"
		},
		{
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
			"0123456789",
			"\xd1\x74\xab\x98\xd2\x77\xd9\xf5"
			"\xa5\x61\x1c\x2c\x9f\x41\x9d\x9f"
		},
		{
			"12345678901234567890123456789012345678901234567890"
			"123456789012345678901234567890",
			"\x57\xed\xf4\xa2\x2b\xe3\xc9\x55"
			"\xac\x49\xda\x2e\x21\x07\xb6\x7a"
		}
	};
	unsigned int i;
	u8 hash[16];
	const u8 *addr[2];
	size_t len[2];
	int errors = 0;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		printf("MD5 test case %d:", i);

		addr[0] = tests[i].data;
		len[0] = strlen(tests[i].data);
		md5_vector(1, addr, len, hash);
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
			md5_vector(1, addr, len, hash);
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
