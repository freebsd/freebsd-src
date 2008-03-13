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
 * RealTek 8139C+/8169/8169S/8110S/8168/8111/8101E PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * This driver is designed to support RealTek's next generation of
 * 10/100 and 10/100/1000 PCI ethernet controllers. There are currently
 * seven devices in this family: the RTL8139C+, the RTL8169, the RTL8169S,
 * RTL8110S, the RTL8168, the RTL8111 and the RTL8101E.
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

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>

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

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Default to using PIO access for this driver.
 */
#define RE_USEIOSPACE

#include <pci/if_rlreg.h>

/* Tunables. */
static int msi_disable = 0;
TUNABLE_INT("hw.re.msi_disable", &msi_disable);

#define RE_CSUM_FEATURES    (CSUM_IP | CSUM_TCP | CSUM_UDP)

/*
 * Various supported device vendors/types and their names.
 */
static struct rl_type re_devs[] = {
	{ DLINK_VENDORID, DLINK_DEVICEID_528T, RL_HWREV_8169S,
		"D-Link DGE-528(T) Gigabit Ethernet Adapter" },
	{ DLINK_VENDORID, DLINK_DEVICEID_528T, RL_HWREV_8169_8110SB,
		"D-Link DGE-528(T) Rev.B1 Gigabit Ethernet Adapter" },
	{ RT_VENDORID, RT_DEVICEID_8139, RL_HWREV_8139CPLUS,
		"RealTek 8139C+ 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8101E, RL_HWREV_8101E,
		"RealTek 8101E PCIe 10/100baseTX" },
	{ RT_VENDORID, RT_DEVICEID_8168, RL_HWREV_8168_SPIN1,
		"RealTek 8168/8111B PCIe Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8168, RL_HWREV_8168_SPIN2,
		"RealTek 8168/8111B PCIe Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8168, RL_HWREV_8168_SPIN3,
		"RealTek 8168/8111B PCIe Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169,
		"RealTek 8169 Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169S,
		"RealTek 8169S Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169_8110SB,
		"RealTek 8169SB/8110SB Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8169_8110SC,
		"RealTek 8169SC/8110SC Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169SC, RL_HWREV_8169_8110SC,
		"RealTek 8169SC/8110SC Single-chip Gigabit Ethernet" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_HWREV_8110S,
		"RealTek 8110S Single-chip Gigabit Ethernet" },
	{ COREGA_VENDORID, COREGA_DEVICEID_CGLAPCIGT, RL_HWREV_8169S,
		"Corega CG-LAPCIGT (RTL8169S) Gigabit Ethernet" },
	{ LINKSYS_VENDORID, LINKSYS_DEVICEID_EG1032, RL_HWREV_8169S,
		"Linksys EG1032 (RTL8169S) Gigabit Ethernet" },
	{ USR_VENDORID, USR_DEVICEID_997902, RL_HWREV_8169S,
		"US Robotics 997902 (RTL8169S) Gigabit Ethernet" },
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
	{ RL_HWREV_8168_SPIN1, RL_8169, "8168"},
	{ RL_HWREV_8169, RL_8169, "8169"},
	{ RL_HWREV_8169S, RL_8169, "8169S"},
	{ RL_HWREV_8110S, RL_8169, "8110S"},
	{ RL_HWREV_8169_8110SB, RL_8169, "8169SB"},
	{ RL_HWREV_8169_8110SC, RL_8169, "8169SC"},
	{ RL_HWREV_8100, RL_8139, "8100"},
	{ RL_HWREV_8101, RL_8139, "8101"},
	{ RL_HWREV_8100E, RL_8169, "8100E"},
	{ RL_HWREV_8101E, RL_8169, "8101E"},
	{ RL_HWREV_8168_SPIN2, RL_8169, "8168"},
	{ RL_HWREV_8168_SPIN3, RL_8169, "8168"},
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
static int re_rxeof		(struct rl_softc *);
static void re_txeof		(struct rl_softc *);
#ifdef DEVICE_POLLING
static void re_poll		(struct ifnet *, enum poll_cmd, int);
static void re_poll_locked	(struct ifnet *, enum poll_cmd, int);
#endif
static int re_intr		(void *);
static void re_tick		(void *);
static void re_tx_task		(void *, int);
static void re_int_task		(void *, int);
static void re_start		(struct ifnet *);
static int re_ioctl		(struct ifnet *, u_long, caddr_t);
static void re_init		(void *);
static void re_init_locked	(struct rl_softc *);
static void re_stop		(struct rl_softc *);
static void re_watchdog		(struct rl_softc *);
static int re_suspend		(device_t);
static int re_resume		(device_t);
static int re_shutdown		(device_t);
static int re_ifmedia_upd	(struct ifnet *);
static void re_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void re_eeprom_putbyte	(struct rl_softc *, int);
static void re_eeprom_getword	(struct rl_softc *, int, u_int16_t *);
static void re_read_eeprom	(struct rl_softc *, caddr_t, int, int);
static int re_gmii_readreg	(device_t, int, int);
static int re_gmii_writereg	(device_t, int, int, int);

static int re_miibus_readreg	(device_t, int, int);
static int re_miibus_writereg	(device_t, int, int, int);
static void re_miibus_statchg	(device_t);

static void re_setmulti		(struct rl_softc *);
static void re_reset		(struct rl_softc *);

#ifdef RE_DIAG
static int re_diag		(struct rl_softc *);
#endif

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

