/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 - 2003 Thomas Moestl <tmm@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/ofw_bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/pci/ofw_pcib_subr.h>

static device_probe_t ofw_pcib_probe;
static device_attach_t ofw_pcib_attach;

static device_method_t ofw_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_pcib_probe),
	DEVMETHOD(device_attach,	ofw_pcib_attach),
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
	DEVMETHOD(pcib_route_interrupt,	ofw_pcib_gen_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	ofw_pcib_gen_get_node),

	/* ofw_pci interface */
	DEVMETHOD(ofw_pci_adjust_busrange,	ofw_pcib_gen_adjust_busrange),

	{ 0, 0 }
};

static devclass_t pcib_devclass;

DEFINE_CLASS_0(pcib, ofw_pcib_driver, ofw_pcib_methods,
    sizeof(struct ofw_pcib_gen_softc));
DRIVER_MODULE(ofw_pcib, pci, ofw_pcib_driver, pcib_devclass, 0, 0);

static int
ofw_pcib_probe(device_t dev)
{
	if ((pci_get_class(dev) == PCIC_BRIDGE) &&
	    (pci_get_subclass(dev) == PCIS_BRIDGE_PCI) &&
	    ofw_bus_get_node(dev) != 0) {
		device_set_desc(dev, "OFW PCI-PCI bridge");
		return (0);
	}
	return (ENXIO);
}

static int
ofw_pcib_attach(device_t dev)
{
	struct ofw_pcib_gen_softc *sc;

	sc = device_get_softc(dev);
	ofw_pcib_gen_setup(dev);
	pcib_attach_common(dev);
	device_add_child(dev, "pci", sc->ops_pcib_sc.secbus);
	return (bus_generic_attach(dev));
}
