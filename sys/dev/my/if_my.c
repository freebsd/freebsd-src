/*-
 * Written by: yen_cw@myson.com.tw
 * Copyright (c) 2002 Myson Technology Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Myson fast ethernet PCI NIC driver, available at: http://www.myson.com.tw/
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#define NBPFILTER	1

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <net/bpf.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/clock.h>	/* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"

/*
 * #define MY_USEIOSPACE
 */

static int      MY_USEIOSPACE = 1;

#if (MY_USEIOSPACE)
#define MY_RES                  SYS_RES_IOPORT
#define MY_RID                  MY_PCI_LOIO
#else
#define MY_RES                  SYS_RES_MEMORY
#define MY_RID                  MY_PCI_LOMEM
#endif


#include <dev/my/if_myreg.h>

#ifndef lint
static          const char rcsid[] =
"$Id: if_my.c,v 1.16 2003/04/15 06:37:25 mdodd Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
struct my_type *my_info_tmp;
static struct my_type my_devs[] = {
	{MYSONVENDORID, MTD800ID, "Myson MTD80X Based Fast Ethernet Card"},
	{MYSONVENDORID, MTD803ID, "Myson MTD80X Based Fast Ethernet Card"},
	{MYSONVENDORID, MTD891ID, "Myson MTD89X Based Giga Ethernet Card"},
	{0, 0, NULL}
};

/*
 * Various supported PHY vendors/types and their names. Note that this driver
 * will work with pretty much any MII-compliant PHY, so failure to positively
 * identify the chip is not a fatal error.
 */
static struct my_type my_phys[] = {
	{MysonPHYID0, MysonPHYID0, "<MYSON MTD981>"},
	{SeeqPHYID0, SeeqPHYID0, "<SEEQ 80225>"},
	{AhdocPHYID0, AhdocPHYID0, "<AHDOC 101>"},
	{MarvellPHYID0, MarvellPHYID0, "<MARVELL 88E1000>"},
	{LevelOnePHYID0, LevelOnePHYID0, "<LevelOne LXT1000>"},
	{0, 0, "<MII-compliant physical interface>"}
};

static int      my_probe(device_t);
static int      my_attach(device_t);
static int      my_detach(device_t);
static int      my_newbuf(struct my_softc *, struct my_chain_onefrag *);
static int      my_encap(struct my_softc *, struct my_chain *, struct mbuf *);
static void     my_rxeof(struct my_softc *);
static void     my_txeof(struct my_softc *);
static void     my_txeoc(struct my_softc *);
static void     my_intr(void *);
static void     my_start(struct ifnet *);
static int      my_ioctl(struct ifnet *, u_long, caddr_t);
static void     my_init(void *);
static void     my_stop(struct my_softc *);
static void     my_watchdog(struct ifnet *);
static void     my_shutdown(device_t);
static int      my_ifmedia_upd(struct ifnet *);
static void     my_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static u_int16_t my_phy_readreg(struct my_softc *, int);
static void     my_phy_writereg(struct my_softc *, int, int);
static void     my_autoneg_xmit(struct my_softc *);
static void     my_autoneg_mii(struct my_softc *, int, int);
static void     my_setmode_mii(struct my_softc *, int);
static void     my_getmode_mii(struct my_softc *);
static void     my_setcfg(struct my_softc *, int);
static uint32_t my_mchash(const uint8_t *);
static void     my_setmulti(struct my_softc *);
static void     my_reset(struct my_softc *);
static int      my_list_rx_init(struct my_softc *);
static int      my_list_tx_init(struct my_softc *);
static long     my_send_cmd_to_phy(struct my_softc *, int, int);

#define MY_SETBIT(sc, reg, x) CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | (x))
#define MY_CLRBIT(sc, reg, x) CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~(x))

static device_method_t my_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, my_probe),
	DEVMETHOD(device_attach, my_attach),
	DEVMETHOD(device_detach, my_detach),
	DEVMETHOD(device_shutdown, my_shutdown),

	{0, 0}
};

static driver_t my_driver = {
	"my",
	my_methods,
	sizeof(struct my_softc)
};

static devclass_t my_devclass;

DRIVER_MODULE(my, pci, my_driver, my_devclass, 0, 0);
MODULE_DEPEND(my, pci, 1, 1, 1);
MODULE_DEPEND(my, ether, 1, 1, 1);

static long
my_send_cmd_to_phy(struct my_softc * sc, int opcode, int regad)
{
	long            miir;
	int             i;
	int             mask, data;

	MY_LOCK(sc);

	/* enable MII output */
	miir = CSR_READ_4(sc, MY_MANAGEMENT);
	miir &= 0xfffffff0;

	miir |= MY_MASK_MIIR_MII_WRITE + MY_MASK_MIIR_MII_MDO;

	/* send 32 1's preamble */
	for (i = 0; i < 32; i++) {
		/* low MDC; MDO is already high (miir) */
		miir &= ~MY_MASK_MIIR_MII_MDC;
		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);

		/* high MDC */
		miir |= MY_MASK_MIIR_MII_MDC;
		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
	}

	/* calculate ST+OP+PHYAD+REGAD+TA */
	data = opcode | (sc->my_phy_addr << 7) | (regad << 2);

	/* sent out */
	mask = 0x8000;
	while (mask) {
		/* low MDC, prepare MDO */
		miir &= ~(MY_MASK_MIIR_MII_MDC + MY_MASK_MIIR_MII_MDO);
		if (mask & data)
			miir |= MY_MASK_MIIR_MII_MDO;

		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
		/* high MDC */
		miir |= MY_MASK_MIIR_MII_MDC;
		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
		DELAY(30);

		/* next */
		mask >>= 1;
		if (mask == 0x2 && opcode == MY_OP_READ)
			miir &= ~MY_MASK_MIIR_MII_WRITE;
	}

	MY_UNLOCK(sc);
	return miir;
}


