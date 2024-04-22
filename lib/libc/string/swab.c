/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2024 rilysh <nightquick@proton.me>
 */

#include <unistd.h>

void
swab(const void * __restrict from, void * __restrict to, ssize_t len)
{
	const unsigned char *f = from;
	unsigned char *t = to;

	while (len > 1) {
		t[0] = f[1];
		t[1] = f[0];

		f += 2;
		t += 2;
		len -= 2;
	}
}
