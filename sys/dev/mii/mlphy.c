/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * driver for Micro Linear 6692 PHYs
 *
 * The Micro Linear 6692 is a strange beast, and dealing with it using
 * this code framework is tricky. The 6692 is actually a 100Mbps-only
 * device, which means that a second PHY is required to support 10Mbps
 * modes. However, even though the 6692 does not support 10Mbps modes,
 * it can still advertise them when performing autonegotiation. If a
 * 10Mbps mode is negotiated, we must program the registers of the
 * companion PHY accordingly in addition to programming the registers
 * of the 6692.
 *
 * This device also does not have vendor/device ID registers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

#define ML_STATE_AUTO_SELF	1
#define ML_STATE_AUTO_OTHER	2

struct mlphy_softc	{
	struct mii_softc	ml_mii;
	int			ml_state;
	int			ml_linked;
};

static int mlphy_probe(device_t);
static int mlphy_attach(device_t);

static device_method_t mlphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		mlphy_probe),
	DEVMETHOD(device_attach,	mlphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t mlphy_devclass;

static driver_t mlphy_driver = {
	"mlphy",
	mlphy_methods,
	sizeof(struct mlphy_softc)
};

DRIVER_MODULE(mlphy, miibus, mlphy_driver, mlphy_devclass, 0, 0);

static int	mlphy_service(struct mii_softc *, struct mii_data *, int);
static void	mlphy_reset(struct mii_softc *);
static void	mlphy_status(struct mii_softc *);

static int
mlphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args	*ma;
	device_t		parent;

	ma = device_get_ivars(dev);
	parent = device_get_parent(device_get_parent(dev));

	/*
	 * Micro Linear PHY reports oui == 0 model == 0
	 */
	if (MII_OUI(ma->mii_id1, ma->mii_id2) != 0 ||
	    MII_MODEL(ma->mii_id2) != 0)
		return (ENXIO);

	/*
	 * Make sure the parent is a `tl'. So far, I have only
	 * encountered the 6692 on an Olicom card with a ThunderLAN
	 * controller chip.
	 */
	if (strcmp(device_get_name(parent), "tl") != 0)
		return (ENXIO);

	device_set_desc(dev, "Micro Linear 6692 media interface");

	return (BUS_PROBE_DEFAULT);
}

static int
mlphy_attach(dev)
	device_t		dev;
{
	struct mlphy_softc *msc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	msc = device_get_softc(dev);
	sc = &msc->ml_mii;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = mlphy_service;
	sc->mii_pdata = mii;

	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

#if 0 /* See above. */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
#endif
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);

	sc->mii_flags &= ~MIIF_NOISOLATE;
	mii_phy_reset(sc);
	sc->mii_flags |= MIIF_NOISOLATE;

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	ma->mii_capmask = ~sc->mii_capabilities;
	device_printf(dev, " ");
	mii_add_media(sc);
	printf("\n");
#undef ADD
	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int
