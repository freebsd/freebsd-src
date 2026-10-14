/*
 * Copyright 2026 Ed Maste
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Microchip LAN8670/1/2 10BASE-T1S Single Pair Ethernet (SPE) PHY Driver.
 *
 * This driver is sufficient for basic connectivity with CSMA/CD.
 *
 * TODO:
 * - Revision-specific configuration for D0 silicon
 * - PHY-Level Collision Avoidance (PLCA) configuration
 * - Sleep mode (C2, D0 and later)
 * - Topology discovery (D0 and later)
 * - Signal Quality Indicator (SQI) (C2, D0 and later)
 * - Cable fault diagnostics (C2 and later)
 * - Harness Defect Detection (D0 and later)
 * - Link status (D0 and later)
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"
#include "miibus_if.h"

/* Indirect configuration parameter access (MMD 0x1F). */
#define	LAN867X_REG_CFGPARAM_ADDR	0x00d8
#define	LAN867X_REG_CFGPARAM_DATA	0x00d9
#define	LAN867X_REG_CFGPARAM_CTRL	0x00da
#define	LAN867X_CFGPARAM_CTRL_READ	0x0002

/* Reset-complete status. */
#define	LAN867X_REG_STS2		0x0019
#define	LAN867X_STS2_RESETC		0x0800

#define	LAN867X_MMD_VEND2		0x1f	/* vendor-specific MMD 2 */

/* Silicon revisions. */
#define	LAN867X_REV_C2			5

struct lan867x_cfg_fixup {
	uint16_t address;
	uint16_t value;
};

/*
 * MMD (MDIO Manageable Device) read/write helpers.
 */
static int
lan867xphy_mmd_read(struct mii_softc *sc, int devad, int reg)
{
	PHY_WRITE(sc, MII_MMDACR, MMDACR_FN_ADDRESS |
	    (devad & MMDACR_DADDRMASK));
	PHY_WRITE(sc, MII_MMDAADR, reg);
	PHY_WRITE(sc, MII_MMDACR, MMDACR_FN_DATANPI |
	    (devad & MMDACR_DADDRMASK));
	return (PHY_READ(sc, MII_MMDAADR));
}

static void
lan867xphy_mmd_write(struct mii_softc *sc, int devad, int reg, int val)
{
	PHY_WRITE(sc, MII_MMDACR, MMDACR_FN_ADDRESS |
	    (devad & MMDACR_DADDRMASK));
	PHY_WRITE(sc, MII_MMDAADR, reg);
	PHY_WRITE(sc, MII_MMDACR, MMDACR_FN_DATANPI |
	    (devad & MMDACR_DADDRMASK));
	PHY_WRITE(sc, MII_MMDAADR, val);
}

/*
 * Microchip Indirect Read - proprietary access mechanism via Vendor specific 2
 * MMD 0x1F.
 */
static uint8_t
lan867xphy_indirect_read(struct mii_softc *sc, uint8_t addr, uint8_t mask)
{
	lan867xphy_mmd_write(sc, LAN867X_MMD_VEND2, LAN867X_REG_CFGPARAM_ADDR,
	    addr);
	lan867xphy_mmd_write(sc, LAN867X_MMD_VEND2, LAN867X_REG_CFGPARAM_CTRL,
	    LAN867X_CFGPARAM_CTRL_READ);
	return (lan867xphy_mmd_read(sc, LAN867X_MMD_VEND2,
	    LAN867X_REG_CFGPARAM_DATA) & mask);
}

/*
 * Hardware revision C2 configuration parameters.
 *
 * From Microchip AN1699 LAN8670/1/2 Configuration Application Note
 * (Revision G, October 2025).
 *
 * Registers 0x0084 (cfgparam1) and 0x008a (cfgparam2) require computed values,
 * and are written between two blocks of constants.
 */
static const struct lan867x_cfg_fixup lan867x_cfgdata_revc_part1[] = {
	{ 0x00d0, 0x3f31 },
	{ 0x00e0, 0xc000 },
};

static const struct lan867x_cfg_fixup lan867x_cfgdata_revc_part2[] = {
	{ 0x00e9, 0x9e50 },
	{ 0x00f5, 0x1cf8 },
	{ 0x00f4, 0xc020 },
	{ 0x00f8, 0xb900 },
	{ 0x00f9, 0x4e53 },
	{ 0x0081, 0x0080 },
	{ 0x0091, 0x9660 },
};

static void
lan867xphy_config(struct mii_softc *sc, const struct lan867x_cfg_fixup *fixup,
    size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		lan867xphy_mmd_write(sc, LAN867X_MMD_VEND2, fixup[i].address,
		    fixup[i].value);
	}
}

