/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * Copyright (c) 2004, Scott Long <scottl@freebsd.org>
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
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/pci_cfgreg.h>
#include <machine/pc/bios.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#define PRVERB(a) do {							\
	if (bootverbose)						\
		printf a ;						\
} while(0)

struct pcie_mcfg_region {
	uint64_t base;
	uint16_t domain;
	uint8_t minbus;
	uint8_t maxbus;
};

#define PCIE_CACHE 8
struct pcie_cfg_elem {
	TAILQ_ENTRY(pcie_cfg_elem)	elem;
	vm_offset_t	vapage;
	vm_paddr_t	papage;
};

SYSCTL_DECL(_hw_pci);

static struct pcie_mcfg_region *mcfg_regions;
static int mcfg_numregions;
static TAILQ_HEAD(pcie_cfg_list, pcie_cfg_elem) pcie_list[MAXCPU];
static int pcie_cache_initted;
static uint32_t pcie_badslots;
int cfgmech;
static int devmax;
static struct mtx pcicfg_mtx;

static int mcfg_enable = 1;
SYSCTL_INT(_hw_pci, OID_AUTO, mcfg, CTLFLAG_RDTUN, &mcfg_enable, 0,
    "Enable support for PCI-e memory mapped config access");

static uint32_t	pci_docfgregread(int domain, int bus, int slot, int func,
		    int reg, int bytes);
static struct pcie_mcfg_region *pcie_lookup_region(int domain, int bus);
static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcireg_cfgopen(void);
static int	pciereg_cfgread(struct pcie_mcfg_region *region, int bus,
		    unsigned slot, unsigned func, unsigned reg, unsigned bytes);
static void	pciereg_cfgwrite(struct pcie_mcfg_region *region, int bus,
		    unsigned slot, unsigned func, unsigned reg, int data,
		    unsigned bytes);

/*
 * Some BIOS writers seem to want to ignore the spec and put
 * 0 in the intline rather than 255 to indicate none.  Some use
 * numbers in the range 128-254 to indicate something strange and
 * apparently undocumented anywhere.  Assume these are completely bogus
 * and map them to 255, which means "none".
 */
static __inline int 
pci_i386_map_intline(int line)
{
	if (line == 0 || line >= 128)
		return (PCI_INVALID_IRQ);
	return (line);
}

static u_int16_t
pcibios_get_version(void)
{
	struct bios_regs args;

	if (PCIbios.ventry == 0) {
		PRVERB(("pcibios: No call entry point\n"));
		return (0);
	}
	args.eax = PCIBIOS_BIOS_PRESENT;
	if (bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL))) {
		PRVERB(("pcibios: BIOS_PRESENT call failed\n"));
		return (0);
	}
	if (args.edx != 0x20494350) {
		PRVERB(("pcibios: BIOS_PRESENT didn't return 'PCI ' in edx\n"));
		return (0);
	}
	return (args.ebx & 0xffff);
}

/* 
 * Initialise access to PCI configuration space 
 */
int
pci_cfgregopen(void)
{
	uint16_t v;
	static int opened = 0;

	if (opened)
		return (1);

	if (cfgmech == CFGMECH_NONE && pcireg_cfgopen() == 0)
		return (0);

	v = pcibios_get_version();
	if (v > 0)
		PRVERB(("pcibios: BIOS version %x.%02x\n", (v & 0xff00) >> 8,
		    v & 0xff));
	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;

	/* $PIR requires PCI BIOS 2.10 or greater. */
	if (v >= 0x0210)
		pci_pir_open();

	return (1);
}

static struct pcie_mcfg_region *
pcie_lookup_region(int domain, int bus)
{
	for (int i = 0; i < mcfg_numregions; i++)
		if (mcfg_regions[i].domain == domain &&
		    bus >= mcfg_regions[i].minbus &&
		    bus <= mcfg_regions[i].maxbus)
			return (&mcfg_regions[i]);
	return (NULL);
}

static uint32_t
pci_docfgregread(int domain, int bus, int slot, int func, int reg, int bytes)
{
	if (domain == 0 && bus == 0 && (1 << slot & pcie_badslots) != 0)
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
		return (-1);
}

