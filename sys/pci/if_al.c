/*
 * Copyright (c) 1997, 1998, 1999
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
 *
 * $FreeBSD$
 */

/*
 * ADMtek AL981 Comet and AN985 Centaur fast ethernet PCI NIC driver.
 * Datasheets for the AL981 are available from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The ADMtek AL981 Comet is still another DEC 21x4x clone. It's
 * a reasonably close copy of the tulip, except for the receiver filter
 * programming. Where the DEC chip has a special setup frame that
 * needs to be downloaded into the transmit DMA engine, the ADMtek chip
 * has physical address and multicast address registers.
 *
 * The AN985 is an update to the AL981 which is mostly the same, except
 * for the following things:
 * - The AN985 uses a 99C66 EEPROM which requires a slightly different
 *   bit sequence to initiate a read.
 * - The AN985 uses a serial MII interface instead of providing direct
 *   access to the PHY registers (it uses an internal PHY though).
 * Although the datasheet for the AN985 is not yet available, you can
 * use an AL981 datasheet as a reference for most of the chip functions,
 * except for the MII interface which matches the DEC 21x4x specification
 * (bits 16, 17, 18 and 19 in the serial I/O register control the MII).
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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/* Enable workaround for small transmitter bug. */
#define AL_TX_STALL_WAR

#define AL_USEIOSPACE

#include <pci/if_alreg.h>

#include "miibus_if.h"

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct al_type al_devs[] = {
	{ AL_VENDORID, AL_DEVICEID_AL981, "ADMtek AL981 10/100BaseTX" },
	{ AL_VENDORID, AL_DEVICEID_AN985, "ADMtek AN985 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int al_probe		__P((device_t));
static int al_attach		__P((device_t));
static int al_detach		__P((device_t));

static int al_newbuf		__P((struct al_softc *,
					struct al_desc *,
					struct mbuf *));
static int al_encap		__P((struct al_softc *,
					struct mbuf *, u_int32_t *));

static void al_rxeof		__P((struct al_softc *));
static void al_txeof		__P((struct al_softc *));
static void al_tick		__P((void *));
static void al_intr		__P((void *));
static void al_start		__P((struct ifnet *));
static int al_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void al_init		__P((void *));
static void al_stop		__P((struct al_softc *));
static void al_watchdog		__P((struct ifnet *));
static void al_shutdown		__P((device_t));
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

static int al_miibus_readreg	__P((device_t, int, int));
static int al_miibus_writereg	__P((device_t, int, int, int));
static void al_miibus_statchg	__P((device_t));

static u_int32_t al_calchash	__P((caddr_t));
static void al_setmulti		__P((struct al_softc *));
static void al_reset		__P((struct al_softc *));
static int al_list_rx_init	__P((struct al_softc *));
static int al_list_tx_init	__P((struct al_softc *));

#ifdef AL_USEIOSPACE
#define AL_RES			SYS_RES_IOPORT
#define AL_RID			AL_PCI_LOIO
#else
#define AL_RES			SYS_RES_MEMORY
#define AL_RID			AL_PCI_LOMEM
#endif

static device_method_t al_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		al_probe),
	DEVMETHOD(device_attach,	al_attach),
	DEVMETHOD(device_detach,	al_detach),
	DEVMETHOD(device_shutdown,	al_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	al_miibus_readreg),
	DEVMETHOD(miibus_writereg,	al_miibus_writereg),
	DEVMETHOD(miibus_statchg,	al_miibus_statchg),

	{ 0, 0 }
};

static driver_t al_driver = {
	"al",
	al_methods,
	sizeof(struct al_softc),
};

static devclass_t al_devclass;

DRIVER_MODULE(if_al, pci, al_driver, al_devclass, 0, 0);
DRIVER_MODULE(miibus, al, miibus_driver, miibus_devclass, 0, 0);

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

	/*
	 * The AN985 has a 99C66 EEPROM on it instead of
	 * a 99C64. It uses a different bit sequence for
	 * specifying the "read" opcode.
	 */
	if (sc->al_did == AL_DEVICEID_AN985)
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
	AL_CLRBIT(sc, AL_SIO, AL_SIO_EE_CLK);
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

