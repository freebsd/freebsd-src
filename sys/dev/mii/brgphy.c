/*
 * Copyright (c) 2000
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
 *
 * $FreeBSD$
 */

/*
 * Driver for the Broadcom BCR5400 1000baseTX PHY. Speed is always
 * 1000mbps; all we need to negotiate here is full or half duplex.
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

#include <dev/mii/brgphyreg.h>
#include <net/if_arp.h>
#include <machine/bus.h>
#include <dev/bge/if_bgereg.h>

#include "miibus_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

static int brgphy_probe(device_t);
static int brgphy_attach(device_t);
static int brgphy_detach(device_t);

static device_method_t brgphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		brgphy_probe),
	DEVMETHOD(device_attach,	brgphy_attach),
	DEVMETHOD(device_detach,	brgphy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t brgphy_devclass;

static driver_t brgphy_driver = {
	"brgphy",
	brgphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(brgphy, miibus, brgphy_driver, brgphy_devclass, 0, 0);

static int	brgphy_service(struct mii_softc *, struct mii_data *, int);
static void 	brgphy_status(struct mii_softc *);
static int	brgphy_mii_phy_auto(struct mii_softc *);
static void	brgphy_reset(struct mii_softc *);
static void	brgphy_loop(struct mii_softc *);
static void	bcm5401_load_dspcode(struct mii_softc *);
static void	bcm5411_load_dspcode(struct mii_softc *);
static void	bcm5703_load_dspcode(struct mii_softc *);
static int	brgphy_mii_model;

static int brgphy_probe(dev)
	device_t		dev;
{
	struct mii_attach_args *ma;

	ma = device_get_ivars(dev);

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5400) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5400);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5401) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5401);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5411) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5411);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5701) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5701);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5703) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5703);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5704) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5704);
		return(0);
	}

	if (MII_OUI(ma->mii_id1, ma->mii_id2) == MII_OUI_xxBROADCOM &&
	    MII_MODEL(ma->mii_id2) == MII_MODEL_xxBROADCOM_BCM5705) {
		device_set_desc(dev, MII_STR_xxBROADCOM_BCM5705);
		return(0);
	}

	return(ENXIO);
}

static int
brgphy_attach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";

	sc = device_get_softc(dev);
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = brgphy_service;
	sc->mii_pdata = mii;

	sc->mii_flags |= MIIF_NOISOLATE;
	mii->mii_instance++;

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)
#define PRINT(s)	printf("%s%s", sep, s); sep = ", "

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst),
	    BMCR_ISO);
#if 0
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_LOOP, sc->mii_inst),
	    BMCR_LOOP|BMCR_S100);
#endif

	brgphy_mii_model = MII_MODEL(ma->mii_id2);
	brgphy_reset(sc);

	sc->mii_capabilities =
	    PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	device_printf(dev, " ");
	if (sc->mii_capabilities & BMSR_MEDIAMASK)
		mii_add_media(mii, (sc->mii_capabilities & ~BMSR_ANEG),
		    sc->mii_inst);
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_TX, 0, sc->mii_inst),
	    BRGPHY_BMCR_FDX);
	PRINT(", 1000baseTX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_TX, IFM_FDX, sc->mii_inst), 0);
	PRINT("1000baseTX-FDX");
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	PRINT("auto");

	printf("\n");
#undef ADD
#undef PRINT

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return(0);
}

static int
brgphy_detach(dev)
	device_t		dev;
{
	struct mii_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(device_get_parent(dev));
	sc->mii_dev = NULL;
	LIST_REMOVE(sc, mii_list);

	return(0);
}

static int
brgphy_service(sc, mii, cmd)
	struct mii_softc *sc;
	struct mii_data *mii;
	int cmd;
{
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int reg, speed, gig;

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

		brgphy_reset(sc);	/* XXX hardware bug work-around */

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/*
			 * If we're already in auto mode, just return.
			 */
			if (PHY_READ(sc, BRGPHY_MII_BMCR) & BRGPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void) brgphy_mii_phy_auto(sc);
			break;
		case IFM_1000_TX:
			speed = BRGPHY_S1000;
			goto setit;
		case IFM_100_TX:
			speed = BRGPHY_S100;
			goto setit;
		case IFM_10_T:
			speed = BRGPHY_S10;
