/*
 *  linux/include/asm-arm/arch-rs/time.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (c) 2002 Simtec Electronics / Ben Dooks
 *
 * Bits taken from linux/include/asm-arm/arch-rpc/time.h
 *
 * (c) 1996-2002 Russell King
 *
 * Bits taken from linux/include/asm-arm/arch-ebsa285/time.h
 *
 * Copyright (C) 1998 Russell King.
 * Copyright (C) 1998 Phil Blundell
*/

#define RTC_PORT(x)    (0x70 + (x))
#define RTC_ALWAYS_BCD (0)

#include <linux/mc146818rtc.h>
#include <asm/mach-types.h>

extern void ioctime_init(void);

/* timer interrut - update things like profiling information and our
 * copy of the RTC's time
*/

static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_timer(regs);
	do_set_rtc();
	do_profile(regs);
}

/* get_isa_cmos_time()
 *
 * get the time from the CMOS RTC
 *
 * from linux/include/asm-arm/arch-ebsa285/time.h
*/

static unsigned long get_isa_cmos_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int i;

	// check to see if the RTC makes sense.....
	if ((CMOS_READ(RTC_VALID) & RTC_VRT) == 0)
		return mktime(1970, 1, 1, 0, 0, 0);

	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way.
	 */
	/* read RTC exactly on falling edge of update flag */
	for (i = 0 ; i < 1000000 ; i++) /* may take up to 1 second... */
		if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
			break;

	for (i = 0 ; i < 1000000 ; i++) /* must try at least 2.228 ms */
		if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
			break;

	do { /* Isn't this overkill ? UIP above should guarantee consistency */
		sec  = CMOS_READ(RTC_SECONDS);
		min  = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day  = CMOS_READ(RTC_DAY_OF_MONTH);
		mon  = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
	} while (sec != CMOS_READ(RTC_SECONDS));

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BCD_TO_BIN(sec);
		BCD_TO_BIN(min);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(day);
		BCD_TO_BIN(mon);
		BCD_TO_BIN(year);
	}
	if ((year += 1900) < 1970)
		year += 100;
	return mktime(year, mon, day, hour, min, sec);
}

/* set_isa_cmos_time()
 *
 * set the CMOS RTC time
 *
 * from linux/include/asm-arm/arch-ebsa285/time.h
*/

static int
set_isa_cmos_time(void)
{
	int retval = 0;
	int real_seconds, real_minutes, cmos_minutes;
	unsigned char save_control, save_freq_select;
	unsigned long nowtime = xtime.tv_sec;

	save_control = CMOS_READ(RTC_CONTROL); /* tell the clock it's being set */
	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	save_freq_select = CMOS_READ(RTC_FREQ_SELECT); /* stop and reset prescaler */
	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

	cmos_minutes = CMOS_READ(RTC_MINUTES);
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
		BCD_TO_BIN(cmos_minutes);

	/*
	 * since we're only adjusting minutes and seconds,
	 * don't interfere with hour overflow. This avoids
	 * messing with unknown time zones but requires your
	 * RTC not to be off by more than 15 minutes
	 */
	real_seconds = nowtime % 60;
	real_minutes = nowtime / 60;
	if (((abs(real_minutes - cmos_minutes) + 15)/30) & 1)
		real_minutes += 30;		/* correct for half hour time zone */
	real_minutes %= 60;

	if (abs(real_minutes - cmos_minutes) < 30) {
		if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
			BIN_TO_BCD(real_seconds);
			BIN_TO_BCD(real_minutes);
		}
		CMOS_WRITE(real_seconds,RTC_SECONDS);
		CMOS_WRITE(real_minutes,RTC_MINUTES);
	} else
		retval = -1;

	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	CMOS_WRITE(save_control, RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);

	return retval;
}


/*
 * Set up timer interrupt.
 */
static inline void setup_timer(void)
{
	int reg_b, reg_d;

	ioctime_init();

	/* ensure we have an RTC chip, and initialise it... */

	reg_d = CMOS_READ(RTC_REG_D);

	CMOS_WRITE(RTC_REF_CLCK_32KHZ, RTC_REG_A);

	/*
	 * Set control reg B
	 *   (24 hour mode, update enabled)
	 */
	reg_b = CMOS_READ(RTC_REG_B) & 0x7f;
	reg_b |= 2;
	CMOS_WRITE(reg_b, RTC_REG_B);

	if ((CMOS_READ(RTC_REG_A) & 0x7f) == RTC_REF_CLCK_32KHZ &&
	    CMOS_READ(RTC_REG_B) == reg_b) {

		/*
		 * We have a RTC.  Check the battery
		 */
		if ((reg_d & 0x80) == 0)
			printk(KERN_WARNING "RTC: *** warning: CMOS battery bad\n");

		printk("RTC: detected\n");

		xtime.tv_sec = get_isa_cmos_time();
		set_rtc = set_isa_cmos_time;
	} else {
		printk("RTC: Warning: No RTC detected\n");
	}

	/* ensure we have the IOC time interrupt setup */

	timer_irq.handler = timer_interrupt;

	setup_arm_irq(IRQ_TIMER, &timer_irq);
}

