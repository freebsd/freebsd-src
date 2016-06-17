/*
 * arch/ppc/kernel/ibm44x_common.c
 *
 * PPC44x system library
 *
 * Matt Porter <mporter@mvista.com>
 * Copyright 2002-2003 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/config.h>
#include <linux/types.h>
#include <asm/ibm44x.h>
#include <asm/mmu.h>

phys_addr_t fixup_bigphys_addr(phys_addr_t addr, phys_addr_t size)
{
	phys_addr_t page_4gb = 0;

        /*
	 * Trap the least significant 32-bit portions of an
	 * address in the 440's 36-bit address space.  Fix
	 * them up with the appropriate ERPN
	 */
	if ((addr >= PPC44x_IO_LO) && (addr < PPC44x_IO_HI))
		page_4gb = PPC44x_IO_PAGE;
	else if ((addr >= PPC44x_PCICFG_LO) && (addr < PPC44x_PCICFG_HI))
		page_4gb = PPC44x_PCICFG_PAGE;
	else if ((addr >= PPC44x_PCIMEM_LO) && (addr < PPC44x_PCIMEM_HI))
		page_4gb = PPC44x_PCIMEM_PAGE;

	return (page_4gb | addr);
};
