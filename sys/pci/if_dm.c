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
 * Davicom DM9102 fast ethernet PCI NIC driver.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Davicom DM9102 is yet another DEC 21x4x clone. This one is actually
 * a pretty faithful copy. Same RX filter programming, same SROM layout,
 * same everything. Datasheets available from www.davicom8.com. Only
 * MII-based transceivers are supported.
 *
 * The DM9102's DMA engine seems pretty weak. Multi-fragment transmits
 * don't seem to work well, and on slow machines you get lots of RX
 * underruns.
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

#define DM_USEIOSPACE

#include <pci/if_dmreg.h>

#include "miibus_if.h"

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct dm_type dm_devs[] = {
	{ DM_VENDORID, DM_DEVICEID_DM9100, "Davicom DM9100 10/100BaseTX" },
	{ DM_VENDORID, DM_DEVICEID_DM9102, "Davicom DM9102 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int dm_probe		__P((device_t));
static int dm_attach		__P((device_t));
static int dm_detach		__P((device_t));

static int dm_newbuf		__P((struct dm_softc *,
					struct dm_desc *,
					struct mbuf *));
static int dm_encap		__P((struct dm_softc *,
					struct mbuf **, u_int32_t *));

static void dm_rxeof		__P((struct dm_softc *));
static void dm_rxeoc		__P((struct dm_softc *));
static void dm_txeof		__P((struct dm_softc *));
static void dm_intr		__P((void *));
static void dm_tick		__P((void *));
static void dm_start		__P((struct ifnet *));
static int dm_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void dm_init		__P((void *));
static void dm_stop		__P((struct dm_softc *));
static void dm_watchdog		__P((struct ifnet *));
static void dm_shutdown		__P((device_t));
static int dm_ifmedia_upd	__P((struct ifnet *));
static void dm_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void dm_delay		__P((struct dm_softc *));
static void dm_eeprom_idle	__P((struct dm_softc *));
static void dm_eeprom_putbyte	__P((struct dm_softc *, int));
static void dm_eeprom_getword	__P((struct dm_softc *, int, u_int16_t *));
static void dm_read_eeprom	__P((struct dm_softc *, caddr_t, int,
							int, int));

static void dm_mii_writebit	__P((struct dm_softc *, int));
static int dm_mii_readbit	__P((struct dm_softc *));
static void dm_mii_sync		__P((struct dm_softc *));
static void dm_mii_send		__P((struct dm_softc *, u_int32_t, int));
static int dm_mii_readreg	__P((struct dm_softc *, struct dm_mii_frame *));
static int dm_mii_writereg	__P((struct dm_softc *, struct dm_mii_frame *));
static int dm_miibus_readreg	__P((device_t, int, int));
static int dm_miibus_writereg	__P((device_t, int, int, int));
static void dm_miibus_statchg	__P((device_t));

static u_int32_t dm_calchash	__P((caddr_t));
static void dm_setfilt		__P((struct dm_softc *));
static void dm_reset		__P((struct dm_softc *));
static int dm_list_rx_init	__P((struct dm_softc *));
static int dm_list_tx_init	__P((struct dm_softc *));

#ifdef DM_USEIOSPACE
#define DM_RES			SYS_RES_IOPORT
#define DM_RID			DM_PCI_LOIO
#else
#define DM_RES			SYS_RES_MEMORY
#define DM_RID			DM_PCI_LOMEM
#endif

static device_method_t dm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dm_probe),
	DEVMETHOD(device_attach,	dm_attach),
	DEVMETHOD(device_detach,	dm_detach),
	DEVMETHOD(device_shutdown,	dm_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	dm_miibus_readreg),
	DEVMETHOD(miibus_writereg,	dm_miibus_writereg),
	DEVMETHOD(miibus_statchg,	dm_miibus_statchg),

	{ 0, 0 }
};

static driver_t dm_driver = {
	"dm",
	dm_methods,
	sizeof(struct dm_softc)
};

static devclass_t dm_devclass;

