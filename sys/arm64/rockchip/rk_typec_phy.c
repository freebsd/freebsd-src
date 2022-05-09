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
 * Rockchip PHY TYPEC
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
#include <dev/extres/syscon/syscon.h>
#include <dev/extres/hwreset/hwreset.h>

#include "syscon_if.h"

#define	GRF_USB3OTG_BASE(x)	(0x2430 + (0x10 * x))
#define	GRF_USB3OTG_CON0(x)	(GRF_USB3OTG_BASE(x) + 0x0)
#define	GRF_USB3OTG_CON1(x)	(GRF_USB3OTG_BASE(x) + 0x4)
#define	 USB3OTG_CON1_U3_DIS	(1 << 0)

#define	GRF_USB3PHY_BASE(x)	(0x0e580 + (0xc * (x)))
#define	GRF_USB3PHY_CON0(x)	(GRF_USB3PHY_BASE(x) + 0x0)
#define	 USB3PHY_CON0_USB2_ONLY	(1 << 3)
#define	GRF_USB3PHY_CON1(x)	(GRF_USB3PHY_BASE(x) + 0x4)
#define	GRF_USB3PHY_CON2(x)	(GRF_USB3PHY_BASE(x) + 0x8)
#define	GRF_USB3PHY_STATUS0	0x0e5c0
#define	GRF_USB3PHY_STATUS1	0x0e5c4

#define	CMN_PLL0_VCOCAL_INIT		(0x84 << 2)
#define	CMN_PLL0_VCOCAL_ITER		(0x85 << 2)
#define	CMN_PLL0_INTDIV			(0x94 << 2)
#define	CMN_PLL0_FRACDIV		(0x95 << 2)
#define	CMN_PLL0_HIGH_THR		(0x96 << 2)
#define	CMN_PLL0_DSM_DIAG		(0x97 << 2)
#define	CMN_PLL0_SS_CTRL1		(0x98 << 2)
#define	CMN_PLL0_SS_CTRL2		(0x99 << 2)
#define	CMN_DIAG_PLL0_FBH_OVRD		(0x1c0 << 2)
#define	CMN_DIAG_PLL0_FBL_OVRD		(0x1c1 << 2)
#define	CMN_DIAG_PLL0_OVRD		(0x1c2 << 2)
#define	CMN_DIAG_PLL0_V2I_TUNE		(0x1c5 << 2)
#define	CMN_DIAG_PLL0_CP_TUNE		(0x1c6 << 2)
#define	CMN_DIAG_PLL0_LF_PROG		(0x1c7 << 2)
#define	CMN_DIAG_HSCLK_SEL		(0x1e0 << 2)
#define	 CMN_DIAG_HSCLK_SEL_PLL_CONFIG	0x30
#define	 CMN_DIAG_HSCLK_SEL_PLL_MASK	0x33

#define	TX_TXCC_MGNFS_MULT_000(lane)	((0x4050 | ((lane) << 9)) << 2)
#define	XCVR_DIAG_BIDI_CTRL(lane)	((0x40e8 | ((lane) << 9)) << 2)
#define	XCVR_DIAG_LANE_FCM_EN_MGN(lane)	((0x40f2 | ((lane) << 9)) << 2)
#define	TX_PSC_A0(lane)			((0x4100 | ((lane) << 9)) << 2)
#define	TX_PSC_A1(lane)			((0x4101 | ((lane) << 9)) << 2)
#define	TX_PSC_A2(lane)			((0x4102 | ((lane) << 9)) << 2)
#define	TX_PSC_A3(lane)			((0x4103 | ((lane) << 9)) << 2)
#define	TX_RCVDET_EN_TMR(lane)		((0x4122 | ((lane) << 9)) << 2)
#define	TX_RCVDET_ST_TMR(lane)		((0x4123 | ((lane) << 9)) << 2)

