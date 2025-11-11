/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pci_cfgreg.h>

struct pcie_mcfg_region {
	char *base;          /* virtual base of mapped MMCFG region */
	uint16_t domain;
	uint8_t minbus;
	uint8_t maxbus;
};

static uint32_t	pci_docfgregread(int domain, int bus, int slot, int func,
		    int reg, int bytes);
static struct pcie_mcfg_region *pcie_lookup_region(int domain, int bus);
static uint32_t	pciereg_cfgread(struct pcie_mcfg_region *region, int bus,
		    unsigned slot, unsigned func, unsigned reg, unsigned bytes);
static void	pciereg_cfgwrite(struct pcie_mcfg_region *region, int bus,
		    unsigned slot, unsigned func, unsigned reg, int data,
		    unsigned bytes);
static uint32_t	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);

SYSCTL_DECL(_hw_pci);

/*
 * For amd64 we assume that type 1 I/O port-based access always works.
 * If an ACPI MCFG table exists, pcie_cfgregopen() will be called to
 * switch to memory-mapped access.
 */
int cfgmech = CFGMECH_1;

static struct pcie_mcfg_region *mcfg_regions;
static int mcfg_numregions;
static uint32_t pcie_badslots;
static struct mtx pcicfg_mtx;
MTX_SYSINIT(pcicfg_mtx, &pcicfg_mtx, "pcicfg_mtx", MTX_SPIN);

static int mcfg_enable = 1;
SYSCTL_INT(_hw_pci, OID_AUTO, mcfg, CTLFLAG_RDTUN, &mcfg_enable, 0,
    "Enable support for PCI-e memory mapped config access");

int
pci_cfgregopen(void)
{
	/* Stub kept for ABI compatibility; nothing to initialize here. */
	return (1);
}

static struct pcie_mcfg_region *
pcie_lookup_region(int domain, int bus)
{
	for (int i = 0; i < mcfg_numregions; i++) {
		if (mcfg_regions[i].domain == domain &&
		    bus >= mcfg_regions[i].minbus &&
		    bus <= mcfg_regions[i].maxbus)
			return (&mcfg_regions[i]);
	}
	return (NULL);
}

static uint32_t
pci_docfgregread(int domain, int bus, int slot, int func, int reg, int bytes)
{
	/* If a slot on bus 0 is known bad for MMCONFIG, force IO access. */
	if (domain == 0 && bus == 0 && (1U << slot & pcie_badslots) != 0)
		return (pcireg_cfgread(bus, slot, func, reg, bytes));

	if (cfgmech == CFGMECH_PCIE) {
		struct pcie_mcfg_region *region;

		region = pcie_lookup_region(domain, bus);
		if (region != NULL)
			return (pciereg_cfgread(region, bus, slot, func, reg,
			    bytes));
	}

	if (domain == 0)
		return (pcireg_cfgread(bus, slot, func, reg, bytes));
	else
		return ((uint32_t)-1);
}

/*
 * Read configuration space register
 */
u_int32_t
pci_cfgregread(int domain, int bus, int slot, int func, int reg, int bytes)
{
	uint32_t line;

	/*
	 * Some BIOS writers put 0 in the intline rather than 255 to indicate
	 * "none".  Some use values >= 128 which are also considered bogus.
	 * Normalize those to PCI_INVALID_IRQ which the rest of the PCI code
	 * understands as "no IRQ".
	 */
	if (reg == PCIR_INTLINE && bytes == 1) {
		line = pci_docfgregread(domain, bus, slot, func, PCIR_INTLINE,
		    1);
		if (line == 0 || line >= 128)
			line = PCI_INVALID_IRQ;
		return (line);
	}
	return (pci_docfgregread(domain, bus, slot, func, reg, bytes));
}

/*
 * Write configuration space register
 */
