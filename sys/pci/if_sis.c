/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * SiS 900/SiS 7016 fast ethernet PCI NIC driver. Datasheets are
 * available from http://www.sis.com.tw.
 *
 * This driver also supports the NatSemi DP83815. Datasheets are
 * available from http://www.national.com.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define SIS_USEIOSPACE

#include <pci/if_sisreg.h>

MODULE_DEPEND(sis, pci, 1, 1, 1);
MODULE_DEPEND(sis, ether, 1, 1, 1);
MODULE_DEPEND(sis, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Various supported device vendors/types and their names.
 */
static struct sis_type sis_devs[] = {
	{ SIS_VENDORID, SIS_DEVICEID_900, "SiS 900 10/100BaseTX" },
	{ SIS_VENDORID, SIS_DEVICEID_7016, "SiS 7016 10/100BaseTX" },
	{ NS_VENDORID, NS_DEVICEID_DP83815, "NatSemi DP8381[56] 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int sis_detach(device_t);
static void sis_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int sis_ifmedia_upd(struct ifnet *);
static void sis_init(void *);
static void sis_initl(struct sis_softc *);
static void sis_intr(void *);
static int sis_ioctl(struct ifnet *, u_long, caddr_t);
static int sis_newbuf(struct sis_softc *, struct sis_desc *, struct mbuf *);
static void sis_start(struct ifnet *);
static void sis_startl(struct ifnet *);
static void sis_stop(struct sis_softc *);
static void sis_watchdog(struct ifnet *);

#ifdef SIS_USEIOSPACE
#define SIS_RES			SYS_RES_IOPORT
#define SIS_RID			SIS_PCI_LOIO
#else
#define SIS_RES			SYS_RES_MEMORY
#define SIS_RID			SIS_PCI_LOMEM
#endif

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

static void
sis_dma_map_desc_next(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct sis_desc	*r;

	r = arg;
	r->sis_next = segs->ds_addr;
}

static void
sis_dma_map_desc_ptr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct sis_desc	*r;

	r = arg;
	r->sis_ptr = segs->ds_addr;
}

static void
sis_dma_map_ring(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	u_int32_t *p;

	p = arg;
	*p = segs->ds_addr;
}

/*
 * Routine to reverse the bits in a word. Stolen almost
 * verbatim from /usr/games/fortune.
 */
static uint16_t
sis_reverse(uint16_t n)
{
	n = ((n >>  1) & 0x5555) | ((n <<  1) & 0xaaaa);
	n = ((n >>  2) & 0x3333) | ((n <<  2) & 0xcccc);
	n = ((n >>  4) & 0x0f0f) | ((n <<  4) & 0xf0f0);
	n = ((n >>  8) & 0x00ff) | ((n <<  8) & 0xff00);

	return(n);
}

static void
sis_delay(struct sis_softc *sc)
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, SIS_CSR);
}

static void
sis_eeprom_idle(struct sis_softc *sc)
{
	int		i;

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
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
sis_eeprom_putbyte(struct sis_softc *sc, int addr)
{
	int		d, i;

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
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
sis_eeprom_getword(struct sis_softc *sc, int addr, uint16_t *dest)
{
	int		i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	sis_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	sis_delay(sc);
	SIO_CLR(SIS_EECTL_CLK);
	sis_delay(sc);
	SIO_SET(SIS_EECTL_CSEL);
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
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
sis_read_eeprom(struct sis_softc *sc, caddr_t dest, int off, int cnt, int swap)
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
}

#if defined(__i386__) || defined(__amd64__)
static device_t
sis_find_bridge(device_t dev)
{
	devclass_t		pci_devclass;
	device_t		*pci_devices;
	int			pci_count = 0;
	device_t		*pci_children;
	int			pci_childcount = 0;
	device_t		*busp, *childp;
	device_t		child = NULL;
	int			i, j;

	if ((pci_devclass = devclass_find("pci")) == NULL)
		return(NULL);

	devclass_get_devices(pci_devclass, &pci_devices, &pci_count);

	for (i = 0, busp = pci_devices; i < pci_count; i++, busp++) {
		pci_childcount = 0;
		device_get_children(*busp, &pci_children, &pci_childcount);
		for (j = 0, childp = pci_children;
		    j < pci_childcount; j++, childp++) {
			if (pci_get_vendor(*childp) == SIS_VENDORID &&
			    pci_get_device(*childp) == 0x0008) {
				child = *childp;
				goto done;
			}
		}
	}

done:
	free(pci_devices, M_TEMP);
	free(pci_children, M_TEMP);
	return(child);
}

static void
sis_read_cmos(struct sis_softc *sc, device_t dev, caddr_t dest, int off, int cnt)
{
	device_t		bridge;
	u_int8_t		reg;
	int			i;
	bus_space_tag_t		btag;

	bridge = sis_find_bridge(dev);
	if (bridge == NULL)
		return;
	reg = pci_read_config(bridge, 0x48, 1);
	pci_write_config(bridge, 0x48, reg|0x40, 1);

	/* XXX */
#if defined(__i386__)
	btag = I386_BUS_SPACE_IO;
#elif defined(__amd64__)
	btag = AMD64_BUS_SPACE_IO;
#endif

	for (i = 0; i < cnt; i++) {
		bus_space_write_1(btag, 0x0, 0x70, i + off);
		*(dest + i) = bus_space_read_1(btag, 0x0, 0x71);
	}

	pci_write_config(bridge, 0x48, reg & ~0x40, 1);
	return;
}

static void
sis_read_mac(struct sis_softc *sc, device_t dev, caddr_t dest)
{
	u_int32_t		filtsave, csrsave;

	filtsave = CSR_READ_4(sc, SIS_RXFILT_CTL);
	csrsave = CSR_READ_4(sc, SIS_CSR);

	CSR_WRITE_4(sc, SIS_CSR, SIS_CSR_RELOAD | filtsave);
	CSR_WRITE_4(sc, SIS_CSR, 0);
		
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave & ~SIS_RXFILTCTL_ENABLE);

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR0);
	((u_int16_t *)dest)[0] = CSR_READ_2(sc, SIS_RXFILT_DATA);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL,SIS_FILTADDR_PAR1);
	((u_int16_t *)dest)[1] = CSR_READ_2(sc, SIS_RXFILT_DATA);
	CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR2);
	((u_int16_t *)dest)[2] = CSR_READ_2(sc, SIS_RXFILT_DATA);

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave);
	CSR_WRITE_4(sc, SIS_CSR, csrsave);
	return;
}
#endif

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
sis_mii_sync(struct sis_softc *sc)
{
	int		i;
 
 	SIO_SET(SIS_MII_DIR|SIS_MII_DATA);
 
 	for (i = 0; i < 32; i++) {
 		SIO_SET(SIS_MII_CLK);
 		DELAY(1);
 		SIO_CLR(SIS_MII_CLK);
 		DELAY(1);
 	}
}
 
