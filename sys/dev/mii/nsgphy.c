/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 2001
 *	Bill Paul <wpaul@bsdi.com>.  All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * Driver for the National Semiconductor DP83891 and DP83861
 * 10/100/1000 PHYs.
 * Datasheet available at: http://www.national.com/ds/DP/DP83861.pdf
 *
 * The DP83891 is the older NatSemi gigE PHY which isn't being sold
 * anymore. The DP83861 is its replacement, which is an 'enhanced'
 * firmware driven component. The major difference between the
 * two is that the 83891 can't generate interrupts, while the
 * 83861 can. (I think it wasn't originally designed to do this, but
 * it can now thanks to firmware updates.) The 83861 also allows
 * access to its internal RAM via indirect register access.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <machine/clock.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/nsgphyreg.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

static int nsgphy_probe		(device_t);
static int nsgphy_attach	(device_t);
static int nsgphy_detach	(device_t);

static device_method_t nsgphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		nsgphy_probe),
	DEVMETHOD(device_attach,	nsgphy_attach),
	DEVMETHOD(device_detach,	nsgphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t nsgphy_devclass;

static driver_t nsgphy_driver = {
	"nsgphy",
	nsgphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(nsgphy, miibus, nsgphy_driver, nsgphy_devclass, 0, 0);

static int	nsgphy_service(struct mii_softc *, struct mii_data *,int);
static void	nsgphy_status(struct mii_softc *);
static int	nsgphy_mii_phy_auto(struct mii_softc *, int);
extern void	mii_phy_auto_timeout(void *);

static int nsgphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_NATSEMI) {
		if (MII_MODEL(ma->mii_id2) == MII_MODEL_NATSEMI_DP83891) {
			device_set_desc(dev, MII_STR_NATSEMI_DP83891);
			return(0);
		}
		if (MII_MODEL(ma->mii_id2) == MII_MODEL_NATSEMI_DP83861) {
			device_set_desc(dev, MII_STR_NATSEMI_DP83861);
			return(0);
		}
	}

	return(ENXIO);
}

static int nsgphy_attach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = nsgphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE;
	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define PRINT(s)	printf("%s%s", sep, s); sep = ", "

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
#if 0
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);
#endif

	mii_phy_reset(sc);

	device_printf(dev, " ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_TX, IFM_FDX, sc->mii_inst),
	    NSGPHY_S1000|NSGPHY_BMCR_FDX);
	PRINT("1000baseTX-FDX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_TX, 0, sc->mii_inst),
	    NSGPHY_S1000);
	PRINT("1000baseTX");
	sc->mii_capabilities =
	    (PHY_READ(sc, MII_BMSR) |
	    (BMSR_10TFDX|BMSR_10THDX)) & ma->mii_capmask;
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
	    NSGPHY_S100|NSGPHY_BMCR_FDX);
	PRINT("100baseTX-FDX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst), NSGPHY_S100);
	PRINT("100baseTX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
	    NSGPHY_S10|NSGPHY_BMCR_FDX);
	PRINT("10baseT-FDX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst), NSGPHY_S10);
	PRINT("10baseT");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	PRINT("auto");
	printf("\n");
#undef ADD
#undef PRINT

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int nsgphy_detach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_parent(dev));
	if (sc->mii_flags & MIIF_DOINGAUTO)
		untimeout(mii_phy_auto_timeout, sc, sc->mii_auto_ch);
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}
int
nsgphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
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
			reg = PHY_READ(sc, MII_BMCR);
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;


		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, NSGPHY_MII_BMCR) & NSGPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void) nsgphy_mii_phy_auto(sc, 0);
			break;
		case IFM_1000_TX:
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, NSGPHY_MII_BMCR,
				    NSGPHY_BMCR_FDX|NSGPHY_BMCR_SPD1);
			} else {
				PHY_WRITE(sc, NSGPHY_MII_BMCR,
				    NSGPHY_BMCR_SPD1);
			}
			PHY_WRITE(sc, NSGPHY_MII_ANAR, NSGPHY_SEL_TYPE);

			/*
			 * When setting the link manually, one side must
			 * be the master and the other the slave. However
			 * ifmedia doesn't give us a good way to specify
			 * this, so we fake it by using one of the LINK
			 * flags. If LINK0 is set, we program the PHY to
			 * be a master, otherwise it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, NSGPHY_MII_1000CTL,
				    NSGPHY_1000CTL_MSE|NSGPHY_1000CTL_MSC);
			} else {
				PHY_WRITE(sc, NSGPHY_MII_1000CTL,
				    NSGPHY_1000CTL_MSE);
			}
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO|BMCR_PDOWN);
			break;
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
			break;
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
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			break;

		/*
		 * Check to see if we have link.
		 */
		reg = PHY_READ(sc, NSGPHY_MII_PHYSUP);
		if (reg & NSGPHY_PHYSUP_LNKSTS)
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 * Actually, for gigE PHYs, we should wait longer, since
		 * 5 seconds is the mimimum time the documentation
		 * says to wait for a 1000mbps link to be established.
		 */
		if (++sc->mii_ticks != 10)
			return (0);
		
		sc->mii_ticks = 0;

		mii_phy_reset(sc);
		if (nsgphy_mii_phy_auto(sc, 0) == EJUSTRETURN)
			return(0);
		break;
	}

	/* Update the media status. */
	nsgphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
nsgphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, physup, anlpar, gstat;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, NSGPHY_MII_BMSR);
	physup = PHY_READ(sc, NSGPHY_MII_PHYSUP);
	if (physup & NSGPHY_PHYSUP_LNKSTS)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, NSGPHY_MII_BMCR);

	if (bmcr & NSGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & NSGPHY_BMCR_AUTOEN) {
		if ((bmsr & NSGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
		anlpar = PHY_READ(sc, NSGPHY_MII_ANLPAR);
		gstat = PHY_READ(sc, NSGPHY_MII_1000STS);
		if (gstat & NSGPHY_1000STS_LPFD)
			mii->mii_media_active |= IFM_1000_TX|IFM_FDX;
		else if (gstat & NSGPHY_1000STS_LPHD)
			mii->mii_media_active |= IFM_1000_TX|IFM_HDX;
		else if (anlpar & NSGPHY_ANLPAR_100T4)
			mii->mii_media_active |= IFM_100_T4;
		else if (anlpar & NSGPHY_ANLPAR_100FDX)
			mii->mii_media_active |= IFM_100_TX|IFM_FDX;
		else if (anlpar & NSGPHY_ANLPAR_100HDX)
			mii->mii_media_active |= IFM_100_TX;
		else if (anlpar & NSGPHY_ANLPAR_10FDX)
			mii->mii_media_active |= IFM_10_T|IFM_FDX;
		else if (anlpar & NSGPHY_ANLPAR_10HDX)
			mii->mii_media_active |= IFM_10_T|IFM_HDX;
		else
			mii->mii_media_active |= IFM_NONE;
		return;
	}

	switch(bmcr & (NSGPHY_BMCR_SPD1|NSGPHY_BMCR_SPD0)) {
	case NSGPHY_S1000:
		mii->mii_media_active |= IFM_1000_TX;
		break;
	case NSGPHY_S100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case NSGPHY_S10:
		mii->mii_media_active |= IFM_10_T;
		break;
	default:
		break;
	}

	if (bmcr & NSGPHY_BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	return;
}


static int
nsgphy_mii_phy_auto(mii, waitfor)
	struct mii_softc *mii;
	int waitfor;
{
	int bmsr, ktcr = 0, i;

	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		mii_phy_reset(mii);
		PHY_WRITE(mii, NSGPHY_MII_BMCR, 0);
		DELAY(1000);
		ktcr = PHY_READ(mii, NSGPHY_MII_1000CTL);
		PHY_WRITE(mii, NSGPHY_MII_1000CTL, ktcr |
		    (NSGPHY_1000CTL_AFD|NSGPHY_1000CTL_AHD));
		ktcr = PHY_READ(mii, NSGPHY_MII_1000CTL);
		DELAY(1000);
		PHY_WRITE(mii, NSGPHY_MII_ANAR,
		    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
		DELAY(1000);
		PHY_WRITE(mii, NSGPHY_MII_BMCR,
		    NSGPHY_BMCR_AUTOEN | NSGPHY_BMCR_STARTNEG);
	}

	if (waitfor) {
		/* Wait 500ms for it to complete. */
		for (i = 0; i < 500; i++) {
			if ((bmsr = PHY_READ(mii, NSGPHY_MII_BMSR)) &
			    NSGPHY_BMSR_ACOMP)
				return (0);
			DELAY(1000);
#if 0
		if ((bmsr & BMSR_ACOMP) == 0)
			printf("%s: autonegotiation failed to complete\n",
			    mii->mii_dev.dv_xname);
#endif
		}

		/*
		 * Don't need to worry about clearing MIIF_DOINGAUTO.
		 * If that's set, a timeout is pending, and it will
		 * clear the flag.
		 */
		return (EIO);
	}

	/*
	 * Just let it finish asynchronously.  This is for the benefit of
	 * the tick handler driving autonegotiation.  Don't want 500ms
	 * delays all the time while the system is running!
	 */
	if ((mii->mii_flags & MIIF_DOINGAUTO) == 0) {
		mii->mii_flags |= MIIF_DOINGAUTO;
		mii->mii_auto_ch = timeout(mii_phy_auto_timeout, mii, hz >> 1);
	}
	return (EJUSTRETURN);
}
