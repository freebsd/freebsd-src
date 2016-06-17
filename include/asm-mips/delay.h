/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 by Waldorf Electronics
 * Copyright (C) 1995 - 1998, 2001 by Ralf Baechle
 */
#ifndef _ASM_DELAY_H
#define _ASM_DELAY_H

#include <linux/config.h>
#include <linux/param.h>

extern unsigned long loops_per_jiffy;

static __inline__ void __delay(unsigned long loops)
{
	__asm__ __volatile__ (
		".set\tnoreorder\n"
		"1:\tbnez\t%0,1b\n\t"
		"subu\t%0,1\n\t"
		".set\treorder"
		:"=r" (loops)
		:"0" (loops));
}

/*
 * Division by multiplication: you don't have to worry about
 * loss of precision.
 *
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)
 */
static __inline__ void __udelay(unsigned long usecs, unsigned long lpj)
{
	unsigned long lo;

	/*
	 * Excessive precission?  Probably ...
	 */
	usecs *= (unsigned long) (((0x8000000000000000ULL / (500000 / HZ)) +
	                           0x80000000ULL) >> 32);
	__asm__("multu\t%2,%3"
		:"=h" (usecs), "=l" (lo)
		:"r" (usecs),"r" (lpj));
	__delay(usecs);
}

static __inline__ void __ndelay(unsigned long nsecs, unsigned long lpj)
{
	unsigned long lo;

	/*
	 * Excessive precission?  Probably ...
	 */
	nsecs *= (unsigned long) (((0x8000000000000000ULL / (500000000 / HZ)) +
	                           0x80000000ULL) >> 32);
	__asm__("multu\t%2,%3"
		:"=h" (nsecs), "=l" (lo)
		:"r" (nsecs),"r" (lpj));
	__delay(nsecs);
}

#ifdef CONFIG_SMP
#define __udelay_val cpu_data[smp_processor_id()].udelay_val
#else
#define __udelay_val loops_per_jiffy
#endif

#define udelay(usecs) __udelay((usecs),__udelay_val)
#define ndelay(nsecs) __ndelay((nsecs),__udelay_val)

#endif /* _ASM_DELAY_H */
