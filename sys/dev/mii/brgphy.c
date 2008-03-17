/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for the Broadcom BCM54xx/57xx 1000baseTX PHY.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/bus.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include <dev/mii/brgphyreg.h>
#include <net/if_arp.h>
#include <machine/bus.h>
#include <dev/bge/if_bgereg.h>
#include <dev/bce/if_bcereg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "miibus_if.h"

static int brgphy_probe(device_t);
static int brgphy_attach(device_t);

struct brgphy_softc {
	struct mii_softc mii_sc;
	int mii_oui;
	int mii_model;
	int mii_rev;
	int serdes_flags;	/* Keeps track of the serdes type used */
#define BRGPHY_5706S	0x0001
#define BRGPHY_5708S	0x0002
	int bce_phy_flags;		/* PHY flags transferred from the MAC driver */
};

static device_method_t brgphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		brgphy_probe),
	DEVMETHOD(device_attach,	brgphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{ 0, 0 }
};

static devclass_t brgphy_devclass;

static driver_t brgphy_driver = {
	"brgphy",
	brgphy_methods,
	sizeof(struct brgphy_softc)
};

DRIVER_MODULE(brgphy, miibus, brgphy_driver, brgphy_devclass, 0, 0);

static int	brgphy_service(struct mii_softc *, struct mii_data *, int);
static void	brgphy_setmedia(struct mii_softc *, int, int);
static void	brgphy_status(struct mii_softc *);
static void	brgphy_mii_phy_auto(struct mii_softc *);
static void	brgphy_reset(struct mii_softc *);
static void	brgphy_enable_loopback(struct mii_softc *);
static void	bcm5401_load_dspcode(struct mii_softc *);
static void	bcm5411_load_dspcode(struct mii_softc *);
static void	brgphy_fixup_5704_a0_bug(struct mii_softc *);
static void	brgphy_fixup_adc_bug(struct mii_softc *);
static void	brgphy_fixup_adjust_trim(struct mii_softc *);
static void	brgphy_fixup_ber_bug(struct mii_softc *);
static void	brgphy_fixup_crc_bug(struct mii_softc *);
static void	brgphy_fixup_jitter_bug(struct mii_softc *);
static void	brgphy_ethernet_wirespeed(struct mii_softc *);
static void	brgphy_jumbo_settings(struct mii_softc *, u_long);

static const struct mii_phydesc brgphys[] = {
	MII_PHY_DESC(xxBROADCOM, BCM5400),
	MII_PHY_DESC(xxBROADCOM, BCM5401),
	MII_PHY_DESC(xxBROADCOM, BCM5411),
	MII_PHY_DESC(xxBROADCOM, BCM5701),
	MII_PHY_DESC(xxBROADCOM, BCM5703),
	MII_PHY_DESC(xxBROADCOM, BCM5704),
	MII_PHY_DESC(xxBROADCOM, BCM5705),
	MII_PHY_DESC(xxBROADCOM, BCM5706),
	MII_PHY_DESC(xxBROADCOM, BCM5714),
	MII_PHY_DESC(xxBROADCOM, BCM5750),
	MII_PHY_DESC(xxBROADCOM, BCM5752),
	MII_PHY_DESC(xxBROADCOM, BCM5754),
	MII_PHY_DESC(xxBROADCOM, BCM5780),
	MII_PHY_DESC(xxBROADCOM, BCM5708C),
	MII_PHY_DESC(xxBROADCOM_ALT1, BCM5755),
	MII_PHY_DESC(xxBROADCOM_ALT1, BCM5787),
	MII_PHY_DESC(xxBROADCOM_ALT1, BCM5708S),
	MII_PHY_DESC(xxBROADCOM_ALT1, BCM5722),
	MII_PHY_END
};


/* Search for our PHY in the list of known PHYs */
static int
brgphy_probe(device_t dev)
{
	return (mii_phy_dev_probe(dev, brgphys, BUS_PROBE_DEFAULT));
}

