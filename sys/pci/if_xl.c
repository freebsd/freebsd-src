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
 *	$Id: if_xl.c,v 1.43 1999/07/07 21:49:14 wpaul Exp $
 */

/*
 * 3Com 3c90x Etherlink XL PCI NIC driver
 *
 * Supports the 3Com "boomerang", "cyclone" and "hurricane" PCI
 * bus-master chips (3c90x cards and embedded controllers) including
 * the following:
 *
 * 3Com 3c900-TPO	10Mbps/RJ-45
 * 3Com 3c900-COMBO	10Mbps/RJ-45,AUI,BNC
 * 3Com 3c905-TX	10/100Mbps/RJ-45
 * 3Com 3c905-T4	10/100Mbps/RJ-45
 * 3Com 3c900B-TPO	10Mbps/RJ-45
 * 3Com 3c900B-COMBO	10Mbps/RJ-45,AUI,BNC
 * 3Com 3c900B-TPC	10Mbps/RJ-45,BNC
 * 3Com 3c900B-FL	10Mbps/Fiber-optic
 * 3Com 3c905B-COMBO	10/100Mbps/RJ-45,AUI,BNC
 * 3Com 3c905B-TX	10/100Mbps/RJ-45
 * 3Com 3c905B-FL/FX	10/100Mbps/Fiber-optic
 * 3Com 3c905C-TX	10/100Mbps/RJ-45
 * 3Com 3c980-TX	10/100Mbps server adapter
 * 3Com 3cSOHO100-TX	10/100Mbps/RJ-45
 * Dell Optiplex GX1 on-board 3c918 10/100Mbps/RJ-45
 * Dell Precision on-board 3c905B 10/100Mbps/RJ-45
 * Dell Latitude laptop docking station embedded 3c905-TX
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The 3c90x series chips use a bus-master DMA interface for transfering
 * packets to and from the controller chip. Some of the "vortex" cards
 * (3c59x) also supported a bus master mode, however for those chips
 * you could only DMA packets to/from a contiguous memory buffer. For
 * transmission this would mean copying the contents of the queued mbuf
 * chain into a an mbuf cluster and then DMAing the cluster. This extra
 * copy would sort of defeat the purpose of the bus master support for
 * any packet that doesn't fit into a single mbuf.
 *
 * By contrast, the 3c90x cards support a fragment-based bus master
 * mode where mbuf chains can be encapsulated using TX descriptors.
 * This is similar to other PCI chips such as the Texas Instruments
 * ThunderLAN and the Intel 82557/82558.
 *
 * The "vortex" driver (if_vx.c) happens to work for the "boomerang"
 * bus master chips because they maintain the old PIO interface for
 * backwards compatibility, but starting with the 3c905B and the
 * "cyclone" chips, the compatibility interface has been dropped.
 * Since using bus master DMA is a big win, we use this driver to
 * support the PCI "boomerang" chips even though they work with the
 * "vortex" driver in order to obtain better performance.
 *
 * This driver is in the /sys/pci directory because it only supports
 * PCI-based NICs.
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

#include "opt_bdg.h"
#ifdef BRIDGE
#include <net/bridge.h>
#endif

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

/*
 * The following #define causes the code to use PIO to access the
 * chip's registers instead of memory mapped mode. The reason PIO mode
 * is on by default is that the Etherlink XL manual seems to indicate
 * that only the newer revision chips (3c905B) support both PIO and
 * memory mapped access. Since we want to be compatible with the older
 * bus master chips, we use PIO here. If you comment this out, the
 * driver will use memory mapped I/O, which may be faster but which
 * might not work on some devices.
 */
#define XL_USEIOSPACE

/*
 * This #define controls the behavior of autonegotiation during the
 * bootstrap phase. It's possible to have the driver initiate an
 * autonegotiation session and then set a timeout which will cause the
 * autoneg results to be polled later, usually once the kernel has
 * finished booting. This is clever and all, but it can have bad side
 * effects in some cases, particularly where NFS is involved. For
 * example, if we're booting diskless with an NFS rootfs, the network
 * interface has to be up and running before we hit the mountroot()
 * code, otherwise mounting the rootfs will fail and we'll probably
 * panic.
 *
 * Consequently, the 'backgrounded' autoneg behavior is turned off
 * by default and we actually sit and wait 5 seconds for autonegotiation
 * to complete before proceeding with the other device probes. If you
 * choose to use the other behavior, you can uncomment this #define and
 * recompile.
 */
/* #define XL_BACKGROUND_AUTONEG */

#include <pci/if_xlreg.h>

#if !defined(lint)
static const char rcsid[] =
	"$Id: if_xl.c,v 1.43 1999/07/07 21:49:14 wpaul Exp $";
#endif

/*
 * Various supported device vendors/types and their names.
 */
static struct xl_type xl_devs[] = {
	{ TC_VENDORID, TC_DEVICEID_BOOMERANG_10BT,
		"3Com 3c900-TPO Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_BOOMERANG_10BT_COMBO,
		"3Com 3c900-COMBO Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_BOOMERANG_10_100BT,
		"3Com 3c905-TX Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_BOOMERANG_100BT4,
		"3Com 3c905-T4 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_KRAKATOA_10BT,
		"3Com 3c900B-TPO Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_KRAKATOA_10BT_COMBO,
		"3Com 3c900B-COMBO Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_KRAKATOA_10BT_TPC,
		"3Com 3c900B-TPC Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_CYCLONE_10FL,
		"3Com 3c900B-FL Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_10_100BT,
		"3Com 3c905B-TX Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_CYCLONE_10_100BT4,
		"3Com 3c905B-T4 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_CYCLONE_10_100FX,
		"3Com 3c905B-FX/SC Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_CYCLONE_10_100_COMBO,
		"3Com 3c905B-COMBO Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_TORNADO_10_100BT,
		"3Com 3c905C-TX Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_10_100BT_SERV,
		"3Com 3c980 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_SOHO100TX,
		"3Com 3cSOHO100-TX OfficeConnect" },
	{ 0, 0, NULL }
};

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

static struct xl_type xl_phys[] = {
	{ TI_PHY_VENDORID, TI_PHY_10BT, "<TI ThunderLAN 10BT (internal)>" },
	{ TI_PHY_VENDORID, TI_PHY_100VGPMI, "<TI TNETE211 100VG Any-LAN>" },
	{ NS_PHY_VENDORID, NS_PHY_83840A, "<National Semiconductor DP83840A>"},
	{ LEVEL1_PHY_VENDORID, LEVEL1_PHY_LXT970, "<Level 1 LXT970>" }, 
	{ INTEL_PHY_VENDORID, INTEL_PHY_82555, "<Intel 82555>" },
	{ SEEQ_PHY_VENDORID, SEEQ_PHY_80220, "<SEEQ 80220>" },
	{ 0, 0, "<MII-compliant physical interface>" }
};

static unsigned long xl_count = 0;
static const char *xl_probe	__P((pcici_t, pcidi_t));
static void xl_attach		__P((pcici_t, int));

static int xl_newbuf		__P((struct xl_softc *,
						struct xl_chain_onefrag *));
static void xl_stats_update	__P((void *));
static int xl_encap		__P((struct xl_softc *, struct xl_chain *,
						struct mbuf * ));

static void xl_rxeof		__P((struct xl_softc *));
static void xl_txeof		__P((struct xl_softc *));
static void xl_txeoc		__P((struct xl_softc *));
static void xl_intr		__P((void *));
static void xl_start		__P((struct ifnet *));
static int xl_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void xl_init		__P((void *));
static void xl_stop		__P((struct xl_softc *));
static void xl_watchdog		__P((struct ifnet *));
static void xl_shutdown		__P((int, void *));
static int xl_ifmedia_upd	__P((struct ifnet *));
static void xl_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static int xl_eeprom_wait	__P((struct xl_softc *));
static int xl_read_eeprom	__P((struct xl_softc *, caddr_t, int,
							int, int));
static void xl_mii_sync		__P((struct xl_softc *));
static void xl_mii_send		__P((struct xl_softc *, u_int32_t, int));
static int xl_mii_readreg	__P((struct xl_softc *, struct xl_mii_frame *));
static int xl_mii_writereg	__P((struct xl_softc *, struct xl_mii_frame *));
static u_int16_t xl_phy_readreg	__P((struct xl_softc *, int));
static void xl_phy_writereg	__P((struct xl_softc *, int, int));

static void xl_autoneg_xmit	__P((struct xl_softc *));
static void xl_autoneg_mii	__P((struct xl_softc *, int, int));
static void xl_setmode_mii	__P((struct xl_softc *, int));
static void xl_getmode_mii	__P((struct xl_softc *));
static void xl_setmode		__P((struct xl_softc *, int));
static u_int8_t xl_calchash	__P((caddr_t));
static void xl_setmulti		__P((struct xl_softc *));
static void xl_setmulti_hash	__P((struct xl_softc *));
static void xl_reset		__P((struct xl_softc *));
static int xl_list_rx_init	__P((struct xl_softc *));
static int xl_list_tx_init	__P((struct xl_softc *));
static void xl_wait		__P((struct xl_softc *));
static void xl_mediacheck	__P((struct xl_softc *));
#ifdef notdef
static void xl_testpacket	__P((struct xl_softc *));
#endif

