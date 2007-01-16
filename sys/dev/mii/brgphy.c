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
 * Driver for the Broadcom BCR5400 1000baseTX PHY.
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
	int mii_model;
	int mii_rev;
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
static int	brgphy_mii_phy_auto(struct mii_softc *);
static void	brgphy_reset(struct mii_softc *);
static void	brgphy_loop(struct mii_softc *);
static void	bcm5401_load_dspcode(struct mii_softc *);
static void	bcm5411_load_dspcode(struct mii_softc *);
static void	brgphy_fixup_adc_bug(struct mii_softc *);
static void	brgphy_fixup_5704_a0_bug(struct mii_softc *);
static void	brgphy_fixup_ber_bug(struct mii_softc *);
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
	MII_PHY_DESC(xxBROADCOM, BCM5706C),
	MII_PHY_DESC(xxBROADCOM, BCM5708C),
	MII_PHY_DESC(xxBROADCOM, BCM5714),
	MII_PHY_DESC(xxBROADCOM, BCM5750),
	MII_PHY_DESC(xxBROADCOM, BCM5752),
	MII_PHY_DESC(xxBROADCOM, BCM5754),
	MII_PHY_DESC(xxBROADCOM, BCM5780),
	MII_PHY_DESC(xxBROADCOM_ALT1, BCM5787),
	MII_PHY_END
};

static int
brgphy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, brgphys, BUS_PROBE_DEFAULT));
}

static int
brgphy_attach(device_t dev)
{
	struct brgphy_softc *bsc;
	struct mii_softc *sc;
	struct mii_attach_args *ma;
	struct mii_data *mii;
	const char *sep = "";
	struct bge_softc *bge_sc = NULL;
	struct bce_softc *bce_sc = NULL;
	int fast_ether_only = FALSE;

	bsc = device_get_softc(dev);
	sc = &bsc->mii_sc;
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
	    BMCR_LOOP | BMCR_S100);
#endif

	bsc->mii_model = MII_MODEL(ma->mii_id2);
	bsc->mii_rev = MII_REV(ma->mii_id2);
	brgphy_reset(sc);

	sc->mii_capabilities = PHY_READ(sc, MII_BMSR) & ma->mii_capmask;
	sc->mii_capabilities &= ~BMSR_ANEG;
	device_printf(dev, " ");
	mii_add_media(sc);

	/* Find the driver associated with this PHY. */
	if (strcmp(mii->mii_ifp->if_dname, "bge") == 0)	{
		bge_sc = mii->mii_ifp->if_softc;
	} else if (strcmp(mii->mii_ifp->if_dname, "bce") == 0) {
		bce_sc = mii->mii_ifp->if_softc;
	}

	/* The 590x chips are 10/100 only. */
	if (strcmp(mii->mii_ifp->if_dname, "bge") == 0 &&
	    pci_get_vendor(bge_sc->bge_dev) == BCOM_VENDORID &&
	    (pci_get_device(bge_sc->bge_dev) == BCOM_DEVICEID_BCM5901 ||
	    pci_get_device(bge_sc->bge_dev) == BCOM_DEVICEID_BCM5901A2))
		fast_ether_only = TRUE;

	if (fast_ether_only == FALSE) {
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T, 0,
		    sc->mii_inst), BRGPHY_BMCR_FDX);
		PRINT(", 1000baseTX");
		ADD(IFM_MAKEWORD(IFM_ETHER, IFM_1000_T,
		    IFM_FDX, sc->mii_inst), 0);
		PRINT("1000baseTX-FDX");
		sc->mii_anegticks = MII_ANEGTICKS_GIGE;
	} else
		sc->mii_anegticks = MII_ANEGTICKS;

	ADD(IFM_MAKEWORD(IFM_ETHER, IFM_AUTO, 0, sc->mii_inst), 0);
	PRINT("auto");

	printf("\n");
#undef ADD
#undef PRINT

	MIIBUS_MEDIAINIT(sc->mii_dev);
	return (0);
}

static int
brgphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;

	switch (cmd) {
	case MII_POLLSTAT:
		/* If we're not polling our PHY instance, just return. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);
		break;
	case MII_MEDIACHG:
		/*
		 * If the media indicates a different PHY instance,
		 * isolate ourselves.
		 */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst) {
			PHY_WRITE(sc, MII_BMCR,
			    PHY_READ(sc, MII_BMCR) | BMCR_ISO);
			return (0);
		}

		/* If the interface is not up, don't do anything. */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			break;

		brgphy_reset(sc);	/* XXX hardware bug work-around */

		switch (IFM_SUBTYPE(ife->ifm_media)) {
		case IFM_AUTO:
#ifdef foo
			/* If we're already in auto mode, just return. */
			if (PHY_READ(sc, BRGPHY_MII_BMCR) & BRGPHY_BMCR_AUTOEN)
				return (0);
#endif
			(void)brgphy_mii_phy_auto(sc);
			break;
		case IFM_1000_T:
		case IFM_100_TX:
		case IFM_10_T:
			brgphy_setmedia(sc, ife->ifm_media,
			    mii->mii_ifp->if_flags & IFF_LINK0);
			break;
#ifdef foo
		case IFM_NONE:
			PHY_WRITE(sc, MII_BMCR, BMCR_ISO | BMCR_PDOWN);
			break;
#endif
		case IFM_100_T4:
		default:
			return (EINVAL);
		}
		break;
	case MII_TICK:
		/* If we're not currently selected, just return. */
		if (IFM_INST(ife->ifm_media) != sc->mii_inst)
			return (0);

		/* Is the interface even up? */
		if ((mii->mii_ifp->if_flags & IFF_UP) == 0)
			return (0);

		/* Only used for autonegotiation. */
		if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
			sc->mii_ticks = 0;	/* Reset autoneg timer. */
			break;
		}

		/*
		 * Check to see if we have link.  If we do, we don't
		 * need to restart the autonegotiation process.
		 */
		if (PHY_READ(sc, BRGPHY_MII_BMSR) & BRGPHY_BMSR_LINK) {
			sc->mii_ticks = 0;	/* Reset autoneg timer. */
			break;
		}

		/* Announce link loss right after it happens. */
		if (sc->mii_ticks++ == 0)
			break;

		/* Only retry autonegotiation every mii_anegticks seconds. */
		if (sc->mii_ticks <= sc->mii_anegticks)
			return (0);

		sc->mii_ticks = 0;
		(void)brgphy_mii_phy_auto(sc);
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
	}
	mii_phy_update(sc, cmd);
	return (0);
}

static void
brgphy_setmedia(struct mii_softc *sc, int media, int master)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	int bmcr, gig;

	switch (IFM_SUBTYPE(media)) {
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

	brgphy_loop(sc);
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, 0);
	PHY_WRITE(sc, BRGPHY_MII_BMCR, bmcr);
	PHY_WRITE(sc, BRGPHY_MII_ANAR, BRGPHY_SEL_TYPE);

	if (IFM_SUBTYPE(media) != IFM_1000_T)
		return;

	PHY_WRITE(sc, BRGPHY_MII_1000CTL, gig);
	PHY_WRITE(sc, BRGPHY_MII_BMCR,
	    bmcr | BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);

	if (bsc->mii_model != MII_MODEL_xxBROADCOM_BCM5701)
		return;

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
}

