/*
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
 *
 * $FreeBSD$
 *
 */

#include <sys/param.h>		/* XXX trim includes */
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <isa/isavar.h>
#include <machine/nexusvar.h>
#include <machine/pci_cfgreg.h>
#include <machine/segments.h>
#include <machine/pc/bios.h>

#include "pcib_if.h"

static int cfgmech;
static int devmax;
static int usebios;

static int	pcibios_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcibios_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcibios_cfgopen(void);
static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcireg_cfgopen(void);

static struct PIR_entry	*pci_route_table;
static int		pci_route_count;

/* 
 * Initialise access to PCI configuration space 
 */
int
pci_cfgregopen(void)
{
    static int			opened = 0;
    u_long			sigaddr;
    static struct PIR_table	*pt;
    u_int8_t			ck, *cv;
    int				i;

    if (opened)
	return(1);

    if (pcibios_cfgopen() != 0) {
	usebios = 1;
    } else if (pcireg_cfgopen() != 0) {
	usebios = 0;
    } else {
	return(0);
    }

    /*
     * Look for the interrupt routing table.
     */
    /* XXX use PCI BIOS if it's available */

    if ((pt == NULL) && ((sigaddr = bios_sigsearch(0, "$PIR", 4, 16, 0)) != 0)) {
	pt = (struct PIR_table *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)pt, ck = 0, i = 0; i < (pt->pt_header.ph_length); i++) {
	    ck += cv[i];
	}
	if (ck == 0) {
	    pci_route_table = &pt->pt_entry[0];
	    pci_route_count = (pt->pt_header.ph_length - sizeof(struct PIR_header)) / sizeof(struct PIR_entry);
	    printf("Using $PIR table, %d entries at %p\n", pci_route_count, pci_route_table);
	}
    }

    opened = 1;
    return(1);
}

/* 
 * Read configuration space register 
 */
u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
    return(usebios ? 
	   pcibios_cfgread(bus, slot, func, reg, bytes) : 
	   pcireg_cfgread(bus, slot, func, reg, bytes));
}

/* 
 * Write configuration space register 
 */
void
pci_cfgregwrite(int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{
    return(usebios ? 
	   pcibios_cfgwrite(bus, slot, func, reg, data, bytes) : 
	   pcireg_cfgwrite(bus, slot, func, reg, data, bytes));
}

/*
 * Route a PCI interrupt
 *
 * XXX this needs to learn to actually route uninitialised interrupts as well
 *     as just returning interrupts for stuff that's already initialised.
 *
 * XXX we don't do anything "right" with the function number in the PIR table
 *     (because the consumer isn't currently passing it in).
 */
int
pci_cfgintr(int bus, int device, int pin)
{
    struct PIR_entry	*pe;
    int			i, irq;
    struct bios_regs	args;
    
    if ((bus < 0) || (bus > 255) || (device < 0) || (device > 255) ||
      (pin < 1) || (pin > 4))
	return(255);

    /*
     * Scan the entry table for a contender
     */
    for (i = 0, pe = pci_route_table; i < pci_route_count; i++, pe++) {
	if ((bus != pe->pe_bus) || (device != pe->pe_device))
	    continue;
	if (!powerof2(pe->pe_intpin[pin - 1].irqs)) {
	    printf("pci_cfgintr: %d:%d:%c is not routed to a unique interrupt\n",
		   bus, device, 'A' + pin - 1);
	    break;
	}
	irq = ffs(pe->pe_intpin[pin - 1].irqs) - 1;
	printf("pci_cfgintr: %d:%d:%c routed to irq %d\n", 
	       bus, device, 'A' + pin - 1, irq);

	/*
	 * Ask the BIOS to route the interrupt
	 */
	args.eax = PCIBIOS_ROUTE_INTERRUPT;
	args.ebx = (bus << 8) | (device << 3);
	args.ecx = (irq << 8) | (0xa + pin - 1);	/* pin value is 0xa - 0xd */
	bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL));

	/* XXX if it fails, we should smack the router hardware directly */

	return(irq);
    }
    return(255);
}


/*
 * Config space access using BIOS functions 
 */
static int
pcibios_cfgread(int bus, int slot, int func, int reg, int bytes)
{
    struct bios_regs args;
    u_int mask;

    switch(bytes) {
    case 1:
	args.eax = PCIBIOS_READ_CONFIG_BYTE;
	mask = 0xff;
	break;
    case 2:
	args.eax = PCIBIOS_READ_CONFIG_WORD;
	mask = 0xffff;
	break;
    case 4:
	args.eax = PCIBIOS_READ_CONFIG_DWORD;
	mask = 0xffffffff;
	break;
    default:
	return(-1);
    }
    args.ebx = (bus << 8) | (slot << 3) | func;
    args.edi = reg;
    bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL));
    /* check call results? */
    return(args.ecx & mask);
}

