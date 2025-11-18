/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbit.h>

unsigned int
stdc_first_leading_zero_uc(unsigned char x)
{
	const int offset = UINT_WIDTH - UCHAR_WIDTH;

	if (x == UCHAR_MAX)
		return (0);

	return (__builtin_clz(~(unsigned int)x << offset) + 1);
}

unsigned int
stdc_first_leading_zero_us(unsigned short x)
{
	const int offset = UINT_WIDTH - USHRT_WIDTH;

	if (x == USHRT_MAX)
		return (0);

	return (__builtin_clz(~(unsigned int)x << offset) + 1);
}

unsigned int
stdc_first_leading_zero_ui(unsigned int x)
{
	if (x == ~0U)
		return (0);

	return (__builtin_clz(~x) + 1);
}

unsigned int
stdc_first_leading_zero_ul(unsigned long x)
{
	if (x == ~0UL)
		return (0);

	return (__builtin_clzl(~x) + 1);
}

unsigned int
stdc_first_leading_zero_ull(unsigned long long x)
{
	if (x == ~0ULL)
		return (0);

	return (__builtin_clzll(~x) + 1);
}
