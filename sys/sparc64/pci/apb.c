/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001, 2003 Thomas Moestl <tmm@FreeBSD.org>
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

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

#include <machine/bus.h>
#include <machine/ofw_bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/ofw_pcib_subr.h>

/*
 * Bridge-specific data.
 */
struct apb_softc {
	struct ofw_pcib_gen_softc	sc_bsc;
	u_int8_t	sc_iomap;
	u_int8_t	sc_memmap;
};

static device_probe_t apb_probe;
static device_attach_t apb_attach;
static bus_alloc_resource_t apb_alloc_resource;
#ifndef OFW_NEWPCI
static pcib_route_interrupt_t apb_route_interrupt;
#endif

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
#ifdef OFW_NEWPCI
	DEVMETHOD(pcib_route_interrupt,	ofw_pcib_gen_route_interrupt),
#else
	DEVMETHOD(pcib_route_interrupt,	apb_route_interrupt),
#endif

	/* ofw_pci interface */
#ifdef OFW_NEWPCI
	DEVMETHOD(ofw_pci_get_node,	ofw_pcib_gen_get_node),
	DEVMETHOD(ofw_pci_adjust_busrange,	ofw_pcib_gen_adjust_busrange),
#endif

	{ 0, 0 }
};

static driver_t apb_driver = {
	"pcib",
	apb_methods,
	sizeof(struct apb_softc),
};

DRIVER_MODULE(apb, pci, apb_driver, pcib_devclass, 0, 0);

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
	struct apb_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Get current bridge configuration.
	 */
	sc->sc_iomap = pci_read_config(dev, APBR_IOMAP, 1);
	sc->sc_memmap = pci_read_config(dev, APBR_MEMMAP, 1);
#ifdef OFW_NEWPCI
	ofw_pcib_gen_setup(dev);
#else
	sc->sc_bsc.ops_pcib_sc.dev = dev;
	sc->sc_bsc.ops_pcib_sc.secbus = pci_read_config(dev, PCIR_SECBUS_1, 1);
	sc->sc_bsc.ops_pcib_sc.subbus = pci_read_config(dev, PCIR_SUBBUS_1, 1);
#endif

	if (bootverbose) {
		device_printf(dev, "  secondary bus     %d\n",
		    sc->sc_bsc.ops_pcib_sc.secbus);
		device_printf(dev, "  subordinate bus   %d\n",
		    sc->sc_bsc.ops_pcib_sc.subbus);
		device_printf(dev, "  I/O decode        ");
		apb_map_print(sc->sc_iomap, APB_IO_SCALE);
		printf("\n");
		device_printf(dev, "  memory decode     ");
		apb_map_print(sc->sc_memmap, APB_MEM_SCALE);
		printf("\n");
	}

#ifndef OFW_NEWPCI
	if (sc->sc_bsc.ops_pcib_sc.secbus == 0)
		panic("apb_attach: APB with uninitialized secbus");
#endif

	device_add_child(dev, "pci", sc->sc_bsc.ops_pcib_sc.secbus);
	return (bus_generic_attach(dev));
}

/*
 * We have to trap resource allocation requests and ensure that the bridge
 * is set up to, or capable of handling them.
 */
static struct resource *
apb_alloc_resource(device_t dev, device_t child, int type, int *rid, 
    u_long start, u_long end, u_long count, u_int flags)
{
	struct apb_softc *sc;

	sc = device_get_softc(dev);
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
			if (!apb_map_checkrange(sc->sc_iomap, APB_IO_SCALE,
			    start, end)) {
				device_printf(dev, "device %s%d requested "
				    "unsupported I/O range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
				return (NULL);
			}
			if (bootverbose)
				device_printf(sc->sc_bsc.ops_pcib_sc.dev,
				    "device %s%d requested decoded I/O range "
				    "0x%lx-0x%lx\n", device_get_name(child),
				    device_get_unit(child), start, end);
			break;

		case SYS_RES_MEMORY:
			if (!apb_map_checkrange(sc->sc_memmap, APB_MEM_SCALE,
			    start, end)) {
				device_printf(dev, "device %s%d requested "
				    "unsupported memory range 0x%lx-0x%lx\n",
				    device_get_name(child),
				    device_get_unit(child), start, end);
				return (NULL);
			}
			if (bootverbose)
				device_printf(sc->sc_bsc.ops_pcib_sc.dev,
				    "device %s%d requested decoded memory "
				    "range 0x%lx-0x%lx\n",
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

#ifndef OFW_NEWPCI
/*
 * Route an interrupt across a PCI bridge - we need to rely on the firmware
 * here.
 */
static int
apb_route_interrupt(device_t pcib, device_t dev, int pin)
{

	/*
	 * XXX: ugly loathsome hack:
	 * We can't use ofw_pci_route_intr() here; the device passed may be
	 * the one of a bridge, so the original device can't be recovered.
	 *
	 * We need to use the firmware to route interrupts, however it has
	 * no interface which could be used to interpret intpins; instead,
	 * all assignments are done by device.
	 *
	 * The MI pci code will try to reroute interrupts of 0, although they
	 * are correct; all other interrupts are preinitialized, so if we
	 * get here, the intline is either 0 (so return 0), or we hit a
	 * device which was not preinitialized (e.g. hotplugged stuff), in
	 * which case we are lost.
	 */
	return (0);
}
#endif /* !OFW_NEWPCI */
