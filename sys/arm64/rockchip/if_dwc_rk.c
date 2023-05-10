/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/dwc/if_dwc.h>
#include <dev/dwc/if_dwcvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/extres/syscon/syscon.h>

#include "if_dwc_if.h"
#include "syscon_if.h"

#define	RK3328_GRF_MAC_CON0		0x0900
#define	 MAC_CON0_GMAC2IO_TX_DL_CFG_MASK	0x7F
#define	 MAC_CON0_GMAC2IO_TX_DL_CFG_SHIFT	0
#define	 MAC_CON0_GMAC2IO_RX_DL_CFG_MASK	0x7F
#define	 MAC_CON0_GMAC2IO_RX_DL_CFG_SHIFT	7

#define	RK3328_GRF_MAC_CON1		0x0904
#define	 MAC_CON1_GMAC2IO_GMAC_TXCLK_DLY_ENA	(1 << 0)
#define	 MAC_CON1_GMAC2IO_GMAC_RXCLK_DLY_ENA	(1 << 1)
#define	 MAC_CON1_GMAC2IO_GMII_CLK_SEL_MASK	(3 << 11)
#define	 MAC_CON1_GMAC2IO_GMII_CLK_SEL_125	(0 << 11)
#define	 MAC_CON1_GMAC2IO_GMII_CLK_SEL_25	(3 << 11)
#define	 MAC_CON1_GMAC2IO_GMII_CLK_SEL_2_5	(2 << 11)
#define	 MAC_CON1_GMAC2IO_RMII_MODE_MASK	(1 << 9)
#define	 MAC_CON1_GMAC2IO_RMII_MODE		(1 << 9)
#define	 MAC_CON1_GMAC2IO_INTF_SEL_MASK		(7 << 4)
#define	 MAC_CON1_GMAC2IO_INTF_RMII		(4 << 4)
#define	 MAC_CON1_GMAC2IO_INTF_RGMII		(1 << 4)
#define	 MAC_CON1_GMAC2IO_RMII_CLK_SEL_MASK	(1 << 7)
#define	 MAC_CON1_GMAC2IO_RMII_CLK_SEL_25	(1 << 7)
#define	 MAC_CON1_GMAC2IO_RMII_CLK_SEL_2_5	(0 << 7)
#define	 MAC_CON1_GMAC2IO_MAC_SPEED_MASK	(1 << 2)
#define	 MAC_CON1_GMAC2IO_MAC_SPEED_100		(1 << 2)
#define	 MAC_CON1_GMAC2IO_MAC_SPEED_10		(0 << 2)
#define	RK3328_GRF_MAC_CON2		0x0908
#define	RK3328_GRF_MACPHY_CON0		0x0B00
#define	 MACPHY_CON0_CLK_50M_MASK		(1 << 14)
#define	 MACPHY_CON0_CLK_50M			(1 << 14)
#define	 MACPHY_CON0_RMII_MODE_MASK		(3 << 6)
#define	 MACPHY_CON0_RMII_MODE			(1 << 6)
#define	RK3328_GRF_MACPHY_CON1		0x0B04
#define	 MACPHY_CON1_RMII_MODE_MASK		(1 << 9)
#define	 MACPHY_CON1_RMII_MODE			(1 << 9)
#define	RK3328_GRF_MACPHY_CON2		0x0B08
#define	RK3328_GRF_MACPHY_CON3		0x0B0C
#define	RK3328_GRF_MACPHY_STATUS	0x0B10

#define	RK3399_GRF_SOC_CON5		0xc214
#define	 SOC_CON5_GMAC_CLK_SEL_MASK		(3 << 4)
#define	 SOC_CON5_GMAC_CLK_SEL_125		(0 << 4)
#define	 SOC_CON5_GMAC_CLK_SEL_25		(3 << 4)
#define	 SOC_CON5_GMAC_CLK_SEL_2_5		(2 << 4)
#define	RK3399_GRF_SOC_CON6		0xc218
#define	 SOC_CON6_GMAC_TXCLK_DLY_ENA		(1 << 7)
#define	 SOC_CON6_TX_DL_CFG_MASK		0x7F
#define	 SOC_CON6_TX_DL_CFG_SHIFT		0
#define	 SOC_CON6_RX_DL_CFG_MASK		0x7F
#define	 SOC_CON6_GMAC_RXCLK_DLY_ENA		(1 << 15)
#define	 SOC_CON6_RX_DL_CFG_SHIFT		8

