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
 * $FreeBSD$
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

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

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
  "$FreeBSD$";
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
	{ DLINK_VENDORID, DLINK_DEVICEID_530TXPLUS,
		"D-Link DFE-530TX+ 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int rl_probe		__P((device_t));
static int rl_attach		__P((device_t));
static int rl_detach		__P((device_t));

static int rl_encap		__P((struct rl_softc *, struct mbuf * ));

static void rl_rxeof		__P((struct rl_softc *));
static void rl_txeof		__P((struct rl_softc *));
static void rl_intr		__P((void *));
static void rl_tick		__P((void *));
static void rl_start		__P((struct ifnet *));
static int rl_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void rl_init		__P((void *));
static void rl_stop		__P((struct rl_softc *));
static void rl_watchdog		__P((struct ifnet *));
static int rl_suspend		__P((device_t));
static int rl_resume		__P((device_t));
static void rl_shutdown		__P((device_t));
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

static int rl_miibus_readreg	__P((device_t, int, int));
static int rl_miibus_writereg	__P((device_t, int, int, int));
static void rl_miibus_statchg	__P((device_t));

static u_int8_t rl_calchash	__P((caddr_t));
static void rl_setmulti		__P((struct rl_softc *));
static void rl_reset		__P((struct rl_softc *));
static int rl_list_tx_init	__P((struct rl_softc *));

#ifdef RL_USEIOSPACE
#define RL_RES			SYS_RES_IOPORT
#define RL_RID			RL_PCI_LOIO
#else
#define RL_RES			SYS_RES_MEMORY
#define RL_RID			RL_PCI_LOMEM
#endif

static device_method_t rl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rl_probe),
	DEVMETHOD(device_attach,	rl_attach),
	DEVMETHOD(device_detach,	rl_detach),
	DEVMETHOD(device_suspend,	rl_suspend),
	DEVMETHOD(device_resume,	rl_resume),
	DEVMETHOD(device_shutdown,	rl_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	rl_miibus_readreg),
	DEVMETHOD(miibus_writereg,	rl_miibus_writereg),
	DEVMETHOD(miibus_statchg,	rl_miibus_statchg),

	{ 0, 0 }
};

static driver_t rl_driver = {
	"rl",
	rl_methods,
	sizeof(struct rl_softc)
};

static devclass_t rl_devclass;

DRIVER_MODULE(if_rl, pci, rl_driver, rl_devclass, 0, 0);
DRIVER_MODULE(miibus, rl, miibus_driver, miibus_devclass, 0, 0);

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
	 * Feed in each bit and strobe the clock.
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

static int rl_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rval = 0;
	u_int16_t		rl8139_reg = 0;

	sc = device_get_softc(dev);

	if (sc->rl_type == RL_8139) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy)
			return(0);
		switch(reg) {
		case MII_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case MII_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case MII_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case MII_ANER:
			rl8139_reg = RL_ANER;
			break;
		case MII_ANLPAR:
			rl8139_reg = RL_LPAR;
			break;
		case MII_PHYIDR1:
		case MII_PHYIDR2:
			return(0);
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			return(0);
		}
		rval = CSR_READ_2(sc, rl8139_reg);
		return(rval);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	rl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int rl_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rl8139_reg = 0;

	sc = device_get_softc(dev);

	if (sc->rl_type == RL_8139) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy)
			return(0);
		switch(reg) {
		case MII_BMCR:
			rl8139_reg = RL_BMCR;
			break;
		case MII_BMSR:
			rl8139_reg = RL_BMSR;
			break;
		case MII_ANAR:
			rl8139_reg = RL_ANAR;
			break;
		case MII_ANER:
			rl8139_reg = RL_ANER;
			break;
		case MII_ANLPAR:
			rl8139_reg = RL_LPAR;
			break;
		case MII_PHYIDR1:
		case MII_PHYIDR2:
			return(0);
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			return(0);
		}
		CSR_WRITE_2(sc, rl8139_reg, data);
		return(0);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	rl_mii_writereg(sc, &frame);

	return(0);
}

