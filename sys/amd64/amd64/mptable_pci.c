/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

/*
 * Host to PCI and PCI to PCI bridge drivers that use the MP Table to route
 * interrupts from PCI devices to I/O APICs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>
#include <machine/legacyvar.h>
#include <machine/mptable.h>
#include <machine/pci_cfgreg.h>

#include "pcib_if.h"

/* Host to PCI bridge driver. */

static int
mptable_hostb_probe(device_t dev)
{

	if (pci_cfgregopen() == 0)
		return (ENXIO);
	if (mptable_pci_probe_table(pcib_get_bus(dev)) != 0)
		return (ENXIO);
	device_set_desc(dev, "MPTable Host-PCI bridge");
	return (0);
}

static device_method_t mptable_hostb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mptable_hostb_probe),
	DEVMETHOD(device_attach,	legacy_pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	legacy_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	legacy_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	legacy_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	legacy_pcib_read_config),
	DEVMETHOD(pcib_write_config,	legacy_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	mptable_pci_route_interrupt),

	{ 0, 0 }
};

static driver_t mptable_hostb_driver = {
	"pcib",
	mptable_hostb_methods,
	1,
};

DRIVER_MODULE(mptable_pcib, legacy, mptable_hostb_driver, pcib_devclass, 0, 0);

/* PCI to PCI bridge driver. */

static int
mptable_pcib_probe(device_t dev)
{
	int bus;

	if ((pci_get_class(dev) != PCIC_BRIDGE) ||
	    (pci_get_subclass(dev) != PCIS_BRIDGE_PCI))
		return (ENXIO);
	bus = pci_read_config(dev, PCIR_SECBUS_1, 1);
	if (bus == 0)
		return (ENXIO);
	if (mptable_pci_probe_table(bus) != 0)
		return (ENXIO);
	device_set_desc(dev, "MPTable PCI-PCI bridge");
	return (-1000);
}

static device_method_t mptable_pcib_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mptable_pcib_probe),
	DEVMETHOD(device_attach,	pcib_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	pcib_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	pcib_maxslots),
	DEVMETHOD(pcib_read_config,	pcib_read_config),
	DEVMETHOD(pcib_write_config,	pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	mptable_pci_route_interrupt),

	{0, 0}
};

static driver_t mptable_pcib_driver = {
	"pcib",
	mptable_pcib_pci_methods,
	sizeof(struct pcib_softc),
};

DRIVER_MODULE(mptable_pcib, pci, mptable_pcib_driver, pcib_devclass, 0, 0);

