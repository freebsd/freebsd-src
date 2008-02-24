/*-
 * Principal Author: Parag Patel
 * Copyright (c) 2001
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * Additonal Copyright (c) 2001 by Traakan Software under same licence.
 * Secondary Author: Matthew Jacob
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/e1000phy.c,v 1.18.2.1 2007/11/01 04:26:33 yongari Exp $");

/*
 * driver for the Marvell 88E1000 series external 1000/100/10-BT PHY.
 */

/*
 * Support added for the Marvell 88E1011 (Alaska) 1000/100/10baseTX and
 * 1000baseSX PHY.
 * Nathan Binkert <nate@openbsd.org>
 * Jung-uk Kim <jkim@niksun.com>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>


#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/e1000phyreg.h>

#include "miibus_if.h"

static int	e1000phy_probe(device_t);
static int	e1000phy_attach(device_t);

struct e1000phy_softc {
	struct mii_softc mii_sc;
	int mii_model;
};

static device_method_t e1000phy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		e1000phy_probe),
	DEVMETHOD(device_attach,	e1000phy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t e1000phy_devclass;
static driver_t e1000phy_driver = {
	"e1000phy",
	e1000phy_methods,
	sizeof(struct e1000phy_softc)
};

DRIVER_MODULE(e1000phy, miibus, e1000phy_driver, e1000phy_devclass, 0, 0);

static int	e1000phy_service(struct mii_softc *, struct mii_data *, int);
static void	e1000phy_status(struct mii_softc *);
static void	e1000phy_reset(struct mii_softc *);
static int	e1000phy_mii_phy_auto(struct e1000phy_softc *);

static const struct mii_phydesc e1000phys[] = {
	MII_PHY_DESC(MARVELL, E1000),
	MII_PHY_DESC(MARVELL, E1011),
	MII_PHY_DESC(MARVELL, E1000_3),
	MII_PHY_DESC(MARVELL, E1000S),
	MII_PHY_DESC(MARVELL, E1000_5),
	MII_PHY_DESC(MARVELL, E1000_6),
	MII_PHY_DESC(MARVELL, E3082),
	MII_PHY_DESC(MARVELL, E1112),
	MII_PHY_DESC(MARVELL, E1149),
	MII_PHY_DESC(MARVELL, E1111),
	MII_PHY_DESC(MARVELL, E1116),
	MII_PHY_DESC(MARVELL, E1118),
	MII_PHY_DESC(xxMARVELL, E1000),
	MII_PHY_DESC(xxMARVELL, E1011),
	MII_PHY_DESC(xxMARVELL, E1000_3),
	MII_PHY_DESC(xxMARVELL, E1000_5),
	MII_PHY_DESC(xxMARVELL, E1111),
	MII_PHY_END
};

static int
e1000phy_probe(device_t	dev)
{

	return (mii_phy_dev_probe(dev, e1000phys, BUS_PROBE_DEFAULT));
}

static int
e1000phy_attach(device_t dev)
{
	struct e1000phy_softc *esc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	int fast_ether;

	esc = device_get_softc(dev);
	sc = &esc->mii_sc;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = e1000phy_service;
	sc->mii_pdata = mii;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;
	mii->mii_instance++;

	fast_ether = 0;
	esc->mii_model = MII_MODEL(ma->mii_id2);
	switch (esc->mii_model) {
	case MII_MODEL_MARVELL_E1011:
	case MII_MODEL_MARVELL_E1112:
		if (PHY_READ(sc, E1000_ESSR) & E1000_ESSR_FIBER_LINK)
			sc->mii_flags |= MIIF_HAVEFIBER;
		break;
	case MII_MODEL_MARVELL_E3082:
		/* 88E3082 10/100 Fast Ethernet PHY. */
		sc->mii_anegticks = MII_ANEGTICKS;
		fast_ether = 1;
		break;
	}

	e1000phy_reset(sc);

	device_printf(dev, " ");

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    E1000_CR_ISOLATE);
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    E1000_CR_SPEED_10);
		printf("10baseT, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_10 | E1000_CR_FULL_DUPLEX);
		printf("10baseT-FDX, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    E1000_CR_SPEED_100);
		printf("100baseTX, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_100 | E1000_CR_FULL_DUPLEX);
		printf("100baseTX-FDX, ");
		if (fast_ether == 0) {
			/*
			 * 1000BT-simplex not supported; driver must ignore
			 * this entry, but it must be present in order to
			 * manually set full-duplex.
			 */
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0,
			    sc->mii_inst), E1000_CR_SPEED_1000);
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX,
			    sc->mii_inst),
			    E1000_CR_SPEED_1000 | E1000_CR_FULL_DUPLEX);
			printf("1000baseTX-FDX, ");
		}
	} else {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX, sc->mii_inst),
		    E1000_CR_SPEED_1000 | E1000_CR_FULL_DUPLEX);
		printf("1000baseSX-FDX, ");
	}
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	printf("auto\n");
#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static void
e1000phy_reset(struct mii_softc *sc)
{
	struct e1000phy_softc *esc;
	uint16_t reg;

	esc = (struct e1000phy_softc *)sc;
	reg = PHY_READ(sc, E1000_SCR);
	if ((sc->mii_flags & MIIF_HAVEFIBER) != 0) {
		reg &= ~E1000_SCR_AUTO_X_MODE;
		PHY_WRITE(sc, E1000_SCR, reg);
		if (esc->mii_model == MII_MODEL_MARVELL_E1112) {
			/* Select 1000BASE-X only mode. */
			PHY_WRITE(sc, E1000_EADR, 2);
			reg = PHY_READ(sc, E1000_SCR);
			reg &= ~E1000_SCR_MODE_MASK;
			reg |= E1000_SCR_MODE_1000BX;
			PHY_WRITE(sc, E1000_SCR, reg);
			PHY_WRITE(sc, E1000_EADR, 1);
		}
	} else {
		switch (esc->mii_model) {
		case MII_MODEL_MARVELL_E1111:
		case MII_MODEL_MARVELL_E1112:
		case MII_MODEL_MARVELL_E1116:
		case MII_MODEL_MARVELL_E1118:
		case MII_MODEL_MARVELL_E1149:
			/* Disable energy detect mode. */
			reg &= ~E1000_SCR_EN_DETECT_MASK;
			reg |= E1000_SCR_AUTO_X_MODE;
			if (esc->mii_model == MII_MODEL_MARVELL_E1116)
				reg &= ~E1000_SCR_POWER_DOWN;
			break;
		case MII_MODEL_MARVELL_E3082:
			reg |= (E1000_SCR_AUTO_X_MODE >> 1);
			break;
		default:
			reg &= ~E1000_SCR_AUTO_X_MODE;
			break;
		}
		/* Enable CRS on TX. */
		reg |= E1000_SCR_ASSERT_CRS_ON_TX;
		/* Auto correction for reversed cable polarity. */
		reg &= ~E1000_SCR_POLARITY_REVERSAL;
		PHY_WRITE(sc, E1000_SCR, reg);

		if (esc->mii_model == MII_MODEL_MARVELL_E1116) {
			PHY_WRITE(sc, E1000_EADR, 2);
			reg = PHY_READ(sc, E1000_SCR);
			reg |= E1000_SCR_RGMII_POWER_UP;
			PHY_WRITE(sc, E1000_SCR, reg);
			PHY_WRITE(sc, E1000_EADR, 0);
		}
	}

	switch (MII_MODEL(esc->mii_model)) {
	case MII_MODEL_MARVELL_E3082:
	case MII_MODEL_MARVELL_E1112:
	case MII_MODEL_MARVELL_E1116:
	case MII_MODEL_MARVELL_E1118:
	case MII_MODEL_MARVELL_E1149:
		break;
	default:
		/* Force TX_CLK to 25MHz clock. */
		reg = PHY_READ(sc, E1000_ESCR);
		reg |= E1000_ESCR_TX_CLK_25;
		PHY_WRITE(sc, E1000_ESCR, reg);
		break;
	}

	/* Reset the PHY so all changes take effect. */
	reg = PHY_READ(sc, E1000_CR);
	reg |= E1000_CR_RESET;
	PHY_WRITE(sc, E1000_CR, reg);
}

