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
 *	$Id: if_sf.c,v 1.11 1999/07/24 21:13:38 wpaul Exp $
 */

/*
 * Adaptec AIC-6915 "Starfire" PCI fast ethernet driver for FreeBSD.
 * Programming manual is available from www.adaptec.com.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Department of Electical Engineering
 * Columbia University, New York City
 */

/*
 * The Adaptec AIC-6915 "Starfire" is a 64-bit 10/100 PCI ethernet
 * controller designed with flexibility and reducing CPU load in mind.
 * The Starfire offers high and low priority buffer queues, a
 * producer/consumer index mechanism and several different buffer
 * queue and completion queue descriptor types. Any one of a number
 * of different driver designs can be used, depending on system and
 * OS requirements. This driver makes use of type0 transmit frame
 * descriptors (since BSD fragments packets across an mbuf chain)
 * and two RX buffer queues prioritized on size (one queue for small
 * frames that will fit into a single mbuf, another with full size
 * mbuf clusters for everything else). The producer/consumer indexes
 * and completion queues are also used.
 *
 * One downside to the Starfire has to do with alignment: buffer
 * queues must be aligned on 256-byte boundaries, and receive buffers
 * must be aligned on longword boundaries. The receive buffer alignment
 * causes problems on the Alpha platform, where the packet payload
 * should be longword aligned. There is no simple way around this.
 *
 * For receive filtering, the Starfire offers 16 perfect filter slots
 * and a 512-bit hash table.
 *
 * The Starfire has no internal transceiver, relying instead on an
 * external MII-based transceiver. Accessing registers on external
 * PHYs is done through a special register map rather than with the
 * usual bitbang MDIO method.
 *
 * Acesssing the registers on the Starfire is a little tricky. The
 * Starfire has a 512K internal register space. When programmed for
 * PCI memory mapped mode, the entire register space can be accessed
 * directly. However in I/O space mode, only 256 bytes are directly
 * mapped into PCI I/O space. The other registers can be accessed
 * indirectly using the SF_INDIRECTIO_ADDR and SF_INDIRECTIO_DATA
 * registers inside the 256-byte I/O window.
 */

#include "bpf.h"

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

#if NBPF > 0
#include <net/bpf.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define SF_USEIOSPACE

/* #define SF_BACKGROUND_AUTONEG */

#include <pci/if_sfreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_sf.c,v 1.11 1999/07/24 21:13:38 wpaul Exp $";
#endif

static struct sf_type sf_devs[] = {
	{ AD_VENDORID, AD_DEVICEID_STARFIRE,
		"Adaptec AIC-6915 10/100BaseTX" },
	{ 0, 0, NULL }
};

static struct sf_type sf_phys[] = {
	{ 0, 0, "<MII-compliant physical interface>" }
};

static int sf_probe		__P((device_t));
static int sf_attach		__P((device_t));
static int sf_detach		__P((device_t));
static void sf_intr		__P((void *));
static void sf_stats_update	__P((void *));
static void sf_rxeof		__P((struct sf_softc *));
static void sf_txeof		__P((struct sf_softc *));
static int sf_encap		__P((struct sf_softc *,
					struct sf_tx_bufdesc_type0 *,
					struct mbuf *));
static void sf_start		__P((struct ifnet *));
static int sf_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void sf_init		__P((void *));
static void sf_stop		__P((struct sf_softc *));
static void sf_watchdog		__P((struct ifnet *));
static void sf_shutdown		__P((device_t));
static int sf_ifmedia_upd	__P((struct ifnet *));
static void sf_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));
static void sf_reset		__P((struct sf_softc *));
static int sf_init_rx_ring	__P((struct sf_softc *));
static void sf_init_tx_ring	__P((struct sf_softc *));
static int sf_newbuf		__P((struct sf_softc *,
					struct sf_rx_bufdesc_type0 *,
					struct mbuf *));
static void sf_setmulti		__P((struct sf_softc *));
static int sf_setperf		__P((struct sf_softc *, int, caddr_t));
static int sf_sethash		__P((struct sf_softc *, caddr_t, int));
#ifdef notdef
static int sf_setvlan		__P((struct sf_softc *, int, u_int32_t));
#endif

static u_int8_t sf_read_eeprom	__P((struct sf_softc *, int));
static u_int32_t sf_calchash	__P((caddr_t));

