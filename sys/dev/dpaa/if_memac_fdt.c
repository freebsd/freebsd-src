/*-
 * Copyright (c) 2012 Semihalf.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/mii_fdt.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "miibus_if.h"

#include "dpaa_eth.h"
#include "if_memac.h"
#include "fman.h"


static int	memac_fdt_probe(device_t dev);
static int	memac_fdt_attach(device_t dev);

static device_method_t memac_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		memac_fdt_probe),
	DEVMETHOD(device_attach,	memac_fdt_attach),
	DEVMETHOD(device_detach,	memac_detach),

	DEVMETHOD(device_shutdown,	memac_shutdown),
	DEVMETHOD(device_suspend,	memac_suspend),
	DEVMETHOD(device_resume,	memac_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	memac_miibus_readreg),
	DEVMETHOD(miibus_writereg,	memac_miibus_writereg),
	DEVMETHOD(miibus_statchg,	memac_miibus_statchg),

	DEVMETHOD_END
};

DEFINE_CLASS_0(memac, memac_driver, memac_methods, sizeof(struct memac_softc));

DRIVER_MODULE(memac, fman, memac_driver, 0, 0);
DRIVER_MODULE(miibus, memac, miibus_driver, 0, 0);
MODULE_DEPEND(memac, ether, 1, 1, 1);
MODULE_DEPEND(memac, miibus, 1, 1, 1);

static int
memac_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "fsl,fman-memac"))
		return (ENXIO);

	device_set_desc(dev,
	    "Freescale Multirate Ethernet Media Access Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
memac_fdt_attach(device_t dev)
{
	struct memac_softc *sc;
	device_t phy_dev;
	phandle_t enet_node, phy_node;
	phandle_t fman_rxtx_node[2];
	pcell_t fman_tx_cell, mac_id;

	sc = device_get_softc(dev);
	enet_node = ofw_bus_get_node(dev);

	if (OF_getprop(enet_node, "local-mac-address",
	    (void *)sc->sc_base.sc_mac_addr, 6) == -1) {
		device_printf(dev,
		    "Could not load local-mac-addr property from DTS\n");
		return (ENXIO);
	}

	/* Get PHY connection type */
	sc->sc_base.sc_mac_enet_mode = mii_fdt_get_contype(enet_node);

	sc->sc_fixed_link = OF_hasprop(enet_node, "fixed-link") ||
	    (ofw_bus_find_child(enet_node, "fixed-link") != 0);
	if (!sc->sc_fixed_link) {
		OF_getprop(enet_node, "phy-handle", &phy_node, sizeof(phy_node));
		phy_node = OF_node_from_xref(phy_node);

		if (OF_getencprop(phy_node, "reg", (void *)&sc->sc_base.sc_phy_addr,
		    sizeof(sc->sc_base.sc_phy_addr)) <= 0)
			return (ENXIO);

		phy_dev = OF_device_from_xref(OF_xref_from_node(OF_parent(phy_node)));

		if (phy_dev == NULL) {
			device_printf(dev, "No PHY found.\n");
			return (ENXIO);
		}

		sc->sc_base.sc_mdio = phy_dev;
	}

	/* Get MAC memory offset in SoC */
	sc->sc_base.sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0, RF_ACTIVE);
	if (sc->sc_base.sc_mem == NULL)
		return (ENXIO);

	sc->sc_base.sc_mac_enet_mode = mii_fdt_get_contype(enet_node);

	if (sc->sc_base.sc_mac_enet_mode == MII_CONTYPE_UNKNOWN) {
		device_printf(dev, "unknown MII type, defaulting to SGMII\n");
		sc->sc_base.sc_mac_enet_mode = MII_CONTYPE_SGMII;
	}

	if (OF_getencprop(enet_node, "cell-index",
	    (void *)&mac_id, sizeof(mac_id)) <= 0)
		return (ENXIO);
	sc->sc_base.sc_eth_id = mac_id;

	/* Get RX/TX port handles */
	if (OF_getencprop(enet_node, "fsl,fman-ports", (void *)fman_rxtx_node,
	    sizeof(fman_rxtx_node)) <= 0)
		return (ENXIO);

	if (fman_rxtx_node[0] == 0)
		return (ENXIO);

	if (fman_rxtx_node[1] == 0)
		return (ENXIO);

	sc->sc_base.sc_rx_port = OF_device_from_xref(fman_rxtx_node[0]);
	sc->sc_base.sc_tx_port = OF_device_from_xref(fman_rxtx_node[1]);

	if (sc->sc_base.sc_rx_port == NULL || sc->sc_base.sc_tx_port == NULL)
		return (ENXIO);

	fman_rxtx_node[1] = OF_node_from_xref(fman_rxtx_node[1]);
	if (OF_getencprop(fman_rxtx_node[1], "cell-index", &fman_tx_cell,
	    sizeof(fman_tx_cell)) <= 0)
		return (ENXIO);
	/* Get QMan channel */
	sc->sc_base.sc_port_tx_qman_chan = fman_qman_channel_id(device_get_parent(dev),
	    fman_tx_cell);

	return (memac_attach(dev));
}
