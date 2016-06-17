/*
 *  linux/include/asm-arm/arch-omaha/time.h
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
#include <asm/system.h>
#include <asm/leds.h>

#define TIMER_INTERVAL	mSEC_10

extern unsigned long (*gettimeoffset)(void);

/*
 * Returns number of ms since last clock interrupt.  Note that interrupts
 * will have been disabled by do_gettimeoffset()
 */
static unsigned long omaha_gettimeoffset(void)
{
	volatile unsigned int *p;
	unsigned long ticks1, ticks2;

	/*
	 * Get the current number of ticks.  Note that there is a race
	 * condition between us reading the timer and checking for
	 * an interrupt.  We get around this by ensuring that the
	 * counter has not reloaded between our two reads.
	 */
	p = (unsigned int *)(IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TCNTB0));
	
	// Its only 16-bits
	ticks2 = __raw_readl(p);
	
	do {
		ticks1 = ticks2;
		ticks2 =  __raw_readl(p);
	} while (ticks2 > ticks1);

	/*
	 * Number of ticks since last interrupt.
	 */
	ticks1 = TIMER_INTERVAL - ticks2;

	// Return number of usecs since last interrupt
	return ticks1;
}

/*
 * IRQ handler for the timer
 */
static void omaha_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile unsigned int *p;
	int tmp;
	
	// Clear the interrupt pending register
	p = (unsigned int *)(IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_SRCPND));
	
	tmp = __raw_readl(p);
	tmp = tmp & ~OMAHA_INT_TIMER0;
	__raw_writel(tmp, p);

	do_leds();
	do_timer(regs);
	do_profile(regs);
}

/*
 * Set up timer interrupt, and return the current time in seconds.
 *
 * The bootloader ensures that the timer always counts at 1MHz.
 */
static inline void setup_timer(void)
{
	volatile unsigned int *p;
	int tmp;

	timer_irq.handler = omaha_timer_interrupt;

	/*
	 * Program reload count.
	 */
	p = (unsigned int *)(IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TCNTB0));
	
	__raw_writel(TIMER_INTERVAL, p);
	
	// Set manual update bit, clear start bit
	p = (unsigned int *)(IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TCON));
	tmp = __raw_readl(p);
	tmp = tmp | 0x2;
	tmp = tmp & ~0x1;
	__raw_writel(tmp, p);
	
	// Clear manual update bit and set start bit
	p = (unsigned int *)(IO_ADDRESS(PLAT_PERIPHERAL_BASE+OMAHA_TCON));
	tmp = __raw_readl(p);
	tmp = tmp & ~0x2;
	tmp = tmp | 0x1;
	__raw_writel(tmp, p);	

	/* 
	 * Make irqs happen for the system timer
	 */
	setup_arm_irq(OMAHA_INT_TIMER0, &timer_irq);
	gettimeoffset = omaha_gettimeoffset;
}
