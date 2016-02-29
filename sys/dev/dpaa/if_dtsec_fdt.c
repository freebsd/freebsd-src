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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "miibus_if.h"

#include <contrib/ncsw/inc/Peripherals/fm_port_ext.h>
#include <contrib/ncsw/inc/xx_ext.h>

#include "if_dtsec.h"


static int	dtsec_fdt_probe(device_t dev);
static int	dtsec_fdt_attach(device_t dev);

static device_method_t dtsec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dtsec_fdt_probe),
	DEVMETHOD(device_attach,	dtsec_fdt_attach),
	DEVMETHOD(device_detach,	dtsec_detach),

	DEVMETHOD(device_shutdown,	dtsec_shutdown),
	DEVMETHOD(device_suspend,	dtsec_suspend),
	DEVMETHOD(device_resume,	dtsec_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	dtsec_miibus_readreg),
	DEVMETHOD(miibus_writereg,	dtsec_miibus_writereg),
	DEVMETHOD(miibus_statchg,	dtsec_miibus_statchg),

	{ 0, 0 }
};

static driver_t dtsec_driver = {
	"dtsec",
	dtsec_methods,
	sizeof(struct dtsec_softc),
};

static devclass_t dtsec_devclass;
DRIVER_MODULE(dtsec, dpaa, dtsec_driver, dtsec_devclass, 0, 0);
DRIVER_MODULE(miibus, dtsec, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(dtsec, ether, 1, 1, 1);
MODULE_DEPEND(dtsec, miibus, 1, 1, 1);

static int
dtsec_fdt_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,dpa-ethernet"))
		return (ENXIO);

	device_set_desc(dev, "Freescale Data Path Triple Speed Ethernet "
	    "Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
dtsec_fdt_attach(device_t dev)
{
	struct dtsec_softc *sc;
	phandle_t node, enet_node, phy_node;
	phandle_t fman_rxtx_node[2];
	char phy_type[6];

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (OF_getprop(node, "fsl,fman-mac", (void *)&enet_node,
	    sizeof(enet_node)) == -1) {
		device_printf(dev, "Could not load fsl,fman-mac property "
		    "from DTS\n");
		return (ENXIO);
	}

	enet_node = OF_instance_to_package(enet_node);

	if (OF_getprop(enet_node, "local-mac-address",
	    (void *)sc->sc_mac_addr, 6) == -1) {
		if (device_get_unit(dev) != 0) {
			device_printf(dev,
			    "Could not load local-mac-addr property "
			    "from DTS\n");
			return (ENXIO);
		}
		sc->sc_hidden = true;
	}

	/* Get link speed */
	if (fdt_is_compatible(enet_node, "fsl,fman-1g-mac") != 0)
		sc->sc_eth_dev_type = ETH_DTSEC;
	else if (fdt_is_compatible(enet_node, "fsl,fman-10g-mac") != 0)
		sc->sc_eth_dev_type = ETH_10GSEC;
	else
		return(ENXIO);

	/* Get MAC memory offset in SoC */
	if (OF_getprop(enet_node, "reg", (void *)&sc->sc_mac_mem_offset,
	    sizeof(sc->sc_mac_mem_offset)) <= 0)
		return (ENXIO);

	/* Get PHY address */
	if (OF_getprop(enet_node, "phy-handle", (void *)&phy_node,
	    sizeof(phy_node)) <= 0)
		return (ENXIO);

	phy_node = OF_instance_to_package(phy_node);

	if (OF_getprop(phy_node, "reg", (void *)&sc->sc_phy_addr,
	    sizeof(sc->sc_phy_addr)) <= 0)
		return (ENXIO);

	/* Get PHY connection type */
	if (OF_getprop(enet_node, "phy-connection-type", (void *)phy_type,
	    sizeof(phy_type)) <= 0)
		return (ENXIO);

	if (!strcmp(phy_type, "sgmii"))
		sc->sc_mac_enet_mode = e_ENET_MODE_SGMII_1000;
	else if (!strcmp(phy_type, "rgmii"))
		sc->sc_mac_enet_mode = e_ENET_MODE_RGMII_1000;
	else if (!strcmp(phy_type, "xgmii"))
		/* We set 10 Gigabit mode flag however we don't support it */
		sc->sc_mac_enet_mode = e_ENET_MODE_XGMII_10000;
	else
		return (ENXIO);

	/* Get RX/TX port handles */
	if (OF_getprop(enet_node, "fsl,port-handles", (void *)fman_rxtx_node,
	    sizeof(fman_rxtx_node)) <= 0)
		return (ENXIO);

	if (fman_rxtx_node[0] == 0)
		return (ENXIO);

	if (fman_rxtx_node[1] == 0)
		return (ENXIO);

	fman_rxtx_node[0] = OF_instance_to_package(fman_rxtx_node[0]);
	fman_rxtx_node[1] = OF_instance_to_package(fman_rxtx_node[1]);

	if (fdt_is_compatible(fman_rxtx_node[0], "fsl,fman-port-1g-rx") == 0)
		return (ENXIO);

	if (fdt_is_compatible(fman_rxtx_node[1], "fsl,fman-port-1g-tx") == 0)
		return (ENXIO);

	/* Get RX port HW id */
	if (OF_getprop(fman_rxtx_node[0], "reg", (void *)&sc->sc_port_rx_hw_id,
	    sizeof(sc->sc_port_rx_hw_id)) <= 0)
		return (ENXIO);

	/* Get TX port HW id */
	if (OF_getprop(fman_rxtx_node[1], "reg", (void *)&sc->sc_port_tx_hw_id,
	    sizeof(sc->sc_port_tx_hw_id)) <= 0)
		return (ENXIO);

	/* Get QMan channel */
	if (OF_getprop(fman_rxtx_node[1], "fsl,qman-channel-id",
	    (void *)&sc->sc_port_tx_qman_chan,
	    sizeof(sc->sc_port_tx_qman_chan)) <= 0)
		return (ENXIO);

	return (dtsec_attach(dev));
}
