/*
 *  linux/arch/arm/mach-ebsa110/mm.c
 *
 *  Copyright (C) 1998-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Extra MM routines for the EBSA-110 architecture
 */
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/page.h>

#include <asm/mach/map.h>

#include "hardware.h"
 
static struct map_desc ebsa110_io_desc[] __initdata = {
	/*
	 * sparse external-decode ISAIO space
	 */
	{ IRQ_STAT,    TRICK4_PHYS, PGDIR_SIZE,  DOMAIN_IO, 0, 1, 0, 0 }, /* IRQ_STAT/IRQ_MCLR */
	{ IRQ_MASK,    TRICK3_PHYS, PGDIR_SIZE,  DOMAIN_IO, 0, 1, 0, 0 }, /* IRQ_MASK/IRQ_MSET */
	{ SOFT_BASE,   TRICK1_PHYS, PGDIR_SIZE,  DOMAIN_IO, 0, 1, 0, 0 }, /* SOFT_BASE */
	{ PIT_BASE,    TRICK0_PHYS, PGDIR_SIZE,  DOMAIN_IO, 0, 1, 0, 0 }, /* PIT_BASE */

	/*
	 * self-decode ISAIO space
	 */
	{ ISAIO_BASE,  ISAIO_PHYS,  ISAIO_SIZE,  DOMAIN_IO, 0, 1, 0, 0 },
	{ ISAMEM_BASE, ISAMEM_PHYS, ISAMEM_SIZE, DOMAIN_IO, 0, 1, 0, 0 },
	LAST_DESC
};

void __init ebsa110_map_io(void)
{
	iotable_init(ebsa110_io_desc);
}