setit:
			brgphy_loop(sc);
			if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
				speed |= BRGPHY_BMCR_FDX;
				gig = BRGPHY_1000CTL_AFD;
			} else {
				gig = BRGPHY_1000CTL_AHD;
			}

			PHY_WRITE(sc, BRGPHY_MII_1000CTL, 0);
			PHY_WRITE(sc, BRGPHY_MII_BMCR, speed);
			PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);

			if (IFM_SUBTYPE(ife->ifm_media) != IFM_1000_TX)
				break;

			PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
			PHY_WRITE(sc, BRGPHY_MII_BMCR,
			    speed|BRGPHY_BMCR_AUTOEN|BRGPHY_BMCR_STARTNEG);

			if (brgphy_mii_model != MII_MODEL_xxBROADCOM_BCM5701)
				break;

			/*
			 * When settning the link manually, one side must
			 * be the master and the other the slave. However
			 * ifmedia doesn't give us a good way to specify
			 * this, so we fake it by using one of the LINK
			 * flags. If LINK0 is set, we program the PHY to
			 * be a master, otherwise it's a slave.
			 */
			if ((mii->mii_ifp->if_flags & IFF_LINK0)) {
				PHY_WRITE(sc, BRGPHY_MII_1000CTL,
				    gig|BRGPHY_1000CTL_MSE|BRGPHY_1000CTL_MSC);
			} else {
				PHY_WRITE(sc, BRGPHY_MII_1000CTL,
				    gig|BRGPHY_1000CTL_MSE);
			}
			break;
#ifdef foo
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO|BMCR_PDOWN);
			break;
#endif
		case IFM_100_T4:
		default:
			return (EINVAL);
		}
		break;

	case MII_TICK:
		/*
		 * If we're not currently selected, just return.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/*
		 * Only used for autonegotiation.
		 */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO)
			return (0);

		/*
		 * Is the interface even up?
		 */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.  Read
		 * the BMSR twice in case it's latched.
		 */
		reg = PHY_READ(sc, BRGPHY_MII_AUXSTS);
		if (reg & BRGPHY_AUXSTS_LINK)
			break;

		/*
		 * Only retry autonegotiation every 5 seconds.
		 */
		if (++sc->mii_ticks != 5)
			return (0);
		
		sc->mii_ticks = 0;
		brgphy_mii_phy_auto(sc);
		return (0);
	}

	/* Update the media status. */
	brgphy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke
	 * the DSP on the Broadcom PHYs if the media changes.
	 */
	if (sc->mii_active != mii->mii_media_active || cmd == MII_MEDIACHG) {
		MIIBUS_STATCHG(sc->mii_dev);
		sc->mii_active = mii->mii_media_active;
		switch (brgphy_mii_model) {
		case MII_MODEL_xxBROADCOM_BCM5401:
			bcm5401_load_dspcode(sc);
			break;
		case MII_MODEL_xxBROADCOM_BCM5411:
			bcm5411_load_dspcode(sc);
			break;
		}
	}
	return (0);
}

void
brgphy_status(sc)
	struct mii_softc *sc;
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmsr, bmcr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);
	if (PHY_READ(sc, BRGPHY_MII_AUXSTS) & BRGPHY_AUXSTS_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);

	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}

		switch (PHY_READ(sc, BRGPHY_MII_AUXSTS) &
		    BRGPHY_AUXSTS_AN_RES) {
		case BRGPHY_RES_1000FD:
			mii->mii_media_active |= IFM_1000_TX | IFM_FDX;
			break;
		case BRGPHY_RES_1000HD:
			mii->mii_media_active |= IFM_1000_TX | IFM_HDX;
			break;
		case BRGPHY_RES_100FD:
			mii->mii_media_active |= IFM_100_TX | IFM_FDX;
			break;
		case BRGPHY_RES_100T4:
			mii->mii_media_active |= IFM_100_T4;
			break;
		case BRGPHY_RES_100HD:
			mii->mii_media_active |= IFM_100_TX | IFM_HDX;
			break;
		case BRGPHY_RES_10FD:
			mii->mii_media_active |= IFM_10_T | IFM_FDX;
			break;
		case BRGPHY_RES_10HD:
			mii->mii_media_active |= IFM_10_T | IFM_HDX;
			break;
		default:
			mii->mii_media_active |= IFM_NONE;
			break;
		}
		return;
	}

	mii->mii_media_active = ife->ifm_media;

	return;
}


