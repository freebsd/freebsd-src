/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
 * Copyright (c) 2003, Thomas Moestl <tmm@FreeBSD.org>
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

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/pciio.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/cache.h>
#include <machine/iommureg.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <sparc64/pci/ofw_pci.h>

#include "pcib_if.h"
#include "pci_if.h"

/* Helper functions. */
static void ofw_pcibus_setup_device(device_t, u_int, u_int, u_int);

/* Methods. */
static device_probe_t ofw_pcibus_probe;
static device_attach_t ofw_pcibus_attach;
static pci_assign_interrupt_t ofw_pcibus_assign_interrupt;
static ofw_bus_get_compat_t ofw_pcibus_get_compat;
static ofw_bus_get_model_t ofw_pcibus_get_model;
static ofw_bus_get_name_t ofw_pcibus_get_name;
static ofw_bus_get_node_t ofw_pcibus_get_node;
static ofw_bus_get_type_t ofw_pcibus_get_type;

static device_method_t ofw_pcibus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_pcibus_probe),
	DEVMETHOD(device_attach,	ofw_pcibus_attach),

	/* Bus interface */

	/* PCI interface */
	DEVMETHOD(pci_assign_interrupt, ofw_pcibus_assign_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_compat,	ofw_pcibus_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_pcibus_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_pcibus_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_pcibus_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_pcibus_get_type),

	{ 0, 0 }
};

struct ofw_pcibus_devinfo {
	struct pci_devinfo	opd_dinfo;
	char			*opd_compat;
	char			*opd_model;
	char			*opd_name;
	char			*opd_type;
	phandle_t		opd_node;
};

struct ofw_pcibus_softc {
	phandle_t	ops_node;
};

static devclass_t pci_devclass;

DEFINE_CLASS_1(pci, ofw_pcibus_driver, ofw_pcibus_methods,
    sizeof(struct ofw_pcibus_softc), pci_driver);
DRIVER_MODULE(ofw_pcibus, pcib, ofw_pcibus_driver, pci_devclass, 0, 0);
MODULE_VERSION(ofw_pcibus, 1);
MODULE_DEPEND(ofw_pcibus, pci, 1, 1, 1);

static int
ofw_pcibus_probe(device_t dev)
{

	if (ofw_bus_get_node(dev) == 0)
		return (ENXIO);
	device_set_desc(dev, "OFW PCI bus");

	return (0);
}

/*
 * Perform miscellaneous setups the firmware usually does not do for us.
 */
static void
ofw_pcibus_setup_device(device_t bridge, u_int busno, u_int slot, u_int func)
{
	u_int lat, clnsz;

	/*
	 * Initialize the latency timer register for busmaster devices to work
	 * properly. This is another task which the firmware does not always
	 * perform. The Min_Gnt register can be used to compute it's recommended
	 * value: it contains the desired latency in units of 1/4 us. To
	 * calculate the correct latency timer value, a bus clock of 33MHz and
	 * no wait states should be assumed.
	 */
	lat = PCIB_READ_CONFIG(bridge, busno, slot, func, PCIR_MINGNT, 1) *
	    33 / 4;
	if (lat != 0) {
#ifdef OFW_PCI_DEBUG
		device_printf(bridge, "device %d/%d/%d: latency timer %d -> "
		    "%d\n", busno, slot, func,
		    PCIB_READ_CONFIG(bridge, busno, slot, func,
			PCIR_LATTIMER, 1), lat);
#endif /* OFW_PCI_DEBUG */
		PCIB_WRITE_CONFIG(bridge, busno, slot, func,
		    PCIR_LATTIMER, min(lat, 255), 1);
	}

	/*
	 * Compute a value to write into the cache line size register.
	 * The role of the streaming cache is unclear in write invalidate
	 * transfers, so it is made sure that it's line size is always reached.
	 */
	clnsz = max(cache.ec_linesize, STRBUF_LINESZ);
	KASSERT((clnsz / STRBUF_LINESZ) * STRBUF_LINESZ == clnsz &&
	    (clnsz / cache.ec_linesize) * cache.ec_linesize == clnsz &&
	    (clnsz / 4) * 4 == clnsz, ("bogus cache line size %d", clnsz));
	PCIB_WRITE_CONFIG(bridge, busno, slot, func, PCIR_CACHELNSZ,
	    clnsz / 4, 1);

	/*
	 * The preset in the intline register is usually wrong. Reset it to 255,
	 * so that the PCI code will reroute the interrupt if needed.
	 */
	PCIB_WRITE_CONFIG(bridge, busno, slot, func, PCIR_INTLINE,
	    PCI_INVALID_IRQ, 1);
}

