/*
 * linux/arch/arm/mach-sa1100/victor.c
 *
 * Author: Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


static void victor_power_off(void)
{
	/* switch off power supply */
	mdelay(2000);
	GPCR = GPIO_GPIO23;
	while (1);
}

static int __init victor_init(void)
{
	pm_power_off = victor_power_off;
	return 0;
}

__initcall(victor_init);


static void __init
fixup_victor(struct machine_desc *desc, struct param_struct *params,
	     char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 4*1024*1024 );
	mi->nr_banks = 1;

	ROOT_DEV = MKDEV( 60, 2 );

	/* Get command line parameters passed from the loader (if any) */
	if( *((char*)0xc0000000) )
		strcpy( *cmdline, ((char *)0xc0000000) );

	/* power off if any problem */
	strcat( *cmdline, " panic=1" );
}

static struct map_desc victor_io_desc[] __initdata = {
 /* virtual     physical    length      domain     r  w  c  b */
  { 0xe8000000, 0x00000000, 0x00200000, DOMAIN_IO, 0, 1, 0, 0 }, /* Flash */
  LAST_DESC
};

static void __init victor_map_io(void)
{
	sa1100_map_io();
	iotable_init(victor_io_desc);

	sa1100_register_uart(0, 3);
}

MACHINE_START(VICTOR, "VisuAide Victor")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_victor)
	MAPIO(victor_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
