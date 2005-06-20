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
#include <i386/isa/pcibus.h>
/* #include <machine/nexusvar.h> */
#include <machine/clock.h>
#include <machine/pci_cfgreg.h>
#include <machine/segments.h>
#include <machine/pc/bios.h>

#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */

static int cfgmech;
static int devmax;

#define PRVERB(a) printf a
static int	pci_cfgintr_unique(struct PIR_entry *pe, int pin);
static int	pci_cfgintr_linked(struct PIR_entry *pe, int pin);
static int	pci_cfgintr_search(struct PIR_entry *pe, int bus, int device, int matchpin, int pin);
static int	pci_cfgintr_virgin(struct PIR_entry *pe, int pin);

static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcireg_cfgopen(void);

static struct PIR_table	*pci_route_table;
static int		pci_route_count;

static u_int16_t
pcibios_get_version(void)
{
    struct bios_regs args;

    if (PCIbios.entry == 0) {
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
    static int			opened = 0;
    u_long			sigaddr;
    static struct PIR_table	*pt;
    u_int8_t			ck, *cv;
    int				i;

    if (opened)
	return(1);

    if (pcireg_cfgopen() == 0)
	return(0);

    /*
     * Look for the interrupt routing table.
     */
    /* We use PCI BIOS's PIR table if it's available */
    if (pcibios_get_version() >= 0x0210 && pt == NULL && 
      (sigaddr = bios_sigsearch(0, "$PIR", 4, 16, 0)) != 0) {
	pt = (struct PIR_table *)(uintptr_t)BIOS_PADDRTOVADDR(sigaddr);
	for (cv = (u_int8_t *)pt, ck = 0, i = 0; i < (pt->pt_header.ph_length); i++) {
	    ck += cv[i];
	}
	if (ck == 0) {
	    pci_route_table = pt;
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
static u_int32_t
pci_do_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
    return(pcireg_cfgread(bus, slot, func, reg, bytes));
}

u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
#ifdef APIC_IO
    /*
     * If we are using the APIC, the contents of the intline register will probably
     * be wrong (since they are set up for use with the PIC.
     * Rather than rewrite these registers (maybe that would be smarter) we trap
     * attempts to read them and translate to our private vector numbers.
     */
    if ((reg == PCIR_INTLINE) && (bytes == 1)) {
	int	pin, line;

	pin = pci_do_cfgregread(bus, slot, func, PCIR_INTPIN, 1);
	line = pci_do_cfgregread(bus, slot, func, PCIR_INTLINE, 1);

	if (pin != 0) {
	    int airq;

	    airq = pci_apic_irq(bus, slot, pin, NULL);
	    if (airq >= 0) {
		/* PCI specific entry found in MP table */
		if (airq != line)
		    undirect_pci_irq(line);
		return(airq);
	    } else {
		/* 
		 * PCI interrupts might be redirected to the
		 * ISA bus according to some MP tables. Use the
		 * same methods as used by the ISA devices
		 * devices to find the proper IOAPIC int pin.
		 */
		airq = isa_apic_irq(line);
		if ((airq >= 0) && (airq != line)) {
		    /* XXX: undirect_pci_irq() ? */
		    undirect_isa_irq(line);
		    return(airq);
		}
	    }
	}
	return(line);
    }
#endif /* APIC_IO */
    return(pci_do_cfgregread(bus, slot, func, reg, bytes));
}

/* 
 * Write configuration space register 
 */
void
pci_cfgregwrite(int bus, int slot, int func, int reg, u_int32_t data, int bytes)
{
    return(pcireg_cfgwrite(bus, slot, func, reg, data, bytes));
}

int
pci_cfgread(pcicfgregs *cfg, int reg, int bytes)
{
	return(pci_cfgregread(cfg->bus, cfg->slot, cfg->func, reg, bytes));
}

void
pci_cfgwrite(pcicfgregs *cfg, int reg, int data, int bytes)
{
	pci_cfgregwrite(cfg->bus, cfg->slot, cfg->func, reg, data, bytes);
}


/*
 * Route a PCI interrupt
 *
 * XXX we don't do anything "right" with the function number in the PIR table
 *     (because the consumer isn't currently passing it in).  We don't care
 *     anyway, due to the way PCI interrupts are assigned.
 */
int
pci_cfgintr(int bus, int device, int pin)
{
    struct PIR_entry	*pe;
    int			i, irq;
    struct bios_regs	args;
    u_int16_t		v;
    int already = 0;
    
    v = pcibios_get_version();
    if (v < 0x0210) {
	PRVERB((
	  "pci_cfgintr: BIOS %x.%02x doesn't support interrupt routing\n",
	  (v & 0xff00) >> 8, v & 0xff));
	return (255);
    }
    if ((bus < 0) || (bus > 255) || (device < 0) || (device > 255) ||
      (pin < 1) || (pin > 4))
	return(255);

    /*
     * Scan the entry table for a contender
     */
    for (i = 0, pe = &pci_route_table->pt_entry[0]; i < pci_route_count; i++, pe++) {
	if ((bus != pe->pe_bus) || (device != pe->pe_device))
	    continue;

	irq = pci_cfgintr_linked(pe, pin);
	if (irq != 255)
	     already = 1;
	if (irq == 255)
	    irq = pci_cfgintr_unique(pe, pin);
	if (irq == 255)
	    irq = pci_cfgintr_virgin(pe, pin);

	if (irq == 255)
	    break;

	/*
	 * Ask the BIOS to route the interrupt
	 */
	args.eax = PCIBIOS_ROUTE_INTERRUPT;
	args.ebx = (bus << 8) | (device << 3);
	args.ecx = (irq << 8) | (0xa + pin - 1);	/* pin value is 0xa - 0xd */
	if (bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL)) && !already) {
	    /*
	     * XXX if it fails, we should try to smack the router
	     * hardware directly.
	     * XXX Also, there may be other choices that we can try that
	     * will work.
	     */
	    PRVERB(("pci_cfgintr: ROUTE_INTERRUPT failed.\n"));
	    return(255);
	}
	printf("pci_cfgintr: %d:%d INT%c routed to irq %d\n", bus, device, 'A' + pin - 1, irq);
	return(irq);
    }

    PRVERB(("pci_cfgintr: can't route an interrupt to %d:%d INT%c\n", bus, device, 'A' + pin - 1));
    return(255);
}

