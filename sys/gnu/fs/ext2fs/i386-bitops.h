/* $FreeBSD: src/sys/gnu/fs/ext2fs/i386-bitops.h,v 1.7 2005/06/16 06:51:38 imp Exp $ */
/*
 * this is mixture of i386/bitops.h and asm/string.h
 * taken from the Linux source tree 
 *
 * XXX replace with Mach routines or reprogram in C
 */
/*-
 * Copyright 1992, Linus Torvalds.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef _SYS_GNU_EXT2FS_I386_BITOPS_H_
#define	_SYS_GNU_EXT2FS_I386_BITOPS_H_

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy *) addr)

static __inline__ int set_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

static __inline__ int clear_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

static __inline__ int change_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btcl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

/*
 * This routine doesn't need to be atomic, but it's faster to code it
 * this way.
 */
static __inline__ int test_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (ADDR),"ir" (nr));
	return oldbit;
}

/*
 * Find-bit routines..
 */
static __inline__ int find_first_zero_bit(void * addr, unsigned size)
{
	int res;
	int _count = (size + 31) >> 5;

	if (!size)
		return 0;
	__asm__("			\n\
		cld			\n\
		movl $-1,%%eax		\n\
		xorl %%edx,%%edx	\n\
		repe; scasl		\n\
		je 1f			\n\
		xorl -4(%%edi),%%eax	\n\
		subl $4,%%edi		\n\
		bsfl %%eax,%%edx	\n\
1:		subl %%ebx,%%edi	\n\
		shll $3,%%edi		\n\
		addl %%edi,%%edx"
		: "=c" (_count), "=D" (addr), "=d" (res)
		: "0" (_count), "1" (addr), "b" (addr)
		: "ax");
	return res;
}

static __inline__ int find_next_zero_bit (void * addr, int size, int offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 5);
	int set = 0, bit = offset & 31, res;
	
	if (bit) {
		/*
		 * Look for zero in first byte
		 */
		__asm__("			\n\
			bsfl %1,%0		\n\
			jne 1f			\n\
			movl $32, %0		\n\
1:			"
			: "=r" (set)
			: "r" (~(*p >> bit)));
		if (set < (32 - bit))
			return set + offset;
		set = 32 - bit;
		p++;
	}
	/*
	 * No zero yet, search remaining full bytes for a zero
	 */
	res = find_first_zero_bit (p, size - 32 * (p - (unsigned long *) addr));
	return (offset + set + res);
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
static __inline__ unsigned long ffz(unsigned long word)
{
	__asm__("bsfl %1,%0"
		:"=r" (word)
		:"r" (~word));
	return word;
}

/* 
 * memscan() taken from linux asm/string.h
 */
/*
 * find the first occurrence of byte 'c', or 1 past the area if none
 */
static __inline__ char * memscan(void * addr, unsigned char c, int size)
{
        if (!size)
                return addr;
        __asm__("			\n\
		cld			\n\
                repnz; scasb		\n\
                jnz 1f			\n\
                dec %%edi		\n\
1:              "
                : "=D" (addr), "=c" (size)
                : "0" (addr), "1" (size), "a" (c));
        return addr;
}

#endif /* !_SYS_GNU_EXT2FS_I386_BITOPS_H_ */
