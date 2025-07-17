/*-
 * Copyright (c) 2015 Luiz Otavio O Souza <loos@FreeBSD.org>
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
#include <sys/socket.h>
#include <sys/module.h>

#include <net/if.h>
#include <net/if_media.h>

#include <machine/bus.h>

#include <dev/mii/miivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>

#include <arm/allwinner/aw_machdep.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>

#include "if_dwc_if.h"

static int
a20_if_dwc_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "allwinner,sun7i-a20-gmac"))
		return (ENXIO);
	device_set_desc(dev, "A20 Gigabit Ethernet Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
a20_if_dwc_init(device_t dev)
{
	struct dwc_softc *sc;
	const char *tx_parent_name;
	clk_t clk_tx, clk_tx_parent;
	regulator_t reg;
	int error;

	sc = device_get_softc(dev);

	/* Configure PHY for MII or RGMII mode */
	switch(sc->phy_mode) {
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		tx_parent_name = "gmac_int_tx";
		break;
	case MII_CONTYPE_MII:
		tx_parent_name = "mii_phy_tx";
		break;
	default:
		device_printf(dev, "unsupported PHY connection type: %d",
		    sc->phy_mode);
		return (ENXIO);
	}

	error = clk_get_by_ofw_name(dev, 0, "allwinner_gmac_tx", &clk_tx);
	if (error != 0) {
		device_printf(dev, "could not get tx clk\n");
		return (error);
	}
	error = clk_get_by_name(dev, tx_parent_name, &clk_tx_parent);
	if (error != 0) {
		device_printf(dev, "could not get clock '%s'\n",
		    tx_parent_name);
		return (error);
	}
	error = clk_set_parent_by_clk(clk_tx, clk_tx_parent);
	if (error != 0) {
		device_printf(dev, "could not set tx clk parent\n");
		return (error);
	}

	/* Enable PHY regulator if applicable */
	if (regulator_get_by_ofw_property(dev, 0, "phy-supply", &reg) == 0) {
		error = regulator_enable(reg);
		if (error != 0) {
			device_printf(dev, "could not enable PHY regulator\n");
			return (error);
		}
	}

	return (0);
}

static int
a20_if_dwc_mii_clk(device_t dev)
{

	return (GMAC_MII_CLK_150_250M_DIV102);
}

static device_method_t a20_dwc_methods[] = {
	DEVMETHOD(device_probe,		a20_if_dwc_probe),

	DEVMETHOD(if_dwc_init,		a20_if_dwc_init),
	DEVMETHOD(if_dwc_mii_clk,	a20_if_dwc_mii_clk),

	DEVMETHOD_END
};

extern driver_t dwc_driver;

DEFINE_CLASS_1(dwc, a20_dwc_driver, a20_dwc_methods, sizeof(struct dwc_softc),
    dwc_driver);
DRIVER_MODULE(a20_dwc, simplebus, a20_dwc_driver, 0, 0);

MODULE_DEPEND(a20_dwc, dwc, 1, 1, 1);
