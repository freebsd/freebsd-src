/*-
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 * RealTek 8139C+/8169/8169S/8110S PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * This driver is designed to support RealTek's next generation of
 * 10/100 and 10/100/1000 PCI ethernet controllers. There are currently
 * four devices in this family: the RTL8139C+, the RTL8169, the RTL8169S
 * and the RTL8110S.
 *
 * The 8139C+ is a 10/100 ethernet chip. It is backwards compatible
 * with the older 8139 family, however it also supports a special
 * C+ mode of operation that provides several new performance enhancing
 * features. These include:
 *
 *	o Descriptor based DMA mechanism. Each descriptor represents
 *	  a single packet fragment. Data buffers may be aligned on
 *	  any byte boundary.
 *
 *	o 64-bit DMA
 *
 *	o TCP/IP checksum offload for both RX and TX
 *
 *	o High and normal priority transmit DMA rings
 *
 *	o VLAN tag insertion and extraction
 *
 *	o TCP large send (segmentation offload)
 *
 * Like the 8139, the 8139C+ also has a built-in 10/100 PHY. The C+
 * programming API is fairly straightforward. The RX filtering, EEPROM
 * access and PHY access is the same as it is on the older 8139 series
 * chips.
 *
 * The 8169 is a 64-bit 10/100/1000 gigabit ethernet MAC. It has almost the
 * same programming API and feature set as the 8139C+ with the following
 * differences and additions:
 *
 *	o 1000Mbps mode
 *
 *	o Jumbo frames
 *
 *	o GMII and TBI ports/registers for interfacing with copper
 *	  or fiber PHYs
 *
 *	o RX and TX DMA rings can have up to 1024 descriptors
 *	  (the 8139C+ allows a maximum of 64)
 *
 *	o Slight differences in register layout from the 8139C+
 *
 * The TX start and timer interrupt registers are at different locations
 * on the 8169 than they are on the 8139C+. Also, the status word in the
 * RX descriptor has a slightly different bit layout. The 8169 does not
 * have a built-in PHY. Most reference boards use a Marvell 88E1000 'Alaska'
 * copper gigE PHY.
 *
 * The 8169S/8110S 10/100/1000 devices have built-in copper gigE PHYs
 * (the 'S' stands for 'single-chip'). These devices have the same
 * programming API as the older 8169, but also have some vendor-specific
 * registers for the on-board PHY. The 8110S is a LAN-on-motherboard
 * part designed to be pin-compatible with the RealTek 8100 10/100 chip.
 *
 * This driver takes advantage of the RX and TX checksum offload and
 * VLAN tag insertion/extraction features. It also implements TX
 * interrupt moderation using the timer interrupt registers, which
 * significantly reduces TX interrupt load. There is also support
 * for jumbo frames, however the 8169/8169S/8110S can not transmit
 * jumbo frames larger than 7440, so the max MTU possible with this
 * driver is 7422 bytes.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>

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

MODULE_DEPEND(re, pci, 1, 1, 1);
MODULE_DEPEND(re, ether, 1, 1, 1);
MODULE_DEPEND(re, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Default to using PIO access for this driver.
 */
#define RE_USEIOSPACE

#include <pci/if_rlreg.h>

#define RE_CSUM_FEATURES    (CSUM_IP | CSUM_TCP | CSUM_UDP)

/*
 * Various supported device vendors/types and their names.
 */
static struct rl_type re_devs[] = {
	{ DLINK_VENDORID, DLINK_DEVICEID_528T, RL_HWREV_8169S,
		"D-Link DGE-528(T) Gigabit Ethernet Adapter" },
	{ RT_VENDORID, RT_DEVICEID_8139, RL_HWREV_8139CPLUS,
		"RealTek 8139C+ 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169,
		"RealTek 8169 Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169S,
		"RealTek 8169S Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169SB,
		"RealTek 8169SB Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8110S,
		"RealTek 8110S Single-chip Gigabit Ethernet" },
	{ COREGA_VENDORID, COREGA_DEVICEID_CGLAPCIGT, RL_HWREV_8169S,
		"Corega CG-LAPCIGT (RTL8169S) Gigabit Ethernet" },
	{ 0, 0, 0, NULL }
};

static struct rl_hwrev re_hwrevs[] = {
	{ RL_HWREV_8139, RL_8139,  "" },
	{ RL_HWREV_8139A, RL_8139, "A" },
	{ RL_HWREV_8139AG, RL_8139, "A-G" },
	{ RL_HWREV_8139B, RL_8139, "B" },
	{ RL_HWREV_8130, RL_8139, "8130" },
	{ RL_HWREV_8139C, RL_8139, "C" },
	{ RL_HWREV_8139D, RL_8139, "8139D/8100B/8100C" },
	{ RL_HWREV_8139CPLUS, RL_8139CPLUS, "C+"},
	{ RL_HWREV_8169, RL_8169, "8169"},
	{ RL_HWREV_8169S, RL_8169, "8169S"},
	{ RL_HWREV_8169SB, RL_8169, "8169SB"},
	{ RL_HWREV_8110S, RL_8169, "8110S"},
	{ RL_HWREV_8100, RL_8139, "8100"},
	{ RL_HWREV_8101, RL_8139, "8101"},
	{ 0, 0, NULL }
};

static int re_probe		(device_t);
static int re_attach		(device_t);
static int re_detach		(device_t);

static int re_encap		(struct rl_softc *, struct mbuf **, int *);

static void re_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static void re_dma_map_desc	(void *, bus_dma_segment_t *, int,
				    bus_size_t, int);
static int re_allocmem		(device_t, struct rl_softc *);
static int re_newbuf		(struct rl_softc *, int, struct mbuf *);
static int re_rx_list_init	(struct rl_softc *);
static int re_tx_list_init	(struct rl_softc *);
#ifdef RE_FIXUP_RX
static __inline void re_fixup_rx
				(struct mbuf *);
#endif
static void re_rxeof		(struct rl_softc *);
static void re_txeof		(struct rl_softc *);
#ifdef DEVICE_POLLING
static void re_poll		(struct ifnet *, enum poll_cmd, int);
static void re_poll_locked	(struct ifnet *, enum poll_cmd, int);
#endif
static void re_intr		(void *);
static void re_tick		(void *);
static void re_tick_locked	(struct rl_softc *);
static void re_start		(struct ifnet *);
static void re_start_locked	(struct ifnet *);
static int re_ioctl		(struct ifnet *, u_long, caddr_t);
static void re_init		(void *);
static void re_init_locked	(struct rl_softc *);
static void re_stop		(struct rl_softc *);
static void re_watchdog		(struct ifnet *);
static int re_suspend		(device_t);
static int re_resume		(device_t);
static void re_shutdown		(device_t);
static int re_ifmedia_upd	(struct ifnet *);
static void re_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void re_eeprom_putbyte	(struct rl_softc *, int);
static void re_eeprom_getword	(struct rl_softc *, int, u_int16_t *);
static void re_read_eeprom	(struct rl_softc *, caddr_t, int, int, int);
static int re_gmii_readreg	(device_t, int, int);
static int re_gmii_writereg	(device_t, int, int, int);