static int sf_phy_readreg	__P((struct sf_softc *, int));
static void sf_phy_writereg	__P((struct sf_softc *, int, int));
static void sf_autoneg_xmit	__P((struct sf_softc *));
static void sf_autoneg_mii	__P((struct sf_softc *, int, int));
static void sf_getmode_mii	__P((struct sf_softc *));
static void sf_setmode_mii	__P((struct sf_softc *, int));

static u_int32_t csr_read_4	__P((struct sf_softc *, int));
static void csr_write_4		__P((struct sf_softc *, int, u_int32_t));

#ifdef SF_USEIOSPACE
#define SF_RES			SYS_RES_IOPORT
#define SF_RID			SF_PCI_LOIO
#else
#define SF_RES			SYS_RES_MEMORY
#define SF_RID			SF_PCI_LOMEM
#endif

static device_method_t sf_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sf_probe),
	DEVMETHOD(device_attach,	sf_attach),
	DEVMETHOD(device_detach,	sf_detach),
	DEVMETHOD(device_shutdown,	sf_shutdown),
	{ 0, 0 }
};

static driver_t sf_driver = {
	"sf",
	sf_methods,
	sizeof(struct sf_softc),
};

static devclass_t sf_devclass;

DRIVER_MODULE(sf, pci, sf_driver, sf_devclass, 0, 0);

#define SF_SETBIT(sc, reg, x)	\
	csr_write_4(sc, reg, csr_read_4(sc, reg) | x)

#define SF_CLRBIT(sc, reg, x)				\
	csr_write_4(sc, reg, csr_read_4(sc, reg) & ~x)

static u_int32_t csr_read_4(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int32_t		val;

#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	val = CSR_READ_4(sc, SF_INDIRECTIO_DATA);
#else
	val = CSR_READ_4(sc, (reg + SF_RMAP_INTREG_BASE));
#endif

	return(val);
}

static u_int8_t sf_read_eeprom(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int8_t		val;

	val = (csr_read_4(sc, SF_EEADDR_BASE +
	    (reg & 0xFFFFFFFC)) >> (8 * (reg & 3))) & 0xFF;

	return(val);
}

static void csr_write_4(sc, reg, val)
	struct sf_softc		*sc;
	int			reg;
	u_int32_t		val;
{
#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	CSR_WRITE_4(sc, SF_INDIRECTIO_DATA, val);
#else
	CSR_WRITE_4(sc, (reg + SF_RMAP_INTREG_BASE), val);
#endif
	return;
}

static u_int32_t sf_calchash(addr)
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
	return(crc >> 23 & 0x1FF);
}

/*
 * Copy the address 'mac' into the perfect RX filter entry at
 * offset 'idx.' The perfect filter only has 16 entries so do
 * some sanity tests.
 */
static int sf_setperf(sc, idx, mac)
	struct sf_softc		*sc;
	int			idx;
	caddr_t			mac;
{
	u_int16_t		*p;

	if (idx < 0 || idx > SF_RXFILT_PERFECT_CNT)
		return(EINVAL);

	if (mac == NULL)
		return(EINVAL);

	p = (u_int16_t *)mac;

	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP), htons(p[2]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 4, htons(p[1]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 8, htons(p[0]));

	return(0);
}

/*
 * Set the bit in the 512-bit hash table that corresponds to the
 * specified mac address 'mac.' If 'prio' is nonzero, update the
 * priority hash table instead of the filter hash table.
 */
static int sf_sethash(sc, mac, prio)
	struct sf_softc		*sc;
	caddr_t			mac;
	int			prio;
{
	u_int32_t		h = 0;

	if (mac == NULL)
		return(EINVAL);

	h = sf_calchash(mac);

	if (prio) {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_PRIOOFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	} else {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_ADDROFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	}

	return(0);
}

#ifdef notdef
/*
 * Set a VLAN tag in the receive filter.
 */
static int sf_setvlan(sc, idx, vlan)
	struct sf_softc		*sc;
	int			idx;
	u_int32_t		vlan;
{
	if (idx < 0 || idx >> SF_RXFILT_HASH_CNT)
		return(EINVAL);

	csr_write_4(sc, SF_RXFILT_HASH_BASE +
	    (idx * SF_RXFILT_HASH_SKIP) + SF_RXFILT_HASH_VLANOFF, vlan);

	return(0);
}
#endif

static int sf_phy_readreg(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	int			i;
	u_int32_t		val = 0;

	for (i = 0; i < SF_TIMEOUT; i++) {
		val = csr_read_4(sc, SF_PHY_REG(sc->sf_phy_addr, reg));
		if (val & SF_MII_DATAVALID)
			break;
	}

	if (i == SF_TIMEOUT)
		return(0);

	if ((val & 0x0000FFFF) == 0xFFFF)
		return(0);

	return(val & 0x0000FFFF);
}

