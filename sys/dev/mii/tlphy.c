/*	$NetBSD: tlphy.c,v 1.18 1999/05/14 11:40:28 drochner Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for Texas Instruments's ThunderLAN PHYs
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/tlphyreg.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

struct tlphy_softc {
	struct mii_softc sc_mii;		/* generic PHY */
	int sc_need_acomp;
};

static int tlphy_probe(device_t);
static int tlphy_attach(device_t);

static device_method_t tlphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		tlphy_probe),
	DEVMETHOD(device_attach,	tlphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t tlphy_devclass;

static driver_t tlphy_driver = {
	"tlphy",
	tlphy_methods,
	sizeof(struct tlphy_softc)
};

DRIVER_MODULE(tlphy, miibus, tlphy_driver, tlphy_devclass, 0, 0);

static int	tlphy_service(struct mii_softc *, struct mii_data *, int);
static int	tlphy_auto(struct tlphy_softc *);
static void	tlphy_acomp(struct tlphy_softc *);
static void	tlphy_status(struct tlphy_softc *);

static int
tlphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;       

	ma = device_get_ivars(dev);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) != MII_OUI_xxTI ||
	    MII_MODEL(ma->mii_id2) != MII_MODEL_xxTI_TLAN10T)
		return (ENXIO);

	device_set_desc(dev, MII_STR_xxTI_TLAN10T);

	return (0);
}

static int
tlphy_attach(dev)
	device_t		dev;
{
	struct tlphy_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";
	int capmask = 0xFFFFFFFF;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->sc_mii.mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->sc_mii.mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, &sc->sc_mii, mii_list);

	sc->sc_mii.mii_inst = mii->mii_instance;
	sc->sc_mii.mii_phy = ma->mii_phyno;
	sc->sc_mii.mii_service = tlphy_service;
	sc->sc_mii.mii_pdata = mii;

	if (mii->mii_instance) {
		struct mii_softc	*other;
		device_t		*devlist;
		int			devs, i;

		device_get_children(sc->sc_mii.mii_dev, &devlist, &devs);
		for (i = 0; i < devs; i++) {
			if (strcmp(device_get_name(devlist[i]), "tlphy")) {
				other = device_get_softc(devlist[i]);
				capmask &= ~other->mii_capabilities;
				break;
			}
		}
		free(devlist, M_TEMP);
	}

	mii->mii_instance++;

	sc->sc_mii.mii_flags &= ~MIIF_NOISOLATE;
	mii_phy_reset(&sc->sc_mii);
	sc->sc_mii.mii_flags |= MIIF_NOISOLATE;

	/*
	 * Note that if we're on a device that also supports 100baseTX,
	 * we are not going to want to use the built-in 10baseT port,
	 * since there will be another PHY on the MII wired up to the
	 * UTP connector.  The parent indicates this to us by specifying
	 * the TLPHY_MEDIA_NO_10_T bit.
	 */
	sc->sc_mii.mii_capabilities =
	    PHY_READ(&sc->sc_mii, MII_BMSR) & capmask /*ma->mii_capmask*/;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->sc_mii.mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_LOOP,
	    sc->sc_mii.mii_inst), BMCR_LOOP);

#define	PRINT(s)	printf("%s%s", sep, s); sep = ", "

	device_printf(dev, " ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_2, 0, sc->sc_mii.mii_inst), 0);
	PRINT("10base2/BNC");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_5, 0, sc->sc_mii.mii_inst), 0);
	PRINT("10base5/AUI");

	if (sc->sc_mii.mii_capabilities & BMSR_MEDIAMASK) {
		printf("%s", sep);
		mii_add_media(&sc->sc_mii);
	}

	printf("\n");
#undef ADD
#undef PRINT
	MIIBUS_MEDIAINIT(sc->sc_mii.mii_dev);
	return(0);
}

