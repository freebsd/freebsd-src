/*-
 * Copyright (c) 2001 Jonathan Lemon
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	$FreeBSD$
 */

/*
 * driver for Intel 82553 and 82555 PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/inphyreg.h>

#include "miibus_if.h"

static int 	inphy_probe(device_t dev);
static int 	inphy_attach(device_t dev);
static int 	inphy_detach(device_t dev);

static device_method_t inphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		inphy_probe),
	DEVMETHOD(device_attach,	inphy_attach),
	DEVMETHOD(device_detach,	inphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t inphy_devclass;

static driver_t inphy_driver = {
	"inphy",
	inphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(inphy, miibus, inphy_driver, inphy_devclass, 0, 0);

int	inphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd);
void	inphy_status(struct mii_softc *sc);


static int
inphy_probe(device_t dev)
{
	struct mii_attach_args *ma;
	device_t parent;

	ma = device_get_ivars(dev);
	parent = device_get_parent(device_get_parent(dev));

	/* Intel 82553 A/B steppings */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxINTEL &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxINTEL_I82553AB) {
		device_set_desc(dev, MII_STR_xxINTEL_I82553AB);
		return (0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_INTEL) {
		switch (MII_MODEL(ma->mii_id2)) {
		case MII_MODEL_INTEL_I82555:
			device_set_desc(dev, MII_STR_INTEL_I82555);
			return (0);
		case MII_MODEL_INTEL_I82553C:
			device_set_desc(dev, MII_STR_INTEL_I82553C);
			return (0);
		case MII_MODEL_INTEL_I82562EM:
			device_set_desc(dev, MII_STR_INTEL_I82562EM);
			return (0);
		case MII_MODEL_INTEL_I82562ET:
			device_set_desc(dev, MII_STR_INTEL_I82562ET);
			return (0);
		}
	}

	return (ENXIO);
}

static int
inphy_attach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = inphy_service;
	sc->mii_pdata = mii;
	mii->mii_instance++;

#if 0
	sc->mii_flags |= MIIF_NOISOLATE;
#endif

	ifmedia_add(&mii->mii_media,
	    IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100, NULL);

	mii_phy_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	if ((sc->mii_capabilities & BMSR_MEDIAMASK) == 0)
		printf("no media present");
	else
		mii_add_media(mii, sc->mii_capabilities, sc->mii_inst);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);

	return (0);
}

static int
inphy_detach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_softc(dev));
	mii_phy_auto_stop(sc);
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return (0);
}

int
inphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	switch (cmd) {
	case MII_POLLSTAT:
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
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
			PHY_WRITE(sc, MII_ANAR, mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
		}
		break;

	case MII_TICK:
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
			return (0);

		/*
		 * check for link.
        	 * Read the status register twice; BMSR_LINK is latch-low.
		 */
        	reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
	                return (0);

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;
		mii_phy_reset(sc);
		if (mii_phy_auto(sc, 0) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	inphy_status(sc);

	/* Callback if something changed. */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_active = mii->mii_media_active;
	}
	return (0);
}

void
inphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, scr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	if (bmsr & BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BMCR_AUTOEN) {
		if ((bmsr & BMSR_ACOMP) == 0) {
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		scr = PHY_READ(sc, MII_INPHY_SCR);
		if (scr & SCR_S100)
			mii->mii_media_active |= IFM_100_TX;
		else
			mii->mii_media_active |= IFM_10_T;
		if (scr & SCR_FDX)
			mii->mii_media_active |= IFM_FDX;
	} else
		mii->mii_media_active |= mii_media_from_bmcr(bmcr);
}
