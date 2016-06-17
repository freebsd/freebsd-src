/*
 * linux/include/asm-sparc64/timex.h
 *
 * sparc64 architecture timex specifications
 */
#ifndef _ASMsparc64_TIMEX_H
#define _ASMsparc64_TIMEX_H

#include <asm/timer.h>

#define CLOCK_TICK_RATE	1193180 /* Underlying HZ */
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

/* Getting on the cycle counter on sparc64. */
typedef unsigned long cycles_t;
#define get_cycles()	tick_ops->get_tick()

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