/* Attach the PHY to the MII bus */
static int
brgphy_attach(device_t dev)
{
	struct brgphy_softc *bsc;
	struct bge_softc *bge_sc = NULL;
	struct bce_softc *bce_sc = NULL;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	struct ifnet *ifp;
	int fast_ether;

	bsc = device_get_softc(dev);
	sc = &bsc->mii_sc;
	ma = device_get_ivars(dev);
	sc->mii_dev = device_get_parent(dev);
	mii = device_get_softc(sc->mii_dev);
	LIST_INSERT_HEAD(&mii->mii_phys, sc, mii_list);

	/* Initialize mii_softc structure */
	sc->mii_inst = mii->mii_instance;
	sc->mii_phy = ma->mii_phyno;
	sc->mii_service = brgphy_service;
	sc->mii_pdata = mii;
	sc->mii_anegticks = MII_ANEGTICKS_GIGE;
	sc->mii_flags |= MIIF_NOISOLATE | MIIF_NOLOOP;
	mii->mii_instance++;

	/* Initialize brgphy_softc structure */
	bsc->mii_oui = MII_OUI(ma->mii_id1, ma->mii_id2);
	bsc->mii_model = MII_MODEL(ma->mii_id2);
	bsc->mii_rev = MII_REV(ma->mii_id2);
	bsc->serdes_flags = 0;

	fast_ether = 0;

	if (bootverbose)
		device_printf(dev, "OUI 0x%06x, model 0x%04x, rev. %d\n",
		    bsc->mii_oui, bsc->mii_model, bsc->mii_rev);

	/* Handle any special cases based on the PHY ID */
	switch (bsc->mii_oui) {
	case MII_OUI_BROADCOM: 
		break;
	case MII_OUI_xxBROADCOM:
		switch (bsc->mii_model) {
			case MII_MODEL_xxBROADCOM_BCM5706:
				/* 
				 * The 5464 PHY used in the 5706 supports both copper
				 * and fiber interfaces over GMII.  Need to check the
				 * shadow registers to see which mode is actually 
				 * in effect, and therefore whether we have 5706C or 
				 * 5706S.
				 */
				PHY_WRITE(sc, BRGPHY_MII_SHADOW_1C, 
					BRGPHY_SHADOW_1C_MODE_CTRL);
				if (PHY_READ(sc, BRGPHY_MII_SHADOW_1C) & 
					BRGPHY_SHADOW_1C_ENA_1000X) {
					bsc->serdes_flags |= BRGPHY_5706S;
					sc->mii_flags |= MIIF_HAVEFIBER;
				}
				break;
		} break;
	case MII_OUI_xxBROADCOM_ALT1:
		switch (bsc->mii_model) {
			case MII_MODEL_xxBROADCOM_ALT1_BCM5708S:
				bsc->serdes_flags |= BRGPHY_5708S;
				sc->mii_flags |= MIIF_HAVEFIBER;
				break;
		} break;
	default:
		device_printf(dev, "Unrecognized OUI for PHY!\n");
	}

	ifp = sc->mii_pdata->mii_ifp;

	/* Find the MAC driver associated with this PHY. */
	if (strcmp(ifp->if_dname, "bge") == 0)	{
		bge_sc = ifp->if_softc;
	} else if (strcmp(ifp->if_dname, "bce") == 0) {
		bce_sc = ifp->if_softc;
	}

	/* Todo: Need to add additional controllers such as 5906 & 5787F */
	/* The 590x chips are 10/100 only. */
	if (bge_sc &&
	    pci_get_vendor(bge_sc->bge_dev) == BCOM_VENDORID &&
	    (pci_get_device(bge_sc->bge_dev) == BCOM_DEVICEID_BCM5901 ||
	    pci_get_device(bge_sc->bge_dev) == BCOM_DEVICEID_BCM5901A2)) {
		fast_ether = 1;
		sc->mii_anegticks = MII_ANEGTICKS;
	}

	brgphy_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	if (sc->mii_capabilities & BMSR_EXTSTAT)
		sc->mii_extcapabilities = PHY_READ(sc, MII_EXTSR);
	device_printf(dev, " ");

#define	ADD(m, c)	ifmedia_add(&mii->mii_media, (m), (c), NULL)

	/* Create an instance of Ethernet media. */
	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_NONE, 0, sc->mii_inst), BMCR_ISO);

	/* Add the supported media types */
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, 0, sc->mii_inst),
			BRGPHY_S10);
		printf("10baseT, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_10_T, IFM_FDX, sc->mii_inst),
			BRGPHY_S10 | BRGPHY_BMCR_FDX);
		printf("10baseT-FDX, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, 0, sc->mii_inst),
			BRGPHY_S100);
		printf("100baseTX, ");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_100_TX, IFM_FDX, sc->mii_inst),
			BRGPHY_S100 | BRGPHY_BMCR_FDX);
		printf("100baseTX-FDX, ");
		if (fast_ether == 0) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0, sc->mii_inst),
				BRGPHY_S1000);
			printf("1000baseT, ");
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, IFM_FDX, sc->mii_inst),
				BRGPHY_S1000 | BRGPHY_BMCR_FDX);
			printf("1000baseT-FDX, ");
		}
	} else {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_SX, IFM_FDX, sc->mii_inst),
			BRGPHY_S1000 | BRGPHY_BMCR_FDX);
		printf("1000baseSX-FDX, ");
		/* 2.5G support is a software enabled feature on the 5708S */
		if (bce_sc && (bce_sc->bce_phy_flags & BCE_PHY_2_5G_CAPABLE_FLAG)) {
			ADD(IFM_MAKEWORD(IFM_ETHER, IFM_2500_SX, IFM_FDX, sc->mii_inst), 0);
			printf("2500baseSX-FDX, ");
		}
	}

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	printf("auto\n");

