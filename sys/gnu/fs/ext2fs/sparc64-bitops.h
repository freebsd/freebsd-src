/* $Id: bitops.h,v 1.31 2000/09/23 02:09:21 davem Exp $
 * bitops.h: Bit string operations on the V9.
 *
 * Copyright 1996, 1997 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC64_BITOPS_H
#define _SPARC64_BITOPS_H

#include <asm/byteorder.h>

extern long __test_and_set_bit(unsigned long nr, volatile void *addr);
extern long __test_and_clear_bit(unsigned long nr, volatile void *addr);
extern long __test_and_change_bit(unsigned long nr, volatile void *addr);

#define test_and_set_bit(nr,addr)	(__test_and_set_bit(nr,addr)!=0)
#define test_and_clear_bit(nr,addr)	(__test_and_clear_bit(nr,addr)!=0)
#define test_and_change_bit(nr,addr)	(__test_and_change_bit(nr,addr)!=0)
#define set_bit(nr,addr)		((void)__test_and_set_bit(nr,addr))
#define clear_bit(nr,addr)		((void)__test_and_clear_bit(nr,addr))
#define change_bit(nr,addr)		((void)__test_and_change_bit(nr,addr))

#define smp_mb__before_clear_bit()	do { } while(0)
#define smp_mb__after_clear_bit()	do { } while(0)

extern __inline__ int test_bit(int nr, __const__ void *addr)
{
	return (1UL & (((__const__ long *) addr)[nr >> 6] >> (nr & 63))) != 0UL;
}

/* The easy/cheese version for now. */
extern __inline__ unsigned long ffz(unsigned long word)
{
	unsigned long result;

#ifdef ULTRA_HAS_POPULATION_COUNT	/* Thanks for nothing Sun... */
	__asm__ __volatile__("
	brz,pn	%0, 1f
	 neg	%0, %%g1
	xnor	%0, %%g1, %%g2
	popc	%%g2, %0
1:	" : "=&r" (result)
	  : "0" (word)
	  : "g1", "g2");
#else
#if 1 /* def EASY_CHEESE_VERSION */
	result = 0;
	while(word & 1) {
		result++;
		word >>= 1;
	}
#else
	unsigned long tmp;

	result = 0;	
	tmp = ~word & -~word;
	if (!(unsigned)tmp) {
		tmp >>= 32;
		result = 32;
	}
	if (!(unsigned short)tmp) {
		tmp >>= 16;
		result += 16;
	}
	if (!(unsigned char)tmp) {
		tmp >>= 8;
		result += 8;
	}
	if (tmp & 0xf0) result += 4;
	if (tmp & 0xcc) result += 2;
	if (tmp & 0xaa) result ++;
#endif
#endif
	return result;
}

#ifdef __KERNEL__

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

#define ffs(x) generic_ffs(x)

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

#ifdef ULTRA_HAS_POPULATION_COUNT

extern __inline__ unsigned int hweight32(unsigned int w)
{
	unsigned int res;

	__asm__ ("popc %1,%0" : "=r" (res) : "r" (w & 0xffffffff));
	return res;
}

extern __inline__ unsigned int hweight16(unsigned int w)
{
	unsigned int res;

	__asm__ ("popc %1,%0" : "=r" (res) : "r" (w & 0xffff));
	return res;
}

extern __inline__ unsigned int hweight8(unsigned int w)
{
	unsigned int res;

	__asm__ ("popc %1,%0" : "=r" (res) : "r" (w & 0xff));
	return res;
}

#else

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)

#endif
#endif /* __KERNEL__ */

/* find_next_zero_bit() finds the first zero bit in a bit string of length
 * 'size' bits, starting the search at bit 'offset'. This is largely based
 * on Linus's ALPHA routines, which are pretty portable BTW.
 */

extern __inline__ unsigned long find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;
	if (offset) {
		tmp = *(p++);
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
	if (tmp == ~0UL)        /* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)

extern long __test_and_set_le_bit(int nr, volatile void *addr);
extern long __test_and_clear_le_bit(int nr, volatile void *addr);

#define test_and_set_le_bit(nr,addr)	(__test_and_set_le_bit(nr,addr)!=0)
#define test_and_clear_le_bit(nr,addr)	(__test_and_clear_le_bit(nr,addr)!=0)
#define set_le_bit(nr,addr)		((void)__test_and_set_le_bit(nr,addr))
#define clear_le_bit(nr,addr)		((void)__test_and_clear_le_bit(nr,addr))

extern __inline__ int test_le_bit(int nr, __const__ void * addr)
{
	int			mask;
	__const__ unsigned char	*ADDR = (__const__ unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	return ((mask & *ADDR) != 0);
}

#define find_first_zero_le_bit(addr, size) \
        find_next_zero_le_bit((addr), (size), 0)

extern __inline__ unsigned long find_next_zero_le_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 6);
	unsigned long result = offset & ~63UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 63UL;
	if(offset) {
		tmp = __swab64p(p++);
		tmp |= (~0UL >> (64-offset));
		if(size < 64)
			goto found_first;
		if(~tmp)
			goto found_middle;
		size -= 64;
		result += 64;
	}
	while(size & ~63) {
		if(~(tmp = __swab64p(p++)))
			goto found_middle;
		result += 64;
		size -= 64;
	}
	if(!size)
		return result;
	tmp = __swab64p(p);
found_first:
	tmp |= (~0UL << size);
	if (tmp == ~0UL)        /* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

#ifdef __KERNEL__

#define ext2_set_bit			test_and_set_le_bit
#define ext2_clear_bit			test_and_clear_le_bit
#define ext2_test_bit  			test_le_bit
#define ext2_find_first_zero_bit	find_first_zero_le_bit
#define ext2_find_next_zero_bit		find_next_zero_le_bit

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr) set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* defined(_SPARC64_BITOPS_H) */
