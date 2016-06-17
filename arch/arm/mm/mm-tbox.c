/*
 *  linux/arch/arm/mm/mm-tbox.c
 *
 *  Copyright (C) 1998, 1999, 2000 Phil Blundell
 *  Copyright (C) 1998-1999 Russell King
 *
 *  Extra MM routines for the Tbox architecture
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#include <asm/mach/map.h>
 
static struct map_desc tbox_io_desc[] __initdata = {
	/* See hardware.h for details */
	{ IO_BASE,	IO_START,	0x00100000, DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

void __init tbox_map_io(void)
{
	iotable_init(tbox_io_desc);
}
