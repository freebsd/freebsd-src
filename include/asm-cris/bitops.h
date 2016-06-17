/* asm/bitops.h for Linux/CRIS
 *
 * TODO: asm versions if speed is needed
 *       set_bit, clear_bit and change_bit wastes cycles being only
 *       macros into test_and_set_bit etc.
 *       kernel-doc things (**) for macros are disabled
 *
 * All bit operations return 0 if the bit was cleared before the
 * operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

#ifndef _CRIS_BITOPS_H
#define _CRIS_BITOPS_H

/* Currently this is unsuitable for consumption outside the kernel.  */
#ifdef __KERNEL__ 

#include <asm/system.h>

/* We use generic_ffs so get it; include guards resolve the possible
   mutually inclusion.  */
#include <linux/bitops.h>

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy *) addr)
#define CONST_ADDR (*(const struct __dummy *) addr)

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

#define set_bit(nr, addr)    (void)test_and_set_bit(nr, addr)
#define __set_bit(nr, addr)    (void)__test_and_set_bit(nr, addr)

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

#define clear_bit(nr, addr)  (void)test_and_clear_bit(nr, addr)
#define __clear_bit(nr, addr)  (void)__test_and_clear_bit(nr, addr)

/*
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */

#define change_bit(nr, addr) (void)test_and_change_bit(nr, addr)

/*
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */

#define __change_bit(nr, addr) (void)__test_and_change_bit(nr, addr)

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */

extern __inline__ int test_and_set_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	save_flags(flags);
	cli();
	retval = (mask & *adr) != 0;
	*adr |= mask;
	restore_flags(flags);
	return retval;
}

extern inline int __test_and_set_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *adr) != 0;
	*adr |= mask;
	return retval;
}

/*
 * clear_bit() doesn't provide any barrier for the compiler.
 */
#define smp_mb__before_clear_bit()      barrier()
#define smp_mb__after_clear_bit()       barrier()

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */

extern __inline__ int test_and_clear_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	save_flags(flags);
	cli();
	retval = (mask & *adr) != 0;
	*adr &= ~mask;
	restore_flags(flags);
	return retval;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.  
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */

extern __inline__ int __test_and_clear_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *adr) != 0;
	*adr &= ~mask;
	return retval;
}
/**
 * test_and_change_bit - Change a bit and return its new value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.  
 * It also implies a memory barrier.
 */

extern __inline__ int test_and_change_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned long flags;
	unsigned int *adr = (unsigned int *)addr;
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	save_flags(flags);
	cli();
	retval = (mask & *adr) != 0;
	*adr ^= mask;
	restore_flags(flags);
	return retval;
}

/* WARNING: non atomic and it can be reordered! */

extern __inline__ int __test_and_change_bit(int nr, void *addr)
{
	unsigned int mask, retval;
	unsigned int *adr = (unsigned int *)addr;

	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *adr) != 0;
	*adr ^= mask;

	return retval;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 *
 * This routine doesn't need to be atomic.
 */

extern __inline__ int test_bit(int nr, const void *addr)
{
	unsigned int mask;
	unsigned int *adr = (unsigned int *)addr;
	
	adr += nr >> 5;
	mask = 1 << (nr & 0x1f);
	return ((mask & *adr) != 0);
}

/*
 * Find-bit routines..
 */

/*
 * Helper functions for the core of the ff[sz] functions, wrapping the
 * syntactically awkward asms.  The asms compute the number of leading
 * zeroes of a bits-in-byte and byte-in-word and word-in-dword-swapped
 * number.  They differ in that the first function also inverts all bits
 * in the input.
 */
extern __inline__ unsigned long cris_swapnwbrlz(unsigned long w)
{
	/* Let's just say we return the result in the same register as the
	   input.  Saying we clobber the input but can return the result
	   in another register:
	   !  __asm__ ("swapnwbr %2\n\tlz %2,%0"
	   !	      : "=r,r" (res), "=r,X" (dummy) : "1,0" (w));
	   confuses gcc (sched.c, gcc from cris-dist-1.14).  */

	unsigned long res;
	__asm__ ("swapnwbr %0 \n\t"
		 "lz %0,%0"
		 : "=r" (res) : "0" (w));
	return res;
}

extern __inline__ unsigned long cris_swapwbrlz(unsigned long w)
{
	unsigned res;
	__asm__ ("swapwbr %0 \n\t"
		 "lz %0,%0"
		 : "=r" (res)
		 : "0" (w));
	return res;
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
extern __inline__ unsigned long ffz(unsigned long w)
{
	/* The generic_ffs function is used to avoid the asm when the
	   argument is a constant.  */
	return __builtin_constant_p (w)
		? (~w ? (unsigned long) generic_ffs ((int) ~w) - 1 : 32)
		: cris_swapnwbrlz (w);
}

/*
 * Somewhat like ffz but the equivalent of generic_ffs: in contrast to
 * ffz we return the first one-bit *plus one*.
 */
extern __inline__ unsigned long kernel_ffs(unsigned long w)
{
	/* The generic_ffs function is used to avoid the asm when the
	   argument is a constant.  */
	return __builtin_constant_p (w)
		? (unsigned long) generic_ffs ((int) w)
		: w ? cris_swapwbrlz (w) + 1 : 0;
}

/*
 * Since we define it "external", it collides with the built-in
 * definition, which doesn't have the same semantics.  We don't want to
 * use -fno-builtin, so just hide the name ffs.
 */
#define ffs kernel_ffs

/**
 * find_next_zero_bit - find the first zero bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 */
extern __inline__ int find_next_zero_bit (void * addr, int size, int offset)
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
	tmp |= ~0UL >> size;
 found_middle:
	return result + ffz(tmp);
}

/**
 * find_first_zero_bit - find the first zero bit in a memory region
 * @addr: The address to start the search at
 * @size: The maximum size to search
 *
 * Returns the bit-number of the first zero bit, not the number of the byte
 * containing a bit.
 */

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

#define ext2_set_bit                 test_and_set_bit
#define ext2_clear_bit               test_and_clear_bit
#define ext2_test_bit                test_bit
#define ext2_find_first_zero_bit     find_first_zero_bit
#define ext2_find_next_zero_bit      find_next_zero_bit

/* Bitmap functions for the minix filesystem.  */
#define minix_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */


#endif /* _CRIS_BITOPS_H */