/*
 * Clock a series of bits through the MII.
 */
static void
sis_mii_send(struct sis_softc *sc, uint32_t bits, int cnt)
{
	int			i;
 
	SIO_CLR(SIS_MII_CLK);
 
	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		if (bits & i) {
			SIO_SET(SIS_MII_DATA);
		} else {
			SIO_CLR(SIS_MII_DATA);
		}
		DELAY(1);
		SIO_CLR(SIS_MII_CLK);
		DELAY(1);
		SIO_SET(SIS_MII_CLK);
	}
}
 
/*
 * Read an PHY register through the MII.
 */
static int
sis_mii_readreg(struct sis_softc *sc, struct sis_mii_frame *frame)
{
	int			i, ack, s;
 
	s = splimp();
 
	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = SIS_MII_STARTDELIM;
	frame->mii_opcode = SIS_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
 	
	/*
 	 * Turn on data xmit.
	 */
	SIO_SET(SIS_MII_DIR);

	sis_mii_sync(sc);
 
	/*
	 * Send command/address info.
	 */
	sis_mii_send(sc, frame->mii_stdelim, 2);
	sis_mii_send(sc, frame->mii_opcode, 2);
	sis_mii_send(sc, frame->mii_phyaddr, 5);
	sis_mii_send(sc, frame->mii_regaddr, 5);
 
	/* Idle bit */
	SIO_CLR((SIS_MII_CLK|SIS_MII_DATA));
	DELAY(1);
	SIO_SET(SIS_MII_CLK);
	DELAY(1);
 
	/* Turn off xmit. */
	SIO_CLR(SIS_MII_DIR);
 
	/* Check for ack */
	SIO_CLR(SIS_MII_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, SIS_EECTL) & SIS_MII_DATA;
	SIO_SET(SIS_MII_CLK);
	DELAY(1);
 
	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(SIS_MII_CLK);
			DELAY(1);
			SIO_SET(SIS_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}
 
	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(SIS_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, SIS_EECTL) & SIS_MII_DATA)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(SIS_MII_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(SIS_MII_CLK);
	DELAY(1);
	SIO_SET(SIS_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}
 
/*
 * Write to a PHY register through the MII.
 */
static int
sis_mii_writereg(struct sis_softc *sc, struct sis_mii_frame *frame)
{
	int			s;
 
	 s = splimp();
 	/*
 	 * Set up frame for TX.
 	 */
 
 	frame->mii_stdelim = SIS_MII_STARTDELIM;
 	frame->mii_opcode = SIS_MII_WRITEOP;
 	frame->mii_turnaround = SIS_MII_TURNAROUND;
 	
 	/*
  	 * Turn on data output.
 	 */
 	SIO_SET(SIS_MII_DIR);
 
 	sis_mii_sync(sc);
 
 	sis_mii_send(sc, frame->mii_stdelim, 2);
 	sis_mii_send(sc, frame->mii_opcode, 2);
 	sis_mii_send(sc, frame->mii_phyaddr, 5);
 	sis_mii_send(sc, frame->mii_regaddr, 5);
 	sis_mii_send(sc, frame->mii_turnaround, 2);
 	sis_mii_send(sc, frame->mii_data, 16);
 
 	/* Idle bit. */
 	SIO_SET(SIS_MII_CLK);
 	DELAY(1);
 	SIO_CLR(SIS_MII_CLK);
 	DELAY(1);
 
 	/*
 	 * Turn off xmit.
 	 */
 	SIO_CLR(SIS_MII_DIR);
 
 	splx(s);
 
 	return(0);
}

static int
sis_miibus_readreg(device_t dev, int phy, int reg)
{
	struct sis_softc	*sc;
	struct sis_mii_frame    frame;

	sc = device_get_softc(dev);

	if (sc->sis_type == SIS_TYPE_83815) {
		if (phy != 0)
			return(0);
		/*
		 * The NatSemi chip can take a while after
		 * a reset to come ready, during which the BMSR
		 * returns a value of 0. This is *never* supposed
		 * to happen: some of the BMSR bits are meant to
		 * be hardwired in the on position, and this can
		 * confuse the miibus code a bit during the probe
		 * and attach phase. So we make an effort to check
		 * for this condition and wait for it to clear.
		 */
		if (!CSR_READ_4(sc, NS_BMSR))
			DELAY(1000);
		return CSR_READ_4(sc, NS_BMCR + (reg * 4));
	}

	/*
	 * Chipsets < SIS_635 seem not to be able to read/write
	 * through mdio. Use the enhanced PHY access register
	 * again for them.
	 */
	if (sc->sis_type == SIS_TYPE_900 &&
	    sc->sis_rev < SIS_REV_635) {
		int i, val = 0;

		if (phy != 0)
			return(0);

		CSR_WRITE_4(sc, SIS_PHYCTL,
		    (phy << 11) | (reg << 6) | SIS_PHYOP_READ);
		SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

		for (i = 0; i < SIS_TIMEOUT; i++) {
			if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
				break;
		}

		if (i == SIS_TIMEOUT) {
			printf("sis%d: PHY failed to come ready\n",
			    sc->sis_unit);
			return(0);
		}

		val = (CSR_READ_4(sc, SIS_PHYCTL) >> 16) & 0xFFFF;

		if (val == 0xFFFF)
			return(0);

		return(val);
	} else {
		bzero((char *)&frame, sizeof(frame));

		frame.mii_phyaddr = phy;
		frame.mii_regaddr = reg;
		sis_mii_readreg(sc, &frame);

		return(frame.mii_data);
	}
}

static int
sis_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct sis_softc	*sc;
	struct sis_mii_frame	frame;

	sc = device_get_softc(dev);

	if (sc->sis_type == SIS_TYPE_83815) {
		if (phy != 0)
			return(0);
		CSR_WRITE_4(sc, NS_BMCR + (reg * 4), data);
		return(0);
	}

	/*
	 * Chipsets < SIS_635 seem not to be able to read/write
	 * through mdio. Use the enhanced PHY access register
	 * again for them.
	 */
	if (sc->sis_type == SIS_TYPE_900 &&
	    sc->sis_rev < SIS_REV_635) {
		int i;

		if (phy != 0)
			return(0);

		CSR_WRITE_4(sc, SIS_PHYCTL, (data << 16) | (phy << 11) |
		    (reg << 6) | SIS_PHYOP_WRITE);
		SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

		for (i = 0; i < SIS_TIMEOUT; i++) {
			if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
				break;
		}

		if (i == SIS_TIMEOUT)
			printf("sis%d: PHY failed to come ready\n",
			    sc->sis_unit);
	} else {
		bzero((char *)&frame, sizeof(frame));

		frame.mii_phyaddr = phy;
		frame.mii_regaddr = reg;
		frame.mii_data = data;
		sis_mii_writereg(sc, &frame);
	}
	return(0);
}

