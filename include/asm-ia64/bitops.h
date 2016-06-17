#ifndef _ASM_IA64_BITOPS_H
#define _ASM_IA64_BITOPS_H

/*
 * Copyright (C) 1998-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/types.h>

#include <asm/intrinsics.h>

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 *
 * The address must be (at least) "long" aligned.
 * Note that there are driver (e.g., eepro100) which use these operations to operate on
 * hw-defined data-structures, so we can't easily change these operations to force a
 * bigger alignment.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */
static __inline__ void
set_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = 1 << (nr & 31);
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old | bit;
	} while (cmpxchg_acq(m, old, new) != old);
}

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void
__set_bit (int nr, volatile void *addr)
{
	*((__u32 *) addr + (nr >> 5)) |= (1 << (nr & 31));
}

/*
 * clear_bit() has "acquire" semantics.
 */
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	do { /* skip */; } while (0)

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static __inline__ void
clear_bit (int nr, volatile void *addr)
{
	__u32 mask, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	mask = ~(1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old & mask;
	} while (cmpxchg_acq(m, old, new) != old);
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void
change_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = (1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old ^ bit;
	} while (cmpxchg_acq(m, old, new) != old);
}

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void
__change_bit (int nr, volatile void *addr)
{
	*((__u32 *) addr + (nr >> 5)) ^= (1 << (nr & 31));
}

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */
static __inline__ int
test_and_set_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = 1 << (nr & 31);
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old | bit;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & bit) != 0;
}

/**
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int
__test_and_set_bit (int nr, volatile void *addr)
{
	__u32 *p = (__u32 *) addr + (nr >> 5);
	__u32 m = 1 << (nr & 31);
	int oldbitset = (*p & m) != 0;

	*p |= m;
	return oldbitset;
}

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */
static __inline__ int
test_and_clear_bit (int nr, volatile void *addr)
{
	__u32 mask, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	mask = ~(1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old & mask;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & ~mask) != 0;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int
__test_and_clear_bit(int nr, volatile void * addr)
{
	__u32 *p = (__u32 *) addr + (nr >> 5);
	__u32 m = 1 << (nr & 31);
	int oldbitset = *p & m;

	*p &= ~m;
	return oldbitset;
}

/**
 * test_and_change_bit - Change a bit and return its new value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */
static __inline__ int
test_and_change_bit (int nr, volatile void *addr)
{
	__u32 bit, old, new;
	volatile __u32 *m;
	CMPXCHG_BUGCHECK_DECL

	m = (volatile __u32 *) addr + (nr >> 5);
	bit = (1 << (nr & 31));
	do {
		CMPXCHG_BUGCHECK(m);
		old = *m;
		new = old ^ bit;
	} while (cmpxchg_acq(m, old, new) != old);
	return (old & bit) != 0;
}

/*
 * WARNING: non atomic version.
 */
static __inline__ int
__test_and_change_bit (int nr, void *addr)
{
	__u32 old, bit = (1 << (nr & 31));
	__u32 *m = (__u32 *) addr + (nr >> 5);

	old = *m;
	*m = old ^ bit;
	return (old & bit) != 0;
}

static __inline__ int
test_bit (int nr, const volatile void *addr)
{
	return 1 & (((const volatile __u32 *) addr)[nr >> 5] >> (nr & 31));
}

/**
 * ffz - find the first zero bit in a memory region
 * @x: The address to start the search at
 *
 * Returns the bit-number (0..63) of the first (least significant) zero bit, not
 * the number of the byte containing a bit.  Undefined if no zero exists, so
 * code should check against ~0UL first...
 */
static inline unsigned long
ffz (unsigned long x)
{
	unsigned long result;

	__asm__ ("popcnt %0=%1" : "=r" (result) : "r" (x & (~x - 1)));
	return result;
}

/**
 * __ffs - find first bit in word.
 * @x: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static __inline__ unsigned long
__ffs (unsigned long x)
{
	unsigned long result;

	__asm__ ("popcnt %0=%1" : "=r" (result) : "r" ((x - 1) & ~x));
	return result;
}

#ifdef __KERNEL__

/*
 * find_last_zero_bit - find the last zero bit in a 64 bit quantity
 * @x: The value to search
 */
static inline unsigned long
ia64_fls (unsigned long x)
{
	long double d = x;
	long exp;

	__asm__ ("getf.exp %0=%1" : "=r"(exp) : "f"(d));
	return exp - 0xffff;
}

/*
 * ffs: find first bit set. This is defined the same way as the libc and compiler builtin
 * ffs routines, therefore differs in spirit from the above ffz (man ffs): it operates on
 * "int" values only and the result value is the bit number + 1.  ffs(0) is defined to
 * return zero.
 */
#define ffs(x)	__builtin_ffs(x)

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */
static __inline__ unsigned long
hweight64 (unsigned long x)
{
	unsigned long result;
	__asm__ ("popcnt %0=%1" : "=r" (result) : "r" (x));
	return result;
}

#define hweight32(x) hweight64 ((x) & 0xfffffffful)
#define hweight16(x) hweight64 ((x) & 0xfffful)
#define hweight8(x)  hweight64 ((x) & 0xfful)

#endif /* __KERNEL__ */

/*
 * Find next zero bit in a bitmap reasonably efficiently..
 */
static inline unsigned long
find_next_zero_bit (void *addr, unsigned long size, unsigned long offset)
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
	if (tmp == ~0UL)		/* any bits zero? */
		return result + size;	/* nope */
found_middle:
	return result + ffz(tmp);
}

/*
 * The optimizer actually does good code for this case..
 */
#define find_first_zero_bit(addr, size) find_next_zero_bit((addr), (size), 0)

#ifdef __KERNEL__

#define ext2_set_bit                 test_and_set_bit
#define ext2_clear_bit               test_and_clear_bit
#define ext2_test_bit                test_bit
#define ext2_find_first_zero_bit     find_first_zero_bit
#define ext2_find_next_zero_bit      find_next_zero_bit

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr)		test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr)			set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr)	test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr)			test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size)	find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* _ASM_IA64_BITOPS_H */