static int re_miibus_readreg	(device_t, int, int);
static int re_miibus_writereg	(device_t, int, int, int);
static void re_miibus_statchg	(device_t);

static void re_setmulti		(struct rl_softc *);
static void re_reset		(struct rl_softc *);

static int re_diag		(struct rl_softc *);

#ifdef RE_USEIOSPACE
#define RL_RES			SYS_RES_IOPORT
#define RL_RID			RL_PCI_LOIO
#else
#define RL_RES			SYS_RES_MEMORY
#define RL_RID			RL_PCI_LOMEM
#endif

static device_method_t re_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		re_probe),
	DEVMETHOD(device_attach,	re_attach),
	DEVMETHOD(device_detach,	re_detach),
	DEVMETHOD(device_suspend,	re_suspend),
	DEVMETHOD(device_resume,	re_resume),
	DEVMETHOD(device_shutdown,	re_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	re_miibus_readreg),
	DEVMETHOD(miibus_writereg,	re_miibus_writereg),
	DEVMETHOD(miibus_statchg,	re_miibus_statchg),

	{ 0, 0 }
};

static driver_t re_driver = {
	"re",
	re_methods,
	sizeof(struct rl_softc)
};

static devclass_t re_devclass;

DRIVER_MODULE(re, pci, re_driver, re_devclass, 0, 0);
DRIVER_MODULE(re, cardbus, re_driver, re_devclass, 0, 0);
DRIVER_MODULE(miibus, re, miibus_driver, miibus_devclass, 0, 0);

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
re_eeprom_putbyte(sc, addr)
	struct rl_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | sc->rl_eecmd_read;

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
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
re_eeprom_getword(sc, addr, dest)
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
	re_eeprom_putbyte(sc, addr);

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
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
re_read_eeprom(sc, dest, off, cnt, swap)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		re_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}
}

static int
re_gmii_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	u_int32_t		rval;
	int			i;

	if (phy != 1)
		return (0);

	sc = device_get_softc(dev);

	/* Let the rgephy driver read the GMEDIASTAT register */

	if (reg == RL_GMEDIASTAT) {
		rval = CSR_READ_1(sc, RL_GMEDIASTAT);
		return (rval);
	}

	CSR_WRITE_4(sc, RL_PHYAR, reg << 16);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (rval & RL_PHYAR_BUSY)
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("re%d: PHY read failed\n", sc->rl_unit);
		return (0);
	}

	return (rval & RL_PHYAR_PHYDATA);
}

static int
re_gmii_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	u_int32_t		rval;
	int			i;

	sc = device_get_softc(dev);

	CSR_WRITE_4(sc, RL_PHYAR, (reg << 16) |
	    (data & RL_PHYAR_PHYDATA) | RL_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (!(rval & RL_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("re%d: PHY write failed\n", sc->rl_unit);
		return (0);
	}

	return (0);
}

static int
re_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	u_int16_t		rval = 0;
	u_int16_t		re8139_reg = 0;

	sc = device_get_softc(dev);

	if (sc->rl_type == RL_8169) {
		rval = re_gmii_readreg(dev, phy, reg);
		return (rval);
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		return (0);
	}
	switch (reg) {
	case MII_BMCR:
		re8139_reg = RL_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RL_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RL_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RL_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RL_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		return (0);
	/*
	 * Allow the rlphy driver to read the media status
	 * register. If we have a link partner which does not
	 * support NWAY, this is the register which will tell
	 * us the results of parallel detection.
	 */
	case RL_MEDIASTAT:
		rval = CSR_READ_1(sc, RL_MEDIASTAT);
		return (rval);
	default:
		printf("re%d: bad phy register\n", sc->rl_unit);
		return (0);
	}
	rval = CSR_READ_2(sc, re8139_reg);
	return (rval);
}

static int
re_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	u_int16_t		re8139_reg = 0;
	int			rval = 0;

	sc = device_get_softc(dev);

	if (sc->rl_type == RL_8169) {
		rval = re_gmii_writereg(dev, phy, reg, data);
		return (rval);
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy)
		return (0);

	switch (reg) {
	case MII_BMCR:
		re8139_reg = RL_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RL_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RL_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RL_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RL_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		return (0);
		break;
	default:
		printf("re%d: bad phy register\n", sc->rl_unit);
		return (0);
	}
	CSR_WRITE_2(sc, re8139_reg, data);
	return (0);
}

static void
re_miibus_statchg(dev)
	device_t		dev;
{

}

/*
 * Program the 64-bit multicast hash filter.
 */
static void
re_setmulti(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	RL_LOCK_ASSERT(sc);

	ifp = sc->rl_ifp;

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
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
	}
	IF_ADDR_UNLOCK(ifp);

	if (mcnt)
		rxfilt |= RL_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RL_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
	CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, RL_MAR4, hashes[1]);
}

static void
re_reset(sc)
	struct rl_softc		*sc;
{
	register int		i;

	RL_LOCK_ASSERT(sc);

	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_RESET);

	for (i = 0; i < RL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RL_COMMAND) & RL_CMD_RESET))
			break;
	}
	if (i == RL_TIMEOUT)
		printf("re%d: reset never completed!\n", sc->rl_unit);

	CSR_WRITE_1(sc, 0x82, 1);
}

/*
 * The following routine is designed to test for a defect on some
 * 32-bit 8169 cards. Some of these NICs have the REQ64# and ACK64#
 * lines connected to the bus, however for a 32-bit only card, they
 * should be pulled high. The result of this defect is that the
 * NIC will not work right if you plug it into a 64-bit slot: DMA
 * operations will be done with 64-bit transfers, which will fail
 * because the 64-bit data lines aren't connected.
 *
 * There's no way to work around this (short of talking a soldering
 * iron to the board), however we can detect it. The method we use
 * here is to put the NIC into digital loopback mode, set the receiver
 * to promiscuous mode, and then try to send a frame. We then compare
 * the frame data we sent to what was received. If the data matches,
 * then the NIC is working correctly, otherwise we know the user has
 * a defective NIC which has been mistakenly plugged into a 64-bit PCI
 * slot. In the latter case, there's no way the NIC can work correctly,
 * so we print out a message on the console and abort the device attach.
 */

