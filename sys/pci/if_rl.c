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
 *	$Id: if_rl.c,v 1.18 1999/07/02 04:17:14 peter Exp $
 */

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the RealTek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The RealTek 8139 PCI NIC redefines the meaning of 'low end.' This is
 * probably the worst PCI ethernet controller ever made, with the possible
 * exception of the FEAST chip made by SMC. The 8139 supports bus-master
 * DMA, but it has a terrible interface that nullifies any performance
 * gains that bus-master DMA usually offers.
 *
 * For transmission, the chip offers a series of four TX descriptor
 * registers. Each transmit frame must be in a contiguous buffer, aligned
 * on a longword (32-bit) boundary. This means we almost always have to
 * do mbuf copies in order to transmit a frame, except in the unlikely
 * case where a) the packet fits into a single mbuf, and b) the packet
 * is 32-bit aligned within the mbuf's data area. The presence of only
 * four descriptor registers means that we can never have more than four
 * packets queued for transmission at any one time.
 *
 * Reception is not much better. The driver has to allocate a single large
 * buffer area (up to 64K in size) into which the chip will DMA received
 * frames. Because we don't know where within this region received packets
 * will begin or end, we have no choice but to copy data from the buffer
 * area into mbufs in order to pass the packets up to the higher protocol
 * levels.
 *
 * It's impossible given this rotten design to really achieve decent
 * performance at 100Mbps, unless you happen to have a 400Mhz PII or
 * some equally overmuscled CPU to drive it.
 *
 * On the bright side, the 8139 does have a built-in PHY, although
 * rather than using an MDIO serial interface like most other NICs, the
 * PHY registers are directly accessible through the 8139's register
 * space. The 8139 supports autonegotiation, as well as a 64-bit multicast
 * filter.
 *
 * The 8129 chip is an older version of the 8139 that uses an external PHY
 * chip. The 8129 has a serial MDIO interface for accessing the MII where
 * the 8139 lets you directly access the on-board PHY registers. We need
 * to select which interface to use depending on the chip type.
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

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of RealTek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#define RL_USEIOSPACE

#include <pci/if_rlreg.h>

#ifndef lint
static const char rcsid[] =
	"$Id: if_rl.c,v 1.18 1999/07/02 04:17:14 peter Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct rl_type rl_devs[] = {
	{ RT_VENDORID, RT_DEVICEID_8129,
		"RealTek 8129 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8139,
		"RealTek 8139 10/100BaseTX" },
	{ ACCTON_VENDORID, ACCTON_DEVICEID_5030,
		"Accton MPX 5030/5038 10/100BaseTX" },
	{ DELTA_VENDORID, DELTA_DEVICEID_8139,
		"Delta Electronics 8139 10/100BaseTX" },
	{ ADDTRON_VENDORID, ADDTRON_DEVICEID_8139,
		"Addtron Technolgy 8139 10/100BaseTX" },
	{ SIS_VENDORID, SIS_DEVICEID_8139,
		"SiS 900 10/100BaseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct rl_type rl_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long rl_count = 0;
static const char *rl_probe	__P((pcici_t, pcidi_t));
static void rl_attach		__P((pcici_t, int));

static int rl_encap		__P((struct rl_softc *, struct mbuf * ));

static void rl_rxeof		__P((struct rl_softc *));
static void rl_txeof		__P((struct rl_softc *));
static void rl_intr		__P((void *));
static void rl_start		__P((struct ifnet *));
static int rl_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void rl_init		__P((void *));
static void rl_stop		__P((struct rl_softc *));
static void rl_watchdog		__P((struct ifnet *));
static void rl_shutdown		__P((int, void *));
static int rl_ifmedia_upd	__P((struct ifnet *));
static void rl_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void rl_eeprom_putbyte	__P((struct rl_softc *, int));
static void rl_eeprom_getword	__P((struct rl_softc *, int, u_int16_t *));
static void rl_read_eeprom	__P((struct rl_softc *, caddr_t,
					int, int, int));
static void rl_mii_sync		__P((struct rl_softc *));
static void rl_mii_send		__P((struct rl_softc *, u_int32_t, int));
static int rl_mii_readreg	__P((struct rl_softc *, struct rl_mii_frame *));
static int rl_mii_writereg	__P((struct rl_softc *, struct rl_mii_frame *));

static u_int16_t rl_phy_readreg	__P((struct rl_softc *, int));
static void rl_phy_writereg	__P((struct rl_softc *, int, int));

static void rl_autoneg_xmit	__P((struct rl_softc *));
static void rl_autoneg_mii	__P((struct rl_softc *, int, int));
static void rl_setmode_mii	__P((struct rl_softc *, int));
static void rl_getmode_mii	__P((struct rl_softc *));
static u_int8_t rl_calchash	__P((caddr_t));
static void rl_setmulti		__P((struct rl_softc *));
static void rl_reset		__P((struct rl_softc *));
static int rl_list_tx_init	__P((struct rl_softc *));

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void rl_eeprom_putbyte(sc, addr)
	struct rl_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | RL_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			EE_SET(RL_EE_DATAIN);
		} else {
			EE_CLR(RL_EE_DATAIN);
		}
		DELAY(100);
		EE_SET(RL_EE_CLK);
		DELAY(150);
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void rl_eeprom_getword(sc, addr, dest)
	struct rl_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Send address of word we want to read.
	 */
	rl_eeprom_putbyte(sc, addr);

	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		EE_SET(RL_EE_CLK);
		DELAY(100);
		if (CSR_READ_1(sc, RL_EECMD) & RL_EE_DATAOUT)
			word |= i;
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void rl_read_eeprom(sc, dest, off, cnt, swap)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		rl_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}


