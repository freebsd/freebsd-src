/*
 *  linux/arch/arm/mach-ebsa110/time.c
 *
 *  Copyright (C) 2001 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/io.h>

#define PIT_CTRL		(PIT_BASE + 0x0d)
#define PIT_T2			(PIT_BASE + 0x09)
#define PIT_T1			(PIT_BASE + 0x05)
#define PIT_T0			(PIT_BASE + 0x01)

/*
 * This is the rate at which your MCLK signal toggles (in Hz)
 * This was measured on a 10 digit frequency counter sampling
 * over 1 second.
 */
#define MCLK	47894000

/*
 * This is the rate at which the PIT timers get clocked
 */
#define CLKBY7	(MCLK / 7)

/*
 * If CLKBY7 is larger than this, then we must do software
 * division of the timer interrupt.
 */
#if CLKBY7 > 6553500
#define DIVISOR	2
#else
#define DIVISOR 1
#endif

/*
 * This is the counter value
 */
#define COUNT	((CLKBY7 + (DIVISOR * HZ / 2)) / (DIVISOR * HZ))

extern unsigned long (*gettimeoffset)(void);

static unsigned long divisor;

/*
 * Get the time offset from the system PIT.  Note that if we have missed an
 * interrupt, then the PIT counter will roll over (ie, be negative).
 * This actually works out to be convenient.
 */
static unsigned long ebsa110_gettimeoffset(void)
{
	unsigned long offset, count;

	__raw_writeb(0x40, PIT_CTRL);
	count = __raw_readb(PIT_T1);
	count |= __raw_readb(PIT_T1) << 8;

	/*
	 * If count > COUNT, make the number negative.
	 */
	if (count > COUNT)
		count |= 0xffff0000;

	offset = COUNT * (DIVISOR - divisor);
	offset -= count;

	/*
	 * `offset' is in units of timer counts.  Convert
	 * offset to units of microseconds.
	 */
	offset = offset * (1000000 / HZ) / (COUNT * DIVISOR);

	return offset;
}

int ebsa110_reset_timer(void)
{
	u32 count;

	/* latch and read timer 1 */
	__raw_writeb(0x40, PIT_CTRL);
	count = __raw_readb(PIT_T1);
	count |= __raw_readb(PIT_T1) << 8;

	count += COUNT;

	__raw_writeb(count & 0xff, PIT_T1);
	__raw_writeb(count >> 8, PIT_T1);

	if (divisor == 0)
		divisor = DIVISOR;
	divisor -= 1;
	return divisor;
}

void __init ebsa110_setup_timer(void)
{
	/*
	 * Timer 1, mode 2, LSB/MSB
	 */
	__raw_writeb(0x70, PIT_CTRL);
	__raw_writeb(COUNT & 0xff, PIT_T1);
	__raw_writeb(COUNT >> 8, PIT_T1);
	divisor = DIVISOR - 1;

	gettimeoffset = ebsa110_gettimeoffset;
}
