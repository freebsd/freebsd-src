/*
 * Copyright (c) 1997, 1998-2003
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

/*
 * RealTek 8129/8139/8139C+/8169 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 and 10/100/1000 adapters
 * based on RealTek chipsets. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
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
 *
 * Fast forward a few years. RealTek now has a new chip called the
 * 8139C+ which at long last implements descriptor-based DMA. Not
 * only that, it supports RX and TX TCP/IP checksum offload, VLAN
 * tagging and insertion, TCP large send and 64-bit addressing.
 * Better still, it allows arbitrary byte alignments for RX and
 * TX buffers, meaning no copying is necessary on any architecture.
 * There are a few limitations however: the RX and TX descriptor
 * rings must be aligned on 256 byte boundaries, they must be in
 * contiguous RAM, and each ring can have a maximum of 64 descriptors.
 * There are two TX descriptor queues: one normal priority and one
 * high. Descriptor ring addresses and DMA buffer addresses are
 * 64 bits wide. The 8139C+ is also backwards compatible with the
 * 8139, so the chip will still function with older drivers: C+
 * mode has to be enabled by setting the appropriate bits in the C+
 * command register. The PHY access mechanism appears to be unchanged.
 *
 * The 8169 is a 10/100/1000 ethernet MAC. It has almost the same
 * programming API as the C+ mode of the 8139C+, with a couple of
 * minor changes and additions: TX start register and timer interrupt
 * register are located at different offsets, and there are additional
 * registers for GMII PHY status and control, as well as TBI-mode
 * status and control. There is also a maximum RX packet size
 * register to allow the chip to receive jumbo frames. The 8169
 * can only be programmed in C+ mode: the old 8139 programming
 * method isn't supported with this chip. Also, RealTek has a LOM
 * (LAN On Motherboard) gigabit MAC chip called the RTL8110S which
 * I believe to be register compatible with the 8169. Unlike the
 * 8139C+, the 8169 can have up to 1024 descriptors per DMA ring.
 * The reference 8169 board design uses a Marvell 88E1000 'Alaska'
 * copper PHY.
 *
 * The 8169S and 8110S are newer versions of the 8169. Available
 * in both 32-bit and 64-bit forms, these devices have built-in
 * copper 10/100/1000 PHYs. The 8110S is a lan-on-motherboard chip
 * that is pin-for-pin compatible with the 8100. Unfortunately,
 * RealTek has not released programming manuals for the 8169S and
 * 8110S yet. The datasheet for the original 8169 provides most
 * of the information, but you must refer to RealTek's 8169 Linux
 * driver to fill in the gaps. Mostly, it appears that the built-in
 * PHY requires some special initialization. The original 8169
 * datasheet and the 8139C+ datasheet can be obtained from
 * http://www.freebsd.org/~wpaul/RealTek.
 *
 * This driver now supports both the old 8139 and new 8139C+
 * programming models. We detect the 8139C+ by looking for the
 * corresponding hardware rev bits, and we detect the 8169 by its
 * PCI ID. Two new NIC type codes, RL_8139CPLUS and RL_8169 have
 * been added to distinguish the chips at runtime. Separate RX and
 * TX handling routines have been added to handle C+ mode, which
 * are selected via function pointers that are initialized during
 * the driver attach phase.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

MODULE_DEPEND(rl, pci, 1, 1, 1);
MODULE_DEPEND(rl, ether, 1, 1, 1);
MODULE_DEPEND(rl, miibus, 1, 1, 1);

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

#define RL_CSUM_FEATURES    (CSUM_IP | CSUM_TCP | CSUM_UDP)

/*
 * Various supported device vendors/types and their names.
 */
static struct rl_type rl_devs[] = {
	{ RT_VENDORID, RT_DEVICEID_8129, RL_8129,
		"RealTek 8129 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8139, RL_8139,
		"RealTek 8139 10/100BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8169, RL_8169,
		"RealTek 8169 10/100/1000BaseTX" },
	{ RT_VENDORID, RT_DEVICEID_8138, RL_8139,
		"RealTek 8139 10/100BaseTX CardBus" },
	{ RT_VENDORID, RT_DEVICEID_8100, RL_8139,
		"RealTek 8100 10/100BaseTX" },
	{ ACCTON_VENDORID, ACCTON_DEVICEID_5030, RL_8139,
		"Accton MPX 5030/5038 10/100BaseTX" },
	{ DELTA_VENDORID, DELTA_DEVICEID_8139, RL_8139,
		"Delta Electronics 8139 10/100BaseTX" },
	{ ADDTRON_VENDORID, ADDTRON_DEVICEID_8139, RL_8139,
		"Addtron Technolgy 8139 10/100BaseTX" },
	{ DLINK_VENDORID, DLINK_DEVICEID_530TXPLUS, RL_8139,
		"D-Link DFE-530TX+ 10/100BaseTX" },
	{ DLINK_VENDORID, DLINK_DEVICEID_690TXD, RL_8139,
		"D-Link DFE-690TXD 10/100BaseTX" },
	{ NORTEL_VENDORID, ACCTON_DEVICEID_5030, RL_8139,
		"Nortel Networks 10/100BaseTX" },
	{ COREGA_VENDORID, COREGA_DEVICEID_FETHERCBTXD, RL_8139,
		"Corega FEther CB-TXD" },
	{ COREGA_VENDORID, COREGA_DEVICEID_FETHERIICBTXD, RL_8139,
		"Corega FEtherII CB-TXD" },
		/* XXX what type of realtek is PEPPERCON_DEVICEID_ROLF ? */
	{ PEPPERCON_VENDORID, PEPPERCON_DEVICEID_ROLF, RL_8139,
		"Peppercon AG ROL-F" },
	{ PLANEX_VENDORID, PLANEX_DEVICEID_FNW3800TX, RL_8139,
		"Planex FNW-3800-TX" },
	{ CP_VENDORID, RT_DEVICEID_8139, RL_8139,
		"Compaq HNE-300" },
	{ LEVEL1_VENDORID, LEVEL1_DEVICEID_FPC0106TX, RL_8139,
		"LevelOne FPC-0106TX" },
	{ EDIMAX_VENDORID, EDIMAX_DEVICEID_EP4103DL, RL_8139,
		"Edimax EP-4103DL CardBus" },
	{ 0, 0, 0, NULL }
};

