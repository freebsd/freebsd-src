/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
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
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

#define BITS_PER_BYTE           8

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

static inline int get_count_order(unsigned int count)
{
        int order;

        order = fls(count) - 1;
        if (count & (count - 1))
                order++;
        return order;
}

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
	while (pos--) {
		addr--;
		bit -= BITS_PER_LONG;
		if (*addr)
			return (bit + __flsl(*addr));
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
		if (size - bit <= BITS_PER_LONG)
			return (size);
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
		if (size - bit <= BITS_PER_LONG)
			return (size);
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

#define	NBLONG	(NBBY * sizeof(long))

#define	__set_bit(i, a)							\
    atomic_set_long(&((volatile long *)(a))[(i)/NBLONG], 1UL << ((i) % NBLONG))

#define	set_bit(i, a)							\
    atomic_set_long(&((volatile long *)(a))[(i)/NBLONG], 1UL << ((i) % NBLONG))

#define	__clear_bit(i, a)						\
    atomic_clear_long(&((volatile long *)(a))[(i)/NBLONG], 1UL << ((i) % NBLONG))

#define	clear_bit(i, a)							\
    atomic_clear_long(&((volatile long *)(a))[(i)/NBLONG], 1UL << ((i) % NBLONG))

#define	test_bit(i, a)							\
    !!(atomic_load_acq_long(&((volatile long *)(a))[(i)/NBLONG]) &	\
    (1UL << ((i) % NBLONG)))

static inline long
test_and_clear_bit(long bit, long *var)
{
	long val;

	var += bit / (sizeof(long) * NBBY);
	bit %= sizeof(long) * NBBY;
	bit = (1UL << bit);
	do {
		val = *(volatile long *)var;
	} while (atomic_cmpset_long(var, val, val & ~bit) == 0);

	return !!(val & bit);
}

static inline long
test_and_set_bit(long bit, long *var)
{
	long val;

	var += bit / (sizeof(long) * NBBY);
	bit %= sizeof(long) * NBBY;
	bit = (1UL << bit);
	do {
		val = *(volatile long *)var;
	} while (atomic_cmpset_long(var, val, val | bit) == 0);

	return !!(val & bit);
}


#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))
#define BITMAP_LAST_WORD_MASK(nbits)                                    \
(                                                                       \
        ((nbits) % BITS_PER_LONG) ?                                     \
                (1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL               \
)


static inline void
bitmap_set(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_set = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_set = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_set >= 0) {
		*p |= mask_to_set;
		nr -= bits_to_set;
		bits_to_set = BITS_PER_LONG;
		mask_to_set = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_set &= BITMAP_LAST_WORD_MASK(size);
		*p |= mask_to_set;
	}
}

static inline void
bitmap_clear(unsigned long *map, int start, int nr)
{
	unsigned long *p = map + BIT_WORD(start);
	const int size = start + nr;
	int bits_to_clear = BITS_PER_LONG - (start % BITS_PER_LONG);
	unsigned long mask_to_clear = BITMAP_FIRST_WORD_MASK(start);

	while (nr - bits_to_clear >= 0) {
		*p &= ~mask_to_clear;
		nr -= bits_to_clear;
		bits_to_clear = BITS_PER_LONG;
		mask_to_clear = ~0UL;
		p++;
	}
	if (nr) {
		mask_to_clear &= BITMAP_LAST_WORD_MASK(size);
		*p &= ~mask_to_clear;
	}
}

enum {
        REG_OP_ISFREE,          /* true if region is all zero bits */
        REG_OP_ALLOC,           /* set all bits in region */
        REG_OP_RELEASE,         /* clear all bits in region */
};

static int __reg_op(unsigned long *bitmap, int pos, int order, int reg_op)
{
        int nbits_reg;          /* number of bits in region */
        int index;              /* index first long of region in bitmap */
        int offset;             /* bit offset region in bitmap[index] */
        int nlongs_reg;         /* num longs spanned by region in bitmap */
        int nbitsinlong;        /* num bits of region in each spanned long */
        unsigned long mask;     /* bitmask for one long of region */
        int i;                  /* scans bitmap by longs */
        int ret = 0;            /* return value */

        /*
         * Either nlongs_reg == 1 (for small orders that fit in one long)
         * or (offset == 0 && mask == ~0UL) (for larger multiword orders.)
         */
        nbits_reg = 1 << order;
        index = pos / BITS_PER_LONG;
        offset = pos - (index * BITS_PER_LONG);
        nlongs_reg = BITS_TO_LONGS(nbits_reg);
        nbitsinlong = min(nbits_reg,  BITS_PER_LONG);

        /*
         * Can't do "mask = (1UL << nbitsinlong) - 1", as that
         * overflows if nbitsinlong == BITS_PER_LONG.
         */
        mask = (1UL << (nbitsinlong - 1));
        mask += mask - 1;
        mask <<= offset;

        switch (reg_op) {
        case REG_OP_ISFREE:
                for (i = 0; i < nlongs_reg; i++) {
                        if (bitmap[index + i] & mask)
                                goto done;
                }
                ret = 1;        /* all bits in region free (zero) */
                break;

        case REG_OP_ALLOC:
                for (i = 0; i < nlongs_reg; i++)
                        bitmap[index + i] |= mask;
                break;

        case REG_OP_RELEASE:
                for (i = 0; i < nlongs_reg; i++)
                        bitmap[index + i] &= ~mask;
                break;
        }
done:
        return ret;
}

/**
 * bitmap_find_free_region - find a contiguous aligned mem region
 *      @bitmap: array of unsigned longs corresponding to the bitmap
 *      @bits: number of bits in the bitmap
 *      @order: region size (log base 2 of number of bits) to find
 *
 * Find a region of free (zero) bits in a @bitmap of @bits bits and
 * allocate them (set them to one).  Only consider regions of length
 * a power (@order) of two, aligned to that power of two, which
 * makes the search algorithm much faster.
 *
 * Return the bit offset in bitmap of the allocated region,
 * or -errno on failure.
 */
static inline int 
bitmap_find_free_region(unsigned long *bitmap, int bits, int order)
{
        int pos, end;           /* scans bitmap by regions of size order */

        for (pos = 0 ; (end = pos + (1 << order)) <= bits; pos = end) {
                if (!__reg_op(bitmap, pos, order, REG_OP_ISFREE))
                        continue;
                __reg_op(bitmap, pos, order, REG_OP_ALLOC);
                return pos;
        }
        return -ENOMEM;
}

/**
 * bitmap_allocate_region - allocate bitmap region
 *      @bitmap: array of unsigned longs corresponding to the bitmap
 *      @pos: beginning of bit region to allocate
 *      @order: region size (log base 2 of number of bits) to allocate
 *
 * Allocate (set bits in) a specified region of a bitmap.
 *
 * Return 0 on success, or %-EBUSY if specified region wasn't
 * free (not all bits were zero).
 */

static inline int
bitmap_allocate_region(unsigned long *bitmap, int pos, int order)
{
        if (!__reg_op(bitmap, pos, order, REG_OP_ISFREE))
                return -EBUSY;
        __reg_op(bitmap, pos, order, REG_OP_ALLOC);
        return 0;
}

/**
 * bitmap_release_region - release allocated bitmap region
 *      @bitmap: array of unsigned longs corresponding to the bitmap
 *      @pos: beginning of bit region to release
 *      @order: region size (log base 2 of number of bits) to release
 *
 * This is the complement to __bitmap_find_free_region() and releases
 * the found region (by clearing it in the bitmap).
 *
 * No return value.
 */
static inline void 
bitmap_release_region(unsigned long *bitmap, int pos, int order)
{
        __reg_op(bitmap, pos, order, REG_OP_RELEASE);
}


#define for_each_set_bit(bit, addr, size) \
	for ((bit) = find_first_bit((addr), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((addr), (size), (bit) + 1))

#endif	/* _LINUX_BITOPS_H_ */