struct if_dwc_rk_softc;

typedef void (*if_dwc_rk_set_delaysfn_t)(struct if_dwc_rk_softc *);
typedef int (*if_dwc_rk_set_speedfn_t)(struct if_dwc_rk_softc *, int);
typedef void (*if_dwc_rk_set_phy_modefn_t)(struct if_dwc_rk_softc *);
typedef void (*if_dwc_rk_phy_powerupfn_t)(struct if_dwc_rk_softc *);

struct if_dwc_rk_ops {
	if_dwc_rk_set_delaysfn_t	set_delays;
	if_dwc_rk_set_speedfn_t		set_speed;
	if_dwc_rk_set_phy_modefn_t	set_phy_mode;
	if_dwc_rk_phy_powerupfn_t	phy_powerup;
};

struct if_dwc_rk_softc {
	struct dwc_softc	base;
	uint32_t		tx_delay;
	uint32_t		rx_delay;
	bool			integrated_phy;
	bool			clock_in;
	phandle_t		phy_node;
	struct syscon		*grf;
	struct if_dwc_rk_ops	*ops;
	/* Common clocks */
	clk_t			mac_clk_rx;
	clk_t			mac_clk_tx;
	clk_t			aclk_mac;
	clk_t			pclk_mac;
	clk_t			clk_stmmaceth;
	/* RMII clocks */
	clk_t			clk_mac_ref;
	clk_t			clk_mac_refout;
	/* PHY clock */
	clk_t			clk_phy;
};

static void rk3328_set_delays(struct if_dwc_rk_softc *sc);
static int rk3328_set_speed(struct if_dwc_rk_softc *sc, int speed);
static void rk3328_set_phy_mode(struct if_dwc_rk_softc *sc);
static void rk3328_phy_powerup(struct if_dwc_rk_softc *sc);

static void rk3399_set_delays(struct if_dwc_rk_softc *sc);
static int rk3399_set_speed(struct if_dwc_rk_softc *sc, int speed);

static struct if_dwc_rk_ops rk3288_ops = {
};

static struct if_dwc_rk_ops rk3328_ops = {
	.set_delays = rk3328_set_delays,
	.set_speed = rk3328_set_speed,
	.set_phy_mode = rk3328_set_phy_mode,
	.phy_powerup = rk3328_phy_powerup,
};

static struct if_dwc_rk_ops rk3399_ops = {
	.set_delays = rk3399_set_delays,
	.set_speed = rk3399_set_speed,
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-gmac", (uintptr_t)&rk3288_ops},
	{"rockchip,rk3328-gmac", (uintptr_t)&rk3328_ops},
	{"rockchip,rk3399-gmac", (uintptr_t)&rk3399_ops},
	{NULL,			 0}
};

