/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	$Id: if_pn.c,v 1.18 1999/04/24 20:14:00 peter Exp $
 */

/*
 * 82c168/82c169 PNIC fast ethernet PCI NIC driver
 *
 * Supports various network adapters based on the Lite-On PNIC
 * PCI network controller chip including the LinkSys LNE100TX.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The PNIC chip is a DEC tulip clone. This driver uses much of the
 * same code from the driver for the Winbond chip (which is also a
 * tulip clone) except for the MII, EEPROM and filter programming.
 *
 * Technically we could merge support for this chip into the 'de'
 * driver, but it's such a mess that I'm afraid to go near it.
 *
 * The PNIC appears to support both an external MII and an internal
 * transceiver. I think most 100Mbps implementations use a PHY attached
 * the the MII. The LinkSys board that I have uses a Myson MTD972
 * 100BaseTX PHY.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define PN_USEIOSPACE

/* #define PN_BACKGROUND_AUTONEG */

#define PN_RX_BUG_WAR

#include <pci/if_pnreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_pn.c,v 1.18 1999/04/24 20:14:00 peter Exp $";
#endif

#ifdef __alpha__
#undef vtophys
#define	vtophys(va)	(pmap_kextract(((vm_offset_t) (va))) \
			 + 1*1024*1024*1024)
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct pn_type pn_devs[] = {
	{ PN_VENDORID, PN_DEVICEID_PNIC,
		"82c168 PNIC 10/100BaseTX" },
	{ PN_VENDORID, PN_DEVICEID_PNIC,
		"82c169 PNIC 10/100BaseTX" },
	{ PN_VENDORID, PN_DEVICEID_PNIC_II,
		"82c115 PNIC II 10/100BaseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct pn_type pn_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long pn_count = 0;
static const char *pn_probe	__P((pcici_t, pcidi_t));
static void pn_attach		__P((pcici_t, int));

static int pn_newbuf		__P((struct pn_softc *,
						struct pn_chain_onefrag *));
static int pn_encap		__P((struct pn_softc *, struct pn_chain *,
						struct mbuf *));

#ifdef PN_RX_BUG_WAR
static void pn_rx_bug_war	__P((struct pn_softc *,
						struct pn_chain_onefrag *));
#endif
static void pn_rxeof		__P((struct pn_softc *));
static void pn_rxeoc		__P((struct pn_softc *));
static void pn_txeof		__P((struct pn_softc *));
static void pn_txeoc		__P((struct pn_softc *));
static void pn_intr		__P((void *));
static void pn_start		__P((struct ifnet *));
static int pn_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void pn_init		__P((void *));
static void pn_stop		__P((struct pn_softc *));
static void pn_watchdog		__P((struct ifnet *));
static void pn_shutdown		__P((int, void *));
static int pn_ifmedia_upd	__P((struct ifnet *));
static void pn_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void pn_eeprom_getword	__P((struct pn_softc *, u_int8_t, u_int16_t *));
static void pn_read_eeprom	__P((struct pn_softc *, caddr_t, int,
							int, int));
static u_int16_t pn_phy_readreg	__P((struct pn_softc *, int));
static void pn_phy_writereg	__P((struct pn_softc *, u_int16_t, u_int16_t));

static void pn_autoneg_xmit	__P((struct pn_softc *));
static void pn_autoneg_mii	__P((struct pn_softc *, int, int));
static void pn_setmode_mii	__P((struct pn_softc *, int));
static void pn_getmode_mii	__P((struct pn_softc *));
static void pn_autoneg		__P((struct pn_softc *, int, int));
static void pn_setmode		__P((struct pn_softc *, int));
static void pn_setcfg		__P((struct pn_softc *, u_int32_t));
static u_int32_t pn_calchash	__P((u_int8_t *));
static void pn_setfilt		__P((struct pn_softc *));
static void pn_reset		__P((struct pn_softc *));
static int pn_list_rx_init	__P((struct pn_softc *));
static int pn_list_tx_init	__P((struct pn_softc *));

#define PN_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define PN_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void pn_eeprom_getword(sc, addr, dest)
	struct pn_softc		*sc;
	u_int8_t		addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int32_t		r;

	CSR_WRITE_4(sc, PN_SIOCTL, PN_EE_READ|addr);

	for (i = 0; i < PN_TIMEOUT; i++) {
		DELAY(1);
		r = CSR_READ_4(sc, PN_SIO);
		if (!(r & PN_SIO_BUSY)) {
			*dest = (u_int16_t)(r & 0x0000FFFF);
			return;
		}
	}

	return;

}

/*
 * Read a sequence of words from the EEPROM.
 */
static void pn_read_eeprom(sc, dest, off, cnt, swap)
	struct pn_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		pn_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

static u_int16_t pn_phy_readreg(sc, reg)
	struct pn_softc		*sc;
	int			reg;
{
	int			i;
	u_int32_t		rval;

	CSR_WRITE_4(sc, PN_MII,
		PN_MII_READ | (sc->pn_phy_addr << 23) | (reg << 18));

	for (i = 0; i < PN_TIMEOUT; i++) {
		DELAY(1);
		rval = CSR_READ_4(sc, PN_MII);
		if (!(rval & PN_MII_BUSY)) {
			if ((u_int16_t)(rval & 0x0000FFFF) == 0xFFFF)
				return(0);
			else
				return((u_int16_t)(rval & 0x0000FFFF));
		}
	}

	return(0);
}

static void pn_phy_writereg(sc, reg, data)
	struct pn_softc		*sc;
	u_int16_t		reg;
	u_int16_t		data;
{
	int			i;

	CSR_WRITE_4(sc, PN_MII,
		PN_MII_WRITE | (sc->pn_phy_addr << 23) | (reg << 18) | data);


	for (i = 0; i < PN_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, PN_MII) & PN_MII_BUSY))
			break;
	}

	return;
}

#define PN_POLY		0xEDB88320
#define PN_BITS		9

static u_int32_t pn_calchash(addr)
	u_int8_t		*addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? PN_POLY : 0);
	}

	return (crc & ((1 << PN_BITS) - 1));
}

/*
 * Initiate an autonegotiation session.
 */
