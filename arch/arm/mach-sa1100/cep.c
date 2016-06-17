/*
 * linux/arch/arm/mach-sa1100/cep.c
 *
 * Author: Matthias Gorjup
 *
 * This file contains all Cep - specific tweaks.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/serial_core.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


extern void convert_to_tag_list(struct param_struct *params, int mem_init);

static void __init
fixup_cep(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
	struct tag *t = (struct tag *)params;


	/*
	 * Apparantly bootldr uses a param_struct.  Groan.
	 */
	if (t->hdr.tag != ATAG_CORE)
		convert_to_tag_list(params, 1);

	if (t->hdr.tag != ATAG_CORE) {
		t->hdr.tag = ATAG_CORE;
		t->hdr.size = tag_size(tag_core);
		t->u.core.flags = 0;
		t->u.core.pagesize = PAGE_SIZE;
		t->u.core.rootdev = RAMDISK_MAJOR << 8 | 0;
		t = tag_next(t);

		t->hdr.tag = ATAG_MEM;
		t->hdr.size = tag_size(tag_mem32);
		t->u.mem.start = 0xc0000000;
		t->u.mem.size  = 32 * 1024 * 1024;
		t = tag_next(t);


		t->hdr.tag = ATAG_RAMDISK;
		t->hdr.size = tag_size(tag_ramdisk);
		t->u.ramdisk.flags = 1;
		t->u.ramdisk.size = 8192;
		t->u.ramdisk.start = 0;
		t = tag_next(t);

		t->hdr.tag = ATAG_INITRD;
		t->hdr.size = tag_size(tag_initrd);
		t->u.initrd.start = 0xc0800000;
		t->u.initrd.size = 3 * 1024 * 1024;
		t = tag_next(t);

		t->hdr.tag = ATAG_NONE;
		t->hdr.size = 0;
	}
}


static struct map_desc cep_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  LAST_DESC
};

static void __init cep_map_io(void)
{

	sa1100_map_io();
	iotable_init(cep_io_desc);

	sa1100_register_uart(0, 1);	/* com port */
	sa1100_register_uart(2, 3);	/* radio module */

	/*
	 * Ensure that these pins are set as outputs and are driving
	 * logic 0.  This ensures that we won't inadvertently toggle
	 * the WS latch in the CPLD, and we don't float causing
	 * excessive power drain.  --rmk
	 */
	GPDR |= GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM;
	GPCR = GPIO_SSP_TXD | GPIO_SSP_SCLK | GPIO_SSP_SFRM;

	/*
	 * Set up registers for sleep mode.
	 */
	PWER = PWER_GPIO0;
	PGSR = 0;
	PCFR = 0;
	PSDR = 0;
}


MACHINE_START(CEP, "Iskratel Cep")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_cep)
	MAPIO(cep_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
