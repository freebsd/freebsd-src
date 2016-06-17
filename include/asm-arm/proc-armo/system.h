/*
 *  linux/include/asm-arm/proc-armo/system.h
 *
 *  Copyright (C) 1995, 1996 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PROC_SYSTEM_H
#define __ASM_PROC_SYSTEM_H

#include <asm/proc-fns.h>

#define vectors_base()	(0)

static inline unsigned long __xchg(unsigned long x, volatile void *ptr, int size)
{
	extern void __bad_xchg(volatile void *, int);

	switch (size) {
		case 1:	return cpu_xchg_1(x, ptr);
		case 4:	return cpu_xchg_4(x, ptr);
		default: __bad_xchg(ptr, size);
	}
	return 0;
}

/*
 * We need to turn the caches off before calling the reset vector - RiscOS
 * messes up if we don't
 */
#define proc_hard_reset()	cpu_proc_fin()

/*
 * A couple of speedups for the ARM
 */

/*
 * Save the current interrupt enable state & disable IRQs
 */
#define local_save_flags_cli(x)				\
	do {						\
	  unsigned long temp;				\
	  __asm__ __volatile__(				\
"	mov	%0, pc		@ save_flags_cli\n"	\
"	orr	%1, %0, #0x08000000\n"			\
"	and	%0, %0, #0x0c000000\n"			\
"	teqp	%1, #0\n"				\
	  : "=r" (x), "=r" (temp)			\
	  :						\
	  : "memory");					\
	} while (0)
	
/*
 * Enable IRQs
 */
#define local_irq_enable()			\
	do {					\
	  unsigned long temp;			\
	  __asm__ __volatile__(			\
"	mov	%0, pc		@ sti\n"	\
"	bic	%0, %0, #0x08000000\n"		\
"	teqp	%0, #0\n"			\
	  : "=r" (temp)				\
	  :					\
	  : "memory");				\
	} while(0)

/*
 * Disable IRQs
 */
#define local_irq_disable()			\
	do {					\
	  unsigned long temp;			\
	  __asm__ __volatile__(			\
"	mov	%0, pc		@ cli\n"	\
"	orr	%0, %0, #0x08000000\n"		\
"	teqp	%0, #0\n"			\
	  : "=r" (temp)				\
	  :					\
	  : "memory");				\
	} while(0)

#define __clf()	do {				\
	unsigned long temp;			\
	__asm__ __volatile__(			\
"	mov	%0, pc		@ clf\n"	\
"	orr	%0, %0, #0x04000000\n"		\
"	teqp	%0, #0\n"			\
	: "=r" (temp));				\
    } while(0)

#define __stf()	do {				\
	unsigned long temp;			\
	__asm__ __volatile__(			\
"	mov	%0, pc		@ stf\n"	\
"	bic	%0, %0, #0x04000000\n"		\
"	teqp	%0, #0\n"			\
	: "=r" (temp));				\
    } while(0)

/*
 * save current IRQ & FIQ state
 */
#define local_save_flags(x)			\
	do {					\
	  __asm__ __volatile__(			\
"	mov	%0, pc		@ save_flags\n"	\
"	and	%0, %0, #0x0c000000\n"		\
	  : "=r" (x));				\
	} while (0)

/*
 * restore saved IRQ & FIQ state
 */
#define local_irq_restore(x)				\
	do {						\
	  unsigned long temp;				\
	  __asm__ __volatile__(				\
"	mov	%0, pc		@ restore_flags\n"	\
"	bic	%0, %0, #0x0c000000\n"			\
"	orr	%0, %0, %1\n"				\
"	teqp	%0, #0\n"				\
	  : "=&r" (temp)				\
	  : "r" (x)					\
	  : "memory");					\
	} while (0)

#define __save_and_sti(x)	({__save_flags(x);__sti();})

#endif