/* 
 * Read configuration space register
 */
u_int32_t
pci_cfgregread(int domain, int bus, int slot, int func, int reg, int bytes)
{
	uint32_t line;

	/*
	 * Some BIOS writers seem to want to ignore the spec and put
	 * 0 in the intline rather than 255 to indicate none.  The rest of
	 * the code uses 255 as an invalid IRQ.
	 */
	if (reg == PCIR_INTLINE && bytes == 1) {
		line = pci_docfgregread(domain, bus, slot, func, PCIR_INTLINE,
		    1);
		return (pci_i386_map_intline(line));
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
	if (domain == 0 && bus == 0 && (1 << slot & pcie_badslots) != 0) {
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

	if (bus <= PCI_BUSMAX
	    && slot < devmax
	    && func <= PCI_FUNCMAX
	    && (unsigned)reg <= PCI_REGMAX
	    && bytes != 3
	    && (unsigned)bytes <= 4
	    && (reg & (bytes - 1)) == 0) {
		switch (cfgmech) {
		case CFGMECH_PCIE:
		case CFGMECH_1:
			outl(CONF1_ADDR_PORT, (1U << 31)
			    | (bus << 16) | (slot << 11) 
			    | (func << 8) | (reg & ~0x03));
			dataport = CONF1_DATA_PORT + (reg & 0x03);
			break;
		case CFGMECH_2:
			outb(CONF2_ENABLE_PORT, 0xf0 | (func << 1));
			outb(CONF2_FORWARD_PORT, bus);
			dataport = 0xc000 | (slot << 8) | reg;
			break;
		}
	}
	return (dataport);
}

/* disable configuration space accesses */
static void
pci_cfgdisable(void)
{
	switch (cfgmech) {
	case CFGMECH_PCIE:
	case CFGMECH_1:
		/*
		 * Do nothing for the config mechanism 1 case.
		 * Writing a 0 to the address port can apparently
		 * confuse some bridges and cause spurious
		 * access failures.
		 */
		break;
	case CFGMECH_2:
		outb(CONF2_ENABLE_PORT, 0);
		break;
	}
}

static int
pcireg_cfgread(int bus, int slot, int func, int reg, int bytes)
{
	int data = -1;
	int port;

	mtx_lock_spin(&pcicfg_mtx);
	port = pci_cfgenable(bus, slot, func, reg, bytes);
	if (port != 0) {
		switch (bytes) {
		case 1:
			data = inb(port);
			break;
		case 2:
			data = inw(port);
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
			outb(port, data);
			break;
		case 2:
			outw(port, data);
			break;
		case 4:
			outl(port, data);
			break;
		}
		pci_cfgdisable();
	}
	mtx_unlock_spin(&pcicfg_mtx);
}

/* check whether the configuration mechanism has been correctly identified */
static int
pci_cfgcheck(int maxdev)
{
	uint32_t id, class;
	uint8_t header;
	uint8_t device;
	int port;

	if (bootverbose) 
		printf("pci_cfgcheck:\tdevice ");

	for (device = 0; device < maxdev; device++) {
		if (bootverbose) 
			printf("%d ", device);

		port = pci_cfgenable(0, device, 0, 0, 4);
		id = inl(port);
		if (id == 0 || id == 0xffffffff)
			continue;

		port = pci_cfgenable(0, device, 0, 8, 4);
		class = inl(port) >> 8;
		if (bootverbose)
			printf("[class=%06x] ", class);
		if (class == 0 || (class & 0xf870ff) != 0)
			continue;

		port = pci_cfgenable(0, device, 0, 14, 1);
		header = inb(port);
		if (bootverbose)
			printf("[hdr=%02x] ", header);
		if ((header & 0x7e) != 0)
			continue;

		if (bootverbose)
			printf("is there (id=%08x)\n", id);

		pci_cfgdisable();
		return (1);
	}
	if (bootverbose) 
		printf("-- nothing found\n");

	pci_cfgdisable();
	return (0);
}

static int
pcireg_cfgopen(void)
{
	uint32_t mode1res, oldval1;
	uint8_t mode2res, oldval2;

	/* Check for type #1 first. */
	oldval1 = inl(CONF1_ADDR_PORT);

	if (bootverbose) {
		printf("pci_open(1):\tmode 1 addr port (0x0cf8) is 0x%08x\n",
		    oldval1);
	}

	cfgmech = CFGMECH_1;
	devmax = 32;

	outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
	DELAY(1);
	mode1res = inl(CONF1_ADDR_PORT);
	outl(CONF1_ADDR_PORT, oldval1);

	if (bootverbose)
		printf("pci_open(1a):\tmode1res=0x%08x (0x%08lx)\n",  mode1res,
		    CONF1_ENABLE_CHK);

	if (mode1res) {
		if (pci_cfgcheck(32)) 
			return (cfgmech);
	}

	outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
	mode1res = inl(CONF1_ADDR_PORT);
	outl(CONF1_ADDR_PORT, oldval1);

	if (bootverbose)
		printf("pci_open(1b):\tmode1res=0x%08x (0x%08lx)\n",  mode1res,
		    CONF1_ENABLE_CHK1);

	if ((mode1res & CONF1_ENABLE_MSK1) == CONF1_ENABLE_RES1) {
		if (pci_cfgcheck(32)) 
			return (cfgmech);
	}

	/* Type #1 didn't work, so try type #2. */
	oldval2 = inb(CONF2_ENABLE_PORT);

	if (bootverbose) {
		printf("pci_open(2):\tmode 2 enable port (0x0cf8) is 0x%02x\n",
		    oldval2);
	}

	if ((oldval2 & 0xf0) == 0) {
		cfgmech = CFGMECH_2;
		devmax = 16;

		outb(CONF2_ENABLE_PORT, CONF2_ENABLE_CHK);
		mode2res = inb(CONF2_ENABLE_PORT);
		outb(CONF2_ENABLE_PORT, oldval2);

		if (bootverbose)
			printf("pci_open(2a):\tmode2res=0x%02x (0x%02x)\n", 
			    mode2res, CONF2_ENABLE_CHK);

		if (mode2res == CONF2_ENABLE_RES) {
			if (bootverbose)
				printf("pci_open(2a):\tnow trying mechanism 2\n");

			if (pci_cfgcheck(16)) 
				return (cfgmech);
		}
	}

	/* Nothing worked, so punt. */
	cfgmech = CFGMECH_NONE;
	devmax = 0;
	return (cfgmech);
}

static bool
pcie_init_cache(void)
{
	struct pcie_cfg_list *pcielist;
	struct pcie_cfg_elem *pcie_array, *elem;
#ifdef SMP
	struct pcpu *pc;
#endif
	vm_offset_t va;
	int i;

#ifdef SMP
	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu)
#endif
	{
		pcie_array = malloc(sizeof(struct pcie_cfg_elem) * PCIE_CACHE,
		    M_DEVBUF, M_NOWAIT);
		if (pcie_array == NULL)
			return (false);

		va = kva_alloc(PCIE_CACHE * PAGE_SIZE);
		if (va == 0) {
			free(pcie_array, M_DEVBUF);
			return (false);
		}

#ifdef SMP
		pcielist = &pcie_list[pc->pc_cpuid];
#else
		pcielist = &pcie_list[0];
#endif
		TAILQ_INIT(pcielist);
		for (i = 0; i < PCIE_CACHE; i++) {
			elem = &pcie_array[i];
			elem->vapage = va + (i * PAGE_SIZE);
			elem->papage = 0;
			TAILQ_INSERT_HEAD(pcielist, elem, elem);
		}
	}
	return (true);
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
			if (val1 == 0xffffffff)
				continue;

			val2 = pciereg_cfgread(region, 0, slot, 0, 0, 4);
			if (val2 != val1)
				pcie_badslots |= (1 << slot);
		}
	}
}

