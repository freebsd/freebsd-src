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

#define SIS_USEIOSPACE

#include <pci/if_sisreg.h>

MODULE_DEPEND(sis, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

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
	{ NS_VENDORID, NS_DEVICEID_DP83815, "NatSemi DP83815 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int sis_probe		__P((device_t));
static int sis_attach		__P((device_t));
static int sis_detach		__P((device_t));

static int sis_newbuf		__P((struct sis_softc *,
					struct sis_desc *,
					struct mbuf *));
static int sis_encap		__P((struct sis_softc *,
					struct mbuf *, u_int32_t *));
static void sis_rxeof		__P((struct sis_softc *));
static void sis_rxeoc		__P((struct sis_softc *));
static void sis_txeof		__P((struct sis_softc *));
static void sis_intr		__P((void *));
static void sis_tick		__P((void *));
static void sis_start		__P((struct ifnet *));
static int sis_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void sis_init		__P((void *));
static void sis_stop		__P((struct sis_softc *));
static void sis_watchdog		__P((struct ifnet *));
static void sis_shutdown		__P((device_t));
static int sis_ifmedia_upd	__P((struct ifnet *));
static void sis_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static u_int16_t sis_reverse	__P((u_int16_t));
static void sis_delay		__P((struct sis_softc *));
static void sis_eeprom_idle	__P((struct sis_softc *));
static void sis_eeprom_putbyte	__P((struct sis_softc *, int));
static void sis_eeprom_getword	__P((struct sis_softc *, int, u_int16_t *));
static void sis_read_eeprom	__P((struct sis_softc *, caddr_t, int,
							int, int));
#ifdef __i386__
static void sis_read_cmos	__P((struct sis_softc *, device_t, caddr_t,
							int, int));
static device_t sis_find_bridge	__P((device_t));
#endif

static int sis_miibus_readreg	__P((device_t, int, int));
static int sis_miibus_writereg	__P((device_t, int, int, int));
static void sis_miibus_statchg	__P((device_t));

static void sis_setmulti_sis	__P((struct sis_softc *));
static void sis_setmulti_ns	__P((struct sis_softc *));
static u_int32_t sis_crc	__P((struct sis_softc *, caddr_t));
static void sis_reset		__P((struct sis_softc *));
static int sis_list_rx_init	__P((struct sis_softc *));
static int sis_list_tx_init	__P((struct sis_softc *));

static void sis_dma_map_desc_ptr	__P((void *, bus_dma_segment_t *,
						int, int));
static void sis_dma_map_desc_next	__P((void *, bus_dma_segment_t *,
						int, int));
static void sis_dma_map_ring		__P((void *, bus_dma_segment_t *,
						int, int));
#ifdef SIS_USEIOSPACE
#define SIS_RES			SYS_RES_IOPORT
#define SIS_RID			SIS_PCI_LOIO
#else
#define SIS_RES			SYS_RES_MEMORY
#define SIS_RID			SIS_PCI_LOMEM
#endif

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

#ifdef __i386__
static int sis_quick=1;
SYSCTL_INT(_hw, OID_AUTO, sis_quick, CTLFLAG_RW,
	&sis_quick,0,"do not mdevget in sis driver");
#endif

DRIVER_MODULE(if_sis, pci, sis_driver, sis_devclass, 0, 0);
DRIVER_MODULE(miibus, sis, miibus_driver, miibus_devclass, 0, 0);

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
sis_dma_map_desc_next(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct sis_desc	*r;

	r = arg;
	r->sis_next = segs->ds_addr;

	return;
}

static void
sis_dma_map_desc_ptr(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct sis_desc	*r;

	r = arg;
	r->sis_ptr = segs->ds_addr;

	return;
}

static void
sis_dma_map_ring(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	u_int32_t *p;

	p = arg;
	*p = segs->ds_addr;

	return;
}

/*
 * Routine to reverse the bits in a word. Stolen almost
 * verbatim from /usr/games/fortune.
 */
static u_int16_t sis_reverse(n)
	u_int16_t		n;
{
	n = ((n >>  1) & 0x5555) | ((n <<  1) & 0xaaaa);
	n = ((n >>  2) & 0x3333) | ((n <<  2) & 0xcccc);
	n = ((n >>  4) & 0x0f0f) | ((n <<  4) & 0xf0f0);
	n = ((n >>  8) & 0x00ff) | ((n <<  8) & 0xff00);

	return(n);
}

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

#ifdef __i386__
static device_t sis_find_bridge(dev)
	device_t		dev;
{
	devclass_t		pci_devclass;
	device_t		*pci_devices;
	int			pci_count = 0;
	device_t		*pci_children;
	int			pci_childcount = 0;
	device_t		*busp, *childp;
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
				free(pci_devices, M_TEMP);
				free(pci_children, M_TEMP);
				return(*childp);
			}
		}
	}

	free(pci_devices, M_TEMP);
	free(pci_children, M_TEMP);
	return(NULL);
}

