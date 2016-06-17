/*
 *  linux/arch/arm/mach-omaha/core.c
 *
 *  Copyright (C) ARM Limited 2002.
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

extern void omaha_map_io(void);

/* 
 * All IO addresses are mapped onto VA 0xExxx.xxxx, where x.xxxx
 * is the (PA + 0xE0000000).
 *
 * Setup a VA for the Omaha interrupt controller.
 */
 
#define VA_IC_BASE              IO_ADDRESS(PLAT_PERIPHERAL_BASE) 

static void sc_mask_and_ack_irq(unsigned int irq)
{
	unsigned int tmp;
	
	// Mask this interrupt
	tmp = __raw_readl(VA_IC_BASE + OMAHA_INTMSK);
	tmp = tmp | (1 << irq);	  	
        __raw_writel(tmp, VA_IC_BASE + OMAHA_INTMSK);
	
	// Clear the source pending register
	tmp = __raw_readl(VA_IC_BASE + OMAHA_SRCPND);
	tmp = tmp | (1 << irq);	  	
        __raw_writel(tmp, VA_IC_BASE + OMAHA_SRCPND);
	
	// Clear the interrupt pending register
	tmp = __raw_readl(VA_IC_BASE + OMAHA_INTPND);
	tmp = tmp | (1 << irq);	  	
	__raw_writel(tmp, VA_IC_BASE + OMAHA_INTPND);

}

static void sc_mask_irq(unsigned int irq)
{
	unsigned int tmp;
	
	// Mask this interrupt
	tmp = __raw_readl(VA_IC_BASE + OMAHA_INTMSK);
	tmp = tmp | (1 << irq);	  	
        __raw_writel(tmp, VA_IC_BASE + OMAHA_INTMSK);
}

static void sc_unmask_irq(unsigned int irq)
{
	unsigned int tmp;
	
	tmp = __raw_readl(VA_IC_BASE + OMAHA_INTMSK);
	tmp = tmp & ~(1 << irq);	  	
        __raw_writel(tmp, VA_IC_BASE + OMAHA_INTMSK);
}
 
static void __init omaha_init_irq(void)
{
	unsigned int i;

	/* bootloader disables interrupt hardware,
	 * so we just set up linux data structures...
	 */

	for (i = 0; i < NR_IRQS; i++) {
		irq_desc[i].valid	= 1;
		irq_desc[i].probe_ok	= 1;
		irq_desc[i].mask_ack	= sc_mask_and_ack_irq;
		irq_desc[i].mask	= sc_mask_irq;
		irq_desc[i].unmask	= sc_unmask_irq;
	}
}

/* Notes
 *
 * IO space has been mapped into the top of virtual memory at 0xExxx xxxx
 * See IO_ACCESS macro for details.
 */

/* 		Logical		Physical
 * Start	0xE0000000	0x00000000
 * End		0xFAFFFFFC	0x1AFFFFFC
 */

/* Map the bottom 2Gb of IO space into the top of memory, but leave
 * space for the high vector table.
 */ 
static struct map_desc omaha_io_desc[] __initdata = {
 { IO_ADDRESS(0x00000000), 0x00000000, 0x1B000000, DOMAIN_IO, 0, 1 },
 LAST_DESC
};

static void __init omaha_map_io(void)
{
