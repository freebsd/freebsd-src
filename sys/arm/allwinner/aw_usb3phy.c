/* $NetBSD: sunxi_usb3phy.c,v 1.1 2018/05/01 23:59:42 jmcneill Exp $ */

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Allwinner USB3PHY
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/extres/phy/phy_usb.h>

#include "phynode_if.h"

#define	USB3PHY_APP			0x00
#define	 APP_FORCE_VBUS			(0x3 << 12)

#define	USB3PHY_PIPE_CLOCK_CONTROL	0x14
#define	 PCC_PIPE_CLK_OPEN		(1 << 6)

#define	USB3PHY_PHY_TUNE_LOW		0x18
#define	 PTL_MAGIC			0x0047fc87

#define	USB3PHY_PHY_TUNE_HIGH		0x1c
#define	 PTH_TX_DEEMPH_3P5DB		(0x1F << 19)
#define	 PTH_TX_DEEMPH_6DB		(0x3F << 13)
#define	 PTH_TX_SWING_FULL		(0x7F << 6)
#define	 PTH_LOS_BIAS			(0x7 << 3)
#define	 PTH_TX_BOOST_LVL		(0x7 << 0)

#define	USB3PHY_PHY_EXTERNAL_CONTROL	0x20
#define	 PEC_REF_SSP_EN			(1 << 26)
#define	 PEC_SSC_EN			(1 << 24)
#define	 PEC_EXTERN_VBUS		(0x3 << 1)

#define __LOWEST_SET_BIT(__mask) ((((__mask) - 1) & (__mask)) ^ (__mask))
#define __SHIFTIN(__x, __mask) ((__x) * __LOWEST_SET_BIT(__mask))

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun50i-h6-usb3-phy",	1 },
	{ NULL,					0 }
};

static struct resource_spec aw_usb3phy_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct awusb3phy_softc {
	struct resource *	res;
	regulator_t		reg;
	int			mode;
};

 /* Phy class and methods. */
static int awusb3phy_phy_enable(struct phynode *phy, bool enable);
static int awusb3phy_get_mode(struct phynode *phy, int *mode);
static int awusb3phy_set_mode(struct phynode *phy, int mode);
static phynode_usb_method_t awusb3phy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable, awusb3phy_phy_enable),
	PHYNODEMETHOD(phynode_usb_get_mode, awusb3phy_get_mode),
	PHYNODEMETHOD(phynode_usb_set_mode, awusb3phy_set_mode),

	PHYNODEMETHOD_END
};
DEFINE_CLASS_1(awusb3phy_phynode, awusb3phy_phynode_class, awusb3phy_phynode_methods,
  sizeof(struct phynode_usb_sc), phynode_usb_class);

#define	RD4(res, o)	bus_read_4(res, (o))
#define	WR4(res, o, v)	bus_write_4(res, (o), (v))

