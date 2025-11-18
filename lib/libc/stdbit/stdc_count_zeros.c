/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbit.h>

unsigned int
stdc_count_zeros_uc(unsigned char x)
{
	return (__builtin_popcount(x ^ UCHAR_MAX));
}

unsigned int
stdc_count_zeros_us(unsigned short x)
{
	return (__builtin_popcount(x ^ USHRT_MAX));
}

unsigned int
stdc_count_zeros_ui(unsigned int x)
{
	return (__builtin_popcount(~x));
}

unsigned int
stdc_count_zeros_ul(unsigned long x)
{
	return (__builtin_popcountl(~x));
}

unsigned int
stdc_count_zeros_ull(unsigned long long x)
{
	return (__builtin_popcountll(~x));
}