DRIVER_MODULE(dm, pci, dm_driver, dm_devclass, 0, 0);
DRIVER_MODULE(miibus, dm, miibus_driver, miibus_devclass, 0, 0);

#define DM_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | x)

#define DM_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~x)

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, DM_SIO,				\
		CSR_READ_4(sc, DM_SIO) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, DM_SIO,				\
		CSR_READ_4(sc, DM_SIO) & ~x)

static void dm_delay(sc)
	struct dm_softc		*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, DM_BUSCTL);
}

static void dm_eeprom_idle(sc)
	struct dm_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, DM_SIO, DM_SIO_EESEL);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO,  DM_SIO_ROMCTL_READ);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO, DM_SIO_EE_CS);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO, DM_SIO_EE_CLK);
	dm_delay(sc);

	for (i = 0; i < 25; i++) {
		DM_CLRBIT(sc, DM_SIO, DM_SIO_EE_CLK);
		dm_delay(sc);
		DM_SETBIT(sc, DM_SIO, DM_SIO_EE_CLK);
		dm_delay(sc);
	}

	DM_CLRBIT(sc, DM_SIO, DM_SIO_EE_CLK);
	dm_delay(sc);
	DM_CLRBIT(sc, DM_SIO, DM_SIO_EE_CS);
	dm_delay(sc);
	CSR_WRITE_4(sc, DM_SIO, 0x00000000);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void dm_eeprom_putbyte(sc, addr)
	struct dm_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | DM_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(DM_SIO_EE_DATAIN);
		} else {
			SIO_CLR(DM_SIO_EE_DATAIN);
		}
		dm_delay(sc);
		SIO_SET(DM_SIO_EE_CLK);
		dm_delay(sc);
		SIO_CLR(DM_SIO_EE_CLK);
		dm_delay(sc);
	}

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void dm_eeprom_getword(sc, addr, dest)
	struct dm_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	dm_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DM_SIO, DM_SIO_EESEL);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO,  DM_SIO_ROMCTL_READ);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO, DM_SIO_EE_CS);
	dm_delay(sc);
	DM_SETBIT(sc, DM_SIO, DM_SIO_EE_CLK);
	dm_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	dm_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(DM_SIO_EE_CLK);
		dm_delay(sc);
		if (CSR_READ_4(sc, DM_SIO) & DM_SIO_EE_DATAOUT)
			word |= i;
		dm_delay(sc);
		SIO_CLR(DM_SIO_EE_CLK);
		dm_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	dm_eeprom_idle(sc);

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void dm_read_eeprom(sc, dest, off, cnt, swap)
	struct dm_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		dm_eeprom_getword(sc, off + i, &word);
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
static void dm_mii_writebit(sc, bit)
	struct dm_softc		*sc;
	int			bit;
{
	if (bit)
		CSR_WRITE_4(sc, DM_SIO, DM_SIO_ROMCTL_WRITE|DM_SIO_MII_DATAOUT);
	else
		CSR_WRITE_4(sc, DM_SIO, DM_SIO_ROMCTL_WRITE);

	DM_SETBIT(sc, DM_SIO, DM_SIO_MII_CLK);
	DM_CLRBIT(sc, DM_SIO, DM_SIO_MII_CLK);

	return;
}

/*
 * Read a bit from the MII bus.
 */
static int dm_mii_readbit(sc)
	struct dm_softc		*sc;
{
	CSR_WRITE_4(sc, DM_SIO, DM_SIO_ROMCTL_READ|DM_SIO_MII_DIR);
	CSR_READ_4(sc, DM_SIO);
	DM_SETBIT(sc, DM_SIO, DM_SIO_MII_CLK);
	DM_CLRBIT(sc, DM_SIO, DM_SIO_MII_CLK);
	if (CSR_READ_4(sc, DM_SIO) & DM_SIO_MII_DATAIN)
		return(1);

	return(0);
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void dm_mii_sync(sc)
	struct dm_softc		*sc;
{
	register int		i;

	CSR_WRITE_4(sc, DM_SIO, DM_SIO_ROMCTL_WRITE);