static int
re_diag(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp = sc->rl_ifp;
	struct mbuf		*m0;
	struct ether_header	*eh;
	struct rl_desc		*cur_rx;
	u_int16_t		status;
	u_int32_t		rxstat;
	int			total_len, i, error = 0;
	u_int8_t		dst[] = { 0x00, 'h', 'e', 'l', 'l', 'o' };
	u_int8_t		src[] = { 0x00, 'w', 'o', 'r', 'l', 'd' };

	/* Allocate a single mbuf */
	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return (ENOBUFS);

	RL_LOCK(sc);

	/*
	 * Initialize the NIC in test mode. This sets the chip up
	 * so that it can send and receive frames, but performs the
	 * following special functions:
	 * - Puts receiver in promiscuous mode
	 * - Enables digital loopback mode
	 * - Leaves interrupts turned off
	 */

	ifp->if_flags |= IFF_PROMISC;
	sc->rl_testmode = 1;
	re_init_locked(sc);
	re_stop(sc);
	DELAY(100000);
	re_init_locked(sc);

	/* Put some data in the mbuf */

	eh = mtod(m0, struct ether_header *);
	bcopy ((char *)&dst, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy ((char *)&src, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_IP);
	m0->m_pkthdr.len = m0->m_len = ETHER_MIN_LEN - ETHER_CRC_LEN;

	/*
	 * Queue the packet, start transmission.
	 * Note: IF_HANDOFF() ultimately calls re_start() for us.
	 */

	CSR_WRITE_2(sc, RL_ISR, 0xFFFF);
	RL_UNLOCK(sc);
	/* XXX: re_diag must not be called when in ALTQ mode */
	IF_HANDOFF(&ifp->if_snd, m0, ifp);
	RL_LOCK(sc);
	m0 = NULL;

	/* Wait for it to propagate through the chip */

	DELAY(100000);
	for (i = 0; i < RL_TIMEOUT; i++) {
		status = CSR_READ_2(sc, RL_ISR);
		if ((status & (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK)) ==
		    (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK))
			break;
		DELAY(10);
	}

	if (i == RL_TIMEOUT) {
		printf("re%d: diagnostic failed, failed to receive packet "
		    "in loopback mode\n", sc->rl_unit);
		error = EIO;
		goto done;
	}

	/*
	 * The packet should have been dumped into the first
	 * entry in the RX DMA ring. Grab it from there.
	 */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->rl_ldata.rl_mtag,
	    sc->rl_ldata.rl_rx_dmamap[0],
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->rl_ldata.rl_mtag,
	    sc->rl_ldata.rl_rx_dmamap[0]);

	m0 = sc->rl_ldata.rl_rx_mbuf[0];
	sc->rl_ldata.rl_rx_mbuf[0] = NULL;
	eh = mtod(m0, struct ether_header *);

	cur_rx = &sc->rl_ldata.rl_rx_list[0];
	total_len = RL_RXBYTES(cur_rx);
	rxstat = le32toh(cur_rx->rl_cmdstat);

	if (total_len != ETHER_MIN_LEN) {
		printf("re%d: diagnostic failed, received short packet\n",
		    sc->rl_unit);
		error = EIO;
		goto done;
	}

	/* Test that the received packet data matches what we sent. */

	if (bcmp((char *)&eh->ether_dhost, (char *)&dst, ETHER_ADDR_LEN) ||
	    bcmp((char *)&eh->ether_shost, (char *)&src, ETHER_ADDR_LEN) ||
	    ntohs(eh->ether_type) != ETHERTYPE_IP) {
		printf("re%d: WARNING, DMA FAILURE!\n", sc->rl_unit);
		printf("re%d: expected TX data: %6D/%6D/0x%x\n", sc->rl_unit,
		    dst, ":", src, ":", ETHERTYPE_IP);
		printf("re%d: received RX data: %6D/%6D/0x%x\n", sc->rl_unit,
		    eh->ether_dhost, ":",  eh->ether_shost, ":",
		    ntohs(eh->ether_type));
		printf("re%d: You may have a defective 32-bit NIC plugged "
		    "into a 64-bit PCI slot.\n", sc->rl_unit);
		printf("re%d: Please re-install the NIC in a 32-bit slot "
		    "for proper operation.\n", sc->rl_unit);
		printf("re%d: Read the re(4) man page for more details.\n",
		    sc->rl_unit);
		error = EIO;
	}

done:
	/* Turn interface off, release resources */

	sc->rl_testmode = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(sc);
	if (m0 != NULL)
		m_freem(m0);

	RL_UNLOCK(sc);

	return (error);
}

/*
 * Probe for a RealTek 8139C+/8169/8110 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
re_probe(dev)
	device_t		dev;
{
	struct rl_type		*t;
	struct rl_softc		*sc;
	int			rid;
	u_int32_t		hwrev;

	t = re_devs;
	sc = device_get_softc(dev);

	while (t->rl_name != NULL) {
		if ((pci_get_vendor(dev) == t->rl_vid) &&
		    (pci_get_device(dev) == t->rl_did)) {

			/*
			 * Temporarily map the I/O space
			 * so we can read the chip ID register.
			 */
			rid = RL_RID;
			sc->rl_res = bus_alloc_resource_any(dev, RL_RES, &rid,
			    RF_ACTIVE);
			if (sc->rl_res == NULL) {
				device_printf(dev,
				    "couldn't map ports/memory\n");
				return (ENXIO);
			}
			sc->rl_btag = rman_get_bustag(sc->rl_res);
			sc->rl_bhandle = rman_get_bushandle(sc->rl_res);
			hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
			bus_release_resource(dev, RL_RES,
			    RL_RID, sc->rl_res);
			if (t->rl_basetype == hwrev) {
				device_set_desc(dev, t->rl_name);
				return (BUS_PROBE_DEFAULT);
			}
		}
		t++;
	}

	return (ENXIO);
}

/*
 * This routine takes the segment list provided as the result of
 * a bus_dma_map_load() operation and assigns the addresses/lengths
 * to RealTek DMA descriptors. This can be called either by the RX
 * code or the TX code. In the RX case, we'll probably wind up mapping
 * at most one segment. For the TX case, there could be any number of
 * segments since TX packets may span multiple mbufs. In either case,
 * if the number of segments is larger than the rl_maxsegs limit
 * specified by the caller, we abort the mapping operation. Sadly,
 * whoever designed the buffer mapping API did not provide a way to
 * return an error from here, so we have to fake it a bit.
 */

