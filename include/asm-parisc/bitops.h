#ifndef _PARISC_BITOPS_H
#define _PARISC_BITOPS_H

#include <linux/spinlock.h>
#include <asm/system.h>
#include <asm/byteorder.h>
#include <asm/atomic.h>

/*
 * HP-PARISC specific bit operations
 * for a detailed description of the functions please refer
 * to include/asm-i386/bitops.h or kerneldoc
 */

#ifdef __LP64__
#   define SHIFT_PER_LONG 6
#ifndef BITS_PER_LONG
#   define BITS_PER_LONG 64
#endif
#else
#   define SHIFT_PER_LONG 5
#ifndef BITS_PER_LONG
#   define BITS_PER_LONG 32
#endif
#endif

#define CHOP_SHIFTCOUNT(x) ((x) & (BITS_PER_LONG - 1))


#define smp_mb__before_clear_bit()      smp_mb()
#define smp_mb__after_clear_bit()       smp_mb()

static __inline__ void set_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	*addr |= mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);
}

static __inline__ void __set_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	*addr |= mask;
}

static __inline__ void clear_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	*addr &= ~mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);
}

static __inline__ void change_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	*addr ^= mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);
}

static __inline__ void __change_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	*addr ^= mask;
}

static __inline__ int test_and_set_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr |= mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);

	return oldbit;
}

static __inline__ int __test_and_set_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr |= mask;

	return oldbit;
}

static __inline__ int test_and_clear_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr &= ~mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);

	return oldbit;
}

static __inline__ int __test_and_clear_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr &= ~mask;

	return oldbit;
}

static __inline__ int test_and_change_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;
	unsigned long flags;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	SPIN_LOCK_IRQSAVE(ATOMIC_HASH(addr), flags);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr ^= mask;
	SPIN_UNLOCK_IRQRESTORE(ATOMIC_HASH(addr), flags);

	return oldbit;
}

static __inline__ int __test_and_change_bit(int nr, void * address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	int oldbit;

	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	oldbit = (*addr & mask) ? 1 : 0;
	*addr ^= mask;

	return oldbit;
}

static __inline__ int test_bit(int nr, const void *address)
{
	unsigned long mask;
	unsigned long *addr = (unsigned long *) address;
	
	addr += (nr >> SHIFT_PER_LONG);
	mask = 1L << CHOP_SHIFTCOUNT(nr);
	
	return !!(*addr & mask);
}

extern __inline__ unsigned long ffz(unsigned long word)
{
	unsigned long result;

	result = 0;
	while (word & 1) {
		result++;
		word >>= 1;
	}

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

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)

#endif /* __KERNEL__ */

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
#define find_first_zero_bit(addr, size) \
	find_next_zero_bit((addr), (size), 0)

static __inline__ unsigned long find_next_zero_bit(void * addr, unsigned long size, unsigned long offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> SHIFT_PER_LONG);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset &= (BITS_PER_LONG-1);
	if (offset) {
		tmp = *(p++);
		tmp |= ~0UL >> (BITS_PER_LONG-offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (~tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG -1)) {
		if (~(tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;
found_first:
	tmp |= ~0UL << size;
found_middle:
	return result + ffz(tmp);
}

#define _EXT2_HAVE_ASM_BITOPS_

#ifdef __KERNEL__
/*
 * test_and_{set,clear}_bit guarantee atomicity without
 * disabling interrupts.
 */
#ifdef __LP64__
#define ext2_set_bit(nr, addr)		test_and_set_bit((nr) ^ 0x38, addr)
#define ext2_clear_bit(nr, addr)	test_and_clear_bit((nr) ^ 0x38, addr)
#else
#define ext2_set_bit(nr, addr)		test_and_set_bit((nr) ^ 0x18, addr)
#define ext2_clear_bit(nr, addr)	test_and_clear_bit((nr) ^ 0x18, addr)
#endif

#endif	/* __KERNEL__ */

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

extern __inline__ unsigned long ext2_find_next_zero_bit(void *addr,
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
found_middle:
	return result + ffz(tmp);
}

/* Bitmap functions for the minix filesystem.  */
#define minix_test_and_set_bit(nr,addr) ext2_set_bit(nr,addr)
#define minix_set_bit(nr,addr) ((void)ext2_set_bit(nr,addr))
#define minix_test_and_clear_bit(nr,addr) ext2_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) ext2_test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) ext2_find_first_zero_bit(addr,size)

#endif /* _PARISC_BITOPS_H */
