/*
 * linux/arch/arm/mach-sa1100/brutus.c
 *
 * Author: Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/hardware.h>
#include <asm/setup.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/serial_sa1100.h>

#include "generic.h"


static void __init
fixup_brutus(struct machine_desc *desc, struct param_struct *params,
	     char **cmdline, struct meminfo *mi)
{
	SET_BANK( 0, 0xc0000000, 4*1024*1024 );
	SET_BANK( 1, 0xc8000000, 4*1024*1024 );
	SET_BANK( 2, 0xd0000000, 4*1024*1024 );
	SET_BANK( 3, 0xd8000000, 4*1024*1024 );
	mi->nr_banks = 4;

	ROOT_DEV = MKDEV(RAMDISK_MAJOR,0);
	setup_ramdisk( 1, 0, 0, 8192 );
	setup_initrd( __phys_to_virt(0xd8000000), 3*1024*1024 );
}

static void __init brutus_map_io(void)
{
	sa1100_map_io();

	sa1100_register_uart(0, 1);
	sa1100_register_uart(1, 3);
	GAFR |= (GPIO_UART_TXD | GPIO_UART_RXD);
	GPDR |= GPIO_UART_TXD;
	GPDR &= ~GPIO_UART_RXD;
	PPAR |= PPAR_UPR;
}

MACHINE_START(BRUTUS, "Intel Brutus (SA1100 eval board)")
	BOOT_MEM(0xc0000000, 0x80000000, 0xf8000000)
	FIXUP(fixup_brutus)
	MAPIO(brutus_map_io)
	INITIRQ(sa1100_init_irq)
MACHINE_END
