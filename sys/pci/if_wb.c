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
 *	$Id: if_wb.c,v 1.6.2.2 1999/05/13 21:19:31 wpaul Exp $
 */

/*
 * Winbond fast ethernet PCI NIC driver
 *
 * Supports various cheap network adapters based on the Winbond W89C840F
 * fast ethernet controller chip. This includes adapters manufactured by
 * Winbond itself and some made by Linksys.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Winbond W89C840F chip is a bus master; in some ways it resembles
 * a DEC 'tulip' chip, only not as complicated. Unfortunately, it has
 * one major difference which is that while the registers do many of
 * the same things as a tulip adapter, the offsets are different: where
 * tulip registers are typically spaced 8 bytes apart, the Winbond
 * registers are spaced 4 bytes apart. The receiver filter is also
 * programmed differently.
 * 
 * Like the tulip, the Winbond chip uses small descriptors containing
 * a status word, a control word and 32-bit areas that can either be used
 * to point to two external data blocks, or to point to a single block
 * and another descriptor in a linked list. Descriptors can be grouped
 * together in blocks to form fixed length rings or can be chained
 * together in linked lists. A single packet may be spread out over
 * several descriptors if necessary.
 *
 * For the receive ring, this driver uses a linked list of descriptors,
 * each pointing to a single mbuf cluster buffer, which us large enough
 * to hold an entire packet. The link list is looped back to created a
 * closed ring.
 *
 * For transmission, the driver creates a linked list of 'super descriptors'
 * which each contain several individual descriptors linked toghether.
 * Each 'super descriptor' contains WB_MAXFRAGS descriptors, which we
 * abuse as fragment pointers. This allows us to use a buffer managment
 * scheme very similar to that used in the ThunderLAN and Etherlink XL
 * drivers.
 *
 * Autonegotiation is performed using the external PHY via the MII bus.
 * The sample boards I have all use a Davicom PHY.
 *
 * Note: the author of the Linux driver for the Winbond chip alludes
 * to some sort of flaw in the chip's design that seems to mandate some
 * drastic workaround which signigicantly impairs transmit performance.
 * I have no idea what he's on about: transmit performance with all
 * three of my test boards seems fine.
 */

#include "bpf.h"
#include "opt_bdg.h"

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

#ifdef BRIDGE
#include <net/bridge.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#define WB_USEIOSPACE

/* #define WB_BACKGROUND_AUTONEG */

