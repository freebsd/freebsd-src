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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/miivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>
#include <dev/syscon/syscon.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>

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

#define	RK3568_GMAC0_BASE		0xfe2a0000
#define	RK3568_GMAC1_BASE		0xfe010000
#define	RK3568_GRF_GMAC0_CON0		0x0380
#define	RK3568_GRF_GMAC1_CON0		0x0388
#define	RK3568_GRF_GMAC_CON1		4	/* offset from CON0 */
#define	 RK3568_GMAC_PHY_INTF_SEL_RGMII	0x00fc0010
#define	 RK3568_GMAC_PHY_INTF_SEL_RMII	0x00fc0040
#define	 RK3568_GMAC_RXCLK_DLY_ENABLE	0x00020002
#define	 RK3568_GMAC_TXCLK_DLY_ENABLE	0x00010001
#define	 RK3568_GMAC_CLK_RX_DL_CFG(val)	(0x7f000000 | ((val) << 8))
#define	 RK3568_GMAC_CLK_TX_DL_CFG(val)	(0x007f0000 | (val))

struct if_dwc_rk_softc;

typedef int (*if_dwc_rk_initfn_t)(struct if_dwc_rk_softc *);
typedef void (*if_dwc_rk_set_delaysfn_t)(struct if_dwc_rk_softc *);
typedef int (*if_dwc_rk_set_speedfn_t)(struct if_dwc_rk_softc *, int);
typedef void (*if_dwc_rk_set_phy_modefn_t)(struct if_dwc_rk_softc *);
typedef void (*if_dwc_rk_phy_powerupfn_t)(struct if_dwc_rk_softc *);

