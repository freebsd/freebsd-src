/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 2019 Rubicon Communications, LLC (Netgate)
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Frank van der Linden.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Driver for the Marvell 88e151x gigabit ethernet PHY. */

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
#include <dev/mii/mv88e151xreg.h>
#include "miidevs.h"

#include "miibus_if.h"

static int mv88e151x_probe(device_t);
static int mv88e151x_attach(device_t);

static device_method_t mv88e151x_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mv88e151x_probe),
	DEVMETHOD(device_attach,	mv88e151x_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD_END
};

static devclass_t mv88e151x_devclass;

static driver_t mv88e151x_driver = {
	"mv88e151x",
	mv88e151x_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(mv88e151x, miibus, mv88e151x_driver, mv88e151x_devclass, 0, 0);

static int mv88e151x_service(struct mii_softc *, struct mii_data *, int);
static void mv88e151x_status(struct mii_softc *);

static const struct mii_phydesc mv88e151xphys[] = {
	MII_PHY_DESC(xxMARVELL, E1512),
	MII_PHY_END
};

static const struct mii_phy_funcs mv88e151x_funcs = {
	mv88e151x_service,
	mv88e151x_status,
	mii_phy_reset
};

static int
mv88e151x_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, mv88e151xphys, BUS_PROBE_DEFAULT));
}

static int
mv88e151x_attach(device_t dev)
{
	const struct mii_attach_args *ma;
	struct mii_softc *sc;
	uint32_t cop_cap, cop_extcap;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);

	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &mv88e151x_funcs, 0);

	PHY_RESET(sc);
	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & sc->mii_capmask;
	cop_cap = sc->mii_capabilities;
	if (sc->mii_capabilities & BMSR_EXTSTAT) {
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
		cop_extcap = sc->mii_extcapabilities;
	}
	device_printf(dev, " ");
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxMARVELL_E1512)
		sc->mii_capabilities &= ~BMSR_ANEG;
	mii_phy_add_media(sc);
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxMARVELL_E1512) {
		/* Switch the fiber PHY and add the supported media. */
		PHY_WRITE(sc, MV88E151X_PAGE, MV88E151X_PAGE_FIBER);
		PHY_RESET(sc);
		sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & sc->mii_capmask;
		if (sc->mii_capabilities & BMSR_EXTSTAT)
			sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
		sc->mii_flags |= MIIF_NOISOLATE;
		printf(", ");
		mii_phy_add_media(sc);

		/* Switch back to copper PHY. */
		PHY_WRITE(sc, MV88E151X_PAGE, MV88E151X_PAGE_COPPER);
		sc->mii_flags &= ~MIIF_NOISOLATE;
		sc->mii_capabilities = cop_cap;
		sc->mii_extcapabilities = cop_extcap;
	}
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);

	mii_phy_setmedia(sc);

	/* Enable the fiber PHY auto negotiation. */
	if (MII_MODEL(ma->mii_id2) == MII_MODEL_xxMARVELL_E1512) {
		PHY_WRITE(sc, MV88E151X_PAGE, MV88E151X_PAGE_FIBER);
		PHY_WRITE(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN |
		    BMCR_STARTNEG | BMCR_FDX | BMCR_S1000);
		PHY_WRITE(sc, MV88E151X_PAGE, MV88E151X_PAGE_COPPER);
	}

	return (0);
}

static int
mv88e151x_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
mv88e151x_fiber_status(struct mii_softc *phy)
{
	struct mii_data *mii = phy->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, anlpar; //, gtcr, gtsr;
	uint32_t reg;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	/* Switch to the fiber PHY. */
	PHY_WRITE(phy, MV88E151X_PAGE, MV88E151X_PAGE_FIBER);

	reg = PHY_READ(phy, MV88E151X_FIBER_STATUS);
	bmsr = PHY_READ(phy, MII_BMSR) | PHY_READ(phy, MII_BMSR);
	if (reg & MV88E151X_STATUS_LINK)
		mii->mii_media_status |= IFM_ACTIVE;
	bmcr = PHY_READ(phy, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		PHY_WRITE(phy, MV88E151X_PAGE, MV88E151X_PAGE_COPPER);
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		/*
		 * NWay autonegotiation takes the highest-order common
		 * bit of the ANAR and ANLPAR (i.e. best media advertised
		 * both by us and our link partner).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			PHY_WRITE(phy, MV88E151X_PAGE, MV88E151X_PAGE_COPPER);
			return;
		}

		anlpar = PHY_READ(phy, MII_ANAR) & PHY_READ(phy, MII_ANLPAR);
		if (anlpar & ANLPAR_X_FD)
			mii->mii_media_active |= IFM_1000_SX | IFM_FDX;
		else if (anlpar & ANLPAR_X_HD)
			mii->mii_media_active |= IFM_1000_SX | IFM_HDX;
		else if (reg & MV88E151X_STATUS_LINK &&
		    reg & MV88E151X_STATUS_SYNC &&
		    (reg & MV88E151X_STATUS_ENERGY) == 0) {
			if ((reg & MV88E151X_STATUS_SPEED_MASK) ==
			    MV88E151X_STATUS_SPEED_1000)
				mii->mii_media_active |= IFM_1000_SX;
			else if ((reg & MV88E151X_STATUS_SPEED_MASK) ==
			    MV88E151X_STATUS_SPEED_100)
				mii->mii_media_active |= IFM_100_FX;
			else
				mii->mii_media_active |= IFM_NONE;
			if ((reg & MV88E151X_STATUS_SPEED_MASK) != 0 &&
			    (reg & MV88E151X_STATUS_FDX))
				mii->mii_media_active |= IFM_FDX;
		} else
			mii->mii_media_active |= IFM_NONE;

		if ((mii->mii_media_active & IFM_NONE) == 0)
			phy->mii_flags |= MIIF_IS_1000X;
		if ((mii->mii_media_active & IFM_FDX) != 0)
			mii->mii_media_active |= mii_phy_flowstatus(phy);
	} else
		mii->mii_media_active = ife->ifm_media;

	if ((mii->mii_media_status & IFM_ACTIVE) == 0)
		phy->mii_flags &= ~MIIF_IS_1000X;

	/* Switch back to copper PHY. */
	PHY_WRITE(phy, MV88E151X_PAGE, MV88E151X_PAGE_COPPER);
}

static void
mv88e151x_status(struct mii_softc *phy)
{
	struct mii_data *mii = phy->mii_pdata;

	mv88e151x_fiber_status(phy);
	if (mii->mii_media_status & IFM_ACTIVE)
		return;
	ukphy_status(phy);
}
