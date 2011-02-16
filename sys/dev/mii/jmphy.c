/*-
 * Copyright (c) 2008, Pyun YongHyeon <yongari@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/jmphy.c,v 1.1.6.5.2.1 2010/12/21 17:09:25 kensmith Exp $");

/*
 * Driver for the JMicron JMP211 10/100/1000, JMP202 10/100 PHY.
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

#include <dev/mii/jmphyreg.h>

#include "miibus_if.h"

static int	jmphy_probe(device_t);
static int	jmphy_attach(device_t);
static void	jmphy_reset(struct mii_softc *);
static uint16_t	jmphy_anar(struct ifmedia_entry *);
static int	jmphy_setmedia(struct mii_softc *, struct ifmedia_entry *);

struct jmphy_softc {
	struct mii_softc mii_sc;
	int mii_oui;
	int mii_model;
	int mii_rev;
};

static device_method_t jmphy_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		jmphy_probe),
	DEVMETHOD(device_attach,	jmphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ NULL, NULL }
};

static devclass_t jmphy_devclass;
static driver_t jmphy_driver = {
	"jmphy",
	jmphy_methods,
	sizeof(struct jmphy_softc)
};

DRIVER_MODULE(jmphy, miibus, jmphy_driver, jmphy_devclass, 0, 0);

static int	jmphy_service(struct mii_softc *, struct mii_data *, int);
static void	jmphy_status(struct mii_softc *);

static const struct mii_phydesc jmphys[] = {
	MII_PHY_DESC(JMICRON, JMP202),
	MII_PHY_DESC(JMICRON, JMP211),
	MII_PHY_END
};

static int
jmphy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, jmphys, BUS_PROBE_DEFAULT));
}

static int
jmphy_attach(device_t dev)
{
	struct jmphy_softc *jsc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	jsc = device_get_softc(dev);
	sc = &jsc->mii_sc;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = ma->mii_data;
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_flags = miibus_get_flags(dev);
	sc->mii_inst = mii->mii_instance++;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = jmphy_service;
	sc->mii_pdata = mii;

	jsc->mii_oui = MII_OUI(ma->mii_id1, ma->mii_id2);
	jsc->mii_model = MII_MODEL(ma->mii_id2);
	jsc->mii_rev = MII_REV(ma->mii_id2);
	if (bootverbose)
		device_printf(dev, "OUI 0x%06x, model 0x%04x, rev. %d\n",
		    jsc->mii_oui, jsc->mii_model, jsc->mii_rev);

	jmphy_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
jmphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		if (jmphy_setmedia(sc, ife) != EJUSTRETURN)
			return (EINVAL);
		break;

	case MII_TICK:
		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		/* Check for link. */
		if ((PHY_READ(sc, JMPHY_SSR) & JMPHY_SSR_LINK_UP) != 0) {
			sc->mii_ticks = 0;
			break;
		}

		/* Announce link loss right after it happens. */
		if (sc->mii_ticks++ == 0)
			break;
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		(void)jmphy_setmedia(sc, ife);
		break;
	}

	/* Update the media status. */
	jmphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
jmphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmcr, ssr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	ssr = PHY_READ(sc, JMPHY_SSR);
	if ((ssr & JMPHY_SSR_LINK_UP) != 0)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if ((bmcr & BMCR_ISO) != 0) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if ((bmcr & BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((ssr & JMPHY_SSR_SPD_DPLX_RESOLVED) == 0) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	switch ((ssr & JMPHY_SSR_SPEED_MASK)) {
	case JMPHY_SSR_SPEED_1000:
		mii->mii_media_active |= IFM_1000_T;
		/*
		 * jmphy(4) got a valid link so reset mii_ticks.
		 * Resetting mii_ticks is needed in order to
		 * detect link loss after auto-negotiation.
		 */
		sc->mii_ticks = 0;
		break;
	case JMPHY_SSR_SPEED_100:
		mii->mii_media_active |= IFM_100_TX;
		sc->mii_ticks = 0;
		break;
	case JMPHY_SSR_SPEED_10:
		mii->mii_media_active |= IFM_10_T;
		sc->mii_ticks = 0;
		break;
	default:
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if ((ssr & JMPHY_SSR_DUPLEX) != 0)
		mii->mii_media_active |= IFM_FDX | mii_phy_flowstatus(sc);
	else
		mii->mii_media_active |= IFM_HDX;

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) {
		if ((PHY_READ(sc, MII_100T2SR) & GTSR_MS_RES) != 0)
			mii->mii_media_active |= IFM_ETH_MASTER;
	}
}

static void
jmphy_reset(struct mii_softc *sc)
{
	struct jmphy_softc *jsc;
	int i;

	jsc = (struct jmphy_softc *)sc;

	/* Disable sleep mode. */
	PHY_WRITE(sc, JMPHY_TMCTL,
	    PHY_READ(sc, JMPHY_TMCTL) & ~JMPHY_TMCTL_SLEEP_ENB);
	PHY_WRITE(sc, MII_BMCR, BMCR_RESET | BMCR_AUTOEN);

	for (i = 0; i < 1000; i++) {
		DELAY(1);
		if ((PHY_READ(sc, MII_BMCR) & BMCR_RESET) == 0)
			break;
	}
}

static uint16_t
jmphy_anar(struct ifmedia_entry *ife)
{
	uint16_t anar;

	anar = 0;
	switch (IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_AUTO:
		anar |= ANAR_TX_FD | ANAR_TX | ANAR_10_FD | ANAR_10;
		break;
	case IFM_1000_T:
		break;
	case IFM_100_TX:
		anar |= ANAR_TX | ANAR_TX_FD;
		break;
	case IFM_10_T:
		anar |= ANAR_10 | ANAR_10_FD;
		break;
	default:
		break;
	}

	return (anar);
}

static int
jmphy_setmedia(struct mii_softc *sc, struct ifmedia_entry *ife)
{
	uint16_t anar, bmcr, gig;

	gig = 0;
	bmcr = PHY_READ(sc, MII_BMCR);
	switch (IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_AUTO:
		gig |= GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX;
		break;
	case IFM_1000_T:
		gig |= GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX;
		break;
	case IFM_100_TX:
	case IFM_10_T:
		break;
	case IFM_NONE:
		PHY_WRITE(sc, MII_BMCR, bmcr | BMCR_ISO | BMCR_PDOWN);
		return (EJUSTRETURN);
	default:
		return (EINVAL);
	}

	if ((ife->ifm_media & IFM_LOOP) != 0)
		bmcr |= BMCR_LOOP;

	anar = jmphy_anar(ife);
	if (((IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO ||
	    (ife->ifm_media & IFM_FDX) != 0) &&
	    (ife->ifm_media & IFM_FLOW) != 0) ||
	    (sc->mii_flags & MIIF_FORCEPAUSE) != 0)
		anar |= ANAR_PAUSE_TOWARDS;

	if ((sc->mii_flags & MIIF_HAVE_GTCR) != 0) {
		if (IFM_SUBTYPE(ife->ifm_media) == IFM_1000_T) {
			gig |= GTCR_MAN_MS;
			if ((ife->ifm_media & IFM_ETH_MASTER) != 0)
				gig |= GTCR_ADV_MS;
		}
		PHY_WRITE(sc, MII_100T2CR, gig);
	}
	PHY_WRITE(sc, MII_ANAR, anar | ANAR_CSMA);
	PHY_WRITE(sc, MII_BMCR, bmcr | BMCR_AUTOEN | BMCR_STARTNEG);

	return (EJUSTRETURN);
}