static void
rk3328_set_delays(struct if_dwc_rk_softc *sc)
{
	uint32_t reg;
	uint32_t tx, rx;

	if (sc->base.phy_mode != PHY_MODE_RGMII)
		return;

	reg = SYSCON_READ_4(sc->grf, RK3328_GRF_MAC_CON0);
	tx = ((reg >> MAC_CON0_GMAC2IO_TX_DL_CFG_SHIFT) & MAC_CON0_GMAC2IO_TX_DL_CFG_MASK);
	rx = ((reg >> MAC_CON0_GMAC2IO_RX_DL_CFG_SHIFT) & MAC_CON0_GMAC2IO_RX_DL_CFG_MASK);

	reg = SYSCON_READ_4(sc->grf, RK3328_GRF_MAC_CON1);
	if (bootverbose) {
		device_printf(sc->base.dev, "current delays settings: tx=%u(%s) rx=%u(%s)\n",
		    tx, ((reg & MAC_CON1_GMAC2IO_GMAC_TXCLK_DLY_ENA) ? "enabled" : "disabled"),
		    rx, ((reg & MAC_CON1_GMAC2IO_GMAC_RXCLK_DLY_ENA) ? "enabled" : "disabled"));

		device_printf(sc->base.dev, "setting new RK3328 RX/TX delays:  %d/%d\n",
			sc->tx_delay, sc->rx_delay);
	}

	reg = (MAC_CON1_GMAC2IO_GMAC_TXCLK_DLY_ENA | MAC_CON1_GMAC2IO_GMAC_RXCLK_DLY_ENA) << 16;
	reg |= (MAC_CON1_GMAC2IO_GMAC_TXCLK_DLY_ENA | MAC_CON1_GMAC2IO_GMAC_RXCLK_DLY_ENA);
	SYSCON_WRITE_4(sc->grf, RK3328_GRF_MAC_CON1, reg);

	reg = 0xffff << 16;
	reg |= ((sc->tx_delay & MAC_CON0_GMAC2IO_TX_DL_CFG_MASK) <<
	    MAC_CON0_GMAC2IO_TX_DL_CFG_SHIFT);
	reg |= ((sc->rx_delay & MAC_CON0_GMAC2IO_TX_DL_CFG_MASK) <<
	    MAC_CON0_GMAC2IO_RX_DL_CFG_SHIFT);
	SYSCON_WRITE_4(sc->grf, RK3328_GRF_MAC_CON0, reg);
}

static int
rk3328_set_speed(struct if_dwc_rk_softc *sc, int speed)
{
	uint32_t reg;

	switch (sc->base.phy_mode) {
	case PHY_MODE_RGMII:
		switch (speed) {
		case IFM_1000_T:
		case IFM_1000_SX:
			reg = MAC_CON1_GMAC2IO_GMII_CLK_SEL_125;
			break;
		case IFM_100_TX:
			reg = MAC_CON1_GMAC2IO_GMII_CLK_SEL_25;
			break;
		case IFM_10_T:
			reg = MAC_CON1_GMAC2IO_GMII_CLK_SEL_2_5;
			break;
		default:
			device_printf(sc->base.dev, "unsupported RGMII media %u\n", speed);
			return (-1);
		}

		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MAC_CON1,
		    ((MAC_CON1_GMAC2IO_GMII_CLK_SEL_MASK << 16) | reg));
		break;
	case PHY_MODE_RMII:
		switch (speed) {
		case IFM_100_TX:
			reg = MAC_CON1_GMAC2IO_RMII_CLK_SEL_25 |
			    MAC_CON1_GMAC2IO_MAC_SPEED_100;
			break;
		case IFM_10_T:
			reg = MAC_CON1_GMAC2IO_RMII_CLK_SEL_2_5 |
			    MAC_CON1_GMAC2IO_MAC_SPEED_10;
			break;
		default:
			device_printf(sc->base.dev, "unsupported RMII media %u\n", speed);
			return (-1);
		}

		SYSCON_WRITE_4(sc->grf,
		    sc->integrated_phy ? RK3328_GRF_MAC_CON2 : RK3328_GRF_MAC_CON1,
		    reg |
		    ((MAC_CON1_GMAC2IO_RMII_CLK_SEL_MASK | MAC_CON1_GMAC2IO_MAC_SPEED_MASK) << 16));
		break;
	}

	return (0);
}

static void
rk3328_set_phy_mode(struct if_dwc_rk_softc *sc)
{

	switch (sc->base.phy_mode) {
	case PHY_MODE_RGMII:
		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MAC_CON1,
		    ((MAC_CON1_GMAC2IO_INTF_SEL_MASK | MAC_CON1_GMAC2IO_RMII_MODE_MASK) << 16) |
		    MAC_CON1_GMAC2IO_INTF_RGMII);
		break;
	case PHY_MODE_RMII:
		SYSCON_WRITE_4(sc->grf, sc->integrated_phy ? RK3328_GRF_MAC_CON2 : RK3328_GRF_MAC_CON1,
		    ((MAC_CON1_GMAC2IO_INTF_SEL_MASK | MAC_CON1_GMAC2IO_RMII_MODE_MASK) << 16) |
		    MAC_CON1_GMAC2IO_INTF_RMII | MAC_CON1_GMAC2IO_RMII_MODE);
		break;
	}
}

