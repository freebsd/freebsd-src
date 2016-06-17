/*
 *  linux/arch/arm/mach-mx1ads/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
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
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <asm/mach/irq.h>

/*
 *
 * We simply use the ENABLE DISABLE registers inside of the MX1
 * to turn on/off specific interrupts.  FIXME- We should
 * also add support for the accelerated interrupt controller
 * by putting offets to irq jump code in the appropriate
 * places.
 *
 */

#define INTENNUM_OFF              0x8
#define INTDISNUM_OFF             0xC

#define VA_AITC_BASE              IO_ADDRESS(MX1ADS_AITC_BASE)
#define MX1ADS_AITC_INTDISNUM     (VA_AITC_BASE + INTDISNUM_OFF)
#define MX1ADS_AITC_INTENNUM      (VA_AITC_BASE + INTENNUM_OFF)

static void
mx1ads_mask_irq(unsigned int irq)
{
	__raw_writel(irq, MX1ADS_AITC_INTDISNUM);
}

static void
mx1ads_unmask_irq(unsigned int irq)
{
	__raw_writel(irq, MX1ADS_AITC_INTENNUM);
}

void __init
mx1ads_init_irq(void)
{
	unsigned int i;

	for (i = 0; i < NR_IRQS; i++) {
		irq_desc[i].valid = 1;
		irq_desc[i].probe_ok = 1;
		irq_desc[i].mask_ack = mx1ads_mask_irq;
		irq_desc[i].mask = mx1ads_mask_irq;
		irq_desc[i].unmask = mx1ads_unmask_irq;
	}

	/* Disable all interrupts initially. */
	/* In MX1 this is done in the bootloader. */
}
