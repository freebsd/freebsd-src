#ifndef _S390_BITOPS_H
#define _S390_BITOPS_H

/*
 *  include/asm-s390/bitops.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Derived from "include/asm-i386/bitops.h"
 *    Copyright (C) 1992, Linus Torvalds
 *
 */
#include <linux/config.h>

/*
 * bit 0 is the LSB of *addr; bit 31 is the MSB of *addr;
 * bit 32 is the LSB of *(addr+4). That combined with the
 * big endian byte order on S390 give the following bit
 * order in memory:
 *    1f 1e 1d 1c 1b 1a 19 18 17 16 15 14 13 12 11 10 \
 *    0f 0e 0d 0c 0b 0a 09 08 07 06 05 04 03 02 01 00
 * after that follows the next long with bit numbers
 *    3f 3e 3d 3c 3b 3a 39 38 37 36 35 34 33 32 31 30
 *    2f 2e 2d 2c 2b 2a 29 28 27 26 25 24 23 22 21 20
 * The reason for this bit ordering is the fact that
 * in the architecture independent code bits operations
 * of the form "flags |= (1 << bitnr)" are used INTERMIXED
 * with operation of the form "set_bit(bitnr, flags)".
 */

/* set ALIGN_CS to 1 if the SMP safe bit operations should
 * align the address to 4 byte boundary. It seems to work
 * without the alignment. 
 */
#ifdef __KERNEL__
#define ALIGN_CS 0
#else
#define ALIGN_CS 1
#ifndef CONFIG_SMP
#error "bitops won't work without CONFIG_SMP"
#endif
#endif

/* bitmap tables from arch/S390/kernel/bitmap.S */
extern const char _oi_bitmap[];
extern const char _ni_bitmap[];
extern const char _zb_findmap[];

#ifdef CONFIG_SMP
/*
 * SMP save set_bit routine based on compare and swap (CS)
 */
static __inline__ void set_bit_cs(int nr, volatile void * addr)
{
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"       /* make OR mask */
             "   l     %0,0(%1)\n"
             "0: lr    %2,%0\n"         /* CS loop starts here */
             "   or    %2,%3\n"          /* set bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b"
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask) :
             : "cc", "memory" );
}

/*
 * SMP save clear_bit routine based on compare and swap (CS)
 */
static __inline__ void clear_bit_cs(int nr, volatile void * addr)
{
        static const int minusone = -1;
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"
             "   x     %3,%4\n"        /* make AND mask */
             "   l     %0,0(%1)\n"
             "0: lr    %2,%0\n"        /* CS loop starts here */
             "   nr    %2,%3\n"        /* clear bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b"
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask)
             : "m" (minusone) : "cc", "memory" );
}

/*
 * SMP save change_bit routine based on compare and swap (CS)
 */
static __inline__ void change_bit_cs(int nr, volatile void * addr)
{
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"     /* make XR mask */
             "   l     %0,0(%1)\n"
             "0: lr    %2,%0\n"        /* CS loop starts here */
             "   xr    %2,%3\n"        /* change bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b"
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask) : 
             : "cc", "memory" );
}

/*
 * SMP save test_and_set_bit routine based on compare and swap (CS)
 */
static __inline__ int test_and_set_bit_cs(int nr, volatile void * addr)
{
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"     /* make OR mask */
             "   l     %0,0(%1)\n"
             "0: lr    %2,%0\n"        /* CS loop starts here */
             "   or    %2,%3\n"        /* set bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b\n"
             "   nr    %0,%3\n"        /* isolate old bit */
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask) :
             : "cc", "memory" );
        return nr != 0;
}

/*
 * SMP save test_and_clear_bit routine based on compare and swap (CS)
 */
static __inline__ int test_and_clear_bit_cs(int nr, volatile void * addr)
{
        static const int minusone = -1;
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"
             "   l     %0,0(%1)\n"
             "   x     %3,%4\n"        /* make AND mask */
             "0: lr    %2,%0\n"        /* CS loop starts here */
             "   nr    %2,%3\n"        /* clear bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b\n"
             "   x     %3,%4\n"
             "   nr    %0,%3\n"         /* isolate old bit */
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask)
             : "m" (minusone) : "cc", "memory" );
        return nr;
}

/*
 * SMP save test_and_change_bit routine based on compare and swap (CS) 
 */
