/* $Id: bitops.h,v 1.67 2001/11/19 18:36:34 davem Exp $
 * bitops.h: Bit string operations on the Sparc.
 *
 * Copyright 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright 1996 Eddie C. Dost   (ecd@skynet.be)
 * Copyright 2001 Anton Blanchard (anton@samba.org)
 */

#ifndef _SPARC_BITOPS_H
#define _SPARC_BITOPS_H

#include <linux/kernel.h>
#include <asm/byteorder.h>
#include <asm/system.h>

#ifdef __KERNEL__

/*
 * Set bit 'nr' in 32-bit quantity at address 'addr' where bit '0'
 * is in the highest of the four bytes and bit '31' is the high bit
 * within the first byte. Sparc is BIG-Endian. Unless noted otherwise
 * all bit-ops return 0 if bit was previously clear and != 0 otherwise.
 */
static inline int test_and_set_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___set_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");

	return mask != 0;
}

static inline void set_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___set_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");
}

static inline int test_and_clear_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___clear_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");

	return mask != 0;
}

static inline void clear_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___clear_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");
}

static inline int test_and_change_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___change_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");

	return mask != 0;
}

static inline void change_bit(unsigned long nr, volatile void *addr)
{
	register unsigned long mask asm("g2");
	register unsigned long *ADDR asm("g1");
	register int tmp1 asm("g3");
	register int tmp2 asm("g4");
	register int tmp3 asm("g5");
	register int tmp4 asm("g7");

	ADDR = ((unsigned long *) addr) + (nr >> 5);
	mask = 1 << (nr & 31);

	__asm__ __volatile__(
	"mov	%%o7, %%g4\n\t"
	"call	___change_bit\n\t"
	" add	%%o7, 8, %%o7\n"
	: "=&r" (mask), "=r" (tmp1), "=r" (tmp2), "=r" (tmp3), "=r" (tmp4)
	: "0" (mask), "r" (ADDR)
	: "memory", "cc");
}

/*
 * non-atomic versions
 */
static inline void __set_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p |= mask;
}

static inline void __clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p &= ~mask;
}

static inline void __change_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p ^= mask;
}

static inline int __test_and_set_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

static inline int __test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

static inline int __test_and_change_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1UL << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

#define smp_mb__before_clear_bit()	do { } while(0)
#define smp_mb__after_clear_bit()	do { } while(0)

/* The following routine need not be atomic. */
static inline int test_bit(int nr, __const__ void *addr)
{
	return (1 & (((__const__ unsigned int *) addr)[nr >> 5] >> (nr & 31))) != 0;
}

/* The easy/cheese version for now. */
static inline unsigned long ffz(unsigned long word)
{
	unsigned long result = 0;

	while(word & 1) {
		result++;
		word >>= 1;
	}
	return result;
}

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
#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)

/*
 * find_next_zero_bit() finds the first zero bit in a bit string of length
 * 'size' bits, starting the search at bit 'offset'. This is largely based
 * on Linus's ALPHA routines, which are pretty portable BTW.
 */
static inline unsigned long find_next_zero_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (32-offset);
		if (size < 32)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size & ~31UL) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
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

/*
 * Linus sez that gcc can optimize the following correctly, we'll see if this
 * holds on the Sparc as it does for the ALPHA.
 */
#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)

static inline int test_le_bit(int nr, __const__ void * addr)
{
	__const__ unsigned char *ADDR = (__const__ unsigned char *) addr;
	return (ADDR[nr >> 3] >> (nr & 7)) & 1;
}

/*
 * non-atomic versions
 */
static inline void __set_le_bit(int nr, void *addr)
{
	unsigned char *ADDR = (unsigned char *)addr;

	ADDR += nr >> 3;
	*ADDR |= 1 << (nr & 0x07);
}

static inline void __clear_le_bit(int nr, void *addr)
{
	unsigned char *ADDR = (unsigned char *)addr;

	ADDR += nr >> 3;
	*ADDR &= ~(1 << (nr & 0x07));
}

static inline int __test_and_set_le_bit(int nr, void *addr)
{
	int mask, retval;
	unsigned char *ADDR = (unsigned char *)addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;
	return retval;
}

static inline int __test_and_clear_le_bit(int nr, void *addr)
{
	int mask, retval;
	unsigned char *ADDR = (unsigned char *)addr;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;
	return retval;
}

static inline unsigned long find_next_zero_le_bit(void *addr, unsigned long size, unsigned long offset)
{
	unsigned long *p = ((unsigned long *) addr) + (offset >> 5);
	unsigned long result = offset & ~31UL;
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if(offset) {
		tmp = *(p++);
		tmp |= __swab32(~0UL >> (32-offset));
		if(size < 32)
			goto found_first;
		if(~tmp)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while(size & ~31UL) {
		if(~(tmp = *(p++)))
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if(!size)
		return result;
	tmp = *p;

found_first:
	tmp = __swab32(tmp) | (~0UL << size);
	if (tmp == ~0UL)        /* Are any bits zero? */
		return result + size; /* Nope. */
	return result + ffz(tmp);

found_middle:
	return result + ffz(__swab32(tmp));
}

#define find_first_zero_le_bit(addr, size) \
        find_next_zero_le_bit((addr), (size), 0)

#define ext2_set_bit			__test_and_set_le_bit
#define ext2_clear_bit			__test_and_clear_le_bit
#define ext2_test_bit			test_le_bit
#define ext2_find_first_zero_bit	find_first_zero_le_bit
#define ext2_find_next_zero_bit		find_next_zero_le_bit

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr)		test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr)			set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr)	test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr)			test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size)	find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* defined(_SPARC_BITOPS_H) */
