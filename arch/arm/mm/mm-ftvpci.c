/*
 *  linux/arch/arm/mm/mm-nexuspci.c
 *   from linux/arch/arm/mm/mm-ebsa110.c
 *
 *  Copyright (C) 1998-1999 Phil Blundell
 *  Copyright (C) 1998-1999 Russell King
 *
 *  Extra MM routines for the FTV/PCI architecture
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
 
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/io.h>

#include <asm/mach/map.h>

static struct map_desc nexuspci_io_desc[] __initdata = {
 	{ INTCONT_BASE,	INTCONT_START,	0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
 	{ PLX_BASE,	PLX_START,	0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
 	{ PCIO_BASE,	PLX_IO_START,	0x00100000, DOMAIN_IO, 0, 1, 0, 0 },
 	{ DUART_BASE,	DUART_START,	0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
	{ STATUS_BASE,	STATUS_START,	0x00001000, DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

void __init nexuspci_map_io(void)
{
	iotable_init(nexuspci_io_desc);
}
