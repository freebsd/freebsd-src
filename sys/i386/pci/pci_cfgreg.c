/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_xbox.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/pci_cfgreg.h>
#include <machine/pc/bios.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/pmap.h>

#ifdef XBOX
#include <machine/xbox.h>
#endif

#define PRVERB(a) do {							\
	if (bootverbose)						\
		printf a ;						\
} while(0)

#define PCIE_CACHE 8
struct pcie_cfg_elem {
	TAILQ_ENTRY(pcie_cfg_elem)	elem;
	vm_offset_t	vapage;
	vm_paddr_t	papage;
};

enum {
	CFGMECH_NONE = 0,
	CFGMECH_1,
	CFGMECH_2,
	CFGMECH_PCIE,
};

static TAILQ_HEAD(pcie_cfg_list, pcie_cfg_elem) pcie_list[MAXCPU];
static uint32_t pciebar;
static int cfgmech;
static int devmax;
static struct mtx pcicfg_mtx;

static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
#ifndef XEN
static int	pcireg_cfgopen(void);

static int	pciereg_cfgopen(void);
#endif
static int	pciereg_cfgread(int bus, int slot, int func, int reg,
				int bytes);
static void	pciereg_cfgwrite(int bus, int slot, int func, int reg,
				 int data, int bytes);

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

#ifndef XEN
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
#endif

/* 
 * Initialise access to PCI configuration space 
 */
int
pci_cfgregopen(void)
{
#ifdef XEN
	return (0);
#else
	static int		opened = 0;
	u_int16_t		vid, did;
	u_int16_t		v;

	if (opened)
		return(1);

	if (pcireg_cfgopen() == 0)
		return(0);

	v = pcibios_get_version();
	if (v > 0)
		PRVERB(("pcibios: BIOS version %x.%02x\n", (v & 0xff00) >> 8,
		    v & 0xff));
	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;

	/* $PIR requires PCI BIOS 2.10 or greater. */
	if (v >= 0x0210)
		pci_pir_open();

	/*
	 * Grope around in the PCI config space to see if this is a
	 * chipset that is capable of doing memory-mapped config cycles.
	 * This also implies that it can do PCIe extended config cycles.
	 */

	/* Check for supported chipsets */
	vid = pci_cfgregread(0, 0, 0, PCIR_VENDOR, 2);
	did = pci_cfgregread(0, 0, 0, PCIR_DEVICE, 2);
	switch (vid) {
	case 0x8086:
		switch (did) {
		case 0x3590:
		case 0x3592:
			/* Intel 7520 or 7320 */
			pciebar = pci_cfgregread(0, 0, 0, 0xce, 2) << 16;
			pciereg_cfgopen();
			break;
		case 0x2580:
		case 0x2584:
			/* Intel 915 or 925 */
			pciebar = pci_cfgregread(0, 0, 0, 0x48, 4);
			pciereg_cfgopen();
			break;
		}
	}

	return(1);
#endif
}

/* 
 * Read configuration space register
 */
u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
	uint32_t line;

	/*
	 * Some BIOS writers seem to want to ignore the spec and put
	 * 0 in the intline rather than 255 to indicate none.  The rest of
	 * the code uses 255 as an invalid IRQ.
	 */
	if (reg == PCIR_INTLINE && bytes == 1) {
		line = pcireg_cfgread(bus, slot, func, PCIR_INTLINE, 1);
		return (pci_i386_map_intline(line));
	}
	return (pcireg_cfgread(bus, slot, func, reg, bytes));
}

/* 
 * Write configuration space register 
 */
