/*	$NetBSD: exphy.c,v 1.16 1999/04/23 04:24:32 thorpej Exp $	*/

/*-
 * Copyright (c) 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Frank van der Linden.
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
__FBSDID("$FreeBSD$");

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
 * driver for 3Com internal PHYs
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include "miibus_if.h"

static int exphy_probe(device_t);
static int exphy_attach(device_t);

static device_method_t exphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		exphy_probe),
	DEVMETHOD(device_attach,	exphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t exphy_devclass;

static driver_t exphy_driver = {
	"xlphy",
	exphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(xlphy, miibus, exphy_driver, exphy_devclass, 0, 0);

static int	exphy_service(struct mii_softc *, struct mii_data *, int);
static void	exphy_reset(struct mii_softc *);

static int
exphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;
	device_t		parent;

	ma = device_get_ivars(dev);
	parent = device_get_parent(device_get_parent(dev));

	/*
	 * Argh, 3Com PHY reports oui == 0 model == 0!
	 */
	if ((MII_OUI(ma->mii_id1, ma->mii_id2) != 0 ||
	    MII_MODEL(ma->mii_id2) != 0) &&
	    (MII_OUI(ma->mii_id1, ma->mii_id2) != MII_OUI_BROADCOM ||
	    MII_MODEL(ma->mii_id2) != MII_MODEL_BROADCOM_3C905C))
		return (ENXIO);

	/*
	 * Make sure the parent is an `ex'.
	 */
	if (strcmp(device_get_name(parent), "xl") != 0)
		return (ENXIO);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == 0)
		device_set_desc(dev, "3Com internal media interface");
	else
		device_set_desc(dev, MII_STR_BROADCOM_3C905C);

	return (0);
}

static int
exphy_attach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);

	/*
	 * The 3Com PHY can never be isolated, so never allow non-zero
	 * instances!
	 */
	if (mii->mii_instance != 0) {
		device_printf(dev, "ignoring this PHY, non-zero instance\n");
		return(ENXIO);
	}

	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = exphy_service;
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

	exphy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");
#undef ADD
	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int
exphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	/*
	 * We can't isolate the 3Com PHY, so it has to be the only one!
	 */
	if (IFM_INST(ife->ifm_media) != sc->mii_inst)
		panic("exphy_service: can't isolate 3Com PHY");

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		mii_phy_setmedia(sc);
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
		 * The 3Com PHY's autonegotiation doesn't need to be
		 * kicked; it continues in the background.
		 */
		break;
	}

	/* Update the media status. */
	ukphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
exphy_reset(struct mii_softc *sc)
{

	mii_phy_reset(sc);

	/*
	 * XXX 3Com PHY doesn't set the BMCR properly after
	 * XXX reset, which breaks autonegotiation.
	 */
	PHY_WRITE(sc, MII_BMCR, BMCR_S100|BMCR_AUTOEN|BMCR_FDX);
}
