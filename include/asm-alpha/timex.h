/*
 * linux/include/asm-alpha/timex.h
 *
 * ALPHA architecture timex specifications
 */
#ifndef _ASMALPHA_TIMEX_H
#define _ASMALPHA_TIMEX_H

#define CLOCK_TICK_RATE	1193180 /* Underlying HZ */

/*
 * Standard way to access the cycle counter.
 * Currently only used on SMP for scheduling.
 *
 * Only the low 32 bits are available as a continuously counting entity. 
 * But this only means we'll force a reschedule every 8 seconds or so,
 * which isn't an evil thing.
 */

typedef unsigned int cycles_t;

static inline cycles_t get_cycles (void)
{
	cycles_t ret;
	__asm__ __volatile__ ("rpcc %0" : "=r"(ret));
	return ret;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
