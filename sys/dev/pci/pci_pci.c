/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$FreeBSD$
 */

/*
 * PCI:PCI bridge support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

static int		pcib_probe(device_t dev);

static device_method_t pcib_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		pcib_probe),
    DEVMETHOD(device_attach,		pcib_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_read_ivar,		pcib_read_ivar),
    DEVMETHOD(bus_write_ivar,		pcib_write_ivar),
    DEVMETHOD(bus_alloc_resource,	pcib_alloc_resource),
    DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    /* pcib interface */
    DEVMETHOD(pcib_maxslots,		pcib_maxslots),
    DEVMETHOD(pcib_read_config,		pcib_read_config),
    DEVMETHOD(pcib_write_config,	pcib_write_config),
    DEVMETHOD(pcib_route_interrupt,	pcib_route_interrupt),

    { 0, 0 }
};

static driver_t pcib_driver = {
    "pcib",
    pcib_methods,
    sizeof(struct pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, pci, pcib_driver, pcib_devclass, 0, 0);

/*
 * sysctl and tunable vars
 */
static int pci_allow_unsupported_io_range = 0;
TUNABLE_INT("hw.pci.allow_unsupported_io_range",
	(int *)&pci_allow_unsupported_io_range);
SYSCTL_DECL(_hw_pci);
SYSCTL_INT(_hw_pci, OID_AUTO, allow_unsupported_io_range, CTLFLAG_RD,
	&pci_allow_unsupported_io_range, 0,
	"Allows the PCI Bridge to pass through an unsupported memory range "
	"assigned by the BIOS.");

/*
 * Generic device interface
 */
static int
pcib_probe(device_t dev)
{
    if ((pci_get_class(dev) == PCIC_BRIDGE) &&
	(pci_get_subclass(dev) == PCIS_BRIDGE_PCI)) {
	device_set_desc(dev, "PCI-PCI bridge");
	return(-10000);
    }
    return(ENXIO);
}

void
pcib_attach_common(device_t dev)
{
    struct pcib_softc	*sc;
    uint8_t		iolow;

    sc = device_get_softc(dev);
    sc->dev = dev;

    /*
     * Get current bridge configuration.
     */
    sc->command   = pci_read_config(dev, PCIR_COMMAND, 1);
    sc->secbus    = pci_read_config(dev, PCIR_SECBUS_1, 1);
    sc->subbus    = pci_read_config(dev, PCIR_SUBBUS_1, 1);
    sc->secstat   = pci_read_config(dev, PCIR_SECSTAT_1, 2);
    sc->bridgectl = pci_read_config(dev, PCIR_BRIDGECTL_1, 2);
    sc->seclat    = pci_read_config(dev, PCIR_SECLAT_1, 1);

    /*
     * Determine current I/O decode.
     */
    if (sc->command & PCIM_CMD_PORTEN) {
	iolow = pci_read_config(dev, PCIR_IOBASEL_1, 1);
	if ((iolow & PCIM_BRIO_MASK) == PCIM_BRIO_32) {
	    sc->iobase = PCI_PPBIOBASE(pci_read_config(dev, PCIR_IOBASEH_1, 2),
				       pci_read_config(dev, PCIR_IOBASEL_1, 1));
	} else {
	    sc->iobase = PCI_PPBIOBASE(0, pci_read_config(dev, PCIR_IOBASEL_1, 1));
	}

	iolow = pci_read_config(dev, PCIR_IOLIMITL_1, 1);
	if ((iolow & PCIM_BRIO_MASK) == PCIM_BRIO_32) {
	    sc->iolimit = PCI_PPBIOLIMIT(pci_read_config(dev, PCIR_IOLIMITH_1, 2),
					 pci_read_config(dev, PCIR_IOLIMITL_1, 1));
	} else {
	    sc->iolimit = PCI_PPBIOLIMIT(0, pci_read_config(dev, PCIR_IOLIMITL_1, 1));
	}
    }

    /*
     * Determine current memory decode.
     */
    if (sc->command & PCIM_CMD_MEMEN) {
	sc->membase   = PCI_PPBMEMBASE(0, pci_read_config(dev, PCIR_MEMBASE_1, 2));
	sc->memlimit  = PCI_PPBMEMLIMIT(0, pci_read_config(dev, PCIR_MEMLIMIT_1, 2));
	sc->pmembase  = PCI_PPBMEMBASE((pci_addr_t)pci_read_config(dev, PCIR_PMBASEH_1, 4),
				       pci_read_config(dev, PCIR_PMBASEL_1, 2));
	sc->pmemlimit = PCI_PPBMEMLIMIT((pci_addr_t)pci_read_config(dev, PCIR_PMLIMITH_1, 4),
					pci_read_config(dev, PCIR_PMLIMITL_1, 2));
    }

    /*
     * Quirk handling.
     */
    switch (pci_get_devid(dev)) {
	case 0x12258086:		/* Intel 82454KX/GX (Orion) */
	{
	    uint8_t	supbus;

	    supbus = pci_read_config(dev, 0x41, 1);
	    if (supbus != 0xff) {
		sc->secbus = supbus + 1;
		sc->subbus = supbus + 1;
	    }
	}
	break;
    }

    if (bootverbose) {
	device_printf(dev, "  secondary bus     %d\n", sc->secbus);
	device_printf(dev, "  subordinate bus   %d\n", sc->subbus);
	device_printf(dev, "  I/O decode        0x%x-0x%x\n", sc->iobase, sc->iolimit);
	device_printf(dev, "  memory decode     0x%x-0x%x\n", sc->membase, sc->memlimit);
	device_printf(dev, "  prefetched decode 0x%x-0x%x\n", sc->pmembase, sc->pmemlimit);
    }

    /*
     * XXX If the secondary bus number is zero, we should assign a bus number
     *     since the BIOS hasn't, then initialise the bridge.
     */

    /*
     * XXX If the subordinate bus number is less than the secondary bus number,
     *     we should pick a better value.  One sensible alternative would be to
     *     pick 255; the only tradeoff here is that configuration transactions
     *     would be more widely routed than absolutely necessary.
     */
}

int
pcib_attach(device_t dev)
{
    struct pcib_softc	*sc;
    device_t		child;

    pcib_attach_common(dev);
    sc = device_get_softc(dev);
    if (sc->secbus != 0) {
	child = device_add_child(dev, "pci", sc->secbus);
	if (child != NULL)
	    return(bus_generic_attach(dev));
    } 

    /* no secondary bus; we should have fixed this */
    return(0);
}

int
pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
    struct pcib_softc	*sc = device_get_softc(dev);
    
    switch (which) {
    case PCIB_IVAR_BUS:
	*result = sc->secbus;
	return(0);
    }
    return(ENOENT);
}

