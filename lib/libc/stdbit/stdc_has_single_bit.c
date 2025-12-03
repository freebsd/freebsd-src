/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbit.h>
#include <stdbool.h>

bool
stdc_has_single_bit_uc(unsigned char x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

bool
stdc_has_single_bit_us(unsigned short x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

bool
stdc_has_single_bit_ui(unsigned int x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

bool
stdc_has_single_bit_ul(unsigned long x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

bool
stdc_has_single_bit_ull(unsigned long long x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}
