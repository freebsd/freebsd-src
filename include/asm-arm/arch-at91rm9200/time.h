/*
 * linux/include/asm-arm/arch-at91rm9200/time.h
 *
 *  Copyright (C) 2003 SAN People
 *  Copyright (C) 2003 ATMEL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_ARCH_TIME_H
#define __ASM_ARCH_TIME_H

#include <asm/system.h>

extern unsigned long (*gettimeoffset)(void);

/*
 * Returns number of microseconds since last timer interrupt.  Note that interrupts
 * will have been disabled by do_gettimeofday()
 *  'LATCH' is hwclock ticks (see CLOCK_TICK_RATE in timex.h) per jiffy.
 *  'tick' is usecs per jiffy (linux/timex.h).
 */
static unsigned long at91rm9200_gettimeoffset(void)
{
	unsigned long elapsed;

	elapsed = (AT91_SYS->ST_CRTR - AT91_SYS->ST_RTAR) & AT91C_ST_ALMV;

	return (unsigned long)(elapsed * tick) / LATCH;
}

/*
 * IRQ handler for the timer.
 */
static void at91rm9200_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if (AT91_SYS->ST_SR & AT91C_ST_PITS) {	/* This is a shared interrupt */
		do {
			do_timer(regs);

			AT91_SYS->ST_RTAR = (AT91_SYS->ST_RTAR + LATCH) & AT91C_ST_ALMV;

		} while (((AT91_SYS->ST_CRTR - AT91_SYS->ST_RTAR) & AT91C_ST_ALMV) >= LATCH);

		do_profile(regs);
	}
}

/*
 * Set up timer interrupt.
 */
static inline void setup_timer(void)
{
	/* Disable all timer interrupts */
	AT91_SYS->ST_IDR = AT91C_ST_PITS | AT91C_ST_WDOVF | AT91C_ST_RTTINC | AT91C_ST_ALMS;
	(void) AT91_SYS->ST_SR;		/* Clear any pending interrupts */

	/*
	 * Make IRQs happen for the system timer.
	 */
	timer_irq.handler = at91rm9200_timer_interrupt;
	timer_irq.flags = SA_SHIRQ | SA_INTERRUPT;
	setup_arm_irq(AT91C_ID_SYS, &timer_irq);
	gettimeoffset = at91rm9200_gettimeoffset;

	/* Set initial alarm to 0 */
	AT91_SYS->ST_RTAR = 0;

	/* Real time counter incremented every 30.51758 microseconds */
	AT91_SYS->ST_RTMR = 1;

	/* Set Period Interval timer */
	AT91_SYS->ST_PIMR = LATCH;

	/* Change the kernel's 'tick' value to 10009 usec. (the default is 10000) */
	tick = (LATCH * 1000000) / CLOCK_TICK_RATE;

	/* Enable Period Interval Timer interrupt */
	AT91_SYS->ST_IER = AT91C_ST_PITS;
}

#endif