static void sis_read_cmos(sc, dev, dest, off, cnt)
	struct sis_softc	*sc;
	device_t		dev;
	caddr_t			dest;
	int			off;
	int			cnt;
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
	btag = I386_BUS_SPACE_IO;

	for (i = 0; i < cnt; i++) {
		bus_space_write_1(btag, 0x0, 0x70, i + off);
		*(dest + i) = bus_space_read_1(btag, 0x0, 0x71);
	}

	pci_write_config(bridge, 0x48, reg & ~0x40, 1);
	return;
}
#endif

static int sis_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct sis_softc	*sc;
	int			i, val = 0;

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
		val = CSR_READ_4(sc, NS_BMCR + (reg * 4));
		return(val);
	}

	if (sc->sis_type == SIS_TYPE_900 && phy != 0)
		return(0);

	CSR_WRITE_4(sc, SIS_PHYCTL, (phy << 11) | (reg << 6) | SIS_PHYOP_READ);
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

static int sis_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct sis_softc	*sc;
	int			i;

	sc = device_get_softc(dev);

	if (sc->sis_type == SIS_TYPE_83815) {
		if (phy != 0)
			return(0);
		CSR_WRITE_4(sc, NS_BMCR + (reg * 4), data);
		return(0);
	}

	if (sc->sis_type == SIS_TYPE_900 && phy != 0)
		return(0);

	CSR_WRITE_4(sc, SIS_PHYCTL, (data << 16) | (phy << 11) |
	    (reg << 6) | SIS_PHYOP_WRITE);
	SIS_SETBIT(sc, SIS_PHYCTL, SIS_PHYCTL_ACCESS);

	for (i = 0; i < SIS_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, SIS_PHYCTL) & SIS_PHYCTL_ACCESS))
			break;
	}

	if (i == SIS_TIMEOUT)
		printf("sis%d: PHY failed to come ready\n", sc->sis_unit);

	return(0);
}

static void sis_miibus_statchg(dev)
	device_t		dev;
{
	struct sis_softc	*sc;

	sc = device_get_softc(dev);
	sis_init(sc);

	return;
}

static u_int32_t sis_crc(sc, addr)
	struct sis_softc	*sc;
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
	 *
	 * The NatSemi chip has a 512-bit filter, which is
	 * different than the SiS, so we special-case it.
	 */
	if (sc->sis_type == SIS_TYPE_83815)
		return((crc >> 23) & 0x1FF);

	return((crc >> 25) & 0x0000007F);
}

static void sis_setmulti_ns(sc)
	struct sis_softc	*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i, filtsave;
	int			bit, index;

	ifp = &sc->arpcom.ac_if;

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

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = sis_crc(sc, LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		index = h >> 3;
		bit = h & 0x1F;
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_FMEM_LO + index);
		if (bit > 0xF)
			bit -= 0x10;
		SIS_SETBIT(sc, SIS_RXFILT_DATA, (1 << bit));
	}

	CSR_WRITE_4(sc, SIS_RXFILT_CTL, filtsave);

	return;
}

