// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include <errno.h>
#include <stdio.h>

#include "curve25519.h"
#include "encoding.h"
#include "subcommands.h"
#include "ctype.h"

int pubkey_main(int argc, const char *argv[])
{
	uint8_t key[WG_KEY_LEN] __attribute__((aligned(sizeof(uintptr_t))));
	char base64[WG_KEY_LEN_BASE64];
	int trailing_char;

	if (argc != 1) {
		fprintf(stderr, "Usage: %s %s\n", PROG_NAME, argv[0]);
		return 1;
	}

	if (fread(base64, 1, sizeof(base64) - 1, stdin) != sizeof(base64) - 1) {
		errno = EINVAL;
		fprintf(stderr, "%s: Key is not the correct length or format\n", PROG_NAME);
		return 1;
	}
	base64[WG_KEY_LEN_BASE64 - 1] = '\0';

	for (;;) {
		trailing_char = getc(stdin);
		if (!trailing_char || char_is_space(trailing_char))
			continue;
		if (trailing_char == EOF)
			break;
		fprintf(stderr, "%s: Trailing characters found after key\n", PROG_NAME);
		return 1;
	}

	if (!key_from_base64(key, base64)) {
		fprintf(stderr, "%s: Key is not the correct length or format\n", PROG_NAME);
		return 1;
	}
	curve25519_generate_public(key, key);
	key_to_base64(base64, key);
	puts(base64);
	return 0;
}
