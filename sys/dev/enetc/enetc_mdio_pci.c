/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/rman.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>

#include <dev/enetc/enetc_hw.h>
#include <dev/enetc/enetc_mdio.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "miibus_if.h"

#define ENETC_MDIO_DEV_ID       0xee01
#define ENETC_MDIO_DEV_NAME     "FSL PCIe IE Central MDIO"

static struct entec_mdio_pci_id {
        uint16_t vendor;
        uint16_t device;
        const char *desc;
} enetc_mdio_pci_ids[] = {
        {PCI_VENDOR_FREESCALE, ENETC_MDIO_DEV_ID, ENETC_MDIO_DEV_NAME},
};
MODULE_PNP_INFO("U16:vendor;U16:device;D:#", pci, enetc_mdio_pci,
    enetc_mdio_pci_ids, nitems(enetc_mdio_pci_ids));

struct enetc_mdio_pci_softc {
	device_t		sc_dev;
	struct mtx		sc_lock;
	struct resource		*sc_regs;
};

static device_attach_t enetc_mdio_pci_attach;
static device_detach_t enetc_mdio_pci_detach;
static device_probe_t enetc_mdio_pci_probe;

static int
enetc_mdio_pci_readreg(device_t dev, int phy, int reg)
{
	struct enetc_mdio_pci_softc *sc;
	uint32_t ret;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_lock);
	ret = enetc_mdio_read(sc->sc_regs, ENETC_EMDIO_BASE, phy, reg);
	mtx_unlock(&sc->sc_lock);

	return (ret);
}

static int
enetc_mdio_pci_writereg(device_t dev, int phy, int reg, int data)
{
	struct enetc_mdio_pci_softc *sc;
	int err;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_lock);
	err = enetc_mdio_write(sc->sc_regs, ENETC_EMDIO_BASE, phy, reg, data);
	mtx_unlock(&sc->sc_lock);
	if (err != 0)
		return (err);

	return (0);
}

static int
enetc_mdio_pci_probe(device_t dev)
{
	struct entec_mdio_pci_id *id;

	for (id = enetc_mdio_pci_ids; id->vendor != 0; ++id) {
		if (pci_get_device(dev) != id->device ||
		    pci_get_vendor(dev) != id->vendor)
			continue;

		device_set_desc(dev, id->desc);

		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
enetc_mdio_pci_attach(device_t dev)
{
	struct enetc_mdio_pci_softc *sc;
	int rid;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	/* Init locks */
	mtx_init(&sc->sc_lock, device_get_nameunit(dev), "MDIO lock", MTX_DEF);

	rid = PCIR_BAR(0);
	sc->sc_regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_regs == NULL) {
		device_printf(dev, "can't allocate resources for PCI MDIO \n");
		return (ENXIO);
	}

	OF_device_register_xref(ofw_bus_get_node(dev), dev);

	return (0);
}

static int
enetc_mdio_pci_detach(device_t dev)
{
	struct enetc_mdio_pci_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0), sc->sc_regs);
	mtx_destroy(&sc->sc_lock);

	return (0);
}

static device_method_t	enetc_mdio_pci_methods[] ={
	DEVMETHOD(device_probe, enetc_mdio_pci_probe),
	DEVMETHOD(device_attach, enetc_mdio_pci_attach),
	DEVMETHOD(device_detach, enetc_mdio_pci_detach),

	DEVMETHOD(miibus_readreg, enetc_mdio_pci_readreg),
	DEVMETHOD(miibus_writereg, enetc_mdio_pci_writereg),

	DEVMETHOD_END
};

static driver_t	enetc_mdio_pci_driver = {
	"enetc_mdio",
	enetc_mdio_pci_methods,
	sizeof(struct enetc_mdio_pci_softc),
};

static devclass_t enetc_mdio_pci_devclass;

DRIVER_MODULE(enetc_mdio, pci, enetc_mdio_pci_driver,
    enetc_mdio_pci_devclass, 0, 0);
DRIVER_MODULE(miibus, enetc_mdio, miibus_driver, miibus_devclass,
    0, 0);
MODULE_VERSION(enetc_mdio, 1);
