/*-
 * SPDX-License-Identifier: 0BSD
 *
 * Copyright (c) Robert Clausecker <fuz@FreeBSD.org>
 * Based on a publication by Daniel Lemire.
 * Public domain where applicable.
 *
 * Daniel Lemire, "Fast Random Integer Generation in an Interval",
 * Association for Computing Machinery, ACM Trans. Model. Comput. Simul.,
 * no. 1, vol. 29, pp. 1--12, New York, NY, USA, January 2019.
 */

#include <stdint.h>
#include <stdlib.h>

uint32_t
arc4random_uniform(uint32_t upper_bound)
{
	uint64_t product;

	/*
	 * The paper uses these variable names:
	 *
	 * L -- log2(UINT32_MAX+1)
	 * s -- upper_bound
	 * x -- arc4random() return value
	 * m -- product
	 * l -- (uint32_t)product
	 * t -- threshold
	 */

	if (upper_bound <= 1)
		return (0);

	product = upper_bound * (uint64_t)arc4random();

	if ((uint32_t)product < upper_bound) {
		uint32_t threshold;

		/* threshold = (2**32 - upper_bound) % upper_bound */
		threshold = -upper_bound % upper_bound;
		while ((uint32_t)product < threshold)
			product = upper_bound * (uint64_t)arc4random();
	}

	return (product >> 32);
}