static void
rk3328_phy_powerup(struct if_dwc_rk_softc *sc)
{
	SYSCON_WRITE_4(sc->grf, RK3328_GRF_MACPHY_CON1,
	    (MACPHY_CON1_RMII_MODE_MASK << 16) |
	    MACPHY_CON1_RMII_MODE);
}

static void
rk3399_set_delays(struct if_dwc_rk_softc *sc)
{
	uint32_t reg, tx, rx;

	if (sc->base.phy_mode != PHY_MODE_RGMII)
		return;

	reg = SYSCON_READ_4(sc->grf, RK3399_GRF_SOC_CON6);
	tx = ((reg >> SOC_CON6_TX_DL_CFG_SHIFT) & SOC_CON6_TX_DL_CFG_MASK);
	rx = ((reg >> SOC_CON6_RX_DL_CFG_SHIFT) & SOC_CON6_RX_DL_CFG_MASK);

	if (bootverbose) {
		device_printf(sc->base.dev, "current delays settings: tx=%u(%s) rx=%u(%s)\n",
		    tx, ((reg & SOC_CON6_GMAC_TXCLK_DLY_ENA) ? "enabled" : "disabled"),
		    rx, ((reg & SOC_CON6_GMAC_RXCLK_DLY_ENA) ? "enabled" : "disabled"));

		device_printf(sc->base.dev, "setting new RK3399 RX/TX delays:  %d/%d\n",
		    sc->rx_delay, sc->tx_delay);
	}

	reg = 0xFFFF << 16;
	reg |= ((sc->tx_delay & SOC_CON6_TX_DL_CFG_MASK) <<
	    SOC_CON6_TX_DL_CFG_SHIFT);
	reg |= ((sc->rx_delay & SOC_CON6_RX_DL_CFG_MASK) <<
	    SOC_CON6_RX_DL_CFG_SHIFT);
	reg |= SOC_CON6_GMAC_TXCLK_DLY_ENA | SOC_CON6_GMAC_RXCLK_DLY_ENA;

	SYSCON_WRITE_4(sc->grf, RK3399_GRF_SOC_CON6, reg);
}

static int
rk3399_set_speed(struct if_dwc_rk_softc *sc, int speed)
{
	uint32_t reg;

	switch (speed) {
	case IFM_1000_T:
	case IFM_1000_SX:
		reg = SOC_CON5_GMAC_CLK_SEL_125;
		break;
	case IFM_100_TX:
		reg = SOC_CON5_GMAC_CLK_SEL_25;
		break;
	case IFM_10_T:
		reg = SOC_CON5_GMAC_CLK_SEL_2_5;
		break;
	default:
		device_printf(sc->base.dev, "unsupported media %u\n", speed);
		return (-1);
	}

	SYSCON_WRITE_4(sc->grf, RK3399_GRF_SOC_CON5,
	    ((SOC_CON5_GMAC_CLK_SEL_MASK << 16) | reg));
	return (0);
}

static int
if_dwc_rk_sysctl_delays(SYSCTL_HANDLER_ARGS)
{
	struct if_dwc_rk_softc *sc;
	int rv;
	uint32_t rxtx;

	sc = arg1;
	rxtx = ((sc->rx_delay << 8) | sc->tx_delay);

	rv = sysctl_handle_int(oidp, &rxtx, 0, req);
	if (rv != 0 || req->newptr == NULL)
		return (rv);
	sc->tx_delay = rxtx & 0xff;
	sc->rx_delay = (rxtx >> 8) & 0xff;

	if (sc->ops->set_delays)
	    sc->ops->set_delays(sc);

	return (0);
}