#undef ADD
	MIIBUS_MEDIAINIT(sc->mii_dev);

	/* If MFW running on bce, bring the PHY up now. */
	if (bce_sc && (bce_sc->bce_flags & BCE_MFW_ENABLE_FLAG)) {
		brgphy_mii_phy_auto(sc);
		brgphy_status(sc);
	}

	return (0);
}

static int
brgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int error = 0;
	int val;

	switch (cmd) {
	case MII_POLLSTAT:
		/* If we're not polling our PHY instance, just return. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			goto brgphy_service_exit;
		break;
	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			PHY_WRITE(sc, MII_BMCR,
			    PHY_READ(sc, MII_BMCR) | BMCR_ISO);
			goto brgphy_service_exit;
		}

		/* If the interface is not up, don't do anything. */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		/* Todo: Why is this here?  Is it really needed? */
		brgphy_reset(sc);	/* XXX hardware bug work-around */

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
			brgphy_mii_phy_auto(sc);
			break;
		case IFM_2500_SX:
		case IFM_1000_SX:
		case IFM_1000_T:
		case IFM_100_TX:
		case IFM_10_T:
			brgphy_setmedia(sc, ife->ifm_media,
			    mii->mii_ifp->if_flags & IFF_LINK0);
			break;
		default:
			error = EINVAL;
			goto brgphy_service_exit;
		}
		break;
	case MII_TICK:
		/* Bail if we're not currently selected. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			goto brgphy_service_exit;

		/* Bail if the interface isn't up. */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			goto brgphy_service_exit;


		/* Bail if autoneg isn't in process. */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;
			break;
		}

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.
		 */
		val	= PHY_READ(sc, MII_BMSR) | PHY_READ(sc, MII_BMSR);
		if (val & BMSR_LINK) {
			sc->mii_ticks = 0;	/* Reset autoneg timer. */
			break;
		}

#if 0
		/* Todo: Is this correct? */
		/* Announce link loss right after it happens. */
		if (sc->mii_ticks++ == 0)
			break;
