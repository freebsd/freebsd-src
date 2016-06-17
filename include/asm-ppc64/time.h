/*
 * Common time prototypes and such for all ppc machines.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) to merge
 * Paul Mackerras' version and mine for PReP and Pmac.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __PPC64_TIME_H
#define __PPC64_TIME_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/mc146818rtc.h>

#include <asm/processor.h>
#include <asm/paca.h>
#include <asm/iSeries/HvCall.h>

/* time.c */
extern unsigned long tb_ticks_per_jiffy;
extern unsigned long tb_ticks_per_usec;
extern unsigned long tb_ticks_per_sec;
extern unsigned long tb_to_xs;
extern unsigned long tb_last_stamp;

struct rtc_time;
extern void to_tm(int tim, struct rtc_time * tm);
extern time_t last_rtc_update;

struct div_result {
	unsigned long result_high;
	unsigned long result_low;
};

int via_calibrate_decr(void);

static __inline__ unsigned long get_tb(void)
{
	return mftb();
}

/* Accessor functions for the decrementer register. */
static __inline__ unsigned int get_dec(void)
{
	return (mfspr(SPRN_DEC));
}

static __inline__ void set_dec(int val)
{
#ifdef CONFIG_PPC_ISERIES
	struct paca_struct *lpaca = get_paca();
	int cur_dec;

	if (lpaca->xLpPaca.xSharedProc) {
		lpaca->xLpPaca.xVirtualDecr = val;
		cur_dec = get_dec();
		lpaca->xLpPaca.xSavedDecr = cur_dec;
		if (cur_dec > val)
			HvCall_setVirtualDecr();
	} else
#endif
		mtspr(SPRN_DEC, val);
}

static inline unsigned long tb_ticks_since(unsigned long tstamp)
{
	return get_tb() - tstamp;
}

#define mulhdu(x,y) \
({unsigned long z; asm ("mulhdu %0,%1,%2" : "=r" (z) : "r" (x), "r" (y)); z;})

void div128_by_32( unsigned long dividend_high, unsigned long dividend_low,
		   unsigned divisor, struct div_result *dr );
#endif /* __KERNEL__ */
#endif /* __PPC64_TIME_H */