static __inline__ int test_and_change_bit_cs(int nr, volatile void * addr)
{
	unsigned long bits, mask;
        __asm__ __volatile__(
#if ALIGN_CS == 1
             "   lhi   %2,3\n"         /* CS must be aligned on 4 byte b. */
             "   nr    %2,%1\n"        /* isolate last 2 bits of address */
             "   xr    %1,%2\n"        /* make addr % 4 == 0 */
             "   sll   %2,3\n"
             "   ar    %0,%2\n"        /* add alignement to bitnr */
#endif
             "   lhi   %2,31\n"
             "   nr    %2,%0\n"        /* make shift value */
             "   xr    %0,%2\n"
             "   srl   %0,3\n"
             "   lhi   %3,1\n"
             "   la    %1,0(%0,%1)\n"  /* calc. address for CS */
             "   sll   %3,0(%2)\n"     /* make OR mask */
             "   l     %0,0(%1)\n"
             "0: lr    %2,%0\n"        /* CS loop starts here */
             "   xr    %2,%3\n"        /* change bit */
             "   cs    %0,%2,0(%1)\n"
             "   jl    0b\n"
             "   nr    %0,%3\n"        /* isolate old bit */
             : "+a" (nr), "+a" (addr), "=&a" (bits), "=&d" (mask) :
             : "cc", "memory" );
        return nr != 0;
}
#endif /* CONFIG_SMP */

/*
 * fast, non-SMP set_bit routine
 */
static __inline__ void __set_bit(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %0,7\n"
             "   xr    %1,%2\n"
             "   nr    %0,%2\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%3)\n"
             "   la    %0,0(%0,%4)\n"
             "   oc    0(1,%1),0(%0)"
             : "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_oi_bitmap) : "cc", "memory" );
}

static __inline__ void 
__constant_set_bit(const int nr, volatile void * addr)
{
  switch (nr&7) {
  case 0:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x01"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3))) 
                          : : "1", "cc", "memory");
    break;
  case 1:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x02"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 2:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x04"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 3:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x08"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 4:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x10"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 5:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x20"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 6:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x40"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 7:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "oi 0(1),0x80"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  }
}

#define set_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_set_bit((nr),(addr)) : \
 __set_bit((nr),(addr)) )

/*
 * fast, non-SMP clear_bit routine
 */
static __inline__ void 
__clear_bit(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %0,7\n"
             "   xr    %1,%2\n"
             "   nr    %0,%2\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%3)\n"
             "   la    %0,0(%0,%4)\n"
             "   nc    0(1,%1),0(%0)"
             : "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_ni_bitmap) : "cc", "memory" );
}

static __inline__ void 
__constant_clear_bit(const int nr, volatile void * addr)
{
  switch (nr&7) {
  case 0:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xFE"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 1:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xFD"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 2:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xFB"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 3:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xF7"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 4:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xEF"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 5:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xDF"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 6:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0xBF"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 7:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "ni 0(1),0x7F"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  }
}

#define clear_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_clear_bit((nr),(addr)) : \
 __clear_bit((nr),(addr)) )

/* 
 * fast, non-SMP change_bit routine 
 */
static __inline__ void __change_bit(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %0,7\n"
             "   xr    %1,%2\n"
             "   nr    %0,%2\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%3)\n"
             "   la    %0,0(%0,%4)\n"
             "   xc    0(1,%1),0(%0)"
             : "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_oi_bitmap) : "cc", "memory" );
}

static __inline__ void 
__constant_change_bit(const int nr, volatile void * addr) 
{
  switch (nr&7) {
  case 0:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x01"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 1:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x02"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 2:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x04"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 3:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x08"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 4:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x10"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "cc", "memory" );
    break;
  case 5:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x20"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 6:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x40"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  case 7:
    __asm__ __volatile__ ("la 1,%0\n\t"
                          "xi 0(1),0x80"
                          : "=m" (*((volatile char *) addr + ((nr>>3)^3)))
                          : : "1", "cc", "memory" );
    break;
  }
}

#define change_bit_simple(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_change_bit((nr),(addr)) : \
 __change_bit((nr),(addr)) )

/*
 * fast, non-SMP test_and_set_bit routine
 */