#include <pci/if_wbreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_wb.c,v 1.6.2.2 1999/05/13 21:19:31 wpaul Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct wb_type wb_devs[] = {
	{ WB_VENDORID, WB_DEVICEID_840F,
		"Winbond W89C840F 10/100BaseTX" },
	{ CP_VENDORID, CP_DEVICEID_RL100,
		"Compex RL100-ATX 10/100baseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct wb_type wb_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long wb_count = 0;
static const char *wb_probe	__P((pcici_t, pcidi_t));
static void wb_attach		__P((pcici_t, int));

static int wb_newbuf		__P((struct wb_softc *,
					struct wb_chain_onefrag *,
					struct mbuf *));
static int wb_encap		__P((struct wb_softc *, struct wb_chain *,
						struct mbuf *));

static void wb_rxeof		__P((struct wb_softc *));
static void wb_rxeoc		__P((struct wb_softc *));
static void wb_txeof		__P((struct wb_softc *));
static void wb_txeoc		__P((struct wb_softc *));
static void wb_intr		__P((void *));
static void wb_start		__P((struct ifnet *));
static int wb_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void wb_init		__P((void *));
static void wb_stop		__P((struct wb_softc *));
static void wb_watchdog		__P((struct ifnet *));
static void wb_shutdown		__P((int, void *));
static int wb_ifmedia_upd	__P((struct ifnet *));
static void wb_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void wb_eeprom_putbyte	__P((struct wb_softc *, int));
static void wb_eeprom_getword	__P((struct wb_softc *, int, u_int16_t *));
static void wb_read_eeprom	__P((struct wb_softc *, caddr_t, int,
							int, int));
static void wb_mii_sync		__P((struct wb_softc *));
static void wb_mii_send		__P((struct wb_softc *, u_int32_t, int));
static int wb_mii_readreg	__P((struct wb_softc *, struct wb_mii_frame *));
static int wb_mii_writereg	__P((struct wb_softc *, struct wb_mii_frame *));
static u_int16_t wb_phy_readreg	__P((struct wb_softc *, int));
static void wb_phy_writereg	__P((struct wb_softc *, int, int));

static void wb_autoneg_xmit	__P((struct wb_softc *));
static void wb_autoneg_mii	__P((struct wb_softc *, int, int));
static void wb_setmode_mii	__P((struct wb_softc *, int));
static void wb_getmode_mii	__P((struct wb_softc *));
static void wb_setcfg		__P((struct wb_softc *, int));
static u_int8_t wb_calchash	__P((caddr_t));
static void wb_setmulti		__P((struct wb_softc *));
static void wb_reset		__P((struct wb_softc *));
static int wb_list_rx_init	__P((struct wb_softc *));
static int wb_list_tx_init	__P((struct wb_softc *));

#define WB_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define WB_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, WB_SIO,				\
		CSR_READ_4(sc, WB_SIO) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, WB_SIO,				\
		CSR_READ_4(sc, WB_SIO) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void wb_eeprom_putbyte(sc, addr)
	struct wb_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | WB_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(WB_SIO_EE_DATAIN);
		} else {
			SIO_CLR(WB_SIO_EE_DATAIN);
		}
		DELAY(100);
		SIO_SET(WB_SIO_EE_CLK);
		DELAY(150);
		SIO_CLR(WB_SIO_EE_CLK);
		DELAY(100);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void wb_eeprom_getword(sc, addr, dest)
	struct wb_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, WB_SIO, WB_SIO_EESEL|WB_SIO_EE_CS);

	/*
	 * Send address of word we want to read.
	 */
	wb_eeprom_putbyte(sc, addr);

	CSR_WRITE_4(sc, WB_SIO, WB_SIO_EESEL|WB_SIO_EE_CS);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(WB_SIO_EE_CLK);
		DELAY(100);
		if (CSR_READ_4(sc, WB_SIO) & WB_SIO_EE_DATAOUT)
			word |= i;
		SIO_CLR(WB_SIO_EE_CLK);
		DELAY(100);
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_4(sc, WB_SIO, 0);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void wb_read_eeprom(sc, dest, off, cnt, swap)
	struct wb_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		wb_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void wb_mii_sync(sc)
	struct wb_softc		*sc;
{
	register int		i;

	SIO_SET(WB_SIO_MII_DIR|WB_SIO_MII_DATAIN);