	for (i = 0; i < 32; i++)
		dm_mii_writebit(sc, 1);

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void dm_mii_send(sc, bits, cnt)
	struct dm_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1)
		dm_mii_writebit(sc, bits & i);
}

/*
 * Read an PHY register through the MII.
 */
static int dm_mii_readreg(sc, frame)
	struct dm_softc		*sc;
	struct dm_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = DM_MII_STARTDELIM;
	frame->mii_opcode = DM_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Sync the PHYs.
	 */
	dm_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	dm_mii_send(sc, frame->mii_stdelim, 2);
	dm_mii_send(sc, frame->mii_opcode, 2);
	dm_mii_send(sc, frame->mii_phyaddr, 5);
	dm_mii_send(sc, frame->mii_regaddr, 5);

#ifdef notdef
	/* Idle bit */
	dm_mii_writebit(sc, 1);
	dm_mii_writebit(sc, 0);
#endif

	/* Check for ack */
	ack = dm_mii_readbit(sc);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			dm_mii_readbit(sc);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		if (!ack) {
			if (dm_mii_readbit(sc))
				frame->mii_data |= i;
		}
	}

fail:

	dm_mii_writebit(sc, 0);
	dm_mii_writebit(sc, 0);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int dm_mii_writereg(sc, frame)
	struct dm_softc		*sc;
	struct dm_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = DM_MII_STARTDELIM;
	frame->mii_opcode = DM_MII_WRITEOP;
	frame->mii_turnaround = DM_MII_TURNAROUND;

	/*
	 * Sync the PHYs.
	 */	
	dm_mii_sync(sc);

	dm_mii_send(sc, frame->mii_stdelim, 2);
	dm_mii_send(sc, frame->mii_opcode, 2);
	dm_mii_send(sc, frame->mii_phyaddr, 5);
	dm_mii_send(sc, frame->mii_regaddr, 5);
	dm_mii_send(sc, frame->mii_turnaround, 2);
	dm_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	dm_mii_writebit(sc, 0);
	dm_mii_writebit(sc, 0);

	splx(s);

	return(0);
}

static int dm_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct dm_softc		*sc;
	struct dm_mii_frame	frame;

	sc = device_get_softc(dev);
	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	dm_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int dm_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct dm_softc		*sc;
	struct dm_mii_frame	frame;

	sc = device_get_softc(dev);
	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	dm_mii_writereg(sc, &frame);

	return(0);
}

static void dm_miibus_statchg(dev)
	device_t		dev;
{
	struct dm_softc		*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->dm_miibus);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T)
		DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_SPEEDSEL);
	else
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_SPEEDSEL);

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_FULLDUPLEX);
	else
		DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_FULLDUPLEX);

	return;
}

#define DM_POLY		0xEDB88320
#define DM_BITS		9

static u_int32_t dm_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? DM_POLY : 0);
	}

	return (crc & ((1 << DM_BITS) - 1));
}

void dm_setfilt(sc)
	struct dm_softc		*sc;
{
	struct dm_desc		*sframe;
	u_int32_t		h, *sp;
	struct ifmultiaddr	*ifma;
	struct ifnet		*ifp;
	int			i;