static int
brgphy_mii_phy_auto(mii)
	struct mii_softc *mii;
{
	int ktcr = 0;

	brgphy_loop(mii);
	brgphy_reset(mii);
	ktcr = BRGPHY_1000CTL_AFD|BRGPHY_1000CTL_AHD;
	if (brgphy_mii_model == MII_MODEL_xxBROADCOM_BCM5701)
		ktcr |= BRGPHY_1000CTL_MSE|BRGPHY_1000CTL_MSC;
	PHY_WRITE(mii, BRGPHY_MII_1000CTL, ktcr);
	ktcr = PHY_READ(mii, BRGPHY_MII_1000CTL);
	DELAY(1000);
	PHY_WRITE(mii, BRGPHY_MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(mii->mii_capabilities) | ANAR_CSMA);
	DELAY(1000);
	PHY_WRITE(mii, BRGPHY_MII_BMCR,
	    BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	PHY_WRITE(mii, BRGPHY_MII_IMR, 0xFF00);
	return (EJUSTRETURN);
}

static void
brgphy_loop(struct mii_softc *sc)
{
	u_int32_t bmsr;
	int i;

	PHY_WRITE(sc, BRGPHY_MII_BMCR, BRGPHY_BMCR_LOOP);
	for (i = 0; i < 15000; i++) {
		bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);
		if (!(bmsr & BRGPHY_BMSR_LINK)) {
#if 0
			device_printf(sc->mii_dev, "looped %d\n", i);
#endif
			break;
		}
		DELAY(10);
	}
}

/* Turn off tap power management on 5401. */
static void
bcm5401_load_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c20 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x0012 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x1804 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x0013 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x1204 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x8006 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0132 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x8006 },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0232 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x0a20 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
	DELAY(40);
}

static void
bcm5411_load_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ 0x1c,				0x8c23 },
		{ 0x1c,				0x8ca3 },
		{ 0x1c,				0x8c23 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
bcm5703_load_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x2aaa },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
bcm5704_load_dspcode(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		u_int16_t	val;
	} dspcode[] = {
		{ 0x1c,				0x8d68 },
		{ 0x1c,				0x8d68 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
brgphy_reset(struct mii_softc *sc)
{
	u_int32_t	val;
	struct ifnet	*ifp;
	struct bge_softc	*bge_sc;

	mii_phy_reset(sc);

	switch (brgphy_mii_model) {
	case MII_MODEL_xxBROADCOM_BCM5401:
		bcm5401_load_dspcode(sc);
		break;
	case MII_MODEL_xxBROADCOM_BCM5411:
		bcm5411_load_dspcode(sc);
		break;
	case MII_MODEL_xxBROADCOM_BCM5703:
		bcm5703_load_dspcode(sc);
		break;
	case MII_MODEL_xxBROADCOM_BCM5704:
		bcm5704_load_dspcode(sc);
		break;
	}

	ifp = sc->mii_pdata->mii_ifp;
	bge_sc = ifp->if_softc;

	/*
	 * Don't enable Ethernet@WireSpeed for the 5700 or the
	 * 5705 A1 and A2 chips. Make sure we only do this test
	 * on "bge" NICs, since other drivers may use this same
	 * PHY subdriver.
	 */
	if (strcmp(ifp->if_name, "bge") == 0 &&
	    (bge_sc->bge_asicrev == BGE_ASICREV_BCM5700 ||
	    bge_sc->bge_chipid == BGE_CHIPID_BCM5705_A1 ||
	    bge_sc->bge_chipid == BGE_CHIPID_BCM5705_A2))
		return;

	/* Enable Ethernet@WireSpeed. */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7007);
	val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, val | (1 << 15) || (1 << 4));
}
