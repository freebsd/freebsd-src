/*
 * Copyright (c) 2022 Jared McNeill <jmcneill@invisible.ca>
 * Copyright (c) 2022 Soren Schmidt <sos@deepcore.dk>
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
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
 * Motorcomm YT8511C/YT8511H/YT8531
 * Integrated 10/100/1000 Gigabit Ethernet phy
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#ifdef FDT
#include <dev/mii/mii_fdt.h>
#endif

#include "miidevs.h"
#include "miibus_if.h"

#define	MCOMMPHY_YT8511_OUI		0x000000
#define	MCOMMPHY_YT8511_MODEL		0x10
#define	MCOMMPHY_YT8511_REV		0x0a

#define MCOMMPHY_YT8531_MODEL           0x11

#define	EXT_REG_ADDR			0x1e
#define	EXT_REG_DATA			0x1f

/* Extended registers */
#define	PHY_CLOCK_GATING_REG		0x0c
#define	 RX_CLK_DELAY_EN		0x0001
#define	 CLK_25M_SEL			0x0006
#define	 CLK_25M_SEL_125M		3
#define	 TX_CLK_DELAY_SEL		0x00f0
#define	PHY_SLEEP_CONTROL1_REG		0x27
#define	 PLLON_IN_SLP			0x4000

/* Registers and values for YT8531 */
#define	YT8531_CHIP_CONFIG		0xa001
#define	 RXC_DLY_EN			(1 << 8)

#define	YT8531_PAD_DRSTR_CFG		0xa010
#define	 PAD_RXC_MASK			0x7
#define	 PAD_RXC_SHIFT			13
#define	 JH7110_RGMII_RXC_STRENGTH	6

#define	YT8531_RGMII_CONFIG1		0xa003
#define	 RX_DELAY_SEL_SHIFT		10
#define	 RX_DELAY_SEL_MASK		0xf
#define	 RXC_DLY_THRESH			2250
#define	 RXC_DLY_ADDON			1900
#define	 TX_DELAY_SEL_FE_MASK		0xf
#define	 TX_DELAY_SEL_FE_SHIFT		4
#define	 TX_DELAY_SEL_MASK		0xf
#define	 TX_DELAY_SEL_SHIFT		0
#define	 TX_CLK_SEL			(1 << 14)
#define	 INTERNAL_DLY_DIV		150

#define	YT8531_SYNCE_CFG		0xa012
#define	 EN_SYNC_E			(1 << 6)

#define	LOWEST_SET_BIT(mask)		((((mask) - 1) & (mask)) ^ (mask))
#define	SHIFTIN(x, mask)		((x) * LOWEST_SET_BIT(mask))

static const struct mii_phydesc mcommphys[] = {
	MII_PHY_DESC(MOTORCOMM,  YT8511),
	MII_PHY_DESC(MOTORCOMM2, YT8531),
	MII_PHY_END
};

struct mcommphy_softc {
	mii_softc_t	mii_sc;
	device_t	dev;
	u_int		rx_delay_ps;
	u_int		tx_delay_ps;
	bool		tx_10_inv;
	bool		tx_100_inv;
	bool		tx_1000_inv;
};

static void mcommphy_yt8531_speed_adjustment(struct mii_softc *sc);

static int
mcommphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	PHY_STATUS(sc);

	/*
	 * For the needs of JH7110 which has two Ethernet devices with
	 * different TX inverted configuration depending on speed used
	 */
	if (sc->mii_mpd_model == MCOMMPHY_YT8531_MODEL &&
	    (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status)) {
		mcommphy_yt8531_speed_adjustment(sc);
	}

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);

	return (0);
}

static const struct mii_phy_funcs mcommphy_funcs = {
	mcommphy_service,
	ukphy_status,
	mii_phy_reset
};

static int
mcommphy_probe(device_t dev)
{
	struct mii_attach_args *ma = device_get_ivars(dev);

	/*
	 * The YT8511C reports an OUI of 0. Best we can do here is to match
	 * exactly the contents of the PHY identification registers.
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MCOMMPHY_YT8511_OUI &&
	    MII_MODEL(ma->mii_id2) == MCOMMPHY_YT8511_MODEL &&
	    MII_REV(ma->mii_id2) == MCOMMPHY_YT8511_REV) {
		device_set_desc(dev, "Motorcomm YT8511 media interface");
		return (BUS_PROBE_DEFAULT);
	}

	/* YT8531 follows a conventional procedure */
	return (mii_phy_dev_probe(dev, mcommphys, BUS_PROBE_DEFAULT));
}