static void
sis_miibus_statchg(device_t dev)
{
	struct sis_softc	*sc;

	sc = device_get_softc(dev);
	SIS_LOCK_ASSERT(sc);
	sis_initl(sc);
}

static uint32_t
sis_mchash(struct sis_softc *sc, const uint8_t *addr)
{
	uint32_t		crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_be(addr, ETHER_ADDR_LEN);

	/*
	 * return the filter bit position
	 *
	 * The NatSemi chip has a 512-bit filter, which is
	 * different than the SiS, so we special-case it.
	 */
	if (sc->sis_type == SIS_TYPE_83815)
		return (crc >> 23);
	else if (sc->sis_rev >= SIS_REV_635 ||
	    sc->sis_rev == SIS_REV_900B)
		return (crc >> 24);
	else
		return (crc >> 25);
}

static void
sis_setmulti_ns(struct sis_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i, filtsave;
	int			bit, index;

	ifp = sc->sis_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SIS_CLRBIT(sc, SIS_RXFILT_CTL, NS_RXFILTCTL_MCHASH);
		SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);
		return;
	}

	/*
	 * We have to explicitly enable the multicast hash table
	 * on the NatSemi chip if we want to use it, which we do.
	 */
	SIS_SETBIT(sc, SIS_RXFILT_CTL, NS_RXFILTCTL_MCHASH);
	SIS_CLRBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ALLMULTI);

	filtsave = CSR_READ_4(sc, SIS_RXFILT_CTL);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 32; i++) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_FMEM_LO + (i*2));
		CSR_WRITE_4(sc, SIS_RXFILT_DATA, 0);
	}

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = sis_mchash(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		index = h >> 3;
		bit = h & 0x1F;
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_FMEM_LO + index);
		if (bit > 0xF)
			bit -= 0x10;
		SIS_SETBIT(sc, SIS_RXFILT_DATA, (1 << bit));
	}
	IF_ADDR_UNLOCK(ifp);

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave);

	return;
}

static void
sis_setmulti_sis(struct sis_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h, i, n, ctl;
	u_int16_t		hashes[16];

	ifp = sc->sis_ifp;

	/* hash table size */
	if (sc->sis_rev >= SIS_REV_635 ||
	    sc->sis_rev == SIS_REV_900B)
		n = 16;
	else
		n = 8;

	ctl = CSR_READ_4(sc, SIS_RXFILT_CTL) & SIS_RXFILTCTL_ENABLE;

	if (ifp->if_flags & IFF_BROADCAST)
		ctl |= SIS_RXFILTCTL_BROAD;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		ctl |= SIS_RXFILTCTL_ALLMULTI;
		if (ifp->if_flags & IFF_PROMISC)
			ctl |= SIS_RXFILTCTL_BROAD|SIS_RXFILTCTL_ALLPHYS;
		for (i = 0; i < n; i++)
			hashes[i] = ~0;
	} else {
		for (i = 0; i < n; i++)
			hashes[i] = 0;
		i = 0;
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
			h = sis_mchash(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
			hashes[h >> 4] |= 1 << (h & 0xf);
			i++;
		}
		IF_ADDR_UNLOCK(ifp);
		if (i > n) {
			ctl |= SIS_RXFILTCTL_ALLMULTI;
			for (i = 0; i < n; i++)
				hashes[i] = ~0;
		}
	}

	for (i = 0; i < n; i++) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, (4 + i) << 16);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA, hashes[i]);
	}

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, ctl);
}

static void
sis_reset(struct sis_softc *sc)
{
	int		i;

	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RESET);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_CSR) & SIS_CSR_RESET))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: reset never completed\n", sc->sis_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	/*
	 * If this is a NetSemi chip, make sure to clear
	 * PME mode.
	 */
	if (sc->sis_type == SIS_TYPE_83815) {
		CSR_WRITE_4(sc, NS_CLKRUN, NS_CLKRUN_PMESTS);
		CSR_WRITE_4(sc, NS_CLKRUN, 0);
	}

        return;
}

