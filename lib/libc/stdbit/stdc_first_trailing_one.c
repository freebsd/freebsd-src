/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbit.h>

unsigned int
stdc_first_trailing_one_uc(unsigned char x)
{
	if (x == 0)
		return (0);

	return (__builtin_ctz(x) + 1);
}

unsigned int
stdc_first_trailing_one_us(unsigned short x)
{
	if (x == 0)
		return (0);

	return (__builtin_ctz(x) + 1);
}

unsigned int
stdc_first_trailing_one_ui(unsigned int x)
{
	if (x == 0)
		return (0);

	return (__builtin_ctz(x) + 1);
}

unsigned int
stdc_first_trailing_one_ul(unsigned long x)
{
	if (x == 0)
		return (0);

	return (__builtin_ctzl(x) + 1);
}

unsigned int
stdc_first_trailing_one_ull(unsigned long long x)
{
	if (x == 0)
		return (0);

	return (__builtin_ctzll(x) + 1);
}
