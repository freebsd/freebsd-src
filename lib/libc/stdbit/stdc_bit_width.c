/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>
#include <stdbit.h>

unsigned int
stdc_bit_width_uc(unsigned char x)
{
	if (x == 0)
		return (0);

	return (UINT_WIDTH - __builtin_clz(x));
}

unsigned int
stdc_bit_width_us(unsigned short x)
{
	if (x == 0)
		return (0);

	return (UINT_WIDTH - __builtin_clz(x));
}

unsigned int
stdc_bit_width_ui(unsigned int x)
{
	if (x == 0)
		return (0);

	return (UINT_WIDTH - __builtin_clz(x));
}

unsigned int
stdc_bit_width_ul(unsigned long x)
{
	if (x == 0)
		return (0);

	return (ULONG_WIDTH - __builtin_clzl(x));
}

unsigned int
stdc_bit_width_ull(unsigned long long x)
{
	if (x == 0)
		return (0);

	return (ULLONG_WIDTH - __builtin_clzll(x));
}