/*
 * Probe for an SiS chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
sis_probe(device_t dev)
{
	struct sis_type		*t;

	t = sis_devs;

	while(t->sis_name != NULL) {
		if ((pci_get_vendor(dev) == t->sis_vid) &&
		    (pci_get_device(dev) == t->sis_did)) {
			device_set_desc(dev, t->sis_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
sis_attach(device_t dev)
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid, waittime = 0;

	waittime = 0;
	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	sc->sis_self = dev;

	mtx_init(&sc->sis_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	if (pci_get_device(dev) == SIS_DEVICEID_900)
		sc->sis_type = SIS_TYPE_900;
	if (pci_get_device(dev) == SIS_DEVICEID_7016)
		sc->sis_type = SIS_TYPE_7016;
	if (pci_get_vendor(dev) == NS_VENDORID)
		sc->sis_type = SIS_TYPE_83815;

	sc->sis_rev = pci_read_config(dev, PCIR_REVID, 1);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = SIS_RID;
	sc->sis_res = bus_alloc_resource_any(dev, SIS_RES, &rid, RF_ACTIVE);

	if (sc->sis_res == NULL) {
		printf("sis%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->sis_btag = rman_get_bustag(sc->sis_res);
	sc->sis_bhandle = rman_get_bushandle(sc->sis_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sis_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sis_irq == NULL) {
		printf("sis%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	sis_reset(sc);

	if (sc->sis_type == SIS_TYPE_900 &&
            (sc->sis_rev == SIS_REV_635 ||
            sc->sis_rev == SIS_REV_900B)) {
		SIO_SET(SIS_CFG_RND_CNT);
		SIO_SET(SIS_CFG_PERR_DETECT);
	}

	/*
	 * Get station address from the EEPROM.
	 */
	switch (pci_get_vendor(dev)) {
	case NS_VENDORID:
		sc->sis_srr = CSR_READ_4(sc, NS_SRR);

		/* We can't update the device description, so spew */
		if (sc->sis_srr == NS_SRR_15C)
			device_printf(dev, "Silicon Revision: DP83815C\n");
		else if (sc->sis_srr == NS_SRR_15D)
			device_printf(dev, "Silicon Revision: DP83815D\n");
		else if (sc->sis_srr == NS_SRR_16A)
			device_printf(dev, "Silicon Revision: DP83816A\n");
		else
			device_printf(dev, "Silicon Revision %x\n", sc->sis_srr);

		/*
		 * Reading the MAC address out of the EEPROM on
		 * the NatSemi chip takes a bit more work than
		 * you'd expect. The address spans 4 16-bit words,
		 * with the first word containing only a single bit.
		 * You have to shift everything over one bit to
		 * get it aligned properly. Also, the bits are
		 * stored backwards (the LSB is really the MSB,
		 * and so on) so you have to reverse them in order
		 * to get the MAC address into the form we want.
		 * Why? Who the hell knows.
		 */
		{
			u_int16_t		tmp[4];

			sis_read_eeprom(sc, (caddr_t)&tmp,
			    NS_EE_NODEADDR, 4, 0);

			/* Shift everything over one bit. */
			tmp[3] = tmp[3] >> 1;
			tmp[3] |= tmp[2] << 15;
			tmp[2] = tmp[2] >> 1;
			tmp[2] |= tmp[1] << 15;
			tmp[1] = tmp[1] >> 1;
			tmp[1] |= tmp[0] << 15;

			/* Now reverse all the bits. */
			tmp[3] = sis_reverse(tmp[3]);
			tmp[2] = sis_reverse(tmp[2]);
			tmp[1] = sis_reverse(tmp[1]);

			bcopy((char *)&tmp[1], eaddr, ETHER_ADDR_LEN);
		}
		break;
	case SIS_VENDORID:
	default:
#if defined(__i386__) || defined(__amd64__)
		/*
		 * If this is a SiS 630E chipset with an embedded
		 * SiS 900 controller, we have to read the MAC address
		 * from the APC CMOS RAM. Our method for doing this
		 * is very ugly since we have to reach out and grab
		 * ahold of hardware for which we cannot properly
		 * allocate resources. This code is only compiled on
		 * the i386 architecture since the SiS 630E chipset
		 * is for x86 motherboards only. Note that there are
		 * a lot of magic numbers in this hack. These are
		 * taken from SiS's Linux driver. I'd like to replace
		 * them with proper symbolic definitions, but that
		 * requires some datasheets that I don't have access
		 * to at the moment.
		 */
		if (sc->sis_rev == SIS_REV_630S ||
		    sc->sis_rev == SIS_REV_630E ||
		    sc->sis_rev == SIS_REV_630EA1)
			sis_read_cmos(sc, dev, (caddr_t)&eaddr, 0x9, 6);

		else if (sc->sis_rev == SIS_REV_635 ||
			 sc->sis_rev == SIS_REV_630ET)
			sis_read_mac(sc, dev, (caddr_t)&eaddr);
		else if (sc->sis_rev == SIS_REV_96x) {
			/* Allow to read EEPROM from LAN. It is shared
			 * between a 1394 controller and the NIC and each
			 * time we access it, we need to set SIS_EECMD_REQ.
			 */
			SIO_SET(SIS_EECMD_REQ);
			for (waittime = 0; waittime < SIS_TIMEOUT;
			    waittime++) {
				/* Force EEPROM to idle state. */
				sis_eeprom_idle(sc);
				if (CSR_READ_4(sc, SIS_EECTL) & SIS_EECMD_GNT) {
					sis_read_eeprom(sc, (caddr_t)&eaddr,
					    SIS_EE_NODEADDR, 3, 0);
					break;
				}
				DELAY(1);
			}
			/*
			 * Set SIS_EECTL_CLK to high, so a other master
			 * can operate on the i2c bus.
			 */
			SIO_SET(SIS_EECTL_CLK);
			/* Refuse EEPROM access by LAN */
			SIO_SET(SIS_EECMD_DONE);
		} else
#endif
			sis_read_eeprom(sc, (caddr_t)&eaddr,
			    SIS_EE_NODEADDR, 3, 0);
		break;
	}

	sc->sis_unit = unit;
	if (debug_mpsafenet)
		callout_init(&sc->sis_stat_ch, CALLOUT_MPSAFE);
	else
		callout_init(&sc->sis_stat_ch, 0);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define SIS_NSEG_NEW 32
	 error = bus_dma_tag_create(NULL,	/* parent */ 
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, SIS_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */ 
			BUS_DMA_ALLOCNOW,	/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->sis_parent_tag);
	if (error)
		goto fail;

	/*
	 * Now allocate a tag for the DMA descriptor lists and a chunk
	 * of DMA-able memory based on the tag.  Also obtain the physical
	 * addresses of the RX and TX ring, which we'll need later.
	 * All of our lists are allocated as a contiguous block
	 * of memory.
	 */
	error = bus_dma_tag_create(sc->sis_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			SIS_RX_LIST_SZ, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			busdma_lock_mutex,	/* lockfunc */
			&Giant,			/* lockarg */
			&sc->sis_rx_tag);
	if (error)
		goto fail;

	error = bus_dmamem_alloc(sc->sis_rx_tag,
	    (void **)&sc->sis_rx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->sis_rx_dmamap);

	if (error) {
		printf("sis%d: no memory for rx list buffers!\n", unit);
		bus_dma_tag_destroy(sc->sis_rx_tag);
		sc->sis_rx_tag = NULL;
		goto fail;
	}

	error = bus_dmamap_load(sc->sis_rx_tag,
	    sc->sis_rx_dmamap, &(sc->sis_rx_list[0]),
	    sizeof(struct sis_desc), sis_dma_map_ring,
	    &sc->sis_rx_paddr, 0);

	if (error) {
		printf("sis%d: cannot get address of the rx ring!\n", unit);
		bus_dmamem_free(sc->sis_rx_tag,
		    sc->sis_rx_list, sc->sis_rx_dmamap);
		bus_dma_tag_destroy(sc->sis_rx_tag);
		sc->sis_rx_tag = NULL;
		goto fail;
	}

	error = bus_dma_tag_create(sc->sis_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			SIS_TX_LIST_SZ, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			busdma_lock_mutex,	/* lockfunc */
			&Giant,			/* lockarg */
			&sc->sis_tx_tag);
	if (error)
		goto fail;

	error = bus_dmamem_alloc(sc->sis_tx_tag,
	    (void **)&sc->sis_tx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->sis_tx_dmamap);

	if (error) {
		printf("sis%d: no memory for tx list buffers!\n", unit);
		bus_dma_tag_destroy(sc->sis_tx_tag);
		sc->sis_tx_tag = NULL;
		goto fail;
	}

	error = bus_dmamap_load(sc->sis_tx_tag,
	    sc->sis_tx_dmamap, &(sc->sis_tx_list[0]),
	    sizeof(struct sis_desc), sis_dma_map_ring,
	    &sc->sis_tx_paddr, 0);

	if (error) {
		printf("sis%d: cannot get address of the tx ring!\n", unit);
		bus_dmamem_free(sc->sis_tx_tag,
		    sc->sis_tx_list, sc->sis_tx_dmamap);
		bus_dma_tag_destroy(sc->sis_tx_tag);
		sc->sis_tx_tag = NULL;
		goto fail;
	}

	error = bus_dma_tag_create(sc->sis_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MCLBYTES, 1,		/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			busdma_lock_mutex,	/* lockfunc */
			&Giant,			/* lockarg */
			&sc->sis_tag);
	if (error)
		goto fail;

	/*
	 * Obtain the physical addresses of the RX and TX
	 * rings which we'll need later in the init routine.
	 */

	ifp = sc->sis_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("sis%d: can not if_alloc()\n", sc->sis_unit);
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sis_ioctl;
	ifp->if_start = sis_start;
	ifp->if_watchdog = sis_watchdog;
	ifp->if_init = sis_init;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, SIS_TX_LIST_CNT - 1);
	ifp->if_snd.ifq_drv_maxlen = SIS_TX_LIST_CNT - 1;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Do MII setup.
	 */
	if (mii_phy_probe(dev, &sc->sis_miibus,
	    sis_ifmedia_upd, sis_ifmedia_sts)) {
		printf("sis%d: MII without any PHY!\n", sc->sis_unit);
		if_free(ifp);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	
	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	ifp->if_capenable = ifp->if_capabilities;

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->sis_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    sis_intr, sc, &sc->sis_intrhand);

	if (error) {
		printf("sis%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		if_free(ifp);
		goto fail;
	}

fail:
	if (error)
		sis_detach(dev);

	return(error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
sis_detach(device_t dev)
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->sis_mtx), ("sis mutex not initialized"));
	SIS_LOCK(sc);
	ifp = sc->sis_ifp;

	/* These should only be active if attach succeeded. */
	if (device_is_attached(dev)) {
		sis_reset(sc);
		sis_stop(sc);
		ether_ifdetach(ifp);
		if_free(ifp);
	}
	if (sc->sis_miibus)
		device_delete_child(dev, sc->sis_miibus);
	bus_generic_detach(dev);

	if (sc->sis_intrhand)
		bus_teardown_intr(dev, sc->sis_irq, sc->sis_intrhand);
	if (sc->sis_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
	if (sc->sis_res)
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);

	if (sc->sis_rx_tag) {
		bus_dmamap_unload(sc->sis_rx_tag,
		    sc->sis_rx_dmamap);
		bus_dmamem_free(sc->sis_rx_tag,
		    sc->sis_rx_list, sc->sis_rx_dmamap);
		bus_dma_tag_destroy(sc->sis_rx_tag);
	}
	if (sc->sis_tx_tag) {
		bus_dmamap_unload(sc->sis_tx_tag,
		    sc->sis_tx_dmamap);
		bus_dmamem_free(sc->sis_tx_tag,
		    sc->sis_tx_list, sc->sis_tx_dmamap);
		bus_dma_tag_destroy(sc->sis_tx_tag);
	}
	if (sc->sis_parent_tag)
		bus_dma_tag_destroy(sc->sis_parent_tag);
	if (sc->sis_tag)
		bus_dma_tag_destroy(sc->sis_tag);

	SIS_UNLOCK(sc);
	mtx_destroy(&sc->sis_mtx);

	return(0);
}