#define	RX_PSC_A0(lane)			((0x8000 | ((lane) << 9)) << 2)
#define	RX_PSC_A1(lane)			((0x8001 | ((lane) << 9)) << 2)
#define	RX_PSC_A2(lane)			((0x8002 | ((lane) << 9)) << 2)
#define	RX_PSC_A3(lane)			((0x8003 | ((lane) << 9)) << 2)
#define	RX_PSC_CAL(lane)		((0x8006 | ((lane) << 9)) << 2)
#define	RX_PSC_RDY(lane)		((0x8007 | ((lane) << 9)) << 2)
#define	RX_SIGDET_HL_FILT_TMR(lane)	((0x8090 | ((lane) << 9)) << 2)
#define	RX_REE_CTRL_DATA_MASK(lane)	((0x81bb | ((lane) << 9)) << 2)
#define	RX_DIAG_SIGDET_TUNE(lane)	((0x81dc | ((lane) << 9)) << 2)

#define	PMA_LANE_CFG			(0xc000 << 2)
#define	PIN_ASSIGN_D_F			0x5100
#define	DP_MODE_CTL			(0xc008 << 2)
#define	DP_MODE_ENTER_A2		0xc104
#define	PMA_CMN_CTRL1			(0xc800 << 2)
#define	 PMA_CMN_CTRL1_READY		(1 << 0)

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-typec-phy",	1 },
	{ NULL,				0 }
};

static struct resource_spec rk_typec_phy_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

struct rk_typec_phy_softc {
	device_t		dev;
	struct resource		*res;
	struct syscon		*grf;
	clk_t			tcpdcore;
	clk_t			tcpdphy_ref;
	hwreset_t		rst_uphy;
	hwreset_t		rst_pipe;
	hwreset_t		rst_tcphy;
	int			mode;
	int			phy_ctrl_id;
};

#define	RK_TYPEC_PHY_READ(sc, reg)		bus_read_4(sc->res, (reg))
#define	RK_TYPEC_PHY_WRITE(sc, reg, val)	bus_write_4(sc->res, (reg), (val))

/* Phy class and methods. */
static int rk_typec_phy_enable(struct phynode *phynode, bool enable);
static int rk_typec_phy_get_mode(struct phynode *phy, int *mode);
static int rk_typec_phy_set_mode(struct phynode *phy, int mode);
static phynode_method_t rk_typec_phy_phynode_methods[] = {
	PHYNODEMETHOD(phynode_enable,		rk_typec_phy_enable),
	PHYNODEMETHOD(phynode_usb_get_mode,	rk_typec_phy_get_mode),
	PHYNODEMETHOD(phynode_usb_set_mode,	rk_typec_phy_set_mode),

	PHYNODEMETHOD_END
};

DEFINE_CLASS_1(rk_typec_phy_phynode, rk_typec_phy_phynode_class,
    rk_typec_phy_phynode_methods,
    sizeof(struct phynode_usb_sc), phynode_usb_class);

enum RK3399_USBPHY {
	RK3399_TYPEC_PHY_DP = 0,
	RK3399_TYPEC_PHY_USB3,
};

static void
rk_typec_phy_set_usb2_only(struct rk_typec_phy_softc *sc, bool usb2only)
{
	uint32_t reg;

	/* Disable usb3tousb2 only */
	reg = SYSCON_READ_4(sc->grf, GRF_USB3PHY_CON0(sc->phy_ctrl_id));
	if (usb2only)
		reg |= USB3PHY_CON0_USB2_ONLY;
	else
		reg &= ~USB3PHY_CON0_USB2_ONLY;
	/* Write Mask */
	reg |= (USB3PHY_CON0_USB2_ONLY) << 16;
	SYSCON_WRITE_4(sc->grf, GRF_USB3PHY_CON0(sc->phy_ctrl_id), reg);

	/* Enable the USB3 Super Speed port */
	reg = SYSCON_READ_4(sc->grf, GRF_USB3OTG_CON1(sc->phy_ctrl_id));
	if (usb2only)
		reg |= USB3OTG_CON1_U3_DIS;
	else
		reg &= ~USB3OTG_CON1_U3_DIS;
	/* Write Mask */
	reg |= (USB3OTG_CON1_U3_DIS) << 16;
	SYSCON_WRITE_4(sc->grf, GRF_USB3OTG_CON1(sc->phy_ctrl_id), reg);
}