#endif

		/* Only retry autonegotiation every mii_anegticks seconds. */
		if (sc->mii_ticks <= sc->mii_anegticks)
			goto brgphy_service_exit;


		/* Retry autonegotiation */
		sc->mii_ticks = 0;
		brgphy_mii_phy_auto(sc);
		break;
	}

	/* Update the media status. */
	brgphy_status(sc);

	/*
	 * Callback if something changed. Note that we need to poke
	 * the DSP on the Broadcom PHYs if the media changes.
	 */
	if (sc->mii_media_active != mii->mii_media_active ||
	    sc->mii_media_status != mii->mii_media_status ||
	    cmd == MII_MEDIACHG) {
		switch (bsc->mii_oui) {
		case MII_OUI_BROADCOM:
			break;
		case MII_OUI_xxBROADCOM:
			switch (bsc->mii_model) {
			case MII_MODEL_xxBROADCOM_BCM5400:
				bcm5401_load_dspcode(sc);
				break;
			case MII_MODEL_xxBROADCOM_BCM5401:
				if (bsc->mii_rev == 1 || bsc->mii_rev == 3)
					bcm5401_load_dspcode(sc);
				break;
			case MII_MODEL_xxBROADCOM_BCM5411:
				bcm5411_load_dspcode(sc);
				break;
			}
			break;
		case MII_OUI_xxBROADCOM_ALT1:
			break;
		}
	}
	mii_phy_update(sc, cmd);
brgphy_service_exit:
	return (error);
}

static void
brgphy_setmedia(struct mii_softc *sc, int media, int master)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	int bmcr = 0, gig;

	switch (IFM_SUBTYPE(media)) {
	case IFM_2500_SX:
		break;
	case IFM_1000_SX:
	case IFM_1000_T:
		bmcr = BRGPHY_S1000;
		break;
	case IFM_100_TX:
		bmcr = BRGPHY_S100;
		break;
	case IFM_10_T:
	default:
		bmcr = BRGPHY_S10;
		break;
	}
	if ((media & IFM_GMASK) == IFM_FDX) {
		bmcr |= BRGPHY_BMCR_FDX;
		gig = BRGPHY_1000CTL_AFD;
	} else {
		gig = BRGPHY_1000CTL_AHD;
	}

	brgphy_enable_loopback(sc);
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, 0);
	PHY_WRITE(sc, BRGPHY_MII_BMCR, bmcr);
	PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);

	if ((IFM_SUBTYPE(media) != IFM_1000_T) && (IFM_SUBTYPE(media) != IFM_1000_SX))
		goto brgphy_setmedia_exit;

	PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
	PHY_WRITE(sc, BRGPHY_MII_BMCR,
	    bmcr | BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);

	if (bsc->mii_model != MII_MODEL_xxBROADCOM_BCM5701)
		goto brgphy_setmedia_exit;

	/*
	 * When setting the link manually, one side must be the master and
	 * the other the slave. However ifmedia doesn't give us a good way
	 * to specify this, so we fake it by using one of the LINK flags.
	 * If LINK0 is set, we program the PHY to be a master, otherwise
	 * it's a slave.
	 */
	if (master) {
		PHY_WRITE(sc, BRGPHY_MII_1000CTL,
		    gig | BRGPHY_1000CTL_MSE | BRGPHY_1000CTL_MSC);
	} else {
		PHY_WRITE(sc, BRGPHY_MII_1000CTL,
		    gig | BRGPHY_1000CTL_MSE);
	}
brgphy_setmedia_exit:
	return;
}

