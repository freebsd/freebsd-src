/*
 * linux/arch/ia64/sn/kernel/sn2/timer.c
 *
 * Copyright (C) 2003 Silicon Graphics, Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/interrupt.h>

#include <asm/hw_irq.h>
#include <asm/system.h>

#include <asm/sn/leds.h>
#include <asm/sn/clksupport.h>


extern unsigned long sn_rtc_cycles_per_second;
static volatile unsigned long last_wall_rtc;

/**
 * gettimeoffset - number of usecs elapsed since &xtime was last updated
 *
 * This function is used by do_gettimeofday() to determine the number
 * of usecs that have elapsed since the last update to &xtime.  On SN
 * this is accomplished using the RTC built in to each Hub chip; each
 * is guaranteed to be synchronized by the PROM, so a local read will
 * suffice (GET_RTC_COUNTER() does this for us).  A snapshot of the RTC
 * value is taken every time wall_jiffies is updated by the
 * update_wall_time_hook (sn2_update_wall_time) which means we don't
 * have to adjust for lost jiffies ticks or anything like that.
 */
extern unsigned long wall_jiffies; /* from kernel/timer.c */

static volatile long rtc_offset __cacheline_aligned;
static long rtc_cycles_per_usec;
static long rtc_per_timer_tick;

unsigned long
sn_gettimeoffset(void)
{
	long current_rtc, elapsed_rtc, old, new_offset;

	do {
		old = rtc_offset;
		current_rtc = GET_RTC_COUNTER();

		/*
		 * Need to address wrapping here!
		 */
		elapsed_rtc = (long)(current_rtc - last_wall_rtc);

		/*
		 * This case is non lethal as the max() will take care of it.
		 */
#if 0
		if (elapsed_rtc < 0) {
			printk(KERN_ERR "sn_gettimeoffset(): time goes "
			       "backwards!\n    current_rtc 0x%016lx, "
			       "last_wall_rtc 0x%016lx, elapsed %08lx, "
			       "offset %li\n", current_rtc, last_wall_rtc,
			       elapsed_rtc, max(elapsed_rtc, rtc_offset)/
			       rtc_cycles_per_usec);
		}
#endif

		new_offset = max(elapsed_rtc, old);
	} while (cmpxchg(&rtc_offset, old, new_offset) != old);

	return new_offset / rtc_cycles_per_usec;
}


void sn2_update_wall_time(void)
{
	rtc_offset -= min(rtc_offset, rtc_per_timer_tick);
	last_wall_rtc = GET_RTC_COUNTER();
}


void sn2_reset_wall_time(void)
{
	rtc_offset = 0;
	last_wall_rtc = GET_RTC_COUNTER();
}

void __init
sn_timer_init(void)
{
	rtc_per_timer_tick = sn_rtc_cycles_per_second / HZ;
	rtc_cycles_per_usec = sn_rtc_cycles_per_second / 1000000;

	last_wall_rtc = GET_RTC_COUNTER();
	update_wall_time_hook = sn2_update_wall_time;
	reset_wall_time_hook = sn2_reset_wall_time;
	gettimeoffset = sn_gettimeoffset;
}
