/*
 * linux/include/asm-arm/arch-sa1100/time.h
 *
 * Copyright (C) 1998 Deborah Wallach.
 * Twiddles  (C) 1999 	Hugo Fiennes <hugo@empeg.com>
 * 
 * 2000/03/29 (C) Nicolas Pitre <nico@cam.org>
 *	Rewritten: big cleanup, much simpler, better HZ accuracy.
 *
 */


#define RTC_DEF_DIVIDER		(32768 - 1)
#define RTC_DEF_TRIM            0

static unsigned long __init sa1100_get_rtc_time(void)
{
	/*
	 * According to the manual we should be able to let RTTR be zero
	 * and then a default diviser for a 32.768KHz clock is used.
	 * Apparently this doesn't work, at least for my SA1110 rev 5.
	 * If the clock divider is uninitialized then reset it to the
	 * default value to get the 1Hz clock.
	 */
	if (RTTR == 0) {
		RTTR = RTC_DEF_DIVIDER + (RTC_DEF_TRIM << 16);
		printk(KERN_WARNING "Warning: uninitialized Real Time Clock\n");
		/* The current RTC value probably doesn't make sense either */
		RCNR = 0;
		return 0;
	}
	return RCNR;
}

static int sa1100_set_rtc(void)
{
	unsigned long current_time = xtime.tv_sec;

	if (RTSR & RTSR_ALE) {
		/* make sure not to forward the clock over an alarm */
		unsigned long alarm = RTAR;
		if (current_time >= alarm && alarm >= RCNR)
			return -ERESTARTSYS;
	}
	RCNR = current_time;
	return 0;
}

/* IRQs are disabled before entering here from do_gettimeofday() */
static unsigned long sa1100_gettimeoffset (void)
{
	unsigned long ticks_to_match, elapsed, usec;

	/* Get ticks before next timer match */
	ticks_to_match = OSMR0 - OSCR;

	/* We need elapsed ticks since last match */
	elapsed = LATCH - ticks_to_match;

	/* Now convert them to usec */
	usec = (unsigned long)(elapsed*tick)/LATCH;

	return usec;
}

static void sa1100_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int next_match;
	unsigned long flags;

	do {
		do_leds();
		local_irq_save(flags);
		do_timer(regs);
		OSSR = OSSR_M0;  /* Clear match on timer 0 */
		next_match = (OSMR0 += LATCH);
		local_irq_restore(flags);
		do_set_rtc();
	} while ((signed long)(next_match - OSCR) <= 0);

	do_profile(regs);
}

static inline void setup_timer (void)
{
	gettimeoffset = sa1100_gettimeoffset;
	set_rtc = sa1100_set_rtc;
	xtime.tv_sec = sa1100_get_rtc_time();
	timer_irq.handler = sa1100_timer_interrupt;
	OSMR0 = 0;		/* set initial match at 0 */
	OSSR = 0xf;		/* clear status on all timers */
	setup_arm_irq(IRQ_OST0, &timer_irq);
	OIER |= OIER_E0;	/* enable match on timer 0 to cause interrupts */
	OSCR = 0;		/* initialize free-running timer, force first match */
}

