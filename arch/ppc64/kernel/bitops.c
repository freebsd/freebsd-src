/*
 * Copyright (C) 1996 Paul Mackerras.
 * Adapted for ppc64 - Todd Inglett, Anton Blanchard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <asm/bitops.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/locks.h>
#include <linux/quotaops.h>
#include <asm/ppcdebug.h>

#undef DEBUG_BITOPS

/*
 * Bitops are weird when viewed on big-endian systems.  They were designed
 * on little endian so the size of the bitset doesn't matter (low order bytes
 * come first) as long as the bit in question is valid.
 *
 * Bits are "tested" often using the C expression (val & (1<<nr)) so we do
 * our best to stay compatible with that.  The assumption is that val will
 * be unsigned long for such tests.  As such, we assume the bits are stored
 * as an array of unsigned long (the usual case is a single unsigned long,
 * of course).  Here's an example bitset with bit numbering:
 *
 *   |63..........0|127........64|195.......128|255.......196|
 *
 * This leads to two problems.  First, if an int, short or char is passed as
 * a bitset it will be a bad memory reference since we want to store in chunks
 * of unsigned long (64 bits here) size.  Second, since these could be char
 * arrays we might have an alignment problem.  We ease the alignment problem
 * by actually doing the operation with 32 bit values yet preserving the
 * 64 bit long layout as shown above.  Got that?  Good.
 */

unsigned long find_next_zero_bit(unsigned long* addr, unsigned long size,
				 unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;

	if (offset) {
		tmp = *p++;
		tmp |= ~0UL >> (64-offset);
		if (size < 64)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 64;
		result += 64;
	}
	while (size & ~63UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 64;
		size -= 64;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp |= ~0UL << size;
	if (tmp == ~0UL)
		return result+size;
found_middle:
	return result + ffz(tmp);
}

unsigned long find_next_bit(unsigned long *addr, unsigned long size, unsigned long offset)
{
        unsigned long *p = addr + (offset >> 6);
        unsigned long result = offset & ~63UL;
        unsigned long tmp;

        if (offset >= size)
                return size;
        size -= result;
        offset &= 63UL;
        if (offset) {
                tmp = *(p++);
                tmp &= (~0UL << offset);
                if (size < 64)
                        goto found_first;
                if (tmp)
                        goto found_middle;
                size -= 64;
                result += 64;
        }
        while (size & ~63UL) {
                if ((tmp = *(p++)))
                        goto found_middle;
                result += 64;
                size -= 64;
        }
        if (!size)
                return result;
        tmp = *p;

found_first:
        tmp &= (~0UL >> (64 - size));
        if (tmp == 0UL)        /* Are any bits set? */
                return result + size; /* Nope. */
found_middle:
        return result + __ffs(tmp);
}

static __inline__ unsigned int ext2_ilog2(unsigned int x)
{
        int lz;

        asm("cntlzw %0,%1" : "=r" (lz) : "r" (x));
        return 31 - lz;
}

static __inline__ unsigned int ext2_ffz(unsigned int x)
{
        u32  tempRC;
        if ((x = ~x) == 0)
                return 32;
        tempRC = ext2_ilog2(x & -x);
        return tempRC;
}

unsigned long find_next_zero_le_bit(unsigned long *addr, unsigned long size, unsigned long
 offset)
{
        unsigned int *p = ((unsigned int *) addr) + (offset >> 5);
        unsigned int result = offset & ~31;
        unsigned int tmp;

        if (offset >= size)
                return size;
        size -= result;
        offset &= 31;
        if (offset) {
                tmp = cpu_to_le32p(p++);
                tmp |= ~0U >> (32-offset); /* bug or feature ? */
                if (size < 32)
                        goto found_first;
                if (tmp != ~0)
                        goto found_middle;
                size -= 32;
                result += 32;
        }
        while (size >= 32) {
                if ((tmp = cpu_to_le32p(p++)) != ~0)
                        goto found_middle;
                result += 32;
                size -= 32;
        }
        if (!size)
                return result;
        tmp = cpu_to_le32p(p);
found_first:
        tmp |= ~0 << size;
        if (tmp == ~0)          /* Are any bits zero? */
                return result + size; /* Nope. */
found_middle:
        return result + ext2_ffz(tmp);
}

void BUG_OUTLINE(char* file, unsigned line) 
{
	udbg_printf("BUG - kernel BUG at %s:%d! \n", __FILE__, __LINE__);
	PPCDBG_ENTER_DEBUGGER();
	printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__);
	__asm__ __volatile__(".long " BUG_ILLEGAL_INSTR);
}