static void sf_phy_writereg(sc, reg, val)
	struct sf_softc		*sc;
	int			reg, val;
{
	int			i;
	int			busy;

	csr_write_4(sc, SF_PHY_REG(sc->sf_phy_addr, reg), val);

	for (i = 0; i < SF_TIMEOUT; i++) {
		busy = csr_read_4(sc, SF_PHY_REG(sc->sf_phy_addr, reg));
		if (!(busy & SF_MII_BUSY))
			break;
	}

	return;
}

static void sf_setmulti(sc)
	struct sf_softc		*sc;
{
	struct ifnet		*ifp;
	int			i;
	struct ifmultiaddr	*ifma;
	u_int8_t		dummy[] = { 0, 0, 0, 0, 0, 0 };

	ifp = &sc->arpcom.ac_if;

	/* First zot all the existing filters. */
	for (i = 1; i < SF_RXFILT_PERFECT_CNT; i++)
		sf_setperf(sc, i, (char *)&dummy);
	for (i = SF_RXFILT_HASH_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);
	SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);

	/* Now program new ones. */
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);
	} else {
		i = 1;
		/* First find the tail of the list. */
		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
					ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_link.le_next == NULL)
				break;
		}
		/* Now traverse the list backwards. */
		for (; ifma != NULL && ifma != (void *)&ifp->if_multiaddrs;
			ifma = (struct ifmultiaddr *)ifma->ifma_link.le_prev) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			/*
			 * Program the first 15 multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < SF_RXFILT_PERFECT_CNT) {
				sf_setperf(sc, i,
			LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
				i++;
				continue;
			}

			sf_sethash(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr), 0);
		}
	}

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void sf_autoneg_xmit(sc)
	struct sf_softc		*sc;
{
	u_int16_t		phy_sts;

	sf_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(sf_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = sf_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	sf_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void sf_autoneg_mii(sc, flag, verbose)
	struct sf_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media, advert, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

#ifndef FORCE_AUTONEG_TFOUR
	/*
	 * First, see if autoneg is supported. If not, there's
	 * no point in continuing.
	 */
	phy_sts = sf_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("sf%d: autonegotiation not supported\n",
							sc->sf_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case SF_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		sf_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case SF_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise sf_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->sf_tx_cnt) {
			sc->sf_want_auto = 1;
			return;
		}
		sf_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->sf_autoneg = 1;
		sc->sf_want_auto = 0;
		return;
		break;
	case SF_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->sf_autoneg = 0;
		break;
	default:
		printf("sf%d: invalid autoneg flag: %d\n", sc->sf_unit, flag);
		return;
	}

	if (sf_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("sf%d: autoneg complete, ", sc->sf_unit);
		phy_sts = sf_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("sf%d: autoneg not complete, ", sc->sf_unit);
	}

	media = sf_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (sf_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = sf_phy_readreg(sc, PHY_ANAR);
		ability = sf_phy_readreg(sc, PHY_LPAR);

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
		sf_phy_writereg(sc, PHY_BMCR, media);
		if ((media & IFM_GMASK) == IFM_FDX) {
			SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		} else {
			SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		}
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	sf_init(sc);

	if (sc->sf_tx_pend) {
		sc->sf_autoneg = 0;
		sc->sf_tx_pend = 0;
		sf_start(ifp);
	}

	return;
}

