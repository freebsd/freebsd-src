/* $FreeBSD$ */
/*
 * Principal Author: Parag Patel
 * Copyright (c) 2001
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
 * Additonal Copyright (c) 2001 by Traakan Software under same licence.
 * Secondary Author: Matthew Jacob
 */

/*
 * driver for the Marvell 88E1000 series external 1000/100/10-BT PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <machine/clock.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>

#include <dev/mii/e1000phyreg.h>

#include "miibus_if.h"

static int e1000phy_probe(device_t);
static int e1000phy_attach(device_t);

static device_method_t e1000phy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		e1000phy_probe),
	DEVMETHOD(device_attach,	e1000phy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t e1000phy_devclass;
static driver_t e1000phy_driver = {
	"e1000phy", e1000phy_methods, sizeof (struct mii_softc)
};
DRIVER_MODULE(e1000phy, miibus, e1000phy_driver, e1000phy_devclass, 0, 0);

static int	e1000phy_service(struct mii_softc *, struct mii_data *, int);
static void	e1000phy_status(struct mii_softc *);
static void	e1000phy_reset(struct mii_softc *);
static int	e1000phy_mii_phy_auto(struct mii_softc *);

static int e1000phy_debug = 0;

static int
e1000phy_probe(device_t	dev)
{
	struct mii_attach_args *ma;
	u_int32_t id;

	ma = device_get_ivars(dev);
	id = ((ma->mii_id1 << 16) | ma->mii_id2) & E1000_ID_MASK;

	if (id != E1000_ID_88E1000 && id != E1000_ID_88E1000S) {
		return ENXIO;
	}

	device_set_desc(dev, MII_STR_MARVELL_E1000);
	return 0;
}

static int
e1000phy_attach(device_t dev)
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;

	getenv_int("e1000phy_debug", &e1000phy_debug);

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = e1000phy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE;
	mii->mii_instance++;
	e1000phy_reset(sc);

	device_printf(dev, " ");

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
/*
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    E1000_CR_ISOLATE);
*/
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
	    E1000_CR_SPEED_10);
	printf("10baseT, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
	    E1000_CR_SPEED_10 | E1000_CR_FULL_DUPLEX);
	printf("10baseT-FDX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
	    E1000_CR_SPEED_100);
	printf("100baseTX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
	    E1000_CR_SPEED_100 | E1000_CR_FULL_DUPLEX);
	printf("100baseTX-FDX, ");
	/*
	 * 1000BT-simplex not supported; driver must ignore this entry,
	 * but it must be present in order to manually set full-duplex.
	 */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0, sc->mii_inst),
	    E1000_CR_SPEED_1000);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX, sc->mii_inst),
	    E1000_CR_SPEED_1000 | E1000_CR_FULL_DUPLEX);
	printf("1000baseTX-FDX, ");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	printf("auto\n");
#undef ADD

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static void
e1000phy_reset(struct mii_softc *sc)
{
	u_int32_t reg;
	int i;

	/* initialize custom E1000 registers to magic values */
	reg = PHY_READ(sc, E1000_SCR);
	reg &= ~E1000_SCR_AUTO_X_MODE;
	PHY_WRITE(sc, E1000_SCR, reg);

	/* normal PHY reset */
	/*mii_phy_reset(sc);*/
	reg = PHY_READ(sc, E1000_CR);
	reg |= E1000_CR_RESET;
	PHY_WRITE(sc, E1000_CR, reg);

	for (i = 0; i < 500; i++) {
		DELAY(1);
		reg = PHY_READ(sc, E1000_CR);
		if (!(reg & E1000_CR_RESET))
			break;
	}

	/* set more custom E1000 registers to magic values */
	reg = PHY_READ(sc, E1000_SCR);
	reg |= E1000_SCR_ASSERT_CRS_ON_TX;
	PHY_WRITE(sc, E1000_SCR, reg);

	reg = PHY_READ(sc, E1000_ESCR);
	reg |= E1000_ESCR_TX_CLK_25;
	PHY_WRITE(sc, E1000_ESCR, reg);

	/* even more magic to reset DSP? */
	PHY_WRITE(sc, 29, 0x1d);
	PHY_WRITE(sc, 30, 0xc1);
	PHY_WRITE(sc, 30, 0x00);
}

static int
e1000phy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
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
			reg = PHY_READ(sc, E1000_CR);
			PHY_WRITE(sc, E1000_CR, reg | E1000_CR_ISOLATE);
			return (0);
		}

		/*
		 * If the interface is not up, don't do anything.
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0) {
			break;
		}

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			e1000phy_reset(sc);
			(void)e1000phy_mii_phy_auto(sc);
			break;

		case IFM_1000_T:
			e1000phy_reset(sc);

			/* TODO - any other way to force 1000BT? */
			(void)e1000phy_mii_phy_auto(sc);
			break;

		case IFM_100_TX:
			e1000phy_reset(sc);

			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, E1000_CR,
				    E1000_CR_FULL_DUPLEX | E1000_CR_SPEED_100);
				PHY_WRITE(sc, E1000_AR, E1000_AR_100TX_FD);
			} else {
				PHY_WRITE(sc, E1000_CR, E1000_CR_SPEED_100);
				PHY_WRITE(sc, E1000_AR, E1000_AR_100TX);
			}
			break;

		case IFM_10_T:
			e1000phy_reset(sc);

			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				PHY_WRITE(sc, E1000_CR,
				    E1000_CR_FULL_DUPLEX | E1000_CR_SPEED_10);
				PHY_WRITE(sc, E1000_AR, E1000_AR_10T_FD);
			} else {
				PHY_WRITE(sc, E1000_CR, E1000_CR_SPEED_10);
				PHY_WRITE(sc, E1000_AR, E1000_AR_10T);
			}

			break;

		default:
			return (EINVAL);
		}

		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			return (0);
		}

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
		 * check for link.
		 * Read the status register twice; BMSR_LINK is latch-low.
		 */
		reg = PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (reg & BMSR_LINK)
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);

		sc->mii_ticks = 0;
		e1000phy_reset(sc);
		e1000phy_mii_phy_auto(sc);
		return (0);
	}

	/* Update the media status. */
	e1000phy_status(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}

static void
e1000phy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	int bmsr, bmcr, esr, ssr, isr, ar, lpar;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, E1000_SR) | PHY_READ(sc, E1000_SR);
	esr = PHY_READ(sc, E1000_ESR);
	bmcr = PHY_READ(sc, E1000_CR);
	ssr = PHY_READ(sc, E1000_SSR);
	isr = PHY_READ(sc, E1000_ISR);
	ar = PHY_READ(sc, E1000_AR);
	lpar = PHY_READ(sc, E1000_LPAR);

	if (bmsr & E1000_SR_LINK_STATUS)
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & E1000_CR_LOOPBACK)
		mii->mii_media_active |= IFM_LOOP;

	if ((!(bmsr & E1000_SR_AUTO_NEG_COMPLETE) || !(ssr & E1000_SSR_LINK) ||
	    !(ssr & E1000_SSR_SPD_DPLX_RESOLVED))) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		return;
	}

	if (ssr & E1000_SSR_1000MBS)
		mii->mii_media_active |= IFM_1000_T;
	else if (ssr & E1000_SSR_100MBS)
		mii->mii_media_active |= IFM_100_TX;
	else
		mii->mii_media_active |= IFM_10_T;

	if (ssr & E1000_SSR_DUPLEX)
		mii->mii_media_active |= IFM_FDX;
	else
		mii->mii_media_active |= IFM_HDX;

	/* FLAG0==rx-flow-control FLAG1==tx-flow-control */
	if ((ar & E1000_AR_PAUSE) && (lpar & E1000_LPAR_PAUSE)) {
		mii->mii_media_active |= IFM_FLAG0 | IFM_FLAG1;
	} else if (!(ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
	    (lpar & E1000_LPAR_PAUSE) && (lpar & E1000_LPAR_ASM_DIR)) {
		mii->mii_media_active |= IFM_FLAG1;
	} else if ((ar & E1000_AR_PAUSE) && (ar & E1000_AR_ASM_DIR) &&
	    !(lpar & E1000_LPAR_PAUSE) && (lpar & E1000_LPAR_ASM_DIR)) {
		mii->mii_media_active |= IFM_FLAG0;
	}
}

static int
e1000phy_mii_phy_auto(struct mii_softc *mii)
{

	PHY_WRITE(mii, E1000_AR, E1000_AR_10T | E1000_AR_10T_FD |
	    E1000_AR_100TX | E1000_AR_100TX_FD | 
	    E1000_AR_PAUSE | E1000_AR_ASM_DIR);
	PHY_WRITE(mii, E1000_1GCR, E1000_1GCR_1000T_FD);
	PHY_WRITE(mii, E1000_CR,
	    E1000_CR_AUTO_NEG_ENABLE | E1000_CR_RESTART_AUTO_NEG);

	return (EJUSTRETURN);
}
