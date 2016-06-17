#ifndef _S390_BYTEORDER_H
#define _S390_BYTEORDER_H

/*
 *  include/asm-s390/byteorder.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com)
 */

#include <asm/types.h>

#ifdef __GNUC__

static __inline__ __const__ __u32 ___arch__swab32(__u32 x)
{
  __u32 temp;

  __asm__ __volatile__ (
          "        st    %0,0(%1)\n"
          "        icm   %0,8,3(%1)\n"
          "        icm   %0,4,2(%1)\n"
          "        icm   %0,2,1(%1)\n"
          "        ic    %0,0(%1)"
          : "+&d" (x) : "a" (&temp) : "cc" );
  return x;
}

static __inline__ __const__ __u32 ___arch__swab32p(__u32 *x)
{
  __u32 result;

  __asm__ __volatile__ (
          "        icm   %0,8,3(%1)\n"
          "        icm   %0,4,2(%1)\n"
          "        icm   %0,2,1(%1)\n"
          "        ic    %0,0(%1)"
          : "=&d" (result) : "a" (x) : "cc" );
  return result;
}

static __inline__ void ___arch__swab32s(__u32 *x)
{
  __asm__ __volatile__ (
          "        icm   0,8,3(%0)\n"
          "        icm   0,4,2(%0)\n"
          "        icm   0,2,1(%0)\n"
          "        ic    0,0(%0)\n"
          "        st    0,0(%0)"
          : : "a" (x) : "0", "memory", "cc");
}

static __inline__ __const__ __u16 ___arch__swab16(__u16 x)
{
  __u16 temp;

  __asm__ __volatile__ (
          "        sth   %0,0(%1)\n"
          "        icm   %0,2,1(%1)\n"
          "        ic    %0,0(%1)\n"
          : "+&d" (x) : "a" (&temp) : "memory", "cc" );
  return x;
}

static __inline__ __const__ __u16 ___arch__swab16p(__u16 *x)
{
  __u16 result;

  __asm__ __volatile__ (
          "        sr    %0,%0\n"
          "        icm   %0,2,1(%1)\n"
          "        ic    %0,0(%1)\n"
          : "=&d" (result) : "a" (x) : "cc" );
  return result;
}

static __inline__ void ___arch__swab16s(__u16 *x)
{
  __asm__ __volatile__(
          "        icm   0,2,1(%0)\n"
          "        ic    0,0(%0)\n"
          "        sth   0,0(%0)"
          : : "a" (x) : "0", "memory", "cc" );
}

#define __arch__swab32(x) ___arch__swab32(x)
#define __arch__swab16(x) ___arch__swab16(x)
#define __arch__swab32p(x) ___arch__swab32p(x)
#define __arch__swab16p(x) ___arch__swab16p(x)
#define __arch__swab32s(x) ___arch__swab32s(x)
#define __arch__swab16s(x) ___arch__swab16s(x)

#if !defined(__STRICT_ANSI__) || defined(__KERNEL__)
#  define __BYTEORDER_HAS_U64__
#  define __SWAB_64_THRU_32__
#endif

#endif /* __GNUC__ */

#include <linux/byteorder/big_endian.h>

#endif /* _S390_BYTEORDER_H */