static int
if_dwc_rk_init_sysctl(struct if_dwc_rk_softc *sc)
{
	struct sysctl_oid *child;
	struct sysctl_ctx_list *ctx_list;

	ctx_list = device_get_sysctl_ctx(sc->base.dev);
	child = device_get_sysctl_tree(sc->base.dev);
	SYSCTL_ADD_PROC(ctx_list,
	    SYSCTL_CHILDREN(child), OID_AUTO, "delays",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, sc, 0,
	    if_dwc_rk_sysctl_delays, "", "RGMII RX/TX delays: ((rx << 8) | tx)");

	return (0);
}

static int
if_dwc_rk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);
	device_set_desc(dev, "Rockchip Gigabit Ethernet Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
if_dwc_rk_init_clocks(device_t dev)
{
	struct if_dwc_rk_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = clk_set_assigned(dev, ofw_bus_get_node(dev));
	if (error != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		return (error);
	}

	/* Enable clocks */
	error = clk_get_by_ofw_name(dev, 0, "stmmaceth", &sc->clk_stmmaceth);
	if (error != 0) {
		device_printf(dev, "could not find clock stmmaceth\n");
		return (error);
	}

	if (clk_get_by_ofw_name(dev, 0, "mac_clk_rx", &sc->mac_clk_rx) != 0) {
		device_printf(sc->base.dev, "could not get mac_clk_rx clock\n");
		sc->mac_clk_rx = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "mac_clk_tx", &sc->mac_clk_tx) != 0) {
		device_printf(sc->base.dev, "could not get mac_clk_tx clock\n");
		sc->mac_clk_tx = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "aclk_mac", &sc->aclk_mac) != 0) {
		device_printf(sc->base.dev, "could not get aclk_mac clock\n");
		sc->aclk_mac = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "pclk_mac", &sc->pclk_mac) != 0) {
		device_printf(sc->base.dev, "could not get pclk_mac clock\n");
		sc->pclk_mac = NULL;
	}

	if (sc->base.phy_mode == PHY_MODE_RGMII) {
		if (clk_get_by_ofw_name(dev, 0, "clk_mac_ref", &sc->clk_mac_ref) != 0) {
			device_printf(sc->base.dev, "could not get clk_mac_ref clock\n");
			sc->clk_mac_ref = NULL;
		}

		if (!sc->clock_in) {
			if (clk_get_by_ofw_name(dev, 0, "clk_mac_refout", &sc->clk_mac_refout) != 0) {
				device_printf(sc->base.dev, "could not get clk_mac_refout clock\n");
				sc->clk_mac_refout = NULL;
			}

			clk_set_freq(sc->clk_stmmaceth, 50000000, 0);
		}
	}

	if ((sc->phy_node != 0) && sc->integrated_phy) {
		if (clk_get_by_ofw_index(dev, sc->phy_node, 0, &sc->clk_phy) != 0) {
			device_printf(sc->base.dev, "could not get PHY clock\n");
			sc->clk_phy = NULL;
		}

		if (sc->clk_phy) {
			clk_set_freq(sc->clk_phy, 50000000, 0);
		}
	}

	if (sc->base.phy_mode == PHY_MODE_RMII) {
		if (sc->mac_clk_rx)
			clk_enable(sc->mac_clk_rx);
		if (sc->clk_mac_ref)
			clk_enable(sc->clk_mac_ref);
		if (sc->clk_mac_refout)
			clk_enable(sc->clk_mac_refout);
	}
	if (sc->clk_phy)
		clk_enable(sc->clk_phy);
	if (sc->aclk_mac)
		clk_enable(sc->aclk_mac);
	if (sc->pclk_mac)
		clk_enable(sc->pclk_mac);
	if (sc->mac_clk_tx)
		clk_enable(sc->mac_clk_tx);

	DELAY(50);

	return (0);
}

