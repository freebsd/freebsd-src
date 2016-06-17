#ifndef _ASM_IA64_DELAY_H
#define _ASM_IA64_DELAY_H

/*
 * Delay routines using a pre-computed "cycles/usec" value.
 *
 * Copyright (C) 1998, 1999 Hewlett-Packard Co
 * Copyright (C) 1998, 1999 David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 1999 Don Dugger <don.dugger@intel.com>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include <asm/processor.h>

static __inline__ void
ia64_set_itm (unsigned long val)
{
	__asm__ __volatile__("mov cr.itm=%0;; srlz.d;;" :: "r"(val) : "memory");
}

static __inline__ unsigned long
ia64_get_itm (void)
{
	unsigned long result;

	__asm__ __volatile__("mov %0=cr.itm;; srlz.d;;" : "=r"(result) :: "memory");
	return result;
}

static __inline__ void
ia64_set_itv (unsigned long val)
{
	__asm__ __volatile__("mov cr.itv=%0;; srlz.d;;" :: "r"(val) : "memory");
}

static __inline__ void
ia64_set_itc (unsigned long val)
{
	__asm__ __volatile__("mov ar.itc=%0;; srlz.d;;" :: "r"(val) : "memory");
}

static __inline__ unsigned long
ia64_get_itc (void)
{
	unsigned long result;

	__asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory");
#ifdef CONFIG_ITANIUM
	while (__builtin_expect ((__s32) result == -1, 0))
		__asm__ __volatile__("mov %0=ar.itc" : "=r"(result) :: "memory");
#endif
	return result;
}

static __inline__ void
__delay (unsigned long loops)
{
        unsigned long saved_ar_lc;

	if (loops < 1)
		return;

	__asm__ __volatile__("mov %0=ar.lc;;" : "=r"(saved_ar_lc));
	__asm__ __volatile__("mov ar.lc=%0;;" :: "r"(loops - 1));
        __asm__ __volatile__("1:\tbr.cloop.sptk.few 1b;;");
	__asm__ __volatile__("mov ar.lc=%0" :: "r"(saved_ar_lc));
}

static __inline__ void
udelay (unsigned long usecs)
{
	unsigned long start = ia64_get_itc();
	unsigned long cycles = usecs*local_cpu_data->cyc_per_usec;

	while (ia64_get_itc() - start < cycles)
		/* skip */;
}

static __inline__ void
ndelay (unsigned long nsecs)
{
	unsigned long start = ia64_get_itc();
	unsigned long cycles = nsecs*local_cpu_data->cyc_per_usec/1000;

	while (ia64_get_itc() - start < cycles)
		/* skip */;
}

#endif /* _ASM_IA64_DELAY_H */