/*
 * Murphy's law says that it's possible the chip can wedge and
 * the 'command in progress' bit may never clear. Hence, we wait
 * only a finite amount of time to avoid getting caught in an
 * infinite loop. Normally this delay routine would be a macro,
 * but it isn't called during normal operation so we can afford
 * to make it a function.
 */
static void xl_wait(sc)
	struct xl_softc		*sc;
{
	register int		i;

	for (i = 0; i < XL_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, XL_STATUS) & XL_STAT_CMDBUSY))
			break;
	}

	if (i == XL_TIMEOUT)
		printf("xl%d: command never completed!\n", sc->xl_unit);

	return;
}

/*
 * MII access routines are provided for adapters with external
 * PHYs (3c905-TX, 3c905-T4, 3c905B-T4) and those with built-in
 * autoneg logic that's faked up to look like a PHY (3c905B-TX).
 * Note: if you don't perform the MDIO operations just right,
 * it's possible to end up with code that works correctly with
 * some chips/CPUs/processor speeds/bus speeds/etc but not
 * with others.
 */
#define MII_SET(x)					\
	CSR_WRITE_2(sc, XL_W4_PHY_MGMT,			\
		CSR_READ_2(sc, XL_W4_PHY_MGMT) | x)

#define MII_CLR(x)					\
	CSR_WRITE_2(sc, XL_W4_PHY_MGMT,			\
		CSR_READ_2(sc, XL_W4_PHY_MGMT) & ~x)

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void xl_mii_sync(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(4);
	MII_SET(XL_MII_DIR|XL_MII_DATA);

	for (i = 0; i < 32; i++) {
		MII_SET(XL_MII_CLK);
		DELAY(1);
		MII_CLR(XL_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void xl_mii_send(sc, bits, cnt)
	struct xl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	XL_SEL_WIN(4);
	MII_CLR(XL_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			MII_SET(XL_MII_DATA);
                } else {
			MII_CLR(XL_MII_DATA);
                }
		DELAY(1);
		MII_CLR(XL_MII_CLK);
		DELAY(1);
		MII_SET(XL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int xl_mii_readreg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = XL_MII_STARTDELIM;
	frame->mii_opcode = XL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Select register window 4.
	 */

	XL_SEL_WIN(4);

	CSR_WRITE_2(sc, XL_W4_PHY_MGMT, 0);
	/*
 	 * Turn on data xmit.
	 */
	MII_SET(XL_MII_DIR);

	xl_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	xl_mii_send(sc, frame->mii_stdelim, 2);
	xl_mii_send(sc, frame->mii_opcode, 2);
	xl_mii_send(sc, frame->mii_phyaddr, 5);
	xl_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((XL_MII_CLK|XL_MII_DATA));
	DELAY(1);
	MII_SET(XL_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	MII_CLR(XL_MII_DIR);

	/* Check for ack */
	MII_CLR(XL_MII_CLK);
	DELAY(1);
	MII_SET(XL_MII_CLK);
	DELAY(1);
	ack = CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(XL_MII_CLK);
			DELAY(1);
			MII_SET(XL_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(XL_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA)
				frame->mii_data |= i;
			DELAY(1);
		}
		MII_SET(XL_MII_CLK);
		DELAY(1);
	}

fail:

	MII_CLR(XL_MII_CLK);
	DELAY(1);
	MII_SET(XL_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int xl_mii_writereg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = XL_MII_STARTDELIM;
	frame->mii_opcode = XL_MII_WRITEOP;
	frame->mii_turnaround = XL_MII_TURNAROUND;
	
	/*
	 * Select the window 4.
	 */
	XL_SEL_WIN(4);

	/*
 	 * Turn on data output.
	 */
	MII_SET(XL_MII_DIR);

	xl_mii_sync(sc);

	xl_mii_send(sc, frame->mii_stdelim, 2);
	xl_mii_send(sc, frame->mii_opcode, 2);
	xl_mii_send(sc, frame->mii_phyaddr, 5);
	xl_mii_send(sc, frame->mii_regaddr, 5);
	xl_mii_send(sc, frame->mii_turnaround, 2);
	xl_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(XL_MII_CLK);
	DELAY(1);
	MII_CLR(XL_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(XL_MII_DIR);

	splx(s);

	return(0);
}

static u_int16_t xl_phy_readreg(sc, reg)
	struct xl_softc		*sc;
	int			reg;
{
	struct xl_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->xl_phy_addr;
	frame.mii_regaddr = reg;
	xl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static void xl_phy_writereg(sc, reg, data)
	struct xl_softc		*sc;
	int			reg;
	int			data;
{
	struct xl_mii_frame	frame;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->xl_phy_addr;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	xl_mii_writereg(sc, &frame);

	return;
}

/*
 * The EEPROM is slow: give it time to come ready after issuing
 * it a command.
 */
static int xl_eeprom_wait(sc)
	struct xl_softc		*sc;
{
	int			i;

	for (i = 0; i < 100; i++) {
		if (CSR_READ_2(sc, XL_W0_EE_CMD) & XL_EE_BUSY)
			DELAY(162);
		else
			break;
	}

	if (i == 100) {
		printf("xl%d: eeprom failed to come ready\n", sc->xl_unit);
		return(1);
	}

	return(0);
}

/*
 * Read a sequence of words from the EEPROM. Note that ethernet address
 * data is stored in the EEPROM in network byte order.
 */
static int xl_read_eeprom(sc, dest, off, cnt, swap)
	struct xl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			err = 0, i;
	u_int16_t		word = 0, *ptr;

	XL_SEL_WIN(0);

	if (xl_eeprom_wait(sc))
		return(1);

	for (i = 0; i < cnt; i++) {
		CSR_WRITE_2(sc, XL_W0_EE_CMD, XL_EE_READ | (off + i));
		err = xl_eeprom_wait(sc);
		if (err)
			break;
		word = CSR_READ_2(sc, XL_W0_EE_DATA);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;	
	}

	return(err ? 1 : 0);
}

/*
 * This routine is taken from the 3Com Etherlink XL manual,
 * page 10-7. It calculates a CRC of the supplied multicast
 * group address and returns the lower 8 bits, which are used
 * as the multicast filter position.
 * Note: the 3c905B currently only supports a 64-bit hash table,
 * which means we really only need 6 bits, but the manual indicates
 * that future chip revisions will have a 256-bit hash table,
 * hence the routine is set up to calculate 8 bits of position
 * info in case we need it some day.
 * Note II, The Sequel: _CURRENT_ versions of the 3c905B have a
 * 256 bit hash table. This means we have to use all 8 bits regardless.
 * On older cards, the upper 2 bits will be ignored. Grrrr....
 */
static u_int8_t xl_calchash(addr)
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
	return(crc & 0x000000FF);
}

/*
 * NICs older than the 3c905B have only one multicast option, which
 * is to enable reception of all multicast frames.
 */
static void xl_setmulti(sc)
	struct xl_softc		*sc;
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int8_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	if (ifp->if_flags & IFF_ALLMULTI) {
		rxfilt |= XL_RXFILTER_ALLMULTI;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
		return;
	}

	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next)
		mcnt++;

	if (mcnt)
		rxfilt |= XL_RXFILTER_ALLMULTI;
	else
		rxfilt &= ~XL_RXFILTER_ALLMULTI;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);

	return;
}

/*
 * 3c905B adapters have a hash filter that we can program.
 */
static void xl_setmulti_hash(sc)
	struct xl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0, i;
	struct ifmultiaddr	*ifma;
	u_int8_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	if (ifp->if_flags & IFF_ALLMULTI) {
		rxfilt |= XL_RXFILTER_ALLMULTI;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
		return;
	} else
		rxfilt &= ~XL_RXFILTER_ALLMULTI;


	/* first, zot all the existing hash bits */
	for (i = 0; i < XL_HASHFILT_SIZE; i++)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_HASH|i);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
				ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = xl_calchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_HASH|XL_HASH_SET|h);
		mcnt++;
	}

	if (mcnt)
		rxfilt |= XL_RXFILTER_MULTIHASH;
	else
		rxfilt &= ~XL_RXFILTER_MULTIHASH;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);

	return;
}

#ifdef notdef
static void xl_testpacket(sc)
	struct xl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == NULL)
		return;

	bcopy(&sc->arpcom.ac_enaddr,
		mtod(m, struct ether_header *)->ether_dhost, ETHER_ADDR_LEN);
	bcopy(&sc->arpcom.ac_enaddr,
		mtod(m, struct ether_header *)->ether_shost, ETHER_ADDR_LEN);
	mtod(m, struct ether_header *)->ether_type = htons(3);
	mtod(m, unsigned char *)[14] = 0;
	mtod(m, unsigned char *)[15] = 0;
	mtod(m, unsigned char *)[16] = 0xE3;
	m->m_len = m->m_pkthdr.len = sizeof(struct ether_header) + 3;
	IF_ENQUEUE(&ifp->if_snd, m);
	xl_start(ifp);

	return;
}
#endif

/*
 * Initiate an autonegotiation session.
 */
static void xl_autoneg_xmit(sc)
	struct xl_softc		*sc;
{
	u_int16_t		phy_sts;
	u_int32_t		icfg;