static void sf_getmode_mii(sc)
	struct sf_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = sf_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("sf%d: PHY status word: %x\n", sc->sf_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("sf%d: 10Mbps half-duplex mode supported\n",
								sc->sf_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("sf%d: 10Mbps full-duplex mode supported\n",
								sc->sf_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("sf%d: 100Mbps half-duplex mode supported\n",
								sc->sf_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("sf%d: 100Mbps full-duplex mode supported\n",
								sc->sf_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("sf%d: 100baseT4 mode supported\n", sc->sf_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("sf%d: forcing on autoneg support for BT4\n",
							 sc->sf_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("sf%d: autoneg supported\n", sc->sf_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void sf_setmode_mii(sc, media)
	struct sf_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->sf_autoneg) {
		printf("sf%d: canceling autoneg session\n", sc->sf_unit);
		ifp->if_timer = sc->sf_autoneg = sc->sf_want_auto = 0;
		bmcr = sf_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		sf_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("sf%d: selecting MII, ", sc->sf_unit);

	bmcr = sf_phy_readreg(sc, PHY_BMCR);

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
		SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
	} else {
		printf("half duplex\n");
		bmcr &= ~PHY_BMCR_DUPLEX;
		SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
	}

	sf_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

/*
 * Set media options.
 */
static int sf_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		sf_autoneg_mii(sc, SF_FLAG_SCHEDDELAY, 1);
	else {
		sf_setmode_mii(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void sf_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sf_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (!(sf_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (sf_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (sf_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = sf_phy_readreg(sc, PHY_LPAR);
	advert = sf_phy_readreg(sc, PHY_ANAR);
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

static int sf_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sf_softc		*sc = ifp->if_softc;
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
			sf_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sf_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sf_setmulti(sc);
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

static void sf_reset(sc)
	struct sf_softc		*sc;
{
	register int		i;

	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);
	DELAY(1000);
	SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);

	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_RESET);

	for (i = 0; i < SF_TIMEOUT; i++) {
		DELAY(10);
		if (!(csr_read_4(sc, SF_PCI_DEVCFG) & SF_PCIDEVCFG_RESET))
			break;
	}

	if (i == SF_TIMEOUT)
		printf("sf%d: reset never completed!\n", sc->sf_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	return;
}

/*
 * Probe for an Adaptec AIC-6915 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 * We also check the subsystem ID so that we can identify exactly which
 * NIC has been found, if possible.
 */
static int sf_probe(dev)
	device_t		dev;
{
	struct sf_type		*t;

	t = sf_devs;

	while(t->sf_name != NULL) {
		if ((pci_get_vendor(dev) == t->sf_vid) &&
		    (pci_get_device(dev) == t->sf_did)) {
			switch(pci_read_config(dev,
			    SF_PCI_SUBVEN_ID >> 16, 4) & 0x8FFF) {
			case AD_SUBSYSID_62011_REV0:
			case AD_SUBSYSID_62011_REV1:
				device_set_desc(dev,
				    "Adaptec ANA-62011 10/100BaseTX");
				return(0);
				break;
			case AD_SUBSYSID_62022:
				device_set_desc(dev,
				    "Adaptec ANA-62022 10/100BaseTX");
				return(0);
				break;
			case AD_SUBSYSID_62044:
				device_set_desc(dev,
				    "Adaptec ANA-62044 10/100BaseTX");
				return(0);
				break;
			case AD_SUBSYSID_62020:
				device_set_desc(dev,
				    "Adaptec ANA-62020 10/100BaseFX");
				return(0);
				break;
			case AD_SUBSYSID_69011:
				device_set_desc(dev,
				    "Adaptec ANA-69011 10/100BaseTX");
				return(0);
				break;
			default:
				device_set_desc(dev, t->sf_name);
				return(0);
				break;
			}
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int sf_attach(dev)
	device_t		dev;
{
	int			s, i;
	u_int32_t		command;
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	struct sf_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
	int			unit, rid, error = 0;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct sf_softc));

	/*
	 * Handle power management nonsense.
	 */
	command = pci_read_config(dev, SF_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, SF_PCI_PWRMGMTCTRL, 4);
		if (command & SF_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, SF_PCI_LOIO, 4);
			membase = pci_read_config(dev, SF_PCI_LOMEM, 4);
			irq = pci_read_config(dev, SF_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("sf%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & SF_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, SF_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, SF_PCI_LOIO, iobase, 4);
			pci_write_config(dev, SF_PCI_LOMEM, membase, 4);
			pci_write_config(dev, SF_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);

#ifdef SF_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("sf%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("sf%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}
#endif

	rid = SF_RID;
	sc->sf_res = bus_alloc_resource(dev, SF_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->sf_res == NULL) {
		printf ("sf%d: couldn't map ports\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->sf_btag = rman_get_bustag(sc->sf_res);
	sc->sf_bhandle = rman_get_bushandle(sc->sf_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sf_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sf_irq == NULL) {
		printf("sf%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sf_irq, INTR_TYPE_NET,
	    sf_intr, sc, &sc->sf_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sf_res);
		bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);
		printf("sf%d: couldn't set up irq\n", unit);
		goto fail;
	}

	callout_handle_init(&sc->sf_stat_ch);

	/* Reset the adapter. */
	sf_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->arpcom.ac_enaddr[i] =
		    sf_read_eeprom(sc, SF_EE_NODEADDR + ETHER_ADDR_LEN - i);

	/*
	 * An Adaptec chip was detected. Inform the world.
	 */
	printf("sf%d: Ethernet address: %6D\n", unit,
	    sc->arpcom.ac_enaddr, ":");

	sc->sf_unit = unit;

	/* Allocate the descriptor queues. */
	sc->sf_ldata = contigmalloc(sizeof(struct sf_list_data), M_DEVBUF,
	    M_NOWAIT, 0x100000, 0xffffffff, PAGE_SIZE, 0);

	if (sc->sf_ldata == NULL) {
		printf("sf%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->sf_irq, sc->sf_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sf_irq);
		bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);
		error = ENXIO;
		goto fail;
	}

	bzero(sc->sf_ldata, sizeof(struct sf_list_data));

	if (bootverbose)
		printf("sf%d: probing for a PHY\n", sc->sf_unit);
	for (i = SF_PHYADDR_MIN; i < SF_PHYADDR_MAX + 1; i++) {
		if (bootverbose)
			printf("sf%d: checking address: %d\n",
						sc->sf_unit, i);
		sc->sf_phy_addr = i;
		sf_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(sf_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = sf_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = sf_phy_readreg(sc, PHY_VENID);
		phy_did = sf_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("sf%d: found PHY at address %d, ",
				sc->sf_unit, sc->sf_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
			phy_vid, phy_did);
		p = sf_phys;
		while(p->sf_vid) {
			if (phy_vid == p->sf_vid &&
				(phy_did | 0x000F) == p->sf_did) {
				sc->sf_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->sf_pinfo == NULL)
			sc->sf_pinfo = &sf_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("sf%d: PHY type: %s\n",
				sc->sf_unit, sc->sf_pinfo->sf_name);
	} else {
		printf("sf%d: MII without any phy!\n", sc->sf_unit);
		free(sc->sf_ldata, M_DEVBUF);
		bus_teardown_intr(dev, sc->sf_irq, sc->sf_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sf_irq);
		bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);
		error = ENXIO;
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "sf";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sf_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sf_start;
	ifp->if_watchdog = sf_watchdog;
	ifp->if_init = sf_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = SF_TX_DLIST_CNT - 1;

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, sf_ifmedia_upd, sf_ifmedia_sts);

	sf_getmode_mii(sc);
	if (cold) {
		sf_autoneg_mii(sc, SF_FLAG_FORCEDELAY, 1);
		sf_stop(sc);
	} else {
		sf_init(sc);
		sf_autoneg_mii(sc, SF_FLAG_SCHEDDELAY, 1);
	}

	media = sc->ifmedia.ifm_media;
	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

fail:
	splx(s);
	return(error);
}

static int sf_detach(dev)
	device_t		dev;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	if_detach(ifp);
	sf_stop(sc);

	bus_teardown_intr(dev, sc->sf_irq, sc->sf_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sf_irq);
	bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);

	free(sc->sf_ldata, M_DEVBUF);
	ifmedia_removeall(&sc->ifmedia);

	splx(s);

	return(0);
}

static int sf_init_rx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_rx_dlist_big,
	    sizeof(struct sf_rx_bufdesc_type0) * SF_RX_DLIST_CNT);
	bzero((char *)ld->sf_rx_clist,
	    sizeof(struct sf_rx_cmpdesc_type3) * SF_RX_CLIST_CNT);

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sf_newbuf(sc, &ld->sf_rx_dlist_big[i], NULL) == ENOBUFS)
			return(ENOBUFS);
	}

	return(0);
}

static void sf_init_tx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_tx_dlist,
	    sizeof(struct sf_tx_bufdesc_type0) * SF_TX_DLIST_CNT);
	bzero((char *)ld->sf_tx_clist,
	    sizeof(struct sf_tx_cmpdesc_type0) * SF_TX_CLIST_CNT);

	for (i = 0; i < SF_TX_DLIST_CNT; i++)
		ld->sf_tx_dlist[i].sf_id = SF_TX_BUFDESC_ID;
	for (i = 0; i < SF_TX_CLIST_CNT; i++)
		ld->sf_tx_clist[i].sf_type = SF_TXCMPTYPE_TX;

	ld->sf_tx_dlist[SF_TX_DLIST_CNT - 1].sf_end = 1;
	sc->sf_tx_cnt = 0;

	return;
}

static int sf_newbuf(sc, c, m)
	struct sf_softc		*sc;
	struct sf_rx_bufdesc_type0	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sf%d: no memory for rx list -- "
			    "packet dropped!\n", sc->sf_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("sf%d: no memory for rx list -- "
			    "packet dropped!\n", sc->sf_unit);
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

	c->sf_mbuf = m_new;
	c->sf_addrlo = SF_RX_HOSTADDR(vtophys(mtod(m_new, caddr_t)));
	c->sf_valid = 1;

	return(0);
}

/*
 * The starfire is programmed to use 'normal' mode for packet reception,
 * which means we use the consumer/producer model for both the buffer
 * descriptor queue and the completion descriptor queue. The only problem
 * with this is that it involves a lot of register accesses: we have to
 * read the RX completion consumer and producer indexes and the RX buffer
 * producer index, plus the RX completion consumer and RX buffer producer
 * indexes have to be updated. It would have been easier if Adaptec had
 * put each index in a separate register, especially given that the damn
 * NIC has a 512K register space.
 *
 * In spite of all the lovely features that Adaptec crammed into the 6915,
 * it is marred by one truly stupid design flaw, which is that receive
 * buffer addresses must be aligned on a longword boundary. This forces
 * the packet payload to be unaligned, which is suboptimal on the x86 and
 * completely unuseable on the Alpha. Our only recourse is to copy received
 * packets into properly aligned buffers before handing them off.
 */

static void sf_rxeof(sc)
	struct sf_softc		*sc;
{
	struct ether_header	*eh;
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct sf_rx_bufdesc_type0	*desc;
	struct sf_rx_cmpdesc_type3	*cur_rx;
	u_int32_t		rxcons, rxprod;
	int			cmpprodidx, cmpconsidx, bufprodidx;

	ifp = &sc->arpcom.ac_if;

	rxcons = csr_read_4(sc, SF_CQ_CONSIDX);
	rxprod = csr_read_4(sc, SF_RXDQ_PTR_Q1);
	cmpprodidx = SF_IDX_LO(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_LO(rxcons);
	bufprodidx = SF_IDX_LO(rxprod);

	while (cmpconsidx != cmpprodidx) {
		struct mbuf		*m0;

		cur_rx = &sc->sf_ldata->sf_rx_clist[cmpconsidx];
		desc = &sc->sf_ldata->sf_rx_dlist_big[cur_rx->sf_endidx];
		m = desc->sf_mbuf;
		SF_INC(cmpconsidx, SF_RX_CLIST_CNT);
		SF_INC(bufprodidx, SF_RX_DLIST_CNT);

		if (!(cur_rx->sf_status1 & SF_RXSTAT1_OK)) {
			ifp->if_ierrors++;
			sf_newbuf(sc, desc, m);
			continue;
		}

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    cur_rx->sf_len + ETHER_ALIGN, 0, ifp, NULL);
		sf_newbuf(sc, desc, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		eh = mtod(m, struct ether_header *);
		ifp->if_ipackets++;

#if NBPF > 0
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

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (rxcons & ~SF_CQ_CONSIDX_RXQ1) | cmpconsidx);
	csr_write_4(sc, SF_RXDQ_PTR_Q1,
	    (rxprod & ~SF_RXDQ_PRODIDX) | bufprodidx);

	return;
}

/*
 * Read the transmit status from the completion queue and release
 * mbufs. Note that the buffer descriptor index in the completion
 * descriptor is an offset from the start of the transmit buffer
 * descriptor list in bytes. This is important because the manual
 * gives the impression that it should match the producer/consumer
 * index, which is the offset in 8 byte blocks.
 */
static void sf_txeof(sc)
	struct sf_softc		*sc;
{
	int			txcons, cmpprodidx, cmpconsidx;
	struct sf_tx_cmpdesc_type1 *cur_cmp;
	struct sf_tx_bufdesc_type0 *cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	txcons = csr_read_4(sc, SF_CQ_CONSIDX);
	cmpprodidx = SF_IDX_HI(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_HI(txcons);

	while (cmpconsidx != cmpprodidx) {
		cur_cmp = &sc->sf_ldata->sf_tx_clist[cmpconsidx];
		cur_tx = &sc->sf_ldata->sf_tx_dlist[cur_cmp->sf_index >> 7];
		SF_INC(cmpconsidx, SF_TX_CLIST_CNT);

		if (cur_cmp->sf_txstat & SF_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else
			ifp->if_oerrors++;

		sc->sf_tx_cnt--;
		if (cur_tx->sf_mbuf != NULL) {
			m_freem(cur_tx->sf_mbuf);
			cur_tx->sf_mbuf = NULL;
		}
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (txcons & ~SF_CQ_CONSIDX_TXQ) |
	    ((cmpconsidx << 16) & 0xFFFF0000));

	return;
}

static void sf_intr(arg)
	void			*arg;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	if (!(csr_read_4(sc, SF_ISR_SHADOW) & SF_ISR_PCIINT_ASSERTED))
		return;

	/* Disable interrupts. */
	csr_write_4(sc, SF_IMR, 0x00000000);

	for (;;) {
		status = csr_read_4(sc, SF_ISR);
		if (status)
			csr_write_4(sc, SF_ISR, status);

		if (!(status & SF_INTRS))
			break;

		if (status & SF_ISR_RXDQ1_DMADONE)
			sf_rxeof(sc);

		if (status & SF_ISR_TX_TXDONE)
			sf_txeof(sc);

		if (status & SF_ISR_ABNORMALINTR) {
			if (status & SF_ISR_STATSOFLOW) {
				untimeout(sf_stats_update, sc,
				    sc->sf_stat_ch);
				sf_stats_update(sc);
			} else
				sf_init(sc);
		}
	}

	/* Re-enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		sf_start(ifp);

	return;
}

static void sf_init(xsc)
	void			*xsc;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	int			i, s;

	s = splimp();

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	sf_stop(sc);
	sf_reset(sc);

	/* Init all the receive filter registers */
	for (i = SF_RXFILT_PERFECT_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);

	/* Empty stats counter registers. */
	for (i = 0; i < sizeof(struct sf_stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	/* Init our MAC address */
	csr_write_4(sc, SF_PAR0, *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	csr_write_4(sc, SF_PAR1, *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));
	sf_setperf(sc, 0, (caddr_t)&sc->arpcom.ac_enaddr);

	if (sf_init_rx_ring(sc) == ENOBUFS) {
		printf("sf%d: initialization failed: no "
		    "memory for rx buffers\n", sc->sf_unit);
		(void)splx(s);
		return;
	}

	sf_init_tx_ring(sc);

	csr_write_4(sc, SF_RXFILT, SF_PERFMODE_NORMAL|SF_HASHMODE_WITHVLAN);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	}

	/* Init the completion queue indexes */
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);

	/* Init the RX completion queue */
	csr_write_4(sc, SF_RXCQ_CTL_1,
	    vtophys(sc->sf_ldata->sf_rx_clist) & SF_RXCQ_ADDR);
	SF_SETBIT(sc, SF_RXCQ_CTL_1, SF_RXCQTYPE_3);

	/* Init RX DMA control. */
	SF_SETBIT(sc, SF_RXDMA_CTL, SF_RXDMA_REPORTBADPKTS);

	/* Init the RX buffer descriptor queue. */
	csr_write_4(sc, SF_RXDQ_ADDR_Q1,
	    vtophys(sc->sf_ldata->sf_rx_dlist_big));
	csr_write_4(sc, SF_RXDQ_CTL_1, (MCLBYTES << 16) | SF_DESCSPACE_16BYTES);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, SF_RX_DLIST_CNT - 1);

	/* Init the TX completion queue */
	csr_write_4(sc, SF_TXCQ_CTL,
	    vtophys(sc->sf_ldata->sf_tx_clist) & SF_RXCQ_ADDR);

	/* Init the TX buffer descriptor queue. */
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO,
		vtophys(sc->sf_ldata->sf_tx_dlist));
	SF_SETBIT(sc, SF_TX_FRAMCTL, SF_TXFRMCTL_CPLAFTERTX);
	csr_write_4(sc, SF_TXDQ_CTL,
	    SF_TXBUFDESC_TYPE0|SF_TXMINSPACE_128BYTES|SF_TXSKIPLEN_8BYTES);
	SF_SETBIT(sc, SF_TXDQ_CTL, SF_TXDQCTL_NODMACMP);

	/* Enable autopadding of short TX frames. */
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_AUTOPAD);

	/* Make sure the duplex mode is set correctly. */
	if ((sc->ifmedia.ifm_media & IFM_GMASK) == IFM_FDX) {
		SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
	} else {
		SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
	}       

	/* Enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);
	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_INTR_ENB);

	/* Enable the RX and TX engines. */
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_RX_ENB|SF_ETHCTL_RXDMA_ENB);
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_TX_ENB|SF_ETHCTL_TXDMA_ENB);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->sf_stat_ch = timeout(sf_stats_update, sc, hz);

	splx(s);

	return;
}

static int sf_encap(sc, c, m_head)
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct sf_frag		*f = NULL;
	struct mbuf		*m;

	m = m_head;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == SF_MAXFRAGS)
				break;
			f = &c->sf_frags[frag];
			if (frag == 0)
				f->sf_pktlen = m_head->m_pkthdr.len;
			f->sf_fraglen = m->m_len;
			f->sf_addr = vtophys(mtod(m, vm_offset_t));
			frag++;
		}
	}

	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sf%d: no memory for tx list", sc->sf_unit);
			return(1);
		}

		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("sf%d: no memory for tx list",
				    sc->sf_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,
		    mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->sf_frags[0];
		f->sf_fraglen = f->sf_pktlen = m_head->m_pkthdr.len;
		f->sf_addr = vtophys(mtod(m_head, caddr_t));
		frag = 1;
	}

	c->sf_mbuf = m_head;
	c->sf_id = SF_TX_BUFDESC_ID;
	c->sf_fragcnt = frag;
	c->sf_intr = 1;
	c->sf_caltcp = 0;
	c->sf_crcen = 1;

	return(0);
}

