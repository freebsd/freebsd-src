/*
 * Copyright (c) 1997, 1998, 1999
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
 * $FreeBSD$
 */

/*
 * SiS 900/SiS 7016 fast ethernet PCI NIC driver. Datasheets are
 * available from http://www.sis.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The SiS 900 is a fairly simple chip. It uses bus master DMA with
 * simple TX and RX descriptors of 3 longwords in size. The receiver
 * has a single perfect filter entry for the station address and a
 * 128-bit multicast hash table. The SiS 900 has a built-in MII-based
 * transceiver while the 7016 requires an external transceiver chip.
 * Both chips offer the standard bit-bang MII interface as well as
 * an enchanced PHY interface which simplifies accessing MII registers.
 *
 * The only downside to this chipset is that RX descriptors must be
 * longword aligned.
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

#define SIS_USEIOSPACE

#include <pci/if_sisreg.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct sis_type sis_devs[] = {
	{ SIS_VENDORID, SIS_DEVICEID_900, "SiS 900 10/100BaseTX" },
	{ SIS_VENDORID, SIS_DEVICEID_7016, "SiS 7016 10/100BaseTX" },
	{ 0, 0, NULL }
};

static struct sis_type sis_phys[] = {
	{ 0, 0, "<MII-compilant physical interface>" },
	{ 0, 0, NULL }
};

static unsigned long		sis_count = 0;
static const char *sis_probe	__P((pcici_t, pcidi_t));
static void sis_attach		__P((pcici_t, int));

static int sis_newbuf		__P((struct sis_softc *,
					struct sis_desc *,
					struct mbuf *));
static int sis_encap		__P((struct sis_softc *,
					struct mbuf *, u_int32_t *));
static void sis_rxeof		__P((struct sis_softc *));
static void sis_rxeoc		__P((struct sis_softc *));
static void sis_txeof		__P((struct sis_softc *));
static void sis_intr		__P((void *));
static void sis_start		__P((struct ifnet *));
static int sis_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void sis_init		__P((void *));
static void sis_stop		__P((struct sis_softc *));
static void sis_watchdog	__P((struct ifnet *));
static void sis_shutdown	__P((int, void *));
static int sis_ifmedia_upd	__P((struct ifnet *));
static void sis_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void sis_delay		__P((struct sis_softc *));
static void sis_eeprom_idle	__P((struct sis_softc *));
static void sis_eeprom_putbyte	__P((struct sis_softc *, int));
static void sis_eeprom_getword	__P((struct sis_softc *, int, u_int16_t *));
static void sis_read_eeprom	__P((struct sis_softc *, caddr_t, int,
							int, int));
static void sis_setmulti	__P((struct sis_softc *));
static u_int32_t sis_calchash	__P((caddr_t));
static void sis_reset		__P((struct sis_softc *));
static int sis_list_rx_init	__P((struct sis_softc *));
static int sis_list_tx_init	__P((struct sis_softc *));

static u_int16_t sis_phy_readreg	__P((struct sis_softc *, int));
static void sis_phy_writereg	__P((struct sis_softc *, int, int));

static void sis_autoneg_xmit	__P((struct sis_softc *));
static void sis_autoneg_mii	__P((struct sis_softc *, int, int));
static void sis_setmode_mii	__P((struct sis_softc *, int));
static void sis_getmode_mii	__P((struct sis_softc *));


#define SIS_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define SIS_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, SIS_EECTL, CSR_READ_4(sc, SIS_EECTL) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, SIS_EECTL, CSR_READ_4(sc, SIS_EECTL) & ~x)

static void sis_delay(sc)
	struct sis_softc	*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, SIS_CSR);

	return;
}

static void sis_eeprom_idle(sc)
	struct sis_softc	*sc;
{
	register int		i;

	SIO_SET(SIS_EECTL_CSEL);
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CLK);
	sis_delay(sc);

	for (i = 0; i < 25; i++) {
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	SIO_CLR(SIS_EECTL_CLK);
	sis_delay(sc);
	SIO_CLR(SIS_EECTL_CSEL);
	sis_delay(sc);
	CSR_WRITE_4(sc, SIS_EECTL, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void sis_eeprom_putbyte(sc, addr)
	struct sis_softc	*sc;
	int			addr;
{
	register int		d, i;

	d = addr | SIS_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(SIS_EECTL_DIN);
		} else {
			SIO_CLR(SIS_EECTL_DIN);
		}
		sis_delay(sc);
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void sis_eeprom_getword(sc, addr, dest)
	struct sis_softc	*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	sis_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CSEL);
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CLK);
	sis_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	sis_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(SIS_EECTL_CLK);
		sis_delay(sc);
		if (CSR_READ_4(sc, SIS_EECTL) & SIS_EECTL_DOUT)
			word |= i;
		sis_delay(sc);
		SIO_CLR(SIS_EECTL_CLK);
		sis_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	sis_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void sis_read_eeprom(sc, dest, off, cnt, swap)
	struct sis_softc	*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		sis_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

static u_int16_t sis_phy_readreg(sc, reg)
	struct sis_softc	*sc;
	int			reg;
{
	int			i, val;

	if (sc->sis_type == SIS_TYPE_900 && sc->sis_phy_addr != 0)
		return(0);

	CSR_WRITE_4(sc, SIS_PHYCTL, (sc->sis_phy_addr << 11) |
	   (reg << 6) | SIS_PHYOP_READ);
	SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
			break;
	}

	if (i == SIS_TIMEOUT) {
		printf("sis%d: PHY failed to come ready\n", sc->sis_unit);
		return(0);
	}

	val = (CSR_READ_4(sc, SIS_PHYCTL) >> 16) & 0xFFFF;

	if (val == 0xFFFF)
		return(0);

	return(val);
}

static void sis_phy_writereg(sc, reg, data)
	struct sis_softc	*sc;
	int			reg, data;
{
	int			i;

	if (sc->sis_type == SIS_TYPE_900 && sc->sis_phy_addr != 0)
		return;

	CSR_WRITE_4(sc, SIS_PHYCTL, (data << 16) | (sc->sis_phy_addr << 11) |
	    (reg << 6) | SIS_PHYOP_WRITE);
	SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: PHY failed to come ready\n", sc->sis_unit);

	return;
}

static void sis_setcfg(sc, media)
	struct sis_softc	*sc;
	u_int16_t		media;
{
	if (media & PHY_BMCR_DUPLEX) {
		SIS_SETBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_SETBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	} else {
		SIS_CLRBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_CLRBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	}

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void sis_autoneg_xmit(sc)
	struct sis_softc		*sc;
{
	u_int16_t		phy_sts;

	sis_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(sis_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = sis_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	sis_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void sis_autoneg_mii(sc, flag, verbose)
	struct sis_softc		*sc;
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
	phy_sts = sis_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("sis%d: autonegotiation not supported\n",
							sc->sis_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case SIS_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		sis_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case SIS_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise sis_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->sis_cdata.sis_tx_cnt) {
			sc->sis_want_auto = 1;
			return;
		}
		sis_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->sis_autoneg = 1;
		sc->sis_want_auto = 0;
		return;
		break;
	case SIS_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->sis_autoneg = 0;
		break;
	default:
		printf("sis%d: invalid autoneg flag: %d\n", sc->sis_unit, flag);
		return;
	}

	if (sis_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("sis%d: autoneg complete, ", sc->sis_unit);
		phy_sts = sis_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("sis%d: autoneg not complete, ", sc->sis_unit);
	}

	media = sis_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (sis_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = sis_phy_readreg(sc, PHY_ANAR);
		ability = sis_phy_readreg(sc, PHY_LPAR);

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
		sis_phy_writereg(sc, PHY_BMCR, media);
		sis_setcfg(sc, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	sis_init(sc);

	if (sc->sis_tx_pend) {
		sc->sis_autoneg = 0;
		sc->sis_tx_pend = 0;
		sis_start(ifp);
	}

	return;
}

static void sis_getmode_mii(sc)
	struct sis_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = sis_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("sis%d: PHY status word: %x\n", sc->sis_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("sis%d: 10Mbps half-duplex mode supported\n",
								sc->sis_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("sis%d: 10Mbps full-duplex mode supported\n",
								sc->sis_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("sis%d: 100Mbps half-duplex mode supported\n",
								sc->sis_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("sis%d: 100Mbps full-duplex mode supported\n",
								sc->sis_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("sis%d: 100baseT4 mode supported\n", sc->sis_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("sis%d: forcing on autoneg support for BT4\n",
							 sc->sis_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("sis%d: autoneg supported\n", sc->sis_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void sis_setmode_mii(sc, media)
	struct sis_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->sis_autoneg) {
		printf("sis%d: canceling autoneg session\n", sc->sis_unit);
		ifp->if_timer = sc->sis_autoneg = sc->sis_want_auto = 0;
		bmcr = sis_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		sis_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("sis%d: selecting MII, ", sc->sis_unit);

	bmcr = sis_phy_readreg(sc, PHY_BMCR);

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

	sis_phy_writereg(sc, PHY_BMCR, bmcr);
	sis_setcfg(sc, bmcr);

	return;
}

static u_int32_t sis_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		crc, carry; 
	int			i, j;
	u_int8_t		c;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (i = 0; i < 6; i++) {
		c = *(addr + i);
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/* return the filter bit position */
	return((crc >> 25) & 0x0000007F);
}