	d = addr | (RL_9346_READ << sc->rl_eewidth);

	/*
	 * Feed in each bit and strobe the clock.
	 */

	for (i = 1 << (sc->rl_eewidth + 3); i; i >>= 1) {
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
static void
re_eeprom_getword(sc, addr, dest)
	struct rl_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/*
	 * Send address of word we want to read.
	 */
	re_eeprom_putbyte(sc, addr);

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

	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
re_read_eeprom(sc, dest, off, cnt)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	CSR_SETBIT_1(sc, RL_EECMD, RL_EEMODE_PROGRAM);

        DELAY(100);

	for (i = 0; i < cnt; i++) {
		CSR_SETBIT_1(sc, RL_EECMD, RL_EE_SEL);
		re_eeprom_getword(sc, off + i, &word);
		CSR_CLRBIT_1(sc, RL_EECMD, RL_EE_SEL);
		ptr = (u_int16_t *)(dest + (i * 2));
                *ptr = word;
	}

	CSR_CLRBIT_1(sc, RL_EECMD, RL_EEMODE_PROGRAM);

	return;
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
		device_printf(sc->rl_dev, "PHY read failed\n");
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
		device_printf(sc->rl_dev, "PHY write failed\n");
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
		device_printf(sc->rl_dev, "bad phy register\n");
		return (0);
	}
	rval = CSR_READ_2(sc, re8139_reg);
	if (sc->rl_type == RL_8139CPLUS && re8139_reg == RL_BMCR) {
		/* 8139C+ has different bit layout. */
		rval &= ~(BMCR_LOOP | BMCR_ISO);
	}
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
		if (sc->rl_type == RL_8139CPLUS) {
			/* 8139C+ has different bit layout. */
			data &= ~(BMCR_LOOP | BMCR_ISO);
		}
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
		device_printf(sc->rl_dev, "bad phy register\n");
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
	u_int32_t		hwrev;

	RL_LOCK_ASSERT(sc);

	ifp = sc->rl_ifp;


	rxfilt = CSR_READ_4(sc, RL_RXCFG);
	rxfilt &= ~(RL_RXCFG_RX_ALLPHYS | RL_RXCFG_RX_MULTI);
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		if (ifp->if_flags & IFF_PROMISC)
			rxfilt |= RL_RXCFG_RX_ALLPHYS;
		/*
		 * Unlike other hardwares, we have to explicitly set
		 * RL_RXCFG_RX_MULTI to receive multicast frames in
		 * promiscuous mode.
		 */
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

	/*
	 * For some unfathomable reason, RealTek decided to reverse
	 * the order of the multicast hash registers in the PCI Express
	 * parts. This means we have to write the hash pattern in reverse
	 * order for those devices.
	 */

	hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;

	switch (hwrev) {
	case RL_HWREV_8100E:
	case RL_HWREV_8101E:
	case RL_HWREV_8168_SPIN1:
	case RL_HWREV_8168_SPIN2:
	case RL_HWREV_8168_SPIN3:
		CSR_WRITE_4(sc, RL_MAR0, bswap32(hashes[1]));
		CSR_WRITE_4(sc, RL_MAR4, bswap32(hashes[0]));
		break;
	default:
		CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
		CSR_WRITE_4(sc, RL_MAR4, hashes[1]);
		break;
	}
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
		device_printf(sc->rl_dev, "reset never completed!\n");

	CSR_WRITE_1(sc, 0x82, 1);
}