static void
re_dma_map_desc(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;
{
	struct rl_dmaload_arg	*ctx;
	struct rl_desc		*d = NULL;
	int			i = 0, idx;

	if (error)
		return;

	ctx = arg;

	/* Signal error to caller if there's too many segments */
	if (nseg > ctx->rl_maxsegs) {
		ctx->rl_maxsegs = 0;
		return;
	}

	/*
	 * Map the segment array into descriptors. Note that we set the
	 * start-of-frame and end-of-frame markers for either TX or RX, but
	 * they really only have meaning in the TX case. (In the RX case,
	 * it's the chip that tells us where packets begin and end.)
	 * We also keep track of the end of the ring and set the
	 * end-of-ring bits as needed, and we set the ownership bits
	 * in all except the very first descriptor. (The caller will
	 * set this descriptor later when it start transmission or
	 * reception.)
	 */
	idx = ctx->rl_idx;
	for (;;) {
		u_int32_t		cmdstat;
		d = &ctx->rl_ring[idx];
		if (le32toh(d->rl_cmdstat) & RL_RDESC_STAT_OWN) {
			ctx->rl_maxsegs = 0;
			return;
		}
		cmdstat = segs[i].ds_len;
		d->rl_bufaddr_lo = htole32(RL_ADDR_LO(segs[i].ds_addr));
		d->rl_bufaddr_hi = htole32(RL_ADDR_HI(segs[i].ds_addr));
		if (i == 0)
			cmdstat |= RL_TDESC_CMD_SOF;
		else
			cmdstat |= RL_TDESC_CMD_OWN;
		if (idx == (RL_RX_DESC_CNT - 1))
			cmdstat |= RL_TDESC_CMD_EOR;
		d->rl_cmdstat = htole32(cmdstat | ctx->rl_flags);
		i++;
		if (i == nseg)
			break;
		RL_DESC_INC(idx);
	}

	d->rl_cmdstat |= htole32(RL_TDESC_CMD_EOF);
	ctx->rl_maxsegs = nseg;
	ctx->rl_idx = idx;
}

/*
 * Map a single buffer address.
 */

static void
re_dma_map_addr(arg, segs, nseg, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	int			error;
{
	bus_addr_t		*addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;
}

static int
re_allocmem(dev, sc)
	device_t		dev;
	struct rl_softc		*sc;
{
	int			error;
	int			nseg;
	int			i;

	/*
	 * Allocate map for RX mbufs.
	 */
	nseg = 32;
	error = bus_dma_tag_create(sc->rl_parent_tag, ETHER_ALIGN, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * nseg, nseg, MCLBYTES, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->rl_ldata.rl_mtag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Allocate map for TX descriptor list.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag, RL_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, RL_TX_LIST_SZ, 1, RL_TX_LIST_SZ, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->rl_ldata.rl_tx_list_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for the TX ring */

	error = bus_dmamem_alloc(sc->rl_ldata.rl_tx_list_tag,
	    (void **)&sc->rl_ldata.rl_tx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->rl_ldata.rl_tx_list_map);
	if (error)
		return (ENOMEM);

	/* Load the map for the TX ring. */

	error = bus_dmamap_load(sc->rl_ldata.rl_tx_list_tag,
	     sc->rl_ldata.rl_tx_list_map, sc->rl_ldata.rl_tx_list,
	     RL_TX_LIST_SZ, re_dma_map_addr,
	     &sc->rl_ldata.rl_tx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for TX buffers */

	for (i = 0; i < RL_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->rl_ldata.rl_mtag, 0,
			    &sc->rl_ldata.rl_tx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for TX\n");
			return (ENOMEM);
		}
	}

	/*
	 * Allocate map for RX descriptor list.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag, RL_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, RL_RX_LIST_SZ, 1, RL_RX_LIST_SZ, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->rl_ldata.rl_rx_list_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for the RX ring */

	error = bus_dmamem_alloc(sc->rl_ldata.rl_rx_list_tag,
	    (void **)&sc->rl_ldata.rl_rx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->rl_ldata.rl_rx_list_map);
	if (error)
		return (ENOMEM);

	/* Load the map for the RX ring. */

	error = bus_dmamap_load(sc->rl_ldata.rl_rx_list_tag,
	     sc->rl_ldata.rl_rx_list_map, sc->rl_ldata.rl_rx_list,
	     RL_RX_LIST_SZ, re_dma_map_addr,
	     &sc->rl_ldata.rl_rx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for RX buffers */

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->rl_ldata.rl_mtag, 0,
			    &sc->rl_ldata.rl_rx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			return (ENOMEM);
		}
	}

	return (0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
re_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		as[3];
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	struct rl_hwrev		*hw_rev;
	int			hwrev;
	u_int16_t		re_did = 0;
	int			unit, error = 0, rid, i;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->rl_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = RL_RID;
	sc->rl_res = bus_alloc_resource_any(dev, RL_RES, &rid,
	    RF_ACTIVE);

	if (sc->rl_res == NULL) {
		printf ("re%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->rl_btag = rman_get_bustag(sc->rl_res);
	sc->rl_bhandle = rman_get_bushandle(sc->rl_res);

	/* Allocate interrupt */
	rid = 0;
	sc->rl_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->rl_irq == NULL) {
		printf("re%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	RL_LOCK(sc);
	re_reset(sc);
	RL_UNLOCK(sc);

	hw_rev = re_hwrevs;
	hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
	while (hw_rev->rl_desc != NULL) {
		if (hw_rev->rl_rev == hwrev) {
			sc->rl_type = hw_rev->rl_type;
			break;
		}
		hw_rev++;
	}

	if (sc->rl_type == RL_8169) {

		/* Set RX length mask */

		sc->rl_rxlenmask = RL_RDESC_STAT_GFRAGLEN;

		/* Force station address autoload from the EEPROM */

		CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_AUTOLOAD);
		for (i = 0; i < RL_TIMEOUT; i++) {
			if (!(CSR_READ_1(sc, RL_EECMD) & RL_EEMODE_AUTOLOAD))
				break;
			DELAY(100);
		}
		if (i == RL_TIMEOUT)
			printf ("re%d: eeprom autoload timed out\n", unit);

			for (i = 0; i < ETHER_ADDR_LEN; i++)
				eaddr[i] = CSR_READ_1(sc, RL_IDR0 + i);
	} else {

		/* Set RX length mask */

		sc->rl_rxlenmask = RL_RDESC_STAT_FRAGLEN;

		sc->rl_eecmd_read = RL_EECMD_READ_6BIT;
		re_read_eeprom(sc, (caddr_t)&re_did, 0, 1, 0);
		if (re_did != 0x8129)
			sc->rl_eecmd_read = RL_EECMD_READ_8BIT;

		/*
		 * Get station address from the EEPROM.
		 */
		re_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3, 0);
		for (i = 0; i < 3; i++) {
			eaddr[(i * 2) + 0] = as[i] & 0xff;
			eaddr[(i * 2) + 1] = as[i] >> 8;
		}
	}

	sc->rl_unit = unit;

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define RL_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, RL_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,	/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->rl_parent_tag);
	if (error)
		goto fail;

	error = re_allocmem(dev, sc);

	if (error)
		goto fail;

	ifp = sc->rl_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("re%d: can not if_alloc()\n", sc->rl_unit);
		error = ENOSPC;
		goto fail;
	}

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->rl_miibus,
	    re_ifmedia_upd, re_ifmedia_sts)) {
		printf("re%d: MII without any phy!\n", sc->rl_unit);
		error = ENXIO;
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_start = re_start;
	ifp->if_hwassist = /*RE_CSUM_FEATURES*/0;
	ifp->if_capabilities |= IFCAP_HWCSUM|IFCAP_VLAN_HWTAGGING;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	ifp->if_watchdog = re_watchdog;
	ifp->if_init = re_init;
	if (sc->rl_type == RL_8169)
		ifp->if_baudrate = 1000000000;
	else
		ifp->if_baudrate = 100000000;
	IFQ_SET_MAXLEN(&ifp->if_snd,  RL_IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = RL_IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_capenable = ifp->if_capabilities & ~IFCAP_HWCSUM;

	callout_handle_init(&sc->rl_stat_ch);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Perform hardware diagnostic. */
	error = re_diag(sc);

	if (error) {
		printf("re%d: attach aborted due to hardware diag failure\n",
		    unit);
		ether_ifdetach(ifp);
		if_free(ifp);
		goto fail;
	}

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->rl_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    re_intr, sc, &sc->rl_intrhand);
	if (error) {
		printf("re%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		if_free(ifp);
	}

fail:
	if (error)
		re_detach(dev);

	return (error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
re_detach(dev)
	device_t		dev;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	int			i;
	int			attached;

	sc = device_get_softc(dev);
	ifp = sc->rl_ifp;
	KASSERT(mtx_initialized(&sc->rl_mtx), ("re mutex not initialized"));

	attached = device_is_attached(dev);
	/* These should only be active if attach succeeded */
	if (attached)
		ether_ifdetach(ifp);
	if (ifp == NULL)
		if_free(ifp);

	RL_LOCK(sc);
#if 0
	sc->suspended = 1;
#endif

	/* These should only be active if attach succeeded */
	if (attached) {
		re_stop(sc);
		/*
		 * Force off the IFF_UP flag here, in case someone
		 * still had a BPF descriptor attached to this
		 * interface. If they do, ether_ifdetach() will cause
		 * the BPF code to try and clear the promisc mode
		 * flag, which will bubble down to re_ioctl(),
		 * which will try to call re_init() again. This will
		 * turn the NIC back on and restart the MII ticker,
		 * which will panic the system when the kernel tries
		 * to invoke the re_tick() function that isn't there
		 * anymore.
		 */
		ifp->if_flags &= ~IFF_UP;
	}
	if (sc->rl_miibus)
		device_delete_child(dev, sc->rl_miibus);
	bus_generic_detach(dev);

	/*
	 * The rest is resource deallocation, so we should already be
	 * stopped here.
	 */
	RL_UNLOCK(sc);

	if (sc->rl_intrhand)
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
	if (sc->rl_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
	if (sc->rl_res)
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);


	/* Unload and free the RX DMA ring memory and map */

	if (sc->rl_ldata.rl_rx_list_tag) {
		bus_dmamap_unload(sc->rl_ldata.rl_rx_list_tag,
		    sc->rl_ldata.rl_rx_list_map);
		bus_dmamem_free(sc->rl_ldata.rl_rx_list_tag,
		    sc->rl_ldata.rl_rx_list,
		    sc->rl_ldata.rl_rx_list_map);
		bus_dma_tag_destroy(sc->rl_ldata.rl_rx_list_tag);
	}

	/* Unload and free the TX DMA ring memory and map */

	if (sc->rl_ldata.rl_tx_list_tag) {
		bus_dmamap_unload(sc->rl_ldata.rl_tx_list_tag,
		    sc->rl_ldata.rl_tx_list_map);
		bus_dmamem_free(sc->rl_ldata.rl_tx_list_tag,
		    sc->rl_ldata.rl_tx_list,
		    sc->rl_ldata.rl_tx_list_map);
		bus_dma_tag_destroy(sc->rl_ldata.rl_tx_list_tag);
	}

	/* Destroy all the RX and TX buffer maps */

	if (sc->rl_ldata.rl_mtag) {
		for (i = 0; i < RL_TX_DESC_CNT; i++)
			bus_dmamap_destroy(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_tx_dmamap[i]);
		for (i = 0; i < RL_RX_DESC_CNT; i++)
			bus_dmamap_destroy(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_rx_dmamap[i]);
		bus_dma_tag_destroy(sc->rl_ldata.rl_mtag);
	}

	/* Unload and free the stats buffer and map */

	if (sc->rl_ldata.rl_stag) {
		bus_dmamap_unload(sc->rl_ldata.rl_stag,
		    sc->rl_ldata.rl_rx_list_map);
		bus_dmamem_free(sc->rl_ldata.rl_stag,
		    sc->rl_ldata.rl_stats,
		    sc->rl_ldata.rl_smap);
		bus_dma_tag_destroy(sc->rl_ldata.rl_stag);
	}

	if (sc->rl_parent_tag)
		bus_dma_tag_destroy(sc->rl_parent_tag);

	mtx_destroy(&sc->rl_mtx);

	return (0);
}

static int
re_newbuf(sc, idx, m)
	struct rl_softc		*sc;
	int			idx;
	struct mbuf		*m;
{
	struct rl_dmaload_arg	arg;
	struct mbuf		*n = NULL;
	int			error;

	if (m == NULL) {
		n = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (n == NULL)
			return (ENOBUFS);
		m = n;
	} else
		m->m_data = m->m_ext.ext_buf;

	m->m_len = m->m_pkthdr.len = MCLBYTES;
#ifdef RE_FIXUP_RX
	/*
	 * This is part of an evil trick to deal with non-x86 platforms.
	 * The RealTek chip requires RX buffers to be aligned on 64-bit
	 * boundaries, but that will hose non-x86 machines. To get around
	 * this, we leave some empty space at the start of each buffer
	 * and for non-x86 hosts, we copy the buffer back six bytes
	 * to achieve word alignment. This is slightly more efficient
	 * than allocating a new buffer, copying the contents, and
	 * discarding the old buffer.
	 */
	m_adj(m, RE_ETHER_ALIGN);
#endif
	arg.sc = sc;
	arg.rl_idx = idx;
	arg.rl_maxsegs = 1;
	arg.rl_flags = 0;
	arg.rl_ring = sc->rl_ldata.rl_rx_list;

	error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag,
	    sc->rl_ldata.rl_rx_dmamap[idx], m, re_dma_map_desc,
	    &arg, BUS_DMA_NOWAIT);
	if (error || arg.rl_maxsegs != 1) {
		if (n != NULL)
			m_freem(n);
		return (ENOMEM);
	}

	sc->rl_ldata.rl_rx_list[idx].rl_cmdstat |= htole32(RL_RDESC_CMD_OWN);
	sc->rl_ldata.rl_rx_mbuf[idx] = m;

	bus_dmamap_sync(sc->rl_ldata.rl_mtag,
	    sc->rl_ldata.rl_rx_dmamap[idx],
	    BUS_DMASYNC_PREREAD);

	return (0);
}

#ifdef RE_FIXUP_RX
static __inline void
re_fixup_rx(m)
	struct mbuf		*m;
{
	int                     i;
	uint16_t                *src, *dst;

	src = mtod(m, uint16_t *);
	dst = src - (RE_ETHER_ALIGN - ETHER_ALIGN) / sizeof *src;

	for (i = 0; i < (m->m_len / sizeof(uint16_t) + 1); i++)
		*dst++ = *src++;

	m->m_data -= RE_ETHER_ALIGN - ETHER_ALIGN;

	return;
}
#endif

static int
re_tx_list_init(sc)
	struct rl_softc		*sc;
{

	RL_LOCK_ASSERT(sc);

	bzero ((char *)sc->rl_ldata.rl_tx_list, RL_TX_LIST_SZ);
	bzero ((char *)&sc->rl_ldata.rl_tx_mbuf,
	    (RL_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
	    sc->rl_ldata.rl_tx_list_map, BUS_DMASYNC_PREWRITE);
	sc->rl_ldata.rl_tx_prodidx = 0;
	sc->rl_ldata.rl_tx_considx = 0;
	sc->rl_ldata.rl_tx_free = RL_TX_DESC_CNT;

	return (0);
}

static int
re_rx_list_init(sc)
	struct rl_softc		*sc;
{
	int			i;

	bzero ((char *)sc->rl_ldata.rl_rx_list, RL_RX_LIST_SZ);
	bzero ((char *)&sc->rl_ldata.rl_rx_mbuf,
	    (RL_RX_DESC_CNT * sizeof(struct mbuf *)));

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (re_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = 0;
	sc->rl_head = sc->rl_tail = NULL;

	return (0);
}

/*
 * RX handler for C+ and 8169. For the gigE chips, we support
 * the reception of jumbo frames that have been fragmented
 * across multiple 2K mbuf cluster buffers.
 */
static void
re_rxeof(sc)
	struct rl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	struct rl_desc		*cur_rx;
	u_int32_t		rxstat, rxvlan;

	RL_LOCK_ASSERT(sc);

	ifp = sc->rl_ifp;
	i = sc->rl_ldata.rl_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (!RL_OWN(&sc->rl_ldata.rl_rx_list[i])) {
		cur_rx = &sc->rl_ldata.rl_rx_list[i];
		m = sc->rl_ldata.rl_rx_mbuf[i];
		total_len = RL_RXBYTES(cur_rx);
		rxstat = le32toh(cur_rx->rl_cmdstat);
		rxvlan = le32toh(cur_rx->rl_vlanctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->rl_ldata.rl_mtag,
		    sc->rl_ldata.rl_rx_dmamap[i],
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->rl_ldata.rl_mtag,
		    sc->rl_ldata.rl_rx_dmamap[i]);

		if (!(rxstat & RL_RDESC_STAT_EOF)) {
			m->m_len = RE_RX_DESC_BUFLEN;
			if (sc->rl_head == NULL)
				sc->rl_head = sc->rl_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->rl_tail->m_next = m;
				sc->rl_tail = m;
			}
			re_newbuf(sc, i, NULL);
			RL_DESC_INC(i);
			continue;
		}

		/*
		 * NOTE: for the 8139C+, the frame length field
		 * is always 12 bits in size, but for the gigE chips,
		 * it is 13 bits (since the max RX frame length is 16K).
		 * Unfortunately, all 32 bits in the status word
		 * were already used, so to make room for the extra
		 * length bit, RealTek took out the 'frame alignment
		 * error' bit and shifted the other status bits
		 * over one slot. The OWN, EOR, FS and LS bits are
		 * still in the same places. We have already extracted
		 * the frame length and checked the OWN bit, so rather
		 * than using an alternate bit mapping, we shift the
		 * status bits one space to the right so we can evaluate
		 * them using the 8169 status as though it was in the
		 * same format as that of the 8139C+.
		 */
		if (sc->rl_type == RL_8169)
			rxstat >>= 1;

		/*
		 * if total_len > 2^13-1, both _RXERRSUM and _GIANT will be
		 * set, but if CRC is clear, it will still be a valid frame.
		 */
		if (rxstat & RL_RDESC_STAT_RXERRSUM && !(total_len > 8191 &&
		    (rxstat & RL_RDESC_STAT_ERRS) == RL_RDESC_STAT_GIANT)) {
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->rl_head != NULL) {
				m_freem(sc->rl_head);
				sc->rl_head = sc->rl_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (re_newbuf(sc, i, NULL)) {
			ifp->if_ierrors++;
			if (sc->rl_head != NULL) {
				m_freem(sc->rl_head);
				sc->rl_head = sc->rl_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		RL_DESC_INC(i);

		if (sc->rl_head != NULL) {
			m->m_len = total_len % RE_RX_DESC_BUFLEN;
			if (m->m_len == 0)
				m->m_len = RE_RX_DESC_BUFLEN;
			/*
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->rl_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->rl_tail->m_next = m;
			}
			m = sc->rl_head;
			sc->rl_head = sc->rl_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

#ifdef RE_FIXUP_RX
		re_fixup_rx(m);
#endif
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming if enabled */

		if (ifp->if_capenable & IFCAP_RXCSUM) {

			/* Check IP header checksum */
			if (rxstat & RL_RDESC_STAT_PROTOID)
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if (!(rxstat & RL_RDESC_STAT_IPSUMBAD))
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;

			/* Check TCP/UDP checksum */
			if ((RL_TCPPKT(rxstat) &&
			    !(rxstat & RL_RDESC_STAT_TCPSUMBAD)) ||
			    (RL_UDPPKT(rxstat) &&
			    !(rxstat & RL_RDESC_STAT_UDPSUMBAD))) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

		if (rxvlan & RL_RDESC_VLANCTL_TAG)
			VLAN_INPUT_TAG(ifp, m,
			    ntohs((rxvlan & RL_RDESC_VLANCTL_DATA)), continue);
		RL_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		RL_LOCK(sc);
	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = i;
}

static void
re_txeof(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = sc->rl_ifp;
	idx = sc->rl_ldata.rl_tx_considx;

	/* Invalidate the TX descriptor list */

	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
	    sc->rl_ldata.rl_tx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (idx != sc->rl_ldata.rl_tx_prodidx) {

		txstat = le32toh(sc->rl_ldata.rl_tx_list[idx].rl_cmdstat);
		if (txstat & RL_TDESC_CMD_OWN)
			break;

		/*
		 * We only stash mbufs in the last descriptor
		 * in a fragment chain, which also happens to
		 * be the only place where the TX status bits
		 * are valid.
		 */

		if (txstat & RL_TDESC_CMD_EOF) {
			m_freem(sc->rl_ldata.rl_tx_mbuf[idx]);
			sc->rl_ldata.rl_tx_mbuf[idx] = NULL;
			bus_dmamap_unload(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_tx_dmamap[idx]);
			if (txstat & (RL_TDESC_STAT_EXCESSCOL|
			    RL_TDESC_STAT_COLCNT))
				ifp->if_collisions++;
			if (txstat & RL_TDESC_STAT_TXERRSUM)
				ifp->if_oerrors++;
			else
				ifp->if_opackets++;
		}
		sc->rl_ldata.rl_tx_free++;
		RL_DESC_INC(idx);
	}

	/* No changes made to the TX ring, so no flush needed */

	if (idx != sc->rl_ldata.rl_tx_considx) {
		sc->rl_ldata.rl_tx_considx = idx;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->rl_ldata.rl_tx_free != RL_TX_DESC_CNT)
		CSR_WRITE_4(sc, RL_TIMERCNT, 1);
}

static void
re_tick(xsc)
	void			*xsc;
{
	struct rl_softc		*sc;

	sc = xsc;
	RL_LOCK(sc);
	re_tick_locked(sc);
	RL_UNLOCK(sc);
}

static void
re_tick_locked(sc)
	struct rl_softc		*sc;
{
	struct mii_data		*mii;

	RL_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->rl_miibus);

	mii_tick(mii);

	sc->rl_stat_ch = timeout(re_tick, sc, hz);
}

#ifdef DEVICE_POLLING
static void
re_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK(sc);
	re_poll_locked(ifp, cmd, count);
	RL_UNLOCK(sc);
}

static void
re_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK_ASSERT(sc);

	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
		return;
	}

	sc->rxcycles = count;
	re_rxeof(sc);
	re_txeof(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		re_start_locked(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int16_t       status;

		status = CSR_READ_2(sc, RL_ISR);
		if (status == 0xffff)
			return;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & RL_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init_locked(sc);
		}
	}
}
#endif /* DEVICE_POLLING */

static void
re_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;

	RL_LOCK(sc);

	ifp = sc->rl_ifp;

	if (sc->suspended || !(ifp->if_flags & IFF_UP))
		goto done_locked;

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done_locked;
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(re_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RL_IMR, 0x0000);
		re_poll_locked(ifp, 0, 1);
		goto done_locked;
	}
#endif /* DEVICE_POLLING */

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		if ((status & RL_INTRS_CPLUS) == 0)
			break;

		if ((status & RL_ISR_RX_OK) ||
		    (status & RL_ISR_RX_ERR))
			re_rxeof(sc);

		if ((status & RL_ISR_TIMEOUT_EXPIRED) ||
		    (status & RL_ISR_TX_ERR) ||
		    (status & RL_ISR_TX_DESC_UNAVAIL))
			re_txeof(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init_locked(sc);
		}

		if (status & RL_ISR_LINKCHG) {
			untimeout(re_tick, sc, sc->rl_stat_ch);
			re_tick_locked(sc);
		}
	}

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		re_start_locked(ifp);

done_locked:
	RL_UNLOCK(sc);
}

static int
re_encap(sc, m_head, idx)
	struct rl_softc		*sc;
	struct mbuf		**m_head;
	int			*idx;
{
	struct mbuf		*m_new = NULL;
	struct rl_dmaload_arg	arg;
	bus_dmamap_t		map;
	int			error;
	struct m_tag		*mtag;

	RL_LOCK_ASSERT(sc);

	if (sc->rl_ldata.rl_tx_free <= 4)
		return (EFBIG);

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. This is according to testing done with an 8169
	 * chip. This is a requirement.
	 */

	arg.rl_flags = 0;

	if ((*m_head)->m_pkthdr.csum_flags & CSUM_IP)
		arg.rl_flags |= RL_TDESC_CMD_IPCSUM;
	if ((*m_head)->m_pkthdr.csum_flags & CSUM_TCP)
		arg.rl_flags |= RL_TDESC_CMD_TCPCSUM;
	if ((*m_head)->m_pkthdr.csum_flags & CSUM_UDP)
		arg.rl_flags |= RL_TDESC_CMD_UDPCSUM;

	arg.sc = sc;
	arg.rl_idx = *idx;
	arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
	if (arg.rl_maxsegs > 4)
		arg.rl_maxsegs -= 4;
	arg.rl_ring = sc->rl_ldata.rl_tx_list;

	map = sc->rl_ldata.rl_tx_dmamap[*idx];
	error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
	    *m_head, re_dma_map_desc, &arg, BUS_DMA_NOWAIT);

	if (error && error != EFBIG) {
		printf("re%d: can't map mbuf (error %d)\n", sc->rl_unit, error);
		return (ENOBUFS);
	}

	/* Too many segments to map, coalesce into a single mbuf */

	if (error || arg.rl_maxsegs == 0) {
		m_new = m_defrag(*m_head, M_DONTWAIT);
		if (m_new == NULL)
			return (ENOBUFS);
		else
			*m_head = m_new;

		arg.sc = sc;
		arg.rl_idx = *idx;
		arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
		arg.rl_ring = sc->rl_ldata.rl_tx_list;

		error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
		    *m_head, re_dma_map_desc, &arg, BUS_DMA_NOWAIT);
		if (error) {
			printf("re%d: can't map mbuf (error %d)\n",
			    sc->rl_unit, error);
			return (EFBIG);
		}
	}

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.  (Swap last and first dmamaps.)
	 */
	sc->rl_ldata.rl_tx_dmamap[*idx] =
	    sc->rl_ldata.rl_tx_dmamap[arg.rl_idx];
	sc->rl_ldata.rl_tx_dmamap[arg.rl_idx] = map;

	sc->rl_ldata.rl_tx_mbuf[arg.rl_idx] = *m_head;
	sc->rl_ldata.rl_tx_free -= arg.rl_maxsegs;

	/*
	 * Set up hardware VLAN tagging. Note: vlan tag info must
	 * appear in the first descriptor of a multi-descriptor
	 * transmission attempt.
	 */

	mtag = VLAN_OUTPUT_TAG(sc->rl_ifp, *m_head);
	if (mtag != NULL)
		sc->rl_ldata.rl_tx_list[*idx].rl_vlanctl =
		    htole32(htons(VLAN_TAG_VALUE(mtag)) | RL_TDESC_VLANCTL_TAG);

	/* Transfer ownership of packet to the chip. */

	sc->rl_ldata.rl_tx_list[arg.rl_idx].rl_cmdstat |=
	    htole32(RL_TDESC_CMD_OWN);
	if (*idx != arg.rl_idx)
		sc->rl_ldata.rl_tx_list[*idx].rl_cmdstat |=
		    htole32(RL_TDESC_CMD_OWN);

	RL_DESC_INC(arg.rl_idx);
	*idx = arg.rl_idx;

	return (0);
}