static void sis_setmulti_sis(sc)
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
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = sis_crc(sc, LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
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
static int sis_probe(dev)
	device_t		dev;
{
	struct sis_type		*t;

	t = sis_devs;

	while(t->sis_name != NULL) {
		if ((pci_get_vendor(dev) == t->sis_vid) &&
		    (pci_get_device(dev) == t->sis_did)) {
			device_set_desc(dev, t->sis_name);
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
static int sis_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	bzero(sc, sizeof(struct sis_softc));

	mtx_init(&sc->sis_mtx, device_get_nameunit(dev), MTX_DEF | MTX_RECURSE);
	SIS_LOCK(sc);

	if (pci_get_device(dev) == SIS_DEVICEID_900)
		sc->sis_type = SIS_TYPE_900;
	if (pci_get_device(dev) == SIS_DEVICEID_7016)
		sc->sis_type = SIS_TYPE_7016;
	if (pci_get_vendor(dev) == NS_VENDORID)
		sc->sis_type = SIS_TYPE_83815;

	/*
	 * Handle power management nonsense.
	 */
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		u_int32_t		iobase, membase, irq;

		/* Save important PCI config data. */
		iobase = pci_read_config(dev, SIS_PCI_LOIO, 4);
		membase = pci_read_config(dev, SIS_PCI_LOMEM, 4);
		irq = pci_read_config(dev, SIS_PCI_INTLINE, 4);

		/* Reset the power state. */
		printf("sis%d: chip is in D%d power mode "
		    "-- setting to D0\n", unit,
		    pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		/* Restore PCI config data. */
		pci_write_config(dev, SIS_PCI_LOIO, iobase, 4);
		pci_write_config(dev, SIS_PCI_LOMEM, membase, 4);
		pci_write_config(dev, SIS_PCI_INTLINE, irq, 4);
	}

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, SYS_RES_IOPORT);
	pci_enable_io(dev, SYS_RES_MEMORY);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef SIS_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("sis%d: failed to enable I/O ports!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("sis%d: failed to enable memory mapping!\n", unit);
		error = ENXIO;;
		goto fail;
	}
#endif

	rid = SIS_RID;
	sc->sis_res = bus_alloc_resource(dev, SIS_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->sis_res == NULL) {
		printf("sis%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->sis_btag = rman_get_bustag(sc->sis_res);
	sc->sis_bhandle = rman_get_bushandle(sc->sis_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sis_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sis_irq == NULL) {
		printf("sis%d: couldn't map interrupt\n", unit);
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sis_irq, INTR_TYPE_NET,
	    sis_intr, sc, &sc->sis_intrhand);

	if (error) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);
		printf("sis%d: couldn't set up irq\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	sis_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	switch (pci_get_vendor(dev)) {
	case NS_VENDORID:
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
#ifdef __i386__
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
		command = pci_read_config(dev, PCIR_REVID, 1);
		if (command == SIS_REV_630S ||
		    command == SIS_REV_630E ||
		    command == SIS_REV_630EA1)
			sis_read_cmos(sc, dev, (caddr_t)&eaddr, 0x9, 6);
		else
#endif
			sis_read_eeprom(sc, (caddr_t)&eaddr,
			    SIS_EE_NODEADDR, 3, 0);
		break;
	}

	/*
	 * A SiS chip was detected. Inform the world.
	 */
	printf("sis%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->sis_unit = unit;
	callout_handle_init(&sc->sis_stat_ch);
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

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
			&sc->sis_parent_tag);

	/*
	 * Now allocate a tag for the DMA descriptor lists.
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
			&sc->sis_ldata.sis_rx_tag);

	error = bus_dma_tag_create(sc->sis_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			SIS_TX_LIST_SZ, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			&sc->sis_ldata.sis_tx_tag);

	error = bus_dma_tag_create(sc->sis_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			SIS_TX_LIST_SZ, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			&sc->sis_tag);

	/*
	 * Now allocate a chunk of DMA-able memory based on the
	 * tag we just created.
	 */
	error = bus_dmamem_alloc(sc->sis_ldata.sis_tx_tag,
	    (void **)&sc->sis_ldata.sis_tx_list, BUS_DMA_NOWAIT,
	    &sc->sis_ldata.sis_tx_dmamap);

	if (error) {
		printf("sis%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->sis_irq, sc->sis_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);
		bus_dma_tag_destroy(sc->sis_ldata.sis_rx_tag);
		bus_dma_tag_destroy(sc->sis_ldata.sis_tx_tag);
		error = ENXIO;
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sis_ldata.sis_rx_tag,
	    (void **)&sc->sis_ldata.sis_rx_list, BUS_DMA_NOWAIT,
	    &sc->sis_ldata.sis_rx_dmamap);

	if (error) {
		printf("sis%d: no memory for list buffers!\n", unit);
		bus_teardown_intr(dev, sc->sis_irq, sc->sis_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);
		bus_dmamem_free(sc->sis_ldata.sis_rx_tag,
		    sc->sis_ldata.sis_tx_list, sc->sis_ldata.sis_tx_dmamap);
		bus_dma_tag_destroy(sc->sis_ldata.sis_rx_tag);
		bus_dma_tag_destroy(sc->sis_ldata.sis_tx_tag);
		error = ENXIO;
		goto fail;
	}


	bzero(sc->sis_ldata.sis_tx_list, SIS_TX_LIST_SZ);
	bzero(sc->sis_ldata.sis_rx_list, SIS_RX_LIST_SZ);

	/*
	 * Obtain the physical addresses of the RX and TX
	 * rings which we'll need later in the init routine.
	 */
	bus_dmamap_load(sc->sis_ldata.sis_tx_tag,
	    sc->sis_ldata.sis_tx_dmamap, &(sc->sis_ldata.sis_tx_list[0]),
	    sizeof(struct sis_desc), sis_dma_map_ring,
	    &sc->sis_cdata.sis_tx_paddr, 0);
	bus_dmamap_load(sc->sis_ldata.sis_rx_tag,
	    sc->sis_ldata.sis_rx_dmamap, &(sc->sis_ldata.sis_rx_list[0]),
	    sizeof(struct sis_desc), sis_dma_map_ring,
	    &sc->sis_cdata.sis_rx_paddr, 0);

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

	/*
	 * Do MII setup.
	 */
	if (mii_phy_probe(dev, &sc->sis_miibus,
	    sis_ifmedia_upd, sis_ifmedia_sts)) {
		printf("sis%d: MII without any PHY!\n", sc->sis_unit);
		bus_teardown_intr(dev, sc->sis_irq, sc->sis_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
		bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);
		bus_dmamem_free(sc->sis_ldata.sis_rx_tag,
		    sc->sis_ldata.sis_rx_list, sc->sis_ldata.sis_rx_dmamap);
		bus_dmamem_free(sc->sis_ldata.sis_rx_tag,
		    sc->sis_ldata.sis_tx_list, sc->sis_ldata.sis_tx_dmamap);
		bus_dma_tag_destroy(sc->sis_ldata.sis_rx_tag);
		bus_dma_tag_destroy(sc->sis_ldata.sis_tx_tag);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	
	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	callout_handle_init(&sc->sis_stat_ch);
	SIS_UNLOCK(sc);
	return(0);

fail:
	SIS_UNLOCK(sc);
	mtx_destroy(&sc->sis_mtx);
	return(error);
}

static int sis_detach(dev)
	device_t		dev;
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;


	sc = device_get_softc(dev);
	SIS_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	sis_reset(sc);
	sis_stop(sc);
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);

	bus_generic_detach(dev);
	device_delete_child(dev, sc->sis_miibus);

	bus_teardown_intr(dev, sc->sis_irq, sc->sis_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sis_irq);
	bus_release_resource(dev, SIS_RES, SIS_RID, sc->sis_res);

	bus_dmamap_unload(sc->sis_ldata.sis_rx_tag,
	    sc->sis_ldata.sis_rx_dmamap);
	bus_dmamap_unload(sc->sis_ldata.sis_tx_tag,
	    sc->sis_ldata.sis_tx_dmamap);
	bus_dmamem_free(sc->sis_ldata.sis_rx_tag,
	    sc->sis_ldata.sis_rx_list, sc->sis_ldata.sis_rx_dmamap);
	bus_dmamem_free(sc->sis_ldata.sis_rx_tag,
	    sc->sis_ldata.sis_tx_list, sc->sis_ldata.sis_tx_dmamap);
	bus_dma_tag_destroy(sc->sis_ldata.sis_rx_tag);
	bus_dma_tag_destroy(sc->sis_ldata.sis_tx_tag);
	bus_dma_tag_destroy(sc->sis_parent_tag);

	SIS_UNLOCK(sc);
	mtx_destroy(&sc->sis_mtx);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int sis_list_tx_init(sc)
	struct sis_softc	*sc;
{
	struct sis_list_data	*ld;
	struct sis_ring_data	*cd;
	int			i, nexti;

	cd = &sc->sis_cdata;
	ld = &sc->sis_ldata;

	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		nexti = (i == (SIS_TX_LIST_CNT - 1)) ? 0 : i+1;
			ld->sis_tx_list[i].sis_nextdesc =
			    &ld->sis_tx_list[nexti];
			bus_dmamap_load(sc->sis_ldata.sis_tx_tag,
			    sc->sis_ldata.sis_tx_dmamap,
			    &ld->sis_tx_list[nexti], sizeof(struct sis_desc),
			    sis_dma_map_desc_next, &ld->sis_tx_list[i], 0);
		ld->sis_tx_list[i].sis_mbuf = NULL;
		ld->sis_tx_list[i].sis_ptr = 0;
		ld->sis_tx_list[i].sis_ctl = 0;
	}

	cd->sis_tx_prod = cd->sis_tx_cons = cd->sis_tx_cnt = 0;

	bus_dmamap_sync(sc->sis_ldata.sis_tx_tag,
	    sc->sis_ldata.sis_rx_dmamap, BUS_DMASYNC_PREWRITE);

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
	int			i,nexti;

	ld = &sc->sis_ldata;
	cd = &sc->sis_cdata;

	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sis_newbuf(sc, &ld->sis_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		nexti = (i == (SIS_RX_LIST_CNT - 1)) ? 0 : i+1;
			ld->sis_rx_list[i].sis_nextdesc =
			    &ld->sis_rx_list[nexti];
			bus_dmamap_load(sc->sis_ldata.sis_rx_tag,
			    sc->sis_ldata.sis_rx_dmamap,
			    &ld->sis_rx_list[nexti],
			    sizeof(struct sis_desc), sis_dma_map_desc_next,
			    &ld->sis_rx_list[i], 0);
		}

	bus_dmamap_sync(sc->sis_ldata.sis_rx_tag,
	    sc->sis_ldata.sis_rx_dmamap, BUS_DMASYNC_PREWRITE);

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

	if (c == NULL)
		return(EINVAL);

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
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
	c->sis_ctl = SIS_RXLEN;

	bus_dmamap_create(sc->sis_tag, 0, &c->sis_map);
	bus_dmamap_load(sc->sis_tag, c->sis_map,
	    mtod(m_new, void *), m_new->m_len,
	    sis_dma_map_desc_ptr, c, 0);
	bus_dmamap_sync(sc->sis_tag, c->sis_map, BUS_DMASYNC_PREWRITE);

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

	while(SIS_OWNDESC(&sc->sis_ldata.sis_rx_list[i])) {

#ifdef DEVICE_POLLING
		if (ifp->if_ipending & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		cur_rx = &sc->sis_ldata.sis_rx_list[i];
		rxstat = cur_rx->sis_rxstat;
		bus_dmamap_sync(sc->sis_tag,
		    cur_rx->sis_map, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sis_tag, cur_rx->sis_map);
		bus_dmamap_destroy(sc->sis_tag, cur_rx->sis_map);
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
#ifdef __i386__
		/*
		 * On the x86 we do not have alignment problems, so try to
		 * allocate a new buffer for the receive ring, and pass up
		 * the one where the packet is already, saving the expensive
		 * copy done in m_devget().
		 * If we are on an architecture with alignment problems, or
		 * if the allocation fails, then use m_devget and leave the
		 * existing buffer in the receive ring.
		 */
		if (sis_quick && sis_newbuf(sc, cur_rx, NULL) == 0) {
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		} else
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
		eh = mtod(m, struct ether_header *);

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
		cur_tx = &sc->sis_ldata.sis_tx_list[idx];

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
			bus_dmamap_unload(sc->sis_tag, cur_tx->sis_map);
			bus_dmamap_destroy(sc->sis_tag, cur_tx->sis_map);
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

static void sis_tick(xsc)
	void			*xsc;
{
	struct sis_softc	*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;

	sc = xsc;
	SIS_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	mii = device_get_softc(sc->sis_miibus);
	mii_tick(mii);

	if (!sc->sis_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sis_link++;
		if (ifp->if_snd.ifq_head != NULL)
			sis_start(ifp);
	}

	sc->sis_stat_ch = timeout(sis_tick, sc, hz);

	SIS_UNLOCK(sc);

	return;
}

#ifdef DEVICE_POLLING
static poll_handler_t sis_poll;

static void
sis_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct  sis_softc *sc = ifp->if_softc;
	SIS_LOCK(sc);
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
	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

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
			sis_init(sc);
		}
	}
done:
	SIS_UNLOCK(sc);
	return;
}
#endif /* DEVICE_POLLING */

static void sis_intr(arg)
	void			*arg;
{
	struct sis_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	SIS_LOCK(sc);
#ifdef DEVICE_POLLING
	if (ifp->if_ipending & IFF_POLLING)
		goto done;
	if (ether_poll_register(sis_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_4(sc, SIS_IER, 0);
		goto done;
	}
#endif /* DEVICE_POLLING */

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		sis_stop(sc);
		goto done;
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 0);

	for (;;) {
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
			sis_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, SIS_IER, 1);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);
done:
	SIS_UNLOCK(sc);

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
			f = &sc->sis_ldata.sis_tx_list[frag];
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

	sc->sis_ldata.sis_tx_list[cur].sis_mbuf = m_head;
	sc->sis_ldata.sis_tx_list[cur].sis_ctl &= ~SIS_CMDSTS_MORE;
	sc->sis_ldata.sis_tx_list[*txidx].sis_ctl |= SIS_CMDSTS_OWN;
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
	SIS_LOCK(sc);

	if (!sc->sis_link) {
		SIS_UNLOCK(sc);
		return;
	}

	idx = sc->sis_cdata.sis_tx_prod;

	if (ifp->if_flags & IFF_OACTIVE) {
		SIS_UNLOCK(sc);
		return;
	}

	while(sc->sis_ldata.sis_tx_list[idx].sis_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (sis_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);

	}

	/* Transmit */
	sc->sis_cdata.sis_tx_prod = idx;
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	SIS_UNLOCK(sc);

	return;
}

static void sis_init(xsc)
	void			*xsc;
{
	struct sis_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;

	SIS_LOCK(sc);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	sis_stop(sc);

	mii = device_get_softc(sc->sis_miibus);

	/* Set MAC address */
	if (sc->sis_type == SIS_TYPE_83815) {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR0);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[0]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR1);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[1]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, NS_FILTADDR_PAR2);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[2]);
	} else {
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR0);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[0]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR1);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[1]);
		CSR_WRITE_4(sc, SIS_RXFILT_CTL, SIS_FILTADDR_PAR2);
		CSR_WRITE_4(sc, SIS_RXFILT_DATA,
		    ((u_int16_t *)sc->arpcom.ac_enaddr)[2]);
	}

	/* Init circular RX list. */
	if (sis_list_rx_init(sc) == ENOBUFS) {
		printf("sis%d: initialization failed: no "
			"memory for rx buffers\n", sc->sis_unit);
		sis_stop(sc);
		SIS_UNLOCK(sc);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	sis_list_tx_init(sc);

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
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, sc->sis_cdata.sis_rx_paddr);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, sc->sis_cdata.sis_tx_paddr);

	/* Set RX configuration */
	CSR_WRITE_4(sc, SIS_RX_CFG, SIS_RXCFG);

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

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, SIS_IMR, SIS_INTRS);
#ifdef DEVICE_POLLING
	/*
	 * ... only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_ipending & IFF_POLLING)
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

	/*
	 * Page 75 of the DP83815 manual recommends the
	 * following register settings "for optimum
	 * performance." Note however that at least three
	 * of the registers are listed as "reserved" in
	 * the register map, so who knows what they do.
	 */
	if (sc->sis_type == SIS_TYPE_83815) {
		CSR_WRITE_4(sc, NS_PHY_PAGE, 0x0001);
		CSR_WRITE_4(sc, NS_PHY_CR, 0x189C);
		CSR_WRITE_4(sc, NS_PHY_TDATA, 0x0000);
		CSR_WRITE_4(sc, NS_PHY_DSPCFG, 0x5040);
		CSR_WRITE_4(sc, NS_PHY_SDCFG, 0x008C);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->sis_stat_ch = timeout(sis_tick, sc, hz);

	SIS_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int sis_ifmedia_upd(ifp)
	struct ifnet		*ifp;
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
static void sis_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sis_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->sis_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int sis_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

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
	default:
		error = EINVAL;
		break;
	}

	return(error);
}

