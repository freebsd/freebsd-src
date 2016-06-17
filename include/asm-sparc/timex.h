/*
 * linux/include/asm-sparc/timex.h
 *
 * sparc architecture timex specifications
 */
#ifndef _ASMsparc_TIMEX_H
#define _ASMsparc_TIMEX_H

#define CLOCK_TICK_RATE	1193180 /* Underlying HZ */
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

/* XXX Maybe do something better at some point... -DaveM */
typedef unsigned long cycles_t;
extern cycles_t cacheflush_time;
#define get_cycles()	(0)

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
