/*
 * bitops.h: Bit string operations on the ppc
 */

#ifdef __KERNEL__
#ifndef _PPC_BITOPS_H
#define _PPC_BITOPS_H

#include <linux/config.h>
#include <asm/byteorder.h>
#include <asm/atomic.h>

/*
 * The test_and_*_bit operations are taken to imply a memory barrier
 * on SMP systems.
 */
#ifdef CONFIG_SMP
#define SMP_WMB		"eieio\n"
#define SMP_MB		"\nsync"
#else
#define SMP_WMB
#define SMP_MB
#endif /* CONFIG_SMP */

/*
 * These used to be if'd out here because using : "cc" as a constraint
 * resulted in errors from egcs.  Things appear to be OK with gcc-2.95.
 */
static __inline__ void set_bit(int nr, volatile void * addr)
{
	unsigned long old;
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	__asm__ __volatile__("\n\
1:	lwarx	%0,0,%3 \n\
	or	%0,%0,%2 \n"
	PPC405_ERR77(0,%3)
"	stwcx.	%0,0,%3 \n\
	bne-	1b"
	: "=&r" (old), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc" );
}

/*
 * non-atomic version
 */
static __inline__ void __set_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p |= mask;
}

/*
 * clear_bit doesn't imply a memory barrier
 */
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()

static __inline__ void clear_bit(int nr, volatile void *addr)
{
	unsigned long old;
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	__asm__ __volatile__("\n\
1:	lwarx	%0,0,%3 \n\
	andc	%0,%0,%2 \n"
	PPC405_ERR77(0,%3)
"	stwcx.	%0,0,%3 \n\
	bne-	1b"
	: "=&r" (old), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc");
}

/*
 * non-atomic version
 */
static __inline__ void __clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p &= ~mask;
}

static __inline__ void change_bit(int nr, volatile void *addr)
{
	unsigned long old;
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	__asm__ __volatile__("\n\
1:	lwarx	%0,0,%3 \n\
	xor	%0,%0,%2 \n"
	PPC405_ERR77(0,%3)
"	stwcx.	%0,0,%3 \n\
	bne-	1b"
	: "=&r" (old), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc");
}

/*
 * non-atomic version
 */
static __inline__ void __change_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);

	*p ^= mask;
}

/*
 * test_and_*_bit do imply a memory barrier (?)
 */
static __inline__ int test_and_set_bit(int nr, volatile void *addr)
{
	unsigned int old, t;
	unsigned int mask = 1 << (nr & 0x1f);
	volatile unsigned int *p = ((volatile unsigned int *)addr) + (nr >> 5);

	__asm__ __volatile__(SMP_WMB "\n\
1:	lwarx	%0,0,%4 \n\
	or	%1,%0,%3 \n"
	PPC405_ERR77(0,%4)
"	stwcx.	%1,0,%4 \n\
	bne	1b"
	SMP_MB
	: "=&r" (old), "=&r" (t), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc", "memory");

	return (old & mask) != 0;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_set_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

static __inline__ int test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned int old, t;
	unsigned int mask = 1 << (nr & 0x1f);
	volatile unsigned int *p = ((volatile unsigned int *)addr) + (nr >> 5);

	__asm__ __volatile__(SMP_WMB "\n\
1:	lwarx	%0,0,%4 \n\
	andc	%1,%0,%3 \n"
	PPC405_ERR77(0,%4)
"	stwcx.	%1,0,%4 \n\
	bne	1b"
	SMP_MB
	: "=&r" (old), "=&r" (t), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc", "memory");

	return (old & mask) != 0;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

static __inline__ int test_and_change_bit(int nr, volatile void *addr)
{
	unsigned int old, t;
	unsigned int mask = 1 << (nr & 0x1f);
	volatile unsigned int *p = ((volatile unsigned int *)addr) + (nr >> 5);

	__asm__ __volatile__(SMP_WMB "\n\
1:	lwarx	%0,0,%4 \n\
	xor	%1,%0,%3 \n"
	PPC405_ERR77(0,%4)
"	stwcx.	%1,0,%4 \n\
	bne	1b"
	SMP_MB
	: "=&r" (old), "=&r" (t), "=m" (*p)
	: "r" (mask), "r" (p), "m" (*p)
	: "cc", "memory");

	return (old & mask) != 0;
}

/*
 * non-atomic version
 */
static __inline__ int __test_and_change_bit(int nr, volatile void *addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	unsigned long *p = ((unsigned long *)addr) + (nr >> 5);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

static __inline__ int test_bit(int nr, __const__ volatile void *addr)
{
	__const__ unsigned int *p = (__const__ unsigned int *) addr;

	return ((p[nr >> 5] >> (nr & 0x1f)) & 1) != 0;
}

/* Return the bit position of the most significant 1 bit in a word */
static __inline__ int __ilog2(unsigned int x)
{
	int lz;

	asm ("cntlzw %0,%1" : "=r" (lz) : "r" (x));
	return 31 - lz;
}

static __inline__ int ffz(unsigned int x)
{
	if ((x = ~x) == 0)
		return 32;
	return __ilog2(x & -x);
}

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */
static __inline__ int ffs(int x)
{
	return __ilog2(x & -x) + 1;
}

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
#define find_first_zero_bit(addr, size) \
	find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long find_next_zero_bit(void * addr,
	unsigned long size, unsigned long offset)
{
	unsigned int * p = ((unsigned int *) addr) + (offset >> 5);
	unsigned int result = offset & ~31UL;
	unsigned int tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = *p++;
		tmp |= ~0UL >> (32-offset);
		if (size < 32)
			goto found_first;
		if (tmp != ~0U)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size >= 32) {
		if ((tmp = *p++) != ~0U)
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


#define ext2_set_bit(nr, addr)		__test_and_set_bit((nr) ^ 0x18, addr)
#define ext2_clear_bit(nr, addr)	__test_and_clear_bit((nr) ^ 0x18, addr)

static __inline__ int ext2_test_bit(int nr, __const__ void * addr)
{
	__const__ unsigned char	*ADDR = (__const__ unsigned char *) addr;

	return (ADDR[nr >> 3] >> (nr & 7)) & 1;
}

/*
 * This implementation of ext2_find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h and modified for a big-endian machine.
 */

#define ext2_find_first_zero_bit(addr, size) \
        ext2_find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long ext2_find_next_zero_bit(void *addr,
	unsigned long size, unsigned long offset)
{
	unsigned int *p = ((unsigned int *) addr) + (offset >> 5);
	unsigned int result = offset & ~31UL;
	unsigned int tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= 31UL;
	if (offset) {
		tmp = cpu_to_le32p(p++);
		tmp |= ~0UL >> (32-offset);
		if (size < 32)
			goto found_first;
		if (tmp != ~0U)
			goto found_middle;
		size -= 32;
		result += 32;
	}
	while (size >= 32) {
		if ((tmp = cpu_to_le32p(p++)) != ~0U)
			goto found_middle;
		result += 32;
		size -= 32;
	}
	if (!size)
		return result;
	tmp = cpu_to_le32p(p);
found_first:
	tmp |= ~0U << size;
	if (tmp == ~0UL)        /* Are any bits zero? */
		return result + size; /* Nope. */
found_middle:
	return result + ffz(tmp);
}

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr) ext2_set_bit(nr,addr)
#define minix_set_bit(nr,addr) ((void)ext2_set_bit(nr,addr))
#define minix_test_and_clear_bit(nr,addr) ext2_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) ext2_test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) ext2_find_first_zero_bit(addr,size)

#endif /* _PPC_BITOPS_H */
#endif /* __KERNEL__ */
