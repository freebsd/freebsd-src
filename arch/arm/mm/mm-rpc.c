/*
 *  linux/arch/arm/mm/mm-rpc.c
 *
 *  Copyright (C) 1998-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Extra MM routines for RiscPC architecture
 */
#include <linux/types.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/page.h>
#include <asm/proc/domain.h>
#include <asm/setup.h>

#include <asm/mach/map.h>

static struct map_desc rpc_io_desc[] __initdata = {
 { SCREEN_BASE,	SCREEN_START,	2*1048576, DOMAIN_IO, 0, 1, 0, 0 }, /* VRAM		*/
 { IO_BASE,	IO_START,	IO_SIZE	 , DOMAIN_IO, 0, 1, 0, 0 }, /* IO space		*/
 { EASI_BASE,	EASI_START,	EASI_SIZE, DOMAIN_IO, 0, 1, 0, 0 }, /* EASI space	*/
 LAST_DESC
};

void __init rpc_map_io(void)
{
	iotable_init(rpc_io_desc);
}
