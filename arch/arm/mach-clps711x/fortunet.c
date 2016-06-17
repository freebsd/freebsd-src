/*
 *  linux/arch/arm/mach-clps711x/fortunet.c
 *
 *  Derived from linux/arch/arm/mach-integrator/arch.c
 *
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
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/blk.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mach/map.h>

#include <asm/mach/arch.h>
#include <asm/mach/amba_kmi.h>

extern void clps711x_map_io(void);
extern void clps711x_init_irq(void);

struct meminfo memmap = { 1, 0xC1000000, {{0xC0000000,0x01000000,0}}};

typedef struct tag_IMAGE_PARAMS
{
	int	ramdisk_ok;
	int	ramdisk_address;
	int	ramdisk_size;
	int	ram_size;
	int	extra_param_type;
	int	extra_param_ptr;
	int	command_line;
	int	extra_ram_start;
	int	extra_ram_size;
} IMAGE_PARAMS;

#define IMAGE_PARAMS_PHYS	0xC0200000

static void __init
fortunet_fixup(struct machine_desc *desc, struct param_struct *params,
		 char **cmdline, struct meminfo *mi)
{
	IMAGE_PARAMS *ip;
	ip = (IMAGE_PARAMS *)__phys_to_virt(IMAGE_PARAMS_PHYS);
	*cmdline = (char *)__phys_to_virt(ip->command_line);
#ifdef CONFIG_BLK_DEV_INITRD
	if(ip->ramdisk_ok)
	{
		initrd_start = __phys_to_virt(ip->ramdisk_address);
		initrd_end = initrd_start + ip->ramdisk_size;
	}
#endif
	memmap.bank[0].size = ip->ram_size;
	memmap.bank[0].node = PHYS_TO_NID(0xC0000000);
	if(ip->extra_ram_size)
	{
		memmap.bank[1].start = ip->extra_ram_start;
		memmap.bank[1].size = ip->extra_ram_size;
		memmap.bank[1].node = PHYS_TO_NID(ip->extra_ram_start);
		mi->nr_banks=2;
	}
	memmap.end = ip->ram_size+0xC0000000;
	*mi = memmap;
}

MACHINE_START(FORTUNET, "ARM-FortuNet")
	MAINTAINER("FortuNet Inc.")
        BOOT_MEM(0xc0000000, 0x80000000, 0xff000000)
	BOOT_PARAMS(0x00000000)
	VIDEO(0xC0000000,0xC00020000)
	FIXUP(fortunet_fixup)
	MAPIO(clps711x_map_io)
	INITIRQ(clps711x_init_irq)
MACHINE_END
