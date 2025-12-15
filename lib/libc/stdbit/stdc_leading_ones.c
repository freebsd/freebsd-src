/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <stdbit.h>

/* Avoid triggering undefined behavior if x == 0. */
static_assert(UCHAR_WIDTH < UINT_WIDTH,
    "stdc_leading_ones_uc needs UCHAR_WIDTH < UINT_WIDTH");

unsigned int
stdc_leading_ones_uc(unsigned char x)
{
	const int offset = UINT_WIDTH - UCHAR_WIDTH;

	return (__builtin_clz(~(x << offset)));
}

/* Avoid triggering undefined behavior if x == 0. */
static_assert(USHRT_WIDTH < UINT_WIDTH,
    "stdc_leading_ones_us needs USHRT_WIDTH < UINT_WIDTH");

unsigned int
stdc_leading_ones_us(unsigned short x)
{
	const int offset = UINT_WIDTH - USHRT_WIDTH;

	return (__builtin_clz(~(x << offset)));
}

unsigned int
stdc_leading_ones_ui(unsigned int x)
{
	if (x == ~0U)
		return (UINT_WIDTH);

	return (__builtin_clz(~x));
}

unsigned int
stdc_leading_ones_ul(unsigned long x)
{
	if (x == ~0UL)
		return (ULONG_WIDTH);

	return (__builtin_clzl(~x));
}

unsigned int
stdc_leading_ones_ull(unsigned long long x)
{
	if (x == ~0ULL)
		return (ULLONG_WIDTH);

	return (__builtin_clzll(~x));
}