/*
 * Initialize the TX and RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
sis_ring_init(struct sis_softc *sc)
{
	int i, error;
	struct sis_desc *dp;

	dp = &sc->sis_tx_list[0];
	for (i = 0; i < SIS_TX_LIST_CNT; i++, dp++) {
		if (i == (SIS_TX_LIST_CNT - 1))
			dp->sis_nextdesc = &sc->sis_tx_list[0];
		else
			dp->sis_nextdesc = dp + 1;
		bus_dmamap_load(sc->sis_tx_tag,
		    sc->sis_tx_dmamap,
		    dp->sis_nextdesc, sizeof(struct sis_desc),
		    sis_dma_map_desc_next, dp, 0);
		dp->sis_mbuf = NULL;
		dp->sis_ptr = 0;
		dp->sis_ctl = 0;
	}

	sc->sis_tx_prod = sc->sis_tx_cons = sc->sis_tx_cnt = 0;

	bus_dmamap_sync(sc->sis_tx_tag,
	    sc->sis_tx_dmamap, BUS_DMASYNC_PREWRITE);

	dp = &sc->sis_rx_list[0];
	for (i = 0; i < SIS_RX_LIST_CNT; i++, dp++) {
		error = sis_newbuf(sc, dp, NULL);
		if (error)
			return(error);
		if (i == (SIS_RX_LIST_CNT - 1))
			dp->sis_nextdesc = &sc->sis_rx_list[0];
		else
			dp->sis_nextdesc = dp + 1;
		bus_dmamap_load(sc->sis_rx_tag,
		    sc->sis_rx_dmamap,
		    dp->sis_nextdesc, sizeof(struct sis_desc),
		    sis_dma_map_desc_next, dp, 0);
		}

	bus_dmamap_sync(sc->sis_rx_tag,
	    sc->sis_rx_dmamap, BUS_DMASYNC_PREWRITE);

	sc->sis_rx_pdsc = &sc->sis_rx_list[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
sis_newbuf(struct sis_softc *sc, struct sis_desc *c, struct mbuf *m)
{

	if (c == NULL)
		return(EINVAL);

	if (m == NULL) {
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			return(ENOBUFS);
	} else
		m->m_data = m->m_ext.ext_buf;

	c->sis_mbuf = m;
	c->sis_ctl = SIS_RXLEN;

	bus_dmamap_create(sc->sis_tag, 0, &c->sis_map);
	bus_dmamap_load(sc->sis_tag, c->sis_map,
	    mtod(m, void *), MCLBYTES,
	    sis_dma_map_desc_ptr, c, 0);
	bus_dmamap_sync(sc->sis_tag, c->sis_map, BUS_DMASYNC_PREREAD);

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
sis_rxeof(struct sis_softc *sc)
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct sis_desc		*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

	SIS_LOCK_ASSERT(sc);

	ifp = sc->sis_ifp;

	for(cur_rx = sc->sis_rx_pdsc; SIS_OWNDESC(cur_rx);
	    cur_rx = cur_rx->sis_nextdesc) {

#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		rxstat = cur_rx->sis_rxstat;
		bus_dmamap_sync(sc->sis_tag,
		    cur_rx->sis_map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sis_tag, cur_rx->sis_map);
		bus_dmamap_destroy(sc->sis_tag, cur_rx->sis_map);
		m = cur_rx->sis_mbuf;
		cur_rx->sis_mbuf = NULL;
		total_len = SIS_RXBYTES(cur_rx);

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
#if defined(__i386__) || defined(__amd64__)
		/*
		 * On the x86 we do not have alignment problems, so try to
		 * allocate a new buffer for the receive ring, and pass up
		 * the one where the packet is already, saving the expensive
		 * copy done in m_devget().
		 * If we are on an architecture with alignment problems, or
		 * if the allocation fails, then use m_devget and leave the
		 * existing buffer in the receive ring.
		 */
		if (sis_newbuf(sc, cur_rx, NULL) == 0)
			m->m_pkthdr.len = m->m_len = total_len;
		else