void
pci_cfgregwrite(int domain, int bus, int slot, int func, int reg, uint32_t data,
    int bytes)
{
	/* If a slot on bus 0 is known bad for MMCONFIG, force IO access. */
	if (domain == 0 && bus == 0 && (1U << slot & pcie_badslots) != 0) {
		pcireg_cfgwrite(bus, slot, func, reg, data, bytes);
		return;
	}

	if (cfgmech == CFGMECH_PCIE) {
		struct pcie_mcfg_region *region;

		region = pcie_lookup_region(domain, bus);
		if (region != NULL) {
			pciereg_cfgwrite(region, bus, slot, func, reg, data,
			    bytes);
			return;
		}
	}

	if (domain == 0)
		pcireg_cfgwrite(bus, slot, func, reg, data, bytes);
}

/*
 * Configuration space access using direct register operations
 */

/* enable configuration space accesses and return data port address */
static int
pci_cfgenable(unsigned bus, unsigned slot, unsigned func, int reg, int bytes)
{
	int dataport = 0;

	/* Accept only 1, 2 or 4 byte accesses, aligned within the register. */
	if ((bytes == 1 || bytes == 2 || bytes == 4) &&
	    bus <= PCI_BUSMAX && slot <= PCI_SLOTMAX && func <= PCI_FUNCMAX &&
	    (unsigned)reg <= PCI_REGMAX && (reg & (bytes - 1)) == 0) {
		outl(CONF1_ADDR_PORT, (1U << 31) | (bus << 16) | (slot << 11)
		    | (func << 8) | (reg & ~0x03));
		dataport = CONF1_DATA_PORT + (reg & 0x03);
	}
	return (dataport);
}

/* disable configuration space accesses */
static void
pci_cfgdisable(void)
{

	/*
	 * Do nothing.  Writing a 0 to the address port can apparently
	 * confuse some bridges and cause spurious access failures.
	 */
}

static uint32_t
pcireg_cfgread(int bus, int slot, int func, int reg, int bytes)
{
	uint32_t data = (uint32_t)-1;
	int port;

	mtx_lock_spin(&pcicfg_mtx);
	port = pci_cfgenable(bus, slot, func, reg, bytes);
	if (port != 0) {
		switch (bytes) {
		case 1:
			data = inb(port) & 0xff;
			break;
		case 2:
			data = inw(port) & 0xffff;
			break;
		case 4:
			data = inl(port);
			break;
		}
		pci_cfgdisable();
	}
	mtx_unlock_spin(&pcicfg_mtx);
	return (data);
}

static void
pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
	int port;

	mtx_lock_spin(&pcicfg_mtx);
	port = pci_cfgenable(bus, slot, func, reg, bytes);
	if (port != 0) {
		switch (bytes) {
		case 1:
			outb(port, data & 0xff);
			break;
		case 2:
			outw(port, data & 0xffff);
			break;
		case 4:
			outl(port, data);
			break;
		}
		pci_cfgdisable();
	}
	mtx_unlock_spin(&pcicfg_mtx);
}

static void
pcie_init_badslots(struct pcie_mcfg_region *region)
{
	uint32_t val1, val2;
	int slot;

	/*
	 * On some AMD systems, some of the devices on bus 0 are
	 * inaccessible using memory-mapped PCI config access.  Walk
	 * bus 0 looking for such devices.  For these devices, we will
	 * fall back to using type 1 config access instead.
	 */
	if (pci_cfgregopen() != 0) {
		for (slot = 0; slot <= PCI_SLOTMAX; slot++) {
			val1 = pcireg_cfgread(0, slot, 0, 0, 4);
			if (val1 == (uint32_t)-1 || val1 == 0xffffffffu)
				continue;

			val2 = pciereg_cfgread(region, 0, slot, 0, 0, 4);
			if (val2 != val1)
				pcie_badslots |= (1U << slot);
		}
	}
}

