/*
 *  linux/arch/arm/mach-mx1ads/arch.c
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/amba_kmi.h>

extern void mx1ads_map_io(void);
extern void mx1ads_init_irq(void);


static void __init
mx1ads_fixup(struct machine_desc *desc, struct param_struct *unused,
		 char **cmdline, struct meminfo *mi)
{
}

MACHINE_START(MX1ADS, "Motorola MX1ADS")
	MAINTAINER("Shane Nay")
#ifdef CONFIG_ARCH_MX1ADS_SRAM
	BOOT_MEM(0x12000000, 0x00200000, 0xf0200000)
#else
	BOOT_MEM(0x08000000, 0x00200000, 0xf0200000)
#endif
	FIXUP(mx1ads_fixup)
	MAPIO(mx1ads_map_io)
	INITIRQ(mx1ads_init_irq)
MACHINE_END

#if 0

#endif