static          u_int16_t
my_phy_readreg(struct my_softc * sc, int reg)
{
	long            miir;
	int             mask, data;

	MY_LOCK(sc);

	if (sc->my_info->my_did == MTD803ID)
		data = CSR_READ_2(sc, MY_PHYBASE + reg * 2);
	else {
		miir = my_send_cmd_to_phy(sc, MY_OP_READ, reg);

		/* read data */
		mask = 0x8000;
		data = 0;
		while (mask) {
			/* low MDC */
			miir &= ~MY_MASK_MIIR_MII_MDC;
			CSR_WRITE_4(sc, MY_MANAGEMENT, miir);

			/* read MDI */
			miir = CSR_READ_4(sc, MY_MANAGEMENT);
			if (miir & MY_MASK_MIIR_MII_MDI)
				data |= mask;

			/* high MDC, and wait */
			miir |= MY_MASK_MIIR_MII_MDC;
			CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
			DELAY(30);

			/* next */
			mask >>= 1;
		}

		/* low MDC */
		miir &= ~MY_MASK_MIIR_MII_MDC;
		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
	}

	MY_UNLOCK(sc);
	return (u_int16_t) data;
}


static void
my_phy_writereg(struct my_softc * sc, int reg, int data)
{
	long            miir;
	int             mask;

	MY_LOCK(sc);

	if (sc->my_info->my_did == MTD803ID)
		CSR_WRITE_2(sc, MY_PHYBASE + reg * 2, data);
	else {
		miir = my_send_cmd_to_phy(sc, MY_OP_WRITE, reg);

		/* write data */
		mask = 0x8000;
		while (mask) {
			/* low MDC, prepare MDO */
			miir &= ~(MY_MASK_MIIR_MII_MDC + MY_MASK_MIIR_MII_MDO);
			if (mask & data)
				miir |= MY_MASK_MIIR_MII_MDO;
			CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
			DELAY(1);

			/* high MDC */
			miir |= MY_MASK_MIIR_MII_MDC;
			CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
			DELAY(1);

			/* next */
			mask >>= 1;
		}

		/* low MDC */
		miir &= ~MY_MASK_MIIR_MII_MDC;
		CSR_WRITE_4(sc, MY_MANAGEMENT, miir);
	}
	MY_UNLOCK(sc);
	return;
}

static uint32_t
my_mchash(const uint8_t *addr)
{
	uint32_t crc, carry;
	int idx, bit;
	uint8_t data;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF;	/* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (data & 0x01);
			crc <<= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/*
	 * return the filter bit position Note: I arrived at the following
	 * nonsense through experimentation. It's not the usual way to
	 * generate the bit position but it's the only thing I could come up
	 * with that works.
	 */
	return (~(crc >> 26) & 0x0000003F);
}


/*
 * Program the 64-bit multicast hash filter.
 */
static void
my_setmulti(struct my_softc * sc)
{
	struct ifnet   *ifp;
	int             h = 0;
	u_int32_t       hashes[2] = {0, 0};
	struct ifmultiaddr *ifma;
	u_int32_t       rxfilt;
	int             mcnt = 0;

	MY_LOCK(sc);

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, MY_TCRRCR);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= MY_AM;
		CSR_WRITE_4(sc, MY_TCRRCR, rxfilt);
		CSR_WRITE_4(sc, MY_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, MY_MAR1, 0xFFFFFFFF);

		MY_UNLOCK(sc);

		return;
	}
	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, MY_MAR0, 0);
	CSR_WRITE_4(sc, MY_MAR1, 0);

	/* now program new ones */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = my_mchash(LLADDR((struct sockaddr_dl *) ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= MY_AM;
	else
		rxfilt &= ~MY_AM;
	CSR_WRITE_4(sc, MY_MAR0, hashes[0]);
	CSR_WRITE_4(sc, MY_MAR1, hashes[1]);
	CSR_WRITE_4(sc, MY_TCRRCR, rxfilt);
	MY_UNLOCK(sc);
	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void
my_autoneg_xmit(struct my_softc * sc)
{
	u_int16_t       phy_sts = 0;

	MY_LOCK(sc);

	my_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while (my_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_RESET);

	phy_sts = my_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL | PHY_BMCR_AUTONEGRSTR;
	my_phy_writereg(sc, PHY_BMCR, phy_sts);

	MY_UNLOCK(sc);
	return;
}


/*
 * Invoke autonegotiation on a PHY.
 */
static void
my_autoneg_mii(struct my_softc * sc, int flag, int verbose)
{
	u_int16_t       phy_sts = 0, media, advert, ability;
	u_int16_t       ability2 = 0;
	struct ifnet   *ifp;
	struct ifmedia *ifm;

	MY_LOCK(sc);

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	ifm->ifm_media = IFM_ETHER | IFM_AUTO;

#ifndef FORCE_AUTONEG_TFOUR
	/*
	 * First, see if autoneg is supported. If not, there's no point in
	 * continuing.
	 */
	phy_sts = my_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("my%d: autonegotiation not supported\n",
			    sc->my_unit);
		ifm->ifm_media = IFM_ETHER | IFM_10_T | IFM_HDX;
		MY_UNLOCK(sc);
		return;
	}
#endif
	switch (flag) {
	case MY_FLAG_FORCEDELAY:
		/*
		 * XXX Never use this option anywhere but in the probe
		 * routine: making the kernel stop dead in its tracks for
		 * three whole seconds after we've gone multi-user is really
		 * bad manners.
		 */
		my_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case MY_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting an
		 * autoneg session, otherwise my_start() may clobber our
		 * timeout, and we don't want to allow transmission during an
		 * autoneg session since that can screw it up.
		 */
		if (sc->my_cdata.my_tx_head != NULL) {
			sc->my_want_auto = 1;
			MY_UNLOCK(sc);
			return;
		}
		my_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->my_autoneg = 1;
		sc->my_want_auto = 0;
		MY_UNLOCK(sc);
		return;
	case MY_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->my_autoneg = 0;
		break;
	default:
		printf("my%d: invalid autoneg flag: %d\n", sc->my_unit, flag);
		MY_UNLOCK(sc);
		return;
	}

	if (my_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("my%d: autoneg complete, ", sc->my_unit);
		phy_sts = my_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("my%d: autoneg not complete, ", sc->my_unit);
	}

	media = my_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (my_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("my%d: link status good. ", sc->my_unit);
		advert = my_phy_readreg(sc, PHY_ANAR);
		ability = my_phy_readreg(sc, PHY_LPAR);
		if ((sc->my_pinfo->my_vid == MarvellPHYID0) ||
		    (sc->my_pinfo->my_vid == LevelOnePHYID0)) {
			ability2 = my_phy_readreg(sc, PHY_1000SR);
			if (ability2 & PHY_1000SR_1000BTXFULL) {
				advert = 0;
				ability = 0;
				/*
				 * this version did not support 1000M,
				 * ifm->ifm_media =
				 * IFM_ETHER|IFM_1000_T|IFM_FDX;
				 */
				ifm->ifm_media =
				    IFM_ETHER | IFM_100_TX | IFM_FDX;
				media &= ~PHY_BMCR_SPEEDSEL;
				media |= PHY_BMCR_1000;
				media |= PHY_BMCR_DUPLEX;
				printf("(full-duplex, 1000Mbps)\n");
			} else if (ability2 & PHY_1000SR_1000BTXHALF) {
				advert = 0;
				ability = 0;
				/*
				 * this version did not support 1000M,
				 * ifm->ifm_media = IFM_ETHER|IFM_1000_T;
				 */
				ifm->ifm_media = IFM_ETHER | IFM_100_TX;
				media &= ~PHY_BMCR_SPEEDSEL;
				media &= ~PHY_BMCR_DUPLEX;
				media |= PHY_BMCR_1000;
				printf("(half-duplex, 1000Mbps)\n");
			}
		}
		if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4) {
			ifm->ifm_media = IFM_ETHER | IFM_100_T4;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(100baseT4)\n");
		} else if (advert & PHY_ANAR_100BTXFULL &&
			   ability & PHY_ANAR_100BTXFULL) {
			ifm->ifm_media = IFM_ETHER | IFM_100_TX | IFM_FDX;
			media |= PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_100BTXHALF &&
			   ability & PHY_ANAR_100BTXHALF) {
			ifm->ifm_media = IFM_ETHER | IFM_100_TX | IFM_HDX;
			media |= PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 100Mbps)\n");
		} else if (advert & PHY_ANAR_10BTFULL &&
			   ability & PHY_ANAR_10BTFULL) {
			ifm->ifm_media = IFM_ETHER | IFM_10_T | IFM_FDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media |= PHY_BMCR_DUPLEX;
			printf("(full-duplex, 10Mbps)\n");
		} else if (advert) {
			ifm->ifm_media = IFM_ETHER | IFM_10_T | IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}
		media &= ~PHY_BMCR_AUTONEGENBL;

		/* Set ASIC's duplex mode to match the PHY. */
		my_phy_writereg(sc, PHY_BMCR, media);
		my_setcfg(sc, media);
	} else {
		if (verbose)
			printf("my%d: no carrier\n", sc->my_unit);
	}

	my_init(sc);
	if (sc->my_tx_pend) {
		sc->my_autoneg = 0;
		sc->my_tx_pend = 0;
		my_start(ifp);
	}
	MY_UNLOCK(sc);
	return;
}