/* Set the media status based on the PHY settings. */
/* IFM_FLAG0 = 0 (RX flow control disabled | 1 (enabled) */
/* IFM_FLAG1 = 0 (TX flow control disabled | 1 (enabled) */
static void
brgphy_status(struct mii_softc *sc)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	struct mii_data *mii = sc->mii_pdata;
	int aux, bmcr, bmsr, anar, anlpar, xstat, val;


	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR) | PHY_READ(sc, BRGPHY_MII_BMSR);
	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);
	anar = PHY_READ(sc, BRGPHY_MII_ANAR);
	anlpar = PHY_READ(sc, BRGPHY_MII_ANLPAR);

	/* Loopback is enabled. */
	if (bmcr & BRGPHY_BMCR_LOOP) {

		mii->mii_media_active |= IFM_LOOP;
	}

	/* Autoneg is still in progress. */
	if ((bmcr & BRGPHY_BMCR_AUTOEN) &&
	    (bmsr & BRGPHY_BMSR_ACOMP) == 0) {
		/* Erg, still trying, I guess... */
		mii->mii_media_active |= IFM_NONE;
		goto brgphy_status_exit;
	}

	/* Autoneg is enabled and complete, link should be up. */
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		aux = PHY_READ(sc, BRGPHY_MII_AUXSTS);

		/* If copper link is up, get the negotiated speed/duplex. */
		if (aux & BRGPHY_AUXSTS_LINK) {
			mii->mii_media_status |= IFM_ACTIVE;
			switch (aux & BRGPHY_AUXSTS_AN_RES) {
			case BRGPHY_RES_1000FD:
				mii->mii_media_active |= IFM_1000_T | IFM_FDX; 	break;
			case BRGPHY_RES_1000HD:
				mii->mii_media_active |= IFM_1000_T | IFM_HDX; 	break;
			case BRGPHY_RES_100FD:
				mii->mii_media_active |= IFM_100_TX | IFM_FDX; break;
			case BRGPHY_RES_100T4:
				mii->mii_media_active |= IFM_100_T4; break;
			case BRGPHY_RES_100HD:
				mii->mii_media_active |= IFM_100_TX | IFM_HDX; 	break;
			case BRGPHY_RES_10FD:
				mii->mii_media_active |= IFM_10_T | IFM_FDX; break;
			case BRGPHY_RES_10HD:
				mii->mii_media_active |= IFM_10_T | IFM_HDX; break;
			default:
				mii->mii_media_active |= IFM_NONE; break;
			}
		}
	} else {
		/* If serdes link is up, get the negotiated speed/duplex. */
		if (bmsr & BRGPHY_BMSR_LINK) {
			mii->mii_media_status |= IFM_ACTIVE;
		}

		/* Check the link speed/duplex based on the PHY type. */
		if (bsc->serdes_flags & BRGPHY_5706S) {
			mii->mii_media_active |= IFM_1000_SX;

			/* If autoneg enabled, read negotiated duplex settings */
			if (bmcr & BRGPHY_BMCR_AUTOEN) {
				val = PHY_READ(sc, BRGPHY_SERDES_ANAR) & PHY_READ(sc, BRGPHY_SERDES_ANLPAR);
				if (val & BRGPHY_SERDES_ANAR_FDX)
					mii->mii_media_active |= IFM_FDX;
				else
					mii->mii_media_active |= IFM_HDX;
			}

		} else if (bsc->serdes_flags & BRGPHY_5708S) {
			PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, BRGPHY_5708S_DIG_PG0);
			xstat = PHY_READ(sc, BRGPHY_5708S_PG0_1000X_STAT1);

			/* Todo: Create #defines for hard coded values */
			switch (xstat & BRGPHY_5708S_PG0_1000X_STAT1_SPEED_MASK) {
			case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_10: 
				mii->mii_media_active |= IFM_10_FL; break;
			case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_100: 
				mii->mii_media_active |= IFM_100_FX; break;
			case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_1G: 
				mii->mii_media_active |= IFM_1000_SX; break;
			case BRGPHY_5708S_PG0_1000X_STAT1_SPEED_25G: 
				mii->mii_media_active |= IFM_2500_SX; break;
			}

			if (xstat & BRGPHY_5708S_PG0_1000X_STAT1_FDX)
				mii->mii_media_active |= IFM_FDX;
			else
				mii->mii_media_active |= IFM_HDX;
		}
	}

