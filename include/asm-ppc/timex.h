/*
 * linux/include/asm-ppc/timex.h
 *
 * ppc architecture timex specifications
 */
#ifdef __KERNEL__
#ifndef _ASMppc_TIMEX_H
#define _ASMppc_TIMEX_H

#include <linux/config.h>
#include <asm/cputable.h>

#define CLOCK_TICK_RATE	1193180 /* Underlying HZ */
#define CLOCK_TICK_FACTOR	20	/* Factor of both 1000000 and CLOCK_TICK_RATE */
#define FINETUNE ((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
	(1000000/CLOCK_TICK_FACTOR) / (CLOCK_TICK_RATE/CLOCK_TICK_FACTOR)) \
		<< (SHIFT_SCALE-SHIFT_HZ)) / HZ)

typedef unsigned long cycles_t;

/*
 * For the "cycle" counter we use the timebase lower half.
 * Currently only used on SMP.
 */

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles(void)
{
	cycles_t ret = 0;

	__asm__ __volatile__(
		"98:	mftb %0\n"
		"99:\n"
		".section __ftr_fixup,\"a\"\n"
		"	.long %1\n"
		"	.long 0\n"
		"	.long 98b\n"
		"	.long 99b\n"
		".previous"
		: "=r" (ret) : "i" (CPU_FTR_601));
	return ret;
}

#endif

#define vxtime_lock()		do {} while (0)
#define vxtime_unlock()		do {} while (0)

#endif /* __KERNEL__ */
