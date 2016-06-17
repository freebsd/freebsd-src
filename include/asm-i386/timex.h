/*
 * linux/include/asm-i386/timex.h
 *
 * i386 architecture timex specifications
 */
#ifndef _ASMi386_TIMEX_H
#define _ASMi386_TIMEX_H

#include <linux/config.h>
#include <asm/msr.h>

#ifdef CONFIG_MELAN
#  define CLOCK_TICK_RATE 1189200 /* AMD Elan has different frequency! */
#else
#  define CLOCK_TICK_RATE 1193180 /* Underlying HZ */
#endif

#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

/*
 * Standard way to access the cycle counter on i586+ CPUs.
 * Currently only used on SMP.
 *
 * If you really have a SMP machine with i486 chips or older,
 * compile for that, and this will just always return zero.
 * That's ok, it just means that the nicer scheduling heuristics
 * won't work for you.
 *
 * We only use the low 32 bits, and we'd simply better make sure
 * that we reschedule before that wraps. Scheduling at least every
 * four billion cycles just basically sounds like a good idea,
 * regardless of how fast the machine is. 
 */
typedef unsigned long long cycles_t;

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles (void)
{
#ifndef CONFIG_X86_TSC
	return 0;
#else
	unsigned long long ret;

	rdtscll(ret);
	return ret;
#endif
}

extern unsigned long cpu_khz;

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif
