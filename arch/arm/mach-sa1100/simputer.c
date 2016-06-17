/*
 * linux/arch/arm/mach-sa1100/simputer.c
 *
 * Author: Vivek K S (modified from Nicolas Pitre's Assabet file)
 *
 * This file contains all Simputer-specific tweaks.
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
#include <asm/arch/simputer.h>

#include "generic.h"

static void __init
fixup_simputer(struct machine_desc *desc, struct param_struct *params,
	      char **cmdline, struct meminfo *mi)
{
}


static struct map_desc simputer_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x02000000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash bank 0 */
  { 0xf2800000, 0x40000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* Simputer Modem */
  { 0xf3800000, 0x48000000, 0x00100000, DOMAIN_IO, 0, 1, 0, 0 }, /* USB */
  LAST_DESC
};

static void __init simputer_map_io(void)
{
	sa1100_map_io();
	iotable_init(simputer_io_desc);

	sa1100_register_uart(1, 2);
	sa1100_register_uart(0, 1);	/* com port */
	sa1100_register_uart(2, 3);	/* radio module */

	/*
	 * Set up registers for sleep mode.
	 */
	PWER = PWER_GPIO0;
	PGSR = 0;
	PCFR = 0;

	/*
	 * Clear all possible wakeup reasons.
	 */
	RCSR = RCSR_HWR | RCSR_SWR | RCSR_WDR | RCSR_SMR;
}


MACHINE_START(SIMPUTER, "Picopeta-Simputer")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	BOOT_PARAMS(0xc0000100)
	FIXUP(fixup_simputer)
	MAPIO(simputer_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