static void
re_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;
	RL_LOCK(sc);
	re_start_locked(ifp);
	RL_UNLOCK(sc);
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */
static void
re_start_locked(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;
	int			idx, queued = 0;

	sc = ifp->if_softc;

	RL_LOCK_ASSERT(sc);

	idx = sc->rl_ldata.rl_tx_prodidx;

	while (sc->rl_ldata.rl_tx_mbuf[idx] == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (re_encap(sc, &m_head, &idx)) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);

		queued++;
	}

	if (queued == 0)
		return;

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
	    sc->rl_ldata.rl_tx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_tx_prodidx = idx;

	/*
	 * RealTek put the TX poll request register in a different
	 * location on the 8169 gigE chip. I don't know why.
	 */

	if (sc->rl_type == RL_8169)
		CSR_WRITE_2(sc, RL_GTXSTART, RL_TXSTART_START);
	else
		CSR_WRITE_2(sc, RL_TXSTART, RL_TXSTART_START);

	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the TIMERINT register, and then trigger an
	 * interrupt. Each time we write to the TIMERCNT register,
	 * the timer count is reset to 0.
	 */
	CSR_WRITE_4(sc, RL_TIMERCNT, 1);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

static void
re_init(xsc)
	void			*xsc;
{
	struct rl_softc		*sc = xsc;

	RL_LOCK(sc);
	re_init_locked(sc);
	RL_UNLOCK(sc);
}

