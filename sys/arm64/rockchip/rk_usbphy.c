/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy_usb.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/extres/syscon/syscon.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/syscon/syscon.h>
#include <dev/fdt/simple_mfd.h>
#include "phynode_if.h"
#include "phynode_usb_if.h"
#include "syscon_if.h"



/* Phy registers */
#define	UOC_CON0			0x00
#define	 UOC_CON0_SIDDQ				(1 << 13)
#define	 UOC_CON0_DISABLE			(1 <<  4)
#define	 UOC_CON0_COMMON_ON_N			(1 <<  0)

#define	UOC_CON2			0x08
#define	 UOC_CON2_SOFT_CON_SEL			(1 << 2)

#define UOC_CON3			0x0c


#define	WR4(_sc, _r, _v)	bus_write_4((_sc)->mem_res, (_r), (_v))
#define	RD4(_sc, _r)		bus_read_4((_sc)->mem_res, (_r))


static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-usb-phy",	1},
	{NULL,				0},
};

struct rk_usbphy_softc {
	device_t		dev;
};

struct rk_phynode_sc {
	struct phynode_usb_sc	usb_sc;
	uint32_t		base;
	int			mode;
	clk_t			clk;
	hwreset_t		hwreset;
	regulator_t		supply_vbus;
	struct syscon		*syscon;
};

static int
rk_phynode_phy_enable(struct phynode *phy, bool enable)
{
	struct rk_phynode_sc *sc;
	int rv;

	sc = phynode_get_softc(phy);

	rv = SYSCON_MODIFY_4(sc->syscon,
	    sc->base + UOC_CON0,
	    UOC_CON0_SIDDQ << 16 | UOC_CON0_SIDDQ,
	    enable ? 0 : UOC_CON0_SIDDQ);

	return (rv);

}

static int
rk_phynode_get_mode(struct phynode *phynode, int *mode)
{
	struct rk_phynode_sc *sc;

	sc = phynode_get_softc(phynode);
	*mode = sc->mode;
	return (0);
}

static int
rk_phynode_set_mode(struct phynode *phynode, int mode)
{
	struct rk_phynode_sc *sc;

	sc = phynode_get_softc(phynode);
	sc->mode = mode;

	return (0);
}


 /* Phy controller class and methods. */
static phynode_method_t rk_phynode_methods[] = {
	PHYNODEUSBMETHOD(phynode_enable,	rk_phynode_phy_enable),
	PHYNODEMETHOD(phynode_usb_get_mode,	rk_phynode_get_mode),
	PHYNODEMETHOD(phynode_usb_set_mode,	rk_phynode_set_mode),
	PHYNODEUSBMETHOD_END
};
DEFINE_CLASS_1(rk_phynode, rk_phynode_class, rk_phynode_methods,
    sizeof(struct rk_phynode_sc), phynode_usb_class);

static int
rk_usbphy_init_phy(struct rk_usbphy_softc *sc, phandle_t node)
{
	struct phynode *phynode;
	struct phynode_init_def phy_init;
	struct rk_phynode_sc *phy_sc;
	int rv;
	uint32_t base;
	clk_t clk;
	hwreset_t hwreset;
	regulator_t supply_vbus;
	struct syscon *syscon;

	clk = NULL;
	hwreset = NULL;
	supply_vbus = NULL;

	rv = OF_getencprop(node, "reg", &base, sizeof(base));
	if (rv <= 0) {
		device_printf(sc->dev, "cannot get 'reg' property.\n");
		goto fail;
	}

	/* FDT resources. All are optional. */
	rv = clk_get_by_ofw_name(sc->dev, node, "phyclk", &clk);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev, "cannot get 'phyclk' clock.\n");
		goto fail;
	}
	rv = hwreset_get_by_ofw_name(sc->dev, node, "phy-reset", &hwreset);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev, "Cannot get 'phy-reset' reset\n");
		goto fail;
	}
	rv = regulator_get_by_ofw_property(sc->dev, node, "vbus-supply",
	     &supply_vbus);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev,  "Cannot get 'vbus' regulator.\n");
		goto fail;
	}

	rv = SYSCON_GET_HANDLE(sc->dev, &syscon);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get parent syscon\n");
		goto fail;
	}

	/* Init HW resources */
	if (hwreset != NULL) {
		rv = hwreset_assert(hwreset);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot assert reset\n");
			goto fail;
		}
	}
	if (clk != NULL) {
		rv = clk_enable(clk);
		if (rv != 0) {
			device_printf(sc->dev,
			     "Cannot enable 'phyclk' clock.\n");
			goto fail;
		}
	}

	if (hwreset != NULL) {
		rv = hwreset_deassert(hwreset);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot deassert reset\n");
			goto fail;
		}
	}

	/* Create and register phy. */
	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = 1;
	phy_init.ofw_node = node;
	phynode = phynode_create(sc->dev, &rk_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(sc->dev, "Cannot create phy.\n");
		return (ENXIO);
	}

	phy_sc = phynode_get_softc(phynode);
	phy_sc->base = base;
	phy_sc->clk = clk;
	phy_sc->hwreset = hwreset;
	phy_sc->supply_vbus = supply_vbus;
	phy_sc->syscon = syscon;
	if (phynode_register(phynode) == NULL) {
		device_printf(sc->dev, "Cannot register phy.\n");
		return (ENXIO);
	}
	/* XXX It breaks boot */
	/* rk_phynode_phy_enable(phynode, 1); */
	return (0);

fail:
	if (supply_vbus != NULL)
		 regulator_release(supply_vbus);
	if (clk != NULL)
		 clk_release(clk);
	if (hwreset != NULL)
		 hwreset_release(hwreset);

	return (ENXIO);
}

static int
rk_usbphy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip USB Phy");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_usbphy_attach(device_t dev)
{
	struct rk_usbphy_softc *sc;
	phandle_t node, child;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(sc->dev);

	/* Attach child devices */
	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		rv = rk_usbphy_init_phy(sc, child);
		if (rv != 0)
			goto fail;
	}
	return (bus_generic_attach(dev));

fail:
	return (ENXIO);
}

static int
rk_usbphy_detach(device_t dev)
{
	return (0);
}

static device_method_t rk_usbphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			rk_usbphy_probe),
	DEVMETHOD(device_attach,		rk_usbphy_attach),
	DEVMETHOD(device_detach,		rk_usbphy_detach),
	DEVMETHOD_END
};

static DEFINE_CLASS_0(rk_usbphy, rk_usbphy_driver, rk_usbphy_methods,
    sizeof(struct rk_usbphy_softc));
EARLY_DRIVER_MODULE(rk_usbphy, simplebus, rk_usbphy_driver, NULL, NULL,
    BUS_PASS_TIMER + BUS_PASS_ORDER_LAST);