	xl_reset(sc);
	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);
	icfg &= ~XL_ICFG_CONNECTOR_MASK;
	if (sc->xl_media & XL_MEDIAOPT_MII ||
	    sc->xl_media & XL_MEDIAOPT_BT4)
		icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
	if (sc->xl_media & XL_MEDIAOPT_BTX)
		icfg |= (XL_XCVR_AUTO << XL_ICFG_CONNECTOR_BITS);
	if (sc->xl_media & XL_MEDIAOPT_BFX)
		icfg |= (XL_XCVR_100BFX << XL_ICFG_CONNECTOR_BITS);
	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	xl_phy_writereg(sc, PHY_BMCR, PHY_BMCR_RESET);
	DELAY(500);
	while(xl_phy_readreg(sc, XL_PHY_GENCTL)
			& PHY_BMCR_RESET);

	phy_sts = xl_phy_readreg(sc, PHY_BMCR);
	phy_sts |= PHY_BMCR_AUTONEGENBL|PHY_BMCR_AUTONEGRSTR;
	xl_phy_writereg(sc, PHY_BMCR, phy_sts);

	return;
}

/*
 * Invoke autonegotiation on a PHY. Also used with the 3Com internal
 * autoneg logic which is mapped onto the MII.
 */
static void xl_autoneg_mii(sc, flag, verbose)
	struct xl_softc		*sc;
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
	phy_sts = xl_phy_readreg(sc, PHY_BMSR);
	if (!(phy_sts & PHY_BMSR_CANAUTONEG)) {
		if (verbose)
			printf("xl%d: autonegotiation not supported\n",
							sc->xl_unit);
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;	
		media = xl_phy_readreg(sc, PHY_BMCR);
		media &= ~PHY_BMCR_SPEEDSEL;
		media &= ~PHY_BMCR_DUPLEX;
		xl_phy_writereg(sc, PHY_BMCR, media);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
				(CSR_READ_1(sc, XL_W3_MAC_CTRL) &
						~XL_MACCTRL_DUPLEX));
		return;
	}
#endif

	switch (flag) {
	case XL_FLAG_FORCEDELAY:
		/*
	 	 * XXX Never use this option anywhere but in the probe
	 	 * routine: making the kernel stop dead in its tracks
 		 * for three whole seconds after we've gone multi-user
		 * is really bad manners.
	 	 */
		xl_autoneg_xmit(sc);
		DELAY(5000000);
		break;
	case XL_FLAG_SCHEDDELAY:
		/*
		 * Wait for the transmitter to go idle before starting
		 * an autoneg session, otherwise xl_start() may clobber
	 	 * our timeout, and we don't want to allow transmission
		 * during an autoneg session since that can screw it up.
	 	 */
		if (sc->xl_cdata.xl_tx_head != NULL) {
			sc->xl_want_auto = 1;
			return;
		}
		xl_autoneg_xmit(sc);
		ifp->if_timer = 5;
		sc->xl_autoneg = 1;
		sc->xl_want_auto = 0;
		return;
		break;
	case XL_FLAG_DELAYTIMEO:
		ifp->if_timer = 0;
		sc->xl_autoneg = 0;
		break;
	default:
		printf("xl%d: invalid autoneg flag: %d\n", sc->xl_unit, flag);
		return;
	}

	if (xl_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_AUTONEGCOMP) {
		if (verbose)
			printf("xl%d: autoneg complete, ", sc->xl_unit);
		phy_sts = xl_phy_readreg(sc, PHY_BMSR);
	} else {
		if (verbose)
			printf("xl%d: autoneg not complete, ", sc->xl_unit);
	}

	media = xl_phy_readreg(sc, PHY_BMCR);

	/* Link is good. Report modes and set duplex mode. */
	if (xl_phy_readreg(sc, PHY_BMSR) & PHY_BMSR_LINKSTAT) {
		if (verbose)
			printf("link status good ");
		advert = xl_phy_readreg(sc, XL_PHY_ANAR);
		ability = xl_phy_readreg(sc, XL_PHY_LPAR);

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

		/* Set ASIC's duplex mode to match the PHY. */
		XL_SEL_WIN(3);
		if (media & PHY_BMCR_DUPLEX)
			CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
		else
			CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
				(CSR_READ_1(sc, XL_W3_MAC_CTRL) &
						~XL_MACCTRL_DUPLEX));
		xl_phy_writereg(sc, PHY_BMCR, media);
	} else {
		if (verbose)
			printf("no carrier (forcing half-duplex, 10Mbps)\n");
		ifm->ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;
		media &= ~PHY_BMCR_SPEEDSEL;
		media &= ~PHY_BMCR_DUPLEX;
		xl_phy_writereg(sc, PHY_BMCR, media);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
				(CSR_READ_1(sc, XL_W3_MAC_CTRL) &
						~XL_MACCTRL_DUPLEX));
	}

	xl_init(sc);

	if (sc->xl_tx_pend) {
		sc->xl_autoneg = 0;
		sc->xl_tx_pend = 0;
		xl_start(ifp);
	}

	return;
}

static void xl_getmode_mii(sc)
	struct xl_softc		*sc;
{
	u_int16_t		bmsr;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	bmsr = xl_phy_readreg(sc, PHY_BMSR);
	if (bootverbose)
		printf("xl%d: PHY status word: %x\n", sc->xl_unit, bmsr);

	/* fallback */
	sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_HDX;

	if (bmsr & PHY_BMSR_10BTHALF) {
		if (bootverbose)
			printf("xl%d: 10Mbps half-duplex mode supported\n",
								sc->xl_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	}

	if (bmsr & PHY_BMSR_10BTFULL) {
		if (bootverbose)
			printf("xl%d: 10Mbps full-duplex mode supported\n",
								sc->xl_unit);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_10_T|IFM_FDX;
	}

	if (bmsr & PHY_BMSR_100BTXHALF) {
		if (bootverbose)
			printf("xl%d: 100Mbps half-duplex mode supported\n",
								sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_HDX;
	}

	if (bmsr & PHY_BMSR_100BTXFULL) {
		if (bootverbose)
			printf("xl%d: 100Mbps full-duplex mode supported\n",
								sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	}

	/* Some also support 100BaseT4. */
	if (bmsr & PHY_BMSR_100BT4) {
		if (bootverbose)
			printf("xl%d: 100baseT4 mode supported\n", sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_T4, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_100_T4;
#ifdef FORCE_AUTONEG_TFOUR
		if (bootverbose)
			printf("xl%d: forcing on autoneg support for BT4\n",
							 sc->xl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
#endif
	}

	if (bmsr & PHY_BMSR_CANAUTONEG) {
		if (bootverbose)
			printf("xl%d: autoneg supported\n", sc->xl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		sc->ifmedia.ifm_media = IFM_ETHER|IFM_AUTO;
	}

	return;
}

/*
 * Set speed and duplex mode.
 */
static void xl_setmode_mii(sc, media)
	struct xl_softc		*sc;
	int			media;
{
	u_int16_t		bmcr;
	u_int32_t		icfg;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * If an autoneg session is in progress, stop it.
	 */
	if (sc->xl_autoneg) {
		printf("xl%d: canceling autoneg session\n", sc->xl_unit);
		ifp->if_timer = sc->xl_autoneg = sc->xl_want_auto = 0;
		bmcr = xl_phy_readreg(sc, PHY_BMCR);
		bmcr &= ~PHY_BMCR_AUTONEGENBL;
		xl_phy_writereg(sc, PHY_BMCR, bmcr);
	}

	printf("xl%d: selecting MII, ", sc->xl_unit);

	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);
	icfg &= ~XL_ICFG_CONNECTOR_MASK;
	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BT4)
		icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
	if (sc->xl_media & XL_MEDIAOPT_BTX) {
		if (sc->xl_type == XL_TYPE_905B)
			icfg |= (XL_XCVR_AUTO << XL_ICFG_CONNECTOR_BITS);
		else
			icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
	}
	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	if (IFM_SUBTYPE(media) == IFM_100_FX) {
		icfg |= (XL_XCVR_100BFX << XL_ICFG_CONNECTOR_BITS);
		CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
		return;
	}

	bmcr = xl_phy_readreg(sc, PHY_BMCR);

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
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
	} else {
		printf("half duplex\n");
		bmcr &= ~PHY_BMCR_DUPLEX;
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
			(CSR_READ_1(sc, XL_W3_MAC_CTRL) & ~XL_MACCTRL_DUPLEX));
	}

	xl_phy_writereg(sc, PHY_BMCR, bmcr);

	return;
}

static void xl_setmode(sc, media)
	struct xl_softc		*sc;
	int			media;
{
	u_int32_t		icfg;
	u_int16_t		mediastat;

	printf("xl%d: selecting ", sc->xl_unit);

	XL_SEL_WIN(4);
	mediastat = CSR_READ_2(sc, XL_W4_MEDIA_STATUS);
	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);

	if (sc->xl_media & XL_MEDIAOPT_BT) {
		if (IFM_SUBTYPE(media) == IFM_10_T) {
			printf("10baseT transceiver, ");
			sc->xl_xcvr = XL_XCVR_10BT;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_10BT << XL_ICFG_CONNECTOR_BITS);
			mediastat |= XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD;
			mediastat &= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BFX) {
		if (IFM_SUBTYPE(media) == IFM_100_FX) {
			printf("100baseFX port, ");
			sc->xl_xcvr = XL_XCVR_100BFX;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_100BFX << XL_ICFG_CONNECTOR_BITS);
			mediastat |= XL_MEDIASTAT_LINKBEAT;
			mediastat &= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & (XL_MEDIAOPT_AUI|XL_MEDIAOPT_10FL)) {
		if (IFM_SUBTYPE(media) == IFM_10_5) {
			printf("AUI port, ");
			sc->xl_xcvr = XL_XCVR_AUI;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_AUI << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD);
			mediastat |= ~XL_MEDIASTAT_SQEENB;
		}
		if (IFM_SUBTYPE(media) == IFM_10_FL) {
			printf("10baseFL transceiver, ");
			sc->xl_xcvr = XL_XCVR_AUI;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_AUI << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD);
			mediastat |= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BNC) {
		if (IFM_SUBTYPE(media) == IFM_10_2) {
			printf("BNC port, ");
			sc->xl_xcvr = XL_XCVR_COAX;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_COAX << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD|
					XL_MEDIASTAT_SQEENB);
		}
	}

	if ((media & IFM_GMASK) == IFM_FDX ||
			IFM_SUBTYPE(media) == IFM_100_FX) {
		printf("full duplex\n");
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
	} else {
		printf("half duplex\n");
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
			(CSR_READ_1(sc, XL_W3_MAC_CTRL) & ~XL_MACCTRL_DUPLEX));
	}

	if (IFM_SUBTYPE(media) == IFM_10_2)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_START);
	else
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);
	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	XL_SEL_WIN(4);
	CSR_WRITE_2(sc, XL_W4_MEDIA_STATUS, mediastat);
	DELAY(800);
	XL_SEL_WIN(7);

	return;
}