static void
re_init_locked(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp = sc->rl_ifp;
	struct mii_data		*mii;
	u_int32_t		rxcfg = 0;

	RL_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->rl_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	re_stop(sc);

	/*
	 * Enable C+ RX and TX mode, as well as VLAN stripping and
	 * RX checksum offload. We must configure the C+ register
	 * before all others.
	 */
	CSR_WRITE_2(sc, RL_CPLUS_CMD, RL_CPLUSCMD_RXENB|
	    RL_CPLUSCMD_TXENB|RL_CPLUSCMD_PCI_MRW|
	    RL_CPLUSCMD_VLANSTRIP|
	    (ifp->if_capenable & IFCAP_RXCSUM ?
	    RL_CPLUSCMD_RXCSUM_ENB : 0));

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	CSR_WRITE_STREAM_4(sc, RL_IDR0,
	    *(u_int32_t *)(&IFP2ENADDR(sc->rl_ifp)[0]));
	CSR_WRITE_STREAM_4(sc, RL_IDR4,
	    *(u_int32_t *)(&IFP2ENADDR(sc->rl_ifp)[4]));
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	if (sc->rl_testmode) {
		if (sc->rl_type == RL_8169)
			CSR_WRITE_4(sc, RL_TXCFG,
			    RL_TXCFG_CONFIG|RL_LOOPTEST_ON);
		else
			CSR_WRITE_4(sc, RL_TXCFG,
			    RL_TXCFG_CONFIG|RL_LOOPTEST_ON_CPLUS);
	} else
		CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	rxcfg |= RL_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		rxcfg |= RL_RXCFG_RX_ALLPHYS;
	else
		rxcfg &= ~RL_RXCFG_RX_ALLPHYS;
	CSR_WRITE_4(sc, RL_RXCFG, rxcfg);

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		rxcfg |= RL_RXCFG_RX_BROAD;
	else
		rxcfg &= ~RL_RXCFG_RX_BROAD;
	CSR_WRITE_4(sc, RL_RXCFG, rxcfg);

	/*
	 * Program the multicast filter, if necessary.
	 */
	re_setmulti(sc);

#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else	/* otherwise ... */
#endif /* DEVICE_POLLING */
	/*
	 * Enable interrupts.
	 */
	if (sc->rl_testmode)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);

	/* Set initial TX threshold */
	sc->rl_txthresh = RL_TX_THRESH_INIT;

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);
#endif
	/*
	 * Load the addresses of the RX and TX lists into the chip.
	 */

	CSR_WRITE_4(sc, RL_RXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_rx_list_addr));
	CSR_WRITE_4(sc, RL_RXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_rx_list_addr));

	CSR_WRITE_4(sc, RL_TXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_tx_list_addr));
	CSR_WRITE_4(sc, RL_TXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_tx_list_addr));

	CSR_WRITE_1(sc, RL_EARLY_TX_THRESH, 16);

	/*
	 * Initialize the timer interrupt register so that
	 * a timer interrupt will be generated once the timer
	 * reaches a certain number of ticks. The timer is
	 * reloaded on each transmit. This gives us TX interrupt
	 * moderation, which dramatically improves TX frame rate.
	 */
	if (sc->rl_type == RL_8169)
		CSR_WRITE_4(sc, RL_TIMERINT_8169, 0x800);
	else
		CSR_WRITE_4(sc, RL_TIMERINT, 0x400);

	/*
	 * For 8169 gigE NICs, set the max allowed RX packet
	 * size so we can receive jumbo frames.
	 */
	if (sc->rl_type == RL_8169)
		CSR_WRITE_2(sc, RL_MAXRXPKTLEN, 16383);

	if (sc->rl_testmode)
		return;

	mii_mediachg(mii);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->rl_stat_ch = timeout(re_tick, sc, hz);
}