#ifdef RE_DIAG

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
	int			total_len, i, error = 0, phyaddr;
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
	re_reset(sc);
	re_init_locked(sc);
	sc->rl_link = 1;
	if (sc->rl_type == RL_8169)
		phyaddr = 1;
	else
		phyaddr = 0;

	re_miibus_writereg(sc->rl_dev, phyaddr, MII_BMCR, BMCR_RESET);
	for (i = 0; i < RL_TIMEOUT; i++) {
		status = re_miibus_readreg(sc->rl_dev, phyaddr, MII_BMCR);
		if (!(status & BMCR_RESET))
			break;
	}

	re_miibus_writereg(sc->rl_dev, phyaddr, MII_BMCR, BMCR_LOOP);
	CSR_WRITE_2(sc, RL_ISR, RL_INTRS);

	DELAY(100000);

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
		CSR_WRITE_2(sc, RL_ISR, status);
		if ((status & (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK)) ==
		    (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK))
			break;
		DELAY(10);
	}

	if (i == RL_TIMEOUT) {
		device_printf(sc->rl_dev,
		    "diagnostic failed, failed to receive packet in"
		    " loopback mode\n");
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
		device_printf(sc->rl_dev,
		    "diagnostic failed, received short packet\n");
		error = EIO;
		goto done;
	}

	/* Test that the received packet data matches what we sent. */

	if (bcmp((char *)&eh->ether_dhost, (char *)&dst, ETHER_ADDR_LEN) ||
	    bcmp((char *)&eh->ether_shost, (char *)&src, ETHER_ADDR_LEN) ||
	    ntohs(eh->ether_type) != ETHERTYPE_IP) {
		device_printf(sc->rl_dev, "WARNING, DMA FAILURE!\n");
		device_printf(sc->rl_dev, "expected TX data: %6D/%6D/0x%x\n",
		    dst, ":", src, ":", ETHERTYPE_IP);
		device_printf(sc->rl_dev, "received RX data: %6D/%6D/0x%x\n",
		    eh->ether_dhost, ":",  eh->ether_shost, ":",
		    ntohs(eh->ether_type));
		device_printf(sc->rl_dev, "You may have a defective 32-bit "
		    "NIC plugged into a 64-bit PCI slot.\n");
		device_printf(sc->rl_dev, "Please re-install the NIC in a "
		    "32-bit slot for proper operation.\n");
		device_printf(sc->rl_dev, "Read the re(4) man page for more "
		    "details.\n");
		error = EIO;
	}

done:
	/* Turn interface off, release resources */

	sc->rl_testmode = 0;
	sc->rl_link = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(sc);
	if (m0 != NULL)
		m_freem(m0);

	RL_UNLOCK(sc);

	return (error);
}