static int
if_dwc_rk_init(device_t dev)
{
	struct if_dwc_rk_softc *sc;
	phandle_t node;
	uint32_t rx, tx;
	int err;
	pcell_t phy_handle;
	char *clock_in_out;
	hwreset_t phy_reset;
	regulator_t phy_supply;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->ops = (struct if_dwc_rk_ops *)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &sc->grf) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	if (OF_getencprop(node, "tx_delay", &tx, sizeof(tx)) <= 0)
		tx = 0x30;
	if (OF_getencprop(node, "rx_delay", &rx, sizeof(rx)) <= 0)
		rx = 0x10;
	sc->tx_delay = tx;
	sc->rx_delay = rx;

	sc->clock_in = true;
	if (OF_getprop_alloc(node, "clock_in_out", (void **)&clock_in_out)) {
		if (strcmp(clock_in_out, "input") == 0)
			sc->clock_in = true;
		else
			sc->clock_in = false;
		OF_prop_free(clock_in_out);
	}

	if (OF_getencprop(node, "phy-handle", (void *)&phy_handle,
	    sizeof(phy_handle)) > 0)
		sc->phy_node = OF_node_from_xref(phy_handle);

	if (sc->phy_node)
		sc->integrated_phy = OF_hasprop(sc->phy_node, "phy-is-integrated");

	if (sc->integrated_phy)
		device_printf(sc->base.dev, "PHY is integrated\n");

	if_dwc_rk_init_clocks(dev);

	if (sc->ops->set_phy_mode)
	    sc->ops->set_phy_mode(sc);

	if (sc->ops->set_delays)
	    sc->ops->set_delays(sc);

	/*
	 * this also sets delays if tunable is defined
	 */
	err = if_dwc_rk_init_sysctl(sc);
	if (err != 0)
		return (err);

	if (regulator_get_by_ofw_property(sc->base.dev, 0,
		            "phy-supply", &phy_supply) == 0) {
		if (regulator_enable(phy_supply)) {
			device_printf(sc->base.dev,
			    "cannot enable 'phy' regulator\n");
		}
	}
	else
		device_printf(sc->base.dev, "no phy-supply property\n");

	/* Power up */
	if (sc->integrated_phy) {
		if (sc->ops->phy_powerup)
			sc->ops->phy_powerup(sc);

		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MACPHY_CON0,
		    (MACPHY_CON0_CLK_50M_MASK << 16) |
		    MACPHY_CON0_CLK_50M);
		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MACPHY_CON0,
		    (MACPHY_CON0_RMII_MODE_MASK << 16) |
		    MACPHY_CON0_RMII_MODE);
		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MACPHY_CON2, 0xffff1234);
		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MACPHY_CON3, 0x003f0035);

		if (hwreset_get_by_ofw_idx(dev, sc->phy_node, 0, &phy_reset)  == 0) {
			hwreset_assert(phy_reset);
			DELAY(20);
			hwreset_deassert(phy_reset);
			DELAY(20);
		}
	}

	return (0);
}

static int
if_dwc_rk_mac_type(device_t dev)
{

	return (DWC_GMAC_NORMAL_DESC);
}

static int
if_dwc_rk_mii_clk(device_t dev)
{

	/* Should be calculated from the clock */
	return (GMAC_MII_CLK_150_250M_DIV102);
}

static int
if_dwc_rk_set_speed(device_t dev, int speed)
{
	struct if_dwc_rk_softc *sc;

	sc = device_get_softc(dev);

	if (sc->ops->set_speed)
	    return sc->ops->set_speed(sc, speed);

	return (0);
}

static device_method_t if_dwc_rk_methods[] = {
	DEVMETHOD(device_probe,		if_dwc_rk_probe),

	DEVMETHOD(if_dwc_init,		if_dwc_rk_init),
	DEVMETHOD(if_dwc_mac_type,	if_dwc_rk_mac_type),
	DEVMETHOD(if_dwc_mii_clk,	if_dwc_rk_mii_clk),
	DEVMETHOD(if_dwc_set_speed,	if_dwc_rk_set_speed),

	DEVMETHOD_END
};

extern driver_t dwc_driver;

DEFINE_CLASS_1(dwc, dwc_rk_driver, if_dwc_rk_methods,
    sizeof(struct if_dwc_rk_softc), dwc_driver);
DRIVER_MODULE(dwc_rk, simplebus, dwc_rk_driver, 0, 0);
MODULE_DEPEND(dwc_rk, dwc, 1, 1, 1);
