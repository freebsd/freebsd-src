#ifndef _ASM_IA64_TIMEX_H
#define _ASM_IA64_TIMEX_H

/*
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 * Copyright (C) 1998-2001 David Mosberger-Tang <davidm@hpl.hp.com>
 */
/*
 * 2001/01/18 davidm	Removed CLOCK_TICK_RATE.  It makes no sense on IA-64.
 *			Also removed cacheflush_time as it's entirely unused.
 */

typedef unsigned long cycles_t;

static inline cycles_t
get_cycles (void)
{
	cycles_t ret;

	__asm__ __volatile__ ("mov %0=ar.itc" : "=r"(ret));
	return ret;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif /* _ASM_IA64_TIMEX_H */
