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
 * ADMtek AL981 Comet fast ethernet PCI NIC driver. Datasheets for
 * the AL981 are available from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The ADMtek AL981 Comet is still another DEC 21x4x clone. It's
 * a reasonably close copy of the tulip, except for the receiver filter
 * programming. Where the DEC chip has a special setup frame that
 * needs to be downloaded into the transmit DMA engine, the ADMtek chip
 * has physical address and multicast address registers.
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

/* Enable workaround for small transmitter bug. */
#define AL_TX_STALL_WAR

#define AL_USEIOSPACE

/* #define AL_BACKGROUND_AUTONEG */

#include <pci/if_alreg.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct al_type al_devs[] = {
	{ AL_VENDORID, AL_DEVICEID_AL981,
		"ADMtek AL981 10/100BaseTX" },
	{ AL_VENDORID, AL_DEVICEID_AN985,
		"ADMtek AN985 10/100BaseTX" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct al_type al_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long al_count = 0;
static const char *al_probe	__P((pcici_t, pcidi_t));
static void al_attach		__P((pcici_t, int));

static int al_newbuf		__P((struct al_softc *,
						struct al_chain_onefrag *));
static int al_encap		__P((struct al_softc *, struct al_chain *,
						struct mbuf *));

static void al_rxeof		__P((struct al_softc *));
static void al_rxeoc		__P((struct al_softc *));
static void al_txeof		__P((struct al_softc *));
static void al_txeoc		__P((struct al_softc *));
static void al_intr		__P((void *));
static void al_start		__P((struct ifnet *));
static int al_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void al_init		__P((void *));
static void al_stop		__P((struct al_softc *));
static void al_watchdog		__P((struct ifnet *));
static void al_shutdown		__P((int, void *));
static int al_ifmedia_upd	__P((struct ifnet *));
static void al_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void al_delay		__P((struct al_softc *));
static void al_eeprom_idle	__P((struct al_softc *));
static void al_eeprom_putbyte	__P((struct al_softc *, int));
static void al_eeprom_getword	__P((struct al_softc *, int, u_int16_t *));
static void al_read_eeprom	__P((struct al_softc *, caddr_t, int,
							int, int));

static void al_mii_writebit	__P((struct al_softc *, int));
static int al_mii_readbit	__P((struct al_softc *));
static void al_mii_sync		__P((struct al_softc *));
static void al_mii_send		__P((struct al_softc *, u_int32_t, int));
static int al_mii_readreg	__P((struct al_softc *, struct al_mii_frame *));
static int al_mii_writereg	__P((struct al_softc *, struct al_mii_frame *));

static u_int16_t al_phy_readreg	__P((struct al_softc *, int));
static void al_phy_writereg	__P((struct al_softc *, int, int));

static void al_autoneg_xmit	__P((struct al_softc *));
static void al_autoneg_mii	__P((struct al_softc *, int, int));
static void al_setmode_mii	__P((struct al_softc *, int));
static void al_getmode_mii	__P((struct al_softc *));
static u_int32_t al_calchash	__P((caddr_t));
static void al_setmulti		__P((struct al_softc *));
static void al_reset		__P((struct al_softc *));
static int al_list_rx_init	__P((struct al_softc *));
static int al_list_tx_init	__P((struct al_softc *));

#define AL_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define AL_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, AL_SIO,				\
		CSR_READ_4(sc, AL_SIO) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, AL_SIO,				\
		CSR_READ_4(sc, AL_SIO) & ~x)

static void al_delay(sc)
	struct al_softc		*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, AL_BUSCTL);
}

static void al_eeprom_idle(sc)
	struct al_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, AL_SIO, AL_SIO_EESEL);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO,  AL_SIO_ROMCTL_READ);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO, AL_SIO_EE_CS);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO, AL_SIO_EE_CLK);
	al_delay(sc);

	for (i = 0; i < 25; i++) {
		AL_CLRBIT(sc, AL_SIO, AL_SIO_EE_CLK);
		al_delay(sc);
		AL_SETBIT(sc, AL_SIO, AL_SIO_EE_CLK);
		al_delay(sc);
	}

	AL_CLRBIT(sc, AL_SIO, AL_SIO_EE_CLK);
	al_delay(sc);
	AL_CLRBIT(sc, AL_SIO, AL_SIO_EE_CS);
	al_delay(sc);
	CSR_WRITE_4(sc, AL_SIO, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void al_eeprom_putbyte(sc, addr)
	struct al_softc		*sc;
	int			addr;
{
	register int		d, i;

	if (sc->al_info->al_did == AL_DEVICEID_AN985)
		d = addr | (AL_EECMD_READ << 2);
	else
		d = addr | AL_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(AL_SIO_EE_DATAIN);
		} else {
			SIO_CLR(AL_SIO_EE_DATAIN);
		}
		al_delay(sc);
		SIO_SET(AL_SIO_EE_CLK);
		al_delay(sc);
		SIO_CLR(AL_SIO_EE_CLK);
		al_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void al_eeprom_getword(sc, addr, dest)
	struct al_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	al_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, AL_SIO, AL_SIO_EESEL);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO,  AL_SIO_ROMCTL_READ);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO, AL_SIO_EE_CS);
	al_delay(sc);
	AL_SETBIT(sc, AL_SIO, AL_SIO_EE_CLK);
	al_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	al_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(AL_SIO_EE_CLK);
		al_delay(sc);
		if (CSR_READ_4(sc, AL_SIO) & AL_SIO_EE_DATAOUT)
			word |= i;
		al_delay(sc);
		SIO_CLR(AL_SIO_EE_CLK);
		al_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	al_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void al_read_eeprom(sc, dest, off, cnt, swap)
	struct al_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		al_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

/*
 * Write a bit to the MII bus.
 */
static void al_mii_writebit(sc, bit)
	struct al_softc		*sc;
	int			bit;
{
	if (bit)
		CSR_WRITE_4(sc, AL_SIO, AL_SIO_ROMCTL_WRITE|AL_SIO_MII_DATAOUT);
	else
		CSR_WRITE_4(sc, AL_SIO, AL_SIO_ROMCTL_WRITE);

	AL_SETBIT(sc, AL_SIO, AL_SIO_MII_CLK);
	AL_CLRBIT(sc, AL_SIO, AL_SIO_MII_CLK);

	return;
}

/*
 * Read a bit from the MII bus.
 */
static int al_mii_readbit(sc)
	struct al_softc		*sc;
{
	CSR_WRITE_4(sc, AL_SIO, AL_SIO_ROMCTL_READ|AL_SIO_MII_DIR);
	CSR_READ_4(sc, AL_SIO);
	AL_SETBIT(sc, AL_SIO, AL_SIO_MII_CLK);
	AL_CLRBIT(sc, AL_SIO, AL_SIO_MII_CLK);
	if (CSR_READ_4(sc, AL_SIO) & AL_SIO_MII_DATAIN)
		return(1);

	return(0);
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void al_mii_sync(sc)
	struct al_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, AL_SIO, AL_SIO_ROMCTL_WRITE);

	for (i = 0; i < 32; i++)
		al_mii_writebit(sc, 1);

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void al_mii_send(sc, bits, cnt)
	struct al_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1)
		al_mii_writebit(sc, bits & i);
}

/*
 * Read an PHY register through the MII.
 */
static int al_mii_readreg(sc, frame)
	struct al_softc		*sc;
	struct al_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = AL_MII_STARTDELIM;
	frame->mii_opcode = AL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Sync the PHYs.
	 */
	al_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	al_mii_send(sc, frame->mii_stdelim, 2);
	al_mii_send(sc, frame->mii_opcode, 2);
	al_mii_send(sc, frame->mii_phyaddr, 5);
	al_mii_send(sc, frame->mii_regaddr, 5);

#ifdef notdef
	/* Idle bit */
	al_mii_writebit(sc, 1);
	al_mii_writebit(sc, 0);
#endif

	/* Check for ack */
	ack = al_mii_readbit(sc);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			al_mii_readbit(sc);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		if (!ack) {
			if (al_mii_readbit(sc))
				frame->mii_data |= i;
		}
	}

fail:

	al_mii_writebit(sc, 0);
	al_mii_writebit(sc, 0);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int al_mii_writereg(sc, frame)
	struct al_softc		*sc;
	struct al_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = AL_MII_STARTDELIM;
	frame->mii_opcode = AL_MII_WRITEOP;
	frame->mii_turnaround = AL_MII_TURNAROUND;

	/*
	 * Sync the PHYs.
	 */	
	al_mii_sync(sc);

	al_mii_send(sc, frame->mii_stdelim, 2);
	al_mii_send(sc, frame->mii_opcode, 2);
	al_mii_send(sc, frame->mii_phyaddr, 5);
	al_mii_send(sc, frame->mii_regaddr, 5);
	al_mii_send(sc, frame->mii_turnaround, 2);
	al_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	al_mii_writebit(sc, 0);
	al_mii_writebit(sc, 0);

	splx(s);

	return(0);
}