static void sis_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sis_softc	*sc;

	sc = ifp->if_softc;

	SIS_LOCK(sc);

	ifp->if_oerrors++;
	printf("sis%d: watchdog timeout\n", sc->sis_unit);

	sis_stop(sc);
	sis_reset(sc);
	sis_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		sis_start(ifp);

	SIS_UNLOCK(sc);

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

	SIS_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(sis_tick, sc, sc->sis_stat_ch);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif
	CSR_WRITE_4(sc, SIS_IER, 0);
	CSR_WRITE_4(sc, SIS_IMR, 0);
	SIS_SETBIT(sc, SIS_CSR, SIS_CSR_TX_DISABLE|SIS_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, SIS_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, SIS_RX_LISTPTR, 0);

	sc->sis_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < SIS_RX_LIST_CNT; i++) {
		if (sc->sis_ldata.sis_rx_list[i].sis_mbuf != NULL) {
			bus_dmamap_unload(sc->sis_tag,
			    sc->sis_ldata.sis_rx_list[i].sis_map);
			bus_dmamap_destroy(sc->sis_tag,
			    sc->sis_ldata.sis_rx_list[i].sis_map);
			m_freem(sc->sis_ldata.sis_rx_list[i].sis_mbuf);
			sc->sis_ldata.sis_rx_list[i].sis_mbuf = NULL;
		}
	}
	bzero(sc->sis_ldata.sis_rx_list,
		sizeof(sc->sis_ldata.sis_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < SIS_TX_LIST_CNT; i++) {
		if (sc->sis_ldata.sis_tx_list[i].sis_mbuf != NULL) {
			bus_dmamap_unload(sc->sis_tag,
			    sc->sis_ldata.sis_tx_list[i].sis_map);
			bus_dmamap_destroy(sc->sis_tag,
			    sc->sis_ldata.sis_tx_list[i].sis_map);
			m_freem(sc->sis_ldata.sis_tx_list[i].sis_mbuf);
			sc->sis_ldata.sis_tx_list[i].sis_mbuf = NULL;
		}
	}

	bzero(sc->sis_ldata.sis_tx_list,
		sizeof(sc->sis_ldata.sis_tx_list));

	SIS_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void sis_shutdown(dev)
	device_t		dev;
{
	struct sis_softc	*sc;

	sc = device_get_softc(dev);
	SIS_LOCK(sc);
	sis_reset(sc);
	sis_stop(sc);
	SIS_UNLOCK(sc);

	return;
}
