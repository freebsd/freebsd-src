/*	$NetBSD: nsphyter.c,v 1.28 2008/01/20 07:58:19 msaitoh Exp $	*/

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

/*-
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mii/nsphyter.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * driver for National Semiconductor's DP83843 `PHYTER' ethernet 10/100 PHY
 * Data Sheet available from www.national.com
 *
 * We also support the DP83815 `MacPHYTER' internal PHY since, for our
 * purposes, they are compatible.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/nsphyterreg.h>

#include "miibus_if.h"

static device_probe_t	nsphyter_probe;
static device_attach_t	nsphyter_attach;

static device_method_t nsphyter_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		nsphyter_probe),
	DEVMETHOD(device_attach,	nsphyter_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t nsphyter_devclass;

static driver_t nsphyter_driver = {
	"nsphyter",
	nsphyter_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(nsphyter, miibus, nsphyter_driver, nsphyter_devclass, 0, 0);

static int	nsphyter_service(struct mii_softc *, struct mii_data *, int);
static void	nsphyter_status(struct mii_softc *);
static void	nsphyter_reset(struct mii_softc *);

static const struct mii_phydesc nsphys[] = {
	MII_PHY_DESC(NATSEMI, DP83815),
	MII_PHY_DESC(NATSEMI, DP83843),
	MII_PHY_DESC(NATSEMI, DP83847),
	MII_PHY_END
};

static int
nsphyter_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, nsphys, BUS_PROBE_DEFAULT));
}

static int
nsphyter_attach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *nic;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = nsphyter_service;
	sc->mii_pdata = mii;

	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	nic = device_get_name(device_get_parent(sc->mii_dev));
	/*
	 * In order for MII loopback to work Am79C971 and greater PCnet
	 * chips additionally need to be placed into external loopback
	 * mode which pcn(4) doesn't do so far.
	 */
	if (strcmp(nic, "pcn") != 0)
#if 1
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP,
		    sc->mii_inst), MII_MEDIA_100_TX);
#else
	if (strcmp(nic, "pcn") == 0)
		sc->mii_flags |= MIIF_NOLOOP;
#endif

	nsphyter_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
nsphyter_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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

		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	nsphyter_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
nsphyter_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr, physts;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
	physts = PHY_READ(sc, MII_NSPHYTER_PHYSTS);

	if ((physts & PHYSTS_LINK) != 0)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, MII_BMCR);
	if ((bmcr & BMCR_ISO) != 0) {
		mii->mii_media_active |= IFM_NONE;
		mii->mii_media_status = 0;
		return;
	}

	if ((bmcr & BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & BMCR_AUTOEN) != 0) {
		/*
		 * The media status bits are only valid if autonegotiation
		 * has completed (or it's disabled).
		 */
		if ((bmsr & BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		if ((physts & PHYSTS_SPEED10) != 0)
			mii->mii_media_active |= IFM_10_T;
		else
			mii->mii_media_active |= IFM_100_TX;
		if ((physts & PHYSTS_DUPLEX) != 0)
#ifdef notyet
			mii->mii_media_active |=
			    IFM_FDX | mii_phy_flowstatus(sc);
#else
			mii->mii_media_active |= IFM_FDX;
#endif
		else
			mii->mii_media_active |= IFM_HDX;
	} else
		mii->mii_media_active = ife->ifm_media;
}

static void
nsphyter_reset(struct mii_softc *sc)
{
	struct ifmedia_entry *ife = sc->mii_pdata->mii_media.ifm_cur;
	int reg, i;

	if ((sc->mii_flags & MIIF_NOISOLATE) != 0)
		reg = BMCR_RESET;
	else
		reg = BMCR_RESET | BMCR_ISO;
	PHY_WRITE(sc, MII_BMCR, reg);

	/*
	 * It is best to allow a little time for the reset to settle
	 * in before we start polling the BMCR again.  Notably, the
	 * DP8384{3,7} manuals state that there should be a 500us delay
	 * between asserting software reset and attempting MII serial
	 * operations.  Be conservative.  Also, a DP83815 can get into
	 * a bad state on cable removal and reinsertion if we do not
	 * delay here.
	 */
	DELAY(1000);

	/*
	 * Wait another 2s for it to complete.
	 * This is only a little overkill as under normal circumstances
	 * the PHY can take up to 1s to complete reset.
	 * This is also a bit odd because after a reset, the BMCR will
	 * clear the reset bit and simply reports 0 even though the reset
	 * is not yet complete.
	 */
	for (i = 0; i < 1000; i++) {
		reg = PHY_READ(sc, MII_BMCR);
		if (reg != 0 && (reg & BMCR_RESET) == 0)
			break;
		DELAY(2000);
	}

	if ((sc->mii_flags & MIIF_NOISOLATE) == 0) {
		if ((ife == NULL && sc->mii_inst != 0) ||
		    (ife != NULL && IFM_INST(ife->ifm_media) != sc->mii_inst))
			PHY_WRITE(sc, MII_BMCR, reg | BMCR_ISO);
	}
}