static u_int16_t al_phy_readreg(sc, reg)
	struct al_softc		*sc;
	int			reg;
{
	u_int16_t		rval = 0;
	u_int16_t		phy_reg = 0;
	struct al_mii_frame	frame;

	if (sc->al_info->al_did == AL_DEVICEID_AN985) {
		if (sc->al_phy_addr != 1)
			return(0);
		frame.mii_phyaddr = sc->al_phy_addr;
		frame.mii_regaddr = reg;
		al_mii_readreg(sc, &frame);
		return(frame.mii_data);
	}

	switch(reg) {
	case PHY_BMCR:
		phy_reg = AL_BMCR;
		break;
	case PHY_BMSR:
		phy_reg = AL_BMSR;
		break;
	case PHY_VENID:
		phy_reg = AL_VENID;
		break;
	case PHY_DEVID:
		phy_reg = AL_DEVID;
		break;
	case PHY_ANAR:
		phy_reg = AL_ANAR;
		break;
	case PHY_LPAR:
		phy_reg = AL_LPAR;
		break;
	case PHY_ANEXP:
		phy_reg = AL_ANER;
		break;
	default:
		printf("al%d: read: bad phy register %x\n",
		    sc->al_unit, reg);
		break;
	}

	rval = CSR_READ_4(sc, phy_reg) & 0x0000FFFF;

	return(rval);
}

static void al_phy_writereg(sc, reg, data)
	struct al_softc		*sc;
	int			reg;
	int			data;
{
	u_int16_t		phy_reg = 0;
	struct al_mii_frame	frame;

	if (sc->al_info->al_did == AL_DEVICEID_AN985) {
		if (sc->al_phy_addr != 1)
			return;
		frame.mii_phyaddr = sc->al_phy_addr;
		frame.mii_regaddr = reg;
		frame.mii_data = data;
		al_mii_writereg(sc, &frame);
		return;
	}

	switch(reg) {
	case PHY_BMCR:
		phy_reg = AL_BMCR;
		break;
	case PHY_BMSR:
		phy_reg = AL_BMSR;
		break;
	case PHY_VENID:
		phy_reg = AL_VENID;
		break;
	case PHY_DEVID:
		phy_reg = AL_DEVID;
		break;
	case PHY_ANAR:
		phy_reg = AL_ANAR;
		break;
	case PHY_LPAR:
		phy_reg = AL_LPAR;
		break;
	case PHY_ANEXP:
		phy_reg = AL_ANER;
		break;
	default:
		printf("al%d: phy_write: bad phy register %x\n",
		    sc->al_unit, reg);
		break;
	}

	CSR_WRITE_4(sc, phy_reg, data);

	return;
}

/*
 * Calculate CRC of a multicast group address, return the lower 6 bits.
 */
static u_int32_t al_calchash(addr)
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
	return((crc >> 26) & 0x0000003F);
}

static void al_setmulti(sc)
	struct al_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, AL_NETCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= AL_NETCFG_RX_ALLMULTI;
		CSR_WRITE_4(sc, AL_NETCFG, rxfilt);
		return;
	} else
		rxfilt &= ~AL_NETCFG_RX_ALLMULTI;

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, AL_MAR0, 0);
	CSR_WRITE_4(sc, AL_MAR1, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = al_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
	}

	CSR_WRITE_4(sc, AL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, AL_MAR1, hashes[1]);
	CSR_WRITE_4(sc, AL_NETCFG, rxfilt);

	return;
}

/*
 * Initiate an autonegotiation session.
 */
static void al_autoneg_xmit(sc)
	struct al_softc		*sc;
{
	u_int16_t		phy_sts;

	al_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(al_phy_readreg(sc, PHY_BMCR)
			& PHY_BMCR_RESET);

	phy_sts = al_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	al_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY.
 */
static void al_autoneg_mii(sc, flag, verbose)
	struct al_softc		*sc;
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
	phy_sts = al_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("al%d: autonegotiation not supported\n",
							sc->al_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		return;
	}
#endif

	switch (flag) {
	case AL_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		al_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case AL_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise al_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->al_cdata.al_tx_head != NULL) {
			sc->al_want_auto = 1;
			return;
		}
		al_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->al_autoneg = 1;
		sc->al_want_auto = 0;
		return;
		break;
	case AL_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->al_autoneg = 0;
		break;
	default:
		printf("al%d: invalid autoneg flag: %d\n", sc->al_unit, flag);
		return;
	}

	if (al_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("al%d: autoneg complete, ", sc->al_unit);
		phy_sts = al_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("al%d: autoneg not complete, ", sc->al_unit);
	}

	media = al_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (al_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = al_phy_readreg(sc, PHY_ANAR);
		ability = al_phy_readreg(sc, PHY_LPAR);

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
		al_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier\n");
	}

	al_init(sc);

	if (sc->al_tx_pend) {
		sc->al_autoneg = 0;
		sc->al_tx_pend = 0;
		al_start(ifp);
	}

	return;
}