static void xl_reset(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(0);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RESET);

	for (i = 0; i < XL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, XL_STATUS) & XL_STAT_CMDBUSY))
			break;
	}

	if (i == XL_TIMEOUT)
		printf("xl%d: reset didn't complete\n", sc->xl_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for a 3Com Etherlink XL chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static const char *
xl_probe(config_id, device_id)
	pcici_t			config_id;
	pcidi_t			device_id;
{
	struct xl_type		*t;

	t = xl_devs;

	while(t->xl_name != NULL) {
		if ((device_id & 0xFFFF) == t->xl_vid &&
		    ((device_id >> 16) & 0xFFFF) == t->xl_did) {
			return(t->xl_name);
		}
		t++;
	}

	return(NULL);
}

/*
 * This routine is a kludge to work around possible hardware faults
 * or manufacturing defects that can cause the media options register
 * (or reset options register, as it's called for the first generation
 * 3cx90x adapters) to return an incorrect result. I have encountered
 * one Dell Latitude laptop docking station with an integrated 3c905-TX
 * which doesn't have any of the 'mediaopt' bits set. This screws up
 * the attach routine pretty badly because it doesn't know what media
 * to look for. If we find ourselves in this predicament, this routine
 * will try to guess the media options values and warn the user of a
 * possible manufacturing defect with his adapter/system/whatever.
 */
static void xl_mediacheck(sc)
	struct xl_softc		*sc;
{
	u_int16_t		devid;

	/*
	 * If some of the media options bits are set, assume they are
	 * correct. If not, try to figure it out down below.
	 * XXX I should check for 10baseFL, but I don't have an adapter
	 * to test with.
	 */
	if (sc->xl_media & (XL_MEDIAOPT_MASK & ~XL_MEDIAOPT_VCO)) {
		/*
	 	 * Check the XCVR value. If it's not in the normal range
	 	 * of values, we need to fake it up here.
	 	 */
		if (sc->xl_xcvr <= XL_XCVR_AUTO)
			return;
		else {
			printf("xl%d: bogus xcvr value "
			"in EEPROM (%x)\n", sc->xl_unit, sc->xl_xcvr);
			printf("xl%d: choosing new default based "
				"on card type\n", sc->xl_unit);
		}
	} else {
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media & XL_MEDIAOPT_10FL)
			return;
		printf("xl%d: WARNING: no media options bits set in "
			"the media options register!!\n", sc->xl_unit);
		printf("xl%d: this could be a manufacturing defect in "
			"your adapter or system\n", sc->xl_unit);
		printf("xl%d: attempting to guess media type; you "
			"should probably consult your vendor\n", sc->xl_unit);
	}


	/*
	 * Read the device ID from the EEPROM.
	 * This is what's loaded into the PCI device ID register, so it has
	 * to be correct otherwise we wouldn't have gotten this far.
	 */
	xl_read_eeprom(sc, (caddr_t)&devid, XL_EE_PRODID, 1, 0);

	switch(devid) {
	case TC_DEVICEID_BOOMERANG_10BT:	/* 3c900-TPO */
	case TC_DEVICEID_KRAKATOA_10BT:		/* 3c900B-TPO */
		sc->xl_media = XL_MEDIAOPT_BT;
		sc->xl_xcvr = XL_XCVR_10BT;
		printf("xl%d: guessing 10BaseT transceiver\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_10BT_COMBO:	/* 3c900-COMBO */
	case TC_DEVICEID_KRAKATOA_10BT_COMBO:	/* 3c900B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_10BT;
		printf("xl%d: guessing COMBO (AUI/BNC/TP)\n", sc->xl_unit);
		break;
	case TC_DEVICEID_KRAKATOA_10BT_TPC:	/* 3c900B-TPC */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC;
		sc->xl_xcvr = XL_XCVR_10BT;
		printf("xl%d: guessing TPC (BNC/TP)\n", sc->xl_unit);
		break;
	case TC_DEVICEID_CYCLONE_10FL:		/* 3c900B-FL */
		sc->xl_media = XL_MEDIAOPT_10FL;
		sc->xl_xcvr = XL_XCVR_AUI;
		printf("xl%d: guessing 10baseFL\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_10_100BT:	/* 3c905-TX */
		sc->xl_media = XL_MEDIAOPT_MII;
		sc->xl_xcvr = XL_XCVR_MII;
		printf("xl%d: guessing MII\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_100BT4:	/* 3c905-T4 */
	case TC_DEVICEID_CYCLONE_10_100BT4:	/* 3c905B-T4 */
		sc->xl_media = XL_MEDIAOPT_BT4;
		sc->xl_xcvr = XL_XCVR_MII;
		printf("xl%d: guessing 100BaseT4/MII\n", sc->xl_unit);
		break;
	case TC_DEVICEID_HURRICANE_10_100BT:	/* 3c905B-TX */
	case TC_DEVICEID_HURRICANE_10_100BT_SERV:/*3c980-TX */
	case TC_DEVICEID_HURRICANE_SOHO100TX:	/* 3cSOHO100-TX */
	case TC_DEVICEID_TORNADO_10_100BT:	/* 3c905C-TX */
		sc->xl_media = XL_MEDIAOPT_BTX;
		sc->xl_xcvr = XL_XCVR_AUTO;
		printf("xl%d: guessing 10/100 internal\n", sc->xl_unit);
		break;
	case TC_DEVICEID_CYCLONE_10_100_COMBO:	/* 3c905B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BTX|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_AUTO;
		printf("xl%d: guessing 10/100 plus BNC/AUI\n", sc->xl_unit);
		break;
	default:
		printf("xl%d: unknown device ID: %x -- "
			"defaulting to 10baseT\n", sc->xl_unit, devid);
		sc->xl_media = XL_MEDIAOPT_BT;
		break;
	}

	return;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static void
xl_attach(config_id, unit)
	pcici_t			config_id;
	int			unit;
{
	int			s, i;
#ifndef XL_USEIOSPACE
	vm_offset_t		pbase, vbase;
#endif
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	unsigned int		round;
	caddr_t			roundptr;
	struct xl_type		*p;
	u_int16_t		phy_vid, phy_did, phy_sts;

	s = splimp();

	sc = malloc(sizeof(struct xl_softc), M_DEVBUF, M_NOWAIT);
	if (sc == NULL) {
		printf("xl%d: no memory for softc struct!\n", unit);
		goto fail;
	}
	bzero(sc, sizeof(struct xl_softc));

	/*
	 * If this is a 3c905B, we have to check one extra thing.
	 * The 905B supports power management and may be placed in
	 * a low-power mode (D3 mode), typically by certain operating
	 * systems which shall not be named. The PCI BIOS is supposed
	 * to reset the NIC and bring it out of low-power mode, but
	 * some do not. Consequently, we have to see if this chip
	 * supports power management, and if so, make sure it's not
	 * in low-power mode. If power management is available, the
	 * capid byte will be 0x01.
	 *
	 * I _think_ that what actually happens is that the chip
	 * loses its PCI configuration during the transition from
	 * D3 back to D0; this means that it should be possible for
	 * us to save the PCI iobase, membase and IRQ, put the chip
	 * back in the D0 state, then restore the PCI config ourselves.
	 */

	command = pci_conf_read(config_id, XL_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(config_id, XL_PCI_PWRMGMTCTRL);
		if (command & XL_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(config_id, XL_PCI_LOIO);
			membase = pci_conf_read(config_id, XL_PCI_LOMEM);
			irq = pci_conf_read(config_id, XL_PCI_INTLINE);

			/* Reset the power state. */
			printf("xl%d: chip is in D%d power mode "
			"-- setting to D0\n", unit, command & XL_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(config_id, XL_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(config_id, XL_PCI_LOIO, iobase);
			pci_conf_write(config_id, XL_PCI_LOMEM, membase);
			pci_conf_write(config_id, XL_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	command |= (PCIM_CMD_PORTEN|PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);

#ifdef XL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("xl%d: failed to enable I/O ports!\n", unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	if (!pci_map_port(config_id, XL_PCI_LOIO,
				(pci_port_t *)&(sc->xl_bhandle))) {
		printf ("xl%d: couldn't map port\n", unit);
		printf ("xl%d: WARNING: check your BIOS and "
		    "set 'Plug & Play OS' to 'no'\n", unit);
		printf ("xl%d: attempting to map iobase manually\n", unit);
		sc->xl_bhandle =
		    pci_conf_read(config_id, XL_PCI_LOIO) & 0xFFFFFFE0;
		/*goto fail;*/
	}

#ifdef __i386__
	sc->xl_btag = I386_BUS_SPACE_IO;
#endif
#ifdef __alpha__
	sc->xl_btag = ALPHA_BUS_SPACE_IO;
#endif
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("xl%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}

	if (!pci_map_mem(config_id, XL_PCI_LOMEM, &vbase, &pbase)) {
		printf ("xl%d: couldn't map memory\n", unit);
		goto fail;
	}
	sc->xl_bhandle = vbase;
#ifdef __i386__
	sc->xl_btag = I386_BUS_SPACE_MEM;
#endif
#ifdef __alpha__
	sc->xl_btag = ALPHA_BUS_SPACE_MEM;
#endif
#endif

	/* Allocate interrupt */
	if (!pci_map_int(config_id, xl_intr, sc, &net_imask)) {
		printf("xl%d: couldn't map interrupt\n", unit);
		goto fail;
	}

	/* Reset the adapter. */
	xl_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	if (xl_read_eeprom(sc, (caddr_t)&eaddr, XL_EE_OEM_ADR0, 3, 1)) {
		printf("xl%d: failed to read station address\n", sc->xl_unit);
		free(sc, M_DEVBUF);
		goto fail;
	}

	/*
	 * A 3Com chip was detected. Inform the world.
	 */
	printf("xl%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->xl_unit = unit;
	callout_handle_init(&sc->xl_stat_ch);
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->xl_ldata_ptr = malloc(sizeof(struct xl_list_data) + 8,
				M_DEVBUF, M_NOWAIT);
	if (sc->xl_ldata_ptr == NULL) {
		free(sc, M_DEVBUF);
		printf("xl%d: no memory for list buffers!\n", unit);
		goto fail;
	}

	sc->xl_ldata = (struct xl_list_data *)sc->xl_ldata_ptr;
	round = (uintptr_t)sc->xl_ldata_ptr & 0xF;
	roundptr = sc->xl_ldata_ptr;
	for (i = 0; i < 8; i++) {
		if (round % 8) {
			round++;
			roundptr++;
		} else
			break;
	}
	sc->xl_ldata = (struct xl_list_data *)roundptr;
	bzero(sc->xl_ldata, sizeof(struct xl_list_data));

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "xl";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = xl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = xl_start;
	ifp->if_watchdog = xl_watchdog;
	ifp->if_init = xl_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = XL_TX_LIST_CNT - 1;

	/*
	 * Figure out the card type. 3c905B adapters have the
	 * 'supportsNoTxLength' bit set in the capabilities
	 * word in the EEPROM.
	 */
	xl_read_eeprom(sc, (caddr_t)&sc->xl_caps, XL_EE_CAPS, 1, 0);
	if (sc->xl_caps & XL_CAPS_NO_TXLENGTH)
		sc->xl_type = XL_TYPE_905B;
	else
		sc->xl_type = XL_TYPE_90X;

	/*
	 * Now we have to see what sort of media we have.
	 * This includes probing for an MII interace and a
	 * possible PHY.
	 */
	XL_SEL_WIN(3);
	sc->xl_media = CSR_READ_2(sc, XL_W3_MEDIA_OPT);
	if (bootverbose)
		printf("xl%d: media options word: %x\n", sc->xl_unit,
							 sc->xl_media);

	xl_read_eeprom(sc, (char *)&sc->xl_xcvr, XL_EE_ICFG_0, 2, 0);
	sc->xl_xcvr &= XL_ICFG_CONNECTOR_MASK;
	sc->xl_xcvr >>= XL_ICFG_CONNECTOR_BITS;

	xl_mediacheck(sc);

	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BTX
			|| sc->xl_media & XL_MEDIAOPT_BT4) {
		/*
		 * In theory I shouldn't need this, but... if this
		 * card supports an MII, either an external one or
		 * an internal fake one, select it in the internal
		 * config register before trying to probe it.
		 */
		u_int32_t		icfg;

		XL_SEL_WIN(3);
		icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);
		icfg &= ~XL_ICFG_CONNECTOR_MASK;
		if (sc->xl_media & XL_MEDIAOPT_MII ||
			sc->xl_media & XL_MEDIAOPT_BT4)
			icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
		if (sc->xl_media & XL_MEDIAOPT_BTX)
			icfg |= (XL_XCVR_AUTO << XL_ICFG_CONNECTOR_BITS);
		if (sc->xl_media & XL_MEDIAOPT_BFX)
			icfg |= (XL_XCVR_100BFX << XL_ICFG_CONNECTOR_BITS);
		CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);

		if (bootverbose)
			printf("xl%d: probing for a PHY\n", sc->xl_unit);
		for (i = XL_PHYADDR_MIN; i < XL_PHYADDR_MAX + 1; i++) {
			if (bootverbose)
				printf("xl%d: checking address: %d\n",
							sc->xl_unit, i);
			sc->xl_phy_addr = i;
			xl_phy_writereg(sc, XL_PHY_GENCTL, PHY_BMCR_RESET);
			DELAY(500);
			while(xl_phy_readreg(sc, XL_PHY_GENCTL)
					& PHY_BMCR_RESET);
			if ((phy_sts = xl_phy_readreg(sc, XL_PHY_GENSTS)))
				break;
		}
		if (phy_sts) {
			phy_vid = xl_phy_readreg(sc, XL_PHY_VENID);
			phy_did = xl_phy_readreg(sc, XL_PHY_DEVID);
			if (bootverbose)
				printf("xl%d: found PHY at address %d, ",
						sc->xl_unit, sc->xl_phy_addr);
			if (bootverbose)
				printf("vendor id: %x device id: %x\n",
					phy_vid, phy_did);
			p = xl_phys;
			while(p->xl_vid) {
				if (phy_vid == p->xl_vid &&
					(phy_did | 0x000F) == p->xl_did) {
					sc->xl_pinfo = p;
					break;
				}
				p++;
			}
			if (sc->xl_pinfo == NULL)
				sc->xl_pinfo = &xl_phys[PHY_UNKNOWN];
			if (bootverbose)
				printf("xl%d: PHY type: %s\n",
					sc->xl_unit, sc->xl_pinfo->xl_name);
		} else {
			printf("xl%d: MII without any phy!\n", sc->xl_unit);
		}
	}

	/*
	 * Do ifmedia setup.
	 */
	ifmedia_init(&sc->ifmedia, 0, xl_ifmedia_upd, xl_ifmedia_sts);

	if (sc->xl_media & XL_MEDIAOPT_BT) {
		if (bootverbose)
			printf("xl%d: found 10baseT\n", sc->xl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
			ifmedia_add(&sc->ifmedia,
			    IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	}

	if (sc->xl_media & (XL_MEDIAOPT_AUI|XL_MEDIAOPT_10FL)) {
		/*
		 * Check for a 10baseFL board in disguise.
		 */
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			if (bootverbose)
				printf("xl%d: found 10baseFL\n", sc->xl_unit);
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_FL, 0, NULL);
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_FL|IFM_HDX,
			    0, NULL);
			if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
				ifmedia_add(&sc->ifmedia,
				    IFM_ETHER|IFM_10_FL|IFM_FDX, 0, NULL);
		} else {
			if (bootverbose)
				printf("xl%d: found AUI\n", sc->xl_unit);
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_5, 0, NULL);
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BNC) {
		if (bootverbose)
			printf("xl%d: found BNC\n", sc->xl_unit);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_2, 0, NULL);
	}

	/*
	 * Technically we could use xl_getmode_mii() to scan the
	 * modes, but the built-in BTX mode on the 3c905B implies
	 * 10/100 full/half duplex support anyway, so why not just
	 * do it and get it over with.
	 */
	if (sc->xl_media & XL_MEDIAOPT_BTX) {
		if (bootverbose)
			printf("xl%d: found 100baseTX\n", sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
			IFM_ETHER|IFM_100_TX|IFM_HDX, 0, NULL);
		if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
			ifmedia_add(&sc->ifmedia,
				IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		if (sc->xl_pinfo != NULL)
			ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	}

	if (sc->xl_media & XL_MEDIAOPT_BFX) {
		if (bootverbose)
			printf("xl%d: found 100baseFX\n", sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_FX, 0, NULL);
	}

	/*
	 * If there's an MII, we have to probe its modes
	 * separately.
	 */
	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BT4) {
		if (bootverbose)
			printf("xl%d: found MII\n", sc->xl_unit);
		xl_getmode_mii(sc);
	}

	/* Choose a default media. */
	switch(sc->xl_xcvr) {
	case XL_XCVR_10BT:
		media = IFM_ETHER|IFM_10_T;
		xl_setmode(sc, media);
		break;
	case XL_XCVR_AUI:
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			media = IFM_ETHER|IFM_10_FL;
			xl_setmode(sc, media);
		} else {
			media = IFM_ETHER|IFM_10_5;
			xl_setmode(sc, media);
		}
		break;
	case XL_XCVR_COAX:
		media = IFM_ETHER|IFM_10_2;
		xl_setmode(sc, media);
		break;
	case XL_XCVR_AUTO:
#ifdef XL_BACKGROUND_AUTONEG
		xl_autoneg_mii(sc, XL_FLAG_SCHEDDELAY, 1);
#else
		xl_autoneg_mii(sc, XL_FLAG_FORCEDELAY, 1);
#endif
		media = sc->ifmedia.ifm_media;
		break;
	case XL_XCVR_100BTX:
	case XL_XCVR_MII:
#ifdef XL_BACKGROUND_AUTONEG
		xl_autoneg_mii(sc, XL_FLAG_SCHEDDELAY, 1);
#else
		xl_autoneg_mii(sc, XL_FLAG_FORCEDELAY, 1);
#endif
		media = sc->ifmedia.ifm_media;
		break;
	case XL_XCVR_100BFX:
		media = IFM_ETHER|IFM_100_FX;
		break;
	default:
		printf("xl%d: unknown XCVR type: %d\n", sc->xl_unit,
							sc->xl_xcvr);
		/*
		 * This will probably be wrong, but it prevents
	 	 * the ifmedia code from panicking.
		 */
		media = IFM_ETHER|IFM_10_T;
		break;
	}

	ifmedia_set(&sc->ifmedia, media);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPF > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	at_shutdown(xl_shutdown, sc, SHUTDOWN_POST_SYNC);

fail:
	splx(s);
	return;
}

/*
 * Initialize the transmit descriptors.
 */
static int xl_list_tx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			i;

	cd = &sc->xl_cdata;
	ld = sc->xl_ldata;
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		cd->xl_tx_chain[i].xl_ptr = &ld->xl_tx_list[i];
		if (i == (XL_TX_LIST_CNT - 1))
			cd->xl_tx_chain[i].xl_next = NULL;
		else
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[i + 1];
	}

	cd->xl_tx_free = &cd->xl_tx_chain[0];
	cd->xl_tx_tail = cd->xl_tx_head = NULL;

	return(0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int xl_list_rx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			i;

	cd = &sc->xl_cdata;
	ld = sc->xl_ldata;

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		cd->xl_rx_chain[i].xl_ptr =
			(struct xl_list_onefrag *)&ld->xl_rx_list[i];
		if (xl_newbuf(sc, &cd->xl_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		if (i == (XL_RX_LIST_CNT - 1)) {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[0];
			ld->xl_rx_list[i].xl_next =
					vtophys(&ld->xl_rx_list[0]);
		} else {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[i + 1];
			ld->xl_rx_list[i].xl_next =
					vtophys(&ld->xl_rx_list[i + 1]);
		}
	}

	cd->xl_rx_head = &cd->xl_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int xl_newbuf(sc, c)
	struct xl_softc		*sc;
	struct xl_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("xl%d: no memory for rx list -- packet dropped!\n",
								sc->xl_unit);
		return(ENOBUFS);
	}

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		printf("xl%d: no memory for rx list -- packet dropped!\n",
								sc->xl_unit);
		m_freem(m_new);
		return(ENOBUFS);
	}

	/* Force longword alignment for packet payload. */
	m_new->m_data += 2;

	c->xl_mbuf = m_new;
	c->xl_ptr->xl_status = 0;
	c->xl_ptr->xl_frag.xl_addr = vtophys(mtod(m_new, caddr_t));
	c->xl_ptr->xl_frag.xl_len = MCLBYTES | XL_LAST_FRAG;

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void xl_rxeof(sc)
	struct xl_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct xl_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int16_t		rxstat;

	ifp = &sc->arpcom.ac_if;

again:

	while((rxstat = sc->xl_cdata.xl_rx_head->xl_ptr->xl_status)) {
		cur_rx = sc->xl_cdata.xl_rx_head;
		sc->xl_cdata.xl_rx_head = cur_rx->xl_next;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & XL_RXSTAT_UP_ERROR) {
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = 0;
			continue;
		}

		/*
		 * If there error bit was not set, the upload complete
		 * bit should be set which means we have a valid packet.
		 * If not, something truly strange has happened.
		 */
		if (!(rxstat & XL_RXSTAT_UP_CMPLT)) {
			printf("xl%d: bad receive status -- packet dropped",
							sc->xl_unit);
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = 0;
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->xl_mbuf;
		total_len = cur_rx->xl_ptr->xl_status & XL_RXSTAT_LENMASK;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (xl_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = 0;
			continue;
		}

		ifp->if_ipackets++;
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;

#if NBPF > 0
		/* Handle BPF listeners. Let the BPF user see the packet. */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m);
#endif

#ifdef BRIDGE
		if (do_bridge) {
			struct ifnet *bdg_ifp ;
			bdg_ifp = bridge_in(m);
			if (bdg_ifp != BDG_LOCAL && bdg_ifp != BDG_DROP)
				bdg_forward(&m, bdg_ifp);
			if (((bdg_ifp != BDG_LOCAL) && (bdg_ifp != BDG_BCAST) &&
			    (bdg_ifp != BDG_MCAST)) || bdg_ifp == BDG_DROP) {
				m_freem(m);
				continue;
			}
		}
#endif

#if NBPF > 0
		/*
		 * Don't pass packet up to the ether_input() layer unless it's
		 * a broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		if (ifp->if_bpf) {
			if (ifp->if_flags & IFF_PROMISC &&
			    (bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
			    ETHER_ADDR_LEN) && (eh->ether_dhost[0] & 1) == 0)){
				m_freem(m);
				continue;
			}
		}
#endif

		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	/*
	 * Handle the 'end of channel' condition. When the upload
	 * engine hits the end of the RX ring, it will stall. This
	 * is our cue to flush the RX ring, reload the uplist pointer
	 * register and unstall the engine.
	 * XXX This is actually a little goofy. With the ThunderLAN
	 * chip, you get an interrupt when the receiver hits the end
	 * of the receive ring, which tells you exactly when you
	 * you need to reload the ring pointer. Here we have to
	 * fake it. I'm mad at myself for not being clever enough
	 * to avoid the use of a goto here.
	 */
	if (CSR_READ_4(sc, XL_UPLIST_PTR) == 0 ||
		CSR_READ_4(sc, XL_UPLIST_STATUS) & XL_PKTSTAT_UP_STALLED) {
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_STALL);
		xl_wait(sc);
		CSR_WRITE_4(sc, XL_UPLIST_PTR,
			vtophys(&sc->xl_ldata->xl_rx_list[0]));
		sc->xl_cdata.xl_rx_head = &sc->xl_cdata.xl_rx_chain[0];
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_UNSTALL);
		goto again;
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void xl_txeof(sc)
	struct xl_softc		*sc;
{
	struct xl_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded. Note: the 3c905B
	 * sets a special bit in the status word to let us
	 * know that a frame has been downloaded, but the
	 * original 3c900/3c905 adapters don't do that.
	 * Consequently, we have to use a different test if
	 * xl_type != XL_TYPE_905B.
	 */
	while(sc->xl_cdata.xl_tx_head != NULL) {
		cur_tx = sc->xl_cdata.xl_tx_head;
		if ((sc->xl_type == XL_TYPE_905B &&
		!(cur_tx->xl_ptr->xl_status & XL_TXSTAT_DL_COMPLETE)) ||
			CSR_READ_4(sc, XL_DOWNLIST_PTR)) {
			break;
		}
		sc->xl_cdata.xl_tx_head = cur_tx->xl_next;
		m_freem(cur_tx->xl_mbuf);
		cur_tx->xl_mbuf = NULL;
		ifp->if_opackets++;

		cur_tx->xl_next = sc->xl_cdata.xl_tx_free;
		sc->xl_cdata.xl_tx_free = cur_tx;
	}

	if (sc->xl_cdata.xl_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->xl_cdata.xl_tx_tail = NULL;
		if (sc->xl_want_auto)
			xl_autoneg_mii(sc, XL_FLAG_SCHEDDELAY, 1);
	} else {
		if (CSR_READ_4(sc, XL_DMACTL) & XL_DMACTL_DOWN_STALLED ||
			!CSR_READ_4(sc, XL_DOWNLIST_PTR)) {
			CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
				vtophys(sc->xl_cdata.xl_tx_head->xl_ptr));
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		}
	}

	return;
}

