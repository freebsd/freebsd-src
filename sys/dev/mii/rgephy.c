/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/rgephy.c,v 1.15.2.5.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Driver for the RealTek 8169S/8110S/8211B/8211C internal 10/100/1000 PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/rgephyreg.h>

#include "miibus_if.h"

#include <machine/bus.h>
#include <pci/if_rlreg.h>

static int rgephy_probe(device_t);
static int rgephy_attach(device_t);

struct rgephy_softc {
	struct mii_softc mii_sc;
	int mii_model;
	int mii_revision;
};

static device_method_t rgephy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		rgephy_probe),
	DEVMETHOD(device_attach,	rgephy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t rgephy_devclass;

static driver_t rgephy_driver = {
	"rgephy",
	rgephy_methods,
	sizeof(struct rgephy_softc)
};

DRIVER_MODULE(rgephy, miibus, rgephy_driver, rgephy_devclass, 0, 0);

static int	rgephy_service(struct mii_softc *, struct mii_data *, int);
static void	rgephy_status(struct mii_softc *);
static int	rgephy_mii_phy_auto(struct mii_softc *);
static void	rgephy_reset(struct mii_softc *);
static void	rgephy_loop(struct mii_softc *);
static void	rgephy_load_dspcode(struct mii_softc *);

static const struct mii_phydesc rgephys[] = {
	MII_PHY_DESC(xxREALTEK, RTL8169S),
	MII_PHY_END
};

static int
rgephy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, rgephys, BUS_PROBE_DEFAULT));
}

static int
rgephy_attach(device_t dev)
{
	struct rgephy_softc *rsc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";

	rsc = device_get_softc(dev);
	sc = &rsc->mii_sc;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = rgephy_service;
	sc->mii_pdata = mii;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;

	mii->mii_instance++;

	rsc->mii_model = MII_MODEL(ma->mii_id2);
	rsc->mii_revision = MII_REV(ma->mii_id2);

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define PRINT(s)	printf("%s%s", sep, s); sep = ", "

#if 0
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);
#endif

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	sc->mii_capabilities &= ~BMSR_ANEG;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);

	device_printf(dev, " ");
	mii_phy_add_media(sc);
	/* RTL8169S do not report auto-sense; add manually. */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), MII_NMEDIA);
	sep = ", ";
	PRINT("auto");
	printf("\n");
#undef ADD
#undef PRINT

	rgephy_reset(sc);
	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
rgephy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct rgephy_softc *rsc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg, speed, gig, anar;

	rsc = (struct rgephy_softc *)sc;

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
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		rgephy_reset(sc);	/* XXX hardware bug work-around */

		anar = PHY_READ(sc, RGEPHY_MII_ANAR);
		anar &= ~(RGEPHY_ANAR_TX_FD | RGEPHY_ANAR_TX |
		    RGEPHY_ANAR_10_FD | RGEPHY_ANAR_10);

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, RGEPHY_MII_BMCR) & RGEPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void) rgephy_mii_phy_auto(sc);
			break;
		case IFM_1000_T:
			speed = RGEPHY_S1000;
			goto setit;
		case IFM_100_TX:
			speed = RGEPHY_S100;
			anar |= RGEPHY_ANAR_TX_FD | RGEPHY_ANAR_TX;
			goto setit;
		case IFM_10_T:
			speed = RGEPHY_S10;
			anar |= RGEPHY_ANAR_10_FD | RGEPHY_ANAR_10;
