/*	$NetBSD: mii_physubr.c,v 1.5 1999/08/03 19:41:49 drochner Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2000, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
__FBSDID("$FreeBSD: src/sys/dev/mii/mii_physubr.c,v 1.29.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Subroutines common to all PHYs.
 */

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

#include "miibus_if.h"

/*
 * Media to register setting conversion table.  Order matters.
 */
const struct mii_media mii_media_table[MII_NMEDIA] = {
	/* None */
	{ BMCR_ISO,		ANAR_CSMA,
	  0, },

	/* 10baseT */
	{ BMCR_S10,		ANAR_CSMA|ANAR_10,
	  0, },

	/* 10baseT-FDX */
	{ BMCR_S10|BMCR_FDX,	ANAR_CSMA|ANAR_10_FD,
	  0, },

	/* 100baseT4 */
	{ BMCR_S100,		ANAR_CSMA|ANAR_T4,
	  0, },

	/* 100baseTX */
	{ BMCR_S100,		ANAR_CSMA|ANAR_TX,
	  0, },

	/* 100baseTX-FDX */
	{ BMCR_S100|BMCR_FDX,	ANAR_CSMA|ANAR_TX_FD,
	  0, },

	/* 1000baseX */
	{ BMCR_S1000,		ANAR_CSMA,
	  0, },

	/* 1000baseX-FDX */
	{ BMCR_S1000|BMCR_FDX,	ANAR_CSMA,
	  0, },

	/* 1000baseT */
	{ BMCR_S1000,		ANAR_CSMA,
	  GTCR_ADV_1000THDX },

	/* 1000baseT-FDX */
	{ BMCR_S1000,		ANAR_CSMA,
	  GTCR_ADV_1000TFDX },
};

void
mii_phy_setmedia(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, anar, gtcr;

	if (IFM_SUBTYPE(ife->ifm_media) == IFM_AUTO) {
		if ((PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN) == 0 ||
		    (sc->mii_flags & MIIF_FORCEANEG))
			(void) mii_phy_auto(sc);
		return;
	}

	/*
	 * Table index is stored in the media entry.
	 */

	KASSERT(ife->ifm_data >=0 && ife->ifm_data < MII_NMEDIA,
	    ("invalid ife->ifm_data (0x%x) in mii_phy_setmedia",
	    ife->ifm_data));

	anar = mii_media_table[ife->ifm_data].mm_anar;
	bmcr = mii_media_table[ife->ifm_data].mm_bmcr;
	gtcr = mii_media_table[ife->ifm_data].mm_gtcr;

	if (mii->mii_media.ifm_media & IFM_ETH_MASTER) {
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_1000_T:
			gtcr |= GTCR_MAN_MS|GTCR_ADV_MS;
			break;

		default:
			panic("mii_phy_setmedia: MASTER on wrong media");
		}
	}

	if (ife->ifm_media & IFM_LOOP)
		bmcr |= BMCR_LOOP;

	PHY_WRITE(sc, MII_ANAR, anar);
	PHY_WRITE(sc, MII_BMCR, bmcr);
	if (sc->mii_flags & MIIF_HAVE_GTCR)
		PHY_WRITE(sc, MII_100T2CR, gtcr);
}

int
mii_phy_auto(struct mii_softc *sc)
{

	/*
	 * Check for 1000BASE-X.  Autonegotiation is a bit
	 * different on such devices.
	 */
	if (sc->mii_flags & MIIF_IS_1000X) {
		uint16_t anar = 0;

		if (sc->mii_extcapabilities & EXTSR_1000XFDX)
			anar |= ANAR_X_FD;
		if (sc->mii_extcapabilities & EXTSR_1000XHDX)
			anar |= ANAR_X_HD;

		if (sc->mii_flags & MIIF_DOPAUSE) {
			/* XXX Asymmetric vs. symmetric? */
			anar |= ANLPAR_X_PAUSE_TOWARDS;
		}

		PHY_WRITE(sc, MII_ANAR, anar);
	} else {
		uint16_t anar;

		anar = BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) |
		    ANAR_CSMA;
		if (sc->mii_flags & MIIF_DOPAUSE)
			anar |= ANAR_FC;
		PHY_WRITE(sc, MII_ANAR, anar);
		if (sc->mii_flags & MIIF_HAVE_GTCR) {
			uint16_t gtcr = 0;

			if (sc->mii_extcapabilities & EXTSR_1000TFDX)
				gtcr |= GTCR_ADV_1000TFDX;
			if (sc->mii_extcapabilities & EXTSR_1000THDX)
				gtcr |= GTCR_ADV_1000THDX;

			PHY_WRITE(sc, MII_100T2CR, gtcr);
		}
	}
	PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
	return (EJUSTRETURN);
}