int
pcie_cfgregopen(uint64_t base, uint16_t domain, uint8_t minbus, uint8_t maxbus)
{
	struct pcie_mcfg_region *region;

	if (!mcfg_enable)
		return (0);

	if (bootverbose)
		printf("PCI: MCFG domain %u bus %u-%u base @ 0x%lx\n",
		    domain, minbus, maxbus, (unsigned long)base);

	/* Resize the array. */
	mcfg_regions = realloc(mcfg_regions,
	    sizeof(*mcfg_regions) * (mcfg_numregions + 1), M_DEVBUF, M_WAITOK);

	region = &mcfg_regions[mcfg_numregions];

	/* Map the MMCONFIG region for the bus range. */
	region->base = pmap_mapdev_pciecfg(base + (minbus << 20),
	    (maxbus + 1 - minbus) << 20);
	region->domain = domain;
	region->minbus = minbus;
	region->maxbus = maxbus;
	mcfg_numregions++;

	cfgmech = CFGMECH_PCIE;

	/* On domain 0, bus 0, check for devices that need IO access. */
	if (domain == 0 && minbus == 0)
		pcie_init_badslots(region);

	return (1);
}

#define PCIE_VADDR(base, reg, bus, slot, func)	\
	((base)				+	\
	((((bus) & 0xff) << 20)		|	\
	(((slot) & 0x1f) << 15)		|	\
	(((func) & 0x7) << 12)		|	\
	((reg) & 0xfff)))

/*
 * AMD BIOS And Kernel Developer's Guides for CPU families starting with 10h
 * require that all accesses to the memory mapped PCI configuration space
 * are done using AX-class registers.  Use inline asm to ensure the correct
 * register class is used (this is the recommended pattern on AMD systems).
 */

static uint32_t
pciereg_cfgread(struct pcie_mcfg_region *region, int bus, unsigned slot,
    unsigned func, unsigned reg, unsigned bytes)
{
	char *va;
	uint32_t data = (uint32_t)-1;

	MPASS(bus >= region->minbus && bus <= region->maxbus);

	if (slot > PCI_SLOTMAX || func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return ((uint32_t)-1);

	va = PCIE_VADDR(region->base, reg, bus - region->minbus, slot, func);

	/*
	 * Use explicit loads with asm to ensure the AX-family register
	 * usage required by certain AMD processors.
	 */
	switch (bytes) {
	case 4:
		__asm("movl %1, %0" : "=a" (data)
		    : "m" (*(volatile uint32_t *)va));
		break;
	case 2:
		__asm("movzwl %1, %0" : "=a" (data)
		    : "m" (*(volatile uint16_t *)va));
		break;
	case 1:
		__asm("movzbl %1, %0" : "=a" (data)
		    : "m" (*(volatile uint8_t *)va));
		break;
	default:
		/* unsupported size */
		data = (uint32_t)-1;
		break;
	}

	return (data);
}

static void
pciereg_cfgwrite(struct pcie_mcfg_region *region, int bus, unsigned slot,
    unsigned func, unsigned reg, int data, unsigned bytes)
{
	char *va;

	MPASS(bus >= region->minbus && bus <= region->maxbus);

	if (slot > PCI_SLOTMAX || func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return;

	va = PCIE_VADDR(region->base, reg, bus - region->minbus, slot, func);

	/*
	 * Use explicit stores with asm to ensure the AX-family register
	 * usage required by certain AMD processors.
	 */
	switch (bytes) {
	case 4:
		__asm("movl %1, %0" : "=m" (*(volatile uint32_t *)va)
		    : "a" (data));
		break;
	case 2:
		__asm("movw %1, %0" : "=m" (*(volatile uint16_t *)va)
		    : "a" ((uint16_t)data));
		break;
	case 1:
		__asm("movb %1, %0" : "=m" (*(volatile uint8_t *)va)
		    : "a" ((uint8_t)data));
		break;
	default:
		/* unsupported size */
		break;
	}
}
