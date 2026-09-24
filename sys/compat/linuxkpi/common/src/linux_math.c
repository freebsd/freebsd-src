/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Netflix, Inc.
 */

#include <linux/math.h>

/*
 * Based on the implementation of `isqrt64()` from `sys/cam/cam_iosched.c` as
 * of FreeBSD commit 153446ecd740702af00cf9b09d8cd39f6d397dd3. The return value
 * was changed from `uint64_t` to `unsigned long`.
 */
unsigned long
linuxkpi_int_sqrt(unsigned long val)
{
	unsigned long res = 0;
	unsigned long bit = 1ULL << (sizeof(unsigned long) * NBBY - 2);

	/*
	 * Find the largest power of 4 smaller than val.
	 */
	while (bit > val)
		bit >>= 2;

	/*
	 * Accumulate the answer, one bit at a time (we keep moving
	 * them over since 2 is the square root of 4 and we test
	 * powers of 4). We accumulate where we find the bit, but
	 * the successive shifts land the bit in the right place
	 * by the end.
	 */
	while (bit != 0) {
		if (val >= res + bit) {
			val -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;
		bit >>= 2;
	}

	return res;
}