struct if_dwc_rk_ops {
	if_dwc_rk_initfn_t		init;
	int				dwc_flags;	/* DWC_HAS_GMAC* */
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
	uint32_t		grf_offset;	/* RK3568: GMAC0/1 CON0 */
	struct if_dwc_rk_ops	*ops;
	/* Common clocks */
	clk_t			mac_clk_rx;
	clk_t			mac_clk_tx;
	clk_t			aclk_mac;
	clk_t			pclk_mac;
	clk_t			clk_stmmaceth;
	clk_t			clk_mac_speed;
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

static int rk3568_init(struct if_dwc_rk_softc *sc);
static void rk3568_set_delays(struct if_dwc_rk_softc *sc);
static int rk3568_set_speed(struct if_dwc_rk_softc *sc, int speed);
static void rk3568_set_phy_mode(struct if_dwc_rk_softc *sc);

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

/* The RK3568 GMAC is a DWC Ethernet QoS core (version 5.x). */
static struct if_dwc_rk_ops rk3568_ops = {
	.init = rk3568_init,
	.dwc_flags = DWC_HAS_GMAC5,
	.set_delays = rk3568_set_delays,
	.set_speed = rk3568_set_speed,
	.set_phy_mode = rk3568_set_phy_mode,
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-gmac", (uintptr_t)&rk3288_ops},
	{"rockchip,rk3328-gmac", (uintptr_t)&rk3328_ops},
	{"rockchip,rk3399-gmac", (uintptr_t)&rk3399_ops},
	{"rockchip,rk3568-gmac", (uintptr_t)&rk3568_ops},
	{NULL,			 0}
};

static void
rk3328_set_delays(struct if_dwc_rk_softc *sc)
{
	uint32_t reg;
	uint32_t tx, rx;

	if (!mii_contype_is_rgmii(sc->base.phy_mode))
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
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
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
	case MII_CONTYPE_RMII:
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
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		SYSCON_WRITE_4(sc->grf, RK3328_GRF_MAC_CON1,
		    ((MAC_CON1_GMAC2IO_INTF_SEL_MASK | MAC_CON1_GMAC2IO_RMII_MODE_MASK) << 16) |
		    MAC_CON1_GMAC2IO_INTF_RGMII);
		break;
	case MII_CONTYPE_RMII:
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

	if (!mii_contype_is_rgmii(sc->base.phy_mode))
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
rk3568_init(struct if_dwc_rk_softc *sc)
{
	bus_addr_t paddr;
	bus_size_t size;

	if (ofw_reg_to_paddr(ofw_bus_get_node(sc->base.dev), 0, &paddr, &size,
	    NULL) != 0)
		return (ENXIO);

	switch (paddr) {
	case RK3568_GMAC0_BASE:
		sc->grf_offset = RK3568_GRF_GMAC0_CON0;
		break;
	case RK3568_GMAC1_BASE:
		sc->grf_offset = RK3568_GRF_GMAC1_CON0;
		break;
	default:
		device_printf(sc->base.dev, "unknown GMAC address %#jx\n",
		    (uintmax_t)paddr);
		return (ENXIO);
	}

	return (0);
}

static void
rk3568_set_delays(struct if_dwc_rk_softc *sc)
{

	if (!mii_contype_is_rgmii(sc->base.phy_mode))
		return;

	if (bootverbose)
		device_printf(sc->base.dev,
		    "setting RK3568 RX/TX delays: %u/%u\n",
		    sc->rx_delay, sc->tx_delay);

	SYSCON_WRITE_4(sc->grf, sc->grf_offset,
	    RK3568_GMAC_CLK_RX_DL_CFG(sc->rx_delay) |
	    RK3568_GMAC_CLK_TX_DL_CFG(sc->tx_delay));
	SYSCON_WRITE_4(sc->grf, sc->grf_offset + RK3568_GRF_GMAC_CON1,
	    RK3568_GMAC_RXCLK_DLY_ENABLE | RK3568_GMAC_TXCLK_DLY_ENABLE);
}

static void
rk3568_set_phy_mode(struct if_dwc_rk_softc *sc)
{

	switch (sc->base.phy_mode) {
	case MII_CONTYPE_RGMII:
	case MII_CONTYPE_RGMII_ID:
	case MII_CONTYPE_RGMII_RXID:
	case MII_CONTYPE_RGMII_TXID:
		SYSCON_WRITE_4(sc->grf, sc->grf_offset + RK3568_GRF_GMAC_CON1,
		    RK3568_GMAC_PHY_INTF_SEL_RGMII);
		break;
	case MII_CONTYPE_RMII:
		SYSCON_WRITE_4(sc->grf, sc->grf_offset + RK3568_GRF_GMAC_CON1,
		    RK3568_GMAC_PHY_INTF_SEL_RMII);
		break;
	}
}

static int
rk3568_set_speed(struct if_dwc_rk_softc *sc, int speed)
{
	uint64_t rate;
	int error;

	/*
	 * The device tree selects the 125MHz clock for gigabit.  The other
	 * speeds need the RGMII speed clock to be switched.
	 */
	if (sc->clk_mac_speed == NULL ||
	    !mii_contype_is_rgmii(sc->base.phy_mode))
		return (0);

	switch (speed) {
	case IFM_1000_T:
	case IFM_1000_SX:
		rate = 125000000;
		break;
	case IFM_100_TX:
		rate = 25000000;
		break;
	case IFM_10_T:
		rate = 2500000;
		break;
	default:
		device_printf(sc->base.dev, "unsupported media %u\n", speed);
		return (-1);
	}

	error = clk_set_freq(sc->clk_mac_speed, rate, 0);
	if (error != 0) {
		device_printf(sc->base.dev,
		    "cannot set the speed clock to %ju Hz: %d\n",
		    (uintmax_t)rate, error);
		return (error);
	}

	return (0);
}

static int
if_dwc_rk_probe(device_t dev)
{
	struct if_dwc_rk_ops *ops;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	ops = (struct if_dwc_rk_ops *)
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (ops == NULL)
		return (ENXIO);

	device_set_desc(dev, "Rockchip Gigabit Ethernet Controller");

	/* eqos(4) also matches the RK3568 GMAC, take precedence over it. */
	return (ops == &rk3568_ops ? BUS_PROBE_VENDOR : BUS_PROBE_DEFAULT);
}

static int
if_dwc_rk_init_clocks(device_t dev)
{
	struct if_dwc_rk_softc *sc;

	sc = device_get_softc(dev);

	/* Enable clocks */

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

	/* Optional clock */
	clk_get_by_ofw_name(dev, 0, "clk_mac_speed", &sc->clk_mac_speed);

	if (sc->base.phy_mode == MII_CONTYPE_RMII) {
		if (clk_get_by_ofw_name(dev, 0, "mac_clk_rx", &sc->mac_clk_rx) != 0) {
			device_printf(sc->base.dev, "could not get mac_clk_rx clock\n");
			sc->mac_clk_rx = NULL;
		}

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

	if (sc->base.phy_mode == MII_CONTYPE_RMII) {
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
	/*
	 * Some SoCs reference the same clock from several clock-names.  On
	 * the RK3568 both mac_clk_tx and clk_mac_speed are SCLK_GMAC0_RX_TX
	 * (resp. SCLK_GMAC1_RX_TX), do not enable it twice or the clk
	 * framework will refuse to change its rate.
	 */
	if (sc->clk_mac_speed != NULL &&
	    (sc->mac_clk_tx == NULL ||
	     strcmp(clk_get_name(sc->mac_clk_tx),
	        clk_get_name(sc->clk_mac_speed)) != 0))
		clk_enable(sc->clk_mac_speed);

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
	sc->base.flags |= sc->ops->dwc_flags;
	if (sc->ops->init != NULL && sc->ops->init(sc) != 0)
		return (ENXIO);
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
if_dwc_rk_mii_clk(device_t dev)
{
	struct if_dwc_rk_softc *sc;
	uint64_t freq;
	clk_t clk;
	int rv;

	sc = device_get_softc(dev);

	/* The QoS cores get the MDIO clock range from the main clock. */
	if ((sc->base.flags & (DWC_HAS_GMAC4 | DWC_HAS_GMAC5)) != 0)
		clk = sc->base.clk_stmmaceth;
	else
		clk = sc->pclk_mac;
	if ((rv = clk_get_freq(clk, &freq)) != 0)
		return (-rv);
	freq = freq / 1000 / 1000;

	if (freq >= 60 && freq <= 100)
		return (GMAC_MII_CLK_60_100M_DIV42);
	else if (freq >= 100 && freq <= 150)
		return (GMAC_MII_CLK_100_150M_DIV62);
	else if (freq >= 20 && freq <= 35)
		return (GMAC_MII_CLK_25_35M_DIV16);
	else if (freq >= 35 && freq <= 60)
		return (GMAC_MII_CLK_35_60M_DIV26);
	else if (freq >= 150 && freq <= 250)
		return (GMAC_MII_CLK_150_250M_DIV102);
	else if (freq >= 250 && freq <= 300)
		return (GMAC_MII_CLK_250_300M_DIV124);

	return (-ERANGE);
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
	DEVMETHOD(if_dwc_mii_clk,	if_dwc_rk_mii_clk),
	DEVMETHOD(if_dwc_set_speed,	if_dwc_rk_set_speed),

	DEVMETHOD_END
};

extern driver_t dwc_driver;

DEFINE_CLASS_1(dwc, dwc_rk_driver, if_dwc_rk_methods,
    sizeof(struct if_dwc_rk_softc), dwc_driver);
DRIVER_MODULE(dwc_rk, simplebus, dwc_rk_driver, 0, 0);
MODULE_DEPEND(dwc_rk, dwc, 1, 1, 1);