static void pn_autoneg_xmit(sc)
	struct pn_softc		*sc;
{
	u_int16_t		phy_sts;

	pn_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(pn_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = pn_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	pn_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void pn_autoneg_mii(sc, flag, verbose)
	struct pn_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media, advert, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

	/*
	 * The 100baseT4 PHY on the 3c905-T4 has the 'autoneg supported'
	 * bit cleared in the status register, but has the 'autoneg enabled'
	 * bit set in the control register. This is a contradiction, and
	 * I'm not sure how to handle it. If you want to force an attempt
	 * to autoneg for 100baseT4 PHYs, #define FORCE_AUTONEG_TFOUR
	 * and see what happens.
	 */
#ifndef FORCE_AUTONEG_TFOUR
	/*
	 * First, see if autoneg is supported. If not, there's
	 * no point in continuing.
	 */
	phy_sts = pn_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("pn%d: autonegotiation not supported\n",
							sc->pn_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case PN_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		pn_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case PN_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise pn_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->pn_cdata.pn_tx_head != NULL) {
			sc->pn_want_auto = 1;
			return;
		}
		pn_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->pn_autoneg = 1;
		sc->pn_want_auto = 0;
		return;
		break;
	case PN_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->pn_autoneg = 0;
		break;
	default:
		printf("pn%d: invalid autoneg flag: %d\n", sc->pn_unit, flag);
		return;
	}

	if (pn_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("pn%d: autoneg complete, ", sc->pn_unit);
		phy_sts = pn_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("pn%d: autoneg not complete, ", sc->pn_unit);
	}

	media = pn_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (pn_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = pn_phy_readreg(sc, PHY_ANAR);
		ability = pn_phy_readreg(sc, PHY_LPAR);

		if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4) {
			ifm->ifm_media = IFM_ETHER|IFM_100_T4;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(100baseT4)\n");
		} else if (advert & PHY_ANAR_100BTXFULL &&
			ability & PHY_ANAR_100BTXFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
			media |= PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_100BTXHALF &&
			ability & PHY_ANAR_100BTXHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_10BTFULL &&
			ability & PHY_ANAR_10BTFULL) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 10Mbps)\n");
		} else if (advert & PHY_ANAR_10BTHALF &&
			ability & PHY_ANAR_10BTHALF) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}

		media &= ~PHY_BMCR_AUTONEGENBL;

		/* Set ASIC's duplex mode to match the PHY. */
		pn_setcfg(sc, ifm->ifm_media);
		pn_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	pn_init(sc);

	if (sc->pn_tx_pend) {
		sc->pn_autoneg = 0;
		sc->pn_tx_pend = 0;
		pn_start(ifp);
	}

	return;
}