/*
 * To get PHY ability.
 */
static void
my_getmode_mii(struct my_softc * sc)
{
	u_int16_t       bmsr;
	struct ifnet   *ifp;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	bmsr = my_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("my%d: PHY status word: %x\n", sc->my_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER | IFM_10_T | IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("my%d: 10Mbps half-duplex mode supported\n",
			       sc->my_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_10_T | IFM_HDX,
		    0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_10_T, 0, NULL);
	}
	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("my%d: 10Mbps full-duplex mode supported\n",
			    sc->my_unit);

		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_10_T | IFM_FDX,
		    0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_10_T | IFM_FDX;
	}
	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("my%d: 100Mbps half-duplex mode supported\n",
			       sc->my_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_100_TX | IFM_HDX,
			    0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_100_TX | IFM_HDX;
	}
	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("my%d: 100Mbps full-duplex mode supported\n",
			    sc->my_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_100_TX | IFM_FDX,
		    0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_100_TX | IFM_FDX;
	}
	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("my%d: 100baseT4 mode supported\n", sc->my_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("my%d: forcing on autoneg support for BT4\n",
			    sc->my_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_AUTO;
#endif
	}
#if 0				/* this version did not support 1000M, */
	if (sc->my_pinfo->my_vid == MarvellPHYID0) {
		if (bootverbose)
			printf("my%d: 1000Mbps half-duplex mode supported\n",
			       sc->my_unit);

		ifp->if_baudrate = 1000000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_1000_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_1000_T | IFM_HDX,
		    0, NULL);
		if (bootverbose)
			printf("my%d: 1000Mbps full-duplex mode supported\n",
			   sc->my_unit);
		ifp->if_baudrate = 1000000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_1000_T | IFM_FDX,
		    0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_1000_T | IFM_FDX;
	}
#endif
	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("my%d: autoneg supported\n", sc->my_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER | IFM_AUTO;
	}
	MY_UNLOCK(sc);
	return;
}

/*
 * Set speed and duplex mode.
 */
static void
my_setmode_mii(struct my_softc * sc, int media)
{
	u_int16_t       bmcr;
	struct ifnet   *ifp;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->my_autoneg) {
		printf("my%d: canceling autoneg session\n", sc->my_unit);
		ifp->if_timer = sc->my_autoneg = sc->my_want_auto = 0;
		bmcr = my_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		my_phy_writereg(sc, PHY_BMCR, bmcr);
	}
	printf("my%d: selecting MII, ", sc->my_unit);
	bmcr = my_phy_readreg(sc, PHY_BMCR);
	bmcr &= ~(PHY_BMCR_AUTONEGENBL | PHY_BMCR_SPEEDSEL | PHY_BMCR_1000 |
		  PHY_BMCR_DUPLEX | PHY_BMCR_LOOPBK);

#if 0				/* this version did not support 1000M, */
	if (IFM_SUBTYPE(media) == IFM_1000_T) {
		printf("1000Mbps/T4, half-duplex\n");
		bmcr &= ~PHY_BMCR_SPEEDSEL;
		bmcr &= ~PHY_BMCR_DUPLEX;
		bmcr |= PHY_BMCR_1000;
	}
#endif
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
	my_phy_writereg(sc, PHY_BMCR, bmcr);
	my_setcfg(sc, bmcr);
	MY_UNLOCK(sc);
	return;
}

/*
 * The Myson manual states that in order to fiddle with the 'full-duplex' and
 * '100Mbps' bits in the netconfig register, we first have to put the
 * transmit and/or receive logic in the idle state.
 */