#endif
		{
			struct mbuf		*m0;
			m0 = m_devget(mtod(m, char *), total_len,
				ETHER_ALIGN, ifp, NULL);
			sis_newbuf(sc, cur_rx, m);
			if (m0 == NULL) {
				ifp->if_ierrors++;
				continue;
			}
			m = m0;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		SIS_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		SIS_LOCK(sc);
	}

	sc->sis_rx_pdsc = cur_rx;
}

static void
sis_rxeoc(struct sis_softc *sc)
{

	SIS_LOCK_ASSERT(sc);
	sis_rxeof(sc);
	sis_initl(sc);
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
sis_txeof(struct sis_softc *sc)
{
	struct ifnet		*ifp;
	u_int32_t		idx;

	SIS_LOCK_ASSERT(sc);
	ifp = sc->sis_ifp;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (idx = sc->sis_tx_cons; sc->sis_tx_cnt > 0;
	    sc->sis_tx_cnt--, SIS_INC(idx, SIS_TX_LIST_CNT) ) {
		struct sis_desc *cur_tx = &sc->sis_tx_list[idx];

		if (SIS_OWNDESC(cur_tx))
			break;

		if (cur_tx->sis_ctl & SIS_CMDSTS_MORE)
			continue;

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
			bus_dmamap_unload(sc->sis_tag, cur_tx->sis_map);
			bus_dmamap_destroy(sc->sis_tag, cur_tx->sis_map);
		}
	}

	if (idx != sc->sis_tx_cons) {
		/* we freed up some buffers */
		sc->sis_tx_cons = idx;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	}

	ifp->if_timer = (sc->sis_tx_cnt == 0) ? 0 : 5;

	return;
}

static void
sis_tick(void *xsc)
{
	struct sis_softc	*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;

	sc = xsc;
	SIS_LOCK(sc);
	sc->in_tick = 1;
	ifp = sc->sis_ifp;

	mii = device_get_softc(sc->sis_miibus);
	mii_tick(mii);

	if (!sc->sis_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sis_link++;
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			sis_startl(ifp);
	}

	callout_reset(&sc->sis_stat_ch, hz,  sis_tick, sc);
	sc->in_tick = 0;
	SIS_UNLOCK(sc);
}

#ifdef DEVICE_POLLING
static poll_handler_t sis_poll;

static void
sis_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct	sis_softc *sc = ifp->if_softc;

	SIS_LOCK(sc);
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_4(sc, SIS_IER, 1);
		goto done;
	}

	/*
	 * On the sis, reading the status register also clears it.
	 * So before returning to intr mode we must make sure that all
	 * possible pending sources of interrupts have been served.
	 * In practice this means run to completion the *eof routines,
	 * and then call the interrupt routine
	 */
	sc->rxcycles = count;
	sis_rxeof(sc);
	sis_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sis_startl(ifp);

	if (sc->rxcycles > 0 || cmd == POLL_AND_CHECK_STATUS) {
		u_int32_t	status;

		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, SIS_ISR);

		if (status & (SIS_ISR_RX_ERR|SIS_ISR_RX_OFLOW))
			sis_rxeoc(sc);

		if (status & (SIS_ISR_RX_IDLE))
			SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RX_ENABLE);

		if (status & SIS_ISR_SYSERR) {
			sis_reset(sc);
			sis_initl(sc);
		}
	}
done:
	SIS_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
sis_intr(void *arg)
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = sc->sis_ifp;

	if (sc->sis_stopped)	/* Most likely shared interrupt */
		return;

	SIS_LOCK(sc);
#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		goto done;
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(sis_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_4(sc, SIS_IER, 0);
		goto done;
	}
#endif /* DEVICE_POLLING */

	/* Disable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 0);

	for (;;) {
		SIS_LOCK_ASSERT(sc);
		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, SIS_ISR);

		if ((status & SIS_INTRS) == 0)
			break;

		if (status &
		    (SIS_ISR_TX_DESC_OK | SIS_ISR_TX_ERR |
		     SIS_ISR_TX_OK | SIS_ISR_TX_IDLE) )
			sis_txeof(sc);

		if (status & (SIS_ISR_RX_DESC_OK|SIS_ISR_RX_OK|SIS_ISR_RX_IDLE))
			sis_rxeof(sc);

		if (status & (SIS_ISR_RX_ERR | SIS_ISR_RX_OFLOW))
			sis_rxeoc(sc);

		if (status & (SIS_ISR_RX_IDLE))
			SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RX_ENABLE);

		if (status & SIS_ISR_SYSERR) {
			sis_reset(sc);
			sis_initl(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 1);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sis_startl(ifp);

#ifdef DEVICE_POLLING
done:
#endif /* DEVICE_POLLING */
	SIS_UNLOCK(sc);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
