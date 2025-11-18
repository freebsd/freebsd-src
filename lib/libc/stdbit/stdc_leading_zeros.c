/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <stdbit.h>

/* Offset must be greater than zero. */
static_assert(UCHAR_WIDTH < UINT_WIDTH,
    "stdc_leading_zeros_uc needs UCHAR_WIDTH < UINT_WIDTH");

unsigned int
stdc_leading_zeros_uc(unsigned char x)
{
	const int offset = UINT_WIDTH - UCHAR_WIDTH;

	return (__builtin_clz((x << offset) + (1U << (offset - 1))));
}

/* Offset must be greater than zero. */
static_assert(USHRT_WIDTH < UINT_WIDTH,
    "stdc_leading_zeros_us needs USHRT_WIDTH < UINT_WIDTH");

unsigned int
stdc_leading_zeros_us(unsigned short x)
{
	const int offset = UINT_WIDTH - USHRT_WIDTH;

	return (__builtin_clz((x << offset) + (1U << (offset - 1))));
}

unsigned int
stdc_leading_zeros_ui(unsigned int x)
{
	if (x == 0)
		return (UINT_WIDTH);

	return (__builtin_clz(x));
}

unsigned int
stdc_leading_zeros_ul(unsigned long x)
{
	if (x == 0)
		return (ULONG_WIDTH);

	return (__builtin_clzl(x));
}

unsigned int
stdc_leading_zeros_ull(unsigned long long x)
{
	if (x == 0)
		return (ULLONG_WIDTH);

	return (__builtin_clzll(x));
}