#endif

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
			 * Only attach to rev. 3 of the Linksys EG1032 adapter.
			 * Rev. 2 i supported by sk(4).
			 */
			if ((t->rl_vid == LINKSYS_VENDORID) &&
				(t->rl_did == LINKSYS_DEVICEID_EG1032) &&
				(pci_get_subdevice(dev) !=
				LINKSYS_SUBDEVICE_EG1032_REV3)) {
				t++;
				continue;
			}

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
	u_int32_t		cmdstat;
	int			totlen = 0;

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
		d = &ctx->rl_ring[idx];
		if (le32toh(d->rl_cmdstat) & RL_RDESC_STAT_OWN) {
			ctx->rl_maxsegs = 0;
			return;
		}
		cmdstat = segs[i].ds_len;
		totlen += segs[i].ds_len;
		d->rl_vlanctl = 0;
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
	    NULL, RL_TX_LIST_SZ, 1, RL_TX_LIST_SZ, 0,
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
	    NULL, RL_RX_LIST_SZ, 1, RL_RX_LIST_SZ, 0,
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
	u_int16_t		as[ETHER_ADDR_LEN / 2];
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	struct rl_hwrev		*hw_rev;
	int			hwrev;
	u_int16_t		re_did = 0;
	int			error = 0, rid, i;
	int			msic, reg;

	sc = device_get_softc(dev);
	sc->rl_dev = dev;

	mtx_init(&sc->rl_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init_mtx(&sc->rl_stat_callout, &sc->rl_mtx, 0);

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = RL_RID;
	sc->rl_res = bus_alloc_resource_any(dev, RL_RES, &rid,
	    RF_ACTIVE);

	if (sc->rl_res == NULL) {
		device_printf(dev, "couldn't map ports/memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->rl_btag = rman_get_bustag(sc->rl_res);
	sc->rl_bhandle = rman_get_bushandle(sc->rl_res);

	msic = 0;
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		msic = pci_msi_count(dev);
		if (bootverbose)
			device_printf(dev, "MSI count : %d\n", msic);
	}
	if (msic == RL_MSI_MESSAGES  && msi_disable == 0) {
		if (pci_alloc_msi(dev, &msic) == 0) {
			if (msic == RL_MSI_MESSAGES) {
				device_printf(dev, "Using %d MSI messages\n",
				    msic);
				sc->rl_msi = 1;
			} else
				pci_release_msi(dev);
		}
	}

	/* Allocate interrupt */
	if (sc->rl_msi == 0) {
		rid = 0;
		sc->rl_irq[0] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (sc->rl_irq[0] == NULL) {
			device_printf(dev, "couldn't allocate IRQ resources\n");
			error = ENXIO;
			goto fail;
		}
	} else {
		for (i = 0, rid = 1; i < RL_MSI_MESSAGES; i++, rid++) {
			sc->rl_irq[i] = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &rid, RF_ACTIVE);
			if (sc->rl_irq[i] == NULL) {
				device_printf(dev,
				    "couldn't llocate IRQ resources for "
				    "message %d\n", rid);
				error = ENXIO;
				goto fail;
			}
		}
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

	sc->rl_eewidth = RL_9356_ADDR_LEN;
	re_read_eeprom(sc, (caddr_t)&re_did, 0, 1);
	if (re_did != 0x8129)
	        sc->rl_eewidth = RL_9346_ADDR_LEN;

	/*
	 * Get station address from the EEPROM.
	 */
	re_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3);
	for (i = 0; i < ETHER_ADDR_LEN / 2; i++)
		as[i] = le16toh(as[i]);
	bcopy(as, eaddr, sizeof(eaddr));

	if (sc->rl_type == RL_8169) {
		/* Set RX length mask */
		sc->rl_rxlenmask = RL_RDESC_STAT_GFRAGLEN;
		sc->rl_txstart = RL_GTXSTART;
	} else {
		/* Set RX length mask */
		sc->rl_rxlenmask = RL_RDESC_STAT_FRAGLEN;
		sc->rl_txstart = RL_TXSTART;
	}

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define RL_NSEG_NEW 32
	error = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MAXBSIZE, RL_NSEG_NEW, BUS_SPACE_MAXSIZE_32BIT, 0,
	    NULL, NULL, &sc->rl_parent_tag);
	if (error)
		goto fail;

	error = re_allocmem(dev, sc);

	if (error)
		goto fail;

	ifp = sc->rl_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->rl_miibus,
	    re_ifmedia_upd, re_ifmedia_sts)) {
		device_printf(dev, "MII without any phy!\n");
		error = ENXIO;
		goto fail;
	}

	/* Take PHY out of power down mode. */
	if (sc->rl_type == RL_8169) {
		uint32_t rev;

		rev = CSR_READ_4(sc, RL_TXCFG);
		/* HWVERID 0, 1 and 2 :  bit26-30, bit23 */
		rev &= 0x7c800000;
		if (rev != 0) {
			/* RTL8169S single chip */
			switch (rev) {
			case RL_HWREV_8169_8110SB:
			case RL_HWREV_8169_8110SC:
			case RL_HWREV_8168_SPIN2:
			case RL_HWREV_8168_SPIN3:
				re_gmii_writereg(dev, 1, 0x1f, 0);
				re_gmii_writereg(dev, 1, 0x0e, 0);
				break;
			default:
				break;
			}
		}
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
	ifp->if_start = re_start;
	ifp->if_hwassist = RE_CSUM_FEATURES;
	ifp->if_capabilities = IFCAP_HWCSUM;
	ifp->if_capenable = ifp->if_capabilities;
	ifp->if_init = re_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, RL_IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = RL_IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	TASK_INIT(&sc->rl_txtask, 1, re_tx_task, ifp);
	TASK_INIT(&sc->rl_inttask, 0, re_int_task, sc);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* VLAN capability setup */
	ifp->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING;
	if (ifp->if_capabilities & IFCAP_HWCSUM)
		ifp->if_capabilities |= IFCAP_VLAN_HWCSUM;
	ifp->if_capenable = ifp->if_capabilities;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	/*
	 * Tell the upper layer(s) we support long frames.
	 * Must appear after the call to ether_ifattach() because
	 * ether_ifattach() sets ifi_hdrlen to the default value.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

#ifdef RE_DIAG
	/*
	 * Perform hardware diagnostic on the original RTL8169.
	 * Some 32-bit cards were incorrectly wired and would
	 * malfunction if plugged into a 64-bit slot.
	 */

	if (hwrev == RL_HWREV_8169) {
		error = re_diag(sc);
		if (error) {
			device_printf(dev,
		    	"attach aborted due to hardware diag failure\n");
			ether_ifdetach(ifp);
			goto fail;
		}
	}
#endif

	/* Hook interrupt last to avoid having to lock softc */
	if (sc->rl_msi == 0)
		error = bus_setup_intr(dev, sc->rl_irq[0],
		    INTR_TYPE_NET | INTR_MPSAFE, re_intr, NULL, sc,
		    &sc->rl_intrhand[0]);
	else {
		for (i = 0; i < RL_MSI_MESSAGES; i++) {
			error = bus_setup_intr(dev, sc->rl_irq[i],
			    INTR_TYPE_NET | INTR_MPSAFE, re_intr, NULL, sc,
		    	    &sc->rl_intrhand[i]);
			if (error != 0)
				break;
		}
	}
	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		ether_ifdetach(ifp);
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
	int			i, rid;

	sc = device_get_softc(dev);
	ifp = sc->rl_ifp;
	KASSERT(mtx_initialized(&sc->rl_mtx), ("re mutex not initialized"));

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif
	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		RL_LOCK(sc);
#if 0
		sc->suspended = 1;
#endif
		re_stop(sc);
		RL_UNLOCK(sc);
		callout_drain(&sc->rl_stat_callout);
		taskqueue_drain(taskqueue_fast, &sc->rl_inttask);
		taskqueue_drain(taskqueue_fast, &sc->rl_txtask);
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
		ether_ifdetach(ifp);
	}
	if (sc->rl_miibus)
		device_delete_child(dev, sc->rl_miibus);
	bus_generic_detach(dev);

	/*
	 * The rest is resource deallocation, so we should already be
	 * stopped here.
	 */

	for (i = 0; i < RL_MSI_MESSAGES; i++) {
		if (sc->rl_intrhand[i] != NULL) {
			bus_teardown_intr(dev, sc->rl_irq[i],
			    sc->rl_intrhand[i]);
			sc->rl_intrhand[i] = NULL;
		}
	}
	if (ifp != NULL)
		if_free(ifp);
	if (sc->rl_msi == 0) {
		if (sc->rl_irq[0] != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, 0,
			    sc->rl_irq[0]);
			sc->rl_irq[0] = NULL;
		}
	} else {
		for (i = 0, rid = 1; i < RL_MSI_MESSAGES; i++, rid++) {
			if (sc->rl_irq[i] != NULL) {
				bus_release_resource(dev, SYS_RES_IRQ, rid,
				    sc->rl_irq[i]);
				sc->rl_irq[i] = NULL;
			}
		}
		pci_release_msi(dev);
	}
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
		if (arg.rl_maxsegs == 0)
			bus_dmamap_unload(sc->rl_ldata.rl_mtag,
			    sc->rl_ldata.rl_rx_dmamap[idx]);
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
static int
re_rxeof(sc)
	struct rl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	struct rl_desc		*cur_rx;
	u_int32_t		rxstat, rxvlan;
	int			maxpkt = 16;

	RL_LOCK_ASSERT(sc);

	ifp = sc->rl_ifp;
	i = sc->rl_ldata.rl_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (!RL_OWN(&sc->rl_ldata.rl_rx_list[i]) && maxpkt) {
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
		maxpkt--;
		if (rxvlan & RL_RDESC_VLANCTL_TAG) {
			m->m_pkthdr.ether_vtag =
			    ntohs((rxvlan & RL_RDESC_VLANCTL_DATA));
			m->m_flags |= M_VLANTAG;
		}
		RL_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		RL_LOCK(sc);
	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = i;

	if (maxpkt)
		return(EAGAIN);

	return(0);
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

	while (sc->rl_ldata.rl_tx_free < RL_TX_DESC_CNT) {
		txstat = le32toh(sc->rl_ldata.rl_tx_list[idx].rl_cmdstat);
		if (txstat & RL_TDESC_CMD_OWN)
			break;

		sc->rl_ldata.rl_tx_list[idx].rl_bufaddr_lo = 0;

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
	sc->rl_ldata.rl_tx_considx = idx;

	/* No changes made to the TX ring, so no flush needed */

	if (sc->rl_ldata.rl_tx_free > RL_TX_DESC_THLD)
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (sc->rl_ldata.rl_tx_free < RL_TX_DESC_CNT) {
		/*
		 * Some chips will ignore a second TX request issued
		 * while an existing transmission is in progress. If
		 * the transmitter goes idle but there are still
		 * packets waiting to be sent, we need to restart the
		 * channel here to flush them out. This only seems to
		 * be required with the PCIe devices.
		 */
		CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);

#ifdef RE_TX_MODERATION
		/*
		 * If not all descriptors have been reaped yet, reload
		 * the timer so that we will eventually get another
		 * interrupt that will cause us to re-enter this routine.
		 * This is done in case the transmitter has gone idle.
		 */
		CSR_WRITE_4(sc, RL_TIMERCNT, 1);
#endif
	} else
		sc->rl_watchdog_timer = 0;
}

