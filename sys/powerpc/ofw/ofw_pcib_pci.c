/*-
 * Copyright (c) 2000 Michael Smith
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofw_bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

static int	ofw_pcib_pci_probe(device_t bus);
static int	ofw_pcib_pci_attach(device_t bus);
static phandle_t ofw_pcib_pci_get_node(device_t bus, device_t dev);

static device_method_t ofw_pcib_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_pcib_pci_probe),
	DEVMETHOD(device_attach,		ofw_pcib_pci_attach),
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
	DEVMETHOD(bus_deactivate_resource, 	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		pcib_maxslots),
	DEVMETHOD(pcib_read_config,		pcib_read_config),
	DEVMETHOD(pcib_write_config,	pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	pcib_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     ofw_pcib_pci_get_node),

	{0, 0}
};

static devclass_t pcib_devclass;

DEFINE_CLASS_0(pcib, ofw_pcib_pci_driver, ofw_pcib_pci_methods,
    sizeof(struct pcib_softc));
DRIVER_MODULE(ofw_pcib, pci, ofw_pcib_pci_driver, pcib_devclass, 0, 0);

static int
ofw_pcib_pci_probe(device_t dev)
{

	if ((pci_get_class(dev) != PCIC_BRIDGE) ||
	    (pci_get_subclass(dev) != PCIS_BRIDGE_PCI)) {
		return (ENXIO);
	}

	if (ofw_bus_get_node(dev) == 0)
		return (ENXIO);

	device_set_desc(dev, "OFW PCI-PCI bridge");
	return (0);
}

static int
ofw_pcib_pci_attach(device_t dev)
{
	pcib_attach_common(dev);

	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));
}

phandle_t
ofw_pcib_pci_get_node(device_t bridge, device_t dev)
{
	/* We have only one child, the PCI bus, so pass it our node */

	return (ofw_bus_get_node(bridge));
}