/*
 * TX 'end of channel' interrupt handler. Actually, we should
 * only get a 'TX complete' interrupt if there's a transmit error,
 * so this is really TX error handler.
 */
static void xl_txeoc(sc)
	struct xl_softc		*sc;
{
	u_int8_t		txstat;

	while((txstat = CSR_READ_1(sc, XL_TX_STATUS))) {
		if (txstat & XL_TXSTATUS_UNDERRUN ||
			txstat & XL_TXSTATUS_JABBER ||
			txstat & XL_TXSTATUS_RECLAIM) {
			printf("xl%d: transmission error: %x\n",
						sc->xl_unit, txstat);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
			xl_wait(sc);
			if (sc->xl_cdata.xl_tx_head != NULL)
				CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
				vtophys(sc->xl_cdata.xl_tx_head->xl_ptr));
			/*
			 * Remember to set this for the
			 * first generation 3c90X chips.
			 */
			CSR_WRITE_1(sc, XL_TX_FREETHRESH, XL_PACKET_SIZE >> 8);
			if (txstat & XL_TXSTATUS_UNDERRUN &&
			    sc->xl_tx_thresh < XL_PACKET_SIZE) {
				sc->xl_tx_thresh += XL_MIN_FRAMELEN;
				printf("xl%d: tx underrun, increasing tx start"
				    " threshold to %d bytes\n", sc->xl_unit,
				    sc->xl_tx_thresh);
			}
			CSR_WRITE_2(sc, XL_COMMAND,
			    XL_CMD_TX_SET_START|sc->xl_tx_thresh);
			if (sc->xl_type == XL_TYPE_905B) {
				CSR_WRITE_2(sc, XL_COMMAND,
				XL_CMD_SET_TX_RECLAIM|(XL_PACKET_SIZE >> 4));
			}
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		} else {
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		}
		/*
		 * Write an arbitrary byte to the TX_STATUS register
	 	 * to clear this interrupt/error and advance to the next.
		 */
		CSR_WRITE_1(sc, XL_TX_STATUS, 0x01);
	}

	return;
}