static struct rl_hwrev rl_hwrevs[] = {
	{ RL_HWREV_8139, RL_8139,  "" },
	{ RL_HWREV_8139A, RL_8139, "A" },
	{ RL_HWREV_8139AG, RL_8139, "A-G" },
	{ RL_HWREV_8139B, RL_8139, "B" },
	{ RL_HWREV_8130, RL_8139, "8130" },
	{ RL_HWREV_8139C, RL_8139, "C" },
	{ RL_HWREV_8139D, RL_8139, "8139D/8100B/8100C" },
	{ RL_HWREV_8139CPLUS, RL_8139CPLUS, "C+"},
	{ RL_HWREV_8169, RL_8169, "8169"},
	{ RL_HWREV_8110, RL_8169, "8169S/8110S"},
	{ RL_HWREV_8100, RL_8139, "8100"},
	{ RL_HWREV_8101, RL_8139, "8101"},
	{ 0, 0, NULL }
};

static int rl_probe		(device_t);
static int rl_attach		(device_t);
static int rl_detach		(device_t);

static int rl_encap		(struct rl_softc *, struct mbuf *);
static int rl_encapcplus	(struct rl_softc *, struct mbuf *, int *);

static void rl_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static void rl_dma_map_desc	(void *, bus_dma_segment_t *, int,
				    bus_size_t, int);
static int rl_allocmem		(device_t, struct rl_softc *);
static int rl_allocmemcplus	(device_t, struct rl_softc *);
static int rl_newbuf		(struct rl_softc *, int, struct mbuf *);
static int rl_rx_list_init	(struct rl_softc *);
static int rl_tx_list_init	(struct rl_softc *);
static void rl_rxeof		(struct rl_softc *);
static void rl_rxeofcplus	(struct rl_softc *);
static void rl_txeof		(struct rl_softc *);
static void rl_txeofcplus	(struct rl_softc *);
static void rl_intr		(void *);
static void rl_intrcplus	(void *);
static void rl_tick		(void *);
static void rl_start		(struct ifnet *);
static void rl_startcplus	(struct ifnet *);
static int rl_ioctl		(struct ifnet *, u_long, caddr_t);
static void rl_init		(void *);
static void rl_stop		(struct rl_softc *);
static void rl_watchdog		(struct ifnet *);
static int rl_suspend		(device_t);
static int rl_resume		(device_t);
static void rl_shutdown		(device_t);
static int rl_ifmedia_upd	(struct ifnet *);
static void rl_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void rl_eeprom_putbyte	(struct rl_softc *, int);
static void rl_eeprom_getword	(struct rl_softc *, int, u_int16_t *);
static void rl_read_eeprom	(struct rl_softc *, caddr_t, int, int, int);
static void rl_mii_sync		(struct rl_softc *);
static void rl_mii_send		(struct rl_softc *, u_int32_t, int);
static int rl_mii_readreg	(struct rl_softc *, struct rl_mii_frame *);
static int rl_mii_writereg	(struct rl_softc *, struct rl_mii_frame *);
static int rl_gmii_readreg	(device_t, int, int);
static int rl_gmii_writereg	(device_t, int, int, int);

static int rl_miibus_readreg	(device_t, int, int);
static int rl_miibus_writereg	(device_t, int, int, int);
static void rl_miibus_statchg	(device_t);

static u_int8_t rl_calchash	(caddr_t);
static void rl_setmulti		(struct rl_softc *);
static void rl_reset		(struct rl_softc *);
static int rl_list_tx_init	(struct rl_softc *);

static void rl_dma_map_rxbuf	(void *, bus_dma_segment_t *, int, int);
static void rl_dma_map_txbuf	(void *, bus_dma_segment_t *, int, int);

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

DRIVER_MODULE(rl, pci, rl_driver, rl_devclass, 0, 0);
DRIVER_MODULE(rl, cardbus, rl_driver, rl_devclass, 0, 0);
DRIVER_MODULE(miibus, rl, miibus_driver, miibus_devclass, 0, 0);

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

static void
rl_dma_map_rxbuf(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct rl_softc *sc;

	sc = arg;
	CSR_WRITE_4(sc, RL_RXADDR, segs->ds_addr & 0xFFFFFFFF);

	return;
}

static void
rl_dma_map_txbuf(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	struct rl_softc *sc;

	sc = arg;
	CSR_WRITE_4(sc, RL_CUR_TXADDR(sc), segs->ds_addr & 0xFFFFFFFF);

	return;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void
rl_eeprom_putbyte(sc, addr)
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

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
rl_eeprom_getword(sc, addr, dest)
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
static void
rl_read_eeprom(sc, dest, off, cnt, swap)
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
		CSR_READ_1(sc, RL_MII) | (x))

#define MII_CLR(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) & ~(x))

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
rl_mii_sync(sc)
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
static void
rl_mii_send(sc, bits, cnt)
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
static int
rl_mii_readreg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;

