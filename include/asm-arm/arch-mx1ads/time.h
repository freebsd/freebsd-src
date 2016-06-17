/*
 *  linux/include/asm-arm/arch-mx1ads/time.h
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
 *
 * Copyright (C) 2002 Shane Nay (shane@minirl.com)
 */
#include <asm/system.h>

/*
 * Where is the timer (VA)?
 */
#define TIMER0_VA_BASE (IO_ADDRESS(MX1ADS_TIM1_BASE)+0x00000000)
#define TIMER1_VA_BASE (IO_ADDRESS(MX1ADS_TIM2_BASE)+0x00000000)

/*
 * How long is the timer interval?
 *
 * Note-
 * Clocking is not accurate enough.  Need to change the input
 * to CLKOUT, and fix what those values are.  However,
 * first need to evaluate what a reasonable value is
 * as several other things depend upon that clock.
 *
 */

#define TIMER_RELOAD	(328)

#define TICKS2USECS(x)	((x) * 30)

#define TIM_32KHZ       0x08
#define TIM_INTEN       0x10
#define TIM_ENAB        0x01

/*
 * What does it look like?
 */
typedef struct TimerStruct {
	unsigned long TimerControl;
	unsigned long TimerPrescaler;
	unsigned long TimerCompare;
	unsigned long TimerCapture;
	unsigned long TimerCounter;
	unsigned long TimerClear;	/* Clear Status */
} TimerStruct_t;

extern unsigned long (*gettimeoffset) (void);

/*
 * Returns number of ms since last clock interrupt.  Note that interrupts
 * will have been disabled by do_gettimeoffset()
 */
static unsigned long
mx1ads_gettimeoffset(void)
{
	volatile TimerStruct_t *timer1 = (TimerStruct_t *) TIMER1_VA_BASE;
	unsigned long ticks, status;

	/*
	 * Get the current number of ticks.  Note that there is a race
	 * condition between us reading the timer and checking for
	 * an interrupt.  We get around this by ensuring that the
	 * counter has not reloaded between our two reads.
	 */
	ticks = timer1->TimerCounter;

	/*
	 * Interrupt pending?  If so, we've reloaded once already.
	 */
	if (timer1->TimerClear & 1)
		ticks += TIMER_RELOAD;

	/*
	 * Convert the ticks to usecs
	 */
	return TICKS2USECS(ticks);
}

/*
 * IRQ handler for the timer
 */
static void
mx1ads_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile TimerStruct_t *timer1 =
	    (volatile TimerStruct_t *) TIMER1_VA_BASE;
	// ...clear the interrupt
	if (timer1->TimerClear) {
		timer1->TimerClear = 0x0;
	}

	do_timer(regs);
	do_profile(regs);
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
static inline void
setup_timer(void)
{
	volatile TimerStruct_t *timer0 =
	    (volatile TimerStruct_t *) TIMER0_VA_BASE;
	volatile TimerStruct_t *timer1 =
	    (volatile TimerStruct_t *) TIMER1_VA_BASE;


	timer_irq.handler = mx1ads_timer_interrupt;

	/*
	 * Initialise to a known state (all timers off, and timing reset)
	 */
	timer0->TimerControl = 0;
	timer1->TimerControl = 0;
	timer0->TimerPrescaler = 0;
	timer1->TimerPrescaler = 0;

	timer1->TimerCompare = 328;
	timer1->TimerControl = (TIM_32KHZ | TIM_INTEN | TIM_ENAB);

	/*
	 * Make irqs happen for the system timer
	 */
	setup_arm_irq(TIM2_INT, &timer_irq);
	gettimeoffset = mx1ads_gettimeoffset;
}