void
pci_cfgregwrite(int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{

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

#ifdef XBOX
	if (arch_i386_is_xbox) {
		/*
		 * The Xbox MCPX chipset is a derivative of the nForce 1
		 * chipset. It almost has the same bus layout; some devices
		 * cannot be used, because they have been removed.
		 */

		/*
		 * Devices 00:00.1 and 00:00.2 used to be memory controllers on
		 * the nForce chipset, but on the Xbox, using them will lockup
		 * the chipset.
		 */
		if (bus == 0 && slot == 0 && (func == 1 || func == 2))
			return dataport;
		
		/*
		 * Bus 1 only contains a VGA controller at 01:00.0. When you try
		 * to probe beyond that device, you only get garbage, which
		 * could cause lockups.
		 */
		if (bus == 1 && (slot != 0 || func != 0))
			return dataport;
		
		/*
		 * Bus 2 used to contain the AGP controller, but the Xbox MCPX
		 * doesn't have one. Probing it can cause lockups.
		 */
		if (bus >= 2)
			return dataport;
	}
#endif

	if (bus <= PCI_BUSMAX
	    && slot < devmax
	    && func <= PCI_FUNCMAX
	    && reg <= PCI_REGMAX
	    && bytes != 3
	    && (unsigned) bytes <= 4
	    && (reg & (bytes - 1)) == 0) {
		switch (cfgmech) {
		case CFGMECH_1:
			outl(CONF1_ADDR_PORT, (1 << 31)
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

	if (cfgmech == CFGMECH_PCIE) {
		data = pciereg_cfgread(bus, slot, func, reg, bytes);
		return (data);
	}

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

	if (cfgmech == CFGMECH_PCIE) {
		pciereg_cfgwrite(bus, slot, func, reg, data, bytes);
		return;
	}

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

#ifndef XEN
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

static int
pciereg_cfgopen(void)
{
	struct pcie_cfg_list *pcielist;
	struct pcie_cfg_elem *pcie_array, *elem;
#ifdef SMP
	struct pcpu *pc;
#endif
	vm_offset_t va;
	int i;

	if (bootverbose)
		printf("Setting up PCIe mappings for BAR 0x%x\n", pciebar);

#ifdef SMP
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu)
#endif
	{

		pcie_array = malloc(sizeof(struct pcie_cfg_elem) * PCIE_CACHE,
		    M_DEVBUF, M_NOWAIT);
		if (pcie_array == NULL)
			return (0);

		va = kmem_alloc_nofault(kernel_map, PCIE_CACHE * PAGE_SIZE);
		if (va == 0) {
			free(pcie_array, M_DEVBUF);
			return (0);
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

	
	cfgmech = CFGMECH_PCIE;
	devmax = 32;
	return (1);
}
#endif /* !XEN */

#define PCIE_PADDR(bar, reg, bus, slot, func)	\
	((bar)				|	\
	(((bus) & 0xff) << 20)		|	\
	(((slot) & 0x1f) << 15)		|	\
	(((func) & 0x7) << 12)		|	\
	((reg) & 0xfff))

/*
 * Find an element in the cache that matches the physical page desired, or
 * create a new mapping from the least recently used element.
 * A very simple LRU algorithm is used here, does it need to be more
 * efficient?
 */
static __inline struct pcie_cfg_elem *
pciereg_findelem(vm_paddr_t papage)
{
	struct pcie_cfg_list *pcielist;
	struct pcie_cfg_elem *elem;

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
	return (elem);
}

static int
pciereg_cfgread(int bus, int slot, int func, int reg, int bytes)
{
	struct pcie_cfg_elem *elem;
	volatile vm_offset_t va;
	vm_paddr_t pa, papage;
	int data;

	critical_enter();
	pa = PCIE_PADDR(pciebar, reg, bus, slot, func);
	papage = pa & ~PAGE_MASK;
	elem = pciereg_findelem(papage);
	va = elem->vapage | (pa & PAGE_MASK);

	switch (bytes) {
	case 4:
		data = *(volatile uint32_t *)(va);
		break;
	case 2:
		data = *(volatile uint16_t *)(va);
		break;
	case 1:
		data = *(volatile uint8_t *)(va);
		break;
	default:
		panic("pciereg_cfgread: invalid width");
	}

	critical_exit();
	return (data);
}

static void
pciereg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
	struct pcie_cfg_elem *elem;
	volatile vm_offset_t va;
	vm_paddr_t pa, papage;

	critical_enter();
	pa = PCIE_PADDR(pciebar, reg, bus, slot, func);
	papage = pa & ~PAGE_MASK;
	elem = pciereg_findelem(papage);
	va = elem->vapage | (pa & PAGE_MASK);

	switch (bytes) {
	case 4:
		*(volatile uint32_t *)(va) = data;
		break;
	case 2:
		*(volatile uint16_t *)(va) = data;
		break;
	case 1:
		*(volatile uint8_t *)(va) = data;
		break;
	default:
		panic("pciereg_cfgwrite: invalid width");
	}

	critical_exit();
}