{
	int			i, ack;

	RL_LOCK(sc);

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
	ack = CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN;
	MII_SET(RL_MII_CLK);
	DELAY(1);

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

	RL_UNLOCK(sc);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
rl_mii_writereg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;

{
	RL_LOCK(sc);

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

	RL_UNLOCK(sc);

	return(0);
}

static int
rl_gmii_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	u_int32_t		rval;
	int			i;

	if (phy != 1)
		return(0);

	sc = device_get_softc(dev);

	CSR_WRITE_4(sc, RL_PHYAR, reg << 16);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (rval & RL_PHYAR_BUSY)
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("rl%d: PHY read failed\n", sc->rl_unit);
		return (0);
	}

	return (rval & RL_PHYAR_PHYDATA);
}

static int
rl_gmii_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	u_int32_t		rval;
	int			i;

	if (phy > 0)
		return(0);

	sc = device_get_softc(dev);

	CSR_WRITE_4(sc, RL_PHYAR, (reg << 16) |
	    (data | RL_PHYAR_PHYDATA) | RL_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (!(rval & RL_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("rl%d: PHY write failed\n", sc->rl_unit);
		return (0);
	}

	return (0);
}

static int
rl_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rval = 0;
	u_int16_t		rl8139_reg = 0;

	sc = device_get_softc(dev);
	RL_LOCK(sc);

	if (sc->rl_type == RL_8169) {
		rval = rl_gmii_readreg(dev, phy, reg);
		RL_UNLOCK(sc);
		return (rval);
	}

	if (sc->rl_type == RL_8139 || sc->rl_type == RL_8139CPLUS) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy) {
			RL_UNLOCK(sc);
			return(0);
		}
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
			RL_UNLOCK(sc);
			return(0);
		/*
		 * Allow the rlphy driver to read the media status
		 * register. If we have a link partner which does not
		 * support NWAY, this is the register which will tell
		 * us the results of parallel detection.
		 */
		case RL_MEDIASTAT:
			rval = CSR_READ_1(sc, RL_MEDIASTAT);
			RL_UNLOCK(sc);
			return(rval);
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			RL_UNLOCK(sc);
			return(0);
		}
		rval = CSR_READ_2(sc, rl8139_reg);
		RL_UNLOCK(sc);
		return(rval);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	rl_mii_readreg(sc, &frame);
	RL_UNLOCK(sc);

	return(frame.mii_data);
}

static int
rl_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct rl_softc		*sc;
	struct rl_mii_frame	frame;
	u_int16_t		rl8139_reg = 0;
	int			rval = 0;

	sc = device_get_softc(dev);
	RL_LOCK(sc);

	if (sc->rl_type == RL_8169) {
		rval = rl_gmii_writereg(dev, phy, reg, data);
		RL_UNLOCK(sc);
		return (rval);
	}

	if (sc->rl_type == RL_8139 || sc->rl_type == RL_8139CPLUS) {
		/* Pretend the internal PHY is only at address 0 */
		if (phy) {
			RL_UNLOCK(sc);
			return(0);
		}
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
			RL_UNLOCK(sc);
			return(0);
			break;
		default:
			printf("rl%d: bad phy register\n", sc->rl_unit);
			RL_UNLOCK(sc);
			return(0);
		}
		CSR_WRITE_2(sc, rl8139_reg, data);
		RL_UNLOCK(sc);
		return(0);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	rl_mii_writereg(sc, &frame);

	RL_UNLOCK(sc);
	return(0);
}

static void
rl_miibus_statchg(dev)
	device_t		dev;
{
	return;
}

/*
 * Calculate CRC of a multicast group address, return the upper 6 bits.
 */
static u_int8_t
rl_calchash(addr)
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
static void
rl_setmulti(sc)
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
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
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

static void
rl_reset(sc)
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

	CSR_WRITE_1(sc, 0x82, 1);

	return;
}

/*
 * Probe for a RealTek 8129/8139 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
rl_probe(dev)
	device_t		dev;
{
	struct rl_type		*t;
	struct rl_softc		*sc;
	struct rl_hwrev		*hw_rev;
	int			rid;
	u_int32_t		hwrev;
	char			desc[64];

	t = rl_devs;
	sc = device_get_softc(dev);

	while(t->rl_name != NULL) {
		if ((pci_get_vendor(dev) == t->rl_vid) &&
		    (pci_get_device(dev) == t->rl_did)) {

			/*
			 * Temporarily map the I/O space
			 * so we can read the chip ID register.
			 */
			rid = RL_RID;
			sc->rl_res = bus_alloc_resource(dev, RL_RES, &rid,
			    0, ~0, 1, RF_ACTIVE);
			if (sc->rl_res == NULL) {
				device_printf(dev,
				    "couldn't map ports/memory\n");
				return(ENXIO);
			}
			sc->rl_btag = rman_get_bustag(sc->rl_res);
			sc->rl_bhandle = rman_get_bushandle(sc->rl_res);
			mtx_init(&sc->rl_mtx,
			    device_get_nameunit(dev),
			    MTX_NETWORK_LOCK, MTX_DEF);
			RL_LOCK(sc);
			if (t->rl_basetype == RL_8139) {
				hwrev = CSR_READ_4(sc, RL_TXCFG) &
				    RL_TXCFG_HWREV;
				hw_rev = rl_hwrevs;
				while (hw_rev->rl_desc != NULL) {
					if (hw_rev->rl_rev == hwrev) {
						sprintf(desc, "%s, rev. %s",
						    t->rl_name,
						    hw_rev->rl_desc);
						sc->rl_type = hw_rev->rl_type;
						break;
					}
					hw_rev++;
				}
				if (hw_rev->rl_desc == NULL) 
					sprintf(desc, "%s, rev. %s",
					    t->rl_name, "unknown");
			} else
				sprintf(desc, "%s", t->rl_name);
			bus_release_resource(dev, RL_RES,
			    RL_RID, sc->rl_res);
			RL_UNLOCK(sc);
			mtx_destroy(&sc->rl_mtx);
			device_set_desc_copy(dev, desc);
			return(0);
		}
		t++;
	}

	return(ENXIO);
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
rl_dma_map_desc(arg, segs, nseg, mapsize, error)
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
	while(1) {
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

	return;
}