/*
 * Look to see if the routing table claims this pin is uniquely routed.
 */
static int
pci_cfgintr_unique(struct PIR_entry *pe, int pin)
{
    int		irq;
    
    if (powerof2(pe->pe_intpin[pin - 1].irqs)) {
	irq = ffs(pe->pe_intpin[pin - 1].irqs) - 1;
	PRVERB(("pci_cfgintr_unique: hard-routed to irq %d\n", irq));
	return(irq);
    }
    return(255);
}

/*
 * Look for another device which shares the same link byte and
 * already has a unique IRQ, or which has had one routed already.
 */
static int
pci_cfgintr_linked(struct PIR_entry *pe, int pin)
{
    struct PIR_entry	*oe;
    struct PIR_intpin	*pi;
    int			i, j, irq;

    /*
     * Scan table slots.
     */
    for (i = 0, oe = &pci_route_table->pt_entry[0]; i < pci_route_count; i++, oe++) {

	/* scan interrupt pins */
	for (j = 0, pi = &oe->pe_intpin[0]; j < 4; j++, pi++) {

	    /* don't look at the entry we're trying to match with */
	    if ((pe == oe) && (i == (pin - 1)))
		continue;

	    /* compare link bytes */
	    if (pi->link != pe->pe_intpin[pin - 1].link)
		continue;
	    
	    /* link destination mapped to a unique interrupt? */
	    if (powerof2(pi->irqs)) {
		irq = ffs(pi->irqs) - 1;
		PRVERB(("pci_cfgintr_linked: linked (%x) to hard-routed irq %d\n",
		       pi->link, irq));
		return(irq);
	    } 

	    /* look for the real PCI device that matches this table entry */
	    if ((irq = pci_cfgintr_search(pe, oe->pe_bus, oe->pe_device, j, pin)) != 255)
		return(irq);
	}
    }
    return(255);
}

/*
 * Scan for the real PCI device at (bus)/(device) using intpin (matchpin) and
 * see if it has already been assigned an interrupt.
 */
static int
pci_cfgintr_search(struct PIR_entry *pe, int bus, int device, int matchpin, int pin)
{
    devclass_t		pci_devclass;
    device_t		*pci_devices;
    int			pci_count;
    device_t		*pci_children;
    int			pci_childcount;
    device_t		*busp, *childp;
    int			i, j, irq;

    /*
     * Find all the PCI busses.
     */
    pci_count = 0;
    if ((pci_devclass = devclass_find("pci")) != NULL)
	devclass_get_devices(pci_devclass, &pci_devices, &pci_count);

    /*
     * Scan all the PCI busses/devices looking for this one.
     */
    irq = 255;
    for (i = 0, busp = pci_devices; (i < pci_count) && (irq == 255); i++, busp++) {
	pci_childcount = 0;
	device_get_children(*busp, &pci_children, &pci_childcount);
		
	for (j = 0, childp = pci_children; j < pci_childcount; j++, childp++) {
	    if ((pci_get_bus(*childp) == bus) &&
		(pci_get_slot(*childp) == device) &&
		(pci_get_intpin(*childp) == matchpin)) {
		irq = pci_get_irq(*childp);
		/*
		 * Some BIOS writers seem to want to ignore the spec and put
		 * 0 in the intline rather than 255 to indicate none.  Once
		 * we've found one that matches, we break because there can
		 * be no others (which is why test looks a little odd).
		 */
		if (irq == 0)
		    irq = 255;
		if (irq != 255)
		    PRVERB(("pci_cfgintr_search: linked (%x) to configured irq %d at %d:%d:%d\n",
		      pe->pe_intpin[pin - 1].link, irq,
		      pci_get_bus(*childp), pci_get_slot(*childp), pci_get_function(*childp)));
		break;
	    }
	}
	if (pci_children != NULL)
	    free(pci_children, M_TEMP);
    }
    if (pci_devices != NULL)
	free(pci_devices, M_TEMP);
    return(irq);
}

/*
 * Pick a suitable IRQ from those listed as routable to this device.
 */
static int
pci_cfgintr_virgin(struct PIR_entry *pe, int pin)
{
    int		irq, ibit;
    
    /* first scan the set of PCI-only interrupts and see if any of these are routable */
    for (irq = 0; irq < 16; irq++) {
	ibit = (1 << irq);

	/* can we use this interrupt? */
	if ((pci_route_table->pt_header.ph_pci_irqs & ibit) &&
	    (pe->pe_intpin[pin - 1].irqs & ibit)) {
	    PRVERB(("pci_cfgintr_virgin: using routable PCI-only interrupt %d\n", irq));
	    return(irq);
	}
    }
    
    /* life is tough, so just pick an interrupt */
    for (irq = 0; irq < 16; irq++) {
	ibit = (1 << irq);
    
	if (pe->pe_intpin[pin - 1].irqs & ibit) {
	    PRVERB(("pci_cfgintr_virgin: using routable interrupt %d\n", irq));
	    return(irq);
	}
    }
    return(255);
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
	DELAY(1);
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
