/*-
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SYS_GNU_EXT2FS_EXT2_BITOPS_H_
#define _SYS_GNU_EXT2FS_EXT2_BITOPS_H_

#define	find_first_zero_bit(data, sz)		find_next_zero_bit(data, sz, 0)

static __inline int
clear_bit(int no, void *data)
{
	uint32_t *p;
	uint32_t mask, new, old;

	p = (uint32_t*)data + (no >> 5);
	mask = (1U << (no & 31));
	do {
		old = *p;
		new = old & ~mask;
	} while (!atomic_cmpset_32(p, old, new));
	return (old & mask);
}

static __inline int
set_bit(int no, void *data)
{
	uint32_t *p;
	uint32_t mask, new, old;

	p = (uint32_t*)data + (no >> 5);
	mask = (1U << (no & 31));
	do {
		old = *p;
		new = old | mask;
	} while (!atomic_cmpset_32(p, old, new));
	return (old & mask);
}

static __inline int
test_bit(int no, void *data)
{
	uint32_t *p;
	uint32_t mask;

	p = (uint32_t*)data + (no >> 5);
	mask = (1U << (no & 31));
	return (*p & mask);
}

static __inline size_t
find_next_zero_bit(void *data, size_t sz, size_t ofs)
{
	uint32_t *p;
	uint32_t mask;
	int bit;

	p = (uint32_t*)data + (ofs >> 5);
	if (ofs & 31) {
		mask = ~0U << (ofs & 31);
		bit = *p | ~mask;
		if (bit != ~0U)
			return (ffs(~bit) + ofs - 1);
		p++;
		ofs = (ofs + 31U) & ~31U;
	}
	while(ofs < sz && *p == ~0U) {
		p++;
		ofs += 32;
	}
	if (ofs == sz)
		return (ofs);
	bit = *p;
	return (ffs(~bit) + ofs - 1);
}

static __inline void *
memscan(void *data, int c, size_t sz)
{
	uint8_t *p;

	p = data;
	while (sz && *p != c) {
		p++;
		sz--;
	}
	return (p);
}

#endif /* _SYS_GNU_EXT2FS_EXT2_BITOPS_H_ */
