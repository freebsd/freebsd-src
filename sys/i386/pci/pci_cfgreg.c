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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>		/* XXX trim includes */
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <isa/isavar.h>
#include <machine/pci_cfgreg.h>
#include <machine/segments.h>
#include <machine/pc/bios.h>

#ifdef APIC_IO
#include <machine/smp.h>
#endif /* APIC_IO */

#include "pcib_if.h"

#define PRVERB(a) do {							\
	if (bootverbose)						\
		printf a ;						\
} while(0)

static int cfgmech;
static int devmax;

static int	pci_cfgintr_valid(struct PIR_entry *pe, int pin, int irq);
static int	pci_cfgintr_unique(struct PIR_entry *pe, int pin);
static int	pci_cfgintr_linked(struct PIR_entry *pe, int pin);
static int	pci_cfgintr_search(struct PIR_entry *pe, int bus, int device, int matchpin, int pin);
static int	pci_cfgintr_virgin(struct PIR_entry *pe, int pin);

static void	pci_print_irqmask(u_int16_t irqs);
static void	pci_print_route_table(struct PIR_table *prt, int size);
static int	pcireg_cfgread(int bus, int slot, int func, int reg, int bytes);
static void	pcireg_cfgwrite(int bus, int slot, int func, int reg, int data, int bytes);
static int	pcireg_cfgopen(void);

static struct PIR_table *pci_route_table;
static int pci_route_count;

static struct mtx pcicfg_mtx;

/* sysctl vars */
SYSCTL_DECL(_hw_pci);

#ifdef PC98
#define PCI_IRQ_OVERRIDE_MASK 0x3e68
#else
#define PCI_IRQ_OVERRIDE_MASK 0xdef4
#endif

static uint32_t pci_irq_override_mask = PCI_IRQ_OVERRIDE_MASK;
TUNABLE_INT("hw.pci.irq_override_mask", &pci_irq_override_mask);
SYSCTL_INT(_hw_pci, OID_AUTO, irq_override_mask, CTLFLAG_RDTUN,
    &pci_irq_override_mask, PCI_IRQ_OVERRIDE_MASK,
    "Mask of allowed irqs to try to route when it has no good clue about\n"
    "which irqs it should use.");


/*
 * Some BIOS writers seem to want to ignore the spec and put
 * 0 in the intline rather than 255 to indicate none.  Some use
 * numbers in the range 128-254 to indicate something strange and
 * apparently undocumented anywhere.  Assume these are completely bogus
 * and map them to 255, which means "none".
 */
static __inline__ int 
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
	static int		opened = 0;
	u_long			sigaddr;
	static struct PIR_table	*pt;
	u_int16_t		v;
	u_int8_t		ck, *cv;
	int			i;

	if (opened)
		return(1);

	if (pcireg_cfgopen() == 0)
		return(0);

	v = pcibios_get_version();
	if (v > 0)
		printf("pcibios: BIOS version %x.%02x\n", (v & 0xff00) >> 8,
		    v & 0xff);

	/*
	 * Look for the interrupt routing table.
	 *
	 * We use PCI BIOS's PIR table if it's available $PIR is the
	 * standard way to do this.  Sadly, some machines are not
	 * standards conforming and have _PIR instead.  We shrug and cope
	 * by looking for both.
	 */
	if (pcibios_get_version() >= 0x0210 && pt == NULL) {
		sigaddr = bios_sigsearch(0, "$PIR", 4, 16, 0);
		if (sigaddr == 0)
			sigaddr = bios_sigsearch(0, "_PIR", 4, 16, 0);
		if (sigaddr != 0) {
			pt = (struct PIR_table *)(uintptr_t)
			    BIOS_PADDRTOVADDR(sigaddr);
			for (cv = (u_int8_t *)pt, ck = 0, i = 0;
			     i < (pt->pt_header.ph_length); i++) {
				ck += cv[i];
			}
			if (ck == 0 && pt->pt_header.ph_length >
			    sizeof(struct PIR_header)) {
				pci_route_table = pt;
				pci_route_count = (pt->pt_header.ph_length -
				    sizeof(struct PIR_header)) / 
				    sizeof(struct PIR_entry);
				printf("Using $PIR table, %d entries at %p\n",
				    pci_route_count, pci_route_table);
				if (bootverbose)
					pci_print_route_table(pci_route_table,
					    pci_route_count);
			}
		}
	}
	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;
	return(1);
}