static void
lan867xphy_config_revc(struct mii_softc *sc)
{
	uint16_t cfgparam1, cfgparam2;
	uint8_t value1, value2;
	int8_t offset1, offset2;

	value1 = lan867xphy_indirect_read(sc, 0x04, 0x1f);
	if ((value1 & 0x10) != 0)
		offset1 = (int8_t)(value1 - 0x20);
	else
		offset1 = (int8_t)value1;

	value2 = lan867xphy_indirect_read(sc, 0x08, 0x1f);
	if ((value2 & 0x10) != 0)
		offset2 = (int8_t)(value2 - 0x20);
	else
		offset2 = (int8_t)value2;

	cfgparam1 = (uint16_t)(((9 + offset1) & 0x3f) << 10) |
	    (uint16_t)(((14 + offset1) & 0x3f) << 4) | 0x03;
	cfgparam2 = (uint16_t)(((40 + offset2) & 0x3f) << 10);

	lan867xphy_config(sc, lan867x_cfgdata_revc_part1,
	    nitems(lan867x_cfgdata_revc_part1));
	lan867xphy_mmd_write(sc, LAN867X_MMD_VEND2, 0x0084, cfgparam1);
	lan867xphy_mmd_write(sc, LAN867X_MMD_VEND2, 0x008a, cfgparam2);
	lan867xphy_config(sc, lan867x_cfgdata_revc_part2,
	    nitems(lan867x_cfgdata_revc_part2));
}

static int lan867x_probe(device_t);
static int lan867x_attach(device_t);

static int lan867xphy_service(struct mii_softc *, struct mii_data *, int);
static void lan867xphy_status(struct mii_softc *);


static const struct mii_phydesc lan867xphys[] = {
	MII_PHY_DESC(SMSC, LAN867X),
	MII_PHY_END
};

static bool
lan867xphy_wait_reset(struct mii_softc *sc)
{
	size_t i;
	int sts2;

	for (i = 0; i < 100; i++) {
		sts2 = lan867xphy_mmd_read(sc, LAN867X_MMD_VEND2,
		    LAN867X_REG_STS2);
		if ((sts2 & LAN867X_STS2_RESETC) != 0)
			return (true);
		DELAY(10);
	}
	device_printf(sc->mii_dev, "reset did not complete (STS2 0x%04x)\n",
	    sts2);
	return (false);
}

static void
lan867xphy_reset(struct mii_softc *sc)
{
	mii_phy_reset(sc);
	if (!lan867xphy_wait_reset(sc))
		return;

	switch (sc->mii_mpd_rev) {
	case LAN867X_REV_C2:
		lan867xphy_config_revc(sc);
		break;
	}

	/* XXX apply PLCA settings */
}

static const struct mii_phy_funcs lan867xphy_funcs = {
	lan867xphy_service,
	lan867xphy_status,
	lan867xphy_reset,
};

static int
lan867xphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	PHY_STATUS(sc);
	mii_phy_update(sc, cmd);
	return (0);
}

static void
lan867xphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;

	mii->mii_media_status = IFM_AVALID | IFM_ACTIVE;
	mii->mii_media_active = IFM_ETHER | IFM_10_T1S | IFM_HDX;
}

static int
lan867x_probe(device_t dev)
{
	return (mii_phy_dev_probe(dev, lan867xphys, BUS_PROBE_DEFAULT));
}

static int
lan867x_attach(device_t dev)
{
	struct mii_softc *sc = device_get_softc(dev);
	int media;

	mii_phy_dev_attach(dev, MIIF_NOISOLATE | MIIF_NOMANPAUSE,
	    &lan867xphy_funcs, 0);

	switch (sc->mii_mpd_rev) {
	case LAN867X_REV_C2:
		break;
	default:
		device_printf(dev,
		    "no revision-specific configuration for rev %d\n",
		    sc->mii_mpd_rev);
	}
	PHY_RESET(sc);

	/* PHY supports 10BASE-T1S only, no negotiation. */
	media = IFM_MAKEWORD(IFM_ETHER, IFM_10_T1S, IFM_HDX, sc->mii_inst);
	ifmedia_add(&sc->mii_pdata->mii_media, media, 0, NULL);
	ifmedia_set(&sc->mii_pdata->mii_media, media);

	return (0);
}

static device_method_t lan867x_methods[] = {
	DEVMETHOD(device_probe, lan867x_probe),
	DEVMETHOD(device_attach, lan867x_attach),
	DEVMETHOD(device_detach, mii_phy_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	DEVMETHOD_END
};

static driver_t lan867x_driver = {
	"lan867xphy",
	lan867x_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(lan867xphy, miibus, lan867x_driver, 0, 0);