/*
 * Map a single buffer address.
 */

static void
rl_dma_map_addr(arg, segs, nseg, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	int			error;
{
	u_int32_t		*addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;

	return;
}

static int
rl_allocmem(dev, sc)
	device_t		dev;
	struct rl_softc		*sc;
{
	int error;

	/*
	 * Now allocate a tag for the DMA descriptor lists.
	 * All of our lists are allocated as a contiguous block
	 * of memory.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			RL_RXBUFLEN + 1518, 1,	/* maxsize,nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->rl_tag);
	if (error)
		return(error);

	/*
	 * Now allocate a chunk of DMA-able memory based on the
	 * tag we just created.
	 */
	error = bus_dmamem_alloc(sc->rl_tag,
	    (void **)&sc->rl_cdata.rl_rx_buf, BUS_DMA_NOWAIT,
	    &sc->rl_cdata.rl_rx_dmamap);

	if (error) {
		printf("rl%d: no memory for list buffers!\n", sc->rl_unit);
		bus_dma_tag_destroy(sc->rl_tag);
		sc->rl_tag = NULL;
		return(error);
	}

	/* Leave a few bytes before the start of the RX ring buffer. */
	sc->rl_cdata.rl_rx_buf_ptr = sc->rl_cdata.rl_rx_buf;
	sc->rl_cdata.rl_rx_buf += sizeof(u_int64_t);

	return(0);
}

static int
rl_allocmemcplus(dev, sc)
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
	    NULL, MCLBYTES * nseg, nseg, MCLBYTES, 0, NULL, NULL,
	    &sc->rl_ldata.rl_mtag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Allocate map for TX descriptor list.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag, RL_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
            NULL, RL_TX_LIST_SZ, 1, RL_TX_LIST_SZ, 0, NULL, NULL,
	    &sc->rl_ldata.rl_tx_list_tag);
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
	     RL_TX_LIST_SZ, rl_dma_map_addr,
	     &sc->rl_ldata.rl_tx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for TX buffers */

	for (i = 0; i < RL_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->rl_ldata.rl_mtag, 0,
			    &sc->rl_ldata.rl_tx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for TX\n");
			return(ENOMEM);
		}
	}

	/*
	 * Allocate map for RX descriptor list.
	 */
	error = bus_dma_tag_create(sc->rl_parent_tag, RL_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
            NULL, RL_TX_LIST_SZ, 1, RL_TX_LIST_SZ, 0, NULL, NULL,
	    &sc->rl_ldata.rl_rx_list_tag);
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
	     RL_TX_LIST_SZ, rl_dma_map_addr,
	     &sc->rl_ldata.rl_rx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for RX buffers */

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->rl_ldata.rl_mtag, 0,
			    &sc->rl_ldata.rl_rx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			return(ENOMEM);
		}
	}

	return(0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
rl_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		as[3];
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	struct rl_type		*t;
	struct rl_hwrev		*hw_rev;
	int			hwrev;
	u_int16_t		rl_did = 0;
	int			unit, error = 0, rid, i;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->rl_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
#ifndef BURN_BRIDGES
	/*
	 * Handle power management nonsense.
	 */

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		u_int32_t		iobase, membase, irq;

		/* Save important PCI config data. */
		iobase = pci_read_config(dev, RL_PCI_LOIO, 4);
		membase = pci_read_config(dev, RL_PCI_LOMEM, 4);
		irq = pci_read_config(dev, RL_PCI_INTLINE, 4);

		/* Reset the power state. */
		printf("rl%d: chip is is in D%d power mode "
		    "-- setting to D0\n", unit,
		    pci_get_powerstate(dev));

		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		/* Restore PCI config data. */
		pci_write_config(dev, RL_PCI_LOIO, iobase, 4);
		pci_write_config(dev, RL_PCI_LOMEM, membase, 4);
		pci_write_config(dev, RL_PCI_INTLINE, irq, 4);
	}