int
mii_phy_tick(struct mii_softc *sc)
{
	struct ifmedia_entry *ife = sc->mii_pdata->mii_media.ifm_cur;
	struct ifnet *ifp = sc->mii_pdata->mii_ifp;
	int reg;

	/* Just bail now if the interface is down. */
	if ((ifp->if_flags & IFF_UP) == 0)
		return (EJUSTRETURN);

	/*
	 * If we're not doing autonegotiation, we don't need to do
	 * any extra work here.  However, we need to check the link
	 * status so we can generate an announcement if the status
	 * changes.
	 */
	if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
		sc->mii_ticks = 0;	/* reset autonegotiation timer. */
		return (0);
	}

	/* Read the status register twice; BMSR_LINK is latch-low. */
	reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (reg & BMSR_LINK) {
		sc->mii_ticks = 0;	/* reset autonegotiation timer. */
		/* See above. */
		return (0);
	}

	/* Announce link loss right after it happens */
	if (sc->mii_ticks++ == 0)
		return (0);

	/* XXX: use default value if phy driver did not set mii_anegticks */
	if (sc->mii_anegticks == 0)
		sc->mii_anegticks = MII_ANEGTICKS_GIGE;

	/* Only retry autonegotiation every mii_anegticks ticks. */
	if (sc->mii_ticks <= sc->mii_anegticks)
		return (EJUSTRETURN);

	sc->mii_ticks = 0;
	mii_phy_reset(sc);
	mii_phy_auto(sc);
	return (0);
}

void
mii_phy_reset(struct mii_softc *sc)
{
	struct ifmedia_entry *ife = sc->mii_pdata->mii_media.ifm_cur;
	int reg, i;

	if (sc->mii_flags & MIIF_NOISOLATE)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/* Wait 100ms for it to complete. */
	for (i = 0; i < 100; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if ((reg & BMCR_RESET) == 0)
			break;
		DELAY(1000);
	}

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0) {
		if ((ife == NULL && sc->mii_inst != 0) ||
		    (ife != NULL && IFM_INST(ife->ifm_media) != sc->mii_inst))
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
	}
}

void
mii_phy_down(struct mii_softc *sc)
{

}

void
mii_phy_update(struct mii_softc *sc, int cmd)
{
	struct mii_data *mii = sc->mii_pdata;

	if (sc->mii_media_active != mii->mii_media_active ||
	    cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_media_active = mii->mii_media_active;
	}
	if (sc->mii_media_status != mii->mii_media_status) {
		MIIBUS_LINKCHG(sc->mii_dev);
		sc->mii_media_status = mii->mii_media_status;
	}
}

/*
 * Given an ifmedia word, return the corresponding ANAR value.
 */
int
mii_anar(int media)
{
	int rv;

	switch (media & (IFM_TMASK|IFM_NMASK|IFM_FDX)) {
	case IFM_ETHER|IFM_10_T:
		rv = ANAR_10|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_10_T|IFM_FDX:
		rv = ANAR_10_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX:
		rv = ANAR_TX|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_TX|IFM_FDX:
		rv = ANAR_TX_FD|ANAR_CSMA;
		break;
	case IFM_ETHER|IFM_100_T4:
		rv = ANAR_T4|ANAR_CSMA;
		break;
	default:
		rv = 0;
		break;
	}

	return (rv);
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_add_media(struct mii_softc *sc)
{
	const char *sep = "";
	struct mii_data *mii;

	mii = device_get_softc(sc->mii_dev);
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0) {
		printf("no media present");
		return;
	}

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	if (sc->mii_capabilities & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst), 0);
		PRINT("10baseT");
	}
	if (sc->mii_capabilities & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    BMCR_FDX);
		PRINT("10baseT-FDX");
	}
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    BMCR_S100);
		PRINT("100baseTX");
	}
	if (sc->mii_capabilities & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    BMCR_S100|BMCR_FDX);
		PRINT("100baseTX-FDX");
	}
	if (sc->mii_capabilities & BMSR_100T4) {
		/*
		 * XXX How do you enable 100baseT4?  I assume we set
		 * XXX BMCR_S100 and then assume the PHYs will take
		 * XXX watever action is necessary to switch themselves
		 * XXX into T4 mode.
		 */
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    BMCR_S100);
		PRINT("100baseT4");
	}
	if (sc->mii_capabilities & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    BMCR_AUTOEN);
		PRINT("auto");
	}



#undef ADD
#undef PRINT
}

/*
 * Initialize generic PHY media based on BMSR, called when a PHY is
 * attached.  We expect to be set up to print a comma-separated list
 * of media names.  Does not print a newline.
 */
