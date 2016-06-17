/*
 * linux/include/asm-arm/arch-shark/time.h
 *
 * by Alexander Schulz
 *
 * Uses the real time clock because you can't run
 * the timer with level triggered interrupts and
 * you can't run the shark with edge triggered
 * inetrrupts (loses ints and hangs).
 *
 * derived from linux/drivers/char/rtc.c and:
 * linux/include/asm-arm/arch-ebsa110/time.h
 * Copyright (c) 1996,1997,1998 Russell King.
 */

#include <asm/leds.h>
#include <linux/mc146818rtc.h>

#define IRQ_TIMER 8

extern void get_rtc_time(struct rtc_time *rtc_tm);
extern void set_rtc_irq_bit(unsigned char bit);
extern unsigned long epoch;

static void timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{

	CMOS_READ(RTC_INTR_FLAGS);	

	do_leds();

	{
#ifdef DIVISOR
		static unsigned int divisor;

		if (divisor-- == 0) {
			divisor = DIVISOR - 1;
#else
		{
#endif
			do_timer(regs);
		}
	}
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static inline void setup_timer(void)
{
        struct rtc_time r_time;
        unsigned long flags;
	int tmp = 0;
	unsigned char val;

        /*
	 * Set the clock to 128 Hz, we already have a valid
	 * vector now:
	 */

	while (HZ > (1<<tmp))
	  tmp++;

	/*
	 * Check that the input was really a power of 2.
	 */
	if (HZ != (1<<tmp))
	  panic("Please set HZ to a power of 2!");

	save_flags(flags);
	cli();
	val = CMOS_READ(RTC_FREQ_SELECT) & 0xf0;
	val |= (16 - tmp);
	CMOS_WRITE(val, RTC_FREQ_SELECT);
	restore_flags(flags);
	set_rtc_irq_bit(RTC_PIE);

	get_rtc_time(&r_time);
	xtime.tv_sec = mktime(r_time.tm_year+epoch, r_time.tm_mon+1, r_time.tm_mday,
			      r_time.tm_hour, r_time.tm_min, r_time.tm_sec);

	timer_irq.handler = timer_interrupt;
	setup_arm_irq(IRQ_TIMER, &timer_irq);
}
