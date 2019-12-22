/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * This file contains facilities for runtime determination of address space
 * mappings for use in DMA/mailbox interactions.  This is only used for the
 * arm64 SoC because the 32-bit SoC used the same mappings.
 */
#if defined (__aarch64__)
#include "opt_soc.h"
#endif

#include <sys/types.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/broadcom/bcm2835/bcm2835_vcbus.h>

/*
 * This structure describes mappings that need to take place when transforming
 * ARM core addresses into vcbus addresses for use with the DMA/mailbox
 * interfaces.  Currently, we only deal with peripheral/SDRAM address spaces
 * here.
 *
 * The SDRAM address space is consistently mapped starting at 0 and extends to
 * the size of the installed SDRAM.
 *
 * Peripherals are mapped further up at spots that vary per-SOC.
 */
struct bcm283x_memory_mapping {
	vm_paddr_t	armc_start;
	vm_paddr_t	armc_size;
	vm_paddr_t	vcbus_start;
};

#ifdef SOC_BCM2835
static struct bcm283x_memory_mapping bcm2835_memmap[] = {
	{
		/* SDRAM */
		.armc_start = 0x00000000,
		.armc_size = BCM2835_ARM_IO_BASE,
		.vcbus_start = BCM2835_VCBUS_SDRAM_BASE,
	},
	{
		/* Peripherals */
		.armc_start = BCM2835_ARM_IO_BASE,
		.armc_size  = BCM28XX_ARM_IO_SIZE,
		.vcbus_start = BCM2835_VCBUS_IO_BASE,
	},
	{ 0, 0, 0 },
};
#endif

#ifdef SOC_BCM2836
static struct bcm283x_memory_mapping bcm2836_memmap[] = {
	{
		/* SDRAM */
		.armc_start = 0x00000000,
		.armc_size = BCM2836_ARM_IO_BASE,
		.vcbus_start = BCM2836_VCBUS_SDRAM_BASE,
	},
	{
		/* Peripherals */
		.armc_start = BCM2836_ARM_IO_BASE,
		.armc_size  = BCM28XX_ARM_IO_SIZE,
		.vcbus_start = BCM2836_VCBUS_IO_BASE,
	},
	{ 0, 0, 0 },
};
#endif

#ifdef SOC_BRCM_BCM2837
static struct bcm283x_memory_mapping bcm2837_memmap[] = {
	{
		/* SDRAM */
		.armc_start = 0x00000000,
		.armc_size = BCM2837_ARM_IO_BASE,
		.vcbus_start = BCM2837_VCBUS_SDRAM_BASE,
	},
	{
		/* Peripherals */
		.armc_start = BCM2837_ARM_IO_BASE,
		.armc_size  = BCM28XX_ARM_IO_SIZE,
		.vcbus_start = BCM2837_VCBUS_IO_BASE,
	},
	{ 0, 0, 0 },
};
#endif

#ifdef SOC_BRCM_BCM2838

/*
 * The BCM2838 supports up to 4GB of SDRAM, but unfortunately we can still only
 * map the first 1GB into the "legacy master view" (vcbus) address space.  Thus,
 * peripherals can still only access the lower end of SDRAM.  For this reason,
 * we also capture the main-peripheral busdma restriction below.
 */
static struct bcm283x_memory_mapping bcm2838_memmap[] = {
	{
		/* SDRAM */
		.armc_start = 0x00000000,
		.armc_size = 0x40000000,
		.vcbus_start = BCM2838_VCBUS_SDRAM_BASE,
	},
	{
		/* Main peripherals */
		.armc_start = BCM2838_ARM_IO_BASE,
		.armc_size = BCM28XX_ARM_IO_SIZE,
		.vcbus_start = BCM2838_VCBUS_IO_BASE,
	},
	{ 0, 0, 0 },
};
#endif

