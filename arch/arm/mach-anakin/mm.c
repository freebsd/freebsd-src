/*
 *  linux/arch/arm/mach-anakin/mm.c
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   09-Apr-2001 W/TTC	Created
 */

#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mach/map.h>

static struct map_desc anakin_io_desc[] __initdata = {
	{ IO_BASE,    IO_START,    IO_SIZE,    DOMAIN_IO, 0, 1, 0, 0 },
	{ FLASH_BASE, FLASH_START, FLASH_SIZE, DOMAIN_IO, 1, 1, 0, 0 },
	{ VGA_BASE,   VGA_START,   VGA_SIZE,   DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

void __init
anakin_map_io(void)
{
	iotable_init(anakin_io_desc);
}