static void
re_tick(xsc)
	void			*xsc;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;

	sc = xsc;
	ifp = sc->rl_ifp;

	RL_LOCK_ASSERT(sc);

	re_watchdog(sc);

	mii = device_get_softc(sc->rl_miibus);
	mii_tick(mii);
	if (sc->rl_link) {
		if (!(mii->mii_media_status & IFM_ACTIVE))
			sc->rl_link = 0;
	} else {
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->rl_link = 1;
			if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
				taskqueue_enqueue_fast(taskqueue_fast,
				    &sc->rl_txtask);
		}
	}

	callout_reset(&sc->rl_stat_callout, hz, re_tick, sc);
}

#ifdef DEVICE_POLLING
static void
re_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		re_poll_locked(ifp, cmd, count);
	RL_UNLOCK(sc);
}

static void
re_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK_ASSERT(sc);

	sc->rxcycles = count;
	re_rxeof(sc);
	re_txeof(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		taskqueue_enqueue_fast(taskqueue_fast, &sc->rl_txtask);

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

static int
re_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	uint16_t		status;

	sc = arg;

	status = CSR_READ_2(sc, RL_ISR);
	if (status == 0xFFFF || (status & RL_INTRS_CPLUS) == 0)
                return (FILTER_STRAY);
	CSR_WRITE_2(sc, RL_IMR, 0);

	taskqueue_enqueue_fast(taskqueue_fast, &sc->rl_inttask);

	return (FILTER_HANDLED);
}

