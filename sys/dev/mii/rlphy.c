/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 * driver for RealTek 8139 internal PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <machine/bus.h>
#include <pci/if_rlreg.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
   "$FreeBSD$";
#endif

static int rlphy_probe		__P((device_t));
static int rlphy_attach		__P((device_t));
static int rlphy_detach		__P((device_t));

static device_method_t rlphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		rlphy_probe),
	DEVMETHOD(device_attach,	rlphy_attach),
	DEVMETHOD(device_detach,	rlphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t rlphy_devclass;

static driver_t rlphy_driver = {
	"rlphy",
	rlphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(rlphy, miibus, rlphy_driver, rlphy_devclass, 0, 0);

int	rlphy_service __P((struct mii_softc *, struct mii_data *, int));
void	rlphy_status __P((struct mii_softc *));

static int rlphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;
	device_t		parent;

	ma = device_get_ivars(dev);
	parent = device_get_parent(device_get_parent(dev));

	/* Test for RealTek 8201L PHY */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_REALTEK &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_REALTEK_RTL8201L) {
		device_set_desc(dev, MII_STR_REALTEK_RTL8201L);
		return(0);
	}

	/*
	 * RealTek PHY doesn't have vendor/device ID registers:
	 * the rl driver fakes up a return value of all zeros.
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) != 0 ||
	    MII_MODEL(ma->mii_id2) != 0)
		return (ENXIO);

	/*
	 * Make sure the parent is an `rl'.
	 */
	if (strcmp(device_get_name(parent), "rl") != 0)
		return (ENXIO);

	device_set_desc(dev, "RealTek internal media interface");

	return (0);
}

static int rlphy_attach(dev)
	device_t		dev;
{
	struct mii_softc	*sc;
	struct mii_attach_args	*ma;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);

	/*
	 * The RealTek PHY can never be isolated, so never allow non-zero
	 * instances!
	 */
	if (mii->mii_instance != 0) {
		device_printf(dev, "ignoring this PHY, non-zero instance\n");
		return(ENXIO);
	}

	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = rlphy_service;
	sc->mii_pdata = mii;
	mii->mii_instance++;

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

#if 0 /* See above. */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
#endif

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);

	mii_phy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->mii_capabilities,
		    sc->mii_inst);
	printf("\n");
#undef ADD
	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int rlphy_detach(dev)
	device_t		dev;
{
	struct mii_softc	*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_softc(dev));
	mii_phy_auto_stop(sc);
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}

int
rlphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	/*
	 * We can't isolate the RealTek PHY, so it has to be the only one!
	 */
	if (IFM_INST(ife->ifm_media) != sc->mii_inst)
		panic("rlphy_service: can't isolate RealTek PHY");

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, MII_BMCR) & BMCR_AUTOEN)
				return (0);
			(void) mii_phy_auto(sc, 0);
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		default:
			/*
			 * BMCR data is stored in the ifmedia entry.
			 */
			PHY_WRITE(sc, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;

		/*
		 * The RealTek PHY's autonegotiation doesn't need to be
		 * kicked; it continues in the background.
		 */
		break;
	}

	/* Update the media status. */
	rlphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

void
rlphy_status(phy)
	struct mii_softc *phy;
{
	struct mii_data *mii = phy->mii_pdata;
	int bmsr, bmcr, anlpar;
	device_t		parent;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(phy, MII_BMSR) | PHY_READ(phy, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(phy, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
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
			return;
		}

		if ((anlpar = PHY_READ(phy, MII_ANAR) &
		    PHY_READ(phy, MII_ANLPAR))) {
			if (anlpar & ANLPAR_T4)
				mii->mii_media_active |= IFM_100_T4;
			else if (anlpar & ANLPAR_TX_FD)
				mii->mii_media_active |= IFM_100_TX|IFM_FDX;
			else if (anlpar & ANLPAR_TX)
				mii->mii_media_active |= IFM_100_TX;
			else if (anlpar & ANLPAR_10_FD)
				mii->mii_media_active |= IFM_10_T|IFM_FDX;
			else if (anlpar & ANLPAR_10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_NONE; 
			return;
		}
		/*
		 * If the other side doesn't support NWAY, then the
		 * best we can do is determine if we have a 10Mbps or
		 * 100Mbps link. There's no way to know if the link 
		 * is full or half duplex, so we default to half duplex
		 * and hope that the user is clever enough to manually
		 * change the media settings if we're wrong.
		 */


		/*
		 * The RealTek PHY supports non-NWAY link speed
		 * detection, however it does not report the link
		 * detection results via the ANLPAR or BMSR registers.
		 * (What? RealTek doesn't do things the way everyone
		 * else does? I'm just shocked, shocked I tell you.)
		 * To determine the link speed, we have to do one
		 * of two things:
		 *
		 * - If this is a standalone RealTek RTL8201(L) PHY,
		 *   we can determine the link speed by testing bit 0
		 *   in the magic, vendor-specific register at offset
		 *   0x19.
		 *
		 * - If this is a RealTek MAC with integrated PHY, we
		 *   can test the 'SPEED10' bit of the MAC's media status
		 *   register.
		 */
		parent = device_get_parent(phy->mii_dev);
		if (strcmp(device_get_name(parent), "rl") != 0) {
			if (PHY_READ(phy, 0x0019) & 0x01)
				mii->mii_media_active |= IFM_100_TX;
			else
				mii->mii_media_active |= IFM_10_T;
		} else {
			if (PHY_READ(phy, RL_MEDIASTAT) &
			    RL_MEDIASTAT_SPEED10)
				mii->mii_media_active |= IFM_10_T;
			else
				mii->mii_media_active |= IFM_100_TX;
		}

	} else
		mii->mii_media_active = mii_media_from_bmcr(bmcr);
}