sis_encap(struct sis_softc *sc, struct mbuf **m_head, uint32_t *txidx)
{
	struct sis_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0, chainlen = 0;

	/*
	 * If there's no way we can send any packets, return now.
	 */
	if (SIS_TX_LIST_CNT - sc->sis_tx_cnt < 2)
		return (ENOBUFS);

	/*
	 * Count the number of frags in this chain to see if
	 * we need to m_defrag.  Since the descriptor list is shared
	 * by all packets, we'll m_defrag long chains so that they
	 * do not use up the entire list, even if they would fit.
	 */

	for (m = *m_head; m != NULL; m = m->m_next)
		chainlen++;

	if ((chainlen > SIS_TX_LIST_CNT / 4) ||
	    ((SIS_TX_LIST_CNT - (chainlen + sc->sis_tx_cnt)) < 2)) {
		m = m_defrag(*m_head, M_DONTWAIT);
		if (m == NULL)
			return (ENOBUFS);
		*m_head = m;
	}
	
	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	cur = frag = *txidx;

	for (m = *m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((SIS_TX_LIST_CNT -
			    (sc->sis_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->sis_tx_list[frag];
			f->sis_ctl = SIS_CMDSTS_MORE | m->m_len;
			bus_dmamap_create(sc->sis_tag, 0, &f->sis_map);
			bus_dmamap_load(sc->sis_tag, f->sis_map,
			    mtod(m, void *), m->m_len,
			    sis_dma_map_desc_ptr, f, 0);
			bus_dmamap_sync(sc->sis_tag,
			    f->sis_map, BUS_DMASYNC_PREREAD);
			if (cnt != 0)
				f->sis_ctl |= SIS_CMDSTS_OWN;
			cur = frag;
			SIS_INC(frag, SIS_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc->sis_tx_list[cur].sis_mbuf = *m_head;
	sc->sis_tx_list[cur].sis_ctl &= ~SIS_CMDSTS_MORE;
	sc->sis_tx_list[*txidx].sis_ctl |= SIS_CMDSTS_OWN;
	sc->sis_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void
sis_start(struct ifnet *ifp)
{
	struct sis_softc	*sc;

	sc = ifp->if_softc;
	SIS_LOCK(sc);
	sis_startl(ifp);
	SIS_UNLOCK(sc);
}

static void
sis_startl(struct ifnet *ifp)
{
	struct sis_softc	*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx, queued = 0;

	sc = ifp->if_softc;

	SIS_LOCK_ASSERT(sc);

	if (!sc->sis_link)
		return;

	idx = sc->sis_tx_prod;

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	while(sc->sis_tx_list[idx].sis_mbuf == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (sis_encap(sc, &m_head, &idx)) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		queued++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);

	}

	if (queued) {
		/* Transmit */
		sc->sis_tx_prod = idx;
		SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_ENABLE);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}
}

static void
sis_init(void *xsc)
{
	struct sis_softc	*sc = xsc;

	SIS_LOCK(sc);
	sis_initl(sc);
	SIS_UNLOCK(sc);
}

static void
sis_initl(struct sis_softc *sc)
{
	struct ifnet		*ifp = sc->sis_ifp;
	struct mii_data		*mii;

	SIS_LOCK_ASSERT(sc);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	sis_stop(sc);
	sc->sis_stopped = 0;

#ifdef notyet
	if (sc->sis_type == SIS_TYPE_83815 && sc->sis_srr >= NS_SRR_16A) {
		/*
		 * Configure 400usec of interrupt holdoff.  This is based
		 * on emperical tests on a Soekris 4801.
 		 */
		CSR_WRITE_4(sc, NS_IHR, 0x100 | 4);
	}
#endif

	mii = device_get_softc(sc->sis_miibus);

	/* Set MAC address */
	if (sc->sis_type == SIS_TYPE_83815) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR0);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[0]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR1);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[1]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR2);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[2]);
	} else {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR0);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[0]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR1);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[1]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR2);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)IFP2ENADDR(sc->sis_ifp))[2]);
	}

	/* Init circular TX/RX lists. */
	if (sis_ring_init(sc) != 0) {
		printf("sis%d: initialization failed: no "
			"memory for rx buffers\n", sc->sis_unit);
		sis_stop(sc);
		return;
	}

	/*
	 * Short Cable Receive Errors (MP21.E)
	 * also: Page 78 of the DP83815 data sheet (september 2002 version)
	 * recommends the following register settings "for optimum
	 * performance." for rev 15C.  The driver from NS also sets
	 * the PHY_CR register for later versions.
	 */
	if (sc->sis_type == SIS_TYPE_83815 && sc->sis_srr <= NS_SRR_15D) {
		CSR_WRITE_4(sc, NS_PHY_PAGE, 0x0001);
		CSR_WRITE_4(sc, NS_PHY_CR, 0x189C);
		if (sc->sis_srr == NS_SRR_15C) {
			/* set val for c2 */
			CSR_WRITE_4(sc, NS_PHY_TDATA, 0x0000);
			/* load/kill c2 */
			CSR_WRITE_4(sc, NS_PHY_DSPCFG, 0x5040);
			/* rais SD off, from 4 to c */
			CSR_WRITE_4(sc, NS_PHY_SDCFG, 0x008C);
		}
		CSR_WRITE_4(sc, NS_PHY_PAGE, 0);
	}


	/*
	 * For the NatSemi chip, we have to explicitly enable the
	 * reception of ARP frames, as well as turn on the 'perfect
	 * match' filter where we store the station address, otherwise
	 * we won't receive unicasts meant for this host.
	 */
	if (sc->sis_type == SIS_TYPE_83815) {
		SIS_SETBIT(sc, SIS_RXFILT_CTL, NS_RXFILTCTL_ARP);
		SIS_SETBIT(sc, SIS_RXFILT_CTL, NS_RXFILTCTL_PERFECT);
	}

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
	if (sc->sis_type == SIS_TYPE_83815)
		sis_setmulti_ns(sc);
	else
		sis_setmulti_sis(sc);

	/* Turn the receive filter on */
	SIS_SETBIT(sc, SIS_RXFILT_CTL, SIS_RXFILTCTL_ENABLE);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, sc->sis_rx_paddr);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, sc->sis_tx_paddr);

	/* SIS_CFG_EDB_MASTER_EN indicates the EDB bus is used instead of
	 * the PCI bus. When this bit is set, the Max DMA Burst Size
	 * for TX/RX DMA should be no larger than 16 double words.
	 */
	if (CSR_READ_4(sc, SIS_CFG) & SIS_CFG_EDB_MASTER_EN) {
		CSR_WRITE_4(sc, SIS_RX_CFG, SIS_RXCFG64);
	} else {
		CSR_WRITE_4(sc, SIS_RX_CFG, SIS_RXCFG256);
	}

	/* Accept Long Packets for VLAN support */
	SIS_SETBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_JABBER);

	/* Set TX configuration */
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_10_T) {
		CSR_WRITE_4(sc, SIS_TX_CFG, SIS_TXCFG_10);
	} else {
		CSR_WRITE_4(sc, SIS_TX_CFG, SIS_TXCFG_100);
	}

	/* Set full/half duplex mode. */
	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		SIS_SETBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_SETBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	} else {
		SIS_CLRBIT(sc, SIS_TX_CFG,
		    (SIS_TXCFG_IGN_HBEAT|SIS_TXCFG_IGN_CARR));
		SIS_CLRBIT(sc, SIS_RX_CFG, SIS_RXCFG_RX_TXPKTS);
	}

	if (sc->sis_type == SIS_TYPE_83816) {
		/*
		 * MPII03.D: Half Duplex Excessive Collisions.
		 * Also page 49 in 83816 manual
		 */
		SIS_SETBIT(sc, SIS_TX_CFG, SIS_TXCFG_MPII03D);
	}

	if (sc->sis_type == SIS_TYPE_83815 && sc->sis_srr < NS_SRR_16A &&
	     IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		uint32_t reg;

		/*
		 * Short Cable Receive Errors (MP21.E) 
		 */
		CSR_WRITE_4(sc, NS_PHY_PAGE, 0x0001);
		reg = CSR_READ_4(sc, NS_PHY_DSPCFG) & 0xfff;
		CSR_WRITE_4(sc, NS_PHY_DSPCFG, reg | 0x1000);
		DELAY(100000);
		reg = CSR_READ_4(sc, NS_PHY_TDATA) & 0xff;
		if ((reg & 0x0080) == 0 || (reg > 0xd8 && reg <= 0xff)) {
			device_printf(sc->sis_self,
			    "Applying short cable fix (reg=%x)\n", reg);
			CSR_WRITE_4(sc, NS_PHY_TDATA, 0x00e8);
			reg = CSR_READ_4(sc, NS_PHY_DSPCFG);
			SIS_SETBIT(sc, NS_PHY_DSPCFG, reg | 0x20);
		}
		CSR_WRITE_4(sc, NS_PHY_PAGE, 0);
	}

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, SIS_IMR, SIS_INTRS);
#ifdef DEVICE_POLLING
	/*
	 * ... only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_4(sc, SIS_IER, 0);
	else
#endif /* DEVICE_POLLING */
	CSR_WRITE_4(sc, SIS_IER, 1);

	/* Enable receiver and transmitter. */
	SIS_CLRBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_RX_ENABLE);

