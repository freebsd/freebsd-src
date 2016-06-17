/*
 * linux/include/asm-parisc/timex.h
 *
 * PARISC architecture timex specifications
 */
#ifndef _ASMPARISC_TIMEX_H
#define _ASMPARISC_TIMEX_H

#include <asm/system.h>
#include <linux/time.h>

typedef unsigned long cycles_t;

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles (void)
{
	return mfctl(16);
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