static void
re_int_task(arg, npending)
	void			*arg;
	int			npending;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;
	int			rval = 0;

	sc = arg;
	ifp = sc->rl_ifp;

	RL_LOCK(sc);

	status = CSR_READ_2(sc, RL_ISR);
        CSR_WRITE_2(sc, RL_ISR, status);

	if (sc->suspended || !(ifp->if_flags & IFF_UP)) {
		RL_UNLOCK(sc);
		return;
	}

#ifdef DEVICE_POLLING
	if  (ifp->if_capenable & IFCAP_POLLING) {
		RL_UNLOCK(sc);
		return;
	}
#endif

	if (status & (RL_ISR_RX_OK|RL_ISR_RX_ERR|RL_ISR_FIFO_OFLOW))
		rval = re_rxeof(sc);

#ifdef RE_TX_MODERATION
	if (status & (RL_ISR_TIMEOUT_EXPIRED|
#else
	if (status & (RL_ISR_TX_OK|
#endif
	    RL_ISR_TX_ERR|RL_ISR_TX_DESC_UNAVAIL))
		re_txeof(sc);

	if (status & RL_ISR_SYSTEM_ERR) {
		re_reset(sc);
		re_init_locked(sc);
	}

	if (status & RL_ISR_LINKCHG) {
		callout_stop(&sc->rl_stat_callout);
		re_tick(sc);
	}

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		taskqueue_enqueue_fast(taskqueue_fast, &sc->rl_txtask);

	RL_UNLOCK(sc);

        if ((CSR_READ_2(sc, RL_ISR) & RL_INTRS_CPLUS) || rval) {
		taskqueue_enqueue_fast(taskqueue_fast, &sc->rl_inttask);
		return;
	}

	CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);

	return;
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

	RL_LOCK_ASSERT(sc);

	if (sc->rl_ldata.rl_tx_free <= RL_TX_DESC_THLD)
		return (EFBIG);

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. This is according to testing done with an 8169
	 * chip. This is a requirement.
	 */

	arg.rl_flags = 0;

	if (((*m_head)->m_pkthdr.csum_flags & CSUM_TSO) != 0)
		arg.rl_flags = RL_TDESC_CMD_LGSEND |
		    ((uint32_t)(*m_head)->m_pkthdr.tso_segsz <<
		    RL_TDESC_CMD_MSSVAL_SHIFT);
	else {
		if ((*m_head)->m_pkthdr.csum_flags & CSUM_IP)
			arg.rl_flags |= RL_TDESC_CMD_IPCSUM;
		if ((*m_head)->m_pkthdr.csum_flags & CSUM_TCP)
			arg.rl_flags |= RL_TDESC_CMD_TCPCSUM;
		if ((*m_head)->m_pkthdr.csum_flags & CSUM_UDP)
			arg.rl_flags |= RL_TDESC_CMD_UDPCSUM;
	}

	arg.rl_idx = *idx;
	arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
	if (arg.rl_maxsegs > RL_TX_DESC_THLD)
		arg.rl_maxsegs -= RL_TX_DESC_THLD;
	arg.rl_ring = sc->rl_ldata.rl_tx_list;

	map = sc->rl_ldata.rl_tx_dmamap[*idx];

	/*
	 * With some of the RealTek chips, using the checksum offload
	 * support in conjunction with the autopadding feature results
	 * in the transmission of corrupt frames. For example, if we
	 * need to send a really small IP fragment that's less than 60
	 * bytes in size, and IP header checksumming is enabled, the
	 * resulting ethernet frame that appears on the wire will
	 * have garbled payload. To work around this, if TX checksum
	 * offload is enabled, we always manually pad short frames out
	 * to the minimum ethernet frame size. We do this by pretending
	 * the mbuf chain has too many fragments so the coalescing code
	 * below can assemble the packet into a single buffer that's
	 * padded out to the mininum frame size.
	 *
	 * Note: this appears unnecessary for TCP, and doing it for TCP
	 * with PCIe adapters seems to result in bad checksums.
	 */

	if (arg.rl_flags && !(arg.rl_flags & RL_TDESC_CMD_TCPCSUM) &&
            (*m_head)->m_pkthdr.len < RL_MIN_FRAMELEN)
		error = EFBIG;
	else
		error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
		    *m_head, re_dma_map_desc, &arg, BUS_DMA_NOWAIT);

	if (error && error != EFBIG) {
		device_printf(sc->rl_dev, "can't map mbuf (error %d)\n", error);
		return (ENOBUFS);
	}

	/* Too many segments to map, coalesce into a single mbuf */

	if (error || arg.rl_maxsegs == 0) {
		if (arg.rl_maxsegs == 0)
			bus_dmamap_unload(sc->rl_ldata.rl_mtag, map);
		m_new = m_defrag(*m_head, M_DONTWAIT);
		if (m_new == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m_new;

		/*
		 * Manually pad short frames, and zero the pad space
		 * to avoid leaking data.
		 */
		if (m_new->m_pkthdr.len < RL_MIN_FRAMELEN) {
			bzero(mtod(m_new, char *) + m_new->m_pkthdr.len,
			    RL_MIN_FRAMELEN - m_new->m_pkthdr.len);
			m_new->m_pkthdr.len += RL_MIN_FRAMELEN -
			    m_new->m_pkthdr.len;
			m_new->m_len = m_new->m_pkthdr.len;
		}

		/* Note that we'll run over RL_TX_DESC_THLD here. */
		arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
		error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
		    *m_head, re_dma_map_desc, &arg, BUS_DMA_NOWAIT);
		if (error || arg.rl_maxsegs == 0) {
			device_printf(sc->rl_dev,
			    "can't map defragmented mbuf (error %d)\n", error);
			m_freem(m_new);
			*m_head = NULL;
			if (arg.rl_maxsegs == 0)
				bus_dmamap_unload(sc->rl_ldata.rl_mtag, map);
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
	if ((*m_head)->m_flags & M_VLANTAG)
		sc->rl_ldata.rl_tx_list[*idx].rl_vlanctl =
		    htole32(htons((*m_head)->m_pkthdr.ether_vtag) |
		    RL_TDESC_VLANCTL_TAG);

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
re_tx_task(arg, npending)
	void			*arg;
	int			npending;
{
	struct ifnet		*ifp;

	ifp = arg;
	re_start(ifp);

	return;
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */
static void
re_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;
	int			idx, queued = 0;

	sc = ifp->if_softc;

	RL_LOCK(sc);

	if (!sc->rl_link || ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		RL_UNLOCK(sc);
		return;
	}

	idx = sc->rl_ldata.rl_tx_prodidx;

	while (sc->rl_ldata.rl_tx_mbuf[idx] == NULL) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (re_encap(sc, &m_head, &idx)) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		ETHER_BPF_MTAP(ifp, m_head);

		queued++;
	}

	if (queued == 0) {
#ifdef RE_TX_MODERATION
		if (sc->rl_ldata.rl_tx_free != RL_TX_DESC_CNT)
			CSR_WRITE_4(sc, RL_TIMERCNT, 1);
#endif
		RL_UNLOCK(sc);
		return;
	}

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
	    sc->rl_ldata.rl_tx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_tx_prodidx = idx;

	CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);

#ifdef RE_TX_MODERATION
	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the TIMERINT register, and then trigger an
	 * interrupt. Each time we write to the TIMERCNT register,
	 * the timer count is reset to 0.
	 */
	CSR_WRITE_4(sc, RL_TIMERCNT, 1);
#endif

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	sc->rl_watchdog_timer = 5;

	RL_UNLOCK(sc);

	return;
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
	union {
		uint32_t align_dummy;
		u_char eaddr[ETHER_ADDR_LEN];
        } eaddr;

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
	    RL_CPLUSCMD_VLANSTRIP|RL_CPLUSCMD_RXCSUM_ENB);

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	/* Copy MAC address on stack to align. */
	bcopy(IF_LLADDR(ifp), eaddr.eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	CSR_WRITE_4(sc, RL_IDR0,
	    htole32(*(u_int32_t *)(&eaddr.eaddr[0])));
	CSR_WRITE_4(sc, RL_IDR4,
	    htole32(*(u_int32_t *)(&eaddr.eaddr[4])));
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

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

	CSR_WRITE_1(sc, RL_EARLY_TX_THRESH, 16);

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
	if (ifp->if_capenable & IFCAP_POLLING)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else	/* otherwise ... */
#endif

	/*
	 * Enable interrupts.
	 */
	if (sc->rl_testmode)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
	CSR_WRITE_2(sc, RL_ISR, RL_INTRS_CPLUS);

	/* Set initial TX threshold */
	sc->rl_txthresh = RL_TX_THRESH_INIT;

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);
#endif

