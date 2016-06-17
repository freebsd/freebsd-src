/*
 *  linux/include/asm-arm/arch-ebsa285/time.h
 *
 *  Copyright (C) 1998 Russell King.
 *  Copyright (C) 1998 Phil Blundell
 *
 * CATS has a real-time clock, though the evaluation board doesn't.
 *
 * Changelog:
 *  21-Mar-1998	RMK	Created
 *  27-Aug-1998	PJB	CATS support
 *  28-Dec-1998	APH	Made leds optional
 *  20-Jan-1999	RMK	Started merge of EBSA285, CATS and NetWinder
 *  16-Mar-1999	RMK	More support for EBSA285-like machines with RTCs in
 */

#define RTC_PORT(x)		(rtc_base+(x))
#define RTC_ALWAYS_BCD		0

#include <linux/mc146818rtc.h>

#include <asm/hardware/dec21285.h>
#include <asm/leds.h>
#include <asm/mach-types.h>

static int rtc_base;

#define mSEC_10_from_14 ((14318180 + 100) / 200)

static unsigned long isa_gettimeoffset(void)
{
	int count;

	static int count_p = (mSEC_10_from_14/6);    /* for the first call after boot */
	static unsigned long jiffies_p = 0;

	/*
	 * cache volatile jiffies temporarily; we have IRQs turned off. 
	 */
	unsigned long jiffies_t;

	/* timer count may underflow right here */
	outb_p(0x00, 0x43);	/* latch the count ASAP */

	count = inb_p(0x40);	/* read the latched count */

	/*
	 * We do this guaranteed double memory access instead of a _p 
	 * postfix in the previous port access. Wheee, hackady hack
	 */
 	jiffies_t = jiffies;

	count |= inb_p(0x40) << 8;

	/* Detect timer underflows.  If we haven't had a timer tick since 
	   the last time we were called, and time is apparently going
	   backwards, the counter must have wrapped during this routine. */
	if ((jiffies_t == jiffies_p) && (count > count_p))
		count -= (mSEC_10_from_14/6);
	else
		jiffies_p = jiffies_t;

	count_p = count;

	count = (((mSEC_10_from_14/6)-1) - count) * tick;
	count = (count + (mSEC_10_from_14/6)/2) / (mSEC_10_from_14/6);

	return count;
}

static void isa_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if (machine_is_netwinder())
		do_leds();

	do_timer(regs);
	do_set_rtc();
	do_profile(regs);
}

static unsigned long __init get_isa_cmos_time(void)
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



static unsigned long timer1_gettimeoffset (void)
{
	unsigned long value = LATCH - *CSR_TIMER1_VALUE;

	return (tick * value) / LATCH;
}

static void timer1_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	*CSR_TIMER1_CLR = 0;

	/* Do the LEDs things */
	do_leds();
	do_timer(regs);
	do_set_rtc();
	do_profile(regs);
}

/*
 * Set up timer interrupt.
 */
static inline void setup_timer(void)
{
	int irq;

	if (machine_is_co285() ||
	    machine_is_personal_server())
		/*
		 * Add-in 21285s shouldn't access the RTC
		 */
		rtc_base = 0;
	else
		rtc_base = 0x70;

	if (rtc_base) {
		int reg_d, reg_b;

		/*
		 * Probe for the RTC.
		 */
		reg_d = CMOS_READ(RTC_REG_D);

		/*
		 * make sure the divider is set
		 */
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

			xtime.tv_sec = get_isa_cmos_time();
			set_rtc = set_isa_cmos_time;
		} else
			rtc_base = 0;
	}

	if (machine_is_ebsa285() ||
	    machine_is_co285() ||
	    machine_is_personal_server()) {
		gettimeoffset = timer1_gettimeoffset;

		*CSR_TIMER1_CLR  = 0;
		*CSR_TIMER1_LOAD = LATCH;
		*CSR_TIMER1_CNTL = TIMER_CNTL_ENABLE | TIMER_CNTL_AUTORELOAD | TIMER_CNTL_DIV16;

		timer_irq.handler = timer1_interrupt;
		irq = IRQ_TIMER1;
	} else {
		/* enable PIT timer */
		/* set for periodic (4) and LSB/MSB write (0x30) */
		outb(0x34, 0x43);
		outb((mSEC_10_from_14/6) & 0xFF, 0x40);
		outb((mSEC_10_from_14/6) >> 8, 0x40);

		gettimeoffset = isa_gettimeoffset;
		timer_irq.handler = isa_timer_interrupt;
		irq = IRQ_ISA_TIMER;
	}
	setup_arm_irq(irq, &timer_irq);
}