/*
 * Set media options.
 */
static int
re_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->rl_miibus);
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */
static void
re_ifmedia_sts(ifp, ifmr)
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
}

static int
re_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	switch (command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > RL_JUMBO_MTU)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		RL_LOCK(sc);
		if (ifp->if_flags & IFF_UP)
			re_init_locked(sc);
		else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			re_stop(sc);
		RL_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		RL_LOCK(sc);
		re_setmulti(sc);
		RL_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->rl_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
		ifp->if_capenable &= ~(IFCAP_HWCSUM | IFCAP_POLLING);
		ifp->if_capenable |=
		    ifr->ifr_reqcap & (IFCAP_HWCSUM | IFCAP_POLLING);
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = RE_CSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			re_init(sc);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
re_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;
	RL_LOCK(sc);
	printf("re%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;

	re_txeof(sc);
	re_rxeof(sc);
	re_init_locked(sc);

	RL_UNLOCK(sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
re_stop(sc)
	struct rl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	RL_LOCK_ASSERT(sc);

	ifp = sc->rl_ifp;
	ifp->if_timer = 0;

	untimeout(re_tick, sc, sc->rl_stat_ch);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	if (sc->rl_head != NULL) {
		m_freem(sc->rl_head);
		sc->rl_head = sc->rl_tail = NULL;
	}

	/* Free the TX list buffers. */

	for (i = 0; i < RL_TX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_tx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_tx_dmamap[i]);
			m_freem(sc->rl_ldata.rl_tx_mbuf[i]);
			sc->rl_ldata.rl_tx_mbuf[i] = NULL;
		}
	}

	/* Free the RX list buffers. */

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_rx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_rx_dmamap[i]);
			m_freem(sc->rl_ldata.rl_rx_mbuf[i]);
			sc->rl_ldata.rl_rx_mbuf[i] = NULL;
		}
	}
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
re_suspend(dev)
	device_t		dev;
{
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	RL_LOCK(sc);
	re_stop(sc);
	sc->suspended = 1;
	RL_UNLOCK(sc);

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
re_resume(dev)
	device_t		dev;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);

	RL_LOCK(sc);

	ifp = sc->rl_ifp;

	/* reinitialize interface if necessary */
	if (ifp->if_flags & IFF_UP)
		re_init_locked(sc);

	sc->suspended = 0;
	RL_UNLOCK(sc);

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
re_shutdown(dev)
	device_t		dev;
{
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	RL_LOCK(sc);
	re_stop(sc);
	/*
	 * Mark interface as down since otherwise we will panic if
	 * interrupt comes in later on, which can happen in some
	 * cases. Another option is to call re_detach() instead of
	 * re_stop(), like ve(4) does.
	 */
	sc->rl_ifp->if_flags &= ~IFF_UP;
	RL_UNLOCK(sc);
}
