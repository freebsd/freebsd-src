/*
 * linux/include/asm-sh/timex.h
 *
 * sh architecture timex specifications
 */
#ifndef __ASM_SH_TIMEX_H
#define __ASM_SH_TIMEX_H

#define CLOCK_TICK_RATE	(current_cpu_data.module_clock/4) /* Underlying HZ */
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

typedef unsigned long long cycles_t;

extern cycles_t cacheflush_time;

static __inline__ cycles_t get_cycles (void)
{
	return 0;
}

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif /* __ASM_SH_TIMEX_H */