#endif
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = RL_RID;
	sc->rl_res = bus_alloc_resource(dev, RL_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->rl_res == NULL) {
		printf ("rl%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

#ifdef notdef
	/* Detect the Realtek 8139B. For some reason, this chip is very
	 * unstable when left to autoselect the media
	 * The best workaround is to set the device to the required
	 * media type or to set it to the 10 Meg speed.
	 */

	if ((rman_get_end(sc->rl_res)-rman_get_start(sc->rl_res))==0xff) {
		printf("rl%d: Realtek 8139B detected. Warning,"
		    " this may be unstable in autoselect mode\n", unit);
	}
#endif

	sc->rl_btag = rman_get_bustag(sc->rl_res);
	sc->rl_bhandle = rman_get_bushandle(sc->rl_res);

	/* Allocate interrupt */
	rid = 0;
	sc->rl_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->rl_irq == NULL) {
		printf("rl%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	rl_reset(sc);
	sc->rl_eecmd_read = RL_EECMD_READ_6BIT;
	rl_read_eeprom(sc, (caddr_t)&rl_did, 0, 1, 0);
	if (rl_did != 0x8129)
		sc->rl_eecmd_read = RL_EECMD_READ_8BIT;

	/*
	 * Get station address from the EEPROM.
	 */
	rl_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3, 0);
	for (i = 0; i < 3; i++) {
		eaddr[(i * 2) + 0] = as[i] & 0xff;
		eaddr[(i * 2) + 1] = as[i] >> 8;
	}

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

	t = rl_devs;
	while(t->rl_name != NULL) {
		if (rl_did == t->rl_did) {
			sc->rl_type = t->rl_basetype;
			break;
		}
		t++;
	}
	if (t->rl_name == NULL) {
		printf("rl%d: unknown device ID: %x\n", unit, rl_did);
		error = ENXIO;
		goto fail;
	}
	if (sc->rl_type == RL_8139) {
		hw_rev = rl_hwrevs;
		hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
		while (hw_rev->rl_desc != NULL) {
			if (hw_rev->rl_rev == hwrev) {
				sc->rl_type = hw_rev->rl_type;
				break;
			}
			hw_rev++;
		}
		if (hw_rev->rl_desc == NULL) {
			printf("rl%d: unknown hwrev: %x\n", unit, hwrev);
		}
	} else if (rl_did == RT_DEVICEID_8129) {
		sc->rl_type = RL_8129;
	} else if (rl_did == RT_DEVICEID_8169) {
		sc->rl_type = RL_8169;
	}

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

	/*
	 * If this is an 8139C+ or 8169 chip, we have to allocate
	 * our busdma tags/memory differently. We need to allocate
	 * a chunk of DMA'able memory for the RX and TX descriptor
	 * lists.
	 */
	if (sc->rl_type == RL_8139CPLUS || sc->rl_type == RL_8169)
		error = rl_allocmemcplus(dev, sc);
	else
		error = rl_allocmem(dev, sc);

	if (error)
		goto fail;

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->rl_miibus,
	    rl_ifmedia_upd, rl_ifmedia_sts)) {
		printf("rl%d: MII without any phy!\n", sc->rl_unit);
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
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	if (RL_ISCPLUS(sc)) {
		ifp->if_start = rl_startcplus;
		ifp->if_hwassist = RL_CSUM_FEATURES;
		ifp->if_capabilities |= IFCAP_HWCSUM|IFCAP_VLAN_HWTAGGING;
	} else
		ifp->if_start = rl_start;
	ifp->if_watchdog = rl_watchdog;
	ifp->if_init = rl_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = RL_IFQ_MAXLEN;
	ifp->if_capenable = ifp->if_capabilities;

	callout_handle_init(&sc->rl_stat_ch);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->rl_irq, INTR_TYPE_NET,
	    RL_ISCPLUS(sc) ? rl_intrcplus : rl_intr, sc, &sc->rl_intrhand);

	if (error) {
		printf("rl%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		rl_detach(dev);

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
rl_detach(dev)
	device_t		dev;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	int			i;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->rl_mtx), ("rl mutex not initialized"));
	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		rl_stop(sc);
		ether_ifdetach(ifp);
	}
	if (sc->rl_miibus)
		device_delete_child(dev, sc->rl_miibus);
	bus_generic_detach(dev);

	if (sc->rl_intrhand)
		bus_teardown_intr(dev, sc->rl_irq, sc->rl_intrhand);
	if (sc->rl_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->rl_irq);
	if (sc->rl_res)
		bus_release_resource(dev, RL_RES, RL_RID, sc->rl_res);

	if (RL_ISCPLUS(sc)) {

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

	} else {
		if (sc->rl_tag) {
			bus_dmamap_unload(sc->rl_tag,
			    sc->rl_cdata.rl_rx_dmamap);
			bus_dmamem_free(sc->rl_tag, sc->rl_cdata.rl_rx_buf,
			    sc->rl_cdata.rl_rx_dmamap);
			bus_dma_tag_destroy(sc->rl_tag);
		}
	}

	if (sc->rl_parent_tag)
		bus_dma_tag_destroy(sc->rl_parent_tag);

	RL_UNLOCK(sc);
	mtx_destroy(&sc->rl_mtx);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
rl_list_tx_init(sc)
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

static int
rl_newbuf (sc, idx, m)
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
			return(ENOBUFS);
		m = n;
	} else
		m->m_data = m->m_ext.ext_buf;

	/*
	 * Initialize mbuf length fields and fixup
	 * alignment so that the frame payload is
	 * longword aligned.
	 */
	m->m_len = m->m_pkthdr.len = 1536;
	m_adj(m, ETHER_ALIGN);

	arg.sc = sc;
	arg.rl_idx = idx;
	arg.rl_maxsegs = 1;
	arg.rl_flags = 0;
	arg.rl_ring = sc->rl_ldata.rl_rx_list;

        error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag,
	    sc->rl_ldata.rl_rx_dmamap[idx], m, rl_dma_map_desc,
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

	return(0);
}

static int
rl_tx_list_init(sc)
	struct rl_softc		*sc;
{
	bzero ((char *)sc->rl_ldata.rl_tx_list, RL_TX_LIST_SZ);
	bzero ((char *)&sc->rl_ldata.rl_tx_mbuf,
	    (RL_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->rl_ldata.rl_tx_list_tag,
	    sc->rl_ldata.rl_tx_list_map, BUS_DMASYNC_PREWRITE);
	sc->rl_ldata.rl_tx_prodidx = 0;
	sc->rl_ldata.rl_tx_considx = 0;
	sc->rl_ldata.rl_tx_free = RL_TX_DESC_CNT;

	return(0);
}

static int
rl_rx_list_init(sc)
	struct rl_softc		*sc;
{
	int			i;

