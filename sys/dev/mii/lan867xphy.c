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
 * - Revision-specific configuration for C2 and D0 silicon
 * - PHY-Level Collision Avoidance (PLCA) configuration
 * - Sleep mode (C2, D0 and later)
 * - Topology discovry (D0 and later)
 * - Signal Quality Indicator (SQI) (C2, D0 and later)
 * - Cable fault diagnostics (C2 and later)
 * - Harness Defect Detection (D0 and later)
 * - Link status (D0 and later)
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/miivar.h>
#include "miidevs.h"
#include "miibus_if.h"

static int lan867x_probe(device_t);
static int lan867x_attach(device_t);

static int lan867xphy_service(struct mii_softc *, struct mii_data *, int);
static void lan867xphy_status(struct mii_softc *);


static const struct mii_phydesc lan867xphys[] = {
	MII_PHY_DESC(SMSC, LAN867X),
	MII_PHY_END
};

static const struct mii_phy_funcs lan867xphy_funcs = {
	lan867xphy_service,
	lan867xphy_status,
	mii_phy_reset,
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