int
pcie_cfgregopen(uint64_t base, uint16_t domain, uint8_t minbus, uint8_t maxbus)
{
	struct pcie_mcfg_region *region;

	if (!mcfg_enable)
		return (0);

	if (!pae_mode && base >= 0x100000000) {
		if (bootverbose)
			printf(
	    "PCI: MCFG domain %u bus %u-%u base 0x%jx too high\n",
			domain, minbus, maxbus, (uintmax_t)base);
		return (0);
	}

	if (bootverbose)
		printf("PCI: MCFG domain %u bus %u-%u base @ 0x%jx\n",
		    domain, minbus, maxbus, (uintmax_t)base);

	if (pcie_cache_initted == 0) {
		if (!pcie_init_cache())
			pcie_cache_initted = -1;
		else
			pcie_cache_initted = 1;
	}

	if (pcie_cache_initted == -1)
		return (0);

	/* Resize the array. */
	mcfg_regions = realloc(mcfg_regions,
	    sizeof(*mcfg_regions) * (mcfg_numregions + 1), M_DEVBUF, M_WAITOK);

	region = &mcfg_regions[mcfg_numregions];
	region->base = base + (minbus << 20);
	region->domain = domain;
	region->minbus = minbus;
	region->maxbus = maxbus;
	mcfg_numregions++;

	cfgmech = CFGMECH_PCIE;
	devmax = 32;

	if (domain == 0 && minbus == 0)
		pcie_init_badslots(region);

	return (1);
}