static int
e1000phy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	struct e1000phy_softc *esc = (struct e1000phy_softc *)sc;
	uint16_t speed, gig;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			reg = PHY_READ(sc, E1000_CR);
			PHY_WRITE(sc, E1000_CR, reg | E1000_CR_ISOLATE);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
			e1000phy_mii_phy_auto(esc);
			break;
		}

		speed = 0;
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_1000_T:
			if (esc->mii_model == MII_MODEL_MARVELL_E3082)
				return (EINVAL);
			speed = E1000_CR_SPEED_1000;
			break;
		case IFM_1000_SX:
			if (esc->mii_model == MII_MODEL_MARVELL_E3082)
				return (EINVAL);
			speed = E1000_CR_SPEED_1000;
			break;
		case IFM_100_TX:
			speed = E1000_CR_SPEED_100;
			break;
		case IFM_10_T:
			speed = E1000_CR_SPEED_10;
			break;
		case IFM_NONE:
			reg = PHY_READ(sc, E1000_CR);
			PHY_WRITE(sc, E1000_CR,
			    reg | E1000_CR_ISOLATE | E1000_CR_POWER_DOWN);
			goto done;
		default:
			return (EINVAL);
		}

		if (((ife->ifm_media & IFM_GMASK) & IFM_FDX) != 0) {
			speed |= E1000_CR_FULL_DUPLEX;
			gig = E1000_1GCR_1000T_FD;
		} else
			gig = E1000_1GCR_1000T;

		reg = PHY_READ(sc, E1000_CR);
		reg &= ~E1000_CR_AUTO_NEG_ENABLE;
		PHY_WRITE(sc, E1000_CR, reg | E1000_CR_RESET);

		/*
		 * When setting the link manually, one side must
		 * be the master and the other the slave. However
		 * ifmedia doesn't give us a good way to specify
		 * this, so we fake it by using one of the LINK
		 * flags. If LINK0 is set, we program the PHY to
		 * be a master, otherwise it's a slave.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) == IFM_1000_T ||
		    (IFM_SUBTYPE(ife->ifm_media) == IFM_1000_SX)) {
			if ((mii->mii_ifp->if_flags & IFF_LINK0))
				PHY_WRITE(sc, E1000_1GCR, gig |
				    E1000_1GCR_MS_ENABLE | E1000_1GCR_MS_VALUE);
			else
				PHY_WRITE(sc, E1000_1GCR, gig |
				    E1000_1GCR_MS_ENABLE);
		} else {
			if (esc->mii_model != MII_MODEL_MARVELL_E3082)
				PHY_WRITE(sc, E1000_1GCR, 0);
		}
		PHY_WRITE(sc, E1000_AR, E1000_AR_SELECTOR_FIELD);
		PHY_WRITE(sc, E1000_CR, speed | E1000_CR_RESET);
done:
		break;
	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			break;

		/*
		 * check for link.
		 * Read the status register twice; BMSR_LINK is latch-low.
		 */
		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK) {
			sc->mii_ticks = 0;
			break;
		}

		/* Announce link loss right after it happens. */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		e1000phy_reset(sc);
		e1000phy_mii_phy_auto(esc);
		break;
	}

	/* Update the media status. */
	e1000phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
