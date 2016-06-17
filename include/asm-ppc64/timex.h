/*
 * linux/include/asm-ppc/timex.h
 *
 * PPC64 architecture timex specifications
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASMPPC64_TIMEX_H
#define _ASMPPC64_TIMEX_H

#define CLOCK_TICK_RATE	1193180 /* Underlying HZ */
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

typedef unsigned long cycles_t;
extern cycles_t cacheflush_time;

static inline cycles_t get_cycles(void)
{
	cycles_t ret;

	__asm__ __volatile__("mftb %0" : "=r" (ret) : );
	return ret;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
