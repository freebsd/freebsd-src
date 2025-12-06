/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbit.h>

unsigned int
stdc_first_trailing_zero_uc(unsigned char x)
{
	if (x == UCHAR_MAX)
		return (0);

	return (__builtin_ctz(~x) + 1);
}

unsigned int
stdc_first_trailing_zero_us(unsigned short x)
{
	if (x == USHRT_MAX)
		return (0);

	return (__builtin_ctz(~x) + 1);
}

unsigned int
stdc_first_trailing_zero_ui(unsigned int x)
{
	if (x == ~0U)
		return (0);

	return (__builtin_ctz(~x) + 1);
}

unsigned int
stdc_first_trailing_zero_ul(unsigned long x)
{
	if (x == ~0UL)
		return (0);

	return (__builtin_ctzl(~x) + 1);
}

unsigned int
stdc_first_trailing_zero_ull(unsigned long long x)
{
	if (x == ~0ULL)
		return (0);

	return (__builtin_ctzll(~x) + 1);
}