#ifdef RE_TX_MODERATION
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
#endif

	/*
	 * For 8169 gigE NICs, set the max allowed RX packet
	 * size so we can receive jumbo frames.
	 */
	if (sc->rl_type == RL_8169)
		CSR_WRITE_2(sc, RL_MAXRXPKTLEN, 16383);

	if (sc->rl_testmode)
		return;

	mii_mediachg(mii);

	CSR_WRITE_1(sc, RL_CFG1, CSR_READ_1(sc, RL_CFG1) | RL_CFG1_DRVLOAD);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->rl_link = 0;
	sc->rl_watchdog_timer = 0;
	callout_reset(&sc->rl_stat_callout, hz, re_tick, sc);
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
	RL_LOCK(sc);
	mii_mediachg(mii);
	RL_UNLOCK(sc);

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

	RL_LOCK(sc);
	mii_pollstat(mii);
	RL_UNLOCK(sc);
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
		RL_LOCK(sc);
		if (ifr->ifr_mtu > RL_JUMBO_MTU)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		RL_UNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		RL_LOCK(sc);
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if (((ifp->if_flags ^ sc->rl_if_flags)
				    & IFF_PROMISC) != 0)
					re_setmulti(sc);
			} else
				re_init_locked(sc);
		} else {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
				re_stop(sc);
		}
		sc->rl_if_flags = ifp->if_flags;
		RL_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		RL_LOCK(sc);
		re_setmulti(sc);
		RL_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->rl_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
	    {
		int mask, reinit;

		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		reinit = 0;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(re_poll, ifp);
				if (error)
					return(error);
				RL_LOCK(sc);
				/* Disable interrupts */
				CSR_WRITE_2(sc, RL_IMR, 0x0000);
				ifp->if_capenable |= IFCAP_POLLING;
				RL_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupts. */
				RL_LOCK(sc);
				CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
				ifp->if_capenable &= ~IFCAP_POLLING;
				RL_UNLOCK(sc);
			}
		}