static int
tlphy_service(self, mii, cmd)
	struct mii_softc *self;
	struct mii_data *mii;
	int cmd;
{
	struct tlphy_softc *sc = (struct tlphy_softc *)self;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg;

	if (sc->sc_need_acomp)
		tlphy_acomp(sc);

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
			return (0);
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst) {
			reg = PHY_READ(&sc->sc_mii, MII_BMCR);
			PHY_WRITE(&sc->sc_mii, MII_BMCR, reg | BMCR_ISO);
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
			 * The ThunderLAN PHY doesn't self-configure after
			 * an autonegotiation cycle, so there's no such
			 * thing as "already in auto mode".
			 */
			(void) tlphy_auto(sc);
			break;
		case IFM_10_2:
		case IFM_10_5:
			PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
			PHY_WRITE(&sc->sc_mii, MII_TLPHY_CTRL, CTRL_AUISEL);
			DELAY(100000);
			break;
		default:
			PHY_WRITE(&sc->sc_mii, MII_TLPHY_CTRL, 0);
			DELAY(100000);
			PHY_WRITE(&sc->sc_mii, MII_ANAR,
			    mii_anar(ife->ifm_media));
			PHY_WRITE(&sc->sc_mii, MII_BMCR, ife->ifm_data);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->sc_mii.mii_inst)
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
		 *
		 * XXX WHAT ABOUT CHECKING LINK ON THE BNC/AUI?!
		 */
		reg = PHY_READ(&sc->sc_mii, MII_BMSR) |
		    PHY_READ(&sc->sc_mii, MII_BMSR);
		if (reg & BMSR_LINK)
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->sc_mii.mii_ticks != 5)
			return (0);

		sc->sc_mii.mii_ticks = 0;
		mii_phy_reset(&sc->sc_mii);
		tlphy_auto(sc);
		return (0);
	}

	/* Update the media status. */
	tlphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(&sc->sc_mii, cmd);
	return (0);
}

static void
tlphy_status(sc)
	struct tlphy_softc *sc;
{
	struct mii_data *mii = sc->sc_mii.mii_pdata;
	int bmsr, bmcr, tlctrl;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmcr = PHY_READ(&sc->sc_mii, MII_BMCR);
	if (bmcr & BMCR_ISO) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;  
		return;
	}

	tlctrl = PHY_READ(&sc->sc_mii, MII_TLPHY_CTRL);
	if (tlctrl & CTRL_AUISEL) {
		mii->mii_media_status = 0;
		mii->mii_media_active = mii->mii_media.ifm_cur->ifm_media;
		return;
	}

	bmsr = PHY_READ(&sc->sc_mii, MII_BMSR) |
	    PHY_READ(&sc->sc_mii, MII_BMSR);
	if (bmsr & BMSR_LINK)   
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't have any way to
	 * tell which media is actually active.  (Note it also
	 * doesn't self-configure after autonegotiation.)  We
	 * just have to report what's in the BMCR.
	 */
	if (bmcr & BMCR_FDX)
		mii->mii_media_active |= IFM_FDX;
	mii->mii_media_active |= IFM_10_T;
}

static int
tlphy_auto(sc)
	struct tlphy_softc *sc;
{
	int error;

	switch ((error = mii_phy_auto(&sc->sc_mii))) {
	case EIO:
		/*
		 * Just assume we're not in full-duplex mode.
		 * XXX Check link and try AUI/BNC?
		 */
		PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
		break;

	case EJUSTRETURN:
		/* Flag that we need to program when it completes. */
		sc->sc_need_acomp = 1;
		break;

	default:
		tlphy_acomp(sc);
	}

	return (error);
}

static void
tlphy_acomp(sc)
	struct tlphy_softc *sc;
{
	int aner, anlpar;

	sc->sc_need_acomp = 0;

	/*
	 * Grr, braindead ThunderLAN PHY doesn't self-configure
	 * after autonegotiation.  We have to do it ourselves
	 * based on the link partner status.
	 */

	aner = PHY_READ(&sc->sc_mii, MII_ANER);
	if (aner & ANER_LPAN) {
		anlpar = PHY_READ(&sc->sc_mii, MII_ANLPAR) &
		    PHY_READ(&sc->sc_mii, MII_ANAR);
		if (anlpar & ANAR_10_FD) {
			PHY_WRITE(&sc->sc_mii, MII_BMCR, BMCR_FDX);
			return;
		}
	}
	PHY_WRITE(&sc->sc_mii, MII_BMCR, 0);
}