e1000phy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, esr, gsr, ssr, isr, ar, lpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, E1000_SR) | PHY_READ(sc, E1000_SR);
	esr = PHY_READ(sc, E1000_ESR);
	bmcr = PHY_READ(sc, E1000_CR);
	ssr = PHY_READ(sc, E1000_SSR);
	isr = PHY_READ(sc, E1000_ISR);
	ar = PHY_READ(sc, E1000_AR);
	lpar = PHY_READ(sc, E1000_LPAR);

	if (bmsr & E1000_SR_LINK_STATUS)
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & E1000_CR_LOOPBACK)
		mii->mii_media_active |= IFM_LOOP;

	if ((((bmcr & E1000_CR_AUTO_NEG_ENABLE) != 0) &&
	    ((bmsr & E1000_SR_AUTO_NEG_COMPLETE) == 0)) ||
	    ((ssr & E1000_SSR_LINK) == 0) ||
	    ((ssr & E1000_SSR_SPD_DPLX_RESOLVED) == 0)) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		if (ssr & E1000_SSR_1000MBS)
			mii->mii_media_active |= IFM_1000_T;
		else if (ssr & E1000_SSR_100MBS)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
	} else {
		if (ssr & E1000_SSR_1000MBS)
			mii->mii_media_active |= IFM_1000_SX;
	}

	if (ssr & E1000_SSR_DUPLEX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		/* FLAG0==rx-flow-control FLAG1==tx-flow-control */
		if ((ar & E1000_AR_PAUSE) && (lpar & E1000_LPAR_PAUSE)) {
			mii->mii_media_active |= IFM_FLAG0 | IFM_FLAG1;
		} else if (!(ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
		    (lpar & E1000_LPAR_PAUSE) && (lpar & E1000_LPAR_ASM_DIR)) {
			mii->mii_media_active |= IFM_FLAG1;
		} else if ((ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
		    !(lpar & E1000_LPAR_PAUSE) && (lpar & E1000_LPAR_ASM_DIR)) {
			mii->mii_media_active |= IFM_FLAG0;
		}
	}

	/* FLAG2 : local PHY resolved to MASTER */
	if ((IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) ||
	    (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX)) {
		PHY_READ(sc, E1000_1GSR);
		gsr = PHY_READ(sc, E1000_1GSR);
		if ((gsr & E1000_1GSR_MS_CONFIG_RES) != 0)
			mii->mii_media_active |= IFM_FLAG2;
	}
}

static int
e1000phy_mii_phy_auto(struct e1000phy_softc *esc)
{
	struct mii_softc *sc;

	sc = &esc->mii_sc;
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0)
		PHY_WRITE(sc, E1000_AR, E1000_AR_10T | E1000_AR_10T_FD |
		    E1000_AR_100TX | E1000_AR_100TX_FD |
		    E1000_AR_PAUSE | E1000_AR_ASM_DIR);
	else
		PHY_WRITE(sc, E1000_AR, E1000_FA_1000X_FD | E1000_FA_1000X |
		    E1000_FA_SYM_PAUSE | E1000_FA_ASYM_PAUSE);
	if (esc->mii_model != MII_MODEL_MARVELL_E3082)
		PHY_WRITE(sc, E1000_1GCR,
		    E1000_1GCR_1000T_FD | E1000_1GCR_1000T);
	PHY_WRITE(sc, E1000_CR,
	    E1000_CR_AUTO_NEG_ENABLE | E1000_CR_RESTART_AUTO_NEG);

	return (EJUSTRETURN);
}