static __inline__ int test_and_set_bit_simple(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        int oldbit;
        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %2,7\n"
             "   xr    %1,%3\n"
             "   nr    %2,%3\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%4)\n"
             "   ic    %0,0(%1)\n"
             "   srl   %0,0(%2)\n"
             "   la    %2,0(%2,%5)\n"
             "   oc    0(1,%1),0(%2)"
             : "=d&" (oldbit), "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_oi_bitmap) : "cc", "memory" );
        return oldbit & 1;
}
#define __test_and_set_bit(X,Y)		test_and_set_bit_simple(X,Y)

/*
 * fast, non-SMP test_and_clear_bit routine
 */
static __inline__ int test_and_clear_bit_simple(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        int oldbit;

        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %2,7\n"
             "   xr    %1,%3\n"
             "   nr    %2,%3\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%4)\n"
             "   ic    %0,0(%1)\n"
             "   srl   %0,0(%2)\n"
             "   la    %2,0(%2,%5)\n"
             "   nc    0(1,%1),0(%2)"
             : "=d&" (oldbit), "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_ni_bitmap) : "cc", "memory" );
        return oldbit & 1;
}
#define __test_and_clear_bit(X,Y)	test_and_clear_bit_simple(X,Y)

/*
 * fast, non-SMP test_and_change_bit routine
 */