static void sf_start(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *cur_tx = NULL;
	struct mbuf		*m_head = NULL;
	int			i, txprod;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	if (sc->sf_autoneg) {
		sc->sf_tx_pend = 1;
		return;
	}

	txprod = csr_read_4(sc, SF_TXDQ_PRODIDX);
	i = SF_IDX_HI(txprod) >> 4;

	while(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		cur_tx = &sc->sf_ldata->sf_tx_dlist[i];
		sf_encap(sc, cur_tx, m_head);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPF > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
#endif
		SF_INC(i, SF_TX_DLIST_CNT);
		sc->sf_tx_cnt++;
		if (sc->sf_tx_cnt == (SF_TX_DLIST_CNT - 2))
			break;
	}

	if (cur_tx == NULL)
		return;

	/* Transmit */
	csr_write_4(sc, SF_TXDQ_PRODIDX,
	    (txprod & ~SF_TXDQ_PRODIDX_HIPRIO) |
	    ((i << 20) & 0xFFFF0000));

	ifp->if_timer = 5;

	return;
}

static void sf_stop(sc)
	struct sf_softc		*sc;
{
	int			i;

	untimeout(sf_stats_update, sc, sc->sf_stat_ch);

	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);
	csr_write_4(sc, SF_RXDQ_ADDR_Q1, 0);
	csr_write_4(sc, SF_RXDQ_CTL_1, 0);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, 0);
	csr_write_4(sc, SF_TXCQ_CTL, 0);
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO, 0);
	csr_write_4(sc, SF_TXDQ_CTL, 0);
	sf_reset(sc);

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf);
			sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf = NULL;
		}
	}

	for (i = 0; i < SF_TX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_tx_dlist[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf);
			sc->sf_ldata->sf_tx_dlist[i].sf_mbuf = NULL;
		}
	}

	return;
}