static int
rk_typec_phy_enable(struct phynode *phynode, bool enable)
{
	struct rk_typec_phy_softc *sc;
	device_t dev;
	intptr_t phy;
	uint32_t reg;
	int err, retry;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	rk_typec_phy_set_usb2_only(sc, false);

	err = clk_enable(sc->tcpdcore);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->tcpdcore));
		return (ENXIO);
	}
	err = clk_enable(sc->tcpdphy_ref);
	if (err != 0) {
		device_printf(dev, "Could not enable clock %s\n",
		    clk_get_name(sc->tcpdphy_ref));
		clk_disable(sc->tcpdcore);
		return (ENXIO);
	}

	hwreset_deassert(sc->rst_tcphy);

	/* 24M configuration, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, PMA_CMN_CTRL1, 0x830);
	for (int i = 0; i < 4; i++) {
		RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_LANE_FCM_EN_MGN(i), 0x90);
		RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_EN_TMR(i), 0x960);
		RK_TYPEC_PHY_WRITE(sc, TX_RCVDET_ST_TMR(i), 0x30);
	}
	reg = RK_TYPEC_PHY_READ(sc, CMN_DIAG_HSCLK_SEL);
	reg &= ~CMN_DIAG_HSCLK_SEL_PLL_MASK;
	reg |= CMN_DIAG_HSCLK_SEL_PLL_CONFIG;
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_HSCLK_SEL, reg);

	/* PLL configuration, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_VCOCAL_INIT, 0xf0);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_VCOCAL_ITER, 0x18);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_INTDIV, 0xd0);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_FRACDIV, 0x4a4a);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_HIGH_THR, 0x34);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_SS_CTRL1, 0x1ee);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_SS_CTRL2, 0x7f03);
	RK_TYPEC_PHY_WRITE(sc, CMN_PLL0_DSM_DIAG, 0x20);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_FBH_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_FBL_OVRD, 0);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_V2I_TUNE, 0x7);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_CP_TUNE, 0x45);
	RK_TYPEC_PHY_WRITE(sc, CMN_DIAG_PLL0_LF_PROG, 0x8);

	/* Configure the TX and RX line, magic values from rockchip */
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A0(0), 0x7799);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A1(0), 0x7798);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A2(0), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_PSC_A3(0), 0x5098);
	RK_TYPEC_PHY_WRITE(sc, TX_TXCC_MGNFS_MULT_000(0), 0x0);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(0), 0xbf);

	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A0(1), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A1(1), 0xa6fd);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A2(1), 0xa410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_A3(1), 0x2410);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_CAL(1), 0x23ff);
	RK_TYPEC_PHY_WRITE(sc, RX_SIGDET_HL_FILT_TMR(1), 0x13);
	RK_TYPEC_PHY_WRITE(sc, RX_REE_CTRL_DATA_MASK(1), 0x03e7);
	RK_TYPEC_PHY_WRITE(sc, RX_DIAG_SIGDET_TUNE(1), 0x1004);
	RK_TYPEC_PHY_WRITE(sc, RX_PSC_RDY(1), 0x2010);
	RK_TYPEC_PHY_WRITE(sc, XCVR_DIAG_BIDI_CTRL(1), 0xfb);

	RK_TYPEC_PHY_WRITE(sc, PMA_LANE_CFG, PIN_ASSIGN_D_F);

	RK_TYPEC_PHY_WRITE(sc, DP_MODE_CTL, DP_MODE_ENTER_A2);

	hwreset_deassert(sc->rst_uphy);

	for (retry = 10000; retry > 0; retry--) {
		reg = RK_TYPEC_PHY_READ(sc, PMA_CMN_CTRL1);
		if (reg & PMA_CMN_CTRL1_READY)
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(sc->dev, "Timeout waiting for PMA\n");
		return (ENXIO);
	}

	hwreset_deassert(sc->rst_pipe);

	return (0);
}

static int
rk_typec_phy_get_mode(struct phynode *phynode, int *mode)
{
	struct rk_typec_phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	*mode = sc->mode;

	return (0);
}

static int
rk_typec_phy_set_mode(struct phynode *phynode, int mode)
{
	struct rk_typec_phy_softc *sc;
	intptr_t phy;
	device_t dev;

	dev = phynode_get_device(phynode);
	phy = phynode_get_id(phynode);
	sc = device_get_softc(dev);

	if (phy != RK3399_TYPEC_PHY_USB3)
		return (ERANGE);

	sc->mode = mode;

	return (0);
}

