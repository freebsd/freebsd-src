/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1994 - 1997, 1999, 2000  Ralf Baechle (ralf@gnu.org)
 * Copyright (c) 2000  Silicon Graphics, Inc.
 */
#ifndef _ASM_BITOPS_H
#define _ASM_BITOPS_H

#include <linux/config.h>
#include <linux/types.h>
#include <asm/byteorder.h>		/* sigh ... */

#if (_MIPS_SZLONG == 32)
#define SZLONG_LOG 5
#define SZLONG_MASK 31UL
#elif (_MIPS_SZLONG == 64)
#define SZLONG_LOG 6
#define SZLONG_MASK 63UL 
#endif

#ifdef __KERNEL__

#include <asm/sgidefs.h>
#include <asm/system.h>

/*
 * clear_bit() doesn't provide any barrier for the compiler.
 */
#define smp_mb__before_clear_bit()	smp_mb()
#define smp_mb__after_clear_bit()	smp_mb()

/*
 * Only disable interrupt for kernel mode stuff to keep usermode stuff
 * that dares to use kernel include files alive.
 */
#define __bi_flags			unsigned long flags
#define __bi_cli()			local_irq_disable()
#define __bi_save_flags(x)		local_save_flags(x)
#define __bi_local_irq_save(x)		local_irq_save(x)
#define __bi_local_irq_restore(x)	local_irq_restore(x)
#else
#define __bi_flags
#define __bi_cli()
#define __bi_save_flags(x)
#define __bi_local_irq_save(x)
#define __bi_local_irq_restore(x)
#endif /* __KERNEL__ */

#ifdef CONFIG_CPU_HAS_LLSC

/*
 * These functions for MIPS ISA > 1 are interrupt and SMP proof and
 * interrupt friendly
 */

/*
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void set_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp;

	__asm__ __volatile__(
		"1:\tll\t%0, %1\t\t# set_bit\n\t"
		"or\t%0, %2\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b"
		: "=&r" (temp), "=m" (*m)
		: "ir" (1UL << (nr & 0x1f)), "m" (*m));
}

/*
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __set_bit(int nr, volatile void * addr)
{
	unsigned long * m = ((unsigned long *) addr) + (nr >> 5);

	*m |= 1UL << (nr & 31);
}

/*
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static __inline__ void clear_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp;

	__asm__ __volatile__(
		"1:\tll\t%0, %1\t\t# clear_bit\n\t"
		"and\t%0, %2\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b\n\t"
		: "=&r" (temp), "=m" (*m)
		: "ir" (~(1UL << (nr & 0x1f))), "m" (*m));
}

/*
 * change_bit - Toggle a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void change_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp;

	__asm__ __volatile__(
		"1:\tll\t%0, %1\t\t# change_bit\n\t"
		"xor\t%0, %2\n\t"
		"sc\t%0, %1\n\t"
		"beqz\t%0, 1b"
		: "=&r" (temp), "=m" (*m)
		: "ir" (1UL << (nr & 0x1f)), "m" (*m));
}

/*
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __change_bit(int nr, volatile void * addr)
{
	unsigned long * m = ((unsigned long *) addr) + (nr >> 5);

	*m ^= 1UL << (nr & 31);
}

/*
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_set_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp;
	int res;

	__asm__ __volatile__(
		".set\tnoreorder\t\t# test_and_set_bit\n"
		"1:\tll\t%0, %1\n\t"
		"or\t%2, %0, %3\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n\t"
		" and\t%2, %0, %3\n\t"
#ifdef CONFIG_SMP
		"sync\n\t"
#endif
		".set\treorder"
		: "=&r" (temp), "=m" (*m), "=&r" (res)
		: "r" (1UL << (nr & 0x1f)), "m" (*m)
		: "memory");

	return res != 0;
}

/*
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_set_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a |= mask;

	return retval;
}

/*
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_clear_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp, res;

	__asm__ __volatile__(
		".set\tnoreorder\t\t# test_and_clear_bit\n"
		"1:\tll\t%0, %1\n\t"
		"or\t%2, %0, %3\n\t"
		"xor\t%2, %3\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n\t"
		" and\t%2, %0, %3\n\t"
#ifdef CONFIG_SMP
		"sync\n\t"
#endif
		".set\treorder"
		: "=&r" (temp), "=m" (*m), "=&r" (res)
		: "r" (1UL << (nr & 0x1f)), "m" (*m)
		: "memory");

	return res != 0;
}

/*
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_clear_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask, retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a &= ~mask;

	return retval;
}

/*
 * test_and_change_bit - Change a bit and return its new value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_change_bit(int nr, volatile void *addr)
{
	unsigned long *m = ((unsigned long *) addr) + (nr >> 5);
	unsigned long temp, res;

	__asm__ __volatile__(
		".set\tnoreorder\t\t# test_and_change_bit\n"
		"1:\tll\t%0, %1\n\t"
		"xor\t%2, %0, %3\n\t"
		"sc\t%2, %1\n\t"
		"beqz\t%2, 1b\n\t"
		" and\t%2, %0, %3\n\t"
#ifdef CONFIG_SMP
		"sync\n\t"
#endif
		".set\treorder"
		: "=&r" (temp), "=m" (*m), "=&r" (res)
		: "r" (1UL << (nr & 0x1f)), "m" (*m)
		: "memory");

	return res != 0;
}

/*
 * __test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_change_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a ^= mask;

	return retval;
}

#else /* MIPS I */

/*
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void set_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	*a |= mask;
	__bi_local_irq_restore(flags);
}

/*
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __set_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	*a |= mask;
}

/*
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_clear_bit() and/or smp_mb__after_clear_bit()
 * in order to ensure changes are visible on other processors.
 */