#if 0
	/* Todo: Change bge/bce to use these settings. */

	/* Fetch flow control settings from the PHY */
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		/* Set FLAG0 is RX is enabled and FLAG1 if TX is enabled */
		if ((anar & BRGPHY_ANAR_PC) && (anlpar & BRGPHY_ANLPAR_PC)) {
			mii->mii_media_active |= IFM_FLAG0 | IFM_FLAG1;
		} else if (!(anar & BRGPHY_ANAR_PC) && (anlpar & BRGPHY_ANAR_ASP) &&
		    (anlpar & BRPHY_ANLPAR_PC) && (anlpar & BRGPHY_ANLPAR_ASP)) {
			mii->mii_media_active |= IFM_FLAG1;
		} else if ((anar & BRGPHY_ANAR_PC) && (anar & BRGPHY_ANAR_ASP) &&
		    !(anlpar & BRGPHY_ANLPAR_PC) && (anlpar & BRGPHY_ANLPAR_ASP)) {
			mii->mii_media_active |= IFM_FLAG0;
		}
	}
	/* Todo: Add support for fiber settings too. */
#endif


brgphy_status_exit:
	return;
}

static void
brgphy_mii_phy_auto(struct mii_softc *sc)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	int ktcr = 0;

	brgphy_reset(sc);

	/* Enable flow control in the advertisement register. */
	if ((sc->mii_flags & MIIF_HAVEFIBER) == 0) {
		/* Pause capability advertisement (pause capable & asymmetric) */
		PHY_WRITE(sc, BRGPHY_MII_ANAR,
	    	BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA |
	    	BRGPHY_ANAR_ASP | BRGPHY_ANAR_PC);
	} else {
		PHY_WRITE(sc, BRGPHY_SERDES_ANAR, BRGPHY_SERDES_ANAR_FDX |
			BRGPHY_SERDES_ANAR_HDX | BRGPHY_SERDES_ANAR_BOTH_PAUSE);
	}

	/* Enable speed in the 1000baseT control register */
	ktcr = BRGPHY_1000CTL_AFD | BRGPHY_1000CTL_AHD;
	if (bsc->mii_model == MII_MODEL_xxBROADCOM_BCM5701)
		ktcr |= BRGPHY_1000CTL_MSE | BRGPHY_1000CTL_MSC;
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, ktcr);
	ktcr = PHY_READ(sc, BRGPHY_MII_1000CTL);

	/* Start autonegotiation */
	PHY_WRITE(sc, BRGPHY_MII_BMCR,BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	PHY_WRITE(sc, BRGPHY_MII_IMR, 0xFF00);

}

/* Enable loopback to force the link down. */
static void
brgphy_enable_loopback(struct mii_softc *sc)
{
	int i;

	PHY_WRITE(sc, BRGPHY_MII_BMCR, BRGPHY_BMCR_LOOP);
	for (i = 0; i < 15000; i++) {
		if (!(PHY_READ(sc, BRGPHY_MII_BMSR) & BRGPHY_BMSR_LINK))
			break;
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
brgphy_fixup_5704_a0_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
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
brgphy_fixup_adc_bug(struct mii_softc *sc)
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
brgphy_fixup_adjust_trim(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x000a },
		{ BRGPHY_MII_DSP_RW_PORT,	0x110b },
		{ BRGPHY_MII_TEST1,			0x0014 },
		{ BRGPHY_MII_AUXCTL,		0x0400 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
brgphy_fixup_ber_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x000a },
		{ BRGPHY_MII_DSP_RW_PORT,	0x310b },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x201f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x9506 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x401f },
		{ BRGPHY_MII_DSP_RW_PORT,	0x14e2 },
		{ BRGPHY_MII_AUXCTL,		0x0400 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
brgphy_fixup_crc_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_DSP_RW_PORT,	0x0a75 },
		{ 0x1c,				0x8c68 },
		{ 0x1c,				0x8d68 },
		{ 0x1c,				0x8c68 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
brgphy_fixup_jitter_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		uint16_t	val;
	} dspcode[] = {
		{ BRGPHY_MII_AUXCTL,		0x0c00 },
		{ BRGPHY_MII_DSP_ADDR_REG,	0x000a },
		{ BRGPHY_MII_DSP_RW_PORT,	0x010b },
		{ BRGPHY_MII_AUXCTL,		0x0400 },
		{ 0,				0 },
	};
	int i;

	for (i = 0; dspcode[i].reg != 0; i++)
		PHY_WRITE(sc, dspcode[i].reg, dspcode[i].val);
}