static int
rk_typec_phy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip RK3399 PHY TYPEC");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_typec_phy_attach(device_t dev)
{
	struct rk_typec_phy_softc *sc;
	struct phynode_init_def phy_init;
	struct phynode *phynode;
	phandle_t node, usb3;
	phandle_t reg_prop[4];

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* 
	 * Find out which phy we are.
	 * There is not property for this so we need to know the
	 * address to use the correct GRF registers.
	 */
	if (OF_getencprop(node, "reg", reg_prop, sizeof(reg_prop)) <= 0) {
		device_printf(dev, "Cannot guess phy controller id\n");
		return (ENXIO);
	}
	switch (reg_prop[1]) {
	case 0xff7c0000:
		sc->phy_ctrl_id = 0;
		break;
	case 0xff800000:
		sc->phy_ctrl_id = 1;
		break;
	default:
		device_printf(dev, "Unknown address %x for typec-phy\n", reg_prop[1]);
		return (ENXIO);
	}

	if (bus_alloc_resources(dev, rk_typec_phy_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		goto fail;
	}

	if (syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "Cannot get syscon handle\n");
		goto fail;
	}

	if (clk_get_by_ofw_name(dev, 0, "tcpdcore", &sc->tcpdcore) != 0) {
		device_printf(dev, "Cannot get tcpdcore clock\n");
		goto fail;
	}
	if (clk_get_by_ofw_name(dev, 0, "tcpdphy-ref", &sc->tcpdphy_ref) != 0) {
		device_printf(dev, "Cannot get tcpdphy-ref clock\n");
		goto fail;
	}

	if (hwreset_get_by_ofw_name(dev, 0, "uphy", &sc->rst_uphy) != 0) {
		device_printf(dev, "Cannot get uphy reset\n");
		goto fail;
	}
	if (hwreset_get_by_ofw_name(dev, 0, "uphy-pipe", &sc->rst_pipe) != 0) {
		device_printf(dev, "Cannot get uphy-pipe reset\n");
		goto fail;
	}
	if (hwreset_get_by_ofw_name(dev, 0, "uphy-tcphy", &sc->rst_tcphy) != 0) {
		device_printf(dev, "Cannot get uphy-tcphy reset\n");
		goto fail;
	}

	/* 
	 * Make sure that the module is asserted 
	 * We need to deassert in a certain order when we enable the phy
	 */
	hwreset_assert(sc->rst_uphy);
	hwreset_assert(sc->rst_pipe);
	hwreset_assert(sc->rst_tcphy);

	/* Set the assigned clocks parent and freq */
	if (clk_set_assigned(dev, node) != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		goto fail;
	}

	/* Only usb3 port is supported right now */
	usb3 = ofw_bus_find_child(node, "usb3-port");
	if (usb3 == 0) {
		device_printf(dev, "Cannot find usb3-port child node\n");
		goto fail;
	}
	/* If the child isn't enable attach the driver
	 *  but do not register the PHY. 
	 */
	if (!ofw_bus_node_status_okay(usb3))
		return (0);

	phy_init.id = RK3399_TYPEC_PHY_USB3;
	phy_init.ofw_node = usb3;
	phynode = phynode_create(dev, &rk_typec_phy_phynode_class, &phy_init);
	if (phynode == NULL) {
		device_printf(dev, "failed to create phy usb3-port\n");
		goto fail;
	}
	if (phynode_register(phynode) == NULL) {
		device_printf(dev, "failed to register phy usb3-port\n");
		goto fail;
	}

	OF_device_register_xref(OF_xref_from_node(usb3), dev);

	return (0);

fail:
	bus_release_resources(dev, rk_typec_phy_spec, &sc->res);

	return (ENXIO);
}

static device_method_t rk_typec_phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_typec_phy_probe),
	DEVMETHOD(device_attach,	rk_typec_phy_attach),

	DEVMETHOD_END
};

static driver_t rk_typec_phy_driver = {
	"rk_typec_phy",
	rk_typec_phy_methods,
	sizeof(struct rk_typec_phy_softc)
};

EARLY_DRIVER_MODULE(rk_typec_phy, simplebus, rk_typec_phy_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(rk_typec_phy, 1);
