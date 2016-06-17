/*
 *  linux/arch/arm/mach-integrator/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
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
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>

/* 
 * All IO addresses are mapped onto VA 0xFFFx.xxxx, where x.xxxx
 * is the (PA >> 12).
 *
 * Setup a VA for the Integrator interrupt controller (for header #0,
 * just for now).
 */
#define VA_IC_BASE              IO_ADDRESS(INTEGRATOR_IC_BASE) 
#define VA_CMIC_BASE            IO_ADDRESS(INTEGRATOR_HDR_BASE) + INTEGRATOR_HDR_IC_OFFSET

#define ALLPCI ( (1 << IRQ_PCIINT0) | (1 << IRQ_PCIINT1) | (1 << IRQ_PCIINT2) | (1 << IRQ_PCIINT3) ) 

static void sc_mask_irq(unsigned int irq)
{
        __raw_writel(1 << irq, VA_IC_BASE + IRQ_ENABLE_CLEAR);
}

static void sc_unmask_irq(unsigned int irq)
{
        __raw_writel(1 << irq, VA_IC_BASE + IRQ_ENABLE_SET);
}
 
void __init integrator_init_irq(void)
{
	unsigned int i;

	for (i = 0; i < NR_IRQS; i++) {
	        if (((1 << i) && INTEGRATOR_SC_VALID_INT) != 0) {
		        irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 1;
			irq_desc[i].mask_ack	= sc_mask_irq;
			irq_desc[i].mask	= sc_mask_irq;
			irq_desc[i].unmask	= sc_unmask_irq;
		}
	}

	/* Disable all interrupts initially. */
	/* Do the core module ones */
	__raw_writel(-1, VA_CMIC_BASE + IRQ_ENABLE_CLEAR);

	/* do the header card stuff next */
	__raw_writel(-1, VA_IC_BASE + IRQ_ENABLE_CLEAR);
	__raw_writel(-1, VA_IC_BASE + FIQ_ENABLE_CLEAR);
}
