/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Copyright (C) 2015-2020 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 *
 * Specialized constant-time ctype.h reimplementations that aren't locale-specific.
 */

#ifndef CTYPE_H
#define CTYPE_H

#include <stdbool.h>

static inline bool char_is_space(int c)
{
	unsigned char d = c - 9;
	return (0x80001FU >> (d & 31)) & (1U >> (d >> 5));
}

static inline bool char_is_digit(int c)
{
	return (unsigned int)(('0' - 1 - c) & (c - ('9' + 1))) >> (sizeof(c) * 8 - 1);
}

static inline bool char_is_alpha(int c)
{
	return (unsigned int)(('a' - 1 - (c | 32)) & ((c | 32) - ('z' + 1))) >> (sizeof(c) * 8 - 1);
}

#endif
