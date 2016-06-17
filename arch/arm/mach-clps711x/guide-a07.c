/*
 *  linux/arch/arm/mach-clps711x/guide-a07.c
 *
 *  Copyright (C) 2003 Iders Incorporated
 *  Copyright (C) 2000-2001 Deep Blue Solutions Ltd
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
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/config.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

extern void clps711x_init_irq(void);
extern void clps711x_map_io(void);

#ifndef CONFIG_I2C_GUIDE
#error Config error - The Guide A07 requires I2C Guide GPIO support. Please enable it.
#endif

/*
 * Map the CS89712 Ethernet port.  That should be moved to the
 * ethernet driver, perhaps.
 * also map the Guide FPGA and persistance locations
 */
static struct map_desc guide_a07_io_desc[] __initdata = {
	{              ETHER_BASE,              ETHER_START,              ETHER_SIZE,
		DOMAIN_IO, 0, 1, 0, 0 },
	{        GD_A07_FPGA_BASE,        GD_A07_FPGA_START,        GD_A07_FPGA_SIZE,
		DOMAIN_IO, 0, 1, 0, 0 },
	{ GD_A07_PERSISTANCE_BASE, GD_A07_PERSISTANCE_START, GD_A07_PERSISTANCE_SIZE,
		DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

static void __init
fixup_guide_a07(struct machine_desc *desc, struct param_struct *params,
	    char **cmdline, struct meminfo *mi)
{
}

static void __init guide_a07_map_io(void)
{
	clps711x_map_io();
	iotable_init(guide_a07_io_desc);
}

MACHINE_START(GUIDEA07, "Guide A07 (cs89712 core)")
	MAINTAINER("Cam Mayor")
	VIDEO(0x60000000, 0x6000bfff)
	BOOT_MEM(0xc0000000, 0x80000000, 0xff000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_guide_a07)
	MAPIO(guide_a07_map_io)
	INITIRQ(clps711x_init_irq)
MACHINE_END

static int guide_a07_hw_init(void)
{
	/* in cs[1] (the FPGA), set zero wait states, clkenb, and sqaen */
	u32 memcfg1 = 0xfc << 8;
	memcfg1 |= clps_readl(MEMCFG1);
	clps_writel(memcfg1, MEMCFG1);

	return 0;
}

__initcall(guide_a07_hw_init);

