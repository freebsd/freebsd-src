/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
__FBSDID("$FreeBSD: src/sys/dev/dc/pnphy.c,v 1.21.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Pseudo-driver for media selection on the Lite-On PNIC 82c168
 * chip. The NWAY support on this chip is horribly broken, so we
 * only support manual mode selection. This is lame, but getting
 * NWAY to work right is amazingly difficult.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>

#include <dev/dc/if_dcreg.h>

#include "miibus_if.h"

#define DC_SETBIT(sc, reg, x)                           \
        CSR_WRITE_4(sc, reg,                            \
                CSR_READ_4(sc, reg) | x)

#define DC_CLRBIT(sc, reg, x)                           \
        CSR_WRITE_4(sc, reg,                            \
                CSR_READ_4(sc, reg) & ~x)

static int pnphy_probe(device_t);
static int pnphy_attach(device_t);

static device_method_t pnphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		pnphy_probe),
	DEVMETHOD(device_attach,	pnphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t pnphy_devclass;

static driver_t pnphy_driver = {
	"pnphy",
	pnphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(pnphy, miibus, pnphy_driver, pnphy_devclass, 0, 0);

static int	pnphy_service(struct mii_softc *, struct mii_data *, int);
static void	pnphy_status(struct mii_softc *);

static int
pnphy_probe(device_t dev)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	/*
	 * The dc driver will report the 82c168 vendor and device
	 * ID to let us know that it wants us to attach.
	 */
	if (ma->mii_id1 != DC_VENDORID_LO ||
	    ma->mii_id2 != DC_DEVICEID_82C168)
		return(ENXIO);

	device_set_desc(dev, "PNIC 82c168 media interface");

	return (BUS_PROBE_DEFAULT);
}

static int
pnphy_attach(device_t dev)
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
	sc->mii_service = pnphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE;
	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	sc->mii_capabilities =
	    BMSR_100TXFDX|BMSR_100TXHDX|BMSR_10TFDX|BMSR_10THDX;
	sc->mii_capabilities &= ma->mii_capmask;
	device_printf(dev, " ");
	mii_add_media(sc);
	printf("\n");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);

#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int
pnphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	switch (cmd) {
	case MII_POLLSTAT:
		/*
		 * If we're not polling our PHY instance, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			return (0);
		}
		break;

	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		sc->mii_flags = 0;

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			/* NWAY is busted on this chip */
		case IFM_100_T4:
			/*
			 * XXX Not supported as a manual setting right now.
			 */
			return (EINVAL);
		case IFM_100_TX:
			mii->mii_media_active = IFM_ETHER|IFM_100_TX;
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX)
				mii->mii_media_active |= IFM_FDX;
			MIIBUS_STATCHG(sc->mii_dev);
			return(0);
		case IFM_10_T:
			mii->mii_media_active = IFM_ETHER|IFM_10_T;
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX)
				mii->mii_media_active |= IFM_FDX;
			MIIBUS_STATCHG(sc->mii_dev);
			return(0);
		default:
			return(EINVAL);
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

		break;
	}

	/* Update the media status. */
	pnphy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
pnphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int reg;
	struct dc_softc		*dc_sc;

	dc_sc = mii->mii_ifp->if_softc;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	reg = CSR_READ_4(dc_sc, DC_ISR);

	if (!(reg & DC_ISR_LINKFAIL))
		mii->mii_media_status |= IFM_ACTIVE;

	if (CSR_READ_4(dc_sc, DC_NETCFG) & DC_NETCFG_SPEEDSEL)
		mii->mii_media_active |= IFM_10_T;
	else
		mii->mii_media_active |= IFM_100_TX;
	if (CSR_READ_4(dc_sc, DC_NETCFG) & DC_NETCFG_FULLDUPLEX)
		mii->mii_media_active |= IFM_FDX;

	return;
}
