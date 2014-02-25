/*-
 * Copyright (c) 2011-2012 Stefan Bethke.
 * Copyright (c) 2014 Adrian Chadd.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/etherswitch/mdio.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/arswitch/arswitchreg.h>
#include <dev/etherswitch/arswitch/arswitchvar.h>
#include <dev/etherswitch/arswitch/arswitch_reg.h>
#include <dev/etherswitch/arswitch/arswitch_8327.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

static void
ar8327_phy_fixup(struct arswitch_softc *sc, int phy)
{

	switch (sc->chip_rev) {
	case 1:
		/* For 100M waveform */
		arswitch_writedbg(sc->sc_dev, phy, 0, 0x02ea);
		/* Turn on Gigabit clock */
		arswitch_writedbg(sc->sc_dev, phy, 0x3d, 0x68a0);
		break;

	case 2:
		arswitch_writemmd(sc->sc_dev, phy, 0x7, 0x3c);
		arswitch_writemmd(sc->sc_dev, phy, 0x4007, 0x0);
		/* fallthrough */
	case 4:
		arswitch_writemmd(sc->sc_dev, phy, 0x3, 0x800d);
		arswitch_writemmd(sc->sc_dev, phy, 0x4003, 0x803f);

		arswitch_writedbg(sc->sc_dev, phy, 0x3d, 0x6860);
		arswitch_writedbg(sc->sc_dev, phy, 0x5, 0x2c46);
		arswitch_writedbg(sc->sc_dev, phy, 0x3c, 0x6000);
		break;
	}
}

static uint32_t
ar8327_get_pad_cfg(struct ar8327_pad_cfg *cfg)
{
	uint32_t t;

	if (!cfg)
		return (0);

	t = 0;
	switch (cfg->mode) {
	case AR8327_PAD_NC:
		break;

	case AR8327_PAD_MAC2MAC_MII:
		t = AR8327_PAD_MAC_MII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_MAC_MII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_MAC_MII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC2MAC_GMII:
		t = AR8327_PAD_MAC_GMII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_MAC_GMII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_MAC_GMII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC_SGMII:
		t = AR8327_PAD_SGMII_EN;

		/*
		 * WAR for the Qualcomm Atheros AP136 board.
		 * It seems that RGMII TX/RX delay settings needs to be
		 * applied for SGMII mode as well, The ethernet is not
		 * reliable without this.
		 */
		t |= cfg->txclk_delay_sel << AR8327_PAD_RGMII_TXCLK_DELAY_SEL_S;
		t |= cfg->rxclk_delay_sel << AR8327_PAD_RGMII_RXCLK_DELAY_SEL_S;
		if (cfg->rxclk_delay_en)
			t |= AR8327_PAD_RGMII_RXCLK_DELAY_EN;
		if (cfg->txclk_delay_en)
			t |= AR8327_PAD_RGMII_TXCLK_DELAY_EN;

		if (cfg->sgmii_delay_en)
			t |= AR8327_PAD_SGMII_DELAY_EN;

		break;

	case AR8327_PAD_MAC2PHY_MII:
		t = AR8327_PAD_PHY_MII_EN;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_PHY_MII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_PHY_MII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC2PHY_GMII:
		t = AR8327_PAD_PHY_GMII_EN;
		if (cfg->pipe_rxclk_sel)
			t |= AR8327_PAD_PHY_GMII_PIPE_RXCLK_SEL;
		if (cfg->rxclk_sel)
			t |= AR8327_PAD_PHY_GMII_RXCLK_SEL;
		if (cfg->txclk_sel)
			t |= AR8327_PAD_PHY_GMII_TXCLK_SEL;
		break;

	case AR8327_PAD_MAC_RGMII:
		t = AR8327_PAD_RGMII_EN;
		t |= cfg->txclk_delay_sel << AR8327_PAD_RGMII_TXCLK_DELAY_SEL_S;
		t |= cfg->rxclk_delay_sel << AR8327_PAD_RGMII_RXCLK_DELAY_SEL_S;
		if (cfg->rxclk_delay_en)
			t |= AR8327_PAD_RGMII_RXCLK_DELAY_EN;
		if (cfg->txclk_delay_en)
			t |= AR8327_PAD_RGMII_TXCLK_DELAY_EN;
		break;

	case AR8327_PAD_PHY_GMII:
		t = AR8327_PAD_PHYX_GMII_EN;
		break;

	case AR8327_PAD_PHY_RGMII:
		t = AR8327_PAD_PHYX_RGMII_EN;
		break;

	case AR8327_PAD_PHY_MII:
		t = AR8327_PAD_PHYX_MII_EN;
		break;
	}

	return (t);
}

/*
 * Map the hard-coded port config from the switch setup to
 * the chipset port config (status, duplex, flow, etc.)
 */