	bzero ((char *)sc->rl_ldata.rl_rx_list, RL_RX_LIST_SZ);
	bzero ((char *)&sc->rl_ldata.rl_rx_mbuf,
	    (RL_RX_DESC_CNT * sizeof(struct mbuf *)));

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (rl_newbuf(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = 0;

	return(0);
}

/*
 * RX handler for C+. This is pretty much like any other
 * descriptor-based RX handler.
 */
static void
rl_rxeofcplus(sc)
	struct rl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	struct rl_desc		*cur_rx;
	u_int32_t		rxstat, rxvlan;

	ifp = &sc->arpcom.ac_if;
	i = sc->rl_ldata.rl_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (!RL_OWN(&sc->rl_ldata.rl_rx_list[i])) {

		cur_rx = &sc->rl_ldata.rl_rx_list[i];
		m = sc->rl_ldata.rl_rx_mbuf[i];
		total_len = RL_RXBYTES(cur_rx) - ETHER_CRC_LEN;
		rxstat = le32toh(cur_rx->rl_cmdstat);
		rxvlan = le32toh(cur_rx->rl_vlanctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->rl_ldata.rl_mtag,
		    sc->rl_ldata.rl_rx_dmamap[i],
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rl_ldata.rl_mtag,
		    sc->rl_ldata.rl_rx_dmamap[i]);

		/*
		 * NOTE: For some reason that I can't comprehend,
		 * the RealTek engineers decided not to implement
		 * the 'frame alignment error' bit in the 8169's
		 * status word. Unfortunately, rather than simply
		 * mark the bit as 'reserved,' they took it away
		 * completely and shifted the other status bits
		 * over one slot. The OWN, EOR, FS and LS bits are
		 * still in the same places, as is the frame length
		 * field. We have already extracted the frame length
		 * and checked the OWN bit, so to work around this
		 * problem, we shift the status bits one space to
		 * the right so that we can evaluate everything else
		 * correctly.
		 */
		if (sc->rl_type == RL_8169)
			rxstat >>= 1;

		if (rxstat & RL_RDESC_STAT_RXERRSUM) {
			ifp->if_ierrors++;
			rl_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (rl_newbuf(sc, i, NULL)) {
			ifp->if_ierrors++;
			rl_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		RL_DESC_INC(i);

		ifp->if_ipackets++;
		m->m_pkthdr.len = m->m_len = total_len;
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
		(*ifp->if_input)(ifp, m);
	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->rl_ldata.rl_rx_list_tag,
	    sc->rl_ldata.rl_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = i;

	return;
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
 * is preceded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 *
 * Note: to make the Alpha happy, the frame payload needs to be aligned
 * on a 32-bit boundary. To achieve this, we pass RL_ETHER_ALIGN (2 bytes)
 * as the offset argument to m_devget().
 */
static void
rl_rxeof(sc)
	struct rl_softc		*sc;
{
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

	bus_dmamap_sync(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
	    BUS_DMASYNC_POSTREAD);

	cur_rx = (CSR_READ_2(sc, RL_CURRXADDR) + 16) % RL_RXBUFLEN;

	/* Do not try to read past this point. */
	limit = CSR_READ_2(sc, RL_CURRXBUF) % RL_RXBUFLEN;

	if (limit < cur_rx)
		max_bytes = (RL_RXBUFLEN - cur_rx) + limit;
	else
		max_bytes = limit - cur_rx;

	while((CSR_READ_1(sc, RL_COMMAND) & RL_CMD_EMPTY_RXBUF) == 0) {
#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		rxbufpos = sc->rl_cdata.rl_rx_buf + cur_rx;
		rxstat = le32toh(*(u_int32_t *)rxbufpos);

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
			m = m_devget(rxbufpos, total_len, RL_ETHER_ALIGN, ifp,
			    NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
			} else {
				m_copyback(m, wrap, total_len - wrap,
					sc->rl_cdata.rl_rx_buf);
			}
			cur_rx = (total_len - wrap + ETHER_CRC_LEN);
		} else {
			m = m_devget(rxbufpos, total_len, RL_ETHER_ALIGN, ifp,
			    NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
			}
			cur_rx += total_len + 4 + ETHER_CRC_LEN;
		}

		/*
		 * Round up to 32-bit boundary.
		 */
		cur_rx = (cur_rx + 3) & ~3;
		CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);

		if (m == NULL)
			continue;

		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, m);
	}

	return;
}

static void
rl_txeofcplus(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = &sc->arpcom.ac_if;
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
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void
rl_txeof(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;

	ifp = &sc->arpcom.ac_if;

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
			bus_dmamap_unload(sc->rl_tag, RL_LAST_DMAMAP(sc));
			bus_dmamap_destroy(sc->rl_tag, RL_LAST_DMAMAP(sc));
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

	ifp->if_timer =
	    (sc->rl_cdata.last_tx == sc->rl_cdata.cur_tx) ? 0 : 5;

	return;
}

static void
rl_tick(xsc)
	void			*xsc;
{
	struct rl_softc		*sc;
	struct mii_data		*mii;

	sc = xsc;
	RL_LOCK(sc);
	mii = device_get_softc(sc->rl_miibus);

	mii_tick(mii);

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);
	RL_UNLOCK(sc);

	return;
}

#ifdef DEVICE_POLLING
static void
rl_poll (struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK(sc);
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		if (RL_ISCPLUS(sc))
			CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
		else
			CSR_WRITE_2(sc, RL_IMR, RL_INTRS);
		goto done;
	}

	sc->rxcycles = count;
	if (RL_ISCPLUS(sc)) {
		rl_rxeofcplus(sc);
		rl_txeofcplus(sc);
	} else {
		rl_rxeof(sc);
		rl_txeof(sc);
	}

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int16_t       status;

		status = CSR_READ_2(sc, RL_ISR);
		if (status == 0xffff)
			goto done;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}
	}
done:
	RL_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
rl_intrcplus(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;

	if (sc->suspended) {
		return;
	}

	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done;
	if (ether_poll_register(rl_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RL_IMR, 0x0000);
		rl_poll(ifp, 0, 1);
		goto done;
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

		if (status & RL_ISR_RX_OK)
			rl_rxeofcplus(sc);

		if (status & RL_ISR_RX_ERR)
			rl_rxeofcplus(sc);

		if ((status & RL_ISR_TIMEOUT_EXPIRED) ||
		    (status & RL_ISR_TX_ERR) ||
		    (status & RL_ISR_TX_DESC_UNAVAIL))
			rl_txeofcplus(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}

	}

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

#ifdef DEVICE_POLLING
done:
#endif
	RL_UNLOCK(sc);

	return;
}

static void
rl_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;

