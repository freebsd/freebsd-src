/*
 * linux/include/asm-x8664/timex.h
 *
 * x8664 architecture timex specifications
 */
#ifndef _ASMx8664_TIMEX_H
#define _ASMx8664_TIMEX_H

#include <linux/config.h>
#include <asm/msr.h>
#include <asm/vsyscall.h>

#define CLOCK_TICK_RATE (vxtime_hz)
#define FINETUNE	((((((long)LATCH * HZ - CLOCK_TICK_RATE) << SHIFT_HZ) * \
			1000000 / CLOCK_TICK_RATE) << (SHIFT_SCALE - SHIFT_HZ)) / HZ)

/*
 * We only use the low 32 bits, and we'd simply better make sure
 * that we reschedule before that wraps. Scheduling at least every
 * four billion cycles just basically sounds like a good idea,
 * regardless of how fast the machine is.
 */
typedef unsigned long long cycles_t;

extern cycles_t cacheflush_time;

static inline cycles_t get_cycles (void)
{
	unsigned long long ret;
	rdtscll(ret);
	return ret;
}

extern unsigned int cpu_khz;

/*
 * Documentation on HPET can be found at:
 *      http://www.intel.com/ial/home/sp/pcmmspec.htm
 *      ftp://download.intel.com/ial/home/sp/mmts098.pdf
 */

#define HPET_ID		0x000
#define HPET_PERIOD	0x004
#define HPET_CFG	0x010
#define HPET_STATUS	0x020
#define HPET_COUNTER	0x0f0
#define HPET_T0_CFG	0x100
#define HPET_T0_CMP	0x108
#define HPET_T0_ROUTE	0x110

#define HPET_ID_VENDOR	0xffff0000
#define HPET_ID_LEGSUP	0x00008000
#define HPET_ID_NUMBER	0x00000f00
#define HPET_ID_REV	0x000000ff

#define HPET_CFG_ENABLE	0x001
#define HPET_CFG_LEGACY	0x002

#define HPET_T0_ENABLE		0x004
#define HPET_T0_PERIODIC	0x008
#define HPET_T0_SETVAL		0x040
#define HPET_T0_32BIT		0x100

extern struct vxtime_data vxtime;
extern unsigned long vxtime_hz;
extern unsigned long hpet_address;

#endif