static void xl_intr(arg)
	void			*arg;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Disable interrupts. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB);

	for (;;) {
		status = CSR_READ_2(sc, XL_STATUS);

		if ((status & XL_INTRS) == 0)
			break;

		CSR_WRITE_2(sc, XL_COMMAND,
		    XL_CMD_INTR_ACK|(status & XL_INTRS));

		if (status & XL_STAT_UP_COMPLETE)
			xl_rxeof(sc);

		if (status & XL_STAT_DOWN_COMPLETE)
			xl_txeof(sc);

		if (status & XL_STAT_TX_COMPLETE) {
			ifp->if_oerrors++;
			xl_txeoc(sc);
		}

		if (status & XL_STAT_ADFAIL) {
			xl_reset(sc);
			xl_init(sc);
		}

		if (status & XL_STAT_STATSOFLOW) {
			sc->xl_stats_no_timeout = 1;
			xl_stats_update(sc);
			sc->xl_stats_no_timeout = 0;
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB|XL_INTRS);

	XL_SEL_WIN(7);

	if (ifp->if_snd.ifq_head != NULL)
		xl_start(ifp);

	return;
}

static void xl_stats_update(xsc)
	void			*xsc;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	struct xl_stats		xl_stats;
	u_int8_t		*p;
	int			i;

	bzero((char *)&xl_stats, sizeof(struct xl_stats));

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	p = (u_int8_t *)&xl_stats;

	/* Read all the stats registers. */
	XL_SEL_WIN(6);

	for (i = 0; i < 16; i++)
		*p++ = CSR_READ_1(sc, XL_W6_CARRIER_LOST + i);

	ifp->if_ierrors += xl_stats.xl_rx_overrun;

	ifp->if_collisions += xl_stats.xl_tx_multi_collision +
				xl_stats.xl_tx_single_collision +
				xl_stats.xl_tx_late_collision;

	/*
	 * Boomerang and cyclone chips have an extra stats counter
	 * in window 4 (BadSSD). We have to read this too in order
	 * to clear out all the stats registers and avoid a statsoflow
	 * interrupt.
	 */
	XL_SEL_WIN(4);
	CSR_READ_1(sc, XL_W4_BADSSD);

	XL_SEL_WIN(7);

	if (!sc->xl_stats_no_timeout)
		sc->xl_stat_ch = timeout(xl_stats_update, sc, hz);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int xl_encap(sc, c, m_head)
	struct xl_softc		*sc;
	struct xl_chain		*c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct xl_frag		*f = NULL;
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
			if (frag == XL_MAXFRAGS)
				break;
			total_len+= m->m_len;
			c->xl_ptr->xl_frag[frag].xl_addr =
					vtophys(mtod(m, vm_offset_t));
			c->xl_ptr->xl_frag[frag].xl_len = m->m_len;
			frag++;
		}
	}

	/*
	 * Handle special case: we used up all 63 fragments,
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
			printf("xl%d: no memory for tx list", sc->xl_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("xl%d: no memory for tx list",
						sc->xl_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->xl_ptr->xl_frag[0];
		f->xl_addr = vtophys(mtod(m_new, caddr_t));
		f->xl_len = total_len = m_new->m_len;
		frag = 1;
	}

	c->xl_mbuf = m_head;
	c->xl_ptr->xl_frag[frag - 1].xl_len |=  XL_LAST_FRAG;
	c->xl_ptr->xl_status = total_len;
	c->xl_ptr->xl_next = 0;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
static void xl_start(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct xl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;

	sc = ifp->if_softc;

	if (sc->xl_autoneg) {
		sc->xl_tx_pend = 1;
		return;
	}

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->xl_cdata.xl_tx_free == NULL) {
		xl_txeoc(sc);
		xl_txeof(sc);
		if (sc->xl_cdata.xl_tx_free == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
		}
	}

	start_tx = sc->xl_cdata.xl_tx_free;

	while(sc->xl_cdata.xl_tx_free != NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		cur_tx = sc->xl_cdata.xl_tx_free;
		sc->xl_cdata.xl_tx_free = cur_tx->xl_next;

		cur_tx->xl_next = NULL;

		/* Pack the data into the descriptor. */
		xl_encap(sc, cur_tx, m_head);

		/* Chain it together. */
		if (prev != NULL) {
			prev->xl_next = cur_tx;
			prev->xl_ptr->xl_next = vtophys(cur_tx->xl_ptr);
		}
		prev = cur_tx;