static int
ofw_pcibus_attach(device_t dev)
{
	device_t pcib;
	struct ofw_pci_register pcir;
	struct ofw_pcibus_devinfo *dinfo;
	phandle_t node, child;
	char *cname;
	u_int slot, busno, func;

	pcib = device_get_parent(dev);

	/*
	 * Ask the bridge for the bus number - in some cases, we need to
	 * renumber buses, so the firmware information cannot be trusted.
	 */
	busno = pcib_get_bus(dev);
	if (bootverbose)
		device_printf(dev, "physical bus=%d\n", busno);

	node = ofw_bus_get_node(dev);
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&cname)) == -1)
			continue;

		if (OF_getprop(child, "reg", &pcir, sizeof(pcir)) == -1) {
			device_printf(dev, "<%s>: incomplete\n", cname);
			free(cname, M_OFWPROP);
			continue;
		}
		slot = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
		func = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);
		if (pci_find_bsf(busno, slot, func) != NULL)
			continue;
		ofw_pcibus_setup_device(pcib, busno, slot, func);
		dinfo = (struct ofw_pcibus_devinfo *)pci_read_device(pcib,
		    busno, slot, func, sizeof(*dinfo));
		if (dinfo != NULL) {
			dinfo->opd_name = cname;
			dinfo->opd_node = child;
			OF_getprop_alloc(child, "compatible", 1,
			    (void **)&dinfo->opd_compat);
			OF_getprop_alloc(child, "device_type", 1,
			    (void **)&dinfo->opd_type);
			OF_getprop_alloc(child, "model", 1,
			    (void **)&dinfo->opd_model);
			pci_add_child(dev, (struct pci_devinfo *)dinfo);
		} else
			free(cname, M_OFWPROP);
	}

	return (bus_generic_attach(dev));
}

static int
ofw_pcibus_assign_interrupt(device_t dev, device_t child)
{
	ofw_pci_intr_t intr;
	int isz;

	isz = OF_getprop(ofw_bus_get_node(child), "interrupts", &intr,
	    sizeof(intr));
	if (isz != sizeof(intr)) {
		/* No property; our best guess is the intpin. */
		intr = pci_get_intpin(child);
	} else if (intr >= 255) {
		/*
		 * A fully specified interrupt (including IGN), as present on
		 * SPARCengine Ultra AX and e450. Extract the INO and return it.
		 */
		return (INTINO(intr));
	}
	/*
	 * If we got intr from a property, it may or may not be an intpin.
	 * For on-board devices, it frequently is not, and is completely out
	 * of the valid intpin range. For PCI slots, it hopefully is, otherwise
	 * we will have trouble interfacing with non-OFW buses such as cardbus.
	 * Since we cannot tell which it is without violating layering, we
	 * will always use the route_interrupt method, and treat exceptions on
	 * the level they become apparent.
	 */
	return (PCIB_ROUTE_INTERRUPT(device_get_parent(dev), child, intr));
}

static const char *
ofw_pcibus_get_compat(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;
 
	dinfo = device_get_ivars(dev);
	return (dinfo->opd_compat);
}
 
static const char *
ofw_pcibus_get_model(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->opd_model);
}

static const char *
ofw_pcibus_get_name(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->opd_name);
}

static phandle_t
ofw_pcibus_get_node(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->opd_node);
}

static const char *
ofw_pcibus_get_type(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->opd_type);
}