/*
 * MII access routines are provided for the 8129, which
 * doesn't have a built-in PHY. For the 8139, we fake things
 * up by diverting rl_phy_readreg()/rl_phy_writereg() to the
 * direct access PHY registers.
 */
#define MII_SET(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) | x)

#define MII_CLR(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) & ~x)

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void rl_mii_sync(sc)
	struct rl_softc		*sc;
{
	register int		i;

	MII_SET(RL_MII_DIR|RL_MII_DATAOUT);

	for (i = 0; i < 32; i++) {
		MII_SET(RL_MII_CLK);
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void rl_mii_send(sc, bits, cnt)
	struct rl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	MII_CLR(RL_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			MII_SET(RL_MII_DATAOUT);
                } else {
			MII_CLR(RL_MII_DATAOUT);
                }
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		MII_SET(RL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int rl_mii_readreg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_2(sc, RL_MII, 0);

	/*
 	 * Turn on data xmit.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((RL_MII_CLK|RL_MII_DATAOUT));
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	MII_CLR(RL_MII_DIR);

	/* Check for ack */
	MII_CLR(RL_MII_CLK);
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);
	ack = CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(RL_MII_CLK);
			DELAY(1);
			MII_SET(RL_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN)
				frame->mii_data |= i;
			DELAY(1);
		}
		MII_SET(RL_MII_CLK);
		DELAY(1);
	}

fail:

	MII_CLR(RL_MII_CLK);
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int rl_mii_writereg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_WRITEOP;
	frame->mii_turnaround = RL_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);
	rl_mii_send(sc, frame->mii_turnaround, 2);
	rl_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(RL_MII_CLK);
	DELAY(1);
	MII_CLR(RL_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(RL_MII_DIR);

	splx(s);

	return(0);
}