/*
 * Note: it is important that this function not be interrupted. We
 * use a two-stage register access scheme: if we are interrupted in
 * between setting the indirect address register and reading from the
 * indirect data register, the contents of the address register could
 * be changed out from under us.
 */     
static void sf_stats_update(xsc)
	void			*xsc;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	struct sf_stats		stats;
	u_int32_t		*ptr;
	int			i, s;

	s = splimp();

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	ptr = (u_int32_t *)&stats;
	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		ptr[i] = csr_read_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)));

	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	ifp->if_collisions += stats.sf_tx_single_colls +
	    stats.sf_tx_multi_colls + stats.sf_tx_excess_colls;

	sc->sf_stat_ch = timeout(sf_stats_update, sc, hz);

	splx(s);

	return;
}

static void sf_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;

	sc = ifp->if_softc;

	if (sc->sf_autoneg) {
		sf_autoneg_mii(sc, SF_FLAG_DELAYTIMEO, 1);
		if (!(ifp->if_flags & IFF_UP))
			sf_stop(sc);
		return;
	}

	ifp->if_oerrors++;
	printf("sf%d: watchdog timeout\n", sc->sf_unit);

	if (sc->sf_pinfo != NULL) {
		if (!(sf_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
			printf("sf%d: no carrier - transceiver "
			    "cable problem?\n", sc->sf_unit);
	}

	sf_stop(sc);
	sf_reset(sc);
	sf_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		sf_start(ifp);

	return;
}

static void sf_shutdown(dev)
	device_t		dev;
{
	struct sf_softc		*sc;

	sc = device_get_softc(dev);

	sf_stop(sc);

	return;
}
