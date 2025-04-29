/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2024 rilysh <nightquick@proton.me>
 */

#include <unistd.h>
#include <sys/endian.h>

void
swab(const void * __restrict from, void * __restrict to, ssize_t len)
{
	const uint16_t *f __aligned(1) = from;
	uint16_t *t __aligned(1) = to;

	/*
	 * POSIX says overlapping copy behavior is undefined, however many
	 * applications assume the old FreeBSD and current GNU libc behavior
	 * that will swap the bytes correctly when from == to. Reading both bytes
	 * and swapping them before writing them back accomplishes this.
	 */
	while (len > 1) {
		*t++ = bswap16(*f++);
		len -= 2;
	}
}