int
pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
    struct pcib_softc	*sc = device_get_softc(dev);

    switch (which) {
    case PCIB_IVAR_BUS:
	sc->secbus = value;
	break;
    }
    return(ENOENT);
}

/*
 * Is this a decoded ISA I/O port address?  Note, we need to do the mask that
 * we do below because of the ISA alias addresses.  I'm not 100% sure that
 * this is correct.  Maybe the bridge needs to be subtractive decode for
 * this to work?
 */
static int
pcib_is_isa_io(u_long start)
{
    if ((start & 0xfffUL)  > 0x3ffUL || start == 0)
	return (0);
    return (1);
}

/*
 * Is this a decoded ISA memory address?
 */
static int
pcib_is_isa_mem(u_long start)
{
    if (start > 0xfffffUL || start == 0)
	return (0);
    return (1);
}

/*
 * Is the prefetch window open (eg, can we allocate memory in it?)
 */
static int
pcib_is_prefetch_open(struct pcib_softc *sc)
{
    return (sc->pmembase > 0 && sc->pmembase < sc->pmemlimit);
}

/*
 * Is the nonprefetch window open (eg, can we allocate memory in it?)
 */
static int
pcib_is_nonprefetch_open(struct pcib_softc *sc)
{
    return (sc->membase > 0 && sc->membase < sc->memlimit);
}

/*
 * Is the io window open (eg, can we allocate ports in it?)
 */
static int
pcib_is_io_open(struct pcib_softc *sc)
{
    return (sc->iobase > 0 && sc->iobase < sc->iolimit);
}

/*
 * We have to trap resource allocation requests and ensure that the bridge
 * is set up to, or capable of handling them.
 */