static int al_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct al_mii_frame	frame;
	u_int16_t		rval = 0;
	u_int16_t		phy_reg = 0;
	struct al_softc		*sc;

	sc = device_get_softc(dev);

	/*
	 * Note: both the AL981 and AN985 have internal PHYs,
	 * however the AL981 provides direct access to the PHY
	 * registers while the AN985 uses a serial MII interface.
	 * The AN985's MII interface is also buggy in that you
	 * can read from any MII address (0 to 31), but only address 1
	 * behaves normally. To deal with both cases, we pretend
	 * that the PHY is at MII address 1.
	 */
	if (phy != 1)
		return(0);

	if (sc->al_did == AL_DEVICEID_AN985) {
		bzero((char *)&frame, sizeof(frame));

		frame.mii_phyaddr = phy;
		frame.mii_regaddr = reg;
		al_mii_readreg(sc, &frame);

		return(frame.mii_data);
	}

	switch(reg) {
	case MII_BMCR:
		phy_reg = AL_BMCR;
		break;
	case MII_BMSR:
		phy_reg = AL_BMSR;
		break;
	case MII_PHYIDR1:
		phy_reg = AL_VENID;
		break;
	case MII_PHYIDR2:
		phy_reg = AL_DEVID;
		break;
	case MII_ANAR:
		phy_reg = AL_ANAR;
		break;
	case MII_ANLPAR:
		phy_reg = AL_LPAR;
		break;
	case MII_ANER:
		phy_reg = AL_ANER;
		break;
	default:
		printf("al%d: read: bad phy register %x\n",
		    sc->al_unit, reg);
		return(0);
		break;
	}

	rval = CSR_READ_4(sc, phy_reg) & 0x0000FFFF;

	if (rval == 0xFFFF)
		return(0);

	return(rval);
}

static int al_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct al_mii_frame	frame;
	struct al_softc		*sc;
	u_int16_t		phy_reg = 0;

	sc = device_get_softc(dev);

	if (phy != 1)
		return(0);

	if (sc->al_did == AL_DEVICEID_AN985) {
		bzero((char *)&frame, sizeof(frame));

		frame.mii_phyaddr = phy;
		frame.mii_regaddr = reg;
		frame.mii_data = data;

		al_mii_writereg(sc, &frame);
		return(0);
	}

	switch(reg) {
	case MII_BMCR:
		phy_reg = AL_BMCR;
		break;
	case MII_BMSR:
		phy_reg = AL_BMSR;
		break;
	case MII_PHYIDR1:
		phy_reg = AL_VENID;
		break;
	case MII_PHYIDR2:
		phy_reg = AL_DEVID;
		break;
	case MII_ANAR:
		phy_reg = AL_ANAR;
		break;
	case MII_ANLPAR:
		phy_reg = AL_LPAR;
		break;
	case MII_ANER:
		phy_reg = AL_ANER;
		break;
	default:
		printf("al%d: phy_write: bad phy register %x\n",
		    sc->al_unit, reg);
		return(0);
		break;
	}

	CSR_WRITE_4(sc, phy_reg, data);

	return(0);
}