static void pn_getmode_mii(sc)
	struct pn_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = pn_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("pn%d: PHY status word: %x\n", sc->pn_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("pn%d: 10Mbps half-duplex mode supported\n",
								sc->pn_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("pn%d: 10Mbps full-duplex mode supported\n",
								sc->pn_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("pn%d: 100Mbps half-duplex mode supported\n",
								sc->pn_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("pn%d: 100Mbps full-duplex mode supported\n",
								sc->pn_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("pn%d: 100baseT4 mode supported\n", sc->pn_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("pn%d: forcing on autoneg support for BT4\n",
							 sc->pn_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("pn%d: autoneg supported\n", sc->pn_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

static void pn_autoneg(sc, flag, verbose)
	struct pn_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int32_t		nway = 0, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

	switch (flag) {
	case PN_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		CSR_WRITE_4(sc, PN_GEN,
		    PN_GEN_MUSTBEONE|PN_GEN_100TX_LOOP);
		PN_CLRBIT(sc, PN_NWAY, PN_NWAY_AUTONEGRSTR);
		PN_SETBIT(sc, PN_NWAY, PN_NWAY_AUTOENB);
		DELAY(5000000);
		break;
	case PN_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise pn_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->pn_cdata.pn_tx_head != NULL) {
			sc->pn_want_auto = 1;
			return;
		}
		CSR_WRITE_4(sc, PN_GEN,
		    PN_GEN_MUSTBEONE|PN_GEN_100TX_LOOP);
		PN_CLRBIT(sc, PN_NWAY, PN_NWAY_AUTONEGRSTR);
		PN_SETBIT(sc, PN_NWAY, PN_NWAY_AUTOENB);
		ifp->if_timer = 5;
		sc->pn_autoneg = 1;
		sc->pn_want_auto = 0;
		return;
		break;
	case PN_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->pn_autoneg = 0;
		break;
	default:
		printf("pn%d: invalid autoneg flag: %d\n", sc->pn_unit, flag);
		return;
	}

	if (CSR_READ_4(sc, PN_NWAY) & PN_NWAY_LPAR) {
		if (verbose)
			printf("pn%d: autoneg complete, ", sc->pn_unit);
	} else {
		if (verbose)
			printf("pn%d: autoneg not complete, ", sc->pn_unit);
	}

	/* Link is good. Report modes and set duplex mode. */
	if (CSR_READ_4(sc, PN_ISR) & PN_ISR_LINKPASS) {
		if (verbose)
			printf("link status good ");

		ability = CSR_READ_4(sc, PN_NWAY);
		if (ability & PN_NWAY_LPAR100T4) {
			ifm->ifm_media = IFM_ETHER|IFM_100_T4;
			nway = PN_NWAY_MODE_100T4;
			printf("(100baseT4)\n");
		} else if (ability & PN_NWAY_LPAR100FULL) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
			nway = PN_NWAY_MODE_100FD;
			printf("(full-duplex, 100Mbps)\n");
		} else if (ability & PN_NWAY_LPAR100HALF) {
			ifm->ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
			nway = PN_NWAY_MODE_100HD;
			printf("(half-duplex, 100Mbps)\n");
		} else if (ability & PN_NWAY_LPAR10FULL) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
			nway = PN_NWAY_MODE_10FD;
			printf("(full-duplex, 10Mbps)\n");
		} else if (ability & PN_NWAY_LPAR10HALF) {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			nway = PN_NWAY_MODE_10HD;
			printf("(half-duplex, 10Mbps)\n");
		}

		/* Set ASIC's duplex mode to match the PHY. */
		pn_setcfg(sc, ifm->ifm_media);
		CSR_WRITE_4(sc, PN_NWAY, nway);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	pn_init(sc);

	if (sc->pn_tx_pend) {
		sc->pn_autoneg = 0;
		sc->pn_tx_pend = 0;
		pn_start(ifp);
	}

	return;
}

static void pn_setmode(sc, media)
	struct pn_softc		*sc;
	int			media;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->pn_autoneg) {
		printf("pn%d: canceling autoneg session\n", sc->pn_unit);
		ifp->if_timer = sc->pn_autoneg = sc->pn_want_auto = 0;
		PN_CLRBIT(sc, PN_NWAY, PN_NWAY_AUTONEGRSTR);
	}

	printf("pn%d: selecting NWAY, ", sc->pn_unit);

	if (IFM_SUBTYPE(media) == IFM_100_T4) {
		printf("100Mbps/T4, half-duplex\n");
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		printf("100Mbps, ");
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		printf("10Mbps, ");
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		printf("full duplex\n");
	} else {
		printf("half duplex\n");
	}

	pn_setcfg(sc, media);

	return;
}

static void pn_setmode_mii(sc, media)
	struct pn_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->pn_autoneg) {
		printf("pn%d: canceling autoneg session\n", sc->pn_unit);
		ifp->if_timer = sc->pn_autoneg = sc->pn_want_auto = 0;
		bmcr = pn_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		pn_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("pn%d: selecting MII, ", sc->pn_unit);

	bmcr = pn_phy_readreg(sc, PHY_BMCR);

	bmcr &= ~(PHY_BMCR_AUTONEGENBL|PHY_BMCR_SPEEDSEL|
			PHY_BMCR_DUPLEX|PHY_BMCR_LOOPBK);

	if (IFM_SUBTYPE(media) == IFM_100_T4) {
		printf("100Mbps/T4, half-duplex\n");
		bmcr |= PHY_BMCR_SPEEDSEL;
		bmcr &= ~PHY_BMCR_DUPLEX;
	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		printf("100Mbps, ");
		bmcr |= PHY_BMCR_SPEEDSEL;
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		printf("10Mbps, ");
		bmcr &= ~PHY_BMCR_SPEEDSEL;
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		printf("full duplex\n");
		bmcr |= PHY_BMCR_DUPLEX;
	} else {
		printf("half duplex\n");
		bmcr &= ~PHY_BMCR_DUPLEX;
	}

	pn_setcfg(sc, media);
	pn_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

/*
 * Programming the receiver filter on the tulip/PNIC is gross. You
 * have to construct a special setup frame and download it to the
 * chip via the transmit DMA engine. This routine is also somewhat
 * gross, as the setup frame is sent synchronously rather than putting
 * on the transmit queue. The transmitter has to be stopped, then we
 * can download the frame and wait for the 'owned' bit to clear.
 *
 * We always program the chip using 'hash perfect' mode, i.e. one perfect
 * address (our node address) and a 512-bit hash filter for multicast
 * frames. We also sneak the broadcast address into the hash filter since
 * we need that too.
 */
void pn_setfilt(sc)
	struct pn_softc		*sc;
{
	struct pn_desc		*sframe;
	u_int32_t		h, *sp;
	struct ifmultiaddr	*ifma;
	struct ifnet		*ifp;
	int			i;

	ifp = &sc->arpcom.ac_if;

	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON);
	PN_SETBIT(sc, PN_ISR, PN_ISR_TX_IDLE);

	sframe = &sc->pn_cdata.pn_sframe;
	sp = (u_int32_t *)&sc->pn_cdata.pn_sbuf;
	bzero((char *)sp, PN_SFRAME_LEN);

	sframe->pn_status = PN_TXSTAT_OWN;
	sframe->pn_next = vtophys(&sc->pn_ldata->pn_tx_list[0]);
	sframe->pn_data = vtophys(&sc->pn_cdata.pn_sbuf);
	sframe->pn_ctl = PN_SFRAME_LEN | PN_TXCTL_TLINK |
			PN_TXCTL_SETUP | PN_FILTER_HASHPERF;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_RX_PROMISC);
	else
		PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_RX_ALLMULTI);

	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = pn_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		h = pn_calchash(etherbroadcastaddr);
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	sp[39] = ((u_int16_t *)sc->arpcom.ac_enaddr)[0];
	sp[40] = ((u_int16_t *)sc->arpcom.ac_enaddr)[1];
	sp[41] = ((u_int16_t *)sc->arpcom.ac_enaddr)[2];

	CSR_WRITE_4(sc, PN_TXADDR, vtophys(sframe));
	PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON);
	CSR_WRITE_4(sc, PN_TXSTART, 0xFFFFFFFF);

	/*
	 * Wait for chip to clear the 'own' bit.
	 */
	for (i = 0; i < PN_TIMEOUT; i++) {
		DELAY(10);
		if (sframe->pn_status != PN_TXSTAT_OWN)
			break;
	}

	if (i == PN_TIMEOUT)
		printf("pn%d: failed to send setup frame\n", sc->pn_unit);

	PN_SETBIT(sc, PN_ISR, PN_ISR_TX_NOBUF|PN_ISR_TX_IDLE);

	return;
}