void
mii_phy_add_media(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	const char *sep = "";

	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0 &&
	    (sc->mii_extcapabilities & EXTSR_MEDIAMASK) == 0) {
		printf("no media present");
		return;
	}

	/* Set aneg timer for 10/100 media. Gigabit media handled below. */
	sc->mii_anegticks = MII_ANEGTICKS;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0)
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
		    MII_MEDIA_NONE);

	/*
	 * There are different interpretations for the bits in
	 * HomePNA PHYs.  And there is really only one media type
	 * that is supported.
	 */
	if (sc->mii_flags & MIIF_IS_HPNA) {
		if (sc->mii_capabilities & BMSR_10THDX) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_HPNA_1, 0,
					 sc->mii_inst),
			    MII_MEDIA_10_T);
			PRINT("HomePNA1");
		}
		return;
	}

	if (sc->mii_capabilities & BMSR_10THDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
		    MII_MEDIA_10_T);
		PRINT("10baseT");
	}
	if (sc->mii_capabilities & BMSR_10TFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_10_T_FDX);
		PRINT("10baseT-FDX");
	}
	if (sc->mii_capabilities & BMSR_100TXHDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
		    MII_MEDIA_100_TX);
		PRINT("100baseTX");
	}
	if (sc->mii_capabilities & BMSR_100TXFDX) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
		    MII_MEDIA_100_TX_FDX);
		PRINT("100baseTX-FDX");
	}
	if (sc->mii_capabilities & BMSR_100T4) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_T4, 0, sc->mii_inst),
		    MII_MEDIA_100_T4);
		PRINT("100baseT4");
	}

	if (sc->mii_extcapabilities & EXTSR_MEDIAMASK) {
		/*
		 * XXX Right now only handle 1000SX and 1000TX.  Need
		 * XXX to handle 1000LX and 1000CX some how.
		 */
		if (sc->mii_extcapabilities & EXTSR_1000XHDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_IS_1000X;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, 0,
			    sc->mii_inst), MII_MEDIA_1000_X);
			PRINT("1000baseSX");
		}
		if (sc->mii_extcapabilities & EXTSR_1000XFDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_IS_1000X;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_X_FDX);
			PRINT("1000baseSX-FDX");
		}

		/*
		 * 1000baseT media needs to be able to manipulate
		 * master/slave mode.  We set IFM_ETH_MASTER in
		 * the "don't care mask" and filter it out when
		 * the media is set.
		 *
		 * All 1000baseT PHYs have a 1000baseT control register.
		 */
		if (sc->mii_extcapabilities & EXTSR_1000THDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_HAVE_GTCR;
			mii->mii_media.ifm_mask |= IFM_ETH_MASTER;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0,
			    sc->mii_inst), MII_MEDIA_1000_T);
			PRINT("1000baseT");
		}
		if (sc->mii_extcapabilities & EXTSR_1000TFDX) {
			sc->mii_anegticks = MII_ANEGTICKS_GIGE;
			sc->mii_flags |= MIIF_HAVE_GTCR;
			mii->mii_media.ifm_mask |= IFM_ETH_MASTER;
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX,
			    sc->mii_inst), MII_MEDIA_1000_T_FDX);
			PRINT("1000baseT-FDX");
		}
	}

	if (sc->mii_capabilities & BMSR_ANEG) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst),
		    MII_NMEDIA);	/* intentionally invalid index */
		PRINT("auto");
	}
#undef ADD
#undef PRINT
}

int
mii_phy_detach(device_t dev)
{
	struct mii_softc *sc;

	sc = device_get_softc(dev);
	mii_phy_down(sc);
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}

const struct mii_phydesc *
mii_phy_match_gen(const struct mii_attach_args *ma,
  const struct mii_phydesc *mpd, size_t len)
{

	for (; mpd->mpd_name != NULL;
	     mpd = (const struct mii_phydesc *) ((const char *) mpd + len)) {
		if (MII_OUI(ma->mii_id1, ma->mii_id2) == mpd->mpd_oui &&
		    MII_MODEL(ma->mii_id2) == mpd->mpd_model)
			return (mpd);
	}
	return (NULL);
}

const struct mii_phydesc *
mii_phy_match(const struct mii_attach_args *ma, const struct mii_phydesc *mpd)
{

	return (mii_phy_match_gen(ma, mpd, sizeof(struct mii_phydesc)));
}

int
mii_phy_dev_probe(device_t dev, const struct mii_phydesc *mpd, int mrv)
{

	mpd = mii_phy_match(device_get_ivars(dev), mpd);
	if (mpd != NULL) {
		device_set_desc(dev, mpd->mpd_name);
		return (mrv);
	}

	return (ENXIO);
}
