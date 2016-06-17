/*
 *  linux/arch/arm/mach-footbridge/mm.c
 *
 *  Copyright (C) 1998-2000 Russell King, Dave Gilbert.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Extra MM routines for the EBSA285 architecture
 */
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
 
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/hardware/dec21285.h>
#include <asm/mach-types.h>

#include <asm/mach/map.h>

/*
 * Common mapping for all systems.  Note that the outbound write flush is
 * commented out since there is a "No Fix" problem with it.  Not mapping
 * it means that we have extra bullet protection on our feet.
 */
static struct map_desc fb_common_io_desc[] __initdata = {
 { ARMCSR_BASE,	 DC21285_ARMCSR_BASE,	    ARMCSR_SIZE,  DOMAIN_IO, 0, 1, 0, 0 },
 { XBUS_BASE,    0x40000000,		    XBUS_SIZE,    DOMAIN_IO, 0, 1, 0, 0 },
 LAST_DESC
};

/*
 * The mapping when the footbridge is in host mode.  We don't map any of
 * this when we are in add-in mode.
 */
static struct map_desc ebsa285_host_io_desc[] __initdata = {
#if defined(CONFIG_ARCH_FOOTBRIDGE) && defined(CONFIG_FOOTBRIDGE_HOST)
 { PCIMEM_BASE,  DC21285_PCI_MEM,	    PCIMEM_SIZE,  DOMAIN_IO, 0, 1, 0, 0 },
 { PCICFG0_BASE, DC21285_PCI_TYPE_0_CONFIG, PCICFG0_SIZE, DOMAIN_IO, 0, 1, 0, 0 },
 { PCICFG1_BASE, DC21285_PCI_TYPE_1_CONFIG, PCICFG1_SIZE, DOMAIN_IO, 0, 1, 0, 0 },
 { PCIIACK_BASE, DC21285_PCI_IACK,	    PCIIACK_SIZE, DOMAIN_IO, 0, 1, 0, 0 },
 { PCIO_BASE,    DC21285_PCI_IO,	    PCIO_SIZE,	  DOMAIN_IO, 0, 1, 0, 0 },
#endif
 LAST_DESC
};

/*
 * The CO-ebsa285 mapping.
 */
static struct map_desc co285_io_desc[] __initdata = {
#ifdef CONFIG_ARCH_CO285
 { PCIO_BASE,	 DC21285_PCI_IO,	    PCIO_SIZE,    DOMAIN_IO, 0, 1, 0, 0 },
 { PCIMEM_BASE,	 DC21285_PCI_MEM,	    PCIMEM_SIZE,  DOMAIN_IO, 0, 1, 0, 0 },
#endif
 LAST_DESC
};

void __init footbridge_map_io(void)
{
	struct map_desc *desc = NULL;

	/*
	 * Set up the common mapping first; we need this to
	 * determine whether we're in host mode or not.
	 */
	iotable_init(fb_common_io_desc);

	/*
	 * Now, work out what we've got to map in addition on this
	 * platform.
	 */
	if (machine_is_co285())
		desc = co285_io_desc;
	else if (footbridge_cfn_mode())
		desc = ebsa285_host_io_desc;

	if (desc)
		iotable_init(desc);
}

#ifdef CONFIG_FOOTBRIDGE_ADDIN

/*
 * These two functions convert virtual addresses to PCI addresses and PCI
 * addresses to virtual addresses.  Note that it is only legal to use these
 * on memory obtained via get_free_page or kmalloc.
 */
unsigned long __virt_to_bus(unsigned long res)
{
#ifdef CONFIG_DEBUG_ERRORS
	if (res < PAGE_OFFSET || res >= (unsigned long)high_memory) {
		printk("__virt_to_bus: invalid virtual address 0x%08lx\n", res);
		__backtrace();
	}
#endif
	return (res - PAGE_OFFSET) + (*CSR_PCISDRAMBASE & 0xfffffff0);
}

unsigned long __bus_to_virt(unsigned long res)
{
	res -= (*CSR_PCISDRAMBASE & 0xfffffff0);
	res += PAGE_OFFSET;

#ifdef CONFIG_DEBUG_ERRORS
	if (res < PAGE_OFFSET || res >= (unsigned long)high_memory) {
		printk("__bus_to_virt: invalid virtual address 0x%08lx\n", res);
		__backtrace();
	}
#endif
	return res;
}

#endif
