/*
 *  linux/arch/arm/mm/mm-rpc.c
 *  linux/arch/arm/mm/mm-riscstation.c
 *
 *  Copyright (C) 1998-1999 Russell King
 *  Copyright (C) 2002 Simtec Electronics / Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Extra MM routines for RiscStation
 */
#include <linux/types.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/proc/domain.h>
#include <asm/setup.h>

#include <asm/mach/map.h>

/* map the EASI space to use for the ISA slot */

static struct map_desc riscstation_io_desc[] __initdata = {
 { IO_BASE,	IO_START,	IO_SIZE	 , DOMAIN_IO, 0, 1, 0, 0 }, /* IO space		*/
 { EASI_BASE,	EASI_START,	EASI_SIZE, DOMAIN_IO, 0, 1, 0, 0 }, /* EASI space	*/
 LAST_DESC
};

void __init riscstation_map_io(void)
{
	iotable_init(riscstation_io_desc);
}