/* 
 * Read configuration space register
 */
u_int32_t
pci_cfgregread(int bus, int slot, int func, int reg, int bytes)
{
	uint32_t line;
#ifdef APIC_IO
	uint32_t pin;

	/*
	 * If we are using the APIC, the contents of the intline
	 * register will probably be wrong (since they are set up for
	 * use with the PIC.  Rather than rewrite these registers
	 * (maybe that would be smarter) we trap attempts to read them
	 * and translate to our private vector numbers.
	 */
	if ((reg == PCIR_INTLINE) && (bytes == 1)) {

		pin = pcireg_cfgread(bus, slot, func, PCIR_INTPIN, 1);
		line = pcireg_cfgread(bus, slot, func, PCIR_INTLINE, 1);

		if (pin != 0) {
			int airq;

			airq = pci_apic_irq(bus, slot, pin);
			if (airq >= 0) {
				/* PCI specific entry found in MP table */
				if (airq != line)
					undirect_pci_irq(line);
				return(airq);
			} else {
				/* 
				 * PCI interrupts might be redirected
				 * to the ISA bus according to some MP
				 * tables. Use the same methods as
				 * used by the ISA devices devices to
				 * find the proper IOAPIC int pin.
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
#else
	/*
	 * Some BIOS writers seem to want to ignore the spec and put
	 * 0 in the intline rather than 255 to indicate none.  The rest of
	 * the code uses 255 as an invalid IRQ.
	 */
	if (reg == PCIR_INTLINE && bytes == 1) {
		line = pcireg_cfgread(bus, slot, func, PCIR_INTLINE, 1);
		return pci_i386_map_intline(line);
	}
#endif /* APIC_IO */
	return(pcireg_cfgread(bus, slot, func, reg, bytes));
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
 * Route a PCI interrupt
 */
int
pci_cfgintr(int bus, int device, int pin, int oldirq)
{
	struct PIR_entry	*pe;
	int			i, irq;
	struct bios_regs	args;
	u_int16_t		v;
 	int already = 0;
 	int errok = 0;
    
	v = pcibios_get_version();
	if (v < 0x0210) {
		PRVERB((
		"pci_cfgintr: BIOS %x.%02x doesn't support interrupt routing\n",
		    (v & 0xff00) >> 8, v & 0xff));
		return (PCI_INVALID_IRQ);
	}
	if ((bus < 0) || (bus > 255) || (device < 0) || (device > 255) ||
	    (pin < 1) || (pin > 4))
		return(PCI_INVALID_IRQ);

	/*
	 * Scan the entry table for a contender
	 */
	for (i = 0, pe = &pci_route_table->pt_entry[0]; i < pci_route_count; 
	     i++, pe++) {
		if ((bus != pe->pe_bus) || (device != pe->pe_device))
			continue;
		/*
		 * A link of 0 means that this intpin is not connected to
		 * any other device's interrupt pins and is not connected to
		 * any of the Interrupt Router's interrupt pins, so we can't
		 * route it.
		 */
		if (pe->pe_intpin[pin - 1].link == 0)
			continue;

		if (pci_cfgintr_valid(pe, pin, oldirq)) {
			printf("pci_cfgintr: %d:%d INT%c BIOS irq %d\n", bus,
			    device, 'A' + pin - 1, oldirq);
			return (oldirq);
		}

		/*
		 * We try to find a linked interrupt, then we look to see
		 * if the interrupt is uniquely routed, then we look for
		 * a virgin interrupt.  The virgin interrupt should return
		 * an interrupt we can route, but if that fails, maybe we
		 * should try harder to route a different interrupt.
		 * However, experience has shown that that's rarely the
		 * failure mode we see.
		 */
		irq = pci_cfgintr_linked(pe, pin);
		if (irq != PCI_INVALID_IRQ)
			already = 1;
		if (irq == PCI_INVALID_IRQ) {
			irq = pci_cfgintr_unique(pe, pin);
			if (irq != PCI_INVALID_IRQ)
				errok = 1;
		}
		if (irq == PCI_INVALID_IRQ)
			irq = pci_cfgintr_virgin(pe, pin);
		if (irq == PCI_INVALID_IRQ)
			break;

		/*
		 * Ask the BIOS to route the interrupt.  If we picked an
		 * interrupt that failed, we should really try other
		 * choices that the BIOS offers us.
		 *
		 * For uniquely routed interrupts, we need to try
		 * to route them on some machines.  Yet other machines
		 * fail to route, so we have to pretend that in that
		 * case it worked.  Isn't pc hardware fun?
		 *
		 * NOTE: if we want to whack hardware to do this, then
		 * I think the right way to do that would be to have
		 * bridge drivers that do this.  I'm not sure that the
		 * $PIR table would be valid for those interrupt
		 * routers.
		 */
		args.eax = PCIBIOS_ROUTE_INTERRUPT;
		args.ebx = (bus << 8) | (device << 3);
		/* pin value is 0xa - 0xd */
		args.ecx = (irq << 8) | (0xa + pin - 1);
		if (!already &&
		    bios32(&args, PCIbios.ventry, GSEL(GCODE_SEL, SEL_KPL)) &&
		    !errok) {
			PRVERB(("pci_cfgintr: ROUTE_INTERRUPT failed.\n"));
			return(PCI_INVALID_IRQ);
		}
		printf("pci_cfgintr: %d:%d INT%c routed to irq %d\n", bus, 
		    device, 'A' + pin - 1, irq);
		return(irq);
	}

	PRVERB(("pci_cfgintr: can't route an interrupt to %d:%d INT%c\n", bus, 
	    device, 'A' + pin - 1));
	return(PCI_INVALID_IRQ);
}

/*
 * Check to see if an existing IRQ setting is valid.
 */
static int
pci_cfgintr_valid(struct PIR_entry *pe, int pin, int irq)
{
	uint32_t irqmask;

	if (!PCI_INTERRUPT_VALID(irq))
		return (0);
	irqmask = pe->pe_intpin[pin - 1].irqs;
	if (irqmask & (1 << irq)) {
		PRVERB(("pci_cfgintr_valid: BIOS irq %d is valid\n", irq));
		return (1);
	}
	return (0);
}

/*
 * Look to see if the routing table claims this pin is uniquely routed.
 */
static int
pci_cfgintr_unique(struct PIR_entry *pe, int pin)
{
	int		irq;
	uint32_t	irqmask;
    
	irqmask = pe->pe_intpin[pin - 1].irqs;
	if (irqmask != 0 && powerof2(irqmask)) {
		irq = ffs(irqmask) - 1;
		PRVERB(("pci_cfgintr_unique: hard-routed to irq %d\n", irq));
		return(irq);
	}
	return(PCI_INVALID_IRQ);
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
	for (i = 0, oe = &pci_route_table->pt_entry[0]; i < pci_route_count;
	     i++, oe++) {
		/* scan interrupt pins */
		for (j = 0, pi = &oe->pe_intpin[0]; j < 4; j++, pi++) {

			/* don't look at the entry we're trying to match */
			if ((pe == oe) && (i == (pin - 1)))
				continue;
			/* compare link bytes */
			if (pi->link != pe->pe_intpin[pin - 1].link)
				continue;
			/* link destination mapped to a unique interrupt? */
			if (pi->irqs != 0 && powerof2(pi->irqs)) {
				irq = ffs(pi->irqs) - 1;
				PRVERB(("pci_cfgintr_linked: linked (%x) to hard-routed irq %d\n",
				    pi->link, irq));
				return(irq);
			} 

			/* 
			 * look for the real PCI device that matches this 
			 * table entry 
			 */
			irq = pci_cfgintr_search(pe, oe->pe_bus, oe->pe_device,
			    j + 1, pin);
			if (irq != PCI_INVALID_IRQ)
				return(irq);
		}
	}
	return(PCI_INVALID_IRQ);
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
	irq = PCI_INVALID_IRQ;
	for (i = 0, busp = pci_devices; (i < pci_count) && (irq == PCI_INVALID_IRQ);
	     i++, busp++) {
		pci_childcount = 0;
		device_get_children(*busp, &pci_children, &pci_childcount);
		
		for (j = 0, childp = pci_children; j < pci_childcount; j++, 
		    childp++) {
			if ((pci_get_bus(*childp) == bus) &&
			    (pci_get_slot(*childp) == device) &&
			    (pci_get_intpin(*childp) == matchpin)) {
				irq = pci_i386_map_intline(pci_get_irq(*childp));
				if (irq != PCI_INVALID_IRQ)
					PRVERB(("pci_cfgintr_search: linked (%x) to configured irq %d at %d:%d:%d\n",
					    pe->pe_intpin[pin - 1].link, irq,
					    pci_get_bus(*childp),
					    pci_get_slot(*childp), 
					    pci_get_function(*childp)));
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
    
	/*
	 * first scan the set of PCI-only interrupts and see if any of these 
	 * are routable
	 */ 
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
		if ((ibit & pci_irq_override_mask) == 0)
			continue;
		if (pe->pe_intpin[pin - 1].irqs & ibit) {
			PRVERB(("pci_cfgintr_virgin: using routable interrupt %d\n", irq));
			return(irq);
		}
	}
	return(PCI_INVALID_IRQ);
}

static void
pci_print_irqmask(u_int16_t irqs)
{
	int i, first;

	if (irqs == 0) {
		printf("none");
		return;
	}
	first = 1;
	for (i = 0; i < 16; i++, irqs >>= 1)
		if (irqs & 1) {
			if (!first)
				printf(" ");
			else
				first = 0;
			printf("%d", i);
		}
}

/*
 * Dump the contents of a PCI BIOS Interrupt Routing Table to the console.
 */
static void
pci_print_route_table(struct PIR_table *prt, int size)
{
	struct PIR_entry *entry;
	struct PIR_intpin *intpin;
	int i, pin;

	printf("PCI-Only Interrupts: ");
	pci_print_irqmask(prt->pt_header.ph_pci_irqs);
	printf("\nLocation  Bus Device Pin  Link  IRQs\n");
	entry = &prt->pt_entry[0];
	for (i = 0; i < size; i++, entry++) {
		intpin = &entry->pe_intpin[0];
		for (pin = 0; pin < 4; pin++, intpin++)
			if (intpin->link != 0) {
				if (entry->pe_slot == 0)
					printf("embedded ");
				else
					printf("slot %-3d ", entry->pe_slot);
				printf(" %3d  %3d    %c   0x%02x  ",
				    entry->pe_bus, entry->pe_device,
				    'A' + pin, intpin->link);
				pci_print_irqmask(intpin->irqs);
				printf("\n");
			}
	}
}

/*
 * See if any interrupts for a given PCI bus are routed in the PIR.  Don't
 * even bother looking if the BIOS doesn't support routing anyways.
 */
int
pci_probe_route_table(int bus)
{
	int i;
	u_int16_t v;

	v = pcibios_get_version();
	if (v < 0x0210)
		return (0);
	for (i = 0; i < pci_route_count; i++)
		if (pci_route_table->pt_entry[i].pe_bus == bus)
			return (1);
	return (0);
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
	    && (reg & (bytes - 1)) == 0) {
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

	oldval1 = inl(CONF1_ADDR_PORT);

	if (bootverbose) {
		printf("pci_open(1):\tmode 1 addr port (0x0cf8) is 0x%08x\n",
		    oldval1);
	}

	if ((oldval1 & CONF1_ENABLE_MSK) == 0) {

		cfgmech = 1;
		devmax = 32;

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK);
		outb(CONF1_ADDR_PORT + 3, 0);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1a):\tmode1res=0x%08x (0x%08lx)\n", 
			    mode1res, CONF1_ENABLE_CHK);

		if (mode1res) {
			if (pci_cfgcheck(32)) 
				return (cfgmech);
		}

		outl(CONF1_ADDR_PORT, CONF1_ENABLE_CHK1);
		mode1res = inl(CONF1_ADDR_PORT);
		outl(CONF1_ADDR_PORT, oldval1);

		if (bootverbose)
			printf("pci_open(1b):\tmode1res=0x%08x (0x%08lx)\n", 
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

