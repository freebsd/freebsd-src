/*
 *  linux/include/asm-arm/delay.h
 *
 *  Copyright (C) 1995-2003 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Delay routines, using a pre-computed "loops_per_second" value.
 */
#ifndef __ASM_ARM_DELAY_H
#define __ASM_ARM_DELAY_H

/*
 * Division by multiplication and shifts.
 *
 *  We want the number of loops to complete, where loops is:
 *      (us * (HZ * loops_per_jiffy)) / 10^6
 *  or
 *      (ns * (HZ * loops_per_jiffy)) / 10^9
 *
 *  Since we don't want to do long division, we multiply both numerator
 *  and denominator by (2^28 / 10^6):
 *
 *      (us * (2^28 / 10^6) * HZ * loops_per_jiffy) / 2^28
 *
 *  =>  (us * (2^28 * HZ / 10^6) * loops_per_jiffy) / 2^28
 *
 *  ~>  (((us * (2^28 * HZ / 10^6)) / 2^11) * (loops_per_jiffy / 2^12)) / 2^5
 *         (for large loops_per_jiffy >> 2^12)
 *
 *  Note: maximum loops_per_jiffy = 67108863 (bogomips = 1342.18)
 *        minimum loops_per_jiffy = 20000 (bogomips = 0.4)
 *
 * Note: we rely on HZ = 100.
 */
#define UDELAY_FACTOR	26843
#define NDELAY_FACTOR	27

#ifndef __ASSEMBLY__

extern void __bad_udelay(void);	/* intentional errors */
extern void __bad_ndelay(void);	/* intentional errors */

extern void __delay(unsigned long loops);
extern void __udelay(unsigned long usecs);
extern void __ndelay(unsigned long nsecs);
extern void __const_delay(unsigned long units);

#define udelay(n) 							\
	(__builtin_constant_p(n) ?					\
		((n) > 20000 ? __bad_udelay()				\
			     : __const_delay((n) * UDELAY_FACTOR))	\
				 : __udelay(n))

#define ndelay(n) 							\
	(__builtin_constant_p(n) ?					\
		((n) > 20000 ? __bad_ndelay()				\
			     : __const_delay((n) * NDELAY_FACTOR))	\
				 : __ndelay(n))

#endif

#endif /* defined(_ARM_DELAY_H) */

