/* $FreeBSD$ */
#ifndef _ALPHA_BITOPS_H
#define _ALPHA_BITOPS_H

/*
 * Copyright 1994, Linus Torvalds.
 */

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * To get proper branch prediction for the main line, we must branch
 * forward to code at the end of this object's .text section, then
 * branch back to restart the operation.
 *
 * bit 0 is the LSB of addr; bit 64 is the LSB of (addr+1).
 */
static __inline unsigned int set_bit(unsigned long, volatile void *);
static __inline unsigned int clear_bit(unsigned long, volatile void *);
static __inline unsigned int change_bit(unsigned long, volatile void *);
static __inline unsigned int test_bit(int, volatile void *);
static __inline unsigned long ffz_b(unsigned long x);
static __inline unsigned long ffz(unsigned long);
/* static __inline int ffs(int); */
static __inline void * memscan(void *, int, size_t);
#ifdef __alpha_cix__
static __inline unsigned long hweight64(unsigned long);
#endif
static __inline unsigned long
find_next_zero_bit(void *, unsigned long, unsigned long);

static __inline unsigned int set_bit(unsigned long nr, volatile void * addr)
{
	unsigned long oldbit;
	unsigned long temp;
	volatile unsigned int *m = ((volatile unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	and %0,%3,%2\n"
	"	bne %2,2f\n"
	"	xor %0,%3,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,3f\n"
	"2:\n"
	".section .text2,\"ax\"\n"
	"3:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (*m), "=&r" (oldbit)
	:"Ir" (1UL << (nr & 31)), "m" (*m));
	return oldbit;
}

static __inline unsigned int clear_bit(unsigned long nr, volatile void * addr)
{
	unsigned long oldbit;
	unsigned long temp;
	volatile unsigned int *m = ((volatile unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	and %0,%3,%2\n"
	"	beq %2,2f\n"
	"	xor %0,%3,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,3f\n"
	"2:\n"
	".section .text2,\"ax\"\n"
	"3:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (*m), "=&r" (oldbit)
	:"Ir" (1UL << (nr & 31)), "m" (*m));
	return oldbit;
}

static __inline unsigned int change_bit(unsigned long nr, volatile void * addr)
{
	unsigned long oldbit;
	unsigned long temp;
	volatile unsigned int *m = ((volatile unsigned int *) addr) + (nr >> 5);

	__asm__ __volatile__(
	"1:	ldl_l %0,%1\n"
	"	xor %0,%2,%0\n"
	"	stl_c %0,%1\n"
	"	beq %0,3f\n"
	".section .text2,\"ax\"\n"
	"3:	br 1b\n"
	".previous"
	:"=&r" (temp), "=m" (*m), "=&r" (oldbit)
	:"Ir" (1UL << (nr & 31)), "m" (*m));
	return oldbit;
}

static __inline unsigned int test_bit(int nr, volatile void * addr)
{
	return 1UL & (((volatile int *) addr)[nr >> 5] >> (nr & 31));
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 *
 * Do a binary search on the bits.  Due to the nature of large
 * constants on the alpha, it is worthwhile to split the search.
 */
static __inline unsigned long ffz_b(unsigned long x)
{
	unsigned long sum = 0;

	x = ~x & -~x;		/* set first 0 bit, clear others */
	if (x & 0xF0) sum += 4;
	if (x & 0xCC) sum += 2;
	if (x & 0xAA) sum += 1;

	return sum;
}

static __inline unsigned long ffz(unsigned long word)
{
#ifdef __alpha_cix__
	/* Whee.  EV6 can calculate it directly.  */
	unsigned long result;
	__asm__("ctlz %1,%0" : "=r"(result) : "r"(~word));
	return result;
#else
	unsigned long bits, qofs, bofs;

	__asm__("cmpbge %1,%2,%0" : "=r"(bits) : "r"(word), "r"(~0UL));
	qofs = ffz_b(bits);
	__asm__("extbl %1,%2,%0" : "=r"(bits) : "r"(word), "r"(qofs));
	bofs = ffz_b(bits);

	return qofs*8 + bofs;
#endif
}

#ifdef __KERNEL__
#if	0
/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

static __inline int ffs(int word)
{
	int result = ffz(~word);
	return word ? result+1 : 0;
}
#endif

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

#ifdef __alpha_cix__
/* Whee.  EV6 can calculate it directly.  */
static __inline unsigned long hweight64(unsigned long w)
{
	unsigned long result;
	__asm__("ctpop %1,%0" : "=r"(result) : "r"(w));
	return result;
}

#define hweight32(x) hweight64((x) & 0xfffffffful)
#define hweight16(x) hweight64((x) & 0xfffful)
#define hweight8(x)  hweight64((x) & 0xfful)
#else
#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x)  generic_hweight8(x)
#endif

#endif /* __alpha_cix__ */

/* from lib/string.c */
static __inline void * memscan(void * addr, int c, size_t size)
{
	unsigned char * p = (unsigned char *) addr;

	while (size) {
		if (*p == c)
			return (void *) p;
		p++;
		size--;
	}
  	return (void *) p;
}


/*
 * Find next zero bit in a bitmap reasonably efficiently..
 */
static __inline unsigned long find_next_zero_bit(void * addr, unsigned long size, unsigned long offset)
{
	unsigned long * p = ((unsigned long *) addr) + (offset >> 6);
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
found_middle:
	return result + ffz(tmp);
}

/*
 * The optimizer actually does good code for this case..
 */
#define find_first_zero_bit(addr, size) \
	find_next_zero_bit((addr), (size), 0)

#ifdef __KERNEL__

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

#endif /* _ALPHA_BITOPS_H */