struct resource *
pcib_alloc_resource(device_t dev, device_t child, int type, int *rid, 
		    u_long start, u_long end, u_long count, u_int flags)
{
    struct pcib_softc	*sc = device_get_softc(dev);
    int ok;

    /*
     * If this is a "default" allocation against this rid, we can't work
     * out where it's coming from (we should actually never see these) so we
     * just have to punt.
     */
    if ((start == 0) && (end == ~0)) {
	device_printf(dev, "can't decode default resource id %d for %s%d, bypassing\n",
		      *rid, device_get_name(child), device_get_unit(child));
    } else {
	/*
	 * Fail the allocation for this range if it's not supported.
	 */
	switch (type) {
	case SYS_RES_IOPORT:
	    ok = 1;
	    if (!pcib_is_isa_io(start)) {
		ok = 0;
		if (pcib_is_io_open(sc))
		    ok = (start >= sc->iobase && end <= sc->iolimit);
		if (!pci_allow_unsupported_io_range) {
		    if (!ok) {
			if (start < sc->iobase)
			    start = sc->iobase;
			if (end > sc->iolimit)
			    end = sc->iolimit;
		    }
		} else {
		    if (start < sc->iobase)
			printf("start (%lx) < sc->iobase (%x)\n", start,
				sc->iobase);
		    if (end > sc->iolimit)
			printf("end (%lx) > sc->iolimit (%x)\n",
				end, sc->iolimit);
		    if (end < start)
			printf("end (%lx) < start (%lx)\n", end, start);
		}
	    }
	    if (end < start) {
		start = 0;
		end = 0;
		ok = 0;
	    }
	    if (!ok) {
		device_printf(dev, "device %s%d requested unsupported I/O "
		  "range 0x%lx-0x%lx (decoding 0x%x-0x%x)\n",
		  device_get_name(child), device_get_unit(child), start, end,
		  sc->iobase, sc->iolimit);
		return (NULL);
	    }
	    if (bootverbose)
		device_printf(sc->dev, "device %s%d requested decoded I/O range 0x%lx-0x%lx\n",
			      device_get_name(child), device_get_unit(child), start, end);
	    break;

	case SYS_RES_MEMORY:
	    ok = 1;
	    if (!pcib_is_isa_mem(start)) {
		ok = 0;
		if (pcib_is_nonprefetch_open(sc))
		    ok = ok || (start >= sc->membase && end <= sc->memlimit);
		if (pcib_is_prefetch_open(sc))
		    ok = ok || (start >= sc->pmembase && end <= sc->pmemlimit);
		if (!pci_allow_unsupported_io_range) {
		    if (!ok) {
			ok = 1;
			if (flags & RF_PREFETCHABLE) {
			    if (pcib_is_prefetch_open(sc)) {
				if (start < sc->pmembase)
				    start = sc->pmembase;
				if (end > sc->pmemlimit)
				    end = sc->pmemlimit;
			    } else {
				ok = 0;
			    }
			} else {	/* non-prefetchable */
			    if (pcib_is_nonprefetch_open(sc)) {
				if (start < sc->membase)
				    start = sc->membase;
				if (end > sc->memlimit)
				    end = sc->memlimit;
			    } else {
				ok = 0;
			    }
			}
		    }
		} else if (!ok) {
		    ok = 1;	/* pci_allow_unsupported_ranges -> always ok */
		    if (pcib_is_nonprefetch_open(sc)) {
			if (start < sc->membase)
			    printf("start (%lx) < sc->membase (%x)\n",
			      start, sc->membase);
			if (end > sc->memlimit)
			    printf("end (%lx) > sc->memlimit (%x)\n",
			      end, sc->memlimit);
		    }
		    if (pcib_is_prefetch_open(sc)) {
			if (start < sc->pmembase)
			    printf("start (%lx) < sc->pmembase (%x)\n",
			      start, sc->pmembase);
			if (end > sc->pmemlimit)
			    printf("end (%lx) > sc->pmemlimit (%x)\n",
			      end, sc->memlimit);
		    }
		    if (end < start)
			printf("end (%lx) < start (%lx)\n", end, start);
		}
	    }
	    if (end < start) {
		start = 0;
		end = 0;
		ok = 0;
	    }
	    if (!ok && bootverbose)
		device_printf(dev,
		  "device %s%d requested unsupported memory range "
		  "0x%lx-0x%lx (decoding 0x%x-0x%x, 0x%x-0x%x)\n",
		  device_get_name(child), device_get_unit(child), start,
		  end, sc->membase, sc->memlimit, sc->pmembase,
		  sc->pmemlimit);
	    if (!ok)
		return (NULL);
	    if (bootverbose)
		device_printf(sc->dev, "device %s%d requested decoded memory range 0x%lx-0x%lx\n",
		  device_get_name(child), device_get_unit(child), start, end);
	    break;

	default:
	    break;
	}
    }

    /*
     * Bridge is OK decoding this resource, so pass it up.
     */
    return(bus_generic_alloc_resource(dev, child, type, rid, start, end, count, flags));
}

