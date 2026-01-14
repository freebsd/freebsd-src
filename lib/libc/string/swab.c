/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2024 rilysh <nightquick@proton.me>
 */

#include <string.h>
#include <unistd.h>
#include <sys/endian.h>

void
swab(const void * __restrict from, void * __restrict to, ssize_t len)
{
	const char *f = from;
	char *t = to;
	uint16_t tmp;

	/*
	 * POSIX says overlapping copy behavior is undefined, however many
	 * applications assume the old FreeBSD and current GNU libc behavior
	 * that will swap the bytes correctly when from == to. Reading both bytes
	 * and swapping them before writing them back accomplishes this.
	 */
	while (len > 1) {
		memcpy(&tmp, f, 2);
		tmp = bswap16(tmp);
		memcpy(t, &tmp, 2);

		f += 2;
		t += 2;
		len -= 2;
	}
}