	ifp = &sc->arpcom.ac_if;

	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_TX_ON);
	DM_SETBIT(sc, DM_ISR, DM_ISR_TX_IDLE);

	sframe = &sc->dm_ldata->dm_sframe;
	sp = (u_int32_t *)&sc->dm_cdata.dm_sbuf;
	bzero((char *)sp, DM_SFRAME_LEN);

	sframe->dm_next = vtophys(&sc->dm_ldata->dm_tx_list[0]);
	sframe->dm_data = vtophys(&sc->dm_cdata.dm_sbuf);
	sframe->dm_ctl = DM_SFRAME_LEN | DM_TXCTL_TLINK |
			DM_TXCTL_SETUP | DM_FILTER_HASHPERF;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_PROMISC);
	else
		DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_ALLMULTI);

	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = dm_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		h = dm_calchash((caddr_t)&etherbroadcastaddr);
		sp[h >> 4] |= 1 << (h & 0xF);
	}

	sp[39] = ((u_int16_t *)sc->arpcom.ac_enaddr)[0];
	sp[40] = ((u_int16_t *)sc->arpcom.ac_enaddr)[1];
	sp[41] = ((u_int16_t *)sc->arpcom.ac_enaddr)[2];

	CSR_WRITE_4(sc, DM_TXADDR, vtophys(sframe));
	DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_TX_ON);
	sframe->dm_status = DM_TXSTAT_OWN;
	CSR_WRITE_4(sc, DM_TXSTART, 0xFFFFFFFF);
	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_TX_ON);

	/*
	 * Wait for chip to clear the 'own' bit.
	 */
	for (i = 0; i < DM_TIMEOUT; i++) {
		DELAY(10);
		if (sframe->dm_status != DM_TXSTAT_OWN)
			break;
	}

	if (i == DM_TIMEOUT)
		printf("dm%d: failed to send setup frame\n", sc->dm_unit);

	DM_SETBIT(sc, DM_ISR, DM_ISR_TX_NOBUF|DM_ISR_TX_IDLE);

	return;
}

static void dm_reset(sc)
	struct dm_softc		*sc;
{
	register int		i;

	DM_SETBIT(sc, DM_BUSCTL, DM_BUSCTL_RESET);