/*
 * PCIB interface.
 */
int
pcib_maxslots(device_t dev)
{
    return(PCI_SLOTMAX);
}

/*
 * Since we are a child of a PCI bus, its parent must support the pcib interface.
 */
uint32_t
pcib_read_config(device_t dev, int b, int s, int f, int reg, int width)
{
    return(PCIB_READ_CONFIG(device_get_parent(device_get_parent(dev)), b, s, f, reg, width));
}

void
pcib_write_config(device_t dev, int b, int s, int f, int reg, uint32_t val, int width)
{
    PCIB_WRITE_CONFIG(device_get_parent(device_get_parent(dev)), b, s, f, reg, val, width);
}

/*
 * Route an interrupt across a PCI bridge.
 */
int
pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
    device_t	bus;
    int		parent_intpin;
    int		intnum;

    /*	
     *
     * The PCI standard defines a swizzle of the child-side device/intpin to
     * the parent-side intpin as follows.
     *
     * device = device on child bus
     * child_intpin = intpin on child bus slot (0-3)
     * parent_intpin = intpin on parent bus slot (0-3)
     *
     * parent_intpin = (device + child_intpin) % 4
     */
    parent_intpin = (pci_get_slot(dev) + (pin - 1)) % 4;

    /*
     * Our parent is a PCI bus.  Its parent must export the pcib interface
     * which includes the ability to route interrupts.
     */
    bus = device_get_parent(pcib);
    intnum = PCIB_ROUTE_INTERRUPT(device_get_parent(bus), pcib, parent_intpin + 1);
    if (PCI_INTERRUPT_VALID(intnum)) {
	device_printf(pcib, "slot %d INT%c is routed to irq %d\n",
	    pci_get_slot(dev), 'A' + pin - 1, intnum);
    }
    return(intnum);
}

/*
 * Try to read the bus number of a host-PCI bridge using appropriate config
 * registers.
 */
int
host_pcib_get_busno(pci_read_config_fn read_config, int bus, int slot, int func,
    uint8_t *busnum)
{
	uint32_t id;

	id = read_config(bus, slot, func, PCIR_DEVVENDOR, 4);
	if (id == 0xffffffff)
		return (0);

	switch (id) {
	case 0x12258086:
		/* Intel 824?? */
		/* XXX This is a guess */
		/* *busnum = read_config(bus, slot, func, 0x41, 1); */
		*busnum = bus;
		break;
	case 0x84c48086:
		/* Intel 82454KX/GX (Orion) */
		*busnum = read_config(bus, slot, func, 0x4a, 1);
		break;
	case 0x84ca8086:
		/*
		 * For the 450nx chipset, there is a whole bundle of
		 * things pretending to be host bridges. The MIOC will 
		 * be seen first and isn't really a pci bridge (the
		 * actual busses are attached to the PXB's). We need to 
		 * read the registers of the MIOC to figure out the
		 * bus numbers for the PXB channels.
		 *
		 * Since the MIOC doesn't have a pci bus attached, we
		 * pretend it wasn't there.
		 */
		return (0);
	case 0x84cb8086:
		switch (slot) {
		case 0x12:
			/* Intel 82454NX PXB#0, Bus#A */
			*busnum = read_config(bus, 0x10, func, 0xd0, 1);
			break;
		case 0x13:
			/* Intel 82454NX PXB#0, Bus#B */
			*busnum = read_config(bus, 0x10, func, 0xd1, 1) + 1;
			break;
		case 0x14:
			/* Intel 82454NX PXB#1, Bus#A */
			*busnum = read_config(bus, 0x10, func, 0xd3, 1);
			break;
		case 0x15:
			/* Intel 82454NX PXB#1, Bus#B */
			*busnum = read_config(bus, 0x10, func, 0xd4, 1) + 1;
			break;
		}
		break;

		/* ServerWorks -- vendor 0x1166 */
	case 0x00051166:
	case 0x00061166:
	case 0x00081166:
	case 0x00091166:
	case 0x00101166:
	case 0x00111166:
	case 0x00171166:
	case 0x01011166:
	case 0x010f1014:
	case 0x02011166:
	case 0x03021014:
		*busnum = read_config(bus, slot, func, 0x44, 1);
		break;
	default:
		/* Don't know how to read bus number. */
		return 0;
	}

	return 1;
}
