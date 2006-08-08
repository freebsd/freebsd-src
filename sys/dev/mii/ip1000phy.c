/*-
 * Copyright (c) 2006, Pyun YongHyeon <yongari@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * Driver for the IC Plus IP1000A 10/100/1000 PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>


#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/ip1000phyreg.h>

#include "miibus_if.h"

#include <machine/bus.h>
#include <dev/stge/if_stgereg.h>

static int ip1000phy_probe(device_t);
static int ip1000phy_attach(device_t);

static device_method_t ip1000phy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ip1000phy_probe),
	DEVMETHOD(device_attach,	ip1000phy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t ip1000phy_devclass;
static driver_t ip1000phy_driver = {
	"ip1000phy",
	ip1000phy_methods,
	sizeof (struct mii_softc)
};

DRIVER_MODULE(ip1000phy, miibus, ip1000phy_driver, ip1000phy_devclass, 0, 0);

static int	ip1000phy_service(struct mii_softc *, struct mii_data *, int);
static void	ip1000phy_status(struct mii_softc *);
static void	ip1000phy_reset(struct mii_softc *);
static int	ip1000phy_mii_phy_auto(struct mii_softc *);

static int
ip1000phy_probe(device_t dev)
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_ICPLUS &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_ICPLUS_IP1000A) {
		device_set_desc(dev, MII_STR_ICPLUS_IP1000A);
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ip1000phy_attach(device_t dev)
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
	sc->mii_service = ip1000phy_service;
	sc->mii_pdata = mii;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;
	sc->mii_flags |= MIIF_NOISOLATE;

	mii->mii_instance++;

	device_printf(dev, " ");

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
	    IP1000PHY_BMCR_10);
	printf("10baseT, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
	    IP1000PHY_BMCR_10 | IP1000PHY_BMCR_FDX);
	printf("10baseT-FDX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
	    IP1000PHY_BMCR_100);
	printf("100baseTX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
	    IP1000PHY_BMCR_100 | IP1000PHY_BMCR_FDX);
	printf("100baseTX-FDX, ");
	/* 1000baseT half-duplex, really supported? */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0, sc->mii_inst),
	    IP1000PHY_BMCR_1000);
	printf("1000baseTX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX, sc->mii_inst),
	    IP1000PHY_BMCR_1000 | IP1000PHY_BMCR_FDX);
	printf("1000baseTX-FDX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	printf("auto\n");
#undef ADD

	ip1000phy_reset(sc);

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int
ip1000phy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	uint32_t gig, reg, speed;

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
			reg = PHY_READ(sc, IP1000PHY_MII_BMCR);
			PHY_WRITE(sc, IP1000PHY_MII_BMCR,
			    reg | IP1000PHY_BMCR_ISO);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0) {
			break;
		}

		ip1000phy_reset(sc);
		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			(void)ip1000phy_mii_phy_auto(sc);
			goto done;
			break;

		case IFM_1000_T:
			/*
			 * XXX
			 * Manual 1000baseT setting doesn't seem to work.
			 */
			speed = IP1000PHY_BMCR_1000;
			break;

		case IFM_100_TX:
			speed = IP1000PHY_BMCR_100;
			break;

		case IFM_10_T:
			speed = IP1000PHY_BMCR_10;
			break;

		default:
			return (EINVAL);
		}

		if (((ife->ifm_media & IFM_GMASK) & IFM_FDX) != 0) {
			speed |= IP1000PHY_BMCR_FDX;
			gig = IP1000PHY_1000CR_1000T_FDX;
		} else
			gig = IP1000PHY_1000CR_1000T;

		PHY_WRITE(sc, IP1000PHY_MII_1000CR, 0);
		PHY_WRITE(sc, IP1000PHY_MII_BMCR, speed);

		if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_T)
			break;

		PHY_WRITE(sc, IP1000PHY_MII_1000CR, gig);
		PHY_WRITE(sc, IP1000PHY_MII_BMCR, speed);

		/*
		 * When settning the link manually, one side must
		 * be the master and the other the slave. However
		 * ifmedia doesn't give us a good way to specify
		 * this, so we fake it by using one of the LINK
		 * flags. If LINK0 is set, we program the PHY to
		 * be a master, otherwise it's a slave.
		 */
		if ((mii->mii_ifp->if_flags & IFF_LINK0))
			PHY_WRITE(sc, IP1000PHY_MII_1000CR, gig |
			    IP1000PHY_1000CR_MASTER |
			    IP1000PHY_1000CR_MMASTER |
			    IP1000PHY_1000CR_MANUAL);
		else
			PHY_WRITE(sc, IP1000PHY_MII_1000CR, gig |
			    IP1000PHY_1000CR_MASTER |
			    IP1000PHY_1000CR_MANUAL);

done:
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
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		/*
		 * check for link.
		 */
		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK) {
			sc->mii_ticks = 0;
			break;
		}

		/* Announce link loss right after it happens */
		if (sc->mii_ticks++ == 0)
			break;

		/*
		 * Only retry autonegotiation every mii_anegticks seconds.
		 */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		ip1000phy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	ip1000phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