/*
 * In order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void pn_setcfg(sc, media)
	struct pn_softc		*sc;
	u_int32_t		media;
{
	int			i, restart = 0;

	if (CSR_READ_4(sc, PN_NETCFG) & (PN_NETCFG_TX_ON|PN_NETCFG_RX_ON)) {
		restart = 1;
		PN_CLRBIT(sc, PN_NETCFG, (PN_NETCFG_TX_ON|PN_NETCFG_RX_ON));

		for (i = 0; i < PN_TIMEOUT; i++) {
			DELAY(10);
			if ((CSR_READ_4(sc, PN_ISR) & PN_ISR_TX_IDLE) &&
				(CSR_READ_4(sc, PN_ISR) & PN_ISR_RX_IDLE))
				break;
		}

		if (i == PN_TIMEOUT)
			printf("pn%d: failed to force tx and "
				"rx to idle state\n", sc->pn_unit);

	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_SPEEDSEL);
		if (sc->pn_pinfo == NULL) {
			CSR_WRITE_4(sc, PN_GEN, PN_GEN_MUSTBEONE|
			    PN_GEN_SPEEDSEL|PN_GEN_100TX_LOOP);
			PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_PCS|
			    PN_NETCFG_SCRAMBLER|PN_NETCFG_MIIENB);
			PN_SETBIT(sc, PN_NWAY, PN_NWAY_SPEEDSEL);
		}
	} else {
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_SPEEDSEL);
		if (sc->pn_pinfo == NULL) {
			CSR_WRITE_4(sc, PN_GEN,
			    PN_GEN_MUSTBEONE|PN_GEN_100TX_LOOP);
			PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_PCS|
			    PN_NETCFG_SCRAMBLER|PN_NETCFG_MIIENB);
			PN_CLRBIT(sc, PN_NWAY, PN_NWAY_SPEEDSEL);
		}
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_FULLDUPLEX);
		if (sc->pn_pinfo == NULL)
			PN_SETBIT(sc, PN_NWAY, PN_NWAY_DUPLEX);
	} else {
		PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_FULLDUPLEX);
		if (sc->pn_pinfo == NULL)
			PN_CLRBIT(sc, PN_NWAY, PN_NWAY_DUPLEX);
	}

	if (restart)
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON|PN_NETCFG_RX_ON);

	return;
}

static void pn_reset(sc)
	struct pn_softc		*sc;
{
	register int		i;

	PN_SETBIT(sc, PN_BUSCTL, PN_BUSCTL_RESET);

	for (i = 0; i < PN_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, PN_BUSCTL) & PN_BUSCTL_RESET))
			break;
	}
	if (i == PN_TIMEOUT)
		printf("pn%d: reset never completed!\n", sc->pn_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for a Lite-On PNIC chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
pn_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct pn_type		*t;
	u_int32_t		rev;

	t = pn_devs;

	while(t->pn_name != NULL) {
		if ((device_id & 0xFFFF) == t->pn_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->pn_did) {
			if (t->pn_did == PN_DEVICEID_PNIC) {
				rev = pci_conf_read(config_id,
				    PN_PCI_REVISION) & 0xFF;
				switch(rev & PN_REVMASK) {
				case PN_REVID_82C168:
					return(t->pn_name);
					break;
				case PN_REVID_82C169:
					t++;
					return(t->pn_name);
				default:
					printf("unknown PNIC rev: %x\n", rev);
					break;
				}
			}
			return(t->pn_name);
		}
		t++;
	}

	return(NULL);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static void
pn_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef PN_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct pn_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct pn_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
#ifdef PN_RX_BUG_WAR
	u_int32_t		revision = 0;
#endif

	s = splimp();

	sc = malloc(sizeof(struct pn_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("pn%d: no memory for softc struct!\n", unit);
		return;
	}
	bzero(sc, sizeof(struct pn_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, PN_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, PN_PCI_PWRMGMTCTRL);
		if (command & PN_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, PN_PCI_LOIO);
			membase = pci_conf_read(config_id, PN_PCI_LOMEM);
			irq = pci_conf_read(config_id, PN_PCI_INTLINE);

			/* Reset the power state. */
			printf("pn%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & PN_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, PN_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, PN_PCI_LOIO, iobase);
			pci_conf_write(config_id, PN_PCI_LOMEM, membase);
			pci_conf_write(config_id, PN_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef PN_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("pn%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, PN_PCI_LOIO,
					(u_short *)&(sc->pn_bhandle))) {
		printf ("pn%d: couldn't map ports\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->pn_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->pn_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("pn%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, PN_PCI_LOMEM, &vbase, &pbase)) {
		printf ("pn%d: couldn't map memory\n", unit);
		goto fail;
	}
	sc->pn_bhandle = vbase;
#ifdef __i386__
	sc->pn_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->pn_btag = ALPHA_BUS_SPACE_MEM;
#endif
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, pn_intr, sc, &net_imask)) {
		printf("pn%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Save the cache line size. */
	sc->pn_cachesize = pci_conf_read(config_id, PN_PCI_CACHELEN) & 0xFF;

	/* Reset the adapter. */
	pn_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	pn_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 1);

	/*
	 * A PNIC chip was detected. Inform the world.
	 */
	printf("pn%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->pn_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->pn_ldata_ptr = malloc(sizeof(struct pn_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->pn_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("pn%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	sc->pn_ldata = (struct pn_list_data *)sc->pn_ldata_ptr;
	round = (unsigned int)sc->pn_ldata_ptr & 0xF;
	roundptr = sc->pn_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->pn_ldata = (struct pn_list_data *)roundptr;
	bzero(sc->pn_ldata, sizeof(struct pn_list_data));

#ifdef PN_RX_BUG_WAR
	revision = pci_conf_read(config_id, PN_PCI_REVISION) & 0x000000FF;
	if (revision == PN_169B_REV || revision == PN_169_REV ||
	    (revision & 0xF0) == PN_168_REV) {
		sc->pn_rx_war = 1;
		sc->pn_rx_buf = malloc(PN_RXLEN * 5, M_DEVBUF, M_NOWAIT);
		if (sc->pn_rx_buf == NULL) {
			printf("pn%d: no memory for workaround buffer\n", unit);
			goto fail;
		}
	} else {
		sc->pn_rx_war = 0;
	}
#endif

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "pn";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = pn_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = pn_start;
	ifp->if_watchdog = pn_watchdog;
	ifp->if_init = pn_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = PN_TX_LIST_CNT - 1;

	ifmedia_init(&sc->ifmedia, 0, pn_ifmedia_upd, pn_ifmedia_sts);

	if (bootverbose)
		printf("pn%d: probing for a PHY\n", sc->pn_unit);
	for (i = PN_PHYADDR_MIN; i < PN_PHYADDR_MAX + 1; i++) {
		if (bootverbose)
			printf("pn%d: checking address: %d\n",
						sc->pn_unit, i);
		sc->pn_phy_addr = i;
		pn_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(pn_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = pn_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = pn_phy_readreg(sc, PHY_VENID);
		phy_did = pn_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("pn%d: found PHY at address %d, ",
					sc->pn_unit, sc->pn_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
				phy_vid, phy_did);
		p = pn_phys;
		while(p->pn_vid) {
			if (phy_vid == p->pn_vid &&
				(phy_did | 0x000F) == p->pn_did) {
				sc->pn_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->pn_pinfo == NULL)
			sc->pn_pinfo = &pn_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("pn%d: PHY type: %s\n",
				sc->pn_unit, sc->pn_pinfo->pn_name);

		pn_getmode_mii(sc);
		pn_autoneg_mii(sc, PN_FLAG_FORCEDELAY, 1);
	} else {
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		pn_autoneg(sc, PN_FLAG_FORCEDELAY, 1);
	}

	media = sc->ifmedia.ifm_media;
	pn_stop(sc);
	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(pn_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int pn_list_tx_init(sc)
	struct pn_softc		*sc;
{
	struct pn_chain_data	*cd;
	struct pn_list_data	*ld;
	int			i;

	cd = &sc->pn_cdata;
	ld = sc->pn_ldata;
	for (i = 0; i < PN_TX_LIST_CNT; i++) {
		cd->pn_tx_chain[i].pn_ptr = &ld->pn_tx_list[i];
		if (i == (PN_TX_LIST_CNT - 1))
			cd->pn_tx_chain[i].pn_nextdesc =
				&cd->pn_tx_chain[0];
		else
			cd->pn_tx_chain[i].pn_nextdesc =
				&cd->pn_tx_chain[i + 1];
	}

	cd->pn_tx_free = &cd->pn_tx_chain[0];
	cd->pn_tx_tail = cd->pn_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int pn_list_rx_init(sc)
	struct pn_softc		*sc;
{
	struct pn_chain_data	*cd;
	struct pn_list_data	*ld;
	int			i;

	cd = &sc->pn_cdata;
	ld = sc->pn_ldata;

	for (i = 0; i < PN_RX_LIST_CNT; i++) {
		cd->pn_rx_chain[i].pn_ptr =
			(struct pn_desc *)&ld->pn_rx_list[i];
		if (pn_newbuf(sc, &cd->pn_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		if (i == (PN_RX_LIST_CNT - 1)) {
			cd->pn_rx_chain[i].pn_nextdesc = &cd->pn_rx_chain[0];
			ld->pn_rx_list[i].pn_next =
					vtophys(&ld->pn_rx_list[0]);
		} else {
			cd->pn_rx_chain[i].pn_nextdesc = &cd->pn_rx_chain[i + 1];
			ld->pn_rx_list[i].pn_next =
					vtophys(&ld->pn_rx_list[i + 1]);
		}
	}

	cd->pn_rx_head = &cd->pn_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int pn_newbuf(sc, c)
	struct pn_softc		*sc;
	struct pn_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("pn%d: no memory for rx list -- packet dropped!\n",
								sc->pn_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("pn%d: no memory for rx list -- packet dropped!\n",
								sc->pn_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

	/*
	 * Zero the buffer. This is part of the workaround for the
	 * promiscuous mode bug in the revision 33 PNIC chips.
	 */
	bzero((char *)mtod(m_new, char *), MCLBYTES);
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	c->pn_mbuf = m_new;
	c->pn_ptr->pn_status = PN_RXSTAT;
	c->pn_ptr->pn_data = vtophys(mtod(m_new, caddr_t));
	c->pn_ptr->pn_ctl = PN_RXCTL_RLINK | PN_RXLEN;

	return(0);
}

#ifdef PN_RX_BUG_WAR
/*
 * Grrrrr.
 * The PNIC chip has a terrible bug in it that manifests itself during
 * periods of heavy activity. The exact mode of failure if difficult to
 * pinpoint: sometimes it only happens in promiscuous mode, sometimes it
 * will happen on slow machines. The bug is that sometimes instead of
 * uploading one complete frame during reception, it uploads what looks
 * like the entire contents of its FIFO memory. The frame we want is at
 * the end of the whole mess, but we never know exactly how much data has
 * been uploaded, so salvaging the frame is hard.
 *
 * There is only one way to do it reliably, and it's disgusting.
 * Here's what we know:
 *
 * - We know there will always be somewhere between one and three extra
 *   descriptors uploaded.
 *
 * - We know the desired received frame will always be at the end of the
 *   total data upload.
 *
 * - We know the size of the desired received frame because it will be
 *   provided in the length field of the status word in the last descriptor.
 *
 * Here's what we do:
 *
 * - When we allocate buffers for the receive ring, we bzero() them.
 *   This means that we know that the buffer contents should be all
 *   zeros, except for data uploaded by the chip.
 *
 * - We also force the PNIC chip to upload frames that include the
 *   ethernet CRC at the end.
 *
 * - We gather all of the bogus frame data into a single buffer.
 *
 * - We then position a pointer at the end of this buffer and scan
 *   backwards until we encounter the first non-zero byte of data.
 *   This is the end of the received frame. We know we will encounter
 *   some data at the end of the frame because the CRC will always be
 *   there, so even if the sender transmits a packet of all zeros,
 *   we won't be fooled.
 *
 * - We know the size of the actual received frame, so we subtract
 *   that value from the current pointer location. This brings us
 *   to the start of the actual received packet.
 *
 * - We copy this into an mbuf and pass it on, along with the actual
 *   frame length.
 *
 * The performance hit is tremendous, but it beats dropping frames all
 * the time.
 */

#define PN_WHOLEFRAME	(PN_RXSTAT_FIRSTFRAG|PN_RXSTAT_LASTFRAG)
static void pn_rx_bug_war(sc, cur_rx)
	struct pn_softc		*sc;
	struct pn_chain_onefrag	*cur_rx;
{
	struct pn_chain_onefrag	*c;
	unsigned char		*ptr;
	int			total_len;
	u_int32_t		rxstat = 0;

	c = sc->pn_rx_bug_save;
	ptr = sc->pn_rx_buf;
	bzero(ptr, sizeof(PN_RXLEN * 5));

	/* Copy all the bytes from the bogus buffers. */
	while ((c->pn_ptr->pn_status & PN_WHOLEFRAME) != PN_WHOLEFRAME) {
		rxstat = c->pn_ptr->pn_status;
		m_copydata(c->pn_mbuf, 0, PN_RXLEN, ptr);
		ptr += PN_RXLEN - 2; /* round down to 32-bit boundary */
		if (c == cur_rx)
			break;
		if (rxstat & PN_RXSTAT_LASTFRAG)
			break;
		c->pn_ptr->pn_status = PN_RXSTAT;
		c->pn_ptr->pn_ctl = PN_RXCTL_RLINK | PN_RXLEN;
		bzero((char *)mtod(c->pn_mbuf, char *), MCLBYTES);
		c = c->pn_nextdesc;
	}

	/* Find the length of the actual receive frame. */
	total_len = PN_RXBYTES(rxstat);

	/* Scan backwards until we hit a non-zero byte. */
	while(*ptr == 0x00)
		ptr--;

	/* Round off. */
	if ((u_int32_t)(ptr) & 0x3)
		ptr -= 1;

	/* Now find the start of the frame. */
	ptr -= total_len;
	if (ptr < sc->pn_rx_buf)
		ptr = sc->pn_rx_buf;

	/*
	 * Now copy the salvaged frame to the last mbuf and fake up
	 * the status word to make it look like a successful
 	 * frame reception.
	 */
	m_copyback(cur_rx->pn_mbuf, 0, total_len, ptr);
	cur_rx->pn_mbuf->m_len = c->pn_mbuf->m_pkthdr.len = MCLBYTES;
	cur_rx->pn_ptr->pn_status |= PN_RXSTAT_FIRSTFRAG;

	return;
}
#endif

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void pn_rxeof(sc)
	struct pn_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct pn_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->pn_cdata.pn_rx_head->pn_ptr->pn_status) &
							PN_RXSTAT_OWN)) {
#ifdef __alpha__
		struct mbuf		*m0 = NULL;
#endif
		cur_rx = sc->pn_cdata.pn_rx_head;
		sc->pn_cdata.pn_rx_head = cur_rx->pn_nextdesc;

#ifdef PN_RX_BUG_WAR
		/*
		 * XXX The PNIC has a nasty receiver bug that manifests
	 	 * under certain conditions (sometimes only in promiscuous
		 * mode, sometimes only on slow machines even when not in
		 * promiscuous mode). We have to keep an eye out for the
		 * failure condition and employ a workaround to recover
		 * any mangled frames.
		 */
		if (sc->pn_rx_war) {
			if ((rxstat & PN_WHOLEFRAME) != PN_WHOLEFRAME) {
				if (rxstat & PN_RXSTAT_FIRSTFRAG)
					sc->pn_rx_bug_save = cur_rx;
				if ((rxstat & PN_RXSTAT_LASTFRAG) == 0)
					continue;
				pn_rx_bug_war(sc, cur_rx);
				rxstat = cur_rx->pn_ptr->pn_status;
			}
		}
#endif

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & PN_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			if (rxstat & PN_RXSTAT_COLLSEEN)
				ifp->if_collisions++;
			cur_rx->pn_ptr->pn_status = PN_RXSTAT;
			cur_rx->pn_ptr->pn_ctl = PN_RXCTL_RLINK | PN_RXLEN;
			bzero((char *)mtod(cur_rx->pn_mbuf, char *), MCLBYTES);
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->pn_mbuf;
		total_len = PN_RXBYTES(cur_rx->pn_ptr->pn_status);

		/* Trim off the CRC. */
		total_len -= ETHER_CRC_LEN;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (pn_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->pn_ptr->pn_status = PN_RXSTAT;
			cur_rx->pn_ptr->pn_ctl = PN_RXCTL_RLINK | PN_RXLEN;
			bzero((char *)mtod(cur_rx->pn_mbuf, char *), MCLBYTES);
			continue;
		}

#ifdef __alpha__
		/*
		 * Grrrr! On the alpha platform, the start of the
		 * packet data must be longword aligned so that ip_input()
		 * doesn't perform any unaligned accesses when it tries
		 * to fiddle with the IP header. But the PNIC is stupid
		 * and wants RX buffers to start on longword boundaries.
		 * So we can't just shift the DMA address over a few
		 * bytes to alter the payload alignment. Instead, we
		 * have to chop out ethernet and IP header parts of
		 * the packet and place then in a separate mbuf with
		 * the alignment fixed up properly.
		 *
		 * As if this chip wasn't broken enough already.
		 */
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			cur_rx->pn_ptr->pn_status = PN_RXSTAT;
			cur_rx->pn_ptr->pn_ctl = PN_RXCTL_RLINK | PN_RXLEN;
			bzero((char *)mtod(cur_rx->pn_mbuf, char *), MCLBYTES);
			continue;
		}

		m0->m_data += 2;
		if (total_len <= (MHLEN - 2)) {
			bcopy(mtod(m, caddr_t), mtod(m0, caddr_t), total_len);
			m_freem(m);
			m = m0;
			m->m_pkthdr.len = m->m_len = total_len;
		} else {
			bcopy(mtod(m, caddr_t), mtod(m0, caddr_t), (MHLEN - 2));
			m->m_len = total_len - (MHLEN - 2);
			m->m_data += (MHLEN - 2);
			m0->m_next = m;
			m0->m_len = (MHLEN - 2);
			m = m0;
			m->m_pkthdr.len = total_len;
		}
#else
		m->m_pkthdr.len = m->m_len = total_len;
#endif
		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet, but
		 * don't pass it up to the ether_input() layer unless it's
		 * a broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		if (ifp->if_bpf) {
			bpf_mtap(ifp, m);
			if (ifp->if_flags & IFF_PROMISC &&
				(bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
						ETHER_ADDR_LEN) &&
					(eh->ether_dhost[0] & 1) == 0)) {
				m_freem(m);
				continue;
			}
		}
#endif
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	return;
}

void pn_rxeoc(sc)
	struct pn_softc		*sc;
{

	pn_rxeof(sc);
	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_RX_ON);
	CSR_WRITE_4(sc, PN_RXADDR, vtophys(sc->pn_cdata.pn_rx_head->pn_ptr));
	PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_RX_ON);
	CSR_WRITE_4(sc, PN_RXSTART, 0xFFFFFFFF);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void pn_txeof(sc)
	struct pn_softc		*sc;
{
	struct pn_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->pn_cdata.pn_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->pn_cdata.pn_tx_head->pn_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->pn_cdata.pn_tx_head;
		txstat = PN_TXSTATUS(cur_tx);

		if (txstat & PN_TXSTAT_OWN)
			break;

		if (txstat & PN_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & PN_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & PN_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & PN_TXSTAT_COLLCNT) >> 3;


		ifp->if_opackets++;
		m_freem(cur_tx->pn_mbuf);
		cur_tx->pn_mbuf = NULL;

		if (sc->pn_cdata.pn_tx_head == sc->pn_cdata.pn_tx_tail) {
			sc->pn_cdata.pn_tx_head = NULL;
			sc->pn_cdata.pn_tx_tail = NULL;
			break;
		}

		sc->pn_cdata.pn_tx_head = cur_tx->pn_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void pn_txeoc(sc)
	struct pn_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->pn_cdata.pn_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->pn_cdata.pn_tx_tail = NULL;
		if (sc->pn_want_auto) {
			if (sc->pn_pinfo == NULL)
				pn_autoneg(sc, PN_FLAG_SCHEDDELAY, 1);
			else
				pn_autoneg_mii(sc, PN_FLAG_SCHEDDELAY, 1);
		}

	}

	return;
}

static void pn_intr(arg)
	void			*arg;
{
	struct pn_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts. */
	if (!(ifp->if_flags & IFF_UP)) {
		pn_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, PN_IMR, 0x00000000);

	for (;;) {
		status = CSR_READ_4(sc, PN_ISR);
		if (status)
			CSR_WRITE_4(sc, PN_ISR, status);

		if ((status & PN_INTRS) == 0)
			break;

		if (status & PN_ISR_RX_OK)
			pn_rxeof(sc);

		if ((status & PN_ISR_RX_WATCHDOG) || (status & PN_ISR_RX_IDLE)
					|| (status & PN_ISR_RX_NOBUF))
			pn_rxeoc(sc);

		if (status & PN_ISR_TX_OK)
			pn_txeof(sc);

 		if (status & PN_ISR_TX_NOBUF)
			pn_txeoc(sc);

		if (status & PN_ISR_TX_IDLE) {
			pn_txeof(sc);
			if (sc->pn_cdata.pn_tx_head != NULL) {
				PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON);
				CSR_WRITE_4(sc, PN_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & PN_ISR_TX_UNDERRUN) {
			ifp->if_oerrors++;
			pn_txeof(sc);
			if (sc->pn_cdata.pn_tx_head != NULL) {
				PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON);
				CSR_WRITE_4(sc, PN_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & PN_ISR_BUS_ERR) {
			pn_reset(sc);
			pn_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, PN_IMR, PN_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		pn_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int pn_encap(sc, c, m_head)
	struct pn_softc		*sc;
	struct pn_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct pn_desc		*f = NULL;
	int			total_len;
	struct mbuf		*m;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	total_len = 0;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == PN_MAXFRAGS)
				break;
			total_len += m->m_len;
			f = &c->pn_ptr->pn_frag[frag];
			f->pn_ctl = PN_TXCTL_TLINK | m->m_len;
			if (frag == 0) {
				f->pn_ctl |= PN_TXCTL_FIRSTFRAG;
				f->pn_status = 0;
			} else
				f->pn_status = PN_TXSTAT_OWN;
			f->pn_data = vtophys(mtod(m, vm_offset_t));
			f->pn_next = vtophys(&c->pn_ptr->pn_frag[frag + 1]);
			frag++;
		}
	}

	/*
	 * Handle special case: we used up all 16 fragments,
	 * but we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("pn%d: no memory for tx list", sc->pn_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("pn%d: no memory for tx list",
						sc->pn_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->pn_ptr->pn_frag[0];
		f->pn_data = vtophys(mtod(m_new, caddr_t));
		f->pn_ctl = total_len = m_new->m_len;
		f->pn_ctl |= PN_TXCTL_TLINK|PN_TXCTL_FIRSTFRAG;
		frag = 1;
	}


	c->pn_mbuf = m_head;
	c->pn_lastdesc = frag - 1;
	PN_TXCTL(c) |= PN_TXCTL_LASTFRAG|PN_TXCTL_FINT;
	PN_TXNEXT(c) = vtophys(&c->pn_nextdesc->pn_ptr->pn_frag[0]);

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void pn_start(ifp)
	struct ifnet		*ifp;
{
	struct pn_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct pn_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->pn_autoneg) {
		sc->pn_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->pn_cdata.pn_tx_free->pn_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->pn_cdata.pn_tx_free;

	while(sc->pn_cdata.pn_tx_free->pn_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->pn_cdata.pn_tx_free;
		sc->pn_cdata.pn_tx_free = cur_tx->pn_nextdesc;

		/* Pack the data into the descriptor. */
		pn_encap(sc, cur_tx, m_head);

		if (cur_tx != start_tx)
			PN_TXOWN(cur_tx) = PN_TXSTAT_OWN;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->pn_mbuf);
#endif
		PN_TXOWN(cur_tx) = PN_TXSTAT_OWN;
		CSR_WRITE_4(sc, PN_TXSTART, 0xFFFFFFFF);
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	sc->pn_cdata.pn_tx_tail = cur_tx;

	if (sc->pn_cdata.pn_tx_head == NULL)
		sc->pn_cdata.pn_tx_head = start_tx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void pn_init(xsc)
	void			*xsc;
{
	struct pn_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	u_int16_t		phy_bmcr = 0;
	int			s;

	if (sc->pn_autoneg)
		return;

	s = splimp();

	if (sc->pn_pinfo != NULL)
		phy_bmcr = pn_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	pn_stop(sc);
	pn_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
	CSR_WRITE_4(sc, PN_BUSCTL, PN_BUSCTL_MUSTBEONE|PN_BUSCTL_ARBITRATION);
	PN_SETBIT(sc, PN_BUSCTL, PN_BURSTLEN_16LONG);
	switch(sc->pn_cachesize) {
	case 32:
		PN_SETBIT(sc, PN_BUSCTL, PN_CACHEALIGN_32LONG);
		break;
	case 16:
		PN_SETBIT(sc, PN_BUSCTL, PN_CACHEALIGN_16LONG);
		break;
	case 8:
		PN_SETBIT(sc, PN_BUSCTL, PN_CACHEALIGN_8LONG);
		break;
	case 0:
	default:
		PN_SETBIT(sc, PN_BUSCTL, PN_CACHEALIGN_NONE);
		break;
	}

	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_TX_IMMEDIATE);
	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_NO_RXCRC);
	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_HEARTBEAT);
	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_STORENFWD);
	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_TX_BACKOFF);

	PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_TX_THRESH);
	PN_SETBIT(sc, PN_NETCFG, PN_TXTHRESH_72BYTES);

	if (sc->pn_pinfo == NULL) {
		PN_CLRBIT(sc, PN_NETCFG, PN_NETCFG_MIIENB);
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_BACKOFF);
	} else {
		PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_MIIENB);
		PN_SETBIT(sc, PN_ENDEC, PN_ENDEC_JABBERDIS);
	}

	pn_setcfg(sc, sc->ifmedia.ifm_media);

	/* Init circular RX list. */
	if (pn_list_rx_init(sc) == ENOBUFS) {
		printf("pn%d: initialization failed: no "
			"memory for rx buffers\n", sc->pn_unit);
		pn_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	pn_list_tx_init(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, PN_RXADDR, vtophys(sc->pn_cdata.pn_rx_head->pn_ptr));

	/*
	 * Load the RX/multicast filter.
	 */
	pn_setfilt(sc);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, PN_IMR, PN_INTRS);
	CSR_WRITE_4(sc, PN_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	PN_SETBIT(sc, PN_NETCFG, PN_NETCFG_TX_ON|PN_NETCFG_RX_ON);
	CSR_WRITE_4(sc, PN_RXSTART, 0xFFFFFFFF);

	/* Restore state of BMCR */
	if (sc->pn_pinfo != NULL)
		pn_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int pn_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct pn_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		if (sc->pn_pinfo == NULL)
			pn_autoneg(sc, PN_FLAG_SCHEDDELAY, 1);
		else
			pn_autoneg_mii(sc, PN_FLAG_SCHEDDELAY, 1);
	} else {
		if (sc->pn_pinfo == NULL)
			pn_setmode(sc, ifm->ifm_media);
		else
			pn_setmode_mii(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void pn_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct pn_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (sc->pn_pinfo == NULL) {
		if (CSR_READ_4(sc, PN_NETCFG) & PN_NETCFG_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		if (CSR_READ_4(sc, PN_NETCFG) & PN_NETCFG_FULLDUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	if (!(pn_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (pn_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (pn_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = pn_phy_readreg(sc, PHY_LPAR);
	advert = pn_phy_readreg(sc, PHY_ANAR);
	if (advert & PHY_ANAR_100BT4 &&
		ability & PHY_ANAR_100BT4) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_T4;
	} else if (advert & PHY_ANAR_100BTXFULL &&
		ability & PHY_ANAR_100BTXFULL) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_TX|IFM_FDX;
	} else if (advert & PHY_ANAR_100BTXHALF &&
		ability & PHY_ANAR_100BTXHALF) {
		ifmr->ifm_active = IFM_ETHER|IFM_100_TX|IFM_HDX;
	} else if (advert & PHY_ANAR_10BTFULL &&
		ability & PHY_ANAR_10BTFULL) {
		ifmr->ifm_active = IFM_ETHER|IFM_10_T|IFM_FDX;
	} else if (advert & PHY_ANAR_10BTHALF &&
		ability & PHY_ANAR_10BTHALF) {
		ifmr->ifm_active = IFM_ETHER|IFM_10_T|IFM_HDX;
	}

	return;
}

static int pn_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct pn_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			s, error = 0;

	s = splimp();

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			pn_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				pn_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		pn_init(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void pn_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct pn_softc		*sc;

	sc = ifp->if_softc;

	if (sc->pn_autoneg) {
		if (sc->pn_pinfo == NULL)
			pn_autoneg(sc, PN_FLAG_DELAYTIMEO, 1);
		else
			pn_autoneg_mii(sc, PN_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	printf("pn%d: watchdog timeout\n", sc->pn_unit);

	if (!(pn_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("pn%d: no carrier - transceiver cable problem?\n",
								sc->pn_unit);
	pn_stop(sc);
	pn_reset(sc);
	pn_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		pn_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void pn_stop(sc)
	struct pn_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	PN_CLRBIT(sc, PN_NETCFG, (PN_NETCFG_RX_ON|PN_NETCFG_TX_ON));
	CSR_WRITE_4(sc, PN_IMR, 0x00000000);
	CSR_WRITE_4(sc, PN_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, PN_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < PN_RX_LIST_CNT; i++) {
		if (sc->pn_cdata.pn_rx_chain[i].pn_mbuf != NULL) {
			m_freem(sc->pn_cdata.pn_rx_chain[i].pn_mbuf);
			sc->pn_cdata.pn_rx_chain[i].pn_mbuf = NULL;
		}
	}
	bzero((char *)&sc->pn_ldata->pn_rx_list,
		sizeof(sc->pn_ldata->pn_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < PN_TX_LIST_CNT; i++) {
		if (sc->pn_cdata.pn_tx_chain[i].pn_mbuf != NULL) {
			m_freem(sc->pn_cdata.pn_tx_chain[i].pn_mbuf);
			sc->pn_cdata.pn_tx_chain[i].pn_mbuf = NULL;
		}
	}

	bzero((char *)&sc->pn_ldata->pn_tx_list,
		sizeof(sc->pn_ldata->pn_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void pn_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct pn_softc		*sc = (struct pn_softc *)arg;

	pn_stop(sc);

	return;
}

static struct pci_device pn_device = {
	"pn",
	pn_probe,
	pn_attach,
	&pn_count,
	NULL
};
COMPAT_PCI_DRIVER(pn, pn_device);