static void al_miibus_statchg(dev)
	device_t		dev;
{
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
static int al_probe(dev)
	device_t		dev;
{
	struct al_type		*t;

	t = al_devs;

	while(t->al_name != NULL) {
		if ((pci_get_vendor(dev) == t->al_vid) &&
		    (pci_get_device(dev) == t->al_did)) {
			device_set_desc(dev, t->al_name);
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
static int al_attach(dev)
	device_t		dev;
{
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct al_softc		*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct al_softc));
	sc->al_did = pci_get_device(dev);

	/*
	 * Handle power management nonsense.
	 */

	command = pci_read_config(dev, AL_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, AL_PCI_PWRMGMTCTRL, 4);
		if (command & AL_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, AL_PCI_LOIO, 4);
			membase = pci_read_config(dev, AL_PCI_LOMEM, 4);
			irq = pci_read_config(dev, AL_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("al%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & AL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, AL_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, AL_PCI_LOIO, iobase, 4);
			pci_write_config(dev, AL_PCI_LOMEM, membase, 4);
			pci_write_config(dev, AL_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);

#ifdef AL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("al%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("al%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#endif

	rid = AL_RID;
	sc->al_res = bus_alloc_resource(dev, AL_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->al_res == NULL) {
		printf("al%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->al_btag = rman_get_bustag(sc->al_res);
	sc->al_bhandle = rman_get_bushandle(sc->al_res);

	/* Allocate interrupt */
	rid = 0;
	sc->al_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->al_irq == NULL) {
		printf("al%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, AL_RES, AL_RID, sc->al_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->al_irq, INTR_TYPE_NET,
	    al_intr, sc, &sc->al_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->al_res);
		bus_release_resource(dev, AL_RES, AL_RID, sc->al_res);
		printf("al%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* Save the cache line size. */
	sc->al_cachesize = pci_read_config(dev, AL_PCI_CACHELEN, 4) & 0xFF;

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
	callout_handle_init(&sc->al_stat_ch);
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->al_ldata = contigmalloc(sizeof(struct al_list_data), M_DEVBUF,
	    M_NOWAIT, 0x100000, 0xffffffff, PAGE_SIZE, 0);

	if (sc->al_ldata == NULL) {
		printf("al%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->al_irq, sc->al_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->al_irq);
		bus_release_resource(dev, AL_RES, AL_RID, sc->al_res);
		error = ENXIO;
		goto fail;
	}
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

	/*
	 * Do MII setup.
	 */
	if (mii_phy_probe(dev, &sc->al_miibus,
	    al_ifmedia_upd, al_ifmedia_sts)) {
		printf("al%d: MII without any PHY!\n", sc->al_unit);
		bus_teardown_intr(dev, sc->al_irq, sc->al_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->al_irq);
		bus_release_resource(dev, AL_RES, AL_RID, sc->al_res);
		error = ENXIO;
		goto fail;
	}

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

static int al_detach(dev)
	device_t		dev;
{
	struct al_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	al_reset(sc);
	al_stop(sc);
	if_detach(ifp);

	bus_generic_detach(dev);
	device_delete_child(dev, sc->al_miibus);

	bus_teardown_intr(dev, sc->al_irq, sc->al_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->al_irq);
	bus_release_resource(dev, AL_RES, AL_RID, sc->al_res);

	contigfree(sc->al_ldata, sizeof(struct al_list_data), M_DEVBUF);

	splx(s);

	return(0);
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
		if (i == (AL_TX_LIST_CNT - 1)) {
			ld->al_tx_list[i].al_nextdesc =
			    &ld->al_tx_list[0];
			ld->al_tx_list[i].al_next =
			    vtophys(&ld->al_tx_list[0]);
		} else {
			ld->al_tx_list[i].al_nextdesc =
			    &ld->al_tx_list[i + 1];
			ld->al_tx_list[i].al_next =
			    vtophys(&ld->al_tx_list[i + 1]);
		}
		ld->al_tx_list[i].al_mbuf = NULL;
		ld->al_tx_list[i].al_data = 0;
		ld->al_tx_list[i].al_ctl = 0;
	}

	cd->al_tx_prod = cd->al_tx_cons = cd->al_tx_cnt = 0;

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
		if (al_newbuf(sc, &ld->al_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (AL_RX_LIST_CNT - 1)) {
			ld->al_rx_list[i].al_nextdesc =
			    &ld->al_rx_list[0];
			ld->al_rx_list[i].al_next =
			    vtophys(&ld->al_rx_list[0]);
		} else {
			ld->al_rx_list[i].al_nextdesc =
			    &ld->al_rx_list[i + 1];
			ld->al_rx_list[i].al_next =
			    vtophys(&ld->al_rx_list[i + 1]);
		}
	}

	cd->al_rx_prod = 0;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int al_newbuf(sc, c, m)
	struct al_softc		*sc;
	struct al_desc		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("al%d: no memory for rx list "
			    "-- packet dropped!\n", sc->al_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("al%d: no memory for rx list "
			    "-- packet dropped!\n", sc->al_unit);
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

	c->al_mbuf = m_new;
	c->al_data = vtophys(mtod(m_new, caddr_t));
	c->al_ctl = AL_RXCTL_RLINK | AL_RXLEN;
	c->al_status = AL_RXSTAT_OWN;

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
	struct al_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;

	i = sc->al_cdata.al_rx_prod;

	while(!(sc->al_ldata->al_rx_list[i].al_status & AL_RXSTAT_OWN)) {
		struct mbuf		*m0 = NULL;

		cur_rx = &sc->al_ldata->al_rx_list[i];
		rxstat = cur_rx->al_status;
		m = cur_rx->al_mbuf;
		cur_rx->al_mbuf = NULL;
		total_len = AL_RXBYTES(rxstat);
		AL_INC(i, AL_RX_LIST_CNT);

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
			al_newbuf(sc, cur_rx, m);
			al_init(sc);
			return;
		}

		/* No errors; receive the packet. */	
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		al_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
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

	sc->al_cdata.al_rx_prod = i;

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void al_txeof(sc)
	struct al_softc		*sc;
{
	struct al_desc		*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->al_cdata.al_tx_cons;
	while(idx != sc->al_cdata.al_tx_prod) {
		u_int32_t		txstat;

		cur_tx = &sc->al_ldata->al_tx_list[idx];
		txstat = cur_tx->al_status;

		if (txstat & AL_TXSTAT_OWN)
			break;

		if (!(cur_tx->al_ctl & AL_TXCTL_LASTFRAG)) {
			sc->al_cdata.al_tx_cnt--;
			AL_INC(idx, AL_TX_LIST_CNT);
			continue;
		}

		if (txstat & AL_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & AL_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & AL_TXSTAT_LATECOLL)
				ifp->if_collisions++;
			al_init(sc);
			return;
		}

		ifp->if_collisions += (txstat & AL_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		if (cur_tx->al_mbuf != NULL) {
			m_freem(cur_tx->al_mbuf);
			cur_tx->al_mbuf = NULL;
		}

		sc->al_cdata.al_tx_cnt--;
		AL_INC(idx, AL_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->al_cdata.al_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void al_tick(xsc)
	void			*xsc;
{
	struct al_softc		*sc;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	sc = xsc;
	mii = device_get_softc(sc->al_miibus);
	mii_tick(mii);

	sc->al_stat_ch = timeout(al_tick, sc, hz);

	splx(s);

	return;
};

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

		if ((status & AL_ISR_TX_OK) ||
		    (status & AL_ISR_TX_NOBUF))
			al_txeof(sc);

		if (status & AL_ISR_TX_IDLE) {
			al_txeof(sc);
			if (sc->al_cdata.al_tx_cnt) {
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

		if ((status & AL_ISR_RX_WATDOGTIMEO) ||
		    (status & AL_ISR_RX_IDLE) ||
		    (status & AL_ISR_RX_NOBUF)) {
			al_rxeof(sc);
			al_init(sc);
		}

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
static int al_encap(sc, m_head, txidx)
	struct al_softc		*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct al_desc		*f = NULL;
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
			if (*txidx != sc->al_cdata.al_tx_prod &&
			    frag == (AL_TX_LIST_CNT - 1))
				return(ENOBUFS);
#endif
			if ((AL_TX_LIST_CNT -
			    (sc->al_cdata.al_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->al_ldata->al_tx_list[frag];
			f->al_ctl = AL_TXCTL_TLINK | m->m_len;
			if (cnt == 0) {
				f->al_status = 0;
				f->al_ctl |= AL_TXCTL_FIRSTFRAG;
			} else
				f->al_status = AL_TXSTAT_OWN;
			f->al_data = vtophys(mtod(m, vm_offset_t));
			cur = frag;
			AL_INC(frag, AL_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->al_ldata->al_tx_list[cur].al_mbuf = m_head;
	sc->al_ldata->al_tx_list[cur].al_ctl |=
	    AL_TXCTL_LASTFRAG|AL_TXCTL_FINT;
	sc->al_ldata->al_tx_list[*txidx].al_status |= AL_TXSTAT_OWN;
	sc->al_cdata.al_tx_cnt += cnt;
	*txidx = frag;

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
	u_int32_t		idx;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	idx = sc->al_cdata.al_tx_prod;

	while(sc->al_ldata->al_tx_list[idx].al_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (al_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
#endif
	}

	/* Transmit */
	sc->al_cdata.al_tx_prod = idx;
	CSR_WRITE_4(sc, AL_TXSTART, 0xFFFFFFFF);

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
	struct mii_data		*mii;
	int			s;

	s = splimp();

	mii = device_get_softc(sc->al_miibus);

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
	CSR_WRITE_4(sc, AL_RXADDR, vtophys(&sc->al_ldata->al_rx_list[0]));
	CSR_WRITE_4(sc, AL_TXADDR, vtophys(&sc->al_ldata->al_tx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, AL_IMR, AL_INTRS);
	CSR_WRITE_4(sc, AL_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	AL_SETBIT(sc, AL_NETCFG, AL_NETCFG_TX_ON|AL_NETCFG_RX_ON);
	CSR_WRITE_4(sc, AL_RXSTART, 0xFFFFFFFF);

	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	sc->al_stat_ch = timeout(al_tick, sc, hz);

	return;
}

/*
 * Set media options.
 */
static int al_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct al_softc		*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		al_init(sc);

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
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->al_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int al_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct al_softc		*sc = ifp->if_softc;
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
		mii = device_get_softc(sc->al_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
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

	ifp->if_oerrors++;
	printf("al%d: watchdog timeout\n", sc->al_unit);

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

	untimeout(al_tick, sc, sc->al_stat_ch);
	AL_CLRBIT(sc, AL_NETCFG, (AL_NETCFG_RX_ON|AL_NETCFG_TX_ON));
	CSR_WRITE_4(sc, AL_IMR, 0x00000000);
	CSR_WRITE_4(sc, AL_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, AL_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < AL_RX_LIST_CNT; i++) {
		if (sc->al_ldata->al_rx_list[i].al_mbuf != NULL) {
			m_freem(sc->al_ldata->al_rx_list[i].al_mbuf);
			sc->al_ldata->al_rx_list[i].al_mbuf = NULL;
		}
	}
	bzero((char *)&sc->al_ldata->al_rx_list,
		sizeof(sc->al_ldata->al_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < AL_TX_LIST_CNT; i++) {
		if (sc->al_ldata->al_tx_list[i].al_mbuf != NULL) {
			m_freem(sc->al_ldata->al_tx_list[i].al_mbuf);
			sc->al_ldata->al_tx_list[i].al_mbuf = NULL;
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
static void al_shutdown(dev)
	device_t		dev;
{
	struct al_softc		*sc;

	sc = device_get_softc(dev);
	/*al_stop(sc); */

	return;
}
