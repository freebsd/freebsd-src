/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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
__FBSDID("$FreeBSD$");

/*
 * Driver for the internal PHY on the SMSC LAN91C111.
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
#include "miidevs.h"

#include "miibus_if.h"

static int	smcphy_probe(device_t);
static int	smcphy_attach(device_t);

static int	smcphy_service(struct mii_softc *, struct mii_data *, int);
static int	smcphy_reset(struct mii_softc *);
static void	smcphy_auto(struct mii_softc *, int);

static device_method_t smcphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		smcphy_probe),
	DEVMETHOD(device_attach,	smcphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	{ 0, 0 }
};

static devclass_t smcphy_devclass;

static driver_t smcphy_driver = {
	"smcphy",
	smcphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(smcphy, miibus, smcphy_driver, smcphy_devclass, 0, 0);

static const struct mii_phydesc smcphys[] = {
	MII_PHY_DESC(SMSC, LAN83C183),
	MII_PHY_END
};

static int
smcphy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, smcphys, BUS_PROBE_DEFAULT));
}

static int
smcphy_attach(device_t dev)
{
	struct	mii_softc *sc;
	struct	mii_attach_args *ma;
	struct	mii_data *mii;

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = ma->mii_data;
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_flags = miibus_get_flags(dev);
	sc->mii_inst = mii->mii_instance++;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = smcphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE | MIIF_NOLOOP;

	if (smcphy_reset(sc) != 0) {
		device_printf(dev, "reset failed\n");
	}

	/* Mask interrupts, we poll instead. */
	PHY_WRITE(sc, 0x1e, 0xffc0);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	mii_phy_add_media(sc);
	printf("\n");

	MIIBUS_MEDIAINIT(sc->mii_dev);
	mii_phy_setmedia(sc);

	return (0);
}

static int
smcphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
        struct	ifmedia_entry *ife;
        int	reg;

	ife = mii->mii_media.ifm_cur;

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
			smcphy_auto(sc, ife->ifm_media);
			break;

		default:
                	mii_phy_setmedia(sc);
			break;
		}

                break;

        case MII_TICK:
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0) {
			return (0);
		}

		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			break;
		}

		/* I have no idea why BMCR_ISO gets set. */
		reg = PHY_READ(sc, MII_BMCR);
		if (reg & BMCR_ISO) {
			PHY_WRITE(sc, MII_BMCR, reg & ~BMCR_ISO);
		}

		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK) {
			sc->mii_ticks = 0;
			break;
		}

		if (++sc->mii_ticks <= MII_ANEGTICKS) {
			break;
		}

		sc->mii_ticks = 0;
		if (smcphy_reset(sc) != 0) {
			device_printf(sc->mii_dev, "reset failed\n");
		}
		smcphy_auto(sc, ife->ifm_media);
                break;
        }

        /* Update the media status. */
        ukphy_status(sc);

        /* Callback if something changed. */
        mii_phy_update(sc, cmd);
        return (0);
}

static int
smcphy_reset(struct mii_softc *sc)
{
	u_int	bmcr;
	int	timeout;

	PHY_WRITE(sc, MII_BMCR, BMCR_RESET);

	for (timeout = 2; timeout > 0; timeout--) {
		DELAY(50000);
		bmcr = PHY_READ(sc, MII_BMCR);
		if ((bmcr & BMCR_RESET) == 0)
			break;
	}

	if (bmcr & BMCR_RESET) {
		return (EIO);
	}

	PHY_WRITE(sc, MII_BMCR, 0x3000);
	return (0);
}

static void
smcphy_auto(struct mii_softc *sc, int media)
{
	uint16_t	anar;

	anar = BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) |
	    ANAR_CSMA;
	if ((media & IFM_FLOW) != 0 || (sc->mii_flags & MIIF_FORCEPAUSE) != 0)
		anar |= ANAR_FC;
	PHY_WRITE(sc, MII_ANAR, anar);
	/* Apparently this helps. */
	anar = PHY_READ(sc, MII_ANAR);
	PHY_WRITE(sc, MII_BMCR, BMCR_AUTOEN | BMCR_STARTNEG);
}