static void
pcibios_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
    struct bios_regs args;

    switch(bytes) {
    case 1:
	args.eax = PCIBIOS_WRITE_CONFIG_BYTE;
	break;
    case 2:
	args.eax = PCIBIOS_WRITE_CONFIG_WORD;
	break;
    case 4:
	args.eax = PCIBIOS_WRITE_CONFIG_DWORD;
	break;
    default:
	return;
    }
    args.ebx = (bus << 8) | (slot << 3) | func;
    args.ecx = data;
    args.edi = reg;
    bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL));
}

/*
 * Determine whether there is a PCI BIOS present
 */
static int
pcibios_cfgopen(void)
{
    /* check for a found entrypoint */
    return(PCIbios.entry != 0);
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
	&& reg <= PCI_REGMAX
	&& bytes != 3
	&& (unsigned) bytes <= 4
	&& (reg & (bytes -1)) == 0) {
	switch (cfgmech) {
	case 1:
	    outl(CONF1_ADDR_PORT, (1 << 31)
		 | (bus << 16) | (slot << 11) 
		 | (func << 8) | (reg & ~0x03));
	    dataport = CONF1_DATA_PORT + (reg & 0x03);
	    break;
	case 2:
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
    case 1:
	outl(CONF1_ADDR_PORT, 0);
	break;
    case 2:
	outb(CONF2_ENABLE_PORT, 0);
	outb(CONF2_FORWARD_PORT, 0);
	break;
    }
}

static int
pcireg_cfgread(int bus, int slot, int func, int reg, int bytes)
{
    int data = -1;
    int port;

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
    return (data);
}

static void
pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes)
{
    int port;

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
}

/* check whether the configuration mechanism has been correctly identified */
static int
pci_cfgcheck(int maxdev)
{
    u_char device;

    if (bootverbose) 
	printf("pci_cfgcheck:\tdevice ");

    for (device = 0; device < maxdev; device++) {
	unsigned id, class, header;
	if (bootverbose) 
	    printf("%d ", device);

	id = inl(pci_cfgenable(0, device, 0, 0, 4));
	if (id == 0 || id == -1)
	    continue;

	class = inl(pci_cfgenable(0, device, 0, 8, 4)) >> 8;
	if (bootverbose)
	    printf("[class=%06x] ", class);
	if (class == 0 || (class & 0xf870ff) != 0)
	    continue;

	header = inb(pci_cfgenable(0, device, 0, 14, 1));
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
    unsigned long mode1res,oldval1;
    unsigned char mode2res,oldval2;

    oldval1 = inl(CONF1_ADDR_PORT);

    if (bootverbose) {
	printf("pci_open(1):\tmode 1 addr port (0x0cf8) is 0x%08lx\n",
	       oldval1);
    }

    if ((oldval1 & CONF1_ENABLE_MSK) == 0) {

	cfgmech = 1;
	devmax = 32;

	outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
	outb(CONF1_ADDR_PORT +3, 0);
	mode1res = inl(CONF1_ADDR_PORT);
	outl(CONF1_ADDR_PORT, oldval1);

	if (bootverbose)
	    printf("pci_open(1a):\tmode1res=0x%08lx (0x%08lx)\n", 
		   mode1res, CONF1_ENABLE_CHK);

	if (mode1res) {
	    if (pci_cfgcheck(32)) 
		return (cfgmech);
	}

	outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
	mode1res = inl(CONF1_ADDR_PORT);
	outl(CONF1_ADDR_PORT, oldval1);

	if (bootverbose)
	    printf("pci_open(1b):\tmode1res=0x%08lx (0x%08lx)\n", 
		   mode1res, CONF1_ENABLE_CHK1);

	if ((mode1res & CONF1_ENABLE_MSK1) == CONF1_ENABLE_RES1) {
	    if (pci_cfgcheck(32)) 
		return (cfgmech);
	}
    }

    oldval2 = inb(CONF2_ENABLE_PORT);

    if (bootverbose) {
	printf("pci_open(2):\tmode 2 enable port (0x0cf8) is 0x%02x\n",
	       oldval2);
    }

    if ((oldval2 & 0xf0) == 0) {

	cfgmech = 2;
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

    cfgmech = 0;
    devmax = 0;
    return (cfgmech);
}

