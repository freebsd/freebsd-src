/*-
 * Copyright (c) 2001-2003, Shunsuke Akiyama <akiyama@FreeBSD.org>.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * driver for RealTek RTL8150 internal PHY
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <machine/bus.h>
#include <dev/mii/ruephyreg.h>

#include "miibus_if.h"

static int ruephy_probe(device_t);
static int ruephy_attach(device_t);

static device_method_t ruephy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ruephy_probe),
	DEVMETHOD(device_attach,	ruephy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t ruephy_devclass;

static driver_t ruephy_driver = {
	"ruephy",
	ruephy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(ruephy, miibus, ruephy_driver, ruephy_devclass, 0, 0);

static int ruephy_service(struct mii_softc *, struct mii_data *, int);
static void ruephy_reset(struct mii_softc *);
static void ruephy_status(struct mii_softc *);

static int
ruephy_probe(device_t dev)
{
	struct mii_attach_args *ma;
	device_t		parent;

	ma = device_get_ivars(dev);
	parent = device_get_parent(device_get_parent(dev));

	/*
	 * RealTek RTL8150 PHY doesn't have vendor/device ID registers:
	 * the rue driver fakes up a return value of all zeros.
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) != 0 ||
	    MII_MODEL(ma->mii_id2) != 0)
		return (ENXIO);

	/*
	 * Make sure the parent is an 'rue'.
	 */
	if (strcmp(device_get_name(parent), "rue") != 0)
		return (ENXIO);

	device_set_desc(dev, "RealTek RTL8150 internal media interface");

	return (BUS_PROBE_DEFAULT);
}

static int
ruephy_attach(device_t dev)
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
		return (ENXIO);
	}

	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = ruephy_service;
	sc->mii_pdata = mii;
	mii->mii_instance++;

	sc->mii_flags |= MIIF_NOISOLATE;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ruephy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");
#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
ruephy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	/*
	 * We can't isolate the RealTek RTL8150 PHY,
	 * so it has to be the only one!
	 */
	if (IFM_INST(ife->ifm_media) != sc->mii_inst)
		panic("ruephy_service: can't isolate RealTek RTL8150 PHY");

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
			(void) mii_phy_auto(sc);
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
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the MSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, RUEPHY_MII_MSR) |
		      PHY_READ(sc, RUEPHY_MII_MSR);
		if (reg & RUEPHY_MSR_LINK)
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks <= 5)
			break;

		sc->mii_ticks = 0;
		ruephy_reset(sc);
		if (mii_phy_auto(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	ruephy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);

	return (0);
}

static void
ruephy_reset(struct mii_softc *sc)
{

	mii_phy_reset(sc);

	/*
	 * XXX RealTek RTL8150 PHY doesn't set the BMCR properly after
	 * XXX reset, which breaks autonegotiation.
	 */
	PHY_WRITE(sc, MII_BMCR, (BMCR_S100 | BMCR_AUTOEN | BMCR_FDX));
}

static void
ruephy_status(struct mii_softc *phy)
{
	struct mii_data *mii = phy->mii_pdata;
	int bmsr, bmcr, msr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	msr = PHY_READ(phy, RUEPHY_MII_MSR) | PHY_READ(phy, RUEPHY_MII_MSR);
	if (msr & RUEPHY_MSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(phy, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	bmsr = PHY_READ(phy, MII_BMSR) | PHY_READ(phy, MII_BMSR);

	if (bmcr & BMCR_AUTOEN) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if (msr & RUEPHY_MSR_SPEED100)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;

		if (msr & RUEPHY_MSR_DUPLEX)
			mii->mii_media_active |= IFM_FDX;
	} else
		mii->mii_media_active = mii_media_from_bmcr(bmcr);
}