static void
mcommphy_yt8511_setup(struct mii_softc *sc)
{
	uint16_t oldaddr, data;

	oldaddr = PHY_READ(sc, EXT_REG_ADDR);

	PHY_WRITE(sc, EXT_REG_ADDR, PHY_CLOCK_GATING_REG);
	data = PHY_READ(sc, EXT_REG_DATA);
	data &= ~CLK_25M_SEL;
	data |= SHIFTIN(CLK_25M_SEL_125M, CLK_25M_SEL);
	if (sc->mii_flags & MIIF_RX_DELAY) {
		data |= RX_CLK_DELAY_EN;
	} else {
		data &= ~RX_CLK_DELAY_EN;
	}
	data &= ~TX_CLK_DELAY_SEL;
	if (sc->mii_flags & MIIF_TX_DELAY) {
		data |= SHIFTIN(0xf, TX_CLK_DELAY_SEL);
	} else {
		data |= SHIFTIN(0x2, TX_CLK_DELAY_SEL);
	}
	PHY_WRITE(sc, EXT_REG_DATA, data);

	PHY_WRITE(sc, EXT_REG_ADDR, PHY_SLEEP_CONTROL1_REG);
	data = PHY_READ(sc, EXT_REG_DATA);
	data |= PLLON_IN_SLP;
	PHY_WRITE(sc, EXT_REG_DATA, data);

	PHY_WRITE(sc, EXT_REG_ADDR, oldaddr);
}

static void
mcommphy_yt8531_speed_adjustment(struct mii_softc *sc)
{
	struct mcommphy_softc *mcomm_sc = (struct mcommphy_softc *)sc;
	struct mii_data *mii = sc->mii_pdata;
	bool tx_clk_inv = false;
	uint16_t reg, oldaddr;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		tx_clk_inv = mcomm_sc->tx_1000_inv;
		break;
	case IFM_100_T:
		tx_clk_inv = mcomm_sc->tx_100_inv;
		break;
	case IFM_10_T:
		tx_clk_inv = mcomm_sc->tx_10_inv;
		break;
	}

	oldaddr = PHY_READ(sc, EXT_REG_ADDR);

	PHY_WRITE(sc, EXT_REG_ADDR, YT8531_RGMII_CONFIG1);
	reg = PHY_READ(sc, EXT_REG_DATA);
	if (tx_clk_inv)
		reg |= TX_CLK_SEL;
	else
		reg &= ~TX_CLK_SEL;
	PHY_WRITE(sc, EXT_REG_DATA, reg);

	PHY_WRITE(sc, EXT_REG_ADDR, oldaddr);
}

#ifdef FDT
static int
mcommphy_yt8531_setup_delay(struct mii_softc *sc)
{
	struct mcommphy_softc *mcomm_sc = (struct mcommphy_softc *)sc;
	uint16_t reg, oldaddr;
	int rx_delay = 0, tx_delay = 0;
	bool rxc_dly_en_off = false;

	if (mcomm_sc->rx_delay_ps > RXC_DLY_THRESH) {
		rx_delay = (mcomm_sc->rx_delay_ps - RXC_DLY_ADDON) /
		    INTERNAL_DLY_DIV;
	} else if (mcomm_sc->rx_delay_ps > 0) {
		rx_delay = mcomm_sc->rx_delay_ps / INTERNAL_DLY_DIV;
		rxc_dly_en_off = true;
	}

	if (mcomm_sc->tx_delay_ps > 0) {
		tx_delay = mcomm_sc->tx_delay_ps / INTERNAL_DLY_DIV;
	}

	oldaddr = PHY_READ(sc, EXT_REG_ADDR);

	/* Modifying Chip Config register */
	PHY_WRITE(sc, EXT_REG_ADDR, YT8531_CHIP_CONFIG);
	reg = PHY_READ(sc, EXT_REG_DATA);
	if (rxc_dly_en_off)
		reg &= ~(RXC_DLY_EN);
	PHY_WRITE(sc, EXT_REG_DATA, reg);

	/* Modifying RGMII Config1 register */
	PHY_WRITE(sc, EXT_REG_ADDR, YT8531_RGMII_CONFIG1);
	reg = PHY_READ(sc, EXT_REG_DATA);
	reg &= ~(RX_DELAY_SEL_MASK << RX_DELAY_SEL_SHIFT);
	reg |= rx_delay << RX_DELAY_SEL_SHIFT;
	reg &= ~(TX_DELAY_SEL_MASK << TX_DELAY_SEL_SHIFT);
	reg |= tx_delay << TX_DELAY_SEL_SHIFT;
	PHY_WRITE(sc, EXT_REG_DATA, reg);

	PHY_WRITE(sc, EXT_REG_ADDR, oldaddr);

	return (0);
}
#endif