static void sis_setmulti(sc)
	struct sis_softc	*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i, filtsave;

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);
		return;
	}

	SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);

	filtsave = CSR_READ_4(sc, SIS_RXFILT_CTL);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 8; i++) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, (4 + ((i * 16) >> 4)) << 16);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA, 0);
	}

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = sis_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, (4 + (h >> 4)) << 16);
		SIS_SETBIT(sc, SIS_RXFILT_DATA, (1 << (h & 0xF)));
	}

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave);

	return;
}

static void sis_reset(sc)
	struct sis_softc	*sc;
{
	register int		i;

	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RESET);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_CSR) & SIS_CSR_RESET))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: reset never completed\n", sc->sis_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for a SiS chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
sis_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct sis_type		*t;

	t = sis_devs;

	while(t->sis_name != NULL) {
		if ((device_id & 0xFFFF) == t->sis_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->sis_did) {
			return(t->sis_name);
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
sis_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef SIS_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	struct sis_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
	u_int16_t		did;

	s = splimp();

	sc = malloc(sizeof(struct sis_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("sis%d: no memory for softc struct!\n", unit);
		goto fail;
	}
	bzero(sc, sizeof(struct sis_softc));

	did = (pci_conf_read(config_id, SIS_PCI_VENDOR_ID) >> 16) & 0xFFFF;
	if (did == SIS_DEVICEID_900)
		sc->sis_type = SIS_TYPE_900;
	if (did == SIS_DEVICEID_7016)
		sc->sis_type = SIS_TYPE_7016;

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, SIS_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, SIS_PCI_PWRMGMTCTRL);
		if (command & SIS_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, SIS_PCI_LOIO);
			membase = pci_conf_read(config_id, SIS_PCI_LOMEM);
			irq = pci_conf_read(config_id, SIS_PCI_INTLINE);

			/* Reset the power state. */
			printf("sis%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & SIS_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, SIS_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, SIS_PCI_LOIO, iobase);
			pci_conf_write(config_id, SIS_PCI_LOMEM, membase);
			pci_conf_write(config_id, SIS_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef SIS_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("sis%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, SIS_PCI_LOIO,
					(u_short *)&(sc->sis_bhandle))) {
		printf ("sis%d: couldn't map ports\n", unit);
		goto fail;
        }
#ifdef __i386__
	sc->sis_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->sis_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("sis%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, SIS_PCI_LOMEM, &vbase, &pbase)) {
		printf ("sis%d: couldn't map memory\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->sis_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->sis_btag = ALPHA_BUS_SPACE_MEM;
#endif
	sc->sis_bhandle = vbase;
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, sis_intr, sc, &net_imask)) {
		printf("sis%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	sis_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	sis_read_eeprom(sc, (caddr_t)&eaddr, SIS_EE_NODEADDR, 3, 0);

	/*
	 * A SiS chip was detected. Inform the world.
	 */
	printf("sis%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->sis_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->sis_ldata = contigmalloc(sizeof(struct sis_list_data), M_DEVBUF,
	    M_NOWAIT, 0x100000, 0xffffffff, PAGE_SIZE, 0);

	if (sc->sis_ldata == NULL) {
		printf("sis%d: no memory for list buffers!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}
	bzero(sc->sis_ldata, sizeof(struct sis_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "sis";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sis_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sis_start;
	ifp->if_watchdog = sis_watchdog;
	ifp->if_init = sis_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = SIS_TX_LIST_CNT - 1;

	if (bootverbose)
		printf("sis%d: probing for a PHY\n", sc->sis_unit);
	for (i = SIS_PHYADDR_MIN; i < SIS_PHYADDR_MAX + 1; i++) {
		if (bootverbose)
			printf("sis%d: checking address: %d\n",
						sc->sis_unit, i);
		sc->sis_phy_addr = i;
		sis_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(sis_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = sis_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = sis_phy_readreg(sc, PHY_VENID);
		phy_did = sis_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("sis%d: found PHY at address %d, ",
				sc->sis_unit, sc->sis_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
			phy_vid, phy_did);
		p = sis_phys;
		while(p->sis_vid) {
			if (phy_vid == p->sis_vid &&
				(phy_did | 0x000F) == p->sis_did) {
				sc->sis_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->sis_pinfo == NULL)
			sc->sis_pinfo = &sis_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("sis%d: PHY type: %s\n",
				sc->sis_unit, sc->sis_pinfo->sis_name);
	} else {
		printf("sis%d: MII without any phy!\n", sc->sis_unit);
		free(sc->sis_ldata, M_DEVBUF);
		free(sc, M_DEVBUF);
		goto fail;
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, sis_ifmedia_upd, sis_ifmedia_sts);

	sis_getmode_mii(sc);
	sis_autoneg_mii(sc, SIS_FLAG_FORCEDELAY, 1);

	media = sc->ifmedia.ifm_media;
	sis_stop(sc);

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(sis_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}


/*
 * Initialize the transmit descriptors.
 */
static int sis_list_tx_init(sc)
	struct sis_softc	*sc;
{
	struct sis_list_data	*ld;
	struct sis_ring_data	*cd;
	int			i;

	cd = &sc->sis_cdata;
	ld = sc->sis_ldata;

	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		if (i == (SIS_TX_LIST_CNT - 1)) {
			ld->sis_tx_list[i].sis_nextdesc =
			    &ld->sis_tx_list[0];
			ld->sis_tx_list[i].sis_next =
			    vtophys(&ld->sis_tx_list[0]);
		} else {
			ld->sis_tx_list[i].sis_nextdesc =
			    &ld->sis_tx_list[i + 1];
			ld->sis_tx_list[i].sis_next =
			    vtophys(&ld->sis_tx_list[i + 1]);
		}
		ld->sis_tx_list[i].sis_mbuf = NULL;
		ld->sis_tx_list[i].sis_ptr = 0;
		ld->sis_tx_list[i].sis_ctl = 0;
	}

	cd->sis_tx_prod = cd->sis_tx_cons = cd->sis_tx_cnt = 0;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int sis_list_rx_init(sc)
	struct sis_softc	*sc;
{
	struct sis_list_data	*ld;
	struct sis_ring_data	*cd;
	int			i;

	ld = sc->sis_ldata;
	cd = &sc->sis_cdata;

	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sis_newbuf(sc, &ld->sis_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (SIS_RX_LIST_CNT - 1)) {
			ld->sis_rx_list[i].sis_nextdesc =
			    &ld->sis_rx_list[0];
			ld->sis_rx_list[i].sis_next =
			    vtophys(&ld->sis_rx_list[0]);
		} else {
			ld->sis_rx_list[i].sis_nextdesc =
			    &ld->sis_rx_list[i + 1];
			ld->sis_rx_list[i].sis_next =
			    vtophys(&ld->sis_rx_list[i + 1]);
		}
	}

	cd->sis_rx_prod = 0;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int sis_newbuf(sc, c, m)
	struct sis_softc	*sc;
	struct sis_desc		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sis%d: no memory for rx list "
			    "-- packet dropped!\n", sc->sis_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("sis%d: no memory for rx list "
			    "-- packet dropped!\n", sc->sis_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->sis_mbuf = m_new;
	c->sis_ptr = vtophys(mtod(m_new, caddr_t));
	c->sis_ctl = SIS_RXLEN;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void sis_rxeof(sc)
	struct sis_softc	*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct sis_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;
	i = sc->sis_cdata.sis_rx_prod;

	while(SIS_OWNDESC(&sc->sis_ldata->sis_rx_list[i])) {
		struct mbuf		*m0 = NULL;

		cur_rx = &sc->sis_ldata->sis_rx_list[i];
		rxstat = cur_rx->sis_rxstat;
		m = cur_rx->sis_mbuf;
		cur_rx->sis_mbuf = NULL;
		total_len = SIS_RXBYTES(cur_rx);
		SIS_INC(i, SIS_RX_LIST_CNT);

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (!(rxstat & SIS_CMDSTS_PKT_OK)) {
			ifp->if_ierrors++;
			if (rxstat & SIS_RXSTAT_COLL)
				ifp->if_collisions++;
			sis_newbuf(sc, cur_rx, m);
			continue;
		}

		/* No errors; receive the packet. */	
		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		sis_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
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
			    ETHER_ADDR_LEN) && !(eh->ether_dhost[0] & 1))) {
				m_freem(m);
				continue;
			}
		}
#endif
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	sc->sis_cdata.sis_rx_prod = i;

	return;
}

void sis_rxeoc(sc)
	struct sis_softc	*sc;
{
	sis_rxeof(sc);
	sis_init(sc);
	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void sis_txeof(sc)
	struct sis_softc	*sc;
{
	struct sis_desc		*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->sis_cdata.sis_tx_cons;
	while (idx != sc->sis_cdata.sis_tx_prod) {
		cur_tx = &sc->sis_ldata->sis_tx_list[idx];

		if (SIS_OWNDESC(cur_tx))
			break;

		if (cur_tx->sis_ctl & SIS_CMDSTS_MORE) {
			sc->sis_cdata.sis_tx_cnt--;
			SIS_INC(idx, SIS_TX_LIST_CNT);
			continue;
		}

		if (!(cur_tx->sis_ctl & SIS_CMDSTS_PKT_OK)) {
			ifp->if_oerrors++;
			if (cur_tx->sis_txstat & SIS_TXSTAT_EXCESSCOLLS)
				ifp->if_collisions++;
			if (cur_tx->sis_txstat & SIS_TXSTAT_OUTOFWINCOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=
		    (cur_tx->sis_txstat & SIS_TXSTAT_COLLCNT) >> 16;

		ifp->if_opackets++;
		if (cur_tx->sis_mbuf != NULL) {
			m_freem(cur_tx->sis_mbuf);
			cur_tx->sis_mbuf = NULL;
		}

		sc->sis_cdata.sis_tx_cnt--;
		SIS_INC(idx, SIS_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->sis_cdata.sis_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void sis_intr(arg)
	void			*arg;
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		sis_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 0);

	for (;;) {
		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, SIS_ISR);

		if ((status & SIS_INTRS) == 0)
			break;

		if ((status & SIS_ISR_TX_OK) ||
		    (status & SIS_ISR_TX_ERR) ||
		    (status & SIS_ISR_TX_IDLE))
			sis_txeof(sc);

		if (status & SIS_ISR_RX_OK)
			sis_rxeof(sc);

		if ((status & SIS_ISR_RX_ERR) ||
		    (status & SIS_ISR_RX_OFLOW)) {
			sis_rxeoc(sc);
		}

		if (status & SIS_ISR_SYSERR) {
			sis_reset(sc);
			sis_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 1);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int sis_encap(sc, m_head, txidx)
	struct sis_softc	*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct sis_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	cur = frag = *txidx;

	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((SIS_TX_LIST_CNT -
			    (sc->sis_cdata.sis_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->sis_ldata->sis_tx_list[frag];
			f->sis_ctl = SIS_CMDSTS_MORE | m->m_len;
			f->sis_ptr = vtophys(mtod(m, vm_offset_t));
			if (cnt != 0)
				f->sis_ctl |= SIS_CMDSTS_OWN;
			cur = frag;
			SIS_INC(frag, SIS_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->sis_ldata->sis_tx_list[cur].sis_mbuf = m_head;
	sc->sis_ldata->sis_tx_list[cur].sis_ctl &= ~SIS_CMDSTS_MORE;
	sc->sis_ldata->sis_tx_list[*txidx].sis_ctl |= SIS_CMDSTS_OWN;
	sc->sis_cdata.sis_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void sis_start(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx;

	sc = ifp->if_softc;

	if (sc->sis_autoneg)
		return;

	idx = sc->sis_cdata.sis_tx_prod;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	while(sc->sis_ldata->sis_tx_list[idx].sis_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (sis_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
#endif
	}

	/* Transmit */
	sc->sis_cdata.sis_tx_prod = idx;
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void sis_init(xsc)
	void			*xsc;
{
	struct sis_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s;
	u_int16_t		phy_bmcr;

	if (sc->sis_autoneg)
		return;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	sis_stop(sc);

	phy_bmcr = sis_phy_readreg(sc, PHY_BMCR);

	/* Set MAC address */
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR0);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[0]);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR1);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[1]);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR2);
	CSR_WRITE_4(sc, SIS_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[2]);

	/* Init circular RX list. */
	if (sis_list_rx_init(sc) == ENOBUFS) {
		printf("sis%d: initialization failed: no "
			"memory for rx buffers\n", sc->sis_unit);
		sis_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	sis_list_tx_init(sc);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLPHYS);
	} else {
		SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLPHYS);
	}

	/*
	 * Set the capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_BROAD);
	} else {
		SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	sis_setmulti(sc);

	/* Turn the receive filter on */
	SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ENABLE);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, SIS_RX_LISTPTR,
	    vtophys(&sc->sis_ldata->sis_rx_list[0]));
	CSR_WRITE_4(sc, SIS_TX_LISTPTR,
	    vtophys(&sc->sis_ldata->sis_tx_list[0]));

	/* Set RX configuration */
	CSR_WRITE_4(sc, SIS_RX_CFG, SIS_RXCFG);
	/* Set TX configuration */
	CSR_WRITE_4(sc, SIS_TX_CFG, SIS_TXCFG);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, SIS_IMR, SIS_INTRS);
	CSR_WRITE_4(sc, SIS_IER, 1);

	/* Enable receiver and transmitter. */
	SIS_CLRBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RX_ENABLE);

	sis_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int sis_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		sis_autoneg_mii(sc, SIS_FLAG_SCHEDDELAY, 1);
	else {
		sis_setmode_mii(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void sis_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sis_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (!(sis_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (sis_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (sis_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = sis_phy_readreg(sc, PHY_LPAR);
	advert = sis_phy_readreg(sc, PHY_ANAR);
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

static int sis_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sis_softc	*sc = ifp->if_softc;
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
			sis_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sis_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sis_setmulti(sc);
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

static void sis_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;

	sc = ifp->if_softc;

	if (sc->sis_autoneg) {
		sis_autoneg_mii(sc, SIS_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	printf("sis%d: watchdog timeout\n", sc->sis_unit);

	sis_stop(sc);
	sis_reset(sc);
	sis_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void sis_stop(sc)
	struct sis_softc	*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	CSR_WRITE_4(sc, SIS_IER, 0);
	CSR_WRITE_4(sc, SIS_IMR, 0);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, 0);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sc->sis_ldata->sis_rx_list[i].sis_mbuf != NULL) {
			m_freem(sc->sis_ldata->sis_rx_list[i].sis_mbuf);
			sc->sis_ldata->sis_rx_list[i].sis_mbuf = NULL;
		}
	}
	bzero((char *)&sc->sis_ldata->sis_rx_list,
		sizeof(sc->sis_ldata->sis_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		if (sc->sis_ldata->sis_tx_list[i].sis_mbuf != NULL) {
			m_freem(sc->sis_ldata->sis_tx_list[i].sis_mbuf);
			sc->sis_ldata->sis_tx_list[i].sis_mbuf = NULL;
		}
	}

	bzero((char *)&sc->sis_ldata->sis_tx_list,
		sizeof(sc->sis_ldata->sis_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void sis_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct sis_softc	*sc;

	sc = arg;

	sis_reset(sc);
	sis_stop(sc);

	return;
}

static struct pci_device sis_device = {
	"sis",
	sis_probe,
	sis_attach,
	&sis_count,
	NULL
};
#ifdef COMPAT_PCI_DRIVER
COMPAT_PCI_DRIVER(sis, sis_device);
#else
DATA_SET(pcidevice_set, sis_device);
#endif
