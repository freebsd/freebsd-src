/*
 *  linux/arch/arm/mach-shark/mm.c
 *
 *  by Alexander Schulz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/io.h>

#include <asm/mach/map.h>

static struct map_desc shark_io_desc[] __initdata = {
	{ IO_BASE	, IO_START	, IO_SIZE	, DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

void __init shark_map_io(void)
{
	iotable_init(shark_io_desc);
}
