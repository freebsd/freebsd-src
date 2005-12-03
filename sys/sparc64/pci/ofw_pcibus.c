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
#include <dev/ofw/ofw_bus_subr.h>
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
static ofw_bus_get_devinfo_t ofw_pcibus_get_devinfo;

static device_method_t ofw_pcibus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_pcibus_probe),
	DEVMETHOD(device_attach,	ofw_pcibus_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pci_print_child),
	DEVMETHOD(bus_probe_nomatch,	pci_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	pci_write_ivar),
	DEVMETHOD(bus_driver_added,	pci_driver_added),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD(bus_get_resource_list, pci_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	pci_delete_resource),
	DEVMETHOD(bus_alloc_resource,	pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_child_pnpinfo_str, pci_child_pnpinfo_str_method),
	DEVMETHOD(bus_child_location_str, pci_child_location_str_method),

	/* PCI interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_enable_busmaster,	pci_enable_busmaster_method),
	DEVMETHOD(pci_disable_busmaster, pci_disable_busmaster_method),
	DEVMETHOD(pci_enable_io,	pci_enable_io_method),
	DEVMETHOD(pci_disable_io,	pci_disable_io_method),
	DEVMETHOD(pci_get_powerstate,	pci_get_powerstate_method),
	DEVMETHOD(pci_set_powerstate,	pci_set_powerstate_method),
	DEVMETHOD(pci_assign_interrupt, ofw_pcibus_assign_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofw_pcibus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{ 0, 0 }
};

struct ofw_pcibus_devinfo {
	struct pci_devinfo	opd_dinfo;
	struct ofw_bus_devinfo	opd_obdinfo;
};

struct ofw_pcibus_softc {
	phandle_t	ops_node;
};

static driver_t ofw_pcibus_driver = {
	"pci",
	ofw_pcibus_methods,
	sizeof(struct ofw_pcibus_softc),
};

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
		if (OF_getprop(child, "reg", &pcir, sizeof(pcir)) == -1)
			continue;
		slot = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
		func = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);
		ofw_pcibus_setup_device(pcib, busno, slot, func);
		dinfo = (struct ofw_pcibus_devinfo *)pci_read_device(pcib,
		    busno, slot, func, sizeof(*dinfo));
		if (dinfo == NULL)
			continue;
		if (ofw_bus_gen_setup_devinfo(&dinfo->opd_obdinfo, child) !=
		    0) {
			pci_freecfg((struct pci_devinfo *)dinfo);
			continue;
		}
		pci_add_child(dev, (struct pci_devinfo *)dinfo);
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

static const struct ofw_bus_devinfo *
ofw_pcibus_get_devinfo(device_t bus, device_t dev)
{
	struct ofw_pcibus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (&dinfo->opd_obdinfo);
}