static __inline__ void clear_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	*a &= ~mask;
	__bi_local_irq_restore(flags);
}

/*
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static __inline__ void change_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	*a ^= mask;
	__bi_local_irq_restore(flags);
}

/*
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static __inline__ void __change_bit(int nr, volatile void * addr)
{
	unsigned long * m = ((unsigned long *) addr) + (nr >> 5);

	*m ^= 1UL << (nr & 31);
}

/*
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_set_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	retval = (mask & *a) != 0;
	*a |= mask;
	__bi_local_irq_restore(flags);

	return retval;
}

/*
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_set_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a |= mask;

	return retval;
}

/*
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_clear_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	retval = (mask & *a) != 0;
	*a &= ~mask;
	__bi_local_irq_restore(flags);

	return retval;
}

/*
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_clear_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a &= ~mask;

	return retval;
}

/*
 * test_and_change_bit - Change a bit and return its new value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static __inline__ int test_and_change_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask, retval;
	__bi_flags;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	__bi_local_irq_save(flags);
	retval = (mask & *a) != 0;
	*a ^= mask;
	__bi_local_irq_restore(flags);

	return retval;
}

/*
 * __test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static __inline__ int __test_and_change_bit(int nr, volatile void * addr)
{
	volatile unsigned long *a = addr;
	unsigned long mask;
	int retval;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a ^= mask;

	return retval;
}

#undef __bi_flags
#undef __bi_cli
#undef __bi_save_flags
#undef __bi_local_irq_restore

#endif /* MIPS I */

/*
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static inline int test_bit(int nr, volatile void *addr)
{
	return 1UL & (((const volatile unsigned long *) addr)[nr >> SZLONG_LOG] >> (nr & SZLONG_MASK));
}

/*
 * ffz - find first zero in word.
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
static __inline__ unsigned long ffz(unsigned long word)
{
	int b = 0, s;

	word = ~word;
	s = 16; if (word << 16 != 0) s = 0; b += s; word >>= s;
	s =  8; if (word << 24 != 0) s = 0; b += s; word >>= s;
	s =  4; if (word << 28 != 0) s = 0; b += s; word >>= s;
	s =  2; if (word << 30 != 0) s = 0; b += s; word >>= s;
	s =  1; if (word << 31 != 0) s = 0; b += s;

	return b;
}


#ifdef __KERNEL__

/*
 * ffs - find first bit set
 * @x: the word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */

#define ffs(x) generic_ffs(x)

/*
 * find_next_zero_bit - find the first zero bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 */
static inline long find_next_zero_bit(void *addr, unsigned long size,
	unsigned long offset)
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
	if (tmp == ~0UL)		/* Are any bits zero? */
		return result + size;	/* Nope. */
found_middle:
	return result + ffz(tmp);
}

#define find_first_zero_bit(addr, size) \
	find_next_zero_bit((addr), (size), 0)

#if 0 /* Fool kernel-doc since it doesn't do macros yet */
/*
 * find_first_zero_bit - find the first zero bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit-number of the first zero bit, not the number of the byte
 * containing a bit.
 */
static int find_first_zero_bit (void *addr, unsigned size);
#endif

#define find_first_zero_bit(addr, size) \
        find_next_zero_bit((addr), (size), 0)


/*
 * hweightN - returns the hamming weight of a N-bit word
 * @x: the word to weigh
 *
 * The Hamming Weight of a number is the total number of bits set in it.
 */

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)


static __inline__ int __test_and_set_le_bit(int nr, void * addr)
{
	unsigned char	*ADDR = (unsigned char *) addr;
	int		mask, retval;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR |= mask;

	return retval;
}

static __inline__ int __test_and_clear_le_bit(int nr, void * addr)
{
	unsigned char	*ADDR = (unsigned char *) addr;
	int		mask, retval;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);
	retval = (mask & *ADDR) != 0;
	*ADDR &= ~mask;

	return retval;
}

static __inline__ int test_le_bit(int nr, const void * addr)
{
	const unsigned char	*ADDR = (const unsigned char *) addr;
	int			mask;

	ADDR += nr >> 3;
	mask = 1 << (nr & 0x07);

	return ((mask & *ADDR) != 0);
}

static inline unsigned long ext2_ffz(unsigned int word)
{
	int b = 0, s;

	word = ~word;
	s = 16; if (word << 16 != 0) s = 0; b += s; word >>= s;
	s =  8; if (word << 24 != 0) s = 0; b += s; word >>= s;
	s =  4; if (word << 28 != 0) s = 0; b += s; word >>= s;
	s =  2; if (word << 30 != 0) s = 0; b += s; word >>= s;
	s =  1; if (word << 31 != 0) s = 0; b += s;

	return b;
}

static inline unsigned long find_next_zero_le_bit(void *addr,
	unsigned long size, unsigned long offset)
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
	tmp |= ~0 << size;
	if (tmp == ~0U)			/* Are any bits zero? */
		return result + size;	/* Nope. */

found_middle:
	return result + ext2_ffz(tmp);
}

#define find_first_zero_le_bit(addr, size) \
	find_next_zero_le_bit((addr), (size), 0)

#define ext2_set_bit			__test_and_set_le_bit
#define ext2_clear_bit			__test_and_clear_le_bit
#define ext2_test_bit			test_le_bit
#define ext2_find_first_zero_bit	find_first_zero_le_bit
#define ext2_find_next_zero_bit		find_next_zero_le_bit

/*
 * Bitmap functions for the minix filesystem.
 *
 * FIXME: These assume that Minix uses the native byte/bitorder.
 * This limits the Minix filesystem's value for data exchange very much.
 */
#define minix_test_and_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr) set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* _ASM_BITOPS_H */