setit:
			rgephy_loop(sc);
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				speed |= RGEPHY_BMCR_FDX;
				gig = RGEPHY_1000CTL_AFD;
				anar &= ~(RGEPHY_ANAR_TX | RGEPHY_ANAR_10);
			} else {
				gig = RGEPHY_1000CTL_AHD;
				anar &=
				    ~(RGEPHY_ANAR_TX_FD | RGEPHY_ANAR_10_FD);
			}

			if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T) {
				PHY_WRITE(sc, RGEPHY_MII_1000CTL, 0);
				PHY_WRITE(sc, RGEPHY_MII_ANAR, anar);
				PHY_WRITE(sc, RGEPHY_MII_BMCR, speed |
				    RGEPHY_BMCR_AUTOEN | RGEPHY_BMCR_STARTNEG);
				break;
			}

			/*
			 * When setting the link manually, one side must
			 * be the master and the other the slave. However
			 * ifmedia doesn't give us a good way to specify
			 * this, so we fake it by using one of the LINK
			 * flags. If LINK0 is set, we program the PHY to
			 * be a master, otherwise it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, RGEPHY_MII_1000CTL,
				    gig|RGEPHY_1000CTL_MSE|RGEPHY_1000CTL_MSC);
			} else {
				PHY_WRITE(sc, RGEPHY_MII_1000CTL,
				    gig|RGEPHY_1000CTL_MSE);
			}
			PHY_WRITE(sc, RGEPHY_MII_BMCR, speed |
			    RGEPHY_BMCR_AUTOEN | RGEPHY_BMCR_STARTNEG);
			break;
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO|BMCR_PDOWN);
			break;
		case IFM_100_T4:
		default:
			return (EINVAL);
		}
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
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		if (rsc->mii_revision >= 2) {
			/* RTL8211B(L) */
			reg = PHY_READ(sc, RGEPHY_MII_SSR);
			if (reg & RGEPHY_SSR_LINK) {
				sc->mii_ticks = 0;
				break;
			}
		} else {
			reg = PHY_READ(sc, RL_GMEDIASTAT);
			if (reg & RL_GMEDIASTAT_LINK) {
				sc->mii_ticks = 0;
				break;
			}
		}

		/* Announce link loss right after it happens. */
		if (sc->mii_ticks++ == 0)
			break;

		/* Only retry autonegotiation every mii_anegticks seconds. */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		rgephy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	rgephy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke
	 * the DSP on the RealTek PHYs if the media changes.
	 *
	 */
	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		rgephy_load_dspcode(sc);
	}
	mii_phy_update(sc, cmd);
	return (0);
}