#define PCIE_PADDR(base, reg, bus, slot, func)	\
	((base)				+	\
	((((bus) & 0xff) << 20)		|	\
	(((slot) & 0x1f) << 15)		|	\
	(((func) & 0x7) << 12)		|	\
	((reg) & 0xfff)))

static __inline vm_offset_t
pciereg_findaddr(struct pcie_mcfg_region *region, int bus, unsigned slot,
    unsigned func, unsigned reg)
{
	struct pcie_cfg_list *pcielist;
	struct pcie_cfg_elem *elem;
	vm_paddr_t pa, papage;

	MPASS(bus >= region->minbus && bus <= region->maxbus);

	pa = PCIE_PADDR(region->base, reg, bus - region->minbus, slot, func);
	papage = pa & ~PAGE_MASK;

	/*
	 * Find an element in the cache that matches the physical page desired,
	 * or create a new mapping from the least recently used element.
	 * A very simple LRU algorithm is used here, does it need to be more
	 * efficient?
	 */
	pcielist = &pcie_list[PCPU_GET(cpuid)];
	TAILQ_FOREACH(elem, pcielist, elem) {
		if (elem->papage == papage)
			break;
	}

	if (elem == NULL) {
		elem = TAILQ_LAST(pcielist, pcie_cfg_list);
		if (elem->papage != 0) {
			pmap_kremove(elem->vapage);
			invlpg(elem->vapage);
		}
		pmap_kenter(elem->vapage, papage);
		elem->papage = papage;
	}

	if (elem != TAILQ_FIRST(pcielist)) {
		TAILQ_REMOVE(pcielist, elem, elem);
		TAILQ_INSERT_HEAD(pcielist, elem, elem);
	}
	return (elem->vapage | (pa & PAGE_MASK));
}

/*
 * AMD BIOS And Kernel Developer's Guides for CPU families starting with 10h
 * have a requirement that all accesses to the memory mapped PCI configuration
 * space are done using AX class of registers.
 * Since other vendors do not currently have any contradicting requirements
 * the AMD access pattern is applied universally.
 */

static int
pciereg_cfgread(struct pcie_mcfg_region *region, int bus, unsigned slot,
    unsigned func, unsigned reg, unsigned bytes)
{
	vm_offset_t va;
	int data = -1;

	if (slot > PCI_SLOTMAX || func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return (-1);

	critical_enter();
	va = pciereg_findaddr(region, bus, slot, func, reg);

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
	}

	critical_exit();
	return (data);
}

static void
pciereg_cfgwrite(struct pcie_mcfg_region *region, int bus, unsigned slot,
    unsigned func, unsigned reg, int data, unsigned bytes)
{
	vm_offset_t va;

	if (slot > PCI_SLOTMAX || func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return;

	critical_enter();
	va = pciereg_findaddr(region, bus, slot, func, reg);

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
	}

	critical_exit();
}