static u_int16_t rl_phy_readreg(sc, reg)
	struct rl_softc		*sc;
	int			reg;
{
	struct rl_mii_frame	frame;
	u_int16_t		rval = 0;
	u_int16_t		rl8139_reg = 0;

	if (sc->rl_type == RL_8139) {
		switch(reg) {
		case PHY_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case PHY_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case PHY_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case PHY_LPAR:
			rl8139_reg = RL_LPAR;
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			return(0);
		}
		rval = CSR_READ_2(sc, rl8139_reg);
		return(rval);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->rl_phy_addr;
	frame.mii_regaddr = reg;
	rl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static void rl_phy_writereg(sc, reg, data)
	struct rl_softc		*sc;
	int			reg;
	int			data;
{
	struct rl_mii_frame	frame;
	u_int16_t		rl8139_reg = 0;

	if (sc->rl_type == RL_8139) {
		switch(reg) {
		case PHY_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case PHY_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case PHY_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case PHY_LPAR:
			rl8139_reg = RL_LPAR;
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			return;
		}
		CSR_WRITE_2(sc, rl8139_reg, data);
		return;
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->rl_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	rl_mii_writereg(sc, &frame);

	return;
}

/*
 * Calculate CRC of a multicast group address, return the upper 6 bits.
 */
static u_int8_t rl_calchash(addr)
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
	return(crc >> 26);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void rl_setmulti(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, RL_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= RL_RXCFG_RX_MULTI;
		CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
		CSR_WRITE_4(sc, RL_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, RL_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, RL_MAR0, 0);
	CSR_WRITE_4(sc, RL_MAR4, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = rl_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}

	if (mcnt)
		rxfilt |= RL_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RL_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
	CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, RL_MAR4, hashes[1]);

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void rl_autoneg_xmit(sc)
	struct rl_softc		*sc;
{
	u_int16_t		phy_sts;

	rl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(rl_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = rl_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	rl_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY. Also used with the 8139 internal
 * transceiver.
 */
static void rl_autoneg_mii(sc, flag, verbose)
	struct rl_softc		*sc;
	int			flag;
	int			verbose;
{
	u_int16_t		phy_sts = 0, media, advert, ability;
	struct ifnet		*ifp;
	struct ifmedia		*ifm;

	ifm = &sc->ifmedia;
	ifp = &sc->arpcom.ac_if;

	/*
	 * The 100baseT4 PHY sometimes has the 'autoneg supported'
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
	phy_sts = rl_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("rl%d: autonegotiation not supported\n",
							sc->rl_unit);
		return;
	}
#endif

	switch (flag) {
	case RL_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		rl_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case RL_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise rl_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->rl_cdata.last_tx != sc->rl_cdata.cur_tx) {
			sc->rl_want_auto = 1;
			return;
		}
		rl_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->rl_autoneg = 1;
		sc->rl_want_auto = 0;
		return;
		break;
	case RL_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->rl_autoneg = 0;
		break;
	default:
		printf("rl%d: invalid autoneg flag: %d\n", sc->rl_unit, flag);
		return;
	}

	if (rl_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("rl%d: autoneg complete, ", sc->rl_unit);
		phy_sts = rl_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("rl%d: autoneg not complete, ", sc->rl_unit);
	}

	media = rl_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (rl_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = rl_phy_readreg(sc, PHY_ANAR);
		ability = rl_phy_readreg(sc, PHY_LPAR);

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
		} else {
			ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
			media &= ~PHY_BMCR_SPEEDSEL;
			media &= ~PHY_BMCR_DUPLEX;
			printf("(half-duplex, 10Mbps)\n");
		}

		/* Set ASIC's duplex mode to match the PHY. */
		rl_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	rl_init(sc);

	if (sc->rl_tx_pend) {
		sc->rl_autoneg = 0;
		sc->rl_tx_pend = 0;
		rl_start(ifp);
	}

	return;
}

static void rl_getmode_mii(sc)
	struct rl_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = rl_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("rl%d: PHY status word: %x\n", sc->rl_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("rl%d: 10Mbps half-duplex mode supported\n",
								sc->rl_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("rl%d: 10Mbps full-duplex mode supported\n",
								sc->rl_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("rl%d: 100Mbps half-duplex mode supported\n",
								sc->rl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("rl%d: 100Mbps full-duplex mode supported\n",
								sc->rl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("rl%d: 100baseT4 mode supported\n", sc->rl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("rl%d: forcing on autoneg support for BT4\n",
							 sc->rl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("rl%d: autoneg supported\n", sc->rl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void rl_setmode_mii(sc, media)
	struct rl_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;

	printf("rl%d: selecting MII, ", sc->rl_unit);

	bmcr = rl_phy_readreg(sc, PHY_BMCR);

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

	rl_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

static void rl_reset(sc)
	struct rl_softc		*sc;
{
	register int		i;

	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_RESET);

	for (i = 0; i < RL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RL_COMMAND) & RL_CMD_RESET))
			break;
	}
	if (i == RL_TIMEOUT)
		printf("rl%d: reset never completed!\n", sc->rl_unit);

        return;
}

/*
 * Probe for a RealTek 8129/8139 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
rl_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct rl_type		*t;

	t = rl_devs;

	while(t->rl_name != NULL) {
		if ((device_id & 0xFFFF) == t->rl_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->rl_did) {
			return(t->rl_name);
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
rl_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef RL_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	struct rl_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
	u_int16_t		rl_did = 0;

	s = splimp();

	sc = malloc(sizeof(struct rl_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("rl%d: no memory for softc struct!\n", unit);
		goto fail;
	}
	bzero(sc, sizeof(struct rl_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, RL_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, RL_PCI_PWRMGMTCTRL);
		if (command & RL_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, RL_PCI_LOIO);
			membase = pci_conf_read(config_id, RL_PCI_LOMEM);
			irq = pci_conf_read(config_id, RL_PCI_INTLINE);

			/* Reset the power state. */
			printf("rl%d: chip is is in D%d power mode "
			"-- setting to D0\n", unit, command & RL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, RL_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, RL_PCI_LOIO, iobase);
			pci_conf_write(config_id, RL_PCI_LOMEM, membase);
			pci_conf_write(config_id, RL_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef RL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("rl%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, RL_PCI_LOIO,
				(pci_port_t *)&(sc->rl_bhandle))) {
		printf ("rl%d: couldn't map ports\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->rl_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->rl_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("rl%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, RL_PCI_LOMEM, &vbase, &pbase)) {
		printf ("rl%d: couldn't map memory\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->rl_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->rl_btag = ALPHA_BUS_SPACE_MEM;
#endif
	sc->rl_bhandle = vbase;
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, rl_intr, sc, &net_imask)) {
		printf("rl%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	rl_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	rl_read_eeprom(sc, (caddr_t)&eaddr, RL_EE_EADDR, 3, 0);

	/*
	 * A RealTek chip was detected. Inform the world.
	 */
	printf("rl%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->rl_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/*
	 * Now read the exact device type from the EEPROM to find
	 * out if it's an 8129 or 8139.
	 */
	rl_read_eeprom(sc, (caddr_t)&rl_did, RL_EE_PCI_DID, 1, 0);

	if (rl_did == RT_DEVICEID_8139 || rl_did == ACCTON_DEVICEID_5030 ||
	    rl_did == DELTA_DEVICEID_8139 || rl_did == ADDTRON_DEVICEID_8139 ||
	    rl_did == SIS_DEVICEID_8139)
		sc->rl_type = RL_8139;
	else if (rl_did == RT_DEVICEID_8129)
		sc->rl_type = RL_8129;
	else {
		printf("rl%d: unknown device ID: %x\n", unit, rl_did);
		free(sc, M_DEVBUF);
		goto fail;
	}

	sc->rl_cdata.rl_rx_buf = contigmalloc(RL_RXBUFLEN + 32, M_DEVBUF,
		M_NOWAIT, 0x100000, 0xffffffff, PAGE_SIZE, 0);

	if (sc->rl_cdata.rl_rx_buf == NULL) {
		free(sc, M_DEVBUF);
		printf("rl%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	/* Leave a few bytes before the start of the RX ring buffer. */
	sc->rl_cdata.rl_rx_buf_ptr = sc->rl_cdata.rl_rx_buf;
	sc->rl_cdata.rl_rx_buf += sizeof(u_int64_t);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "rl";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = rl_start;
	ifp->if_watchdog = rl_watchdog;
	ifp->if_init = rl_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	if (sc->rl_type == RL_8129) {
		if (bootverbose)
			printf("rl%d: probing for a PHY\n", sc->rl_unit);
		for (i = RL_PHYADDR_MIN; i < RL_PHYADDR_MAX + 1; i++) {
			if (bootverbose)
				printf("rl%d: checking address: %d\n",
							sc->rl_unit, i);
			sc->rl_phy_addr = i;
			rl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
			DELAY(500);
			while(rl_phy_readreg(sc, PHY_BMCR)
					& PHY_BMCR_RESET);
			if ((phy_sts = rl_phy_readreg(sc, PHY_BMSR)))
				break;
		}
		if (phy_sts) {
			phy_vid = rl_phy_readreg(sc, PHY_VENID);
			phy_did = rl_phy_readreg(sc, PHY_DEVID);
			if (bootverbose)
				printf("rl%d: found PHY at address %d, ",
						sc->rl_unit, sc->rl_phy_addr);
			if (bootverbose)
				printf("vendor id: %x device id: %x\n",
					phy_vid, phy_did);
			p = rl_phys;
			while(p->rl_vid) {
				if (phy_vid == p->rl_vid &&
					(phy_did | 0x000F) == p->rl_did) {
					sc->rl_pinfo = p;
					break;
				}
				p++;
			}
			if (sc->rl_pinfo == NULL)
				sc->rl_pinfo = &rl_phys[PHY_UNKNOWN];
			if (bootverbose)
				printf("rl%d: PHY type: %s\n",
					sc->rl_unit, sc->rl_pinfo->rl_name);
		} else {
			printf("rl%d: MII without any phy!\n", sc->rl_unit);
		}
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, rl_ifmedia_upd, rl_ifmedia_sts);

	rl_getmode_mii(sc);

	/* Choose a default media. */
	media = IFM_ETHER|IFM_AUTO;
	ifmedia_set(&sc->ifmedia, media);

	rl_autoneg_mii(sc, RL_FLAG_FORCEDELAY, 1);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(rl_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int rl_list_tx_init(sc)
	struct rl_softc		*sc;
{
	struct rl_chain_data	*cd;
	int			i;

	cd = &sc->rl_cdata;
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		cd->rl_tx_chain[i] = NULL;
		CSR_WRITE_4(sc,
		    RL_TXADDR0 + (i * sizeof(u_int32_t)), 0x0000000);
	}

	sc->rl_cdata.cur_tx = 0;
	sc->rl_cdata.last_tx = 0;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * You know there's something wrong with a PCI bus-master chip design
 * when you have to use m_devget().
 *
 * The receive operation is badly documented in the datasheet, so I'll
 * attempt to document it here. The driver provides a buffer area and
 * places its base address in the RX buffer start address register.
 * The chip then begins copying frames into the RX buffer. Each frame
 * is preceeded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 *
 * Note: to make the Alpha happy, the frame payload needs to be aligned
 * on a 32-bit boundary. To achieve this, we cheat a bit by copying from
 * the ring buffer starting at an address two bytes before the actual
 * data location. We can then shave off the first two bytes using m_adj().
 * The reason we do this is because m_devget() doesn't let us specify an
 * offset into the mbuf storage space, so we have to artificially create
 * one. The ring is allocated in such a way that there are a few unused
 * bytes of space preceecing it so that it will be safe for us to do the
 * 2-byte backstep even if reading from the ring at offset 0.
 */
static void rl_rxeof(sc)
	struct rl_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	u_int32_t		rxstat;
	caddr_t			rxbufpos;
	int			wrap = 0;
	u_int16_t		cur_rx;
	u_int16_t		limit;
	u_int16_t		rx_bytes = 0, max_bytes;

	ifp = &sc->arpcom.ac_if;

	cur_rx = (CSR_READ_2(sc, RL_CURRXADDR) + 16) % RL_RXBUFLEN;

	/* Do not try to read past this point. */
	limit = CSR_READ_2(sc, RL_CURRXBUF) % RL_RXBUFLEN;

	if (limit < cur_rx)
		max_bytes = (RL_RXBUFLEN - cur_rx) + limit;
	else
		max_bytes = limit - cur_rx;

	while((CSR_READ_1(sc, RL_COMMAND) & RL_CMD_EMPTY_RXBUF) == 0) {
		rxbufpos = sc->rl_cdata.rl_rx_buf + cur_rx;
		rxstat = *(u_int32_t *)rxbufpos;

		/*
		 * Here's a totally undocumented fact for you. When the
		 * RealTek chip is in the process of copying a packet into
		 * RAM for you, the length will be 0xfff0. If you spot a
		 * packet header with this value, you need to stop. The
		 * datasheet makes absolutely no mention of this and
		 * RealTek should be shot for this.
		 */
		if ((u_int16_t)(rxstat >> 16) == RL_RXSTAT_UNFINISHED)
			break;
	
		if (!(rxstat & RL_RXSTAT_RXOK)) {
			ifp->if_ierrors++;
			if (rxstat & (RL_RXSTAT_BADSYM|RL_RXSTAT_RUNT|
					RL_RXSTAT_GIANT|RL_RXSTAT_CRCERR|
					RL_RXSTAT_ALIGNERR)) {
				CSR_WRITE_2(sc, RL_COMMAND, RL_CMD_TX_ENB);
				CSR_WRITE_2(sc, RL_COMMAND, RL_CMD_TX_ENB|
							RL_CMD_RX_ENB);
				CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);
				CSR_WRITE_4(sc, RL_RXADDR,
					vtophys(sc->rl_cdata.rl_rx_buf));
				CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);
				cur_rx = 0;
			}
			break;
		}

		/* No errors; receive the packet. */	
		total_len = rxstat >> 16;
		rx_bytes += total_len + 4;

		/*
		 * XXX The RealTek chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
	 	 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		/*
		 * Avoid trying to read more bytes than we know
		 * the chip has prepared for us.
		 */
		if (rx_bytes > max_bytes)
			break;

		rxbufpos = sc->rl_cdata.rl_rx_buf +
			((cur_rx + sizeof(u_int32_t)) % RL_RXBUFLEN);

		if (rxbufpos == (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN))
			rxbufpos = sc->rl_cdata.rl_rx_buf;

		wrap = (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN) - rxbufpos;

		if (total_len > wrap) {
			m = m_devget(rxbufpos - RL_ETHER_ALIGN,
			   wrap + RL_ETHER_ALIGN, 0, ifp, NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
				printf("rl%d: out of mbufs, tried to "
					"copy %d bytes\n", sc->rl_unit, wrap);
			}
			else {
				m_adj(m, RL_ETHER_ALIGN);
				m_copyback(m, wrap, total_len - wrap,
					sc->rl_cdata.rl_rx_buf);
			}
			cur_rx = (total_len - wrap + ETHER_CRC_LEN);
		} else {
			m = m_devget(rxbufpos - RL_ETHER_ALIGN,
			    total_len + RL_ETHER_ALIGN, 0, ifp, NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
				printf("rl%d: out of mbufs, tried to "
				"copy %d bytes\n", sc->rl_unit, total_len);
			} else
				m_adj(m, RL_ETHER_ALIGN);
			cur_rx += total_len + 4 + ETHER_CRC_LEN;
		}

		/*
		 * Round up to 32-bit boundary.
		 */
		cur_rx = (cur_rx + 3) & ~3;
		CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);

		if (m == NULL)
			continue;

		eh = mtod(m, struct ether_header *);
		ifp->if_ipackets++;

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

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void rl_txeof(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded.
	 */
	do {
		txstat = CSR_READ_4(sc, RL_LAST_TXSTAT(sc));
		if (!(txstat & (RL_TXSTAT_TX_OK|
		    RL_TXSTAT_TX_UNDERRUN|RL_TXSTAT_TXABRT)))
			break;

		ifp->if_collisions += (txstat & RL_TXSTAT_COLLCNT) >> 24;

		if (RL_LAST_TXMBUF(sc) != NULL) {
			m_freem(RL_LAST_TXMBUF(sc));
			RL_LAST_TXMBUF(sc) = NULL;
		}
		if (txstat & RL_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else {
			ifp->if_oerrors++;
			if ((txstat & RL_TXSTAT_TXABRT) ||
			    (txstat & RL_TXSTAT_OUTOFWIN))
				CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
		}
		RL_INC(sc->rl_cdata.last_tx);
		ifp->if_flags &= ~IFF_OACTIVE;
	} while (sc->rl_cdata.last_tx != sc->rl_cdata.cur_tx);

	if (sc->rl_cdata.last_tx == sc->rl_cdata.cur_tx) {
		if (sc->rl_want_auto)
			rl_autoneg_mii(sc, RL_FLAG_SCHEDDELAY, 1);
	}

	return;
}

static void rl_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Disable interrupts. */
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		if ((status & RL_INTRS) == 0)
			break;

		if (status & RL_ISR_RX_OK)
			rl_rxeof(sc);

		if (status & RL_ISR_RX_ERR)
			rl_rxeof(sc);

		if ((status & RL_ISR_TX_OK) || (status & RL_ISR_TX_ERR))
			rl_txeof(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}

	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		rl_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int rl_encap(sc, m_head)
	struct rl_softc		*sc;
	struct mbuf		*m_head;
{
	struct mbuf		*m_new = NULL;

	/*
	 * The RealTek is brain damaged and wants longword-aligned
	 * TX buffers, plus we can only have one fragment buffer
	 * per packet. We have to copy pretty much all the time.
	 */

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("rl%d: no memory for tx list", sc->rl_unit);
		return(1);
	}
	if (m_head->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			printf("rl%d: no memory for tx list",
					sc->rl_unit);
			return(1);
		}
	}
	m_copydata(m_head, 0, m_head->m_pkthdr.len,	
				mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
	m_freem(m_head);
	m_head = m_new;

	/* Pad frames to at least 60 bytes. */
	if (m_head->m_pkthdr.len < RL_MIN_FRAMELEN) {
		m_head->m_pkthdr.len +=
			(RL_MIN_FRAMELEN - m_head->m_pkthdr.len);
		m_head->m_len = m_head->m_pkthdr.len;
	}

	RL_CUR_TXMBUF(sc) = m_head;

	return(0);
}

/*
 * Main transmit routine.
 */

static void rl_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;

	if (sc->rl_autoneg) {
		sc->rl_tx_pend = 1;
		return;
	}

	while(RL_CUR_TXMBUF(sc) == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		rl_encap(sc, m_head);

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, RL_CUR_TXMBUF(sc));
#endif
		/*
		 * Transmit the frame.
	 	 */
		CSR_WRITE_4(sc, RL_CUR_TXADDR(sc),
		    vtophys(mtod(RL_CUR_TXMBUF(sc), caddr_t)));
		CSR_WRITE_4(sc, RL_CUR_TXSTAT(sc),
		    RL_TX_EARLYTHRESH | RL_CUR_TXMBUF(sc)->m_pkthdr.len);

		RL_INC(sc->rl_cdata.cur_tx);
	}

	/*
	 * We broke out of the loop because all our TX slots are
	 * full. Mark the NIC as busy until it drains some of the
	 * packets from the queue.
	 */
	if (RL_CUR_TXMBUF(sc) != NULL)
		ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void rl_init(xsc)
	void			*xsc;
{
	struct rl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s, i;
	u_int32_t		rxcfg = 0;
	u_int16_t		phy_bmcr = 0;

	if (sc->rl_autoneg)
		return;

	s = splimp();

	/*
	 * XXX Hack for the 8139: the built-in autoneg logic's state
	 * gets reset by rl_init() when we don't want it to. Try
	 * to preserve it. (For 8129 cards with real external PHYs,
	 * the BMCR register doesn't change, but this doesn't hurt.)
	 */
	if (sc->rl_type == RL_8139)
		phy_bmcr = rl_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rl_stop(sc);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, RL_IDR0 + i, sc->arpcom.ac_enaddr[i]);
	}

	/* Init the RX buffer pointer register. */
	CSR_WRITE_4(sc, RL_RXADDR, vtophys(sc->rl_cdata.rl_rx_buf));

	/* Init TX descriptors. */
	rl_list_tx_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	rxcfg |= RL_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxcfg |= RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxcfg |= RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	rl_setmulti(sc);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/* Restore state of BMCR */
	if (sc->rl_pinfo != NULL)
		rl_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int rl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		rl_autoneg_mii(sc, RL_FLAG_SCHEDDELAY, 1);
	else
		rl_setmode_mii(sc, ifm->ifm_media);

	return(0);
}

/*
 * Report current media status.
 */
static void rl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct rl_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	if (!(rl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (rl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
	
		if (rl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = rl_phy_readreg(sc, PHY_LPAR);
	advert = rl_phy_readreg(sc, PHY_ANAR);
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

static int rl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
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
			rl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rl_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		rl_setmulti(sc);
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

static void rl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;

	if (sc->rl_autoneg) {
		rl_autoneg_mii(sc, RL_FLAG_DELAYTIMEO, 1);
		return;
	}

	printf("rl%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;
	if (!(rl_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
		printf("rl%d: no carrier - transceiver cable problem?\n",
								sc->rl_unit);
	rl_txeof(sc);
	rl_rxeof(sc);
	rl_init(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void rl_stop(sc)
	struct rl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		if (sc->rl_cdata.rl_tx_chain[i] != NULL) {
			m_freem(sc->rl_cdata.rl_tx_chain[i]);
			sc->rl_cdata.rl_tx_chain[i] = NULL;
			CSR_WRITE_4(sc, RL_TXADDR0 + i, 0x0000000);
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void rl_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct rl_softc		*sc = (struct rl_softc *)arg;

	rl_stop(sc);

	return;
}


static struct pci_device rl_device = {
	"rl",
	rl_probe,
	rl_attach,
	&rl_count,
	NULL
};
COMPAT_PCI_DRIVER(rl, rl_device);