static int
awusb3phy_phy_enable(struct phynode *phynode, bool enable)
{
	struct awusb3phy_softc *sc;
	device_t dev;
	uint32_t val;
	int error = 0;

	dev = phynode_get_device(phynode);
	sc = device_get_softc(dev);

	device_printf(dev, "%s: called\n", __func__);

	if (enable) {
		val = RD4(sc->res, USB3PHY_PHY_EXTERNAL_CONTROL);
		device_printf(dev, "EXTERNAL_CONTROL: %x\n", val);
		val |= PEC_EXTERN_VBUS;
		val |= PEC_SSC_EN;
		val |= PEC_REF_SSP_EN;
		device_printf(dev, "EXTERNAL_CONTROL: %x\n", val);
		WR4(sc->res, USB3PHY_PHY_EXTERNAL_CONTROL, val);

		val = RD4(sc->res, USB3PHY_PIPE_CLOCK_CONTROL);
		device_printf(dev, "PIPE_CONTROL: %x\n", val);
		val |= PCC_PIPE_CLK_OPEN;
		device_printf(dev, "PIPE_CONTROL: %x\n", val);
		WR4(sc->res, USB3PHY_PIPE_CLOCK_CONTROL, val);

		val = RD4(sc->res, USB3PHY_APP);
		device_printf(dev, "APP: %x\n", val);
		val |= APP_FORCE_VBUS;
		device_printf(dev, "APP: %x\n", val);
		WR4(sc->res, USB3PHY_APP, val);

		WR4(sc->res, USB3PHY_PHY_TUNE_LOW, PTL_MAGIC);

		val = RD4(sc->res, USB3PHY_PHY_TUNE_HIGH);
		device_printf(dev, "PHY_TUNE_HIGH: %x\n", val);
		val |= PTH_TX_BOOST_LVL;
		val |= PTH_LOS_BIAS;
		val &= ~PTH_TX_SWING_FULL;
		val |= __SHIFTIN(0x55, PTH_TX_SWING_FULL);
		val &= ~PTH_TX_DEEMPH_6DB;
		val |= __SHIFTIN(0x20, PTH_TX_DEEMPH_6DB);
		val &= ~PTH_TX_DEEMPH_3P5DB;
		val |= __SHIFTIN(0x15, PTH_TX_DEEMPH_3P5DB);
		device_printf(dev, "PHY_TUNE_HIGH: %x\n", val);
		WR4(sc->res, USB3PHY_PHY_TUNE_HIGH, val);

		if (sc->reg)
			error = regulator_enable(sc->reg);
	} else {
		if (sc->reg)
			error = regulator_disable(sc->reg);
	}

	if (error != 0) {
		device_printf(dev,
		    "couldn't %s regulator for phy\n",
		    enable ? "enable" : "disable");
		return (error);
	}

	return (0);
}

static int
awusb3phy_get_mode(struct phynode *phynode, int *mode)
{
	struct awusb3phy_softc *sc;
	device_t dev;

	dev = phynode_get_device(phynode);
	sc = device_get_softc(dev);

	*mode = sc->mode;

	return (0);
}

static int
awusb3phy_set_mode(struct phynode *phynode, int mode)
{
	device_t dev;
	struct awusb3phy_softc *sc;

	dev = phynode_get_device(phynode);
	sc = device_get_softc(dev);

	if (mode != PHY_USB_MODE_HOST)
		return (EINVAL);

	sc->mode = mode;

	return (0);
}

static int
awusb3phy_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner USB3PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
awusb3phy_attach(device_t dev)
{
	struct phynode *phynode;
	struct phynode_init_def phy_init;
	struct awusb3phy_softc *sc;
	clk_t clk;
	hwreset_t rst;
	phandle_t node;
	int error, i;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (bus_alloc_resources(dev, aw_usb3phy_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	/* Enable clocks */
	for (i = 0; clk_get_by_ofw_index(dev, 0, i, &clk) == 0; i++) {
		error = clk_enable(clk);
		if (error != 0) {
			device_printf(dev, "couldn't enable clock %s\n",
			    clk_get_name(clk));
			return (error);
		}
	}

	/* De-assert resets */
	for (i = 0; hwreset_get_by_ofw_idx(dev, 0, i, &rst) == 0; i++) {
		error = hwreset_deassert(rst);
		if (error != 0) {
			device_printf(dev, "couldn't de-assert reset %d\n",
			    i);
			return (error);
		}
	}

	/* Get regulators */
	regulator_get_by_ofw_property(dev, node, "phy-supply", &sc->reg);

	/* Create the phy */
	phy_init.ofw_node = ofw_bus_get_node(dev);
	phynode = phynode_create(dev, &awusb3phy_phynode_class,
	    &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create USB PHY\n");
		return (ENXIO);
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to create USB PHY\n");
		return (ENXIO);
	}

	return (error);
}

static device_method_t awusb3phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awusb3phy_probe),
	DEVMETHOD(device_attach,	awusb3phy_attach),

	DEVMETHOD_END
};

static driver_t awusb3phy_driver = {
	"awusb3phy",
	awusb3phy_methods,
	sizeof(struct awusb3phy_softc)
};

/* aw_usb3phy needs to come up after regulators/gpio/etc, but before ehci/ohci */
EARLY_DRIVER_MODULE(awusb3phy, simplebus, awusb3phy_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(awusb3phy, 1);