#endif /* DEVICE_POLLING */
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if (ifp->if_capenable & IFCAP_TXCSUM)
				ifp->if_hwassist |= RE_CSUM_FEATURES;
			else
				ifp->if_hwassist &= ~RE_CSUM_FEATURES;
			reinit = 1;
		}
		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			reinit = 1;
		}
		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			if ((IFCAP_TSO4 & ifp->if_capenable) &&
			    (IFCAP_TSO4 & ifp->if_capabilities))
				ifp->if_hwassist |= CSUM_TSO;
			else
				ifp->if_hwassist &= ~CSUM_TSO;
		}
		if (reinit && ifp->if_drv_flags & IFF_DRV_RUNNING)
			re_init(sc);
		VLAN_CAPABILITIES(ifp);
	    }
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
re_watchdog(sc)
	struct rl_softc		*sc;
{

	RL_LOCK_ASSERT(sc);

	if (sc->rl_watchdog_timer == 0 || --sc->rl_watchdog_timer != 0)
		return;

	device_printf(sc->rl_dev, "watchdog timeout\n");
	sc->rl_ifp->if_oerrors++;

	re_txeof(sc);
	re_rxeof(sc);
	re_init_locked(sc);
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

	sc->rl_watchdog_timer = 0;
	callout_stop(&sc->rl_stat_callout);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);
	CSR_WRITE_2(sc, RL_ISR, 0xFFFF);

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
static int
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
	 * cases.
	 */
	sc->rl_ifp->if_flags &= ~IFF_UP;
	RL_UNLOCK(sc);

	return (0);
}