static void
rgephy_status(struct mii_softc *sc)
{
	struct rgephy_softc *rsc;
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr;
	uint16_t ssr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	rsc = (struct rgephy_softc *)sc;
	if (rsc->mii_revision >= 2) {
		ssr = PHY_READ(sc, RGEPHY_MII_SSR);
		if (ssr & RGEPHY_SSR_LINK)
			mii->mii_media_status |= IFM_ACTIVE;
	} else {
		bmsr = PHY_READ(sc, RL_GMEDIASTAT);
		if (bmsr & RL_GMEDIASTAT_LINK)
			mii->mii_media_status |= IFM_ACTIVE;
	}

	bmsr = PHY_READ(sc, RGEPHY_MII_BMSR);

	bmcr = PHY_READ(sc, RGEPHY_MII_BMCR);
	if (bmcr & RGEPHY_BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & RGEPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & RGEPHY_BMCR_AUTOEN) {
		if ((bmsr & RGEPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	if (rsc->mii_revision >= 2) {
		ssr = PHY_READ(sc, RGEPHY_MII_SSR);
		switch (ssr & RGEPHY_SSR_SPD_MASK) {
		case RGEPHY_SSR_S1000:
			mii->mii_media_active |= IFM_1000_T;
			break;
		case RGEPHY_SSR_S100:
			mii->mii_media_active |= IFM_100_TX;
			break;
		case RGEPHY_SSR_S10:
			mii->mii_media_active |= IFM_10_T;
			break;
		default:
			mii->mii_media_active |= IFM_NONE;
			break;
		}
		if (ssr & RGEPHY_SSR_FDX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	} else {
		bmsr = PHY_READ(sc, RL_GMEDIASTAT);
		if (bmsr & RL_GMEDIASTAT_1000MBPS)
			mii->mii_media_active |= IFM_1000_T;
		else if (bmsr & RL_GMEDIASTAT_100MBPS)
			mii->mii_media_active |= IFM_100_TX;
		else if (bmsr & RL_GMEDIASTAT_10MBPS)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_NONE;
		if (bmsr & RL_GMEDIASTAT_FDX)
			mii->mii_media_active |= IFM_FDX;
		else
			mii->mii_media_active |= IFM_HDX;
	}
}

static int
rgephy_mii_phy_auto(struct mii_softc *mii)
{

	rgephy_loop(mii);
	rgephy_reset(mii);

	PHY_WRITE(mii, RGEPHY_MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
	DELAY(1000);
	PHY_WRITE(mii, RGEPHY_MII_1000CTL,
            RGEPHY_1000CTL_AHD|RGEPHY_1000CTL_AFD);
	DELAY(1000);
	PHY_WRITE(mii, RGEPHY_MII_BMCR,
	    RGEPHY_BMCR_AUTOEN | RGEPHY_BMCR_STARTNEG);
	DELAY(100);

	return (EJUSTRETURN);
}

static void
rgephy_loop(struct mii_softc *sc)
{
	struct rgephy_softc *rsc;
	int i;

	rsc = (struct rgephy_softc *)sc;
	if (rsc->mii_revision < 2) {
		PHY_WRITE(sc, RGEPHY_MII_BMCR, RGEPHY_BMCR_PDOWN);
		DELAY(1000);
	}

	for (i = 0; i < 15000; i++) {
		if (!(PHY_READ(sc, RGEPHY_MII_BMSR) & RGEPHY_BMSR_LINK)) {
#if 0
			device_printf(sc->mii_dev, "looped %d\n", i);
#endif
			break;
		}
		DELAY(10);
	}
}

#define PHY_SETBIT(x, y, z) \
	PHY_WRITE(x, y, (PHY_READ(x, y) | (z)))
#define PHY_CLRBIT(x, y, z) \
	PHY_WRITE(x, y, (PHY_READ(x, y) & ~(z)))

/*
 * Initialize RealTek PHY per the datasheet. The DSP in the PHYs of
 * existing revisions of the 8169S/8110S chips need to be tuned in
 * order to reliably negotiate a 1000Mbps link. This is only needed
 * for rev 0 and rev 1 of the PHY. Later versions work without
 * any fixups.
 */
static void
rgephy_load_dspcode(struct mii_softc *sc)
{
	struct rgephy_softc *rsc;
	int val;

	rsc = (struct rgephy_softc *)sc;
	if (rsc->mii_revision >= 2)
		return;

	PHY_WRITE(sc, 31, 0x0001);
	PHY_WRITE(sc, 21, 0x1000);
	PHY_WRITE(sc, 24, 0x65C7);
	PHY_CLRBIT(sc, 4, 0x0800);
	val = PHY_READ(sc, 4) & 0xFFF;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0x00A1);
	PHY_WRITE(sc, 2, 0x0008);
	PHY_WRITE(sc, 1, 0x1020);
	PHY_WRITE(sc, 0, 0x1000);
	PHY_SETBIT(sc, 4, 0x0800);
	PHY_CLRBIT(sc, 4, 0x0800);
	val = (PHY_READ(sc, 4) & 0xFFF) | 0x7000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xFF41);
	PHY_WRITE(sc, 2, 0xDE60);
	PHY_WRITE(sc, 1, 0x0140);
	PHY_WRITE(sc, 0, 0x0077);
	val = (PHY_READ(sc, 4) & 0xFFF) | 0xA000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xDF01);
	PHY_WRITE(sc, 2, 0xDF20);
	PHY_WRITE(sc, 1, 0xFF95);
	PHY_WRITE(sc, 0, 0xFA00);
	val = (PHY_READ(sc, 4) & 0xFFF) | 0xB000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xFF41);
	PHY_WRITE(sc, 2, 0xDE20);
	PHY_WRITE(sc, 1, 0x0140);
	PHY_WRITE(sc, 0, 0x00BB);
	val = (PHY_READ(sc, 4) & 0xFFF) | 0xF000;
	PHY_WRITE(sc, 4, val);
	PHY_WRITE(sc, 3, 0xDF01);
	PHY_WRITE(sc, 2, 0xDF20);
	PHY_WRITE(sc, 1, 0xFF95);
	PHY_WRITE(sc, 0, 0xBF00);
	PHY_SETBIT(sc, 4, 0x0800);
	PHY_CLRBIT(sc, 4, 0x0800);
	PHY_WRITE(sc, 31, 0x0000);

	DELAY(40);
}

static void
rgephy_reset(struct mii_softc *sc)
{
	struct rgephy_softc *rsc;
	uint16_t ssr;

	rsc = (struct rgephy_softc *)sc;
	if (rsc->mii_revision == 3) {
		/* RTL8211C(L) */
		ssr = PHY_READ(sc, RGEPHY_MII_SSR);
		if ((ssr & RGEPHY_SSR_ALDPS) != 0) {
			ssr &= ~RGEPHY_SSR_ALDPS;
			PHY_WRITE(sc, RGEPHY_MII_SSR, ssr);
		}
	}

	mii_phy_reset(sc);
	DELAY(1000);
	rgephy_load_dspcode(sc);
}