static uint32_t
ar8327_get_port_init_status(struct ar8327_port_cfg *cfg)
{
	uint32_t t;

	if (!cfg->force_link)
		return (AR8X16_PORT_STS_LINK_AUTO);

	t = AR8X16_PORT_STS_TXMAC | AR8X16_PORT_STS_RXMAC;
	t |= cfg->duplex ? AR8X16_PORT_STS_DUPLEX : 0;
	t |= cfg->rxpause ? AR8X16_PORT_STS_RXFLOW : 0;
	t |= cfg->txpause ? AR8X16_PORT_STS_TXFLOW : 0;

	switch (cfg->speed) {
	case AR8327_PORT_SPEED_10:
		t |= AR8X16_PORT_STS_SPEED_10;
		break;
	case AR8327_PORT_SPEED_100:
		t |= AR8X16_PORT_STS_SPEED_100;
		break;
	case AR8327_PORT_SPEED_1000:
		t |= AR8X16_PORT_STS_SPEED_1000;
		break;
	}

	return (t);
}

/*
 * Initialise the ar8327 specific hardware features from
 * the hints provided in the boot environment.
 */
static int
ar8327_init_pdata(struct arswitch_softc *sc)
{
	struct ar8327_pad_cfg pc;
	struct ar8327_port_cfg port_cfg;
	uint32_t t;

	/* XXX hard-coded DB120 defaults for now! */

	/* Port 0 - rgmii; 1000/full */
	bzero(&port_cfg, sizeof(port_cfg));
	port_cfg.speed = AR8327_PORT_SPEED_1000;
	port_cfg.duplex = 1;
	port_cfg.rxpause = 1;
	port_cfg.txpause = 1;
	port_cfg.force_link = 1;
	sc->ar8327.port0_status = ar8327_get_port_init_status(&port_cfg);

	/* Port 6 - ignore */
	bzero(&port_cfg, sizeof(port_cfg));
	sc->ar8327.port6_status = ar8327_get_port_init_status(&port_cfg);

	/* Pad 0 */
	bzero(&pc, sizeof(pc));
	pc.mode = AR8327_PAD_MAC_RGMII,
	pc.txclk_delay_en = true,
	pc.rxclk_delay_en = true,
	pc.txclk_delay_sel = AR8327_CLK_DELAY_SEL1,
	pc.rxclk_delay_sel = AR8327_CLK_DELAY_SEL2,

	t = ar8327_get_pad_cfg(&pc);
#if 0
	if (AR8X16_IS_SWITCH(sc, AR8337))
		t |= AR8337_PAD_MAC06_EXCHANGE_EN;
#endif
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD0_MODE, t);

	/* Pad 5 */
	bzero(&pc, sizeof(pc));
	t = ar8327_get_pad_cfg(&pc);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD5_MODE, t);

	/* Pad 6 */
	bzero(&pc, sizeof(pc));
	t = ar8327_get_pad_cfg(&pc);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PAD6_MODE, t);

	/* XXX LED config */

	/* XXX SGMII config */

	return (0);
}

static int
ar8327_hw_setup(struct arswitch_softc *sc)
{
	int i;
	int err;

	/* pdata fetch and setup */
	err = ar8327_init_pdata(sc);
	if (err != 0)
		return (err);

	/* XXX init leds */

	for (i = 0; i < AR8327_NUM_PHYS; i++) {
		/* phy fixup */
		ar8327_phy_fixup(sc, i);

		/* start PHY autonegotiation? */
		/* XXX is this done as part of the normal PHY setup? */

	};

	/* Let things settle */
	DELAY(1000);

	return (0);
}

/*
 * Initialise other global values, for the AR8327.
 */
static int
ar8327_hw_global_setup(struct arswitch_softc *sc)
{
	uint32_t t;

	/* enable CPU port and disable mirror port */
	t = AR8327_FWD_CTRL0_CPU_PORT_EN |
	    AR8327_FWD_CTRL0_MIRROR_PORT;
	arswitch_writereg(sc->sc_dev, AR8327_REG_FWD_CTRL0, t);

	/* forward multicast and broadcast frames to CPU */
	t = (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_UC_FLOOD_S) |
	    (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_MC_FLOOD_S) |
	    (AR8327_PORTS_ALL << AR8327_FWD_CTRL1_BC_FLOOD_S);
	arswitch_writereg(sc->sc_dev, AR8327_REG_FWD_CTRL1, t);

	/* enable jumbo frames */
	/* XXX need to macro-shift the value! */
	arswitch_modifyreg(sc->sc_dev, AR8327_REG_MAX_FRAME_SIZE,
	    AR8327_MAX_FRAME_SIZE_MTU, 9018 + 8 + 2);

	/* Enable MIB counters */
	arswitch_modifyreg(sc->sc_dev, AR8327_REG_MODULE_EN,
	    AR8327_MODULE_EN_MIB, AR8327_MODULE_EN_MIB);

	return (0);
}

/*
 * Port setup.
 */
