/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <limits.h>
#include <stdbit.h>

/* Ensure we don't shift 1U out of range. */
static_assert(UCHAR_WIDTH < UINT_WIDTH,
    "stdc_bit_ceil_uc needs UCHAR_WIDTH < UINT_WIDTH");

unsigned char
stdc_bit_ceil_uc(unsigned char x)
{
	if (x <= 1)
		return (1);

	return (1U << (UINT_WIDTH - __builtin_clz(x - 1)));
}

/* Ensure we don't shift 1U out of range. */
static_assert(USHRT_WIDTH < UINT_WIDTH,
    "stdc_bit_ceil_us needs USHRT_WIDTH < UINT_WIDTH");

unsigned short
stdc_bit_ceil_us(unsigned short x)
{
	if (x <= 1)
		return (1);

	return (1U << (UINT_WIDTH - __builtin_clz(x - 1)));
}

unsigned int
stdc_bit_ceil_ui(unsigned int x)
{
	if (x <= 1)
		return (1);

	if (x > UINT_MAX/2 + 1)
		return (0);

	return (1U << (UINT_WIDTH - __builtin_clz(x - 1)));
}

unsigned long
stdc_bit_ceil_ul(unsigned long x)
{
	if (x <= 1)
		return (1);

	if (x > ULONG_MAX/2 + 1)
		return (0);

	return (1UL << (ULONG_WIDTH - __builtin_clzl(x - 1)));
}

unsigned long long
stdc_bit_ceil_ull(unsigned long long x)
{
	if (x <= 1)
		return (1);

	if (x > ULLONG_MAX/2 + 1)
		return (0);

	return (1ULL << (ULLONG_WIDTH - __builtin_clzll(x - 1)));
}
