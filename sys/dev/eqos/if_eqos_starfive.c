/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

#include "opt_platform.h"
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/gpio.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>

#include <dev/eqos/if_eqos_var.h>

#include "if_eqos_if.h"
#include "gpio_if.h"

#include <dev/clk/clk.h>

/* JH7110's board specific code for eqos Ethernet controller driver */

#define JH7110_CSR_FREQ		198000000

#define	WR4(sc, o, v) bus_write_4(sc->base.res[EQOS_RES_MEM], (o), (v))

static const struct ofw_compat_data compat_data[] = {
	{"starfive,jh7110-dwmac",	1},
	{ NULL,				0}
};

struct if_eqos_starfive_softc {
	struct eqos_softc		base;
	clk_t				gtx;
	clk_t				tx;
	clk_t				stmmaceth;
	clk_t				pclk;
};

static int
if_eqos_starfive_set_speed(device_t dev, int speed)
{
	struct if_eqos_starfive_softc *sc = device_get_softc(dev);
	uint64_t freq;
	int err;

	switch (speed) {
	case IFM_1000_T:
	case IFM_1000_SX:
		freq = 125000000;
		break;
	case IFM_100_TX:
		freq = 25000000;
		break;
	case IFM_10_T:
		freq = 2500000;
		break;
	default:
		device_printf(dev, "unsupported media %u\n", speed);
		return (-EINVAL);
	}

	clk_set_freq(sc->gtx, freq, 0);
	err = clk_enable(sc->gtx);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->gtx));
	}

	return (0);
}



static int
if_eqos_starfive_clk_init(device_t dev)
{
	struct if_eqos_starfive_softc *sc = device_get_softc(dev);
	int err;

	if (clk_get_by_ofw_name(dev, 0, "gtx", &sc->gtx) != 0) {
		device_printf(sc->base.dev, "could not get gtx clock\n");
		return (ENXIO);
	}

	if (clk_get_by_ofw_name(dev, 0, "tx", &sc->tx) == 0) {
		err = clk_enable(sc->tx);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(sc->tx));
		}
	}
	if (clk_get_by_ofw_name(dev, 0, "stmmaceth", &sc->stmmaceth) == 0) {
		err = clk_enable(sc->stmmaceth);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(sc->stmmaceth));
		}
	}
	if (clk_get_by_ofw_name(dev, 0, "pclk", &sc->pclk) == 0) {
		err = clk_enable(sc->pclk);
		if (err != 0) {
			device_printf(dev, "Could not enable clock %s\n",
			    clk_get_name(sc->pclk));
		}
	}

	return (0);
}

static int
if_eqos_starfive_init(device_t dev)
{
	struct if_eqos_starfive_softc *sc = device_get_softc(dev);
	hwreset_t rst_ahb, rst_stmmaceth;
	phandle_t node;

	node = ofw_bus_get_node(dev);

	sc->base.ttc = 0x10;
	sc->base.rtc = 0;

	if (OF_hasprop(node, "snps,force_thresh_dma_mode"))
		sc->base.thresh_dma_mode = true;

	if (OF_hasprop(node, "snps,no-pbl-x8"))
		sc->base.pblx8 = false;

	if (OF_hasprop(node, "snps,txpbl")) {
		OF_getencprop(node, "snps,txpbl", &sc->base.txpbl,
		    sizeof(sc->base.txpbl));
	}
	if (OF_hasprop(node, "snps,rxpbl")) {
		OF_getencprop(node, "snps,rxpbl", &sc->base.rxpbl,
		    sizeof(sc->base.rxpbl));
	}

	if (hwreset_get_by_ofw_name(dev, 0, "ahb", &rst_ahb)) {
		device_printf(dev, "Cannot get ahb reset\n");
		return (ENXIO);
	}
	if (hwreset_assert(rst_ahb) != 0) {
		device_printf(dev, "Cannot assert ahb reset\n");
		return (ENXIO);
	}

	if (hwreset_get_by_ofw_name(dev, 0, "stmmaceth", &rst_stmmaceth)) {
		device_printf(dev, "Cannot get stmmaceth reset\n");
		return (ENXIO);
	}
	if (hwreset_assert(rst_stmmaceth) != 0) {
		device_printf(dev, "Cannot assert stmmaceth reset\n");
		return (ENXIO);
	}

	sc->base.csr_clock = JH7110_CSR_FREQ;
	sc->base.csr_clock_range = GMAC_MAC_MDIO_ADDRESS_CR_150_250;

	if (if_eqos_starfive_clk_init(dev) != 0) {
		device_printf(dev, "Clock initialization failed\n");
		return (ENXIO);
	}
	if (hwreset_deassert(rst_ahb) != 0) {
		device_printf(dev, "Cannot deassert rst_ahb\n");
		return (ENXIO);
	}
	if (hwreset_deassert(rst_stmmaceth) != 0) {
		device_printf(dev, "Cannot deassert rst_stmmaceth\n");
		return (ENXIO);
	}

	return (0);
}

static int
eqos_starfive_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "DesignWare EQOS Gigabit Ethernet for JH7110");

	return (BUS_PROBE_DEFAULT);
}


static device_method_t eqos_starfive_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eqos_starfive_probe),

	/* EQOS interface */
	DEVMETHOD(if_eqos_init,		if_eqos_starfive_init),
	DEVMETHOD(if_eqos_set_speed,	if_eqos_starfive_set_speed),

	DEVMETHOD_END
};

DEFINE_CLASS_1(eqos, eqos_starfive_driver, eqos_starfive_methods,
    sizeof(struct if_eqos_starfive_softc), eqos_driver);
DRIVER_MODULE(eqos_starfive, simplebus, eqos_starfive_driver, 0, 0);