static void
brgphy_ethernet_wirespeed(struct mii_softc *sc)
{
	uint32_t	val;

	/* Enable Ethernet@WireSpeed. */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7007);
	val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, val | (1 << 15) | (1 << 4));
}

static void
brgphy_jumbo_settings(struct mii_softc *sc, u_long mtu)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	uint32_t	val;

	/* Set or clear jumbo frame settings in the PHY. */
	if (mtu > ETHER_MAX_LEN) {
		if (bsc->mii_model == MII_MODEL_xxBROADCOM_BCM5401) {
			/* BCM5401 PHY cannot read-modify-write. */
			PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x4c20);
		} else {
			PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7);
			val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
			PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
			    val | BRGPHY_AUXCTL_LONG_PKT);
		}

		val = PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL);
		PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL,
		    val | BRGPHY_PHY_EXTCTL_HIGH_LA);
	} else {
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7);
		val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
		    val & ~(BRGPHY_AUXCTL_LONG_PKT | 0x7));

		val = PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL);
		PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL, 
			val & ~BRGPHY_PHY_EXTCTL_HIGH_LA);
	}
}

static void
brgphy_reset(struct mii_softc *sc)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	struct bge_softc *bge_sc = NULL;
	struct bce_softc *bce_sc = NULL;
	struct ifnet *ifp;

	mii_phy_reset(sc);

	/* Handle any PHY specific procedures to finish the reset. */
	switch (bsc->mii_oui) {
	case MII_OUI_BROADCOM:
		break;
	case MII_OUI_xxBROADCOM:
		switch (bsc->mii_model) {
		case MII_MODEL_xxBROADCOM_BCM5400:
			bcm5401_load_dspcode(sc);
			break;
		case MII_MODEL_xxBROADCOM_BCM5401:
			if (bsc->mii_rev == 1 || bsc->mii_rev == 3)
				bcm5401_load_dspcode(sc);
			break;
		case MII_MODEL_xxBROADCOM_BCM5411:
			bcm5411_load_dspcode(sc);
			break;
		}
		break;
	case MII_OUI_xxBROADCOM_ALT1:
		break;
	}

	ifp = sc->mii_pdata->mii_ifp;

	/* Find the driver associated with this PHY. */
	if (strcmp(ifp->if_dname, "bge") == 0)	{
		bge_sc = ifp->if_softc;
	} else if (strcmp(ifp->if_dname, "bce") == 0) {
		bce_sc = ifp->if_softc;
	}

	/* Handle any bge (NetXtreme/NetLink) workarounds. */
	if (bge_sc) {
		/* Fix up various bugs */
		if (bge_sc->bge_flags & BGE_FLAG_5704_A0_BUG)
			brgphy_fixup_5704_a0_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_ADC_BUG)
			brgphy_fixup_adc_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_ADJUST_TRIM)
			brgphy_fixup_adjust_trim(sc);
		if (bge_sc->bge_flags & BGE_FLAG_BER_BUG)
			brgphy_fixup_ber_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_CRC_BUG)
			brgphy_fixup_crc_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_JITTER_BUG)
			brgphy_fixup_jitter_bug(sc);

		brgphy_jumbo_settings(sc, ifp->if_mtu);

		/*
		 * Don't enable Ethernet@WireSpeed for the 5700 or the
		 * 5705 A1 and A2 chips.
		 */
		if (bge_sc->bge_asicrev != BGE_ASICREV_BCM5700 &&
		    bge_sc->bge_chipid != BGE_CHIPID_BCM5705_A1 &&
		    bge_sc->bge_chipid != BGE_CHIPID_BCM5705_A2)
			brgphy_ethernet_wirespeed(sc);

		/* Enable Link LED on Dell boxes */
		if (bge_sc->bge_flags & BGE_FLAG_NO_3LED) {
			PHY_WRITE(sc, BRGPHY_MII_PHY_EXTCTL,
			    PHY_READ(sc, BRGPHY_MII_PHY_EXTCTL) &
			    ~BRGPHY_PHY_EXTCTL_3_LED);
		}
	/* Handle any bce (NetXtreme II) workarounds. */
	} else if (bce_sc) {

		if (BCE_CHIP_NUM(bce_sc) == BCE_CHIP_NUM_5708 &&
			BCE_CHIP_BOND_ID(bce_sc) & BCE_CHIP_BOND_ID_SERDES_BIT) {

			/* Store autoneg capabilities/results in digital block (Page 0) */
			PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, BRGPHY_5708S_DIG3_PG2);
			PHY_WRITE(sc, BRGPHY_5708S_PG2_DIGCTL_3_0, 
				BRGPHY_5708S_PG2_DIGCTL_3_0_USE_IEEE);
			PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, BRGPHY_5708S_DIG_PG0);

			/* Enable fiber mode and autodetection */
			PHY_WRITE(sc, BRGPHY_5708S_PG0_1000X_CTL1, 
				PHY_READ(sc, BRGPHY_5708S_PG0_1000X_CTL1) | 
				BRGPHY_5708S_PG0_1000X_CTL1_AUTODET_EN | 
				BRGPHY_5708S_PG0_1000X_CTL1_FIBER_MODE);

			/* Enable parallel detection */
			PHY_WRITE(sc, BRGPHY_5708S_PG0_1000X_CTL2, 
				PHY_READ(sc, BRGPHY_5708S_PG0_1000X_CTL2) | 
				BRGPHY_5708S_PG0_1000X_CTL2_PAR_DET_EN);

			/* Advertise 2.5G support through next page during autoneg */
			if (bce_sc->bce_phy_flags & BCE_PHY_2_5G_CAPABLE_FLAG)
				PHY_WRITE(sc, BRGPHY_5708S_ANEG_NXT_PG_XMIT1, 
					PHY_READ(sc, BRGPHY_5708S_ANEG_NXT_PG_XMIT1) | 
					BRGPHY_5708S_ANEG_NXT_PG_XMIT1_25G);

			/* Increase TX signal amplitude */
			if ((BCE_CHIP_ID(bce_sc) == BCE_CHIP_ID_5708_A0) ||
			    (BCE_CHIP_ID(bce_sc) == BCE_CHIP_ID_5708_B0) ||
			    (BCE_CHIP_ID(bce_sc) == BCE_CHIP_ID_5708_B1)) {
				PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
					BRGPHY_5708S_TX_MISC_PG5);
				PHY_WRITE(sc, BRGPHY_5708S_PG5_TXACTL1, 
					PHY_READ(sc, BRGPHY_5708S_PG5_TXACTL1) & ~0x30);
				PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
					BRGPHY_5708S_DIG_PG0);
			}

			/* Backplanes use special driver/pre-driver/pre-emphasis values. */
			if ((bce_sc->bce_shared_hw_cfg & BCE_SHARED_HW_CFG_PHY_BACKPLANE) &&
				(bce_sc->bce_port_hw_cfg & BCE_PORT_HW_CFG_CFG_TXCTL3_MASK)) {
					PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR, 
						BRGPHY_5708S_TX_MISC_PG5);
					PHY_WRITE(sc, BRGPHY_5708S_PG5_TXACTL3, 
						bce_sc->bce_port_hw_cfg & 
						BCE_PORT_HW_CFG_CFG_TXCTL3_MASK);
					PHY_WRITE(sc, BRGPHY_5708S_BLOCK_ADDR,
						BRGPHY_5708S_DIG_PG0);
			}
		} else {
			brgphy_fixup_ber_bug(sc);
			brgphy_jumbo_settings(sc, ifp->if_mtu);
			brgphy_ethernet_wirespeed(sc);
		}

	}
}

