/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pcib_if.h"
#include "pci_if.h"

static int	ofw_pci_probe(device_t);
static const struct ofw_bus_devinfo* pci_ofw_get_devinfo(device_t, device_t);

static device_method_t ofw_pci_methods[] = {
	DEVMETHOD(device_probe,		ofw_pci_probe),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	pci_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pci, ofw_pci_driver, ofw_pci_methods, sizeof(struct pci_softc),
    pci_driver);
DRIVER_MODULE(ofw_pci, pcib, ofw_pci_driver, 0, 0);
MODULE_DEPEND(ofw_pci, simplebus, 1, 1, 1);
MODULE_DEPEND(ofw_pci, pci, 1, 1, 1);
MODULE_VERSION(ofw_pci, 1);

static int
ofw_pci_probe(device_t dev)
{
	device_t parent;

	parent = device_get_parent(dev);
	if (ofw_bus_get_node(parent) == -1)
		return (ENXIO);

	device_set_desc(dev, "OFW PCI bus");
	return (BUS_PROBE_DEFAULT);
}

/* Pass the request up to our parent. */
static const struct ofw_bus_devinfo*
pci_ofw_get_devinfo(device_t bus, device_t dev)
{

	return OFW_BUS_GET_DEVINFO(device_get_parent(bus), dev);
}