static struct bcm283x_memory_soc_cfg {
	struct bcm283x_memory_mapping	*memmap;
	const char			*soc_compat;
	bus_addr_t			 busdma_lowaddr;
} bcm283x_memory_configs[] = {
#ifdef SOC_BCM2835
	/* Legacy */
	{
		.memmap = bcm2835_memmap,
		.soc_compat = "raspberrypi,model-b",
		.busdma_lowaddr = BUS_SPACE_MAXADDR_32BIT,
	},
	/* Modern */
	{
		.memmap = bcm2835_memmap,
		.soc_compat = "brcm,bcm2835",
		.busdma_lowaddr = BUS_SPACE_MAXADDR_32BIT,
	},
#endif
#ifdef SOC_BCM2836
	/* Legacy */
	{
		.memmap = bcm2836_memmap,
		.soc_compat = "brcm,bcm2709",
		.busdma_lowaddr = BUS_SPACE_MAXADDR_32BIT,
	},
	/* Modern */
	{
		.memmap = bcm2836_memmap,
		.soc_compat = "brcm,bcm2836",
		.busdma_lowaddr = BUS_SPACE_MAXADDR_32BIT,
	},

#endif
#ifdef SOC_BRCM_BCM2837
	{
		.memmap = bcm2837_memmap,
		.soc_compat = "brcm,bcm2837",
		.busdma_lowaddr = BUS_SPACE_MAXADDR_32BIT,
	},
#endif
#ifdef SOC_BRCM_BCM2838
	{
		.memmap = bcm2838_memmap,
		.soc_compat = "brcm,bcm2711",
		.busdma_lowaddr = BCM2838_PERIPH_MAXADDR,
	},
	{
		.memmap = bcm2838_memmap,
		.soc_compat = "brcm,bcm2838",
		.busdma_lowaddr = BCM2838_PERIPH_MAXADDR,
	},
#endif
};

static struct bcm283x_memory_soc_cfg *booted_soc_memcfg;

static struct bcm283x_memory_soc_cfg *
bcm283x_get_current_memcfg(void)
{
	phandle_t root;
	int i;

	/* We'll cache it once we decide, because it won't change per-boot. */
	if (booted_soc_memcfg != NULL)
		return (booted_soc_memcfg);

	KASSERT(nitems(bcm283x_memory_configs) != 0,
	    ("No SOC memory configurations enabled!"));

	root = OF_finddevice("/");
	for (i = 0; i < nitems(bcm283x_memory_configs); ++i) {
		booted_soc_memcfg = &bcm283x_memory_configs[i];
		printf("Checking root against %s\n",
		booted_soc_memcfg->soc_compat);
		if (ofw_bus_node_is_compatible(root,
		    booted_soc_memcfg->soc_compat))
			return (booted_soc_memcfg);
	}

	/*
	 * The kernel doesn't fit the board; we can't really make a reasonable
	 * guess, as these SOC are different enough that something will blow up
	 * later.
	 */
	panic("No suitable SOC memory configuration found.");
}

#define	BCM283X_MEMMAP_ISTERM(ent)	\
    ((ent)->armc_start == 0 && (ent)->armc_size == 0 && \
    (ent)->vcbus_start == 0)

vm_paddr_t
bcm283x_armc_to_vcbus(vm_paddr_t pa)
{
	struct bcm283x_memory_soc_cfg *cfg;
	struct bcm283x_memory_mapping *map, *ment;

	/* Guaranteed not NULL if we haven't panicked yet. */
	cfg = bcm283x_get_current_memcfg();
	map = cfg->memmap;
	for (ment = map; !BCM283X_MEMMAP_ISTERM(ment); ++ment) {
		if (pa >= ment->armc_start &&
		    pa < ment->armc_start + ment->armc_size) {
			return (pa - ment->armc_start) + ment->vcbus_start;
		}
	}

	/*
	 * Assume 1:1 mapping for anything else, but complain about it on
	 * verbose boots.
	 */
	if (bootverbose)
		printf("bcm283x_vcbus: No armc -> vcbus mapping found: %jx\n",
		    (uintmax_t)pa);
	return (pa);
}

vm_paddr_t
bcm283x_vcbus_to_armc(vm_paddr_t vca)
{
	struct bcm283x_memory_soc_cfg *cfg;
	struct bcm283x_memory_mapping *map, *ment;

	/* Guaranteed not NULL if we haven't panicked yet. */
	cfg = bcm283x_get_current_memcfg();
	map = cfg->memmap;
	for (ment = map; !BCM283X_MEMMAP_ISTERM(ment); ++ment) {
		if (vca >= ment->vcbus_start &&
		    vca < ment->vcbus_start + ment->armc_size) {
			return (vca - ment->vcbus_start) + ment->armc_start;
		}
	}

	/*
	 * Assume 1:1 mapping for anything else, but complain about it on
	 * verbose boots.
	 */
	if (bootverbose)
		printf("bcm283x_vcbus: No vcbus -> armc mapping found: %jx\n",
		    (uintmax_t)vca);
	return (vca);
}

bus_addr_t
bcm283x_dmabus_peripheral_lowaddr(void)
{
	struct bcm283x_memory_soc_cfg *cfg;

	cfg = bcm283x_get_current_memcfg();
	return (cfg->busdma_lowaddr);
}