#if NBPF > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp, cur_tx->xl_mbuf);
#endif
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interupt once for the whole chain rather than
	 * once for each packet.
	 */
	cur_tx->xl_ptr->xl_status |= XL_TXSTAT_DL_INTR;

	/*
	 * Queue the packets. If the TX channel is clear, update
	 * the downlist pointer register.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_STALL);
	xl_wait(sc);

	if (sc->xl_cdata.xl_tx_head != NULL) {
		sc->xl_cdata.xl_tx_tail->xl_next = start_tx;
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_next =
					vtophys(start_tx->xl_ptr);
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_status &=
					~XL_TXSTAT_DL_INTR;
		sc->xl_cdata.xl_tx_tail = cur_tx;
	} else {
		sc->xl_cdata.xl_tx_head = start_tx;
		sc->xl_cdata.xl_tx_tail = cur_tx;
	}
	if (!CSR_READ_4(sc, XL_DOWNLIST_PTR))
		CSR_WRITE_4(sc, XL_DOWNLIST_PTR, vtophys(start_tx->xl_ptr));

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);

	XL_SEL_WIN(7);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	/*
	 * XXX Under certain conditions, usually on slower machines
	 * where interrupts may be dropped, it's possible for the
	 * adapter to chew up all the buffers in the receive ring
	 * and stall, without us being able to do anything about it.
	 * To guard against this, we need to make a pass over the
	 * RX queue to make sure there aren't any packets pending.
	 * Doing it here means we can flush the receive ring at the
	 * same time the chip is DMAing the transmit descriptors we
	 * just gave it.
 	 *
	 * 3Com goes to some lengths to emphasize the Parallel Tasking (tm)
	 * nature of their chips in all their marketing literature;
	 * we may as well take advantage of it. :)
	 */
	xl_rxeof(sc);

	return;
}

static void xl_init(xsc)
	void			*xsc;
{
	struct xl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s, i;
	u_int16_t		rxfilt = 0;
	u_int16_t		phy_bmcr = 0;

	if (sc->xl_autoneg)
		return;

	s = splimp();

	/*
	 * XXX Hack for the 3c905B: the built-in autoneg logic's state
	 * gets reset by xl_init() when we don't want it to. Try
	 * to preserve it. (For 3c905 cards with real external PHYs,
	 * the BMCR register doesn't change, but this doesn't hurt.)
	 */
	if (sc->xl_pinfo != NULL)
		phy_bmcr = xl_phy_readreg(sc, PHY_BMCR);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	xl_stop(sc);

	xl_wait(sc);

	/* Init our MAC address */
	XL_SEL_WIN(2);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, XL_W2_STATION_ADDR_LO + i,
				sc->arpcom.ac_enaddr[i]);
	}

	/* Clear the station mask. */
	for (i = 0; i < 3; i++)
		CSR_WRITE_2(sc, XL_W2_STATION_MASK_LO + (i * 2), 0);