static void
ar8327_port_init(struct arswitch_softc *sc, int port)
{
	uint32_t t;

	if (port == AR8X16_PORT_CPU)
		t = sc->ar8327.port0_status;
	else if (port == 6)
		t = sc->ar8327.port6_status;
        else
#if 0
	/* XXX DB120 - hard-code port0 to 1000/full */
	if (port == 0) {
		t = AR8X16_PORT_STS_SPEED_1000;
		t |= AR8X16_PORT_STS_TXMAC | AR8X16_PORT_STS_RXMAC;
		t |= AR8X16_PORT_STS_DUPLEX;
		t |= AR8X16_PORT_STS_RXFLOW;
		t |= AR8X16_PORT_STS_TXFLOW;
	} else
#endif
		t = AR8X16_PORT_STS_LINK_AUTO;

	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_STATUS(port), t);
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_HEADER(port), 0);

	t = 1 << AR8327_PORT_VLAN0_DEF_SVID_S;
	t |= 1 << AR8327_PORT_VLAN0_DEF_CVID_S;
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN0(port), t);

	t = AR8327_PORT_VLAN1_OUT_MODE_UNTOUCH << AR8327_PORT_VLAN1_OUT_MODE_S;
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN1(port), t);

	t = AR8327_PORT_LOOKUP_LEARN;
	t |= AR8X16_PORT_CTRL_STATE_FORWARD << AR8327_PORT_LOOKUP_STATE_S;
	arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_LOOKUP(port), t);
}

static int
ar8327_port_vlan_setup(struct arswitch_softc *sc, etherswitch_port_t *p)
{

	/* XXX stub for now */
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_port_vlan_get(struct arswitch_softc *sc, etherswitch_port_t *p)
{

	/* XXX stub for now */
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static void
ar8327_reset_vlans(struct arswitch_softc *sc)
{
	int i;
	uint32_t mode, t;

	/*
	 * For now, let's default to one portgroup, just so traffic
	 * flows.  All ports can see other ports.
	 */
	for (i = 0; i < AR8327_NUM_PORTS; i++) {
		/* set pvid = i */
		t = i << AR8327_PORT_VLAN0_DEF_SVID_S;
		t |= i << AR8327_PORT_VLAN0_DEF_CVID_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN0(i), t);

		/* set egress == out_keep */
		mode = AR8327_PORT_VLAN1_OUT_MODE_UNTOUCH;

		t = AR8327_PORT_VLAN1_PORT_VLAN_PROP;
		t |= mode << AR8327_PORT_VLAN1_OUT_MODE_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_VLAN1(i), t);

		/* Set ingress = out_keep; members = 0x3f for all ports */

		t = 0x3f;	/* all ports */
		t |= AR8327_PORT_LOOKUP_LEARN;

		/* in_port_only, forward */
		t |= AR8X16_PORT_VLAN_MODE_PORT_ONLY << AR8327_PORT_LOOKUP_IN_MODE_S;
		t |= AR8X16_PORT_CTRL_STATE_FORWARD << AR8327_PORT_LOOKUP_STATE_S;
		arswitch_writereg(sc->sc_dev, AR8327_REG_PORT_LOOKUP(i), t);
	}
}

static int
ar8327_vlan_getvgroup(struct arswitch_softc *sc, etherswitch_vlangroup_t *vg)
{
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_vlan_setvgroup(struct arswitch_softc *sc, etherswitch_vlangroup_t *vg)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_get_pvid(struct arswitch_softc *sc, int port, int *pvid)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static int
ar8327_set_pvid(struct arswitch_softc *sc, int port, int pvid)
{

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

void
ar8327_attach(struct arswitch_softc *sc)
{

	sc->hal.arswitch_hw_setup = ar8327_hw_setup;
	sc->hal.arswitch_hw_global_setup = ar8327_hw_global_setup;

	sc->hal.arswitch_port_init = ar8327_port_init;
	sc->hal.arswitch_port_vlan_setup = ar8327_port_vlan_setup;
	sc->hal.arswitch_port_vlan_get = ar8327_port_vlan_get;

	sc->hal.arswitch_vlan_init_hw = ar8327_reset_vlans;
	sc->hal.arswitch_vlan_getvgroup = ar8327_vlan_getvgroup;
	sc->hal.arswitch_vlan_setvgroup = ar8327_vlan_setvgroup;
	sc->hal.arswitch_vlan_get_pvid = ar8327_get_pvid;
	sc->hal.arswitch_vlan_set_pvid = ar8327_set_pvid;

	/* Set the switch vlan capabilities. */
	sc->info.es_vlan_caps = ETHERSWITCH_VLAN_DOT1Q |
	    ETHERSWITCH_VLAN_PORT | ETHERSWITCH_VLAN_DOUBLE_TAG;
	sc->info.es_nvlangroups = AR8X16_MAX_VLANS;
}
