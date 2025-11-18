/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbit.h>

unsigned int
stdc_count_ones_uc(unsigned char x)
{
	return (__builtin_popcount(x));
}

unsigned int
stdc_count_ones_us(unsigned short x)
{
	return (__builtin_popcount(x));
}

unsigned int
stdc_count_ones_ui(unsigned int x)
{
	return (__builtin_popcount(x));
}

unsigned int
stdc_count_ones_ul(unsigned long x)
{
	return (__builtin_popcountl(x));
}

unsigned int
stdc_count_ones_ull(unsigned long long x)
{
	return (__builtin_popcountll(x));
}
