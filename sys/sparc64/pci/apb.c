/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Thomas Moestl <tmm@FreeBSD.org>
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
 *	from: FreeBSD: src/sys/dev/pci/pci_pci.c,v 1.3 2000/12/13
 *
 * $FreeBSD$
 */

/*
 * Support for the Sun APB (Advanced PCI Bridge) PCI-PCI bridge.
 * This bridge does not fully comply to the PCI bridge specification, and is
 * therefore not supported by the generic driver.
 * We can use some pf the pcib methods anyway.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <machine/resource.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/pcib.h>

#include "pcib_if.h"

/*
 * Bridge-specific data.
 */
struct apb_softc {
	u_int8_t	iomap;
	u_int8_t	memmap;
};

static int apb_probe(device_t dev);
static int apb_attach(device_t dev);
static struct resource *apb_alloc_resource(device_t dev, device_t child,
    int type, int *rid, u_long start, u_long end, u_long count, u_int flags);
static int apb_route_interrupt(device_t pcib, device_t dev, int pin);

static device_method_t apb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		apb_probe),
	DEVMETHOD(device_attach,	apb_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	apb_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	pcib_maxslots),
	DEVMETHOD(pcib_read_config,	pcib_read_config),
	DEVMETHOD(pcib_write_config,	pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	apb_route_interrupt),

	{ 0, 0 }
};

static driver_t apb_driver = {
	"pcib",
	apb_methods,
	sizeof(struct pcib_softc),
};

static devclass_t apb_devclass;

DRIVER_MODULE(apb, pci, apb_driver, apb_devclass, 0, 0);

#define	APB_SOFTC(sc)	((struct apb_softc *)((sc)->extptr))

/* APB specific registers */
#define	APBR_IOMAP	0xde
#define	APBR_MEMMAP	0xdf

/* Definitions for the mapping registers */
#define	APB_IO_SCALE	0x200000
#define	APB_MEM_SCALE	0x20000000

/*
 * Generic device interface
 */
static int
apb_probe(device_t dev)
{

	if (pci_get_vendor(dev) == 0x108e &&	/* Sun */
	    pci_get_device(dev) == 0x5000)  {	/* APB */
		device_set_desc(dev, "APB PCI-PCI bridge");
		return (0);
	}
	return (ENXIO);
}

static void
apb_map_print(u_int8_t map, u_long scale)
{
	int i, first;

	for (first = 1, i = 0; i < 8; i++) {
		if ((map & (1 << i)) != 0) {
			printf("%s0x%lx-0x%lx", first ? "" : ", ",
			    i * scale, (i + 1) * scale - 1);
			first = 0;
		}
	}
}

static int
apb_map_checkrange(u_int8_t map, u_long scale, u_long start, u_long end)
{
	int i, ei;

	i = start / scale;
	ei = end / scale;
	if (i > 7 || ei > 7)
		return (0);
	for (; i <= ei; i++)
		if ((map & (1 << i)) == 0)
			return (0);
	return (1);
}

static int
apb_attach(device_t dev)
{
	struct pcib_softc *sc;
	struct apb_softc *asc;
	device_t child;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->extptr = malloc(sizeof(struct apb_softc), M_DEVBUF, M_NOWAIT);
	asc = APB_SOFTC(sc);

	/*
	 * Get current bridge configuration.
	 */
	sc->command = pci_read_config(dev, PCIR_COMMAND, 1);
	sc->secbus = pci_read_config(dev, PCIR_SECBUS_1, 1);
	sc->subbus = pci_read_config(dev, PCIR_SUBBUS_1, 1);
	sc->secstat = pci_read_config(dev, PCIR_SECSTAT_1, 2);
	sc->bridgectl = pci_read_config(dev, PCIR_BRIDGECTL_1, 2);
	sc->seclat = pci_read_config(dev, PCIR_SECLAT_1, 1);

	/* The APB does not implement base/limit registers. */
	sc->iobase = sc->iolimit = 0;
	sc->membase = sc->memlimit = 0;
	sc->pmembase = sc->pmemlimit = 0;

	asc->iomap = pci_read_config(dev, APBR_IOMAP, 1);
	asc->memmap = pci_read_config(dev, APBR_MEMMAP, 1);

	if (bootverbose) {
		device_printf(dev, "  secondary bus     %d\n", sc->secbus);
		device_printf(dev, "  subordinate bus   %d\n", sc->subbus);
		device_printf(dev, "  I/O decode        ");
		apb_map_print(asc->iomap, APB_IO_SCALE);
		printf("\n");
		device_printf(dev, "  memory decode     ");
		apb_map_print(asc->memmap, APB_MEM_SCALE);
		printf("\n");
	}

	/*
	 * XXX If the subordinate bus number is less than the secondary bus
	 * number, we should pick a better value.  One sensible alternative
	 * would be to pick 255; the only tradeoff here is that configuration
	 * transactions would be more widely routed than absolutely necessary.
	 */
	if (sc->secbus != 0) {
		child = device_add_child(dev, "pci", -1);
		if (child != NULL)
			return (bus_generic_attach(dev));
	} else
		panic("apb_attach: APB with uninitialized secbus");

	/* no secondary bus; we should have fixed this */
	return (0);
}

/*
 * We have to trap resource allocation requests and ensure that the bridge
 * is set up to, or capable of handling them.
 */
static struct resource *
apb_alloc_resource(device_t dev, device_t child, int type, int *rid, 
    u_long start, u_long end, u_long count, u_int flags)
{
	struct pcib_softc *sc;
	struct apb_softc *asc;

	sc = device_get_softc(dev);
	asc = APB_SOFTC(sc);
	/*
	 * If this is a "default" allocation against this rid, we can't work
	 * out where it's coming from (we should actually never see these) so we
	 * just have to punt.
	 */
	if ((start == 0) && (end == ~0)) {
		device_printf(dev, "can't decode default resource id %d for "
		    "%s%d, bypassing\n", *rid, device_get_name(child),
		    device_get_unit(child));
	} else {
		/*
		 * Fail the allocation for this range if it's not supported.
		 * XXX we should probably just fix up the bridge decode and
		 * soldier on.
		 */
		switch (type) {
		case SYS_RES_IOPORT:
			if (!apb_map_checkrange(asc->iomap, APB_IO_SCALE, start,
			    end)) {
				device_printf(dev, "device %s%d requested "
				    "unsupported I/O range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
				return (NULL);
			}
			if (bootverbose)
				device_printf(sc->dev, "device %s%d requested "
				    "decoded I/O range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
			break;

		case SYS_RES_MEMORY:
			if (!apb_map_checkrange(asc->memmap, APB_MEM_SCALE,
			    start, end)) {
				device_printf(dev, "device %s%d requested "
				    "unsupported memory range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
				return (NULL);
			}
			if (bootverbose)
				device_printf(sc->dev, "device %s%d requested "
				    "decoded memory range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
			break;

		default:
			break;
		}
	}

	/*
	 * Bridge is OK decoding this resource, so pass it up.
	 */
	return (bus_generic_alloc_resource(dev, child, type, rid, start, end,
	    count, flags));
}

/*
 * Route an interrupt across a PCI bridge - the APB does not route interrupts,
 * and routing of interrupts that are not preinitialized is not supported yet.
 */
static int
apb_route_interrupt(device_t pcib, device_t dev, int pin)
{

	panic("apb_route_interrupt");
}