ip1000phy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	uint32_t bmsr, bmcr, stat;
	uint32_t ar, lpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, IP1000PHY_MII_BMSR) |
	    PHY_READ(sc, IP1000PHY_MII_BMSR);
	if ((bmsr & IP1000PHY_BMSR_LINK) != 0)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, IP1000PHY_MII_BMCR);
	if ((bmcr & IP1000PHY_BMCR_LOOP) != 0)
		mii->mii_media_active |= IFM_LOOP;

	if ((bmcr & IP1000PHY_BMCR_AUTOEN) != 0) {
		if ((bmsr & IP1000PHY_BMSR_ANEGCOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
                }
        }

	stat = PHY_READ(sc, STGE_PhyCtrl);
	switch (PC_LinkSpeed(stat)) {
	case PC_LinkSpeed_Down:
		mii->mii_media_active |= IFM_NONE;
		return;
	case PC_LinkSpeed_10:
		mii->mii_media_active |= IFM_10_T;
		break;
	case PC_LinkSpeed_100:
		mii->mii_media_active |= IFM_100_TX;
		break;
	case PC_LinkSpeed_1000:
		mii->mii_media_active |= IFM_1000_T;
		break;
	}
	if ((stat & PC_PhyDuplexStatus) != 0)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	ar = PHY_READ(sc, IP1000PHY_MII_ANAR);
	lpar = PHY_READ(sc, IP1000PHY_MII_ANLPAR);

	/*
	 * FLAG0 : Rx flow-control
	 * FLAG1 : Tx flow-control
	 */
	if ((ar & IP1000PHY_ANAR_PAUSE) && (lpar & IP1000PHY_ANLPAR_PAUSE))
		mii->mii_media_active |= IFM_FLAG0 | IFM_FLAG1;
	else if (!(ar & IP1000PHY_ANAR_PAUSE) && (ar & IP1000PHY_ANAR_APAUSE) &&
	    (lpar & IP1000PHY_ANLPAR_PAUSE) && (lpar & IP1000PHY_ANLPAR_APAUSE))
		mii->mii_media_active |= IFM_FLAG1;
	else if ((ar & IP1000PHY_ANAR_PAUSE) && (ar & IP1000PHY_ANAR_APAUSE) &&
	    !(lpar & IP1000PHY_ANLPAR_PAUSE) &&
	    (lpar & IP1000PHY_ANLPAR_APAUSE)) {
		mii->mii_media_active |= IFM_FLAG0;
	}

	/*
	 * FLAG2 : local PHY resolved to MASTER
	 */
	if ((mii->mii_media_active & IFM_1000_T) != 0) {
		stat = PHY_READ(sc, IP1000PHY_MII_1000SR);
		if ((stat & IP1000PHY_1000SR_MASTER) != 0)
			mii->mii_media_active |= IFM_FLAG2;
	}
}

static int
ip1000phy_mii_phy_auto(struct mii_softc *mii)
{
	uint32_t reg;

	PHY_WRITE(mii, IP1000PHY_MII_ANAR,
	    IP1000PHY_ANAR_10T | IP1000PHY_ANAR_10T_FDX |
	    IP1000PHY_ANAR_100TX | IP1000PHY_ANAR_100TX_FDX |
	    IP1000PHY_ANAR_PAUSE | IP1000PHY_ANAR_APAUSE);
	reg = IP1000PHY_1000CR_1000T | IP1000PHY_1000CR_1000T_FDX;
	reg |= IP1000PHY_1000CR_MASTER;
	PHY_WRITE(mii, IP1000PHY_MII_1000CR, reg);
	PHY_WRITE(mii, IP1000PHY_MII_BMCR, (IP1000PHY_BMCR_FDX |
	    IP1000PHY_BMCR_AUTOEN | IP1000PHY_BMCR_STARTNEG));

	return (EJUSTRETURN);
}

static void
ip1000phy_load_dspcode(struct mii_softc *sc)
{

	PHY_WRITE(sc, 31, 0x0001);
	PHY_WRITE(sc, 27, 0x01e0);
	PHY_WRITE(sc, 31, 0x0002);
	PHY_WRITE(sc, 27, 0xeb8e);
	PHY_WRITE(sc, 31, 0x0000);
	PHY_WRITE(sc, 30, 0x005e);
	PHY_WRITE(sc, 9, 0x0700);

	DELAY(50);
}

static void
ip1000phy_reset(struct mii_softc *sc)
{
	struct stge_softc *stge_sc;
	struct mii_data *mii;
	uint32_t reg;

	mii_phy_reset(sc);

	/* clear autoneg/full-duplex as we don't want it after reset */
	reg = PHY_READ(sc, IP1000PHY_MII_BMCR);
	reg &= ~(IP1000PHY_BMCR_AUTOEN | IP1000PHY_BMCR_FDX);
	PHY_WRITE(sc, MII_BMCR, reg);

	mii = sc->mii_pdata;
	/*
	 * XXX There should be more general way to pass PHY specific
	 * data via mii interface.
	 */
	if (strcmp(mii->mii_ifp->if_dname, "stge") == 0) {
		stge_sc = mii->mii_ifp->if_softc;
		if (stge_sc->sc_rev >= 0x40 && stge_sc->sc_rev <= 0x4e)
			ip1000phy_load_dspcode(sc);
	}
}
