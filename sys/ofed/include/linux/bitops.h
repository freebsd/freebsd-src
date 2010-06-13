/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */
#ifndef	_LINUX_BITOPS_H_
#define	_LINUX_BITOPS_H_

#define	BITS_PER_LONG		(sizeof(long) * 8)
#define	BIT_MASK(n)		(~0UL >> (BITS_PER_LONG - (n)))
#define	BITS_TO_LONGS(n)	roundup2((n), BITS_PER_LONG)

static inline int
__ffsl(long mask)
{
	return (ffsl(mask) - 1);
}

static inline int
__flsl(long mask)
{
	return (flsl(mask) - 1);
}

static inline unsigned long
find_first_bit(unsigned long *addr, unsigned long size)
{
	int mask;
	int bit;

	for (bit = 0; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (*addr == 0)
			continue;
		return (bit + __ffsl(*addr));
	}
	if (size) {
		mask = (*addr) & BIT_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	return (bit);
}

static inline unsigned long
find_first_zero_bit(unsigned long *addr, unsigned long size)
{
	int mask;
	int bit;

	for (bit = 0; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (~(*addr) == 0)
			continue;
		return (bit + __ffsl(~(*addr)));
	}
	if (size) {
		mask = ~(*addr) & BIT_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	return (bit);
}

static inline unsigned long
find_last_bit(unsigned long *addr, unsigned long size)
{
	int mask;
	int offs;
	int bit;
	int pos;

	pos = size / BITS_PER_LONG;
	offs = size % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = (*addr) & BIT_MASK(offs);
		if (mask)
			return (bit + __flsl(mask));
	}
	while (--pos) {
		addr--;
		bit -= BITS_PER_LONG;
		if (*addr)
			return (bit + __flsl(mask));
	}
	return (size);
}

static inline unsigned long
find_next_bit(unsigned long *addr, unsigned long size, unsigned long offset)
{
	int mask;
	int offs;
	int bit;
	int pos;

	pos = offset / BITS_PER_LONG;
	offs = size % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = (*addr) & ~BIT_MASK(offs);
		if (mask)
			return (bit + __ffsl(mask));
		bit += BITS_PER_LONG;
		addr++;
	}
	for (size -= bit; size >= BITS_PER_LONG;
	    size -= BITS_PER_LONG, bit += BITS_PER_LONG, addr++) {
		if (*addr == 0)
			continue;
		return (bit + __ffsl(*addr));
	}
	if (size) {
		mask = (*addr) & BIT_MASK(size);
		if (mask)
			bit += __ffsl(mask);
		else
			bit += size;
	}
	return (bit);
}

#define	NBINT	(NBBY * sizeof(int))

#define	set_bit(i, a)							\
    atomic_set_int((volatile int *)(a)[(i)/NBINT], (i) % NBINT)

#define	clear_bit(i, a)							\
    atomic_clear_int((volatile int *)(a)[(i)/NBINT], (i) % NBINT)

#define	test_bit(i, a)							\
    !!(atomic_load_acq_int((volatile int *)(a)[(i)/NBINT]) & 1 << ((i) % NBINT))

#endif	/* _LINUX_BITOPS_H_ */