static int
mcommphy_yt8531_setup(struct mii_softc *sc)
{
	uint16_t reg, oldaddr;

	oldaddr = PHY_READ(sc, EXT_REG_ADDR);

	/* Modifying Pad Drive Strength register */
	PHY_WRITE(sc, EXT_REG_ADDR, YT8531_PAD_DRSTR_CFG);
	reg = PHY_READ(sc, EXT_REG_DATA);
	reg &= ~(PAD_RXC_MASK << PAD_RXC_SHIFT);
	reg |= (JH7110_RGMII_RXC_STRENGTH << PAD_RXC_SHIFT);
	PHY_WRITE(sc, EXT_REG_DATA, reg);

	/* Modifying SyncE Config register */
	PHY_WRITE(sc, EXT_REG_ADDR, YT8531_SYNCE_CFG);
	reg = PHY_READ(sc, EXT_REG_DATA);
	reg &= ~(EN_SYNC_E);
	PHY_WRITE(sc, EXT_REG_DATA, reg);

	PHY_WRITE(sc, EXT_REG_ADDR, oldaddr);

#ifdef FDT
	if (mcommphy_yt8531_setup_delay(sc) != 0)
		return (ENXIO);
#endif

	return (0);
}

#ifdef FDT
static void
mcommphy_fdt_get_config(struct mcommphy_softc *sc)
{
	mii_fdt_phy_config_t *cfg;
	pcell_t val;

	cfg = mii_fdt_get_config(sc->dev);

	if (OF_hasprop(cfg->phynode, "motorcomm,tx-clk-10-inverted"))
		sc->tx_10_inv = true;
	if (OF_hasprop(cfg->phynode, "motorcomm,tx-clk-100-inverted"))
		sc->tx_100_inv = true;
	if (OF_hasprop(cfg->phynode, "motorcomm,tx-clk-1000-inverted"))
		sc->tx_1000_inv = true;

	/* Grab raw delay values (picoseconds); adjusted later. */
	if (OF_getencprop(cfg->phynode, "rx-internal-delay-ps", &val,
	    sizeof(val)) > 0) {
		sc->rx_delay_ps = val;
	}
	if (OF_getencprop(cfg->phynode, "tx-internal-delay-ps", &val,
	    sizeof(val)) > 0) {
		sc->tx_delay_ps = val;
	}

	mii_fdt_free_config(cfg);
}
#endif

static int
mcommphy_attach(device_t dev)
{
	struct mcommphy_softc *mcomm_sc = device_get_softc(dev);
	mii_softc_t *mii_sc = &mcomm_sc->mii_sc;
	int ret = 0;

	mcomm_sc->dev = dev;

#ifdef FDT
	mcommphy_fdt_get_config(mcomm_sc);
#endif

	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &mcommphy_funcs, 0);

	PHY_RESET(mii_sc);

	if (mii_sc->mii_mpd_model == MCOMMPHY_YT8511_MODEL)
		mcommphy_yt8511_setup(mii_sc);
	else if (mii_sc->mii_mpd_model == MCOMMPHY_YT8531_MODEL)
		ret = mcommphy_yt8531_setup(mii_sc);
	else {
		device_printf(dev, "no PHY model detected\n");
		return (ENXIO);
	}
	if (ret) {
		device_printf(dev, "PHY setup failed, error: %d\n", ret);
		return (ret);
	}

	mii_sc->mii_capabilities = PHY_READ(mii_sc, MII_BMSR) &
	    mii_sc->mii_capmask;
	if (mii_sc->mii_capabilities & BMSR_EXTSTAT)
		mii_sc->mii_extcapabilities = PHY_READ(mii_sc, MII_EXTSR);
	device_printf(dev, " ");
	mii_phy_add_media(mii_sc);
	printf("\n");

	MIIBUS_MEDIAINIT(mii_sc->mii_dev);

	return (0);
}

static device_method_t mcommphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mcommphy_probe),
	DEVMETHOD(device_attach,	mcommphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD_END
};

static driver_t mcommphy_driver = {
	"mcommphy",
	mcommphy_methods,
	sizeof(struct mcommphy_softc)
};

DRIVER_MODULE(mcommphy, miibus, mcommphy_driver, 0, 0);