static void
my_setcfg(struct my_softc * sc, int bmcr)
{
	int             i, restart = 0;

	MY_LOCK(sc);
	if (CSR_READ_4(sc, MY_TCRRCR) & (MY_TE | MY_RE)) {
		restart = 1;
		MY_CLRBIT(sc, MY_TCRRCR, (MY_TE | MY_RE));
		for (i = 0; i < MY_TIMEOUT; i++) {
			DELAY(10);
			if (!(CSR_READ_4(sc, MY_TCRRCR) &
			    (MY_TXRUN | MY_RXRUN)))
				break;
		}
		if (i == MY_TIMEOUT)
			printf("my%d: failed to force tx and rx to idle \n",
			    sc->my_unit);
	}
	MY_CLRBIT(sc, MY_TCRRCR, MY_PS1000);
	MY_CLRBIT(sc, MY_TCRRCR, MY_PS10);
	if (bmcr & PHY_BMCR_1000)
		MY_SETBIT(sc, MY_TCRRCR, MY_PS1000);
	else if (!(bmcr & PHY_BMCR_SPEEDSEL))
		MY_SETBIT(sc, MY_TCRRCR, MY_PS10);
	if (bmcr & PHY_BMCR_DUPLEX)
		MY_SETBIT(sc, MY_TCRRCR, MY_FD);
	else
		MY_CLRBIT(sc, MY_TCRRCR, MY_FD);
	if (restart)
		MY_SETBIT(sc, MY_TCRRCR, MY_TE | MY_RE);
	MY_UNLOCK(sc);
	return;
}

static void
my_reset(struct my_softc * sc)
{
	register int    i;

	MY_LOCK(sc);
	MY_SETBIT(sc, MY_BCR, MY_SWR);
	for (i = 0; i < MY_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, MY_BCR) & MY_SWR))
			break;
	}
	if (i == MY_TIMEOUT)
		printf("m0x%d: reset never completed!\n", sc->my_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
	MY_UNLOCK(sc);
	return;
}

/*
 * Probe for a Myson chip. Check the PCI vendor and device IDs against our
 * list and return a device name if we find a match.
 */