	for (i = 0; i < 32; i++) {
		SIO_SET(WB_SIO_MII_CLK);
		DELAY(1);
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void wb_mii_send(sc, bits, cnt)
	struct wb_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	SIO_CLR(WB_SIO_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			SIO_SET(WB_SIO_MII_DATAIN);
                } else {
			SIO_CLR(WB_SIO_MII_DATAIN);
                }
		DELAY(1);
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
		SIO_SET(WB_SIO_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int wb_mii_readreg(sc, frame)
	struct wb_softc		*sc;
	struct wb_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = WB_MII_STARTDELIM;
	frame->mii_opcode = WB_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_4(sc, WB_SIO, 0);

	/*
 	 * Turn on data xmit.
	 */
	SIO_SET(WB_SIO_MII_DIR);

	wb_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	wb_mii_send(sc, frame->mii_stdelim, 2);
	wb_mii_send(sc, frame->mii_opcode, 2);
	wb_mii_send(sc, frame->mii_phyaddr, 5);
	wb_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	SIO_CLR((WB_SIO_MII_CLK|WB_SIO_MII_DATAIN));
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(WB_SIO_MII_DIR);
	/* Check for ack */
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, WB_SIO) & WB_SIO_MII_DATAOUT;
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(WB_SIO_MII_CLK);
			DELAY(1);
			SIO_SET(WB_SIO_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(WB_SIO_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, WB_SIO) & WB_SIO_MII_DATAOUT)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(WB_SIO_MII_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int wb_mii_writereg(sc, frame)
	struct wb_softc		*sc;
	struct wb_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = WB_MII_STARTDELIM;
	frame->mii_opcode = WB_MII_WRITEOP;
	frame->mii_turnaround = WB_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	SIO_SET(WB_SIO_MII_DIR);

	wb_mii_sync(sc);

	wb_mii_send(sc, frame->mii_stdelim, 2);
	wb_mii_send(sc, frame->mii_opcode, 2);
	wb_mii_send(sc, frame->mii_phyaddr, 5);
	wb_mii_send(sc, frame->mii_regaddr, 5);
	wb_mii_send(sc, frame->mii_turnaround, 2);
	wb_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(WB_SIO_MII_CLK);
	DELAY(1);
	SIO_CLR(WB_SIO_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	SIO_CLR(WB_SIO_MII_DIR);

	splx(s);

	return(0);
}

static u_int16_t wb_phy_readreg(sc, reg)
	struct wb_softc		*sc;
	int			reg;
{
	struct wb_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->wb_phy_addr;
	frame.mii_regaddr = reg;
	wb_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static void wb_phy_writereg(sc, reg, data)
	struct wb_softc		*sc;
	int			reg;
	int			data;
{
	struct wb_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->wb_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	wb_mii_writereg(sc, &frame);

	return;
}

static u_int8_t wb_calchash(addr)
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

	/*
	 * return the filter bit position
	 * Note: I arrived at the following nonsense
	 * through experimentation. It's not the usual way to
	 * generate the bit position but it's the only thing
	 * I could come up with that works.
	 */
	return(~(crc >> 26) & 0x0000003F);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void wb_setmulti(sc)
	struct wb_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, WB_NETCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= WB_NETCFG_RX_MULTI;
		CSR_WRITE_4(sc, WB_NETCFG, rxfilt);
		CSR_WRITE_4(sc, WB_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, WB_MAR1, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, WB_MAR0, 0);
	CSR_WRITE_4(sc, WB_MAR1, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = wb_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= WB_NETCFG_RX_MULTI;
	else
		rxfilt &= ~WB_NETCFG_RX_MULTI;

	CSR_WRITE_4(sc, WB_MAR0, hashes[0]);
	CSR_WRITE_4(sc, WB_MAR1, hashes[1]);
	CSR_WRITE_4(sc, WB_NETCFG, rxfilt);

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void wb_autoneg_xmit(sc)
	struct wb_softc		*sc;
{
	u_int16_t		phy_sts;

	wb_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(wb_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = wb_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	wb_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void wb_autoneg_mii(sc, flag, verbose)
	struct wb_softc		*sc;
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
	phy_sts = wb_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("wb%d: autonegotiation not supported\n",
							sc->wb_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case WB_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		wb_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case WB_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise wb_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->wb_cdata.wb_tx_head != NULL) {
			sc->wb_want_auto = 1;
			return;
		}
		wb_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->wb_autoneg = 1;
		sc->wb_want_auto = 0;
		return;
		break;
	case WB_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->wb_autoneg = 0;
		break;
	default:
		printf("wb%d: invalid autoneg flag: %d\n", sc->wb_unit, flag);
		return;
	}

	if (wb_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("wb%d: autoneg complete, ", sc->wb_unit);
		phy_sts = wb_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("wb%d: autoneg not complete, ", sc->wb_unit);
	}

	media = wb_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (wb_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = wb_phy_readreg(sc, PHY_ANAR);
		ability = wb_phy_readreg(sc, PHY_LPAR);

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
		} else /* if (advert & PHY_ANAR_10BTHALF &&
			ability & PHY_ANAR_10BTHALF) */ {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}

		media &= ~PHY_BMCR_AUTONEGENBL;

		/* Set ASIC's duplex mode to match the PHY. */
		wb_setcfg(sc, media);
		wb_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	wb_init(sc);

	if (sc->wb_tx_pend) {
		sc->wb_autoneg = 0;
		sc->wb_tx_pend = 0;
		wb_start(ifp);
	}

	return;
}

static void wb_getmode_mii(sc)
	struct wb_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = wb_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("wb%d: PHY status word: %x\n", sc->wb_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("wb%d: 10Mbps half-duplex mode supported\n",
								sc->wb_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("wb%d: 10Mbps full-duplex mode supported\n",
								sc->wb_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("wb%d: 100Mbps half-duplex mode supported\n",
								sc->wb_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("wb%d: 100Mbps full-duplex mode supported\n",
								sc->wb_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("wb%d: 100baseT4 mode supported\n", sc->wb_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("wb%d: forcing on autoneg support for BT4\n",
							 sc->wb_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("wb%d: autoneg supported\n", sc->wb_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void wb_setmode_mii(sc, media)
	struct wb_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->wb_autoneg) {
		printf("wb%d: canceling autoneg session\n", sc->wb_unit);
		ifp->if_timer = sc->wb_autoneg = sc->wb_want_auto = 0;
		bmcr = wb_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		wb_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("wb%d: selecting MII, ", sc->wb_unit);

	bmcr = wb_phy_readreg(sc, PHY_BMCR);

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

	wb_setcfg(sc, bmcr);
	wb_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

/*
 * The Winbond manual states that in order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void wb_setcfg(sc, bmcr)
	struct wb_softc		*sc;
	int			bmcr;
{
	int			i, restart = 0;

	if (CSR_READ_4(sc, WB_NETCFG) & (WB_NETCFG_TX_ON|WB_NETCFG_RX_ON)) {
		restart = 1;
		WB_CLRBIT(sc, WB_NETCFG, (WB_NETCFG_TX_ON|WB_NETCFG_RX_ON));

		for (i = 0; i < WB_TIMEOUT; i++) {
			DELAY(10);
			if ((CSR_READ_4(sc, WB_ISR) & WB_ISR_TX_IDLE) &&
				(CSR_READ_4(sc, WB_ISR) & WB_ISR_RX_IDLE))
				break;
		}

		if (i == WB_TIMEOUT)
			printf("wb%d: failed to force tx and "
				"rx to idle state\n", sc->wb_unit);
	}

	if (bmcr & PHY_BMCR_SPEEDSEL)
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_100MBPS);
	else
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_100MBPS);

	if (bmcr & PHY_BMCR_DUPLEX)
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_FULLDUPLEX);
	else
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_FULLDUPLEX);

	if (restart)
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON|WB_NETCFG_RX_ON);

	return;
}

static void wb_reset(sc)
	struct wb_softc		*sc;
{
	register int		i;

	WB_SETBIT(sc, WB_BUSCTL, WB_BUSCTL_RESET);

	for (i = 0; i < WB_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, WB_BUSCTL) & WB_BUSCTL_RESET))
			break;
	}
	if (i == WB_TIMEOUT)
		printf("wb%d: reset never completed!\n", sc->wb_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	/* Reset the damn PHY too. */
	if (sc->wb_pinfo != NULL)
		wb_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);

        return;
}

/*
 * Probe for a Winbond chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
wb_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct wb_type		*t;

	t = wb_devs;

	while(t->wb_name != NULL) {
		if ((device_id & 0xFFFF) == t->wb_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->wb_did) {
			return(t->wb_name);
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
wb_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef WB_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct wb_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct wb_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;

	s = splimp();

	sc = malloc(sizeof(struct wb_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("wb%d: no memory for softc struct!\n", unit);
		return;
	}
	bzero(sc, sizeof(struct wb_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, WB_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, WB_PCI_PWRMGMTCTRL);
		if (command & WB_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, WB_PCI_LOIO);
			membase = pci_conf_read(config_id, WB_PCI_LOMEM);
			irq = pci_conf_read(config_id, WB_PCI_INTLINE);

			/* Reset the power state. */
			printf("wb%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & WB_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, WB_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, WB_PCI_LOIO, iobase);
			pci_conf_write(config_id, WB_PCI_LOMEM, membase);
			pci_conf_write(config_id, WB_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef WB_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("wb%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, WB_PCI_LOIO,
			(pci_port_t *)&(sc->wb_bhandle))) {
		printf ("wb%d: couldn't map ports\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->wb_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->wb_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("wb%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, WB_PCI_LOMEM, &vbase, &pbase)) {
		printf ("wb%d: couldn't map memory\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->wb_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->wb_btag = I386_BUS_SPACE_MEM;
#endif
	sc->wb_bhandle = vbase;
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, wb_intr, sc, &net_imask)) {
		printf("wb%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	wb_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	wb_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 0);

	/*
	 * A Winbond chip was detected. Inform the world.
	 */
	printf("wb%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->wb_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->wb_ldata_ptr = malloc(sizeof(struct wb_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->wb_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("wb%d: no memory for list buffers!\n", unit);
		return;
	}

	sc->wb_ldata = (struct wb_list_data *)sc->wb_ldata_ptr;
	round = (uintptr_t)sc->wb_ldata_ptr & 0xF;
	roundptr = sc->wb_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->wb_ldata = (struct wb_list_data *)roundptr;
	bzero(sc->wb_ldata, sizeof(struct wb_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "wb";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wb_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = wb_start;
	ifp->if_watchdog = wb_watchdog;
	ifp->if_init = wb_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = WB_TX_LIST_CNT - 1;

	if (bootverbose)
		printf("wb%d: probing for a PHY\n", sc->wb_unit);
	for (i = WB_PHYADDR_MIN; i < WB_PHYADDR_MAX + 1; i++) {
		if (bootverbose)
			printf("wb%d: checking address: %d\n",
						sc->wb_unit, i);
		sc->wb_phy_addr = i;
		wb_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(wb_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = wb_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = wb_phy_readreg(sc, PHY_VENID);
		phy_did = wb_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("wb%d: found PHY at address %d, ",
					sc->wb_unit, sc->wb_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
				phy_vid, phy_did);
		p = wb_phys;
		while(p->wb_vid) {
			if (phy_vid == p->wb_vid &&
				(phy_did | 0x000F) == p->wb_did) {
				sc->wb_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->wb_pinfo == NULL)
			sc->wb_pinfo = &wb_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("wb%d: PHY type: %s\n",
				sc->wb_unit, sc->wb_pinfo->wb_name);
	} else {
		printf("wb%d: MII without any phy!\n", sc->wb_unit);
		goto fail;
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, wb_ifmedia_upd, wb_ifmedia_sts);

	wb_getmode_mii(sc);
	wb_autoneg_mii(sc, WB_FLAG_FORCEDELAY, 1);
	media = sc->ifmedia.ifm_media;
	wb_stop(sc);

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(wb_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int wb_list_tx_init(sc)
	struct wb_softc		*sc;
{
	struct wb_chain_data	*cd;
	struct wb_list_data	*ld;
	int			i;

	cd = &sc->wb_cdata;
	ld = sc->wb_ldata;

	for (i = 0; i < WB_TX_LIST_CNT; i++) {
		cd->wb_tx_chain[i].wb_ptr = &ld->wb_tx_list[i];
		if (i == (WB_TX_LIST_CNT - 1)) {
			cd->wb_tx_chain[i].wb_nextdesc =
				&cd->wb_tx_chain[0];
		} else {
			cd->wb_tx_chain[i].wb_nextdesc =
				&cd->wb_tx_chain[i + 1];
		}
	}

	cd->wb_tx_free = &cd->wb_tx_chain[0];
	cd->wb_tx_tail = cd->wb_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int wb_list_rx_init(sc)
	struct wb_softc		*sc;
{
	struct wb_chain_data	*cd;
	struct wb_list_data	*ld;
	int			i;

	cd = &sc->wb_cdata;
	ld = sc->wb_ldata;

	for (i = 0; i < WB_RX_LIST_CNT; i++) {
		cd->wb_rx_chain[i].wb_ptr =
			(struct wb_desc *)&ld->wb_rx_list[i];
		if (wb_newbuf(sc, &cd->wb_rx_chain[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (WB_RX_LIST_CNT - 1)) {
			cd->wb_rx_chain[i].wb_nextdesc = &cd->wb_rx_chain[0];
			ld->wb_rx_list[i].wb_next = 
					vtophys(&ld->wb_rx_list[0]);
		} else {
			cd->wb_rx_chain[i].wb_nextdesc =
					&cd->wb_rx_chain[i + 1];
			ld->wb_rx_list[i].wb_next =
					vtophys(&ld->wb_rx_list[i + 1]);
		}
	}

	cd->wb_rx_head = &cd->wb_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int wb_newbuf(sc, c, m)
	struct wb_softc		*sc;
	struct wb_chain_onefrag	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("wb%d: no memory for rx "
			    "list -- packet dropped!\n", sc->wb_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("wb%d: no memory for rx "
			    "list -- packet dropped!\n", sc->wb_unit);
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

	c->wb_mbuf = m_new;
	c->wb_ptr->wb_data = vtophys(mtod(m_new, caddr_t));
	c->wb_ptr->wb_ctl = WB_RXCTL_RLINK | (MCLBYTES - 1);
	c->wb_ptr->wb_status = WB_RXSTAT;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void wb_rxeof(sc)
	struct wb_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct wb_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->wb_cdata.wb_rx_head->wb_ptr->wb_status) &
							WB_RXSTAT_OWN)) {
		struct mbuf		*m0 = NULL;

		cur_rx = sc->wb_cdata.wb_rx_head;
		sc->wb_cdata.wb_rx_head = cur_rx->wb_nextdesc;
		m = cur_rx->wb_mbuf;

		if ((rxstat & WB_RXSTAT_MIIERR)
			 || WB_RXBYTES(cur_rx->wb_ptr->wb_status) == 0) {
			ifp->if_ierrors++;
			wb_reset(sc);
			printf("wb%x: receiver babbling: possible chip "
				"bug, forcing reset\n", sc->wb_unit);
			ifp->if_flags |= IFF_OACTIVE;
			ifp->if_timer = 2;
			return;
		}

		if (rxstat & WB_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			wb_newbuf(sc, cur_rx, m);
			continue;
		}

		/* No errors; receive the packet. */	
		total_len = WB_RXBYTES(cur_rx->wb_ptr->wb_status);

		/*
		 * XXX The Winbond chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
	 	 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		     total_len + ETHER_ALIGN, 0, ifp, NULL);
		wb_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);

#ifdef BRIDGE
		if (do_bridge) {
			struct ifnet		*bdg_ifp;
			bdg_ifp = bridge_in(m);
			if (bdg_ifp != BDG_LOCAL && bdg_ifp != BDG_DROP)
				bdg_forward(&m, bdg_ifp);
			if (((bdg_ifp != BDG_LOCAL) && (bdg_ifp != BDG_BCAST) &&
			    (bdg_ifp != BDG_MCAST)) || bdg_ifp == BDG_DROP) {
				m_freem(m);
				continue;
			}
		}
#endif

#if NBPF > 0
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

void wb_rxeoc(sc)
	struct wb_softc		*sc;
{
	wb_rxeof(sc);

	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXADDR, vtophys(&sc->wb_ldata->wb_rx_list[0]));
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	if (CSR_READ_4(sc, WB_ISR) & WB_RXSTATE_SUSPEND)
		CSR_WRITE_4(sc, WB_RXSTART, 0xFFFFFFFF);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void wb_txeof(sc)
	struct wb_softc		*sc;
{
	struct wb_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->wb_cdata.wb_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->wb_cdata.wb_tx_head->wb_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->wb_cdata.wb_tx_head;
		txstat = WB_TXSTATUS(cur_tx);

		if ((txstat & WB_TXSTAT_OWN) || txstat == WB_UNSENT)
			break;

		if (txstat & WB_TXSTAT_TXERR) {
			ifp->if_oerrors++;
			if (txstat & WB_TXSTAT_ABORT)
				ifp->if_collisions++;
			if (txstat & WB_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & WB_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		m_freem(cur_tx->wb_mbuf);
		cur_tx->wb_mbuf = NULL;

		if (sc->wb_cdata.wb_tx_head == sc->wb_cdata.wb_tx_tail) {
			sc->wb_cdata.wb_tx_head = NULL;
			sc->wb_cdata.wb_tx_tail = NULL;
			break;
		}

		sc->wb_cdata.wb_tx_head = cur_tx->wb_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void wb_txeoc(sc)
	struct wb_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->wb_cdata.wb_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->wb_cdata.wb_tx_tail = NULL;
		if (sc->wb_want_auto)
			wb_autoneg_mii(sc, WB_FLAG_SCHEDDELAY, 1);
	} else {
		if (WB_TXOWN(sc->wb_cdata.wb_tx_head) == WB_UNSENT) {
			WB_TXOWN(sc->wb_cdata.wb_tx_head) = WB_TXSTAT_OWN;
			ifp->if_timer = 5;
			CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
		}
	}

	return;
}

static void wb_intr(arg)
	void			*arg;
{
	struct wb_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return;

	/* Disable interrupts. */
	CSR_WRITE_4(sc, WB_IMR, 0x00000000);

	for (;;) {

		status = CSR_READ_4(sc, WB_ISR);
		if (status)
			CSR_WRITE_4(sc, WB_ISR, status);

		if ((status & WB_INTRS) == 0)
			break;

		if (status & WB_ISR_RX_OK)
			wb_rxeof(sc);

		if (status & WB_ISR_RX_IDLE)
			wb_rxeoc(sc);

		if ((status & WB_ISR_RX_NOBUF) || (status & WB_ISR_RX_ERR)) {
			ifp->if_ierrors++;
#ifdef foo
			wb_stop(sc);
			wb_reset(sc);
			wb_init(sc);
#endif
		}

		if (status & WB_ISR_TX_OK)
			wb_txeof(sc);

		if (status & WB_ISR_TX_NOBUF)
			wb_txeoc(sc);

		if (status & WB_ISR_TX_IDLE) {
			wb_txeof(sc);
			if (sc->wb_cdata.wb_tx_head != NULL) {
				WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
				CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & WB_ISR_TX_UNDERRUN) {
			ifp->if_oerrors++;
			wb_txeof(sc);
			WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
			/* Jack up TX threshold */
			sc->wb_txthresh += WB_TXTHRESH_CHUNK;
			WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_THRESH);
			WB_SETBIT(sc, WB_NETCFG, WB_TXTHRESH(sc->wb_txthresh));
			WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
		}

		if (status & WB_ISR_BUS_ERR) {
			wb_reset(sc);
			wb_init(sc);
		}

	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, WB_IMR, WB_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		wb_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int wb_encap(sc, c, m_head)
	struct wb_softc		*sc;
	struct wb_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct wb_desc		*f = NULL;
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
			if (frag == WB_MAXFRAGS)
				break;
			total_len += m->m_len;
			f = &c->wb_ptr->wb_frag[frag];
			f->wb_ctl = WB_TXCTL_TLINK | m->m_len;
			if (frag == 0) {
				f->wb_ctl |= WB_TXCTL_FIRSTFRAG;
				f->wb_status = 0;
			} else
				f->wb_status = WB_TXSTAT_OWN;
			f->wb_next = vtophys(&c->wb_ptr->wb_frag[frag + 1]);
			f->wb_data = vtophys(mtod(m, vm_offset_t));
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
			printf("wb%d: no memory for tx list", sc->wb_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("wb%d: no memory for tx list",
						sc->wb_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->wb_ptr->wb_frag[0];
		f->wb_status = 0;
		f->wb_data = vtophys(mtod(m_new, caddr_t));
		f->wb_ctl = total_len = m_new->m_len;
		f->wb_ctl |= WB_TXCTL_TLINK|WB_TXCTL_FIRSTFRAG;
		frag = 1;
	}

	if (total_len < WB_MIN_FRAMELEN) {
		f = &c->wb_ptr->wb_frag[frag];
		f->wb_ctl = WB_MIN_FRAMELEN - total_len;
		f->wb_data = vtophys(&sc->wb_cdata.wb_pad);
		f->wb_ctl |= WB_TXCTL_TLINK;
		f->wb_status = WB_TXSTAT_OWN;
		frag++;
	}

	c->wb_mbuf = m_head;
	c->wb_lastdesc = frag - 1;
	WB_TXCTL(c) |= WB_TXCTL_LASTFRAG;
	WB_TXNEXT(c) = vtophys(&c->wb_nextdesc->wb_ptr->wb_frag[0]);

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void wb_start(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct wb_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->wb_autoneg) {
		sc->wb_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->wb_cdata.wb_tx_free->wb_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->wb_cdata.wb_tx_free;

	while(sc->wb_cdata.wb_tx_free->wb_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->wb_cdata.wb_tx_free;
		sc->wb_cdata.wb_tx_free = cur_tx->wb_nextdesc;

		/* Pack the data into the descriptor. */
		wb_encap(sc, cur_tx, m_head);

		if (cur_tx != start_tx)
			WB_TXOWN(cur_tx) = WB_TXSTAT_OWN;

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->wb_mbuf);
#endif
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interupt once for the whole chain rather than
	 * once for each packet.
	 */
	WB_TXCTL(cur_tx) |= WB_TXCTL_FINT;
	cur_tx->wb_ptr->wb_frag[0].wb_ctl |= WB_TXCTL_FINT;
	sc->wb_cdata.wb_tx_tail = cur_tx;

	if (sc->wb_cdata.wb_tx_head == NULL) {
		sc->wb_cdata.wb_tx_head = start_tx;
		WB_TXOWN(start_tx) = WB_TXSTAT_OWN;
		CSR_WRITE_4(sc, WB_TXSTART, 0xFFFFFFFF);
	} else {
		/*
		 * We need to distinguish between the case where
		 * the own bit is clear because the chip cleared it
		 * and where the own bit is clear because we haven't
		 * set it yet. The magic value WB_UNSET is just some
		 * ramdomly chosen number which doesn't have the own
	 	 * bit set. When we actually transmit the frame, the
		 * status word will have _only_ the own bit set, so
		 * the txeoc handler will be able to tell if it needs
		 * to initiate another transmission to flush out pending
		 * frames.
		 */
		WB_TXOWN(start_tx) = WB_UNSENT;
	}

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void wb_init(xsc)
	void			*xsc;
{
	struct wb_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s, i;
	u_int16_t		phy_bmcr = 0;

	if (sc->wb_autoneg)
		return;

	s = splimp();

	if (sc->wb_pinfo != NULL)
		phy_bmcr = wb_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	wb_stop(sc);
	wb_reset(sc);

	sc->wb_txthresh = WB_TXTHRESH_INIT;

	/*
	 * Set cache alignment and burst length.
	 */
	CSR_WRITE_4(sc, WB_BUSCTL, WB_BUSCTL_CONFIG);
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_THRESH);
	WB_SETBIT(sc, WB_NETCFG, WB_TXTHRESH(sc->wb_txthresh));

	/* This doesn't tend to work too well at 100Mbps. */
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_EARLY_ON);

	wb_setcfg(sc, phy_bmcr);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, WB_NODE0 + i, sc->arpcom.ac_enaddr[i]);
	}

	/* Init circular RX list. */
	if (wb_list_rx_init(sc) == ENOBUFS) {
		printf("wb%d: initialization failed: no "
			"memory for rx buffers\n", sc->wb_unit);
		wb_stop(sc);
		(void)splx(s);
		return;
	}

	/* Init TX descriptors. */
	wb_list_tx_init(sc);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ALLPHYS);
	} else {
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ALLPHYS);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_BROAD);
	} else {
		WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_BROAD);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	wb_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXADDR, vtophys(&sc->wb_ldata->wb_rx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, WB_IMR, WB_INTRS);
	CSR_WRITE_4(sc, WB_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_RX_ON);
	CSR_WRITE_4(sc, WB_RXSTART, 0xFFFFFFFF);

	WB_CLRBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);
	CSR_WRITE_4(sc, WB_TXADDR, vtophys(&sc->wb_ldata->wb_tx_list[0]));
	WB_SETBIT(sc, WB_NETCFG, WB_NETCFG_TX_ON);

	/* Restore state of BMCR */
	if (sc->wb_pinfo != NULL)
		wb_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int wb_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		wb_autoneg_mii(sc, WB_FLAG_SCHEDDELAY, 1);
	else
		wb_setmode_mii(sc, ifm->ifm_media);

	return(0);
}

/*
 * Report current media status.
 */
static void wb_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct wb_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (!(wb_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (wb_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (wb_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = wb_phy_readreg(sc, PHY_LPAR);
	advert = wb_phy_readreg(sc, PHY_ANAR);
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

static int wb_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct wb_softc		*sc = ifp->if_softc;
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
			wb_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				wb_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		wb_setmulti(sc);
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

static void wb_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct wb_softc		*sc;

	sc = ifp->if_softc;

	if (sc->wb_autoneg) {
		wb_autoneg_mii(sc, WB_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	printf("wb%d: watchdog timeout\n", sc->wb_unit);

	if (!(wb_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("wb%d: no carrier - transceiver cable problem?\n",
								sc->wb_unit);

	wb_stop(sc);
	wb_reset(sc);
	wb_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		wb_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void wb_stop(sc)
	struct wb_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	WB_CLRBIT(sc, WB_NETCFG, (WB_NETCFG_RX_ON|WB_NETCFG_TX_ON));
	CSR_WRITE_4(sc, WB_IMR, 0x00000000);
	CSR_WRITE_4(sc, WB_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, WB_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < WB_RX_LIST_CNT; i++) {
		if (sc->wb_cdata.wb_rx_chain[i].wb_mbuf != NULL) {
			m_freem(sc->wb_cdata.wb_rx_chain[i].wb_mbuf);
			sc->wb_cdata.wb_rx_chain[i].wb_mbuf = NULL;
		}
	}
	bzero((char *)&sc->wb_ldata->wb_rx_list,
		sizeof(sc->wb_ldata->wb_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < WB_TX_LIST_CNT; i++) {
		if (sc->wb_cdata.wb_tx_chain[i].wb_mbuf != NULL) {
			m_freem(sc->wb_cdata.wb_tx_chain[i].wb_mbuf);
			sc->wb_cdata.wb_tx_chain[i].wb_mbuf = NULL;
		}
	}

	bzero((char *)&sc->wb_ldata->wb_tx_list,
		sizeof(sc->wb_ldata->wb_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void wb_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct wb_softc		*sc = (struct wb_softc *)arg;

	wb_stop(sc);

	return;
}

static struct pci_device wb_device = {
	"wb",
	wb_probe,
	wb_attach,
	&wb_count,
	NULL
};
COMPAT_PCI_DRIVER(wb, wb_device);