static void
brgphy_status(struct mii_softc *sc)
{
	struct mii_data *mii = sc->mii_pdata;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int bmcr, bmsr;

	mii->mii_media_status = IFM_AVALID;
	mii->mii_media_active = IFM_ETHER;

	bmcr = PHY_READ(sc, BRGPHY_MII_BMCR);
	bmsr = PHY_READ(sc, BRGPHY_MII_BMSR);

	if (bmsr & BRGPHY_BMSR_LINK)
		mii->mii_media_status |= IFM_ACTIVE;

	if (bmcr & BRGPHY_BMCR_LOOP)
		mii->mii_media_active |= IFM_LOOP;

	if (bmcr & BRGPHY_BMCR_AUTOEN) {
		if ((bmsr & BRGPHY_BMSR_ACOMP) == 0) {
			/* Erg, still trying, I guess... */
			mii->mii_media_active |= IFM_NONE;
			return;
		}
	}

	if (bmsr & BRGPHY_BMSR_LINK) {
		switch (PHY_READ(sc, BRGPHY_MII_AUXSTS) &
		    BRGPHY_AUXSTS_AN_RES) {
		case BRGPHY_RES_1000FD:
			mii->mii_media_active |= IFM_1000_T | IFM_FDX;
			break;
		case BRGPHY_RES_1000HD:
			mii->mii_media_active |= IFM_1000_T | IFM_HDX;
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
	} else
		mii->mii_media_active = ife->ifm_media;
}

static int
brgphy_mii_phy_auto(struct mii_softc *sc)
{
	struct brgphy_softc *bsc = (struct brgphy_softc *)sc;
	int ktcr = 0;

	brgphy_loop(sc);
	brgphy_reset(sc);
	ktcr = BRGPHY_1000CTL_AFD | BRGPHY_1000CTL_AHD;
	if (bsc->mii_model == MII_MODEL_xxBROADCOM_BCM5701)
		ktcr |= BRGPHY_1000CTL_MSE | BRGPHY_1000CTL_MSC;
	PHY_WRITE(sc, BRGPHY_MII_1000CTL, ktcr);
	ktcr = PHY_READ(sc, BRGPHY_MII_1000CTL);
	DELAY(1000);
	PHY_WRITE(sc, BRGPHY_MII_ANAR,
	    BMSR_MEDIA_TO_ANAR(sc->mii_capabilities) | ANAR_CSMA);
	DELAY(1000);
	PHY_WRITE(sc, BRGPHY_MII_BMCR,
	    BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	PHY_WRITE(sc, BRGPHY_MII_IMR, 0xFF00);
	return (EJUSTRETURN);
}

static void
brgphy_loop(struct mii_softc *sc)
{
	int i;

	PHY_WRITE(sc, BRGPHY_MII_BMCR, BRGPHY_BMCR_LOOP);
	for (i = 0; i < 15000; i++) {
		if (!(PHY_READ(sc, BRGPHY_MII_BMSR) & BRGPHY_BMSR_LINK)) {
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
brgphy_fixup_5704_a0_bug(struct mii_softc *sc)
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
brgphy_fixup_ber_bug(struct mii_softc *sc)
{
	static const struct {
		int		reg;
		u_int16_t	val;
	} dspcode[] = {
		{ 0x18,				0x0c00 },
		{ 0x17,				0x000a },
		{ 0x15,				0x310b },
		{ 0x17,				0x201f },
		{ 0x15,				0x9506 },
		{ 0x17,				0x401f },
		{ 0x15,				0x14e2 },
		{ 0x18,				0x0400 },
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
	u_int32_t	val;

	/* Enable Ethernet@WireSpeed. */
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7007);
	val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
	PHY_WRITE(sc, BRGPHY_MII_AUXCTL, val | (1 << 15) | (1 << 4));
}

static void
brgphy_jumbo_settings(struct mii_softc *sc, u_long mtu)
{
	u_int32_t	val;

	/* Set or clear jumbo frame settings in the PHY. */
	if (mtu > ETHER_MAX_LEN) {
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL, 0x7);
		val = PHY_READ(sc, BRGPHY_MII_AUXCTL);
		PHY_WRITE(sc, BRGPHY_MII_AUXCTL,
		    val | BRGPHY_AUXCTL_LONG_PKT);

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

	ifp = sc->mii_pdata->mii_ifp;

	/* Find the driver associated with this PHY. */
	if (strcmp(ifp->if_dname, "bge") == 0)	{
		bge_sc = ifp->if_softc;
	} else if (strcmp(ifp->if_dname, "bce") == 0) {
		bce_sc = ifp->if_softc;
	}

	/* Handle any NetXtreme/bge workarounds. */
	if (bge_sc) {
		/* Fix up various bugs */
		if (bge_sc->bge_flags & BGE_FLAG_ADC_BUG)
			brgphy_fixup_adc_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_5704_A0_BUG)
			brgphy_fixup_5704_a0_bug(sc);
		if (bge_sc->bge_flags & BGE_FLAG_BER_BUG)
			brgphy_fixup_ber_bug(sc);
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
	} else if (bce_sc) {
		brgphy_fixup_ber_bug(sc);
		brgphy_jumbo_settings(sc, ifp->if_mtu);
		brgphy_ethernet_wirespeed(sc);
	}
}
