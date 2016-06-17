/* $Id: delay.h,v 1.12.2.1 2002/02/02 02:11:52 kanoj Exp $
 * delay.h: Linux delay routines on the V9.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu).
 */

#ifndef __SPARC64_DELAY_H
#define __SPARC64_DELAY_H

#include <linux/config.h>
#include <linux/param.h>

#ifndef __ASSEMBLY__

#ifdef CONFIG_SMP
#include <asm/smp.h>
#else
extern unsigned long loops_per_jiffy;
#endif 

extern __inline__ void __delay(unsigned long loops)
{
	__asm__ __volatile__(
"	b,pt	%%xcc, 1f\n"
"	 cmp	%0, 0\n"
"	.align	32\n"
"1:\n"
"	bne,pt	%%xcc, 1b\n"
"	 subcc	%0, 1, %0\n"
	: "=&r" (loops)
	: "0" (loops)
	: "cc");
}

extern __inline__ void __udelay(unsigned long usecs, unsigned long lps)
{
	usecs *= 0x00000000000010c6UL;		/* 2**32 / 1000000 */

	__asm__ __volatile__(
"	mulx	%1, %2, %0\n"
"	srlx	%0, 32, %0\n"
	: "=r" (usecs)
	: "r" (usecs), "r" (lps));

	__delay(usecs * HZ);
}

extern __inline__ void __ndelay(unsigned long usecs, unsigned long lps)
{
	usecs *= 0x0000000000000005UL;		/* 2**32 / 10000 */

	__asm__ __volatile__(
"	mulx	%1, %2, %0\n"
"	srlx	%0, 32, %0\n"
	: "=r" (usecs)
	: "r" (usecs), "r" (lps));

	__delay(usecs * HZ);
}

#ifdef CONFIG_SMP
#define __udelay_val cpu_data[smp_processor_id()].udelay_val
#else
#define __udelay_val loops_per_jiffy
#endif

#define udelay(usecs) __udelay((usecs),__udelay_val)
#define ndelay(usecs) __ndelay((usecs),__udelay_val)

#endif /* !__ASSEMBLY__ */

#endif /* defined(__SPARC64_DELAY_H) */
