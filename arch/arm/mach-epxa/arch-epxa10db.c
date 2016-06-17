/*
 *  linux/arch/arm/mach-epxa/arch-epxa10db.c
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
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>

#include <asm/mach/arch.h>

extern void epxa_map_io(void);
extern void epxa_init_irq(void);


static void __init
epxa10db_fixup(struct machine_desc *desc, struct param_struct *params,
		 char **cmdline, struct meminfo *mi)
{

        mi->nr_banks      = 1;
        mi->bank[0].start = 0;
        mi->bank[0].size  = (128*1024*1024);
        mi->bank[0].node  = 0;

/*
        ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
        setup_ramdisk( 1, 0, 0, 8192 );
        setup_initrd(0xc0200000, 6*1024*1024);
*/
}

MACHINE_START(CAMELOT, "Altera Epxa10db")
	MAINTAINER("Altera Corporation")
	BOOT_MEM(0x00000000, 0x7fffc000, 0xffffc000)
      	FIXUP(epxa10db_fixup)
	MAPIO(epxa_map_io)
	INITIRQ(epxa_init_irq)
        BOOT_PARAMS(0x100)
MACHINE_END