static __inline__ int test_and_change_bit_simple(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        int oldbit;

        __asm__ __volatile__(
             "   lhi   %1,24\n"
             "   lhi   %2,7\n"
             "   xr    %1,%3\n"
             "   nr    %2,%1\n"
             "   srl   %1,3\n"
             "   la    %1,0(%1,%4)\n"
             "   ic    %0,0(%1)\n"
             "   srl   %0,0(%2)\n"
             "   la    %2,0(%2,%5)\n"
             "   xc    0(1,%1),0(%2)"
             : "=d&" (oldbit), "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr), "a" (&_oi_bitmap) : "cc", "memory" );
        return oldbit & 1;
}
#define __test_and_change_bit(X,Y)	test_and_change_bit_simple(X,Y)

#ifdef CONFIG_SMP
#define set_bit             set_bit_cs
#define clear_bit           clear_bit_cs
#define change_bit          change_bit_cs
#define test_and_set_bit    test_and_set_bit_cs
#define test_and_clear_bit  test_and_clear_bit_cs
#define test_and_change_bit test_and_change_bit_cs
#else
#define set_bit             set_bit_simple
#define clear_bit           clear_bit_simple
#define change_bit          change_bit_simple
#define test_and_set_bit    test_and_set_bit_simple
#define test_and_clear_bit  test_and_clear_bit_simple
#define test_and_change_bit test_and_change_bit_simple
#endif


/*
 * This routine doesn't need to be atomic.
 */

static __inline__ int __test_bit(int nr, volatile void * addr)
{
	unsigned long reg1, reg2;
        int oldbit;

        __asm__ __volatile__(
             "   lhi   %2,24\n"
             "   lhi   %1,7\n"
             "   xr    %2,%3\n"
             "   nr    %1,%3\n"
             "   srl   %2,3\n"
             "   ic    %0,0(%2,%4)\n"
             "   srl   %0,0(%1)"
             : "=d&" (oldbit), "=&a" (reg1), "=&a" (reg2)
             : "r" (nr), "a" (addr) : "cc" );
        return oldbit & 1;
}

static __inline__ int __constant_test_bit(int nr, volatile void * addr) {
    return (((volatile char *) addr)[(nr>>3)^3] & (1<<(nr&7))) != 0;
}

#define test_bit(nr,addr) \
(__builtin_constant_p((nr)) ? \
 __constant_test_bit((nr),(addr)) : \
 __test_bit((nr),(addr)) )

/*
 * Find-bit routines..
 */
static __inline__ int find_first_zero_bit(void * addr, unsigned size)
{
	unsigned long cmp, count;
        int res;

        if (!size)
                return 0;
        __asm__("   lhi  %1,-1\n"
                "   lr   %2,%3\n"
                "   slr  %0,%0\n"
                "   ahi  %2,31\n"
                "   srl  %2,5\n"
                "0: c    %1,0(%0,%4)\n"
                "   jne  1f\n"
                "   ahi  %0,4\n"
                "   brct %2,0b\n"
                "   lr   %0,%3\n"
                "   j    4f\n"
                "1: l    %2,0(%0,%4)\n"
                "   sll  %0,3\n"
                "   lhi  %1,0xff\n"
                "   tml  %2,0xffff\n"
                "   jno  2f\n"
                "   ahi  %0,16\n"
                "   srl  %2,16\n"
                "2: tml  %2,0x00ff\n"
                "   jno  3f\n"
                "   ahi  %0,8\n"
                "   srl  %2,8\n"
                "3: nr   %2,%1\n"
                "   ic   %2,0(%2,%5)\n"
                "   alr  %0,%2\n"
                "4:"
                : "=&a" (res), "=&d" (cmp), "=&a" (count)
                : "a" (size), "a" (addr), "a" (&_zb_findmap) : "cc" );
        return (res < size) ? res : size;
}

static __inline__ int find_next_zero_bit (void * addr, int size, int offset)
{
        unsigned long * p = ((unsigned long *) addr) + (offset >> 5);
        unsigned long bitvec, reg;
        int set, bit = offset & 31, res;

        if (bit) {
                /*
                 * Look for zero in first word
                 */
                bitvec = (*p) >> bit;
                __asm__("   slr  %0,%0\n"
                        "   lhi  %2,0xff\n"
                        "   tml  %1,0xffff\n"
                        "   jno  0f\n"
                        "   ahi  %0,16\n"
                        "   srl  %1,16\n"
                        "0: tml  %1,0x00ff\n"
                        "   jno  1f\n"
                        "   ahi  %0,8\n"
                        "   srl  %1,8\n"
                        "1: nr   %1,%2\n"
                        "   ic   %1,0(%1,%3)\n"
                        "   alr  %0,%1"
                        : "=&d" (set), "+a" (bitvec), "=&d" (reg)
                        : "a" (&_zb_findmap) : "cc" );
                if (set < (32 - bit))
                        return set + offset;
                offset += 32 - bit;
                p++;
        }
        /*
         * No zero yet, search remaining full words for a zero
         */
        res = find_first_zero_bit (p, size - 32 * (p - (unsigned long *) addr));
        return (offset + res);
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
static __inline__ unsigned long ffz(unsigned long word)
{
	unsigned long reg;
        int result;

        __asm__("   slr  %0,%0\n"
                "   lhi  %2,0xff\n"
                "   tml  %1,0xffff\n"
                "   jno  0f\n"
                "   ahi  %0,16\n"
                "   srl  %1,16\n"
                "0: tml  %1,0x00ff\n"
                "   jno  1f\n"
                "   ahi  %0,8\n"
                "   srl  %1,8\n"
                "1: nr   %1,%2\n"
                "   ic   %1,0(%1,%3)\n"
                "   alr  %0,%1"
                : "=&d" (result), "+a" (word), "=&d" (reg)
                : "a" (&_zb_findmap) : "cc" );
        return result;
}

/*
 * ffs: find first bit set. This is defined the same way as
 * the libc and compiler builtin ffs routines, therefore
 * differs in spirit from the above ffz (man ffs).
 */

extern int __inline__ ffs (int x)
{
        int r;

        if (x == 0)
          return 0;
        __asm__("    slr  %0,%0\n"
                "    tml  %1,0xffff\n"
                "    jnz  0f\n"
                "    ahi  %0,16\n"
                "    srl  %1,16\n"
                "0:  tml  %1,0x00ff\n"
                "    jnz  1f\n"
                "    ahi  %0,8\n"
                "    srl  %1,8\n"
                "1:  tml  %1,0x000f\n"
                "    jnz  2f\n"
                "    ahi  %0,4\n"
                "    srl  %1,4\n"
                "2:  tml  %1,0x0003\n"
                "    jnz  3f\n"
                "    ahi  %0,2\n"
                "    srl  %1,2\n"
                "3:  tml  %1,0x0001\n"
                "    jnz  4f\n"
                "    ahi  %0,1\n"
                "4:"
                : "=&d" (r), "+d" (x) : : "cc" );
        return r+1;
}

/*
 * hweightN: returns the hamming weight (i.e. the number
 * of bits set) of a N-bit word
 */

#define hweight32(x) generic_hweight32(x)
#define hweight16(x) generic_hweight16(x)
#define hweight8(x) generic_hweight8(x)


#ifdef __KERNEL__

/*
 * ATTENTION: intel byte ordering convention for ext2 and minix !!
 * bit 0 is the LSB of addr; bit 31 is the MSB of addr;
 * bit 32 is the LSB of (addr+4).
 * That combined with the little endian byte order of Intel gives the
 * following bit order in memory:
 *    07 06 05 04 03 02 01 00 15 14 13 12 11 10 09 08 \
 *    23 22 21 20 19 18 17 16 31 30 29 28 27 26 25 24
 */

#define ext2_set_bit(nr, addr)       test_and_set_bit((nr)^24, addr)
#define ext2_clear_bit(nr, addr)     test_and_clear_bit((nr)^24, addr)
#define ext2_test_bit(nr, addr)      test_bit((nr)^24, addr)
static __inline__ int ext2_find_first_zero_bit(void *vaddr, unsigned size)
{
	unsigned long cmp, count;
        int res;

        if (!size)
                return 0;
        __asm__("   lhi  %1,-1\n"
                "   lr   %2,%3\n"
                "   ahi  %2,31\n"
                "   srl  %2,5\n"
                "   slr  %0,%0\n"
                "0: cl   %1,0(%0,%4)\n"
                "   jne  1f\n"
                "   ahi  %0,4\n"
                "   brct %2,0b\n"
                "   lr   %0,%3\n"
                "   j    4f\n"
                "1: l    %2,0(%0,%4)\n"
                "   sll  %0,3\n"
                "   ahi  %0,24\n"
                "   lhi  %1,0xff\n"
                "   tmh  %2,0xffff\n"
                "   jo   2f\n"
                "   ahi  %0,-16\n"
                "   srl  %2,16\n"
                "2: tml  %2,0xff00\n"
                "   jo   3f\n"
                "   ahi  %0,-8\n"
                "   srl  %2,8\n"
                "3: nr   %2,%1\n"
                "   ic   %2,0(%2,%5)\n"
                "   alr  %0,%2\n"
                "4:"
                : "=&a" (res), "=&d" (cmp), "=&a" (count)
                : "a" (size), "a" (vaddr), "a" (&_zb_findmap) : "cc" );
        return (res < size) ? res : size;
}

static __inline__ int 
ext2_find_next_zero_bit(void *vaddr, unsigned size, unsigned offset)
{
        unsigned long *addr = vaddr;
        unsigned long *p = addr + (offset >> 5);
        unsigned long word, reg;
        int bit = offset & 31UL, res;

        if (offset >= size)
                return size;

        if (bit) {
                __asm__("   ic   %0,0(%1)\n"
                        "   icm  %0,2,1(%1)\n"
                        "   icm  %0,4,2(%1)\n"
                        "   icm  %0,8,3(%1)"
                        : "=&a" (word) : "a" (p) : "cc" );
		word >>= bit;
                res = bit;
                /* Look for zero in first longword */
                __asm__("   lhi  %2,0xff\n"
                        "   tml  %1,0xffff\n"
                	"   jno  0f\n"
                	"   ahi  %0,16\n"
                	"   srl  %1,16\n"
                	"0: tml  %1,0x00ff\n"
                	"   jno  1f\n"
                	"   ahi  %0,8\n"
                	"   srl  %1,8\n"
                	"1: nr   %1,%2\n"
                	"   ic   %1,0(%1,%3)\n"
                	"   alr  %0,%1"
                	: "+&d" (res), "+&a" (word), "=&d" (reg)
                  	: "a" (&_zb_findmap) : "cc" );
                if (res < 32)
			return (p - addr)*32 + res;
                p++;
        }
        /* No zero yet, search remaining full bytes for a zero */
        res = ext2_find_first_zero_bit (p, size - 32 * (p - addr));
        return (p - addr) * 32 + res;
}

/* Bitmap functions for the minix filesystem.  */
/* FIXME !!! */
#define minix_test_and_set_bit(nr,addr) test_and_set_bit(nr,addr)
#define minix_set_bit(nr,addr) set_bit(nr,addr)
#define minix_test_and_clear_bit(nr,addr) test_and_clear_bit(nr,addr)
#define minix_test_bit(nr,addr) test_bit(nr,addr)
#define minix_find_first_zero_bit(addr,size) find_first_zero_bit(addr,size)

#endif /* __KERNEL__ */

#endif /* _S390_BITOPS_H */
