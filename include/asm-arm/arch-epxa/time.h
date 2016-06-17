/*
 *  linux/include/asm-arm/arch-epxa10db/time.h
 *
 *  Copyright (C) 2001 Altera Corporation
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
#include <linux/config.h>
#include <asm/system.h>
#include <asm/leds.h>
#include <asm/arch/hardware.h>
#define TIMER00_TYPE (volatile unsigned int*)
#include <asm/arch/timer00.h>


/*
 * IRQ handler for the timer
 */
static void excalibur_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{

	// ...clear the interrupt
	*TIMER0_CR(IO_ADDRESS(EXC_TIMER00_BASE))|=TIMER0_CR_CI_MSK;

	do_leds();
	do_timer(regs);
	do_profile(regs);
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
extern __inline__ void setup_timer(void)
{


	timer_irq.handler = excalibur_timer_interrupt;


	/* 
	 * Make irqs happen for the system timer
	 */
	setup_arm_irq(IRQ_TIMER0, &timer_irq);


	/* Stop the timer, reconfigure it and then restart it */

	*TIMER0_CR(IO_ADDRESS(EXC_TIMER00_BASE))=0;
	*TIMER0_LIMIT(IO_ADDRESS(EXC_TIMER00_BASE))=(unsigned int)(EXC_AHB2_CLK_FREQUENCY/200);
	*TIMER0_PRESCALE(IO_ADDRESS(EXC_TIMER00_BASE))=1;
	*TIMER0_CR(IO_ADDRESS(EXC_TIMER00_BASE))=TIMER0_CR_IE_MSK | TIMER0_CR_S_MSK;
}
