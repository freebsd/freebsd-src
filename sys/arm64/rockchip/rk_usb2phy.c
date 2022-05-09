/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.Org>
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

/*
 * Rockchip USB2PHY
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>
#include <machine/bus.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/phy/phy_usb.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/extres/syscon/syscon.h>

#include "clkdev_if.h"
#include "syscon_if.h"

#define	RK3399_GRF_USB20_PHY0_CON0	0x0
#define	RK3399_GRF_USB20_PHY0_CON1	0x4
#define	RK3399_GRF_USB20_PHY0_CON2	0x8
#define	RK3399_GRF_USB20_PHY0_CON3	0xC

struct rk_usb2phy_reg {
	uint32_t	offset;
	uint32_t	enable_mask;
	uint32_t	disable_mask;
};

struct rk_usb2phy_regs {
	struct rk_usb2phy_reg	clk_ctl;
};

struct rk_usb2phy_regs rk3399_regs = {
	.clk_ctl = {
		/* bit 4 put pll in suspend */
		.enable_mask = 0x100000,
		.disable_mask = 0x100010,
	}
};

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-usb2phy",	(uintptr_t)&rk3399_regs },
	{ NULL,				0 }
};

struct rk_usb2phy_softc {
	device_t		dev;
	struct syscon		*grf;
	regulator_t		phy_supply;
	clk_t			clk;
	int			mode;
};

/* Phy class and methods. */
static int rk_usb2phy_enable(struct phynode *phynode, bool enable);
static int rk_usb2phy_get_mode(struct phynode *phy, int *mode);
static int rk_usb2phy_set_mode(struct phynode *phy, int mode);
static phynode_method_t rk_usb2phy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,		rk_usb2phy_enable),
	PHYNODEMETHOD(phynode_usb_get_mode,	rk_usb2phy_get_mode),
	PHYNODEMETHOD(phynode_usb_set_mode,	rk_usb2phy_set_mode),

	PHYNODEMETHOD_END
};

DEFINE_CLASS_1(rk_usb2phy_phynode, rk_usb2phy_phynode_class,
    rk_usb2phy_phynode_methods,
    sizeof(struct phynode_usb_sc), phynode_usb_class);

enum RK3399_USBPHY {
	RK3399_USBPHY_HOST = 0,
	RK3399_USBPHY_OTG,
};

static int
rk_usb2phy_enable(struct phynode *phynode, bool enable)
{
	struct rk_usb2phy_softc *sc;
	device_t dev;
	intptr_t phy;
	int error;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_USBPHY_HOST)
		return (ERANGE);

	if (sc->phy_supply) {
		if (enable)
			error = regulator_enable(sc->phy_supply);
		else
			error = regulator_disable(sc->phy_supply);
		if (error != 0) {
			device_printf(dev, "Cannot %sable the regulator\n",
			    enable ? "En" : "Dis");
			goto fail;
		}
	}

	return (0);
fail:
	return (ENXIO);
}

static int
rk_usb2phy_get_mode(struct phynode *phynode, int *mode)
{
	struct rk_usb2phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_USBPHY_HOST)
		return (ERANGE);

	*mode = sc->mode;

	return (0);
}

static int
rk_usb2phy_set_mode(struct phynode *phynode, int mode)
{
	struct rk_usb2phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_USBPHY_HOST)
		return (ERANGE);

	sc->mode = mode;

	return (0);
}

/* Clock class and method */
struct rk_usb2phy_clk_sc {
	device_t	clkdev;
	struct syscon	*grf;
	struct rk_usb2phy_regs	*regs;
};

static int
rk_usb2phy_clk_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
rk_usb2phy_clk_set_gate(struct clknode *clk, bool enable)
{
	struct rk_usb2phy_clk_sc *sc;

	sc = clknode_get_softc(clk);

	if (enable)
		SYSCON_WRITE_4(sc->grf, sc->regs->clk_ctl.offset,
		    sc->regs->clk_ctl.enable_mask);
	else
		SYSCON_WRITE_4(sc->grf, sc->regs->clk_ctl.offset,
		    sc->regs->clk_ctl.disable_mask);
	return (0);
}

static int
rk_usb2phy_clk_recalc(struct clknode *clk, uint64_t *freq)
{

	*freq = 480000000;

	return (0);
}