#ifdef notdef
	/* Reset TX and RX. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
#endif

	/* Init circular RX list. */
	if (xl_list_rx_init(sc) == ENOBUFS) {
		printf("xl%d: initialization failed: no "
			"memory for rx buffers\n", sc->xl_unit);
		xl_stop(sc);
		return;
	}

	/* Init TX descriptors. */
	xl_list_tx_init(sc);

	/*
	 * Set the TX freethresh value.
	 * Note that this has no effect on 3c905B "cyclone"
	 * cards but is required for 3c900/3c905 "boomerang"
	 * cards in order to enable the download engine.
	 */
	CSR_WRITE_1(sc, XL_TX_FREETHRESH, XL_PACKET_SIZE >> 8);

	/* Set the TX start threshold for best performance. */
	sc->xl_tx_thresh = XL_MIN_FRAMELEN;
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_SET_START|sc->xl_tx_thresh);

	/*
	 * If this is a 3c905B, also set the tx reclaim threshold.
	 * This helps cut down on the number of tx reclaim errors
	 * that could happen on a busy network. The chip multiplies
	 * the register value by 16 to obtain the actual threshold
	 * in bytes, so we divide by 16 when setting the value here.
	 * The existing threshold value can be examined by reading
	 * the register at offset 9 in window 5.
	 */
	if (sc->xl_type == XL_TYPE_905B) {
		CSR_WRITE_2(sc, XL_COMMAND,
			XL_CMD_SET_TX_RECLAIM|(XL_PACKET_SIZE >> 4));
	}

	/* Set RX filter bits. */
	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	/* Set the individual bit to receive frames for this host only. */
	rxfilt |= XL_RXFILTER_INDIVIDUAL;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxfilt |= XL_RXFILTER_ALLFRAMES;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	} else {
		rxfilt &= ~XL_RXFILTER_ALLFRAMES;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxfilt |= XL_RXFILTER_BROADCAST;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	} else {
		rxfilt &= ~XL_RXFILTER_BROADCAST;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	if (sc->xl_type == XL_TYPE_905B)
		xl_setmulti_hash(sc);
	else
		xl_setmulti(sc);

	/*
	 * Load the address of the RX list. We have to
	 * stall the upload engine before we can manipulate
	 * the uplist pointer register, then unstall it when
	 * we're finished. We also have to wait for the
	 * stall command to complete before proceeding.
	 * Note that we have to do this after any RX resets
	 * have completed since the uplist register is cleared
	 * by a reset.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_STALL);
	xl_wait(sc);
	CSR_WRITE_4(sc, XL_UPLIST_PTR, vtophys(&sc->xl_ldata->xl_rx_list[0]));
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_UNSTALL);

	/*
	 * If the coax transceiver is on, make sure to enable
	 * the DC-DC converter.
 	 */
	XL_SEL_WIN(3);
	if (sc->xl_xcvr == XL_XCVR_COAX)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_START);
	else
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	/* Clear out the stats counters. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_DISABLE);
	sc->xl_stats_no_timeout = 1;
	xl_stats_update(sc);
	sc->xl_stats_no_timeout = 0;
	XL_SEL_WIN(4);
	CSR_WRITE_2(sc, XL_W4_NET_DIAG, XL_NETDIAG_UPPER_BYTES_ENABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_ENABLE);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ACK|0xFF);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STAT_ENB|XL_INTRS);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB|XL_INTRS);

	/* Set the RX early threshold */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_THRESH|(XL_PACKET_SIZE >>2));
	CSR_WRITE_2(sc, XL_DMACTL, XL_DMACTL_UP_RX_EARLY);

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_ENABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);

	/* Restore state of BMCR */
	if (sc->xl_pinfo != NULL)
		xl_phy_writereg(sc, PHY_BMCR, phy_bmcr);

	/* Select window 7 for normal operations. */
	XL_SEL_WIN(7);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	sc->xl_stat_ch = timeout(xl_stats_update, sc, hz);

	return;
}

/*
 * Set media options.
 */
static int xl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct ifmedia		*ifm;

	sc = ifp->if_softc;
	ifm = &sc->ifmedia;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_100_FX:
	case IFM_10_2:
	case IFM_10_5:
		xl_setmode(sc, ifm->ifm_media);
		return(0);
		break;
	default:
		break;
	}

	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BTX
		|| sc->xl_media & XL_MEDIAOPT_BT4) {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO)
			xl_autoneg_mii(sc, XL_FLAG_SCHEDDELAY, 1);
		else
			xl_setmode_mii(sc, ifm->ifm_media);
	} else {
		xl_setmode(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void xl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct xl_softc		*sc;
	u_int16_t		advert = 0, ability = 0;
	u_int32_t		icfg;

	sc = ifp->if_softc;

	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG) & XL_ICFG_CONNECTOR_MASK;
	icfg >>= XL_ICFG_CONNECTOR_BITS;

	ifmr->ifm_active = IFM_ETHER;

	switch(icfg) {
	case XL_XCVR_10BT:
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (CSR_READ_1(sc, XL_W3_MAC_CTRL) & XL_MACCTRL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		break;
	case XL_XCVR_AUI:
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			ifmr->ifm_active = IFM_ETHER|IFM_10_FL;
			if (CSR_READ_1(sc, XL_W3_MAC_CTRL) & XL_MACCTRL_DUPLEX)
				ifmr->ifm_active |= IFM_FDX;
			else
				ifmr->ifm_active |= IFM_HDX;
		} else
			ifmr->ifm_active = IFM_ETHER|IFM_10_5;
		break;
	case XL_XCVR_COAX:
		ifmr->ifm_active = IFM_ETHER|IFM_10_2;
		break;
	/*
	 * XXX MII and BTX/AUTO should be separate cases.
	 */

	case XL_XCVR_100BTX:
	case XL_XCVR_AUTO:
	case XL_XCVR_MII:
		if (!(xl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_AUTONEGENBL)) {
			if (xl_phy_readreg(sc, PHY_BMCR) & PHY_BMCR_SPEEDSEL)
				ifmr->ifm_active = IFM_ETHER|IFM_100_TX;
			else
				ifmr->ifm_active = IFM_ETHER|IFM_10_T;
			XL_SEL_WIN(3);
			if (CSR_READ_2(sc, XL_W3_MAC_CTRL) &
						XL_MACCTRL_DUPLEX)
				ifmr->ifm_active |= IFM_FDX;
			else
				ifmr->ifm_active |= IFM_HDX;
			break;
		}
		ability = xl_phy_readreg(sc, XL_PHY_LPAR);
		advert = xl_phy_readreg(sc, XL_PHY_ANAR);
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
		break;
	case XL_XCVR_100BFX:
		ifmr->ifm_active = IFM_ETHER|IFM_100_FX;
		break;
	default:
		printf("xl%d: unknown XCVR type: %d\n", sc->xl_unit, icfg);
		break;
	}

	return;
}

static int xl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct xl_softc		*sc = ifp->if_softc;
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
			xl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				xl_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (sc->xl_type == XL_TYPE_905B)
			xl_setmulti_hash(sc);
		else
			xl_setmulti(sc);
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

static void xl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	u_int16_t		status = 0;

	sc = ifp->if_softc;

	if (sc->xl_autoneg) {
		xl_autoneg_mii(sc, XL_FLAG_DELAYTIMEO, 1);
		return;
	}

	ifp->if_oerrors++;
	XL_SEL_WIN(4);
	status = CSR_READ_2(sc, XL_W4_MEDIA_STATUS);
	printf("xl%d: watchdog timeout\n", sc->xl_unit);

	if (status & XL_MEDIASTAT_CARRIER)
		printf("xl%d: no carrier - transceiver cable problem?\n",
								sc->xl_unit);
	xl_txeoc(sc);
	xl_txeof(sc);
	xl_rxeof(sc);
	xl_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		xl_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void xl_stop(sc)
	struct xl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_DISCARD);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);
	DELAY(800);
#ifdef notdef
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
#endif
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ACK|XL_STAT_INTLATCH);

	/* Stop the stats updater. */
	untimeout(xl_stats_update, sc, sc->xl_stat_ch);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_rx_chain[i].xl_mbuf != NULL) {
			m_freem(sc->xl_cdata.xl_rx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_rx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->xl_ldata->xl_rx_list,
		sizeof(sc->xl_ldata->xl_rx_list));
	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_tx_chain[i].xl_mbuf != NULL) {
			m_freem(sc->xl_cdata.xl_tx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_tx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->xl_ldata->xl_tx_list,
		sizeof(sc->xl_ldata->xl_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void xl_shutdown(howto, arg)
	int			howto;
	void			*arg;
{
	struct xl_softc		*sc = (struct xl_softc *)arg;

	xl_stop(sc);

	return;
}


static struct pci_device xl_device = {
	"xl",
	xl_probe,
	xl_attach,
	&xl_count,
	NULL
};
COMPAT_PCI_DRIVER(xl, xl_device);