mlphy_service(xsc, mii, cmd)
	struct mii_softc *xsc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry	*ife = mii->mii_media.ifm_cur;
	int			reg;
	struct mii_softc	*other = NULL;
	struct mlphy_softc	*msc = (struct mlphy_softc *)xsc;
	struct mii_softc	*sc = (struct mii_softc *)&msc->ml_mii;
	device_t		*devlist;
	int			devs, i;

	/*
	 * See if there's another PHY on this bus with us.
	 * If so, we may need it for 10Mbps modes.
	 */
	device_get_children(msc->ml_mii.mii_dev, &devlist, &devs);
	for (i = 0; i < devs; i++) {
		if (strcmp(device_get_name(devlist[i]), "mlphy")) {
			other = device_get_softc(devlist[i]);
			break;
		}
	}
	free(devlist, M_TEMP);

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
			/*
			 * For autonegotiation, reset and isolate the
			 * companion PHY (if any) and then do NWAY
			 * autonegotiation ourselves.
			 */
			msc->ml_state = ML_STATE_AUTO_SELF;
			if (other != NULL) {
				mii_phy_reset(other);
				PHY_WRITE(other, MII_BMCR, BMCR_ISO);
			}
			(void) mii_phy_auto(sc);
			msc->ml_linked = 0;
			return(0);
		case IFM_10_T:
			/*
			 * For 10baseT modes, reset and program the
			 * companion PHY (of any), then program ourselves
			 * to match. This will put us in pass-through
			 * mode and let the companion PHY do all the
			 * work.
			 *
			 * BMCR data is stored in the ifmedia entry.
			 */
			if (other != NULL) {
				mii_phy_reset(other);
				PHY_WRITE(other, MII_BMCR, ife->ifm_data);
			}
			PHY_WRITE(sc, MII_ANAR, mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
			msc->ml_state = 0;
			break;
		case IFM_100_TX:
			/*
			 * For 100baseTX modes, reset and isolate the
			 * companion PHY (if any), then program ourselves
			 * accordingly.
			 *
			 * BMCR data is stored in the ifmedia entry.
			 */
			if (other != NULL) {
				mii_phy_reset(other);
				PHY_WRITE(other, MII_BMCR, BMCR_ISO);
			}
			PHY_WRITE(sc, MII_ANAR, mii_anar(ife->ifm_media));
			PHY_WRITE(sc, MII_BMCR, ife->ifm_data);
			msc->ml_state = 0;
			break;
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		default:
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
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 * If we're in a 10Mbps mode, check the link of the
		 * 10Mbps PHY. Sometimes the Micro Linear PHY's
		 * linkstat bit will clear while the linkstat bit of
		 * the companion PHY will remain set.
		 */
		if (msc->ml_state == ML_STATE_AUTO_OTHER) {
			reg = PHY_READ(other, MII_BMSR) |
			    PHY_READ(other, MII_BMSR);
		} else {
			reg = PHY_READ(sc, MII_BMSR) |
			    PHY_READ(sc, MII_BMSR);
		}

		if (reg & BMSR_LINK) {
			if (!msc->ml_linked) {
				msc->ml_linked = 1;
				mlphy_status(sc);
			}
			break;
		}

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks <= 5)
			break;
		
		sc->mii_ticks = 0;
		msc->ml_linked = 0;
		mii->mii_media_active = IFM_NONE;
		mii_phy_reset(sc);
		msc->ml_state = ML_STATE_AUTO_SELF;
		if (other != NULL) {
			mii_phy_reset(other);
			PHY_WRITE(other, MII_BMCR, BMCR_ISO);
		}
		mii_phy_auto(sc);
		return(0);
	}

	/* Update the media status. */

	if (msc->ml_state == ML_STATE_AUTO_OTHER) {
		int			other_inst;
		other_inst = other->mii_inst;
		other->mii_inst = sc->mii_inst;
		(void) (*other->mii_service)(other, mii, MII_POLLSTAT);
		other->mii_inst = other_inst;
		sc->mii_media_active = other->mii_media_active;
		sc->mii_media_status = other->mii_media_status;
	} else
		ukphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

/*
 * The Micro Linear PHY comes out of reset with the 'autoneg
 * enable' bit set, which we don't want.
 */
static void
mlphy_reset(sc)
	struct mii_softc	*sc;
{
	int			reg;

	mii_phy_reset(sc);
	reg = PHY_READ(sc, MII_BMCR);
	reg &= ~BMCR_AUTOEN;
	PHY_WRITE(sc, MII_BMCR, reg);

	return;
}

/*
 * If we negotiate a 10Mbps mode, we need to check for an alternate
 * PHY and make sure it's enabled and set correctly.
 */
static void
mlphy_status(sc)
	struct mii_softc	*sc;
{
	struct mlphy_softc	*msc = (struct mlphy_softc *)sc;
	struct mii_data		*mii = msc->ml_mii.mii_pdata;
	struct mii_softc	*other = NULL;
	device_t		*devlist;
	int			devs, i;

	/* See if there's another PHY on the bus with us. */
	device_get_children(msc->ml_mii.mii_dev, &devlist, &devs);
	for (i = 0; i < devs; i++) {
		if (strcmp(device_get_name(devlist[i]), "mlphy")) {
			other = device_get_softc(devlist[i]);
			break;
		}
	}
	free(devlist, M_TEMP);

	if (other == NULL)
		return;

	ukphy_status(sc);

	if (IFM_SUBTYPE(mii->mii_media_active) != IFM_10_T) {
		msc->ml_state = ML_STATE_AUTO_SELF;
		mii_phy_reset(other);
		PHY_WRITE(other, MII_BMCR, BMCR_ISO);
	}

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T) {
		msc->ml_state = ML_STATE_AUTO_OTHER;
		mlphy_reset(&msc->ml_mii);
		PHY_WRITE(&msc->ml_mii, MII_BMCR, BMCR_ISO);
		mii_phy_reset(other);
		mii_phy_auto(other);
	}

	return;
}