static void al_getmode_mii(sc)
	struct al_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = al_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("al%d: PHY status word: %x\n", sc->al_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("al%d: 10Mbps half-duplex mode supported\n",
								sc->al_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("al%d: 10Mbps full-duplex mode supported\n",
								sc->al_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("al%d: 100Mbps half-duplex mode supported\n",
								sc->al_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("al%d: 100Mbps full-duplex mode supported\n",
								sc->al_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("al%d: 100baseT4 mode supported\n", sc->al_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("al%d: forcing on autoneg support for BT4\n",
							 sc->al_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0 NULL):
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("al%d: autoneg supported\n", sc->al_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void al_setmode_mii(sc, media)
	struct al_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->al_autoneg) {
		printf("al%d: canceling autoneg session\n", sc->al_unit);
		ifp->if_timer = sc->al_autoneg = sc->al_want_auto = 0;
		bmcr = al_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		al_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("al%d: selecting MII, ", sc->al_unit);

	bmcr = al_phy_readreg(sc, PHY_BMCR);

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

	al_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

static void al_reset(sc)
	struct al_softc		*sc;
{
	register int		i;

	AL_SETBIT(sc, AL_BUSCTL, AL_BUSCTL_RESET);

	for (i = 0; i < AL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, AL_BUSCTL) & AL_BUSCTL_RESET))
			break;
	}
#ifdef notdef
	if (i == AL_TIMEOUT)
		printf("al%d: reset never completed!\n", sc->al_unit);
#endif
	CSR_WRITE_4(sc, AL_BUSCTL, AL_BUSCTL_ARBITRATION);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for an ADMtek chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
al_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct al_type		*t;

	t = al_devs;

	while(t->al_name != NULL) {
		if ((device_id & 0xFFFF) == t->al_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->al_did) {
			return(t->al_name);
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
al_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef AL_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct al_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct al_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;
	u_int32_t		device_id;

	s = splimp();

	sc = malloc(sizeof(struct al_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("al%d: no memory for softc struct!\n", unit);
		goto fail;
	}
	bzero(sc, sizeof(struct al_softc));

	/* Save the device type; we need it later */
	device_id = pci_conf_read(config_id, AL_PCI_VENDOR_ID);
	p = al_devs;

	while(p->al_name != NULL) {
		if ((device_id & 0xFFFF) == p->al_vid &&
		    ((device_id >> 16) & 0xFFFF) == p->al_did) {
			break;
		}
		p++;
	}
	sc->al_info = p;

	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(config_id, AL_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, AL_PCI_PWRMGMTCTRL);
		if (command & AL_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, AL_PCI_LOIO);
			membase = pci_conf_read(config_id, AL_PCI_LOMEM);
			irq = pci_conf_read(config_id, AL_PCI_INTLINE);

			/* Reset the power state. */
			printf("al%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & AL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, AL_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, AL_PCI_LOIO, iobase);
			pci_conf_write(config_id, AL_PCI_LOMEM, membase);
			pci_conf_write(config_id, AL_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef AL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("al%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, AL_PCI_LOIO,
					(u_short *)&(sc->al_bhandle))) {
		printf ("al%d: couldn't map ports\n", unit);
		goto fail;
        }
#ifdef __i386__
	sc->al_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->al_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("al%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, AL_PCI_LOMEM, &vbase, &pbase)) {
		printf ("al%d: couldn't map memory\n", unit);
		goto fail;
	}
#ifdef __i386__
	sc->al_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->al_btag = ALPHA_BUS_SPACE_MEM;
#endif
	sc->al_bhandle = vbase;
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, al_intr, sc, &net_imask)) {
		printf("al%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Save cache line size. */
	sc->al_cachesize = pci_conf_read(config_id, AL_PCI_CACHELEN) & 0xFF;

	/* Reset the adapter. */
	al_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	al_read_eeprom(sc, (caddr_t)&eaddr, AL_EE_NODEADDR, 3, 0);

	/*
	 * An ADMtek chip was detected. Inform the world.
	 */
	printf("al%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->al_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->al_ldata_ptr = malloc(sizeof(struct al_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->al_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("al%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	sc->al_ldata = (struct al_list_data *)sc->al_ldata_ptr;
	round = (unsigned long)sc->al_ldata_ptr & 0xF;
	roundptr = sc->al_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->al_ldata = (struct al_list_data *)roundptr;
	bzero(sc->al_ldata, sizeof(struct al_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "al";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = al_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = al_start;
	ifp->if_watchdog = al_watchdog;
	ifp->if_init = al_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = AL_TX_LIST_CNT - 1;

	if (bootverbose)
		printf("al%d: probing for a PHY\n", sc->al_unit);
	for (i = AL_PHYADDR_MIN; i < AL_PHYADDR_MAL + 1; i++) {
		if (bootverbose)
			printf("al%d: checking address: %d\n",
						sc->al_unit, i);
		sc->al_phy_addr = i;
		al_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
		DELAY(500);
		while(al_phy_readreg(sc, PHY_BMCR)
				& PHY_BMCR_RESET);
		if ((phy_sts = al_phy_readreg(sc, PHY_BMSR)))
			break;
	}
	if (phy_sts) {
		phy_vid = al_phy_readreg(sc, PHY_VENID);
		phy_did = al_phy_readreg(sc, PHY_DEVID);
		if (bootverbose)
			printf("al%d: found PHY at address %d, ",
				sc->al_unit, sc->al_phy_addr);
		if (bootverbose)
			printf("vendor id: %x device id: %x\n",
			phy_vid, phy_did);
		p = al_phys;
		while(p->al_vid) {
			if (phy_vid == p->al_vid &&
				(phy_did | 0x000F) == p->al_did) {
				sc->al_pinfo = p;
				break;
			}
			p++;
		}
		if (sc->al_pinfo == NULL)
			sc->al_pinfo = &al_phys[PHY_UNKNOWN];
		if (bootverbose)
			printf("al%d: PHY type: %s\n",
				sc->al_unit, sc->al_pinfo->al_name);
	} else {
#ifdef DIAGNOSTIC
		printf("al%d: MII without any phy!\n", sc->al_unit);
#endif
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, al_ifmedia_upd, al_ifmedia_sts);

	if (sc->al_pinfo != NULL) {
		al_getmode_mii(sc);
		al_autoneg_mii(sc, AL_FLAG_FORCEDELAY, 1);
	} else {
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	}

	media = sc->ifmedia.ifm_media;
	al_stop(sc);

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(al_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int al_list_tx_init(sc)
	struct al_softc		*sc;
{
	struct al_chain_data	*cd;
	struct al_list_data	*ld;
	int			i;

	cd = &sc->al_cdata;
	ld = sc->al_ldata;
	for (i = 0; i < AL_TX_LIST_CNT; i++) {
		cd->al_tx_chain[i].al_ptr = &ld->al_tx_list[i];
		if (i == (AL_TX_LIST_CNT - 1))
			cd->al_tx_chain[i].al_nextdesc =
				&cd->al_tx_chain[0];
		else
			cd->al_tx_chain[i].al_nextdesc =
				&cd->al_tx_chain[i + 1];
	}

	cd->al_tx_free = &cd->al_tx_chain[0];
	cd->al_tx_tail = cd->al_tx_head = NULL;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int al_list_rx_init(sc)
	struct al_softc		*sc;
{
	struct al_chain_data	*cd;
	struct al_list_data	*ld;
	int			i;

	cd = &sc->al_cdata;
	ld = sc->al_ldata;

	for (i = 0; i < AL_RX_LIST_CNT; i++) {
		cd->al_rx_chain[i].al_ptr =
			(volatile struct al_desc *)&ld->al_rx_list[i];
		if (al_newbuf(sc, &cd->al_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		if (i == (AL_RX_LIST_CNT - 1)) {
			cd->al_rx_chain[i].al_nextdesc =
						&cd->al_rx_chain[0];
			ld->al_rx_list[i].al_next =
					vtophys(&ld->al_rx_list[0]);
		} else {
			cd->al_rx_chain[i].al_nextdesc =
						&cd->al_rx_chain[i + 1];
			ld->al_rx_list[i].al_next =
					vtophys(&ld->al_rx_list[i + 1]);
		}
	}

	cd->al_rx_head = &cd->al_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int al_newbuf(sc, c)
	struct al_softc		*sc;
	struct al_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("al%d: no memory for rx list -- packet dropped!\n",
								sc->al_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("al%d: no memory for rx list -- packet dropped!\n",
								sc->al_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

	c->al_mbuf = m_new;
	c->al_ptr->al_status = AL_RXSTAT;
	c->al_ptr->al_data = vtophys(mtod(m_new, caddr_t));
	c->al_ptr->al_ctl = MCLBYTES - 1;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void al_rxeof(sc)
	struct al_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct al_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	while(!((rxstat = sc->al_cdata.al_rx_head->al_ptr->al_status) &
							AL_RXSTAT_OWN)) {
#ifdef __alpha__
		struct mbuf		*m0 = NULL;
#endif
		cur_rx = sc->al_cdata.al_rx_head;
		sc->al_cdata.al_rx_head = cur_rx->al_nextdesc;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & AL_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			if (rxstat & AL_RXSTAT_COLLSEEN)
				ifp->if_collisions++;
			cur_rx->al_ptr->al_status = AL_RXSTAT;
			cur_rx->al_ptr->al_ctl = (MCLBYTES - 1);
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->al_mbuf;
		total_len = AL_RXBYTES(cur_rx->al_ptr->al_status);

		total_len -= ETHER_CRC_LEN;

#ifdef __alpha__
		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (al_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->al_ptr->al_status = AL_RXSTAT;
			cur_rx->al_ptr->al_ctl = (MCLBYTES - 1);
			continue;
		}

		/*
		 * Sadly, the ADMtek chip doesn't decode the last few
		 * bits of the RX DMA buffer address, so we have to
		 * cheat in order to obtain proper payload alignment
		 * on the alpha.
		 */
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			cur_rx->al_ptr->al_status = AL_RXSTAT;
			cur_rx->al_ptr->al_ctl = (MCLBYTES - 1);
			continue;
		}

		m0->m_data += 2;
		if (total_len <= (MHLEN - 2)) {
			bcopy(mtod(m, caddr_t), mtod(m0, caddr_t), total_len);				m_freem(m);
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
		m->m_pkthdr.rcvif = ifp;
#else
		if (total_len < MINCLSIZE) {
			m = m_devget(mtod(cur_rx->al_mbuf, char *),
				total_len, 0, ifp, NULL);
			cur_rx->al_ptr->al_status = AL_RXSTAT;
			cur_rx->al_ptr->al_ctl = (MCLBYTES - 1);
			if (m == NULL) {
				ifp->if_ierrors++;
				continue;
			}
		} else {
			m = cur_rx->al_mbuf;
		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
			if (al_newbuf(sc, cur_rx) == ENOBUFS) {
				ifp->if_ierrors++;
				cur_rx->al_ptr->al_status = AL_RXSTAT;
				cur_rx->al_ptr->al_ctl = (MCLBYTES - 1);
				continue;
			}
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		}
#endif

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

void al_rxeoc(sc)
	struct al_softc		*sc;
{

	al_rxeof(sc);
	AL_CLRBIT(sc, AL_NETCFG, AL_NETCFG_RX_ON);
	CSR_WRITE_4(sc, AL_RXADDR, vtophys(sc->al_cdata.al_rx_head->al_ptr));
	AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_RX_ON);
	CSR_WRITE_4(sc, AL_RXSTART, 0xFFFFFFFF);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void al_txeof(sc)
	struct al_softc		*sc;
{
	struct al_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	if (sc->al_cdata.al_tx_head == NULL)
		return;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	while(sc->al_cdata.al_tx_head->al_mbuf != NULL) {
		u_int32_t		txstat;

		cur_tx = sc->al_cdata.al_tx_head;
		txstat = AL_TXSTATUS(cur_tx);

		if (txstat & AL_TXSTAT_OWN)
			break;

		if (txstat & AL_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & AL_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & AL_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions += (txstat & AL_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		m_freem(cur_tx->al_mbuf);
		cur_tx->al_mbuf = NULL;

		if (sc->al_cdata.al_tx_head == sc->al_cdata.al_tx_tail) {
			sc->al_cdata.al_tx_head = NULL;
			sc->al_cdata.al_tx_tail = NULL;
			ifp->if_flags &= ~IFF_OACTIVE;
			break;
		}

		sc->al_cdata.al_tx_head = cur_tx->al_nextdesc;
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler.
 */
static void al_txeoc(sc)
	struct al_softc		*sc;
{
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	ifp->if_timer = 0;

	if (sc->al_cdata.al_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->al_cdata.al_tx_tail = NULL;
		if (sc->al_want_auto)
			al_autoneg_mii(sc, AL_FLAG_DELAYTIMEO, 1);
	}

	return;
}

static void al_intr(arg)
	void			*arg;
{
	struct al_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		al_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, AL_IMR, 0x00000000);

	for (;;) {
		status = CSR_READ_4(sc, AL_ISR);
		if (status)
			CSR_WRITE_4(sc, AL_ISR, status);

		if ((status & AL_INTRS) == 0)
			break;

		if (status & AL_ISR_TX_OK)
			al_txeof(sc);

		if (status & AL_ISR_TX_NOBUF)
			al_txeoc(sc);

		if (status & AL_ISR_TX_IDLE) {
			al_txeof(sc);
			if (sc->al_cdata.al_tx_head != NULL) {
				AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_TX_ON);
				CSR_WRITE_4(sc, AL_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & AL_ISR_TX_UNDERRUN) {
			u_int32_t		cfg;
			cfg = CSR_READ_4(sc, AL_NETCFG);
			if ((cfg & AL_NETCFG_TX_THRESH) == AL_TXTHRESH_160BYTES)
				AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_STORENFWD);
			else
				CSR_WRITE_4(sc, AL_NETCFG, cfg + 0x4000);
		}

		if (status & AL_ISR_RX_OK)
			al_rxeof(sc);

		if ((status & AL_ISR_RX_WATDOGTIMEO)
					|| (status & AL_ISR_RX_NOBUF))
			al_rxeoc(sc);

		if (status & AL_ISR_BUS_ERR) {
			al_reset(sc);
			al_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, AL_IMR, AL_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		al_start(ifp);
	}

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int al_encap(sc, c, m_head)
	struct al_softc		*sc;
	struct al_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	volatile struct al_desc	*f = NULL;
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
			if (frag == AL_MAXFRAGS)
				break;
			total_len += m->m_len;
			f = &c->al_ptr->al_frag[frag];
			f->al_ctl = AL_TXCTL_TLINK | m->m_len;
			if (frag == 0) {
				f->al_status = 0;
				f->al_ctl |= AL_TXCTL_FIRSTFRAG;
			} else
				f->al_status = AL_TXSTAT_OWN;
			f->al_next = vtophys(&c->al_ptr->al_frag[frag + 1]);
			f->al_data = vtophys(mtod(m, vm_offset_t));
			frag++;
		}
	}

	/*
	 * Handle special case: we ran out of fragments,
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
			printf("al%d: no memory for tx list", sc->al_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("al%d: no memory for tx list",
						sc->al_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->al_ptr->al_frag[0];
		f->al_status = 0;
		f->al_data = vtophys(mtod(m_new, caddr_t));
		f->al_ctl = total_len = m_new->m_len;
		f->al_ctl |= AL_TXCTL_TLINK|AL_TXCTL_FIRSTFRAG;
		frag = 1;
	}

	c->al_mbuf = m_head;
	c->al_lastdesc = frag - 1;
	AL_TXCTL(c) |= AL_TXCTL_LASTFRAG|AL_TXCTL_FINT;
	AL_TXNEXT(c) = vtophys(&c->al_nextdesc->al_ptr->al_frag[0]);
	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void al_start(ifp)
	struct ifnet		*ifp;
{
	struct al_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct al_chain		*cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	if (sc->al_autoneg) {
		sc->al_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->al_cdata.al_tx_free->al_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	start_tx = sc->al_cdata.al_tx_free;

	while(sc->al_cdata.al_tx_free->al_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->al_cdata.al_tx_free;
		sc->al_cdata.al_tx_free = cur_tx->al_nextdesc;

		/* Pack the data into the descriptor. */
		al_encap(sc, cur_tx, m_head);

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->al_mbuf);
#endif
		AL_TXOWN(cur_tx) = AL_TXSTAT_OWN;
		CSR_WRITE_4(sc, AL_TXSTART, 0xFFFFFFFF);
#ifdef AL_TX_STALL_WAR
		/*
		 * Work around some strange behavior in the Comet. For
		 * some reason, the transmitter will sometimes wedge if
		 * we queue up a descriptor chain that wraps from the end
		 * of the transmit list back to the beginning. If we reach
		 * the end of the list and still have more packets to queue,
		 * don't queue them now: end the transmit session here and
		 * then wait until it finishes before sending the other
		 * packets.
		 */
		if (cur_tx == &sc->al_cdata.al_tx_chain[AL_TX_LIST_CNT - 1]) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
#endif
	}

	sc->al_cdata.al_tx_tail = cur_tx;
	if (sc->al_cdata.al_tx_head == NULL)
		sc->al_cdata.al_tx_head = start_tx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void al_init(xsc)
	void			*xsc;
{
	struct al_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	u_int16_t		phy_bmcr = 0;
	int			s;

	if (sc->al_autoneg)
		return;

	s = splimp();

	if (sc->al_pinfo != NULL)
		phy_bmcr = al_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	al_stop(sc);
	al_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
	CSR_WRITE_4(sc, AL_BUSCTL, AL_BUSCTL_ARBITRATION);
	AL_SETBIT(sc, AL_BUSCTL, AL_BURSTLEN_16LONG);
	switch(sc->al_cachesize) {
	case 32:
		AL_SETBIT(sc, AL_BUSCTL, AL_CACHEALIGN_32LONG);
		break;
	case 16:
		AL_SETBIT(sc, AL_BUSCTL, AL_CACHEALIGN_16LONG);
		break;
	case 8:
		AL_SETBIT(sc, AL_BUSCTL, AL_CACHEALIGN_8LONG);
		break;
	case 0:
	default:
		AL_SETBIT(sc, AL_BUSCTL, AL_CACHEALIGN_NONE);
		break;
	}

	AL_CLRBIT(sc, AL_NETCFG, AL_NETCFG_HEARTBEAT);
	AL_CLRBIT(sc, AL_NETCFG, AL_NETCFG_STORENFWD);

	AL_CLRBIT(sc, AL_NETCFG, AL_NETCFG_TX_THRESH);

	if (IFM_SUBTYPE(sc->ifmedia.ifm_media) == IFM_10_T)
		AL_SETBIT(sc, AL_NETCFG, AL_TXTHRESH_160BYTES);
	else
		AL_SETBIT(sc, AL_NETCFG, AL_TXTHRESH_72BYTES);

	/* Init our MAC address */
	CSR_WRITE_4(sc, AL_PAR0, *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	CSR_WRITE_4(sc, AL_PAR1, *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));

	/* Init circular RX list. */
	if (al_list_rx_init(sc) == ENOBUFS) {
		printf("al%d: initialization failed: no "
			"memory for rx buffers\n", sc->al_unit);
		al_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	al_list_tx_init(sc);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_RX_PROMISC);
	} else {
		AL_CLRBIT(sc, AL_NETCFG, AL_NETCFG_RX_PROMISC);
	}

	/*
	 * Load the multicast filter.
	 */
	al_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, AL_RXADDR, vtophys(sc->al_cdata.al_rx_head->al_ptr));
	CSR_WRITE_4(sc, AL_TXADDR, vtophys(&sc->al_ldata->al_tx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, AL_IMR, AL_INTRS);
	CSR_WRITE_4(sc, AL_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_TX_ON|AL_NETCFG_RX_ON);
	CSR_WRITE_4(sc, AL_RXSTART, 0xFFFFFFFF);

	/* Restore state of BMCR */
	if (sc->al_pinfo != NULL)
		al_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int al_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct al_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
		al_autoneg_mii(sc, AL_FLAG_SCHEDDELAY, 1);
	else {
		al_setmode_mii(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void al_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct al_softc		*sc;
	u_int16_t		advert = 0, ability = 0;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;

	if (!(al_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
		if (al_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
			ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
		else
			ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (al_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		return;
	}

	ability = al_phy_readreg(sc, PHY_LPAR);
	advert = al_phy_readreg(sc, PHY_ANAR);
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

static int al_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct al_softc		*sc = ifp->if_softc;
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
			al_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				al_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		al_setmulti(sc);
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

static void al_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct al_softc		*sc;

	sc = ifp->if_softc;

	if (sc->al_autoneg) {
		al_autoneg_mii(sc, AL_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	printf("al%d: watchdog timeout\n", sc->al_unit);

	if (sc->al_pinfo != NULL) {
		if (!(al_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT))
			printf("al%d: no carrier - transceiver "
				"cable problem?\n", sc->al_unit);
	}

	al_stop(sc);
	al_reset(sc);
	al_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		al_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void al_stop(sc)
	struct al_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	AL_CLRBIT(sc, AL_NETCFG, (AL_NETCFG_RX_ON|AL_NETCFG_TX_ON));
	CSR_WRITE_4(sc, AL_IMR, 0x00000000);
	CSR_WRITE_4(sc, AL_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, AL_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < AL_RX_LIST_CNT; i++) {
		if (sc->al_cdata.al_rx_chain[i].al_mbuf != NULL) {
			m_freem(sc->al_cdata.al_rx_chain[i].al_mbuf);
			sc->al_cdata.al_rx_chain[i].al_mbuf = NULL;
		}
	}
	bzero((char *)&sc->al_ldata->al_rx_list,
		sizeof(sc->al_ldata->al_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < AL_TX_LIST_CNT; i++) {
		if (sc->al_cdata.al_tx_chain[i].al_mbuf != NULL) {
			m_freem(sc->al_cdata.al_tx_chain[i].al_mbuf);
			sc->al_cdata.al_tx_chain[i].al_mbuf = NULL;
		}
	}

	bzero((char *)&sc->al_ldata->al_tx_list,
		sizeof(sc->al_ldata->al_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void al_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct al_softc		*sc = (struct al_softc *)arg;

	al_stop(sc);

	return;
}

static struct pci_device al_device = {
	"al",
	al_probe,
	al_attach,
	&al_count,
	NULL
};
DATA_SET(pcidevice_set, al_device);