static int
my_probe(device_t dev)
{
	struct my_type *t;

	t = my_devs;
	while (t->my_name != NULL) {
		if ((pci_get_vendor(dev) == t->my_vid) &&
		    (pci_get_device(dev) == t->my_did)) {
			device_set_desc(dev, t->my_name);
			my_info_tmp = t;
			return (0);
		}
		t++;
	}
	return (ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia setup and
 * ethernet/BPF attach.
 */
static int
my_attach(device_t dev)
{
	int             s, i;
	u_char          eaddr[ETHER_ADDR_LEN];
	u_int32_t       command, iobase;
	struct my_softc *sc;
	struct ifnet   *ifp;
	int             media = IFM_ETHER | IFM_100_TX | IFM_FDX;
	unsigned int    round;
	caddr_t         roundptr;
	struct my_type *p;
	u_int16_t       phy_vid, phy_did, phy_sts = 0;
	int             rid, unit, error = 0;

	s = splimp();
	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	if (sc == NULL) {
		printf("my%d: no memory for softc struct!\n", unit);
		error = ENXIO;
		goto fail;

	}
	bzero(sc, sizeof(struct my_softc));
	mtx_init(&sc->my_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	MY_LOCK(sc);

	/*
	 * Map control/status registers.
	 */
#if 0
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command & 0x000000ff, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
#endif
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_PORTEN | PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command & 0x000000ff, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

	if (my_info_tmp->my_did == MTD800ID) {
		iobase = pci_read_config(dev, MY_PCI_LOIO, 4);
		if (iobase & 0x300)
			MY_USEIOSPACE = 0;
	}
	if (MY_USEIOSPACE) {
		if (!(command & PCIM_CMD_PORTEN)) {
			printf("my%d: failed to enable I/O ports!\n", unit);
			free(sc, M_DEVBUF);
			error = ENXIO;
			goto fail;
		}
#if 0
		if (!pci_map_port(config_id, MY_PCI_LOIO, (u_int16_t *) & (sc->my_bhandle))) {
			printf("my%d: couldn't map ports\n", unit);
			error = ENXIO;
			goto fail;
		}
		  
		sc->my_btag = I386_BUS_SPACE_IO;
#endif
	} else {
		if (!(command & PCIM_CMD_MEMEN)) {
			printf("my%d: failed to enable memory mapping!\n",
			    unit);
			error = ENXIO;
			goto fail;
		}
#if 0
		 if (!pci_map_mem(config_id, MY_PCI_LOMEM, &vbase, &pbase)) {
			printf ("my%d: couldn't map memory\n", unit);
			error = ENXIO;
			goto fail;
		}
		sc->my_btag = I386_BUS_SPACE_MEM;
		sc->my_bhandle = vbase;
#endif
	}

	rid = MY_RID;
	sc->my_res = bus_alloc_resource(dev, MY_RES, &rid,
					0, ~0, 1, RF_ACTIVE);

	if (sc->my_res == NULL) {
		printf("my%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}
	sc->my_btag = rman_get_bustag(sc->my_res);
	sc->my_bhandle = rman_get_bushandle(sc->my_res);

	rid = 0;
	sc->my_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					RF_SHAREABLE | RF_ACTIVE);

	if (sc->my_irq == NULL) {
		printf("my%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, MY_RES, MY_RID, sc->my_res);
		error = ENXIO;
		goto fail;
	}
	error = bus_setup_intr(dev, sc->my_irq, INTR_TYPE_NET,
			       my_intr, sc, &sc->my_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->my_irq);
		bus_release_resource(dev, MY_RES, MY_RID, sc->my_res);
		printf("my%d: couldn't set up irq\n", unit);
		goto fail;
	}
	callout_handle_init(&sc->my_stat_ch);

	sc->my_info = my_info_tmp;

	/* Reset the adapter. */
	my_reset(sc);

	/*
	 * Get station address
	 */
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		eaddr[i] = CSR_READ_1(sc, MY_PAR0 + i);

	/*
	 * A Myson chip was detected. Inform the world.
	 */
	printf("my%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->my_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->my_ldata_ptr = malloc(sizeof(struct my_list_data) + 8,
				  M_DEVBUF, M_NOWAIT);
	if (sc->my_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("my%d: no memory for list buffers!\n", unit);
		error = ENXIO;
		goto fail;
	}
	sc->my_ldata = (struct my_list_data *) sc->my_ldata_ptr;
	round = (uintptr_t)sc->my_ldata_ptr & 0xF;
	roundptr = sc->my_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->my_ldata = (struct my_list_data *) roundptr;
	bzero(sc->my_ldata, sizeof(struct my_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = my_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = my_start;
	ifp->if_watchdog = my_watchdog;
	ifp->if_init = my_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	if (sc->my_info->my_did == MTD803ID)
		sc->my_pinfo = my_phys;
	else {
		if (bootverbose)
			printf("my%d: probing for a PHY\n", sc->my_unit);
		for (i = MY_PHYADDR_MIN; i < MY_PHYADDR_MAX + 1; i++) {
			if (bootverbose)
				printf("my%d: checking address: %d\n",
				    sc->my_unit, i);
			sc->my_phy_addr = i;
			phy_sts = my_phy_readreg(sc, PHY_BMSR);
			if ((phy_sts != 0) && (phy_sts != 0xffff))
				break;
			else
				phy_sts = 0;
		}
		if (phy_sts) {
			phy_vid = my_phy_readreg(sc, PHY_VENID);
			phy_did = my_phy_readreg(sc, PHY_DEVID);
			if (bootverbose) {
				printf("my%d: found PHY at address %d, ",
				    sc->my_unit, sc->my_phy_addr);
				printf("vendor id: %x device id: %x\n",
				    phy_vid, phy_did);
			}
			p = my_phys;
			while (p->my_vid) {
				if (phy_vid == p->my_vid) {
					sc->my_pinfo = p;
					break;
				}
				p++;
			}
			if (sc->my_pinfo == NULL)
				sc->my_pinfo = &my_phys[PHY_UNKNOWN];
			if (bootverbose)
				printf("my%d: PHY type: %s\n",
				       sc->my_unit, sc->my_pinfo->my_name);
		} else {
			printf("my%d: MII without any phy!\n", sc->my_unit);
			error = ENXIO;
			goto fail;
		}
	}

	/* Do ifmedia setup. */
	ifmedia_init(&sc->ifmedia, 0, my_ifmedia_upd, my_ifmedia_sts);
	my_getmode_mii(sc);
	my_autoneg_mii(sc, MY_FLAG_FORCEDELAY, 1);
	media = sc->ifmedia.ifm_media;
	my_stop(sc);
	ifmedia_set(&sc->ifmedia, media);

	ether_ifattach(ifp, eaddr);

#if 0
	at_shutdown(my_shutdown, sc, SHUTDOWN_POST_SYNC);
	shutdownhook_establish(my_shutdown, sc);
#endif
	 
	MY_UNLOCK(sc);
	return (0);

fail:
	MY_UNLOCK(sc);
	mtx_destroy(&sc->my_mtx);
	splx(s);
	return (error);
}

static int
my_detach(device_t dev)
{
	struct my_softc *sc;
	struct ifnet   *ifp;
	int             s;

	s = splimp();
	sc = device_get_softc(dev);
	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ether_ifdetach(ifp);
	my_stop(sc);

#if 0
	bus_generic_detach(dev);
	device_delete_child(dev, sc->rl_miibus);
#endif

	bus_teardown_intr(dev, sc->my_irq, sc->my_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->my_irq);
	bus_release_resource(dev, MY_RES, MY_RID, sc->my_res);
#if 0
	contigfree(sc->my_cdata.my_rx_buf, MY_RXBUFLEN + 32, M_DEVBUF);
#endif
	free(sc, M_DEVBUF);
	MY_UNLOCK(sc);
	splx(s);
	mtx_destroy(&sc->my_mtx);
	return (0);
}


/*
 * Initialize the transmit descriptors.
 */
static int
my_list_tx_init(struct my_softc * sc)
{
	struct my_chain_data *cd;
	struct my_list_data *ld;
	int             i;

	MY_LOCK(sc);
	cd = &sc->my_cdata;
	ld = sc->my_ldata;
	for (i = 0; i < MY_TX_LIST_CNT; i++) {
		cd->my_tx_chain[i].my_ptr = &ld->my_tx_list[i];
		if (i == (MY_TX_LIST_CNT - 1))
			cd->my_tx_chain[i].my_nextdesc = &cd->my_tx_chain[0];
		else
			cd->my_tx_chain[i].my_nextdesc =
			    &cd->my_tx_chain[i + 1];
	}
	cd->my_tx_free = &cd->my_tx_chain[0];
	cd->my_tx_tail = cd->my_tx_head = NULL;
	MY_UNLOCK(sc);
	return (0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that we
 * arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
my_list_rx_init(struct my_softc * sc)
{
	struct my_chain_data *cd;
	struct my_list_data *ld;
	int             i;

	MY_LOCK(sc);
	cd = &sc->my_cdata;
	ld = sc->my_ldata;
	for (i = 0; i < MY_RX_LIST_CNT; i++) {
		cd->my_rx_chain[i].my_ptr =
		    (struct my_desc *) & ld->my_rx_list[i];
		if (my_newbuf(sc, &cd->my_rx_chain[i]) == ENOBUFS) {
			MY_UNLOCK(sc);
			return (ENOBUFS);
		}
		if (i == (MY_RX_LIST_CNT - 1)) {
			cd->my_rx_chain[i].my_nextdesc = &cd->my_rx_chain[0];
			ld->my_rx_list[i].my_next = vtophys(&ld->my_rx_list[0]);
		} else {
			cd->my_rx_chain[i].my_nextdesc =
			    &cd->my_rx_chain[i + 1];
			ld->my_rx_list[i].my_next =
			    vtophys(&ld->my_rx_list[i + 1]);
		}
	}
	cd->my_rx_head = &cd->my_rx_chain[0];
	MY_UNLOCK(sc);
	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
my_newbuf(struct my_softc * sc, struct my_chain_onefrag * c)
{
	struct mbuf    *m_new = NULL;

	MY_LOCK(sc);
	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("my%d: no memory for rx list -- packet dropped!\n",
		       sc->my_unit);
		MY_UNLOCK(sc);
		return (ENOBUFS);
	}
	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("my%d: no memory for rx list -- packet dropped!\n",
		       sc->my_unit);
		m_freem(m_new);
		MY_UNLOCK(sc);
		return (ENOBUFS);
	}
	c->my_mbuf = m_new;
	c->my_ptr->my_data = vtophys(mtod(m_new, caddr_t));
	c->my_ptr->my_ctl = (MCLBYTES - 1) << MY_RBSShift;
	c->my_ptr->my_status = MY_OWNByNIC;
	MY_UNLOCK(sc);
	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to the higher
 * level protocols.
 */
static void
my_rxeof(struct my_softc * sc)
{
	struct ether_header *eh;
	struct mbuf    *m;
	struct ifnet   *ifp;
	struct my_chain_onefrag *cur_rx;
	int             total_len = 0;
	u_int32_t       rxstat;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	while (!((rxstat = sc->my_cdata.my_rx_head->my_ptr->my_status)
	    & MY_OWNByNIC)) {
		cur_rx = sc->my_cdata.my_rx_head;
		sc->my_cdata.my_rx_head = cur_rx->my_nextdesc;

		if (rxstat & MY_ES) {	/* error summary: give up this rx pkt */
			ifp->if_ierrors++;
			cur_rx->my_ptr->my_status = MY_OWNByNIC;
			continue;
		}
		/* No errors; receive the packet. */
		total_len = (rxstat & MY_FLNGMASK) >> MY_FLNGShift;
		total_len -= ETHER_CRC_LEN;

		if (total_len < MINCLSIZE) {
			m = m_devget(mtod(cur_rx->my_mbuf, char *),
			    total_len, 0, ifp, NULL);
			cur_rx->my_ptr->my_status = MY_OWNByNIC;
			if (m == NULL) {
				ifp->if_ierrors++;
				continue;
			}
		} else {
			m = cur_rx->my_mbuf;
			/*
			 * Try to conjure up a new mbuf cluster. If that
			 * fails, it means we have an out of memory condition
			 * and should leave the buffer in place and continue.
			 * This will result in a lost packet, but there's
			 * little else we can do in this situation.
			 */
			if (my_newbuf(sc, cur_rx) == ENOBUFS) {
				ifp->if_ierrors++;
				cur_rx->my_ptr->my_status = MY_OWNByNIC;
				continue;
			}
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		}
		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet, but
		 * don't pass it up to the ether_input() layer unless it's a
		 * broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		if (ifp->if_bpf) {
			BPF_MTAP(ifp, m);
			if (ifp->if_flags & IFF_PROMISC &&
			    (bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
				ETHER_ADDR_LEN) &&
			     (eh->ether_dhost[0] & 1) == 0)) {
				m_freem(m);
				continue;
			}
		}
#endif
		MY_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		MY_LOCK(sc);
	}
	MY_UNLOCK(sc);
	return;
}


/*
 * A frame was downloaded to the chip. It's safe for us to clean up the list
 * buffers.
 */
static void
my_txeof(struct my_softc * sc)
{
	struct my_chain *cur_tx;
	struct ifnet   *ifp;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	/* Clear the timeout timer. */
	ifp->if_timer = 0;
	if (sc->my_cdata.my_tx_head == NULL) {
		MY_UNLOCK(sc);
		return;
	}
	/*
	 * Go through our tx list and free mbufs for those frames that have
	 * been transmitted.
	 */
	while (sc->my_cdata.my_tx_head->my_mbuf != NULL) {
		u_int32_t       txstat;

		cur_tx = sc->my_cdata.my_tx_head;
		txstat = MY_TXSTATUS(cur_tx);
		if ((txstat & MY_OWNByNIC) || txstat == MY_UNSENT)
			break;
		if (!(CSR_READ_4(sc, MY_TCRRCR) & MY_Enhanced)) {
			if (txstat & MY_TXERR) {
				ifp->if_oerrors++;
				if (txstat & MY_EC) /* excessive collision */
					ifp->if_collisions++;
				if (txstat & MY_LC)	/* late collision */
					ifp->if_collisions++;
			}
			ifp->if_collisions += (txstat & MY_NCRMASK) >>
			    MY_NCRShift;
		}
		ifp->if_opackets++;
		m_freem(cur_tx->my_mbuf);
		cur_tx->my_mbuf = NULL;
		if (sc->my_cdata.my_tx_head == sc->my_cdata.my_tx_tail) {
			sc->my_cdata.my_tx_head = NULL;
			sc->my_cdata.my_tx_tail = NULL;
			break;
		}
		sc->my_cdata.my_tx_head = cur_tx->my_nextdesc;
	}
	if (CSR_READ_4(sc, MY_TCRRCR) & MY_Enhanced) {
		ifp->if_collisions += (CSR_READ_4(sc, MY_TSR) & MY_NCRMask);
	}
	MY_UNLOCK(sc);
	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void
my_txeoc(struct my_softc * sc)
{
	struct ifnet   *ifp;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	if (sc->my_cdata.my_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->my_cdata.my_tx_tail = NULL;
		if (sc->my_want_auto)
			my_autoneg_mii(sc, MY_FLAG_SCHEDDELAY, 1);
	} else {
		if (MY_TXOWN(sc->my_cdata.my_tx_head) == MY_UNSENT) {
			MY_TXOWN(sc->my_cdata.my_tx_head) = MY_OWNByNIC;
			ifp->if_timer = 5;
			CSR_WRITE_4(sc, MY_TXPDR, 0xFFFFFFFF);
		}
	}
	MY_UNLOCK(sc);
	return;
}

static void
my_intr(void *arg)
{
	struct my_softc *sc;
	struct ifnet   *ifp;
	u_int32_t       status;

	sc = arg;
	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	if (!(ifp->if_flags & IFF_UP)) {
		MY_UNLOCK(sc);
		return;
	}
	/* Disable interrupts. */
	CSR_WRITE_4(sc, MY_IMR, 0x00000000);

	for (;;) {
		status = CSR_READ_4(sc, MY_ISR);
		status &= MY_INTRS;
		if (status)
			CSR_WRITE_4(sc, MY_ISR, status);
		else
			break;

		if (status & MY_RI)	/* receive interrupt */
			my_rxeof(sc);

		if ((status & MY_RBU) || (status & MY_RxErr)) {
			/* rx buffer unavailable or rx error */
			ifp->if_ierrors++;
#ifdef foo
			my_stop(sc);
			my_reset(sc);
			my_init(sc);
#endif
		}
		if (status & MY_TI)	/* tx interrupt */
			my_txeof(sc);
		if (status & MY_ETI)	/* tx early interrupt */
			my_txeof(sc);
		if (status & MY_TBU)	/* tx buffer unavailable */
			my_txeoc(sc);

#if 0				/* 90/1/18 delete */
		if (status & MY_FBE) {
			my_reset(sc);
			my_init(sc);
		}
#endif

	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, MY_IMR, MY_INTRS);
	if (ifp->if_snd.ifq_head != NULL)
		my_start(ifp);
	MY_UNLOCK(sc);
	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
my_encap(struct my_softc * sc, struct my_chain * c, struct mbuf * m_head)
{
	struct my_desc *f = NULL;
	int             total_len;
	struct mbuf    *m, *m_new = NULL;

	MY_LOCK(sc);
	/* calculate the total tx pkt length */
	total_len = 0;
	for (m = m_head; m != NULL; m = m->m_next)
		total_len += m->m_len;
	/*
	 * Start packing the mbufs in this chain into the fragment pointers.
	 * Stop when we run out of fragments or hit the end of the mbuf
	 * chain.
	 */
	m = m_head;
	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("my%d: no memory for tx list", sc->my_unit);
		MY_UNLOCK(sc);
		return (1);
	}
	if (m_head->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			printf("my%d: no memory for tx list", sc->my_unit);
			MY_UNLOCK(sc);
			return (1);
		}
	}
	m_copydata(m_head, 0, m_head->m_pkthdr.len, mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
	m_freem(m_head);
	m_head = m_new;
	f = &c->my_ptr->my_frag[0];
	f->my_status = 0;
	f->my_data = vtophys(mtod(m_new, caddr_t));
	total_len = m_new->m_len;
	f->my_ctl = MY_TXFD | MY_TXLD | MY_CRCEnable | MY_PADEnable;
	f->my_ctl |= total_len << MY_PKTShift;	/* pkt size */
	f->my_ctl |= total_len;	/* buffer size */
	/* 89/12/29 add, for mtd891 *//* [ 89? ] */
	if (sc->my_info->my_did == MTD891ID)
		f->my_ctl |= MY_ETIControl | MY_RetryTxLC;
	c->my_mbuf = m_head;
	c->my_lastdesc = 0;
	MY_TXNEXT(c) = vtophys(&c->my_nextdesc->my_ptr->my_frag[0]);
	MY_UNLOCK(sc);
	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
static void
my_start(struct ifnet * ifp)
{
	struct my_softc *sc;
	struct mbuf    *m_head = NULL;
	struct my_chain *cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;
	MY_LOCK(sc);
	if (sc->my_autoneg) {
		sc->my_tx_pend = 1;
		MY_UNLOCK(sc);
		return;
	}
	/*
	 * Check for an available queue slot. If there are none, punt.
	 */
	if (sc->my_cdata.my_tx_free->my_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		MY_UNLOCK(sc);
		return;
	}
	start_tx = sc->my_cdata.my_tx_free;
	while (sc->my_cdata.my_tx_free->my_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->my_cdata.my_tx_free;
		sc->my_cdata.my_tx_free = cur_tx->my_nextdesc;

		/* Pack the data into the descriptor. */
		my_encap(sc, cur_tx, m_head);

		if (cur_tx != start_tx)
			MY_TXOWN(cur_tx) = MY_OWNByNIC;
#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame to
		 * him.
		 */
		BPF_MTAP(ifp, cur_tx->my_mbuf);
#endif
	}
	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL) {
		MY_UNLOCK(sc);
		return;
	}
	/*
	 * Place the request for the upload interrupt in the last descriptor
	 * in the chain. This way, if we're chaining several packets at once,
	 * we'll only get an interupt once for the whole chain rather than
	 * once for each packet.
	 */
	MY_TXCTL(cur_tx) |= MY_TXIC;
	cur_tx->my_ptr->my_frag[0].my_ctl |= MY_TXIC;
	sc->my_cdata.my_tx_tail = cur_tx;
	if (sc->my_cdata.my_tx_head == NULL)
		sc->my_cdata.my_tx_head = start_tx;
	MY_TXOWN(start_tx) = MY_OWNByNIC;
	CSR_WRITE_4(sc, MY_TXPDR, 0xFFFFFFFF);	/* tx polling demand */

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	MY_UNLOCK(sc);
	return;
}

static void
my_init(void *xsc)
{
	struct my_softc *sc = xsc;
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	int             s;
	u_int16_t       phy_bmcr = 0;

	MY_LOCK(sc);
	if (sc->my_autoneg) {
		MY_UNLOCK(sc);
		return;
	}
	s = splimp();
	if (sc->my_pinfo != NULL)
		phy_bmcr = my_phy_readreg(sc, PHY_BMCR);
	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	my_stop(sc);
	my_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
#if 0				/* 89/9/1 modify,  */
	CSR_WRITE_4(sc, MY_BCR, MY_RPBLE512);
	CSR_WRITE_4(sc, MY_TCRRCR, MY_TFTSF);
#endif
	CSR_WRITE_4(sc, MY_BCR, MY_PBL8);
	CSR_WRITE_4(sc, MY_TCRRCR, MY_TFTSF | MY_RBLEN | MY_RPBLE512);
	/*
	 * 89/12/29 add, for mtd891,
	 */
	if (sc->my_info->my_did == MTD891ID) {
		MY_SETBIT(sc, MY_BCR, MY_PROG);
		MY_SETBIT(sc, MY_TCRRCR, MY_Enhanced);
	}
	my_setcfg(sc, phy_bmcr);
	/* Init circular RX list. */
	if (my_list_rx_init(sc) == ENOBUFS) {
		printf("my%d: init failed: no memory for rx buffers\n",
		    sc->my_unit);
		my_stop(sc);
		(void)splx(s);
		MY_UNLOCK(sc);
		return;
	}
	/* Init TX descriptors. */
	my_list_tx_init(sc);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		MY_SETBIT(sc, MY_TCRRCR, MY_PROM);
	else
		MY_CLRBIT(sc, MY_TCRRCR, MY_PROM);

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		MY_SETBIT(sc, MY_TCRRCR, MY_AB);
	else
		MY_CLRBIT(sc, MY_TCRRCR, MY_AB);

	/*
	 * Program the multicast filter, if necessary.
	 */
	my_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	MY_CLRBIT(sc, MY_TCRRCR, MY_RE);
	CSR_WRITE_4(sc, MY_RXLBA, vtophys(&sc->my_ldata->my_rx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, MY_IMR, MY_INTRS);
	CSR_WRITE_4(sc, MY_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	MY_SETBIT(sc, MY_TCRRCR, MY_RE);
	MY_CLRBIT(sc, MY_TCRRCR, MY_TE);
	CSR_WRITE_4(sc, MY_TXLBA, vtophys(&sc->my_ldata->my_tx_list[0]));
	MY_SETBIT(sc, MY_TCRRCR, MY_TE);

	/* Restore state of BMCR */
	if (sc->my_pinfo != NULL)
		my_phy_writereg(sc, PHY_BMCR, phy_bmcr);
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	(void)splx(s);
	MY_UNLOCK(sc);
	return;
}

/*
 * Set media options.
 */

static int
my_ifmedia_upd(struct ifnet * ifp)
{
	struct my_softc *sc;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	MY_LOCK(sc);
	ifm = &sc->ifmedia;
	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER) {
		MY_UNLOCK(sc);
		return (EINVAL);
	}
	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		my_autoneg_mii(sc, MY_FLAG_SCHEDDELAY, 1);
	else
		my_setmode_mii(sc, ifm->ifm_media);
	MY_UNLOCK(sc);
	return (0);
}

/*
 * Report current media status.
 */

static void
my_ifmedia_sts(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct my_softc *sc;
	u_int16_t advert = 0, ability = 0;

	sc = ifp->if_softc;
	MY_LOCK(sc);
	ifmr->ifm_active = IFM_ETHER;
	if (!(my_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
#if 0				/* this version did not support 1000M, */
		if (my_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_1000)
			ifmr->ifm_active = IFM_ETHER | IFM_1000TX;
#endif
		if (my_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER | IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER | IFM_10_T;
		if (my_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;

		MY_UNLOCK(sc);
		return;
	}
	ability = my_phy_readreg(sc, PHY_LPAR);
	advert = my_phy_readreg(sc, PHY_ANAR);

#if 0				/* this version did not support 1000M, */
	if (sc->my_pinfo->my_vid = MarvellPHYID0) {
		ability2 = my_phy_readreg(sc, PHY_1000SR);
		if (ability2 & PHY_1000SR_1000BTXFULL) {
			advert = 0;
			ability = 0;
	  		ifmr->ifm_active = IFM_ETHER|IFM_1000_T|IFM_FDX;
	  	} else if (ability & PHY_1000SR_1000BTXHALF) {
			advert = 0;
			ability = 0;
			ifmr->ifm_active = IFM_ETHER|IFM_1000_T|IFM_HDX;
		}
	}
#endif
	if (advert & PHY_ANAR_100BT4 && ability & PHY_ANAR_100BT4)
		ifmr->ifm_active = IFM_ETHER | IFM_100_T4;
	else if (advert & PHY_ANAR_100BTXFULL && ability & PHY_ANAR_100BTXFULL)
		ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_FDX;
	else if (advert & PHY_ANAR_100BTXHALF && ability & PHY_ANAR_100BTXHALF)
		ifmr->ifm_active = IFM_ETHER | IFM_100_TX | IFM_HDX;
	else if (advert & PHY_ANAR_10BTFULL && ability & PHY_ANAR_10BTFULL)
		ifmr->ifm_active = IFM_ETHER | IFM_10_T | IFM_FDX;
	else if (advert & PHY_ANAR_10BTHALF && ability & PHY_ANAR_10BTHALF)
		ifmr->ifm_active = IFM_ETHER | IFM_10_T | IFM_HDX;
	MY_UNLOCK(sc);
	return;
}

static int
my_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct my_softc *sc = ifp->if_softc;
	struct ifreq   *ifr = (struct ifreq *) data;
	int             s, error = 0;

	s = splimp();
	MY_LOCK(sc);
	switch (command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			my_init(sc);
		else if (ifp->if_flags & IFF_RUNNING)
			my_stop(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		my_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}
	MY_UNLOCK(sc);
	(void)splx(s);
	return (error);
}

static void
my_watchdog(struct ifnet * ifp)
{
	struct my_softc *sc;

	sc = ifp->if_softc;
	MY_LOCK(sc);
	if (sc->my_autoneg) {
		my_autoneg_mii(sc, MY_FLAG_DELAYTIMEO, 1);
		MY_UNLOCK(sc);
		return;
	}
	ifp->if_oerrors++;
	printf("my%d: watchdog timeout\n", sc->my_unit);
	if (!(my_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("my%d: no carrier - transceiver cable problem?\n",
		    sc->my_unit);
	my_stop(sc);
	my_reset(sc);
	my_init(sc);
	if (ifp->if_snd.ifq_head != NULL)
		my_start(ifp);
	MY_LOCK(sc);
	return;
}


/*
 * Stop the adapter and free any mbufs allocated to the RX and TX lists.
 */
static void
my_stop(struct my_softc * sc)
{
	register int    i;
	struct ifnet   *ifp;

	MY_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	MY_CLRBIT(sc, MY_TCRRCR, (MY_RE | MY_TE));
	CSR_WRITE_4(sc, MY_IMR, 0x00000000);
	CSR_WRITE_4(sc, MY_TXLBA, 0x00000000);
	CSR_WRITE_4(sc, MY_RXLBA, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < MY_RX_LIST_CNT; i++) {
		if (sc->my_cdata.my_rx_chain[i].my_mbuf != NULL) {
			m_freem(sc->my_cdata.my_rx_chain[i].my_mbuf);
			sc->my_cdata.my_rx_chain[i].my_mbuf = NULL;
		}
	}
	bzero((char *)&sc->my_ldata->my_rx_list,
	    sizeof(sc->my_ldata->my_rx_list));
	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < MY_TX_LIST_CNT; i++) {
		if (sc->my_cdata.my_tx_chain[i].my_mbuf != NULL) {
			m_freem(sc->my_cdata.my_tx_chain[i].my_mbuf);
			sc->my_cdata.my_tx_chain[i].my_mbuf = NULL;
		}
	}
	bzero((char *)&sc->my_ldata->my_tx_list,
	    sizeof(sc->my_ldata->my_tx_list));
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	MY_UNLOCK(sc);
	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't get confused
 * by errant DMAs when rebooting.
 */
static void
my_shutdown(device_t dev)
{
	struct my_softc *sc;

	sc = device_get_softc(dev);
	my_stop(sc);
	return;
}