	for (i = 0; i < DM_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, DM_BUSCTL) & DM_BUSCTL_RESET))
			break;
	}

	if (i == DM_TIMEOUT)
		printf("dm%d: reset never completed!\n", sc->dm_unit);

	CSR_WRITE_4(sc, DM_BUSCTL, 0);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for an Davicom chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int dm_probe(dev)
	device_t		dev;
{
	struct dm_type		*t;

	t = dm_devs;

	while(t->dm_name != NULL) {
		if ((pci_get_vendor(dev) == t->dm_vid) &&
		    (pci_get_device(dev) == t->dm_did)) {
			device_set_desc(dev, t->dm_name);
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
static int dm_attach(dev)
	device_t		dev;
{
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct dm_softc		*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct dm_softc));

	/*
	 * Handle power management nonsense.
	 */

	command = pci_read_config(dev, DM_PCI_CAPID, 4) & 0x000000FF;
	if (command == 0x01) {

		command = pci_read_config(dev, DM_PCI_PWRMGMTCTRL, 4);
		if (command & DM_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_read_config(dev, DM_PCI_LOIO, 4);
			membase = pci_read_config(dev, DM_PCI_LOMEM, 4);
			irq = pci_read_config(dev, DM_PCI_INTLINE, 4);

			/* Reset the power state. */
			printf("dm%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & DM_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_write_config(dev, DM_PCI_PWRMGMTCTRL, command, 4);

			/* Restore PCI config data. */
			pci_write_config(dev, DM_PCI_LOIO, iobase, 4);
			pci_write_config(dev, DM_PCI_LOMEM, membase, 4);
			pci_write_config(dev, DM_PCI_INTLINE, irq, 4);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCI_COMMAND_STATUS_REG, command, 4);
	command = pci_read_config(dev, PCI_COMMAND_STATUS_REG, 4);

#ifdef DM_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("dm%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("dm%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#endif

	rid = DM_RID;
	sc->dm_res = bus_alloc_resource(dev, DM_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->dm_res == NULL) {
		printf("dm%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->dm_btag = rman_get_bustag(sc->dm_res);
	sc->dm_bhandle = rman_get_bushandle(sc->dm_res);

	/* Allocate interrupt */
	rid = 0;
	sc->dm_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->dm_irq == NULL) {
		printf("dm%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, DM_RES, DM_RID, sc->dm_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->dm_irq, INTR_TYPE_NET,
	    dm_intr, sc, &sc->dm_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->dm_res);
		bus_release_resource(dev, DM_RES, DM_RID, sc->dm_res);
		printf("dm%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* Save the cache line size. */
	sc->dm_cachesize = pci_read_config(dev, DM_PCI_CACHELEN, 4) & 0xFF;

	/* Reset the adapter. */
	dm_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	dm_read_eeprom(sc, (caddr_t)&eaddr, DM_EE_NODEADDR, 3, 0);

	/*
	 * A Davicom chip was detected. Inform the world.
	 */
	printf("dm%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->dm_unit = unit;
	callout_handle_init(&sc->dm_stat_ch);
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->dm_ldata = contigmalloc(sizeof(struct dm_list_data), M_DEVBUF,
	    M_NOWAIT, 0x100000, 0xffffffff, PAGE_SIZE, 0);

	if (sc->dm_ldata == NULL) {
		printf("dm%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->dm_irq, sc->dm_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->dm_irq);
		bus_release_resource(dev, DM_RES, DM_RID, sc->dm_res);
		error = ENXIO;
		goto fail;
	}
	bzero(sc->dm_ldata, sizeof(struct dm_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "dm";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = dm_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = dm_start;
	ifp->if_watchdog = dm_watchdog;
	ifp->if_init = dm_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = DM_TX_LIST_CNT - 1;

	/*
	 * Do MII setup.
	 */
	if (mii_phy_probe(dev, &sc->dm_miibus,
	    dm_ifmedia_upd, dm_ifmedia_sts)) {
		printf("dm%d: MII without any PHY!\n", sc->dm_unit);
		bus_teardown_intr(dev, sc->dm_irq, sc->dm_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->dm_irq);
		bus_release_resource(dev, DM_RES, DM_RID, sc->dm_res);
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

static int dm_detach(dev)
	device_t		dev;
{
	struct dm_softc		*sc;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	dm_reset(sc);
	dm_stop(sc);
	if_detach(ifp);

	bus_generic_detach(dev);
	device_delete_child(dev, sc->dm_miibus);

	bus_teardown_intr(dev, sc->dm_irq, sc->dm_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->dm_irq);
	bus_release_resource(dev, DM_RES, DM_RID, sc->dm_res);

	contigfree(sc->dm_ldata, sizeof(struct dm_list_data), M_DEVBUF);

	splx(s);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int dm_list_tx_init(sc)
	struct dm_softc		*sc;
{
	struct dm_chain_data	*cd;
	struct dm_list_data	*ld;
	int			i;

	cd = &sc->dm_cdata;
	ld = sc->dm_ldata;
	for (i = 0; i < DM_TX_LIST_CNT; i++) {
		if (i == (DM_TX_LIST_CNT - 1)) {
			ld->dm_tx_list[i].dm_nextdesc =
			    &ld->dm_tx_list[0];
			ld->dm_tx_list[i].dm_next =
			    vtophys(&ld->dm_tx_list[0]);
		} else {
			ld->dm_tx_list[i].dm_nextdesc =
			    &ld->dm_tx_list[i + 1];
			ld->dm_tx_list[i].dm_next =
			    vtophys(&ld->dm_tx_list[i + 1]);
		}
		ld->dm_tx_list[i].dm_mbuf = NULL;
		ld->dm_tx_list[i].dm_data = 0;
		ld->dm_tx_list[i].dm_ctl = 0;
	}

	cd->dm_tx_prod = cd->dm_tx_cons = cd->dm_tx_cnt = 0;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int dm_list_rx_init(sc)
	struct dm_softc		*sc;
{
	struct dm_chain_data	*cd;
	struct dm_list_data	*ld;
	int			i;

	cd = &sc->dm_cdata;
	ld = sc->dm_ldata;

	for (i = 0; i < DM_RX_LIST_CNT; i++) {
		if (dm_newbuf(sc, &ld->dm_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (DM_RX_LIST_CNT - 1)) {
			ld->dm_rx_list[i].dm_nextdesc =
			    &ld->dm_rx_list[0];
			ld->dm_rx_list[i].dm_next =
			    vtophys(&ld->dm_rx_list[0]);
		} else {
			ld->dm_rx_list[i].dm_nextdesc =
			    &ld->dm_rx_list[i + 1];
			ld->dm_rx_list[i].dm_next =
			    vtophys(&ld->dm_rx_list[i + 1]);
		}
	}

	cd->dm_rx_prod = 0;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int dm_newbuf(sc, c, m)
	struct dm_softc		*sc;
	struct dm_desc		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("dm%d: no memory for rx list "
			    "-- packet dropped!\n", sc->dm_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("dm%d: no memory for rx list "
			    "-- packet dropped!\n", sc->dm_unit);
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

	c->dm_mbuf = m_new;
	c->dm_data = vtophys(mtod(m_new, caddr_t));
	c->dm_ctl = DM_RXCTL_RLINK | DM_RXLEN;
	c->dm_status = DM_RXSTAT_OWN;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void dm_rxeof(sc)
	struct dm_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct dm_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;
	i = sc->dm_cdata.dm_rx_prod;

	while(!(sc->dm_ldata->dm_rx_list[i].dm_status & DM_RXSTAT_OWN)) {
		struct mbuf		*m0 = NULL;

		cur_rx = &sc->dm_ldata->dm_rx_list[i];
		rxstat = cur_rx->dm_status;
		m = cur_rx->dm_mbuf;
		cur_rx->dm_mbuf = NULL;
		total_len = DM_RXBYTES(rxstat);
		DM_INC(i, DM_RX_LIST_CNT);

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & DM_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			if (rxstat & DM_RXSTAT_COLLSEEN)
				ifp->if_collisions++;
			dm_newbuf(sc, cur_rx, m);
			dm_init(sc);
			return;
		}

		/* No errors; receive the packet. */	
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		dm_newbuf(sc, cur_rx, m);
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

	sc->dm_cdata.dm_rx_prod = i;

	return;
}

void dm_rxeoc(sc)
	struct dm_softc		*sc;
{
	dm_rxeof(sc);
	DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_ON);
	CSR_WRITE_4(sc, DM_RXSTART, 0xFFFFFFFF);
	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void dm_txeof(sc)
	struct dm_softc		*sc;
{
	struct dm_desc		*cur_tx = NULL;
	struct ifnet		*ifp;
	int			idx;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->dm_cdata.dm_tx_cons;
	while(idx != sc->dm_cdata.dm_tx_prod) {
		u_int32_t		txstat;

		cur_tx = &sc->dm_ldata->dm_tx_list[idx];
		txstat = cur_tx->dm_status;

		if (txstat & DM_TXSTAT_OWN)
			break;

		if (!(cur_tx->dm_ctl & DM_TXCTL_LASTFRAG)) {
			sc->dm_cdata.dm_tx_cnt--;
			DM_INC(idx, DM_TX_LIST_CNT);
			continue;
		}

		if (txstat & DM_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & DM_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & DM_TXSTAT_LATECOLL)
				ifp->if_collisions++;
			dm_init(sc);
			return;
		}

		ifp->if_collisions += (txstat & DM_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		if (cur_tx->dm_mbuf != NULL) {
			m_freem(cur_tx->dm_mbuf);
			cur_tx->dm_mbuf = NULL;
		}

		sc->dm_cdata.dm_tx_cnt--;
		DM_INC(idx, DM_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->dm_cdata.dm_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void dm_tick(xsc)
	void			*xsc;
{
	struct dm_softc		*sc;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	sc = xsc;
	mii = device_get_softc(sc->dm_miibus);
	mii_tick(mii);

	sc->dm_stat_ch = timeout(dm_tick, sc, hz);

	splx(s);

	return;
}

static void dm_intr(arg)
	void			*arg;
{
	struct dm_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		dm_stop(sc);
		return;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, DM_IMR, 0x00000000);

	for (;;) {
		status = CSR_READ_4(sc, DM_ISR);
		if (status)
			CSR_WRITE_4(sc, DM_ISR, status);

		if ((status & DM_INTRS) == 0)
			break;

		if ((status & DM_ISR_TX_OK) || (status & DM_ISR_TX_EARLY))
			dm_txeof(sc);

		if (status & DM_ISR_TX_NOBUF)
			dm_txeof(sc);

		if (status & DM_ISR_TX_IDLE) {
			dm_txeof(sc);
			if (sc->dm_cdata.dm_tx_cnt) {
				DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_TX_ON);
				CSR_WRITE_4(sc, DM_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & DM_ISR_TX_UNDERRUN) {
			u_int32_t		cfg;
			cfg = CSR_READ_4(sc, DM_NETCFG);
			if ((cfg & DM_NETCFG_TX_THRESH) == DM_TXTHRESH_160BYTES)
				DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_STORENFWD);
			else
				CSR_WRITE_4(sc, DM_NETCFG, cfg + 0x4000);
		}

		if (status & DM_ISR_RX_OK) {
			dm_rxeof(sc);
			DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_ON);
			CSR_WRITE_4(sc, DM_RXSTART, 0xFFFFFFFF);
		}

		if ((status & DM_ISR_RX_WATDOGTIMEO)
					|| (status & DM_ISR_RX_NOBUF))
			dm_rxeoc(sc);

		if (status & DM_ISR_BUS_ERR) {
			dm_reset(sc);
			dm_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, DM_IMR, DM_INTRS);

	if (ifp->if_snd.ifq_head != NULL)
		dm_start(ifp);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int dm_encap(sc, m_head, txidx)
	struct dm_softc		*sc;
	struct mbuf		**m_head;
	u_int32_t		*txidx;
{
	struct dm_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0;
	struct mbuf             *m_new = NULL;

	m = *m_head;
	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("dm%d: no memory for tx list", sc->dm_unit);
		return(ENOBUFS);
	}
	if (m->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			printf("dm%d: no memory for tx list", sc->dm_unit);
			return(ENOBUFS);
		}
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m->m_pkthdr.len;
	m_freem(m);
	*m_head = m_new;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	cur = frag = *txidx;

	for (m = m_new; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((DM_TX_LIST_CNT -
			    (sc->dm_cdata.dm_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->dm_ldata->dm_tx_list[frag];
			f->dm_ctl = DM_TXCTL_TLINK | m->m_len;
			if (cnt == 0) {
				f->dm_status = 0;
				f->dm_ctl |= DM_TXCTL_FIRSTFRAG;
			} else
				f->dm_status = DM_TXSTAT_OWN;
			f->dm_data = vtophys(mtod(m, vm_offset_t));
			cur = frag;
			DM_INC(frag, DM_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->dm_ldata->dm_tx_list[cur].dm_mbuf = *m_head;
	sc->dm_ldata->dm_tx_list[cur].dm_ctl |=
	    DM_TXCTL_LASTFRAG|DM_TXCTL_FINT;
	sc->dm_ldata->dm_tx_list[*txidx].dm_status |= DM_TXSTAT_OWN;
	sc->dm_cdata.dm_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void dm_start(ifp)
	struct ifnet		*ifp;
{
	struct dm_softc		*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	idx = sc->dm_cdata.dm_tx_prod;

	while(sc->dm_ldata->dm_tx_list[idx].dm_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (dm_encap(sc, &m_head, &idx)) {
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

	sc->dm_cdata.dm_tx_prod = idx;
	CSR_WRITE_4(sc, DM_TXSTART, 0xFFFFFFFF);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void dm_init(xsc)
	void			*xsc;
{
	struct dm_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	dm_stop(sc);
	dm_reset(sc);

	mii = device_get_softc(sc->dm_miibus);

	/*
	 * Set cache alignment and burst length.
	 */
	CSR_WRITE_4(sc, DM_BUSCTL, DM_BURSTLEN_32LONG);
	switch(sc->dm_cachesize) {
	case 32:
		DM_SETBIT(sc, DM_BUSCTL, DM_CACHEALIGN_32LONG);
		break;
	case 16:
		DM_SETBIT(sc, DM_BUSCTL, DM_CACHEALIGN_16LONG);
		break;
	case 8:
		DM_SETBIT(sc, DM_BUSCTL, DM_CACHEALIGN_8LONG);
		break;
	case 0:
	default:
		DM_SETBIT(sc, DM_BUSCTL, DM_CACHEALIGN_NONE);
		break;
	}

	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_HEARTBEAT);
	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_STORENFWD);

	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_TX_THRESH);
	DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_SPEEDSEL);

	if (IFM_SUBTYPE(mii->mii_media.ifm_media) == IFM_10_T)
		DM_SETBIT(sc, DM_NETCFG, DM_TXTHRESH_160BYTES);
	else
		DM_SETBIT(sc, DM_NETCFG, DM_TXTHRESH_72BYTES);

	/* Init circular RX list. */
	if (dm_list_rx_init(sc) == ENOBUFS) {
		printf("dm%d: initialization failed: no "
			"memory for rx buffers\n", sc->dm_unit);
		dm_stop(sc);
		(void)splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	dm_list_tx_init(sc);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_PROMISC);
	} else {
		DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_RX_PROMISC);
	}

	/*
	 * Set the capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_RX_BROAD);
	} else {
		DM_CLRBIT(sc, DM_NETCFG, DM_NETCFG_RX_BROAD);
	}

	/*
	 * Load the RX/multicast filter.
	 */
	dm_setfilt(sc);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, DM_RXADDR, vtophys(&sc->dm_ldata->dm_rx_list[0]));
	/*CSR_WRITE_4(sc, DM_TXADDR, vtophys(&sc->dm_ldata->dm_tx_list[0]));*/

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, DM_IMR, DM_INTRS);
	CSR_WRITE_4(sc, DM_ISR, 0xFFFFFFFF);

	/* Enable receiver and transmitter. */
	DM_SETBIT(sc, DM_NETCFG, DM_NETCFG_TX_ON|DM_NETCFG_RX_ON);
	CSR_WRITE_4(sc, DM_RXSTART, 0xFFFFFFFF);

	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	sc->dm_stat_ch = timeout(dm_tick, sc, hz);

	return;
}

/*
 * Set media options.
 */
static int dm_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct dm_softc		*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		dm_init(sc);

	return(0);
}

/*
 * Report current media status.
 */
static void dm_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct dm_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->dm_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int dm_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct dm_softc		*sc = ifp->if_softc;
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
			dm_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				dm_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		dm_init(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->dm_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

static void dm_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct dm_softc		*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("dm%d: watchdog timeout\n", sc->dm_unit);

	dm_stop(sc);
	dm_reset(sc);
	dm_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		dm_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void dm_stop(sc)
	struct dm_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(dm_tick, sc, sc->dm_stat_ch);

	DM_CLRBIT(sc, DM_NETCFG, (DM_NETCFG_RX_ON|DM_NETCFG_TX_ON));
	CSR_WRITE_4(sc, DM_IMR, 0x00000000);
	CSR_WRITE_4(sc, DM_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, DM_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < DM_RX_LIST_CNT; i++) {
		if (sc->dm_ldata->dm_rx_list[i].dm_mbuf != NULL) {
			m_freem(sc->dm_ldata->dm_rx_list[i].dm_mbuf);
			sc->dm_ldata->dm_rx_list[i].dm_mbuf = NULL;
		}
	}
	bzero((char *)&sc->dm_ldata->dm_rx_list,
		sizeof(sc->dm_ldata->dm_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < DM_TX_LIST_CNT; i++) {
		if (sc->dm_ldata->dm_tx_list[i].dm_mbuf != NULL) {
			m_freem(sc->dm_ldata->dm_tx_list[i].dm_mbuf);
			sc->dm_ldata->dm_tx_list[i].dm_mbuf = NULL;
		}
	}

	bzero((char *)&sc->dm_ldata->dm_tx_list,
		sizeof(sc->dm_ldata->dm_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void dm_shutdown(dev)
	device_t		dev;
{
	struct dm_softc		*sc;

	sc = device_get_softc(dev);

	dm_reset(sc);
	dm_stop(sc);

	return;
}