	if (sc->suspended) {
		return;
	}

	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done;
	if (ether_poll_register(rl_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RL_IMR, 0x0000);
		rl_poll(ifp, 0, 1);
		goto done;
	}
#endif /* DEVICE_POLLING */

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
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

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

#ifdef DEVICE_POLLING
done:
#endif
	RL_UNLOCK(sc);

	return;
}

static int
rl_encapcplus(sc, m_head, idx)
	struct rl_softc		*sc;
	struct mbuf		*m_head;
	int			*idx;
{
	struct mbuf		*m_new = NULL;
	struct rl_dmaload_arg	arg;
	bus_dmamap_t		map;
	int			error;
	struct m_tag		*mtag;

	if (sc->rl_ldata.rl_tx_free < 4)
		return(EFBIG);

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. (This is according to testing done with an 8169
	 * chip. I'm not sure if this is a requirement or a bug.)
	 */

	arg.rl_flags = 0;

	if (m_head->m_pkthdr.csum_flags & CSUM_IP)
		arg.rl_flags |= RL_TDESC_CMD_IPCSUM;
	if (m_head->m_pkthdr.csum_flags & CSUM_TCP)
		arg.rl_flags |= RL_TDESC_CMD_TCPCSUM;
	if (m_head->m_pkthdr.csum_flags & CSUM_UDP)
		arg.rl_flags |= RL_TDESC_CMD_UDPCSUM;

	arg.sc = sc;
	arg.rl_idx = *idx;
	arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
	arg.rl_ring = sc->rl_ldata.rl_tx_list;

	map = sc->rl_ldata.rl_tx_dmamap[*idx];
	error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
	    m_head, rl_dma_map_desc, &arg, BUS_DMA_NOWAIT);

	if (error && error != EFBIG) {
		printf("rl%d: can't map mbuf (error %d)\n", sc->rl_unit, error);
		return(ENOBUFS);
	}

	/* Too many segments to map, coalesce into a single mbuf */

	if (error || arg.rl_maxsegs == 0) {
		m_new = m_defrag(m_head, M_DONTWAIT);
		if (m_new == NULL)
			return(1);
		else
			m_head = m_new;

		arg.sc = sc;
		arg.rl_idx = *idx;
		arg.rl_maxsegs = sc->rl_ldata.rl_tx_free;
		arg.rl_ring = sc->rl_ldata.rl_tx_list;

		error = bus_dmamap_load_mbuf(sc->rl_ldata.rl_mtag, map,
		    m_head, rl_dma_map_desc, &arg, BUS_DMA_NOWAIT);
		if (error) {
			printf("rl%d: can't map mbuf (error %d)\n",
			    sc->rl_unit, error);
			return(EFBIG);
		}
	}

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->rl_ldata.rl_tx_dmamap[*idx] =
	    sc->rl_ldata.rl_tx_dmamap[arg.rl_idx];
	sc->rl_ldata.rl_tx_dmamap[arg.rl_idx] = map;

	sc->rl_ldata.rl_tx_mbuf[arg.rl_idx] = m_head;
	sc->rl_ldata.rl_tx_free -= arg.rl_maxsegs;

	/*
	 * Set up hardware VLAN tagging. Note: vlan tag info must
	 * appear in the first descriptor of a multi-descriptor
	 * transmission attempt.
	 */

	mtag = VLAN_OUTPUT_TAG(&sc->arpcom.ac_if, m_head);
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

	return(0);
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */

static void
rl_startcplus(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;
	int			idx;

	sc = ifp->if_softc;
	RL_LOCK(sc);

	idx = sc->rl_ldata.rl_tx_prodidx;

	while (sc->rl_ldata.rl_tx_mbuf[idx] == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (rl_encapcplus(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

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

	RL_UNLOCK(sc);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
rl_encap(sc, m_head)
	struct rl_softc		*sc;
	struct mbuf		*m_head;
{
	struct mbuf		*m_new = NULL;

	/*
	 * The RealTek is brain damaged and wants longword-aligned
	 * TX buffers, plus we can only have one fragment buffer
	 * per packet. We have to copy pretty much all the time.
	 */
	m_new = m_defrag(m_head, M_DONTWAIT);

	if (m_new == NULL) {
		m_freem(m_head);
		return(1);
	}
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

static void
rl_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;
	RL_LOCK(sc);

	while(RL_CUR_TXMBUF(sc) == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (rl_encap(sc, m_head)) {
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, RL_CUR_TXMBUF(sc));

		/*
		 * Transmit the frame.
		 */
		bus_dmamap_create(sc->rl_tag, 0, &RL_CUR_DMAMAP(sc));
		bus_dmamap_load(sc->rl_tag, RL_CUR_DMAMAP(sc),
		    mtod(RL_CUR_TXMBUF(sc), void *),
		    RL_CUR_TXMBUF(sc)->m_pkthdr.len, rl_dma_map_txbuf,
		    sc, BUS_DMA_NOWAIT);
		bus_dmamap_sync(sc->rl_tag, RL_CUR_DMAMAP(sc),
		    BUS_DMASYNC_PREREAD);
		CSR_WRITE_4(sc, RL_CUR_TXSTAT(sc),
		    RL_TXTHRESH(sc->rl_txthresh) |
		    RL_CUR_TXMBUF(sc)->m_pkthdr.len);

		RL_INC(sc->rl_cdata.cur_tx);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}

	/*
	 * We broke out of the loop because all our TX slots are
	 * full. Mark the NIC as busy until it drains some of the
	 * packets from the queue.
	 */
	if (RL_CUR_TXMBUF(sc) != NULL)
		ifp->if_flags |= IFF_OACTIVE;

	RL_UNLOCK(sc);

	return;
}

static void
rl_init(xsc)
	void			*xsc;
{
	struct rl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	u_int32_t		rxcfg = 0;

	RL_LOCK(sc);
	mii = device_get_softc(sc->rl_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rl_stop(sc);

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	CSR_WRITE_4(sc, RL_IDR0, *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	CSR_WRITE_4(sc, RL_IDR4, *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	if (RL_ISCPLUS(sc)) {
		rl_rx_list_init(sc);
		rl_tx_list_init(sc);
	} else {

		/* Init the RX buffer pointer register. */
		bus_dmamap_load(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
		    sc->rl_cdata.rl_rx_buf, RL_RXBUFLEN,
		    rl_dma_map_rxbuf, sc, BUS_DMA_NOWAIT);
		bus_dmamap_sync(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap,
		    BUS_DMASYNC_PREWRITE);

		/* Init TX descriptors. */
		rl_list_tx_init(sc);
	}

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
	if (RL_ISCPLUS(sc))
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
	else
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	/* Set initial TX threshold */
	sc->rl_txthresh = RL_TX_THRESH_INIT;

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);
#endif
	/*
	 * If this is a C+ capable chip, enable C+ RX and TX mode,
	 * and load the addresses of the RX and TX lists into the chip.
	 */
	if (RL_ISCPLUS(sc)) {
		CSR_WRITE_2(sc, RL_CPLUS_CMD, RL_CPLUSCMD_RXENB|
		    RL_CPLUSCMD_TXENB|RL_CPLUSCMD_PCI_MRW|
		    RL_CPLUSCMD_VLANSTRIP|
		    (ifp->if_capenable & IFCAP_RXCSUM ?
		    RL_CPLUSCMD_RXCSUM_ENB : 0));

		CSR_WRITE_4(sc, RL_RXLIST_ADDR_HI,
		    RL_ADDR_HI(sc->rl_ldata.rl_rx_list_addr));
		CSR_WRITE_4(sc, RL_RXLIST_ADDR_LO,
		    RL_ADDR_LO(sc->rl_ldata.rl_rx_list_addr));

		CSR_WRITE_4(sc, RL_TXLIST_ADDR_HI,
		    RL_ADDR_HI(sc->rl_ldata.rl_tx_list_addr));
		CSR_WRITE_4(sc, RL_TXLIST_ADDR_LO,
		    RL_ADDR_LO(sc->rl_ldata.rl_tx_list_addr));

		CSR_WRITE_1(sc, RL_EARLY_TX_THRESH, RL_EARLYTXTHRESH_CNT);

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
			CSR_WRITE_2(sc, RL_MAXRXPKTLEN, RL_PKTSZ(16384));

	}

	mii_mediachg(mii);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->rl_stat_ch = timeout(rl_tick, sc, hz);
	RL_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
rl_ifmedia_upd(ifp)
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
static void
rl_ifmedia_sts(ifp, ifmr)
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

static int
rl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	RL_LOCK(sc);

	switch(command) {
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
	case SIOCSIFCAP:
		ifp->if_capenable = ifr->ifr_reqcap;
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = RL_CSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
		if (ifp->if_flags & IFF_RUNNING)
			rl_init(sc);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	RL_UNLOCK(sc);

	return(error);
}

static void
rl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;
	RL_LOCK(sc);
	printf("rl%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;

	if (RL_ISCPLUS(sc)) {
		rl_txeofcplus(sc);
		rl_rxeofcplus(sc);
	} else {
		rl_txeof(sc);
		rl_rxeof(sc);
	}

	rl_init(sc);

	RL_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
rl_stop(sc)
	struct rl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	RL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(rl_tick, sc, sc->rl_stat_ch);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	if (RL_ISCPLUS(sc)) {

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

	} else {

		bus_dmamap_unload(sc->rl_tag, sc->rl_cdata.rl_rx_dmamap);

		/*
		 * Free the TX list buffers.
		 */
		for (i = 0; i < RL_TX_LIST_CNT; i++) {
			if (sc->rl_cdata.rl_tx_chain[i] != NULL) {
				bus_dmamap_unload(sc->rl_tag,
				    sc->rl_cdata.rl_tx_dmamap[i]);
				bus_dmamap_destroy(sc->rl_tag,
				    sc->rl_cdata.rl_tx_dmamap[i]);
				m_freem(sc->rl_cdata.rl_tx_chain[i]);
				sc->rl_cdata.rl_tx_chain[i] = NULL;
				CSR_WRITE_4(sc, RL_TXADDR0 + i, 0x0000000);
			}
		}
	}

	RL_UNLOCK(sc);
	return;
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
rl_suspend(dev)
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
static int
rl_resume(dev)
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

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
rl_shutdown(dev)
	device_t		dev;
{
	struct rl_softc		*sc;

	sc = device_get_softc(dev);

	rl_stop(sc);

	return;
}