#ifdef notdef
	mii_mediachg(mii);
#endif

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (!sc->in_tick)
		callout_reset(&sc->sis_stat_ch, hz,  sis_tick, sc);
}

/*
 * Set media options.
 */
static int
sis_ifmedia_upd(struct ifnet *ifp)
{
	struct sis_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->sis_miibus);
	sc->sis_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
static void
sis_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct sis_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->sis_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static int
sis_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct sis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			sis_init(sc);
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			SIS_LOCK(sc);
			sis_stop(sc);
			SIS_UNLOCK(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		SIS_LOCK(sc);
		if (sc->sis_type == SIS_TYPE_83815)
			sis_setmulti_ns(sc);
		else
			sis_setmulti_sis(sc);
		SIS_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->sis_miibus);
		SIS_LOCK(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		SIS_UNLOCK(sc);
		break;
	case SIOCSIFCAP:
		ifp->if_capenable &= ~IFCAP_POLLING;
		ifp->if_capenable |= ifr->ifr_reqcap & IFCAP_POLLING;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return(error);
}

static void
sis_watchdog(struct ifnet *ifp)
{
	struct sis_softc *sc;

	sc = ifp->if_softc;

	SIS_LOCK(sc);
	if (sc->sis_stopped) {
		SIS_UNLOCK(sc);
		return;
	}

	ifp->if_oerrors++;
	printf("sis%d: watchdog timeout\n", sc->sis_unit);

	sis_stop(sc);
	sis_reset(sc);
	sis_initl(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sis_startl(ifp);

	SIS_UNLOCK(sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
sis_stop(struct sis_softc *sc)
{
	int i;
	struct ifnet *ifp;
	struct sis_desc *dp;

	if (sc->sis_stopped)
		return;
	SIS_LOCK_ASSERT(sc);
	ifp = sc->sis_ifp;
	ifp->if_timer = 0;

	callout_stop(&sc->sis_stat_ch);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif
	CSR_WRITE_4(sc, SIS_IER, 0);
	CSR_WRITE_4(sc, SIS_IMR, 0);
	CSR_READ_4(sc, SIS_ISR); /* clear any interrupts already pending */
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, 0);

	sc->sis_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	dp = &sc->sis_rx_list[0];
	for (i = 0; i < SIS_RX_LIST_CNT; i++, dp++) {
		if (dp->sis_mbuf == NULL)
			continue;
		bus_dmamap_unload(sc->sis_tag, dp->sis_map);
		bus_dmamap_destroy(sc->sis_tag, dp->sis_map);
		m_freem(dp->sis_mbuf);
		dp->sis_mbuf = NULL;
	}
	bzero(sc->sis_rx_list, SIS_RX_LIST_SZ);

	/*
	 * Free the TX list buffers.
	 */
	dp = &sc->sis_tx_list[0];
	for (i = 0; i < SIS_TX_LIST_CNT; i++, dp++) {
		if (dp->sis_mbuf == NULL)
			continue;
		bus_dmamap_unload(sc->sis_tag, dp->sis_map);
		bus_dmamap_destroy(sc->sis_tag, dp->sis_map);
		m_freem(dp->sis_mbuf);
		dp->sis_mbuf = NULL;
	}

	bzero(sc->sis_tx_list, SIS_TX_LIST_SZ);

	sc->sis_stopped = 1;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
sis_shutdown(device_t dev)
{
	struct sis_softc	*sc;

	sc = device_get_softc(dev);
	SIS_LOCK(sc);
	sis_reset(sc);
	sis_stop(sc);
	SIS_UNLOCK(sc);
}

static device_method_t sis_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sis_probe),
	DEVMETHOD(device_attach,	sis_attach),
	DEVMETHOD(device_detach,	sis_detach),
	DEVMETHOD(device_shutdown,	sis_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	sis_miibus_readreg),
	DEVMETHOD(miibus_writereg,	sis_miibus_writereg),
	DEVMETHOD(miibus_statchg,	sis_miibus_statchg),

	{ 0, 0 }
};

static driver_t sis_driver = {
	"sis",
	sis_methods,
	sizeof(struct sis_softc)
};

static devclass_t sis_devclass;

DRIVER_MODULE(sis, pci, sis_driver, sis_devclass, 0, 0);
DRIVER_MODULE(miibus, sis, miibus_driver, miibus_devclass, 0, 0);
