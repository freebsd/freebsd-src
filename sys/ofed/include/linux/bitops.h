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

#ifdef __LP64__
#define	BITS_PER_LONG		64
#else
#define	BITS_PER_LONG		32
#endif
#define	BIT_MASK(n)		(~0UL >> (BITS_PER_LONG - (n)))
#define	BITS_TO_LONGS(n)	howmany((n), BITS_PER_LONG)

static inline int
__ffs(int mask)
{
	return (ffs(mask) - 1);
}

static inline int
__fls(int mask)
{
	return (fls(mask) - 1);
}

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


#define	ffz(mask)	__ffs(~(mask))

static inline unsigned long
find_first_bit(unsigned long *addr, unsigned long size)
{
	long mask;
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
	long mask;
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
	long mask;
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
	long mask;
	int offs;
	int bit;
	int pos;

	if (offset >= size)
		return (size);
	pos = offset / BITS_PER_LONG;
	offs = offset % BITS_PER_LONG;
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

static inline unsigned long
find_next_zero_bit(unsigned long *addr, unsigned long size,
    unsigned long offset)
{
	long mask;
	int offs;
	int bit;
	int pos;

	if (offset >= size)
		return (size);
	pos = offset / BITS_PER_LONG;
	offs = offset % BITS_PER_LONG;
	bit = BITS_PER_LONG * pos;
	addr += pos;
	if (offs) {
		mask = ~(*addr) & ~BIT_MASK(offs);
		if (mask)
			return (bit + __ffsl(mask));
		bit += BITS_PER_LONG;
		addr++;
	}
	for (size -= bit; size >= BITS_PER_LONG;
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

static inline void
bitmap_zero(unsigned long *addr, int size)
{
	int len;

	len = BITS_TO_LONGS(size) * sizeof(long);
	memset(addr, 0, len);
}

static inline void
bitmap_fill(unsigned long *addr, int size)
{
	int tail;
	int len;

	len = (size / BITS_PER_LONG) * sizeof(long);
	memset(addr, 0xff, len);
	tail = size & (BITS_PER_LONG - 1);
	if (tail) 
		addr[size / BITS_PER_LONG] = BIT_MASK(tail);
}

static inline int
bitmap_full(unsigned long *addr, int size)
{
	long mask;
	int tail;
	int len;
	int i;

	len = size / BITS_PER_LONG;
	for (i = 0; i < len; i++)
		if (addr[i] != ~0UL)
			return (0);
	tail = size & (BITS_PER_LONG - 1);
	if (tail) {
		mask = BIT_MASK(tail);
		if ((addr[i] & mask) != mask)
			return (0);
	}
	return (1);
}

static inline int
bitmap_empty(unsigned long *addr, int size)
{
	long mask;
	int tail;
	int len;
	int i;

	len = size / BITS_PER_LONG;
	for (i = 0; i < len; i++)
		if (addr[i] != 0)
			return (0);
	tail = size & (BITS_PER_LONG - 1);
	if (tail) {
		mask = BIT_MASK(tail);
		if ((addr[i] & mask) != 0)
			return (0);
	}
	return (1);
}

#define	NBINT	(NBBY * sizeof(int))

#define	set_bit(i, a)							\
    atomic_set_int(&((volatile int *)(a))[(i)/NBINT], 1 << (i) % NBINT)

#define	clear_bit(i, a)							\
    atomic_clear_int(&((volatile int *)(a))[(i)/NBINT], 1 << (i) % NBINT)

#define	test_bit(i, a)							\
    !!(atomic_load_acq_int(&((volatile int *)(a))[(i)/NBINT]) & 1 << ((i) % NBINT))

static inline long
test_and_clear_bit(long bit, long *var)
{
	long val;

	bit = 1 << bit;
	do {
		val = *(volatile long *)var;
	} while (atomic_cmpset_long(var, val, val & ~bit) == 0);

	return !!(val & bit);
}

static inline long
test_and_set_bit(long bit, long *var)
{
	long val;

	bit = 1 << bit;
	do {
		val = *(volatile long *)var;
	} while (atomic_cmpset_long(var, val, val | bit) == 0);

	return !!(val & bit);
}

#endif	/* _LINUX_BITOPS_H_ */