static clknode_method_t rk_usb2phy_clk_clknode_methods[] = {
	/* Device interface */

	CLKNODEMETHOD(clknode_init,		rk_usb2phy_clk_init),
	CLKNODEMETHOD(clknode_set_gate,		rk_usb2phy_clk_set_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	rk_usb2phy_clk_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk_usb2phy_clk_clknode, rk_usb2phy_clk_clknode_class,
    rk_usb2phy_clk_clknode_methods, sizeof(struct rk_usb2phy_clk_sc),
    clknode_class);

static int
rk_usb2phy_clk_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk)
{

	if (ncells != 0)
		return (ERANGE);

	*clk = clknode_find_by_id(clkdom, 0);

	if (*clk == NULL)
		return (ENXIO);
	return (0);
}

static int
rk_usb2phy_export_clock(struct rk_usb2phy_softc *devsc)
{
	struct clknode_init_def def;
	struct rk_usb2phy_clk_sc *sc;
	const char **clknames;
	struct clkdom *clkdom;
	struct clknode *clk;
	clk_t clk_parent;
	phandle_t node;
	phandle_t regs[2];
	int i, nclocks, ncells, error;

	node = ofw_bus_get_node(devsc->dev);

	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells != 1) {
		device_printf(devsc->dev, "couldn't find parent clock\n");
		return (ENXIO);
	}

	nclocks = ofw_bus_string_list_to_array(node, "clock-output-names",
	    &clknames);
	if (nclocks != 1)
		return (ENXIO);

	clkdom = clkdom_create(devsc->dev);
	clkdom_set_ofw_mapper(clkdom, rk_usb2phy_clk_ofw_map);

	memset(&def, 0, sizeof(def));
	def.id = 0;
	def.name = clknames[0];
	def.parent_names = malloc(sizeof(char *) * ncells, M_OFWPROP, M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(devsc->dev, 0, i, &clk_parent);
		if (error != 0) {
			device_printf(devsc->dev, "cannot get clock %d\n", error);
			return (ENXIO);
		}
		def.parent_names[i] = clk_get_name(clk_parent);
		clk_release(clk_parent);
	}
	def.parent_cnt = ncells;

	clk = clknode_create(clkdom, &rk_usb2phy_clk_clknode_class, &def);
	if (clk == NULL) {
		device_printf(devsc->dev, "cannot create clknode\n");
		return (ENXIO);
	}

	sc = clknode_get_softc(clk);
	sc->clkdev = device_get_parent(devsc->dev);
	sc->grf = devsc->grf;
	sc->regs = (struct rk_usb2phy_regs *)ofw_bus_search_compatible(devsc->dev, compat_data)->ocd_data;
	OF_getencprop(node, "reg", regs, sizeof(regs));
	sc->regs->clk_ctl.offset = regs[0];
	clknode_register(clkdom, clk);

	if (clkdom_finit(clkdom) != 0) {
		device_printf(devsc->dev, "cannot finalize clkdom initialization\n");
		return (ENXIO);
	}

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);
}

static int
rk_usb2phy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 USB2PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_usb2phy_attach(device_t dev)
{
	struct rk_usb2phy_softc *sc;
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	phandle_t node, host;
	int err;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	if (syscon_get_handle_default(dev, &sc->grf) != 0) {
		device_printf(dev, "Cannot get syscon handle\n");
		return (ENXIO);
	}

	if (clk_get_by_ofw_name(dev, 0, "phyclk", &sc->clk) != 0) {
		device_printf(dev, "Cannot get clock\n");
		return (ENXIO);
	}
	err = clk_enable(sc->clk);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->clk));
		return (ENXIO);
	}

	err = rk_usb2phy_export_clock(sc);
	if (err != 0)
		return (err);

	/* Only host is supported right now */

	host = ofw_bus_find_child(node, "host-port");
	if (host == 0) {
		device_printf(dev, "Cannot find host-port child node\n");
		return (ENXIO);
	}

	if (!ofw_bus_node_status_okay(host)) {
		device_printf(dev, "host-port isn't okay\n");
		return (0);
	}

	regulator_get_by_ofw_property(dev, host, "phy-supply", &sc->phy_supply);
	phy_init.id = RK3399_USBPHY_HOST;
	phy_init.ofw_node = host;
	phynode = phynode_create(dev, &rk_usb2phy_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create host USB2PHY\n");
		return (ENXIO);
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to register host USB2PHY\n");
		return (ENXIO);
	}

	OF_device_register_xref(OF_xref_from_node(host), dev);

	return (0);
}

static device_method_t rk_usb2phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_usb2phy_probe),
	DEVMETHOD(device_attach,	rk_usb2phy_attach),

	DEVMETHOD_END
};

static driver_t rk_usb2phy_driver = {
	"rk_usb2phy",
	rk_usb2phy_methods,
	sizeof(struct rk_usb2phy_softc)
};

EARLY_DRIVER_MODULE(rk_usb2phy, simplebus, rk_usb2phy_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_usb2phy, 1);
