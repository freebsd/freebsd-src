/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <stdbit.h>

/* Ensure we do not shift 1U out of range. */
static_assert(UCHAR_WIDTH < UINT_WIDTH,
    "stdc_trailing_zeros_uc needs UCHAR_WIDTH < UINT_WIDTH");

unsigned int
stdc_trailing_zeros_uc(unsigned char x)
{
	return (__builtin_ctz(x | 1U << UCHAR_WIDTH));
}

/* Ensure we do not shift 1U out of range. */
static_assert(USHRT_WIDTH < UINT_WIDTH,
    "stdc_trailing_zeros_uc needs USHRT_WIDTH < UINT_WIDTH");

unsigned int
stdc_trailing_zeros_us(unsigned short x)
{
	return (__builtin_ctz(x | 1U << USHRT_WIDTH));
}

unsigned int
stdc_trailing_zeros_ui(unsigned int x)
{
	if (x == 0U)
		return (UINT_WIDTH);

	return (__builtin_ctz(x));
}

unsigned int
stdc_trailing_zeros_ul(unsigned long x)
{
	if (x == 0UL)
		return (ULONG_WIDTH);

	return (__builtin_ctzl(x));
}

unsigned int
stdc_trailing_zeros_ull(unsigned long long x)
{
	if (x == 0ULL)
		return (ULLONG_WIDTH);

	return (__builtin_ctzll(x));
}