static void rl_miibus_statchg(dev)
	device_t		dev;
{
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
static int rl_probe(dev)
	device_t		dev;
{
	struct rl_type		*t;

	t = rl_devs;

	while(t->rl_name != NULL) {
		if ((pci_get_vendor(dev) == t->rl_vid) &&
		    (pci_get_device(dev) == t->rl_did)) {
			device_set_desc(dev, t->rl_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int rl_attach(dev)
	device_t		dev;
{
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		rl_did = 0;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct rl_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_read_config(dev, RL_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, RL_PCI_PWRMGMTCTRL, 4);
		if (command & RL_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, RL_PCI_LOIO, 4);
			membase = pci_read_config(dev, RL_PCI_LOMEM, 4);
			irq = pci_read_config(dev, RL_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("rl%d: chip is is in D%d power mode "
			"-- setting to D0\n", unit, command & RL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, RL_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, RL_PCI_LOIO, iobase, 4);
			pci_write_config(dev, RL_PCI_LOMEM, membase, 4);
			pci_write_config(dev, RL_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef RL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("rl%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("rl%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;
		goto fail;
	}
#endif

	rid = RL_RID; 
	sc->rl_res = bus_alloc_resource(dev, RL_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->rl_res == NULL) {
		printf ("rl%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->rl_btag = rman_get_bustag(sc->rl_res);
	sc->rl_bhandle = rman_get_bushandle(sc->rl_res);

	rid = 0;
	sc->rl_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->rl_irq == NULL) {
		printf("rl%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->rl_irq, INTR_TYPE_NET,
	    rl_intr, sc, &sc->rl_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
		printf("rl%d: couldn't set up irq\n", unit);
		goto fail;
	}

	callout_handle_init(&sc->rl_stat_ch);

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
	    rl_did == DLINK_DEVICEID_530TXPLUS)
		sc->rl_type = RL_8139;
	else if (rl_did == RT_DEVICEID_8129)
		sc->rl_type = RL_8129;
	else {
		printf("rl%d: unknown device ID: %x\n", unit, rl_did);
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
		error = ENXIO;
		goto fail;
	}

	sc->rl_cdata.rl_rx_buf = contigmalloc(RL_RXBUFLEN + 1518, M_DEVBUF,
		M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->rl_cdata.rl_rx_buf == NULL) {
		printf("rl%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
		error = ENXIO;
		goto fail;
	}

	/* Leave a few bytes before the start of the RX ring buffer. */
	sc->rl_cdata.rl_rx_buf_ptr = sc->rl_cdata.rl_rx_buf;
	sc->rl_cdata.rl_rx_buf += sizeof(u_int64_t);

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->rl_miibus,
	    rl_ifmedia_upd, rl_ifmedia_sts)) {
		printf("rl%d: MII without any phy!\n", sc->rl_unit);
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);
		free(sc->rl_cdata.rl_rx_buf, M_DEVBUF);
		error = ENXIO;
		goto fail;
	}

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

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

fail:
	splx(s);
	return(error);
}

static int rl_detach(dev)
	device_t		dev;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	rl_stop(sc);

	bus_generic_detach(dev);
	device_delete_child(dev, sc->rl_miibus);

	bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
	bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);

	contigfree(sc->rl_cdata.rl_rx_buf, RL_RXBUFLEN + 32, M_DEVBUF);

	splx(s);

	return(0);
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
			rl_init(sc);
			return;
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
			/*
			 * Fool m_devget() into thinking we want to copy
			 * the whole buffer so we don't end up fragmenting
			 * the data.
			 */
			m = m_devget(rxbufpos - RL_ETHER_ALIGN,
			    total_len + RL_ETHER_ALIGN, 0, ifp, NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
			} else {
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
			int			oldthresh;
			ifp->if_oerrors++;
			if ((txstat & RL_TXSTAT_TXABRT) ||
			    (txstat & RL_TXSTAT_OUTOFWIN))
				CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
			oldthresh = sc->rl_txthresh;
			/* error recovery */
			rl_reset(sc);
			rl_init(sc);
			/*
			 * If there was a transmit underrun,
			 * bump the TX threshold.
			 */
			if (txstat & RL_TXSTAT_TX_UNDERRUN)
				sc->rl_txthresh = oldthresh + 32;
			return;
		}
		RL_INC(sc->rl_cdata.last_tx);
		ifp->if_flags &= ~IFF_OACTIVE;
	} while (sc->rl_cdata.last_tx != sc->rl_cdata.cur_tx);

	return;
}

static void rl_tick(xsc)
	void			*xsc;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	sc = xsc;
	mii = device_get_softc(sc->rl_miibus);

	mii_tick(mii);

	splx(s);

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);

	return;
}

static void rl_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;

	if (sc->suspended) {
		return;
	}

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

	if (ifp->if_snd.ifq_head != NULL)
		rl_start(ifp);

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
	if (m_new == NULL)
		return(1);
	if (m_head->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return(1);
		}
	}
	m_copydata(m_head, 0, m_head->m_pkthdr.len, mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
	m_freem(m_head);
	m_head = m_new;

	/* Pad frames to at least 60 bytes. */
	if (m_head->m_pkthdr.len < RL_MIN_FRAMELEN) {
		/*
		 * Make security concious people happy: zero out the
		 * bytes in the pad area, since we don't know what
		 * this mbuf cluster buffer's previous user might
		 * have left in it.
	 	 */
		bzero(mtod(m_head, char *) + m_head->m_pkthdr.len,
		     RL_MIN_FRAMELEN - m_head->m_pkthdr.len);
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

	while(RL_CUR_TXMBUF(sc) == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (rl_encap(sc, m_head)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, RL_CUR_TXMBUF(sc));

		/*
		 * Transmit the frame.
	 	 */
		CSR_WRITE_4(sc, RL_CUR_TXADDR(sc),
		    vtophys(mtod(RL_CUR_TXMBUF(sc), caddr_t)));
		CSR_WRITE_4(sc, RL_CUR_TXSTAT(sc),
		    RL_TXTHRESH(sc->rl_txthresh) |
		    RL_CUR_TXMBUF(sc)->m_pkthdr.len);

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
	struct mii_data		*mii;
	int			s, i;
	u_int32_t		rxcfg = 0;

	s = splimp();

	mii = device_get_softc(sc->rl_miibus);

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

	/* Set initial TX threshold */
	sc->rl_txthresh = RL_TX_THRESH_INIT;

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	mii_mediachg(mii);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);

	return;
}

/*
 * Set media options.
 */
static int rl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->rl_miibus);
	mii_mediachg(mii);

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
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->rl_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int rl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
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
		mii = device_get_softc(sc->rl_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
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

	printf("rl%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;

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

	untimeout(rl_tick, sc, sc->rl_stat_ch);

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
static void rl_shutdown(dev)
	device_t		dev;
{
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	rl_stop(sc);

	return;
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int rl_suspend(dev)
	device_t		dev;
{
	register int		i;
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	rl_stop(sc);

	for (i = 0; i < 5; i++)
		sc->saved_maps[i] = pci_read_config(dev, PCIR_MAPS + i * 4, 4);
	sc->saved_biosaddr = pci_read_config(dev, PCIR_BIOS, 4);
	sc->saved_intline = pci_read_config(dev, PCIR_INTLINE, 1);
	sc->saved_cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	sc->saved_lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);

	sc->suspended = 1;

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int rl_resume(dev)
	device_t		dev;
{
	register int		i;
	struct rl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	/* better way to do this? */
	for (i = 0; i < 5; i++)
		pci_write_config(dev, PCIR_MAPS + i * 4, sc->saved_maps[i], 4);
	pci_write_config(dev, PCIR_BIOS, sc->saved_biosaddr, 4);
	pci_write_config(dev, PCIR_INTLINE, sc->saved_intline, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, sc->saved_cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, sc->saved_lattimer, 1);

	/* reenable busmastering */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, RL_RES);

        /* reinitialize interface if necessary */
        if (ifp->if_flags & IFF_UP)
                rl_init(sc);

	sc->suspended = 0;

	return (0);
}
