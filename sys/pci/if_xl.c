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
 * 3Com 3c905C-TX	10/100Mbps/RJ-45 (Tornado ASIC)
 * 3Com 3c980-TX	10/100Mbps server adapter (Hurricane ASIC)
 * 3Com 3c980C-TX	10/100Mbps server adapter (Tornado ASIC)
 * 3Com 3cSOHO100-TX	10/100Mbps/RJ-45 (Hurricane ASIC)
 * 3Com 3c450-TX	10/100Mbps/RJ-45 (Tornado ASIC)
 * 3Com 3c555		10/100Mbps/RJ-45 (MiniPCI, Laptop Hurricane)
 * 3Com 3c556		10/100Mbps/RJ-45 (MiniPCI, Hurricane ASIC)
 * 3Com 3c556B		10/100Mbps/RJ-45 (MiniPCI, Hurricane ASIC)
 * 3Com 3c575TX		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3c575B		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3c575C		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656	10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656b	10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656c	10/100Mbps/RJ-45 (Cardbus, Tornado ASIC)
 * Dell Optiplex GX1 on-board 3c918 10/100Mbps/RJ-45
 * Dell on-board 3c920 10/100Mbps/RJ-45
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
 * chain into an mbuf cluster and then DMAing the cluster. This extra
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

MODULE_DEPEND(xl, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

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

#include <pci/if_xlreg.h>

#define XL905B_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)

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
	{ TC_VENDORID, TC_DEVICEID_TORNADO_10_100BT_SERV,
		"3Com 3c980C Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_SOHO100TX,
		"3Com 3cSOHO100-TX OfficeConnect" },
	{ TC_VENDORID, TC_DEVICEID_TORNADO_HOMECONNECT,
		"3Com 3c450-TX HomeConnect" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_555,
		"3Com 3c555 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_556,
		"3Com 3c556 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_556B,
		"3Com 3c556B Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_575A,
		"3Com 3c575TX Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_575B,
		"3Com 3c575B Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_575C,
		"3Com 3c575C Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_656,
		"3Com 3c656 Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_HURRICANE_656B,
		"3Com 3c656B Fast Etherlink XL" },
	{ TC_VENDORID, TC_DEVICEID_TORNADO_656C,
		"3Com 3c656C Fast Etherlink XL" },
	{ 0, 0, NULL }
};

static int xl_probe		(device_t);
static int xl_attach		(device_t);
static int xl_detach		(device_t);

static int xl_newbuf		(struct xl_softc *, struct xl_chain_onefrag *);
static void xl_stats_update	(void *);
static int xl_encap		(struct xl_softc *, struct xl_chain *,
						struct mbuf *);
static void xl_rxeof		(struct xl_softc *);
static int xl_rx_resync		(struct xl_softc *);
static void xl_txeof		(struct xl_softc *);
static void xl_txeof_90xB	(struct xl_softc *);
static void xl_txeoc		(struct xl_softc *);
static void xl_intr		(void *);
static void xl_start		(struct ifnet *);
static void xl_start_90xB	(struct ifnet *);
static int xl_ioctl		(struct ifnet *, u_long, caddr_t);
static void xl_init		(void *);
static void xl_stop		(struct xl_softc *);
static void xl_watchdog		(struct ifnet *);
static void xl_shutdown		(device_t);
static int xl_suspend		(device_t); 
static int xl_resume		(device_t);

static int xl_ifmedia_upd	(struct ifnet *);
static void xl_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static int xl_eeprom_wait	(struct xl_softc *);
static int xl_read_eeprom	(struct xl_softc *, caddr_t, int, int, int);
static void xl_mii_sync		(struct xl_softc *);
static void xl_mii_send		(struct xl_softc *, u_int32_t, int);
static int xl_mii_readreg	(struct xl_softc *, struct xl_mii_frame *);
static int xl_mii_writereg	(struct xl_softc *, struct xl_mii_frame *);

static void xl_setcfg		(struct xl_softc *);
static void xl_setmode		(struct xl_softc *, int);
static u_int8_t xl_calchash	(caddr_t);
static void xl_setmulti		(struct xl_softc *);
static void xl_setmulti_hash	(struct xl_softc *);
static void xl_reset		(struct xl_softc *);
static int xl_list_rx_init	(struct xl_softc *);
static int xl_list_tx_init	(struct xl_softc *);
static int xl_list_tx_init_90xB	(struct xl_softc *);
static void xl_wait		(struct xl_softc *);
static void xl_mediacheck	(struct xl_softc *);
static void xl_choose_xcvr	(struct xl_softc *, int);
static void xl_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static void xl_dma_map_txbuf	(void *, bus_dma_segment_t *, int, bus_size_t,
						int);
#ifdef notdef
static void xl_testpacket	(struct xl_softc *);
#endif

static int xl_miibus_readreg	(device_t, int, int);
static int xl_miibus_writereg	(device_t, int, int, int);
static void xl_miibus_statchg	(device_t);
static void xl_miibus_mediainit	(device_t);

#ifdef XL_USEIOSPACE
#define XL_RES			SYS_RES_IOPORT
#define XL_RID			XL_PCI_LOIO
#else
#define XL_RES			SYS_RES_MEMORY
#define XL_RID			XL_PCI_LOMEM
#endif

static device_method_t xl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xl_probe),
	DEVMETHOD(device_attach,	xl_attach),
	DEVMETHOD(device_detach,	xl_detach),
	DEVMETHOD(device_shutdown,	xl_shutdown),
	DEVMETHOD(device_suspend,	xl_suspend),
	DEVMETHOD(device_resume,	xl_resume),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	xl_miibus_readreg),
	DEVMETHOD(miibus_writereg,	xl_miibus_writereg),
	DEVMETHOD(miibus_statchg,	xl_miibus_statchg),
	DEVMETHOD(miibus_mediainit,	xl_miibus_mediainit),

	{ 0, 0 }
};

static driver_t xl_driver = {
	"xl",
	xl_methods,
	sizeof(struct xl_softc)
};

static devclass_t xl_devclass;

DRIVER_MODULE(if_xl, cardbus, xl_driver, xl_devclass, 0, 0);
DRIVER_MODULE(if_xl, pci, xl_driver, xl_devclass, 0, 0);
DRIVER_MODULE(miibus, xl, miibus_driver, miibus_devclass, 0, 0);

static void
xl_dma_map_addr(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg, error;
{
	u_int32_t *paddr;
	
	paddr = arg;
	*paddr = segs->ds_addr;
}

static void
xl_dma_map_txbuf(arg, segs, nseg, mapsize, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg;
	bus_size_t mapsize;
	int error;
{
	struct xl_list *l;
	int i, total_len;

	if (error)
		return;

	KASSERT(nseg <= XL_MAXFRAGS, ("too many DMA segments"));

	total_len = 0;
	l = arg;
	for (i = 0; i < nseg; i++) {
		KASSERT(segs[i].ds_len <= MCLBYTES, ("segment size too large"));
		l->xl_frag[i].xl_addr = segs[i].ds_addr;
		l->xl_frag[i].xl_len = segs[i].ds_len;
		total_len += segs[i].ds_len;
	}
	l->xl_frag[nseg - 1].xl_len |= XL_LAST_FRAG;
	l->xl_status = total_len;
	l->xl_next = 0;
}

/*
 * Murphy's law says that it's possible the chip can wedge and
 * the 'command in progress' bit may never clear. Hence, we wait
 * only a finite amount of time to avoid getting caught in an
 * infinite loop. Normally this delay routine would be a macro,
 * but it isn't called during normal operation so we can afford
 * to make it a function.
 */
static void
xl_wait(sc)
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
		CSR_READ_2(sc, XL_W4_PHY_MGMT) | (x))

#define MII_CLR(x)					\
	CSR_WRITE_2(sc, XL_W4_PHY_MGMT,			\
		CSR_READ_2(sc, XL_W4_PHY_MGMT) & ~(x))

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
xl_mii_sync(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(4);
	MII_SET(XL_MII_DIR|XL_MII_DATA);

	for (i = 0; i < 32; i++) {
		MII_SET(XL_MII_CLK);
		MII_SET(XL_MII_DATA);
		MII_CLR(XL_MII_CLK);
		MII_SET(XL_MII_DATA);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void
xl_mii_send(sc, bits, cnt)
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
		MII_CLR(XL_MII_CLK);
		MII_SET(XL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int
xl_mii_readreg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	int			i, ack;

	XL_LOCK(sc);

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
	MII_SET(XL_MII_CLK);

	/* Turn off xmit. */
	MII_CLR(XL_MII_DIR);

	/* Check for ack */
	MII_CLR(XL_MII_CLK);
	MII_SET(XL_MII_CLK);
	ack = CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(XL_MII_CLK);
			MII_SET(XL_MII_CLK);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(XL_MII_CLK);
		if (!ack) {
			if (CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA)
				frame->mii_data |= i;
		}
		MII_SET(XL_MII_CLK);
	}

fail:

	MII_CLR(XL_MII_CLK);
	MII_SET(XL_MII_CLK);

	XL_UNLOCK(sc);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int
xl_mii_writereg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	XL_LOCK(sc);

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
	MII_CLR(XL_MII_CLK);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(XL_MII_DIR);

	XL_UNLOCK(sc);

	return(0);
}

static int
xl_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct xl_softc		*sc;
	struct xl_mii_frame	frame;

	sc = device_get_softc(dev);

	/*
	 * Pretend that PHYs are only available at MII address 24.
	 * This is to guard against problems with certain 3Com ASIC
	 * revisions that incorrectly map the internal transceiver
	 * control registers at all MII addresses. This can cause
	 * the miibus code to attach the same PHY several times over.
	 */
	if ((!(sc->xl_flags & XL_FLAG_PHYOK)) && phy != 24)
		return(0);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	xl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static int
xl_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct xl_softc		*sc;
	struct xl_mii_frame	frame;

	sc = device_get_softc(dev);

	if ((!(sc->xl_flags & XL_FLAG_PHYOK)) && phy != 24)
		return(0);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	xl_mii_writereg(sc, &frame);

	return(0);
}

static void
xl_miibus_statchg(dev)
	device_t		dev;
{
        struct xl_softc		*sc;
        struct mii_data		*mii;

	
	sc = device_get_softc(dev);
	mii = device_get_softc(sc->xl_miibus);

	XL_LOCK(sc);

	xl_setcfg(sc);

	/* Set ASIC's duplex mode to match the PHY. */
	XL_SEL_WIN(3);
	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
	else
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
			(CSR_READ_1(sc, XL_W3_MAC_CTRL) & ~XL_MACCTRL_DUPLEX));

	XL_UNLOCK(sc);

        return;
}

/*
 * Special support for the 3c905B-COMBO. This card has 10/100 support
 * plus BNC and AUI ports. This means we will have both an miibus attached
 * plus some non-MII media settings. In order to allow this, we have to
 * add the extra media to the miibus's ifmedia struct, but we can't do
 * that during xl_attach() because the miibus hasn't been attached yet.
 * So instead, we wait until the miibus probe/attach is done, at which
 * point we will get a callback telling is that it's safe to add our
 * extra media.
 */
static void
xl_miibus_mediainit(dev)
	device_t		dev;
{
        struct xl_softc		*sc;
        struct mii_data		*mii;
	struct ifmedia		*ifm;
	
	sc = device_get_softc(dev);
	mii = device_get_softc(sc->xl_miibus);
	ifm = &mii->mii_media;

	XL_LOCK(sc);

	if (sc->xl_media & (XL_MEDIAOPT_AUI|XL_MEDIAOPT_10FL)) {
		/*
		 * Check for a 10baseFL board in disguise.
		 */
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			if (bootverbose)
				printf("xl%d: found 10baseFL\n", sc->xl_unit);
			ifmedia_add(ifm, IFM_ETHER|IFM_10_FL, 0, NULL);
			ifmedia_add(ifm, IFM_ETHER|IFM_10_FL|IFM_HDX, 0, NULL);
			if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
				ifmedia_add(ifm,
				    IFM_ETHER|IFM_10_FL|IFM_FDX, 0, NULL);
		} else {
			if (bootverbose)
				printf("xl%d: found AUI\n", sc->xl_unit);
			ifmedia_add(ifm, IFM_ETHER|IFM_10_5, 0, NULL);
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BNC) {
		if (bootverbose)
			printf("xl%d: found BNC\n", sc->xl_unit);
		ifmedia_add(ifm, IFM_ETHER|IFM_10_2, 0, NULL);
	}

	XL_UNLOCK(sc);

	return;
}

/*
 * The EEPROM is slow: give it time to come ready after issuing
 * it a command.
 */
static int
xl_eeprom_wait(sc)
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
static int
xl_read_eeprom(sc, dest, off, cnt, swap)
	struct xl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			err = 0, i;
	u_int16_t		word = 0, *ptr;
#define EEPROM_5BIT_OFFSET(A) ((((A) << 2) & 0x7F00) | ((A) & 0x003F))
#define EEPROM_8BIT_OFFSET(A) ((A) & 0x003F)
	/* WARNING! DANGER!
	 * It's easy to accidentally overwrite the rom content!
	 * Note: the 3c575 uses 8bit EEPROM offsets.
	 */
	XL_SEL_WIN(0);

	if (xl_eeprom_wait(sc))
		return(1);

	if (sc->xl_flags & XL_FLAG_EEPROM_OFFSET_30)
		off += 0x30;

	for (i = 0; i < cnt; i++) {
		if (sc->xl_flags & XL_FLAG_8BITROM)
			CSR_WRITE_2(sc, XL_W0_EE_CMD, 
			    XL_EE_8BIT_READ | EEPROM_8BIT_OFFSET(off + i));
		else
			CSR_WRITE_2(sc, XL_W0_EE_CMD,
			    XL_EE_READ | EEPROM_5BIT_OFFSET(off + i));
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
static void
xl_setmulti(sc)
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

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
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
static void
xl_setmulti_hash(sc)
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
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
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
static void
xl_testpacket(sc)
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

static void
xl_setcfg(sc)
	struct xl_softc		*sc;
{
	u_int32_t		icfg;

	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);
	icfg &= ~XL_ICFG_CONNECTOR_MASK;
	if (sc->xl_media & XL_MEDIAOPT_MII ||
		sc->xl_media & XL_MEDIAOPT_BT4)
		icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
	if (sc->xl_media & XL_MEDIAOPT_BTX)
		icfg |= (XL_XCVR_AUTO << XL_ICFG_CONNECTOR_BITS);

	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	return;
}

static void
xl_setmode(sc, media)
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

static void
xl_reset(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(0);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RESET | 
		    ((sc->xl_flags & XL_FLAG_WEIRDRESET) ?
		     XL_RESETOPT_DISADVFD:0));

	for (i = 0; i < XL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, XL_STATUS) & XL_STAT_CMDBUSY))
			break;
	}

	if (i == XL_TIMEOUT)
		printf("xl%d: reset didn't complete\n", sc->xl_unit);

	/* Reset TX and RX. */
	/* Note: the RX reset takes an absurd amount of time
	 * on newer versions of the Tornado chips such as those
	 * on the 3c905CX and newer 3c908C cards. We wait an
	 * extra amount of time so that xl_wait() doesn't complain
	 * and annoy the users.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	DELAY(100000);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);

	if (sc->xl_flags & XL_FLAG_INVERT_LED_PWR || 
	    sc->xl_flags & XL_FLAG_INVERT_MII_PWR) {
		XL_SEL_WIN(2);
		CSR_WRITE_2(sc, XL_W2_RESET_OPTIONS, CSR_READ_2(sc,
		    XL_W2_RESET_OPTIONS) 
		    | ((sc->xl_flags & XL_FLAG_INVERT_LED_PWR)?XL_RESETOPT_INVERT_LED:0)
		    | ((sc->xl_flags & XL_FLAG_INVERT_MII_PWR)?XL_RESETOPT_INVERT_MII:0)
		    );
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(100000);
        return;
}

/*
 * Probe for a 3Com Etherlink XL chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
xl_probe(dev)
	device_t		dev;
{
	struct xl_type		*t;

	t = xl_devs;

	while(t->xl_name != NULL) {
		if ((pci_get_vendor(dev) == t->xl_vid) &&
		    (pci_get_device(dev) == t->xl_did)) {
			device_set_desc(dev, t->xl_name);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

/*
 * This routine is a kludge to work around possible hardware faults
 * or manufacturing defects that can cause the media options register
 * (or reset options register, as it's called for the first generation
 * 3c90x adapters) to return an incorrect result. I have encountered
 * one Dell Latitude laptop docking station with an integrated 3c905-TX
 * which doesn't have any of the 'mediaopt' bits set. This screws up
 * the attach routine pretty badly because it doesn't know what media
 * to look for. If we find ourselves in this predicament, this routine
 * will try to guess the media options values and warn the user of a
 * possible manufacturing defect with his adapter/system/whatever.
 */
static void
xl_mediacheck(sc)
	struct xl_softc		*sc;
{

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

	xl_choose_xcvr(sc, 1);

	return;
}

static void
xl_choose_xcvr(sc, verbose)
	struct xl_softc		*sc;
	int			verbose;
{
	u_int16_t		devid;

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
		if (verbose)
			printf("xl%d: guessing 10BaseT "
			    "transceiver\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_10BT_COMBO:	/* 3c900-COMBO */
	case TC_DEVICEID_KRAKATOA_10BT_COMBO:	/* 3c900B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_10BT;
		if (verbose)
			printf("xl%d: guessing COMBO "
			    "(AUI/BNC/TP)\n", sc->xl_unit);
		break;
	case TC_DEVICEID_KRAKATOA_10BT_TPC:	/* 3c900B-TPC */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC;
		sc->xl_xcvr = XL_XCVR_10BT;
		if (verbose)
			printf("xl%d: guessing TPC (BNC/TP)\n", sc->xl_unit);
		break;
	case TC_DEVICEID_CYCLONE_10FL:		/* 3c900B-FL */
		sc->xl_media = XL_MEDIAOPT_10FL;
		sc->xl_xcvr = XL_XCVR_AUI;
		if (verbose)
			printf("xl%d: guessing 10baseFL\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_10_100BT:	/* 3c905-TX */
	case TC_DEVICEID_HURRICANE_555:		/* 3c555 */
	case TC_DEVICEID_HURRICANE_556:		/* 3c556 */
	case TC_DEVICEID_HURRICANE_556B:	/* 3c556B */
	case TC_DEVICEID_HURRICANE_575A:	/* 3c575TX */
	case TC_DEVICEID_HURRICANE_575B:	/* 3c575B */
	case TC_DEVICEID_HURRICANE_575C:	/* 3c575C */
	case TC_DEVICEID_HURRICANE_656:		/* 3c656 */
	case TC_DEVICEID_HURRICANE_656B:	/* 3c656B */
	case TC_DEVICEID_TORNADO_656C:		/* 3c656C */
		sc->xl_media = XL_MEDIAOPT_MII;
		sc->xl_xcvr = XL_XCVR_MII;
		if (verbose)
			printf("xl%d: guessing MII\n", sc->xl_unit);
		break;
	case TC_DEVICEID_BOOMERANG_100BT4:	/* 3c905-T4 */
	case TC_DEVICEID_CYCLONE_10_100BT4:	/* 3c905B-T4 */
		sc->xl_media = XL_MEDIAOPT_BT4;
		sc->xl_xcvr = XL_XCVR_MII;
		if (verbose)
			printf("xl%d: guessing 100BaseT4/MII\n", sc->xl_unit);
		break;
	case TC_DEVICEID_HURRICANE_10_100BT:	/* 3c905B-TX */
	case TC_DEVICEID_HURRICANE_10_100BT_SERV:/*3c980-TX */
	case TC_DEVICEID_TORNADO_10_100BT_SERV:	/* 3c980C-TX */
	case TC_DEVICEID_HURRICANE_SOHO100TX:	/* 3cSOHO100-TX */
	case TC_DEVICEID_TORNADO_10_100BT:	/* 3c905C-TX */
	case TC_DEVICEID_TORNADO_HOMECONNECT:	/* 3c450-TX */
		sc->xl_media = XL_MEDIAOPT_BTX;
		sc->xl_xcvr = XL_XCVR_AUTO;
		if (verbose)
			printf("xl%d: guessing 10/100 internal\n", sc->xl_unit);
		break;
	case TC_DEVICEID_CYCLONE_10_100_COMBO:	/* 3c905B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BTX|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_AUTO;
		if (verbose)
			printf("xl%d: guessing 10/100 "
			    "plus BNC/AUI\n", sc->xl_unit);
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
static int
xl_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	int			media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	int			unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->xl_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	XL_LOCK(sc);

	sc->xl_flags = 0;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_555)
		sc->xl_flags |= XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_PHYOK;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_556 ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_556B)
		sc->xl_flags |= XL_FLAG_FUNCREG | XL_FLAG_PHYOK |
		    XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_WEIRDRESET |
		    XL_FLAG_INVERT_LED_PWR | XL_FLAG_INVERT_MII_PWR;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_555 ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_556)
		sc->xl_flags |= XL_FLAG_8BITROM;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_556B)
		sc->xl_flags |= XL_FLAG_NO_XCVR_PWR;

	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_575A ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_575B ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_575C ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_656B ||
	    pci_get_device(dev) == TC_DEVICEID_TORNADO_656C)
		sc->xl_flags |= XL_FLAG_FUNCREG | XL_FLAG_PHYOK |
		    XL_FLAG_EEPROM_OFFSET_30 | XL_FLAG_8BITROM;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_656)
		sc->xl_flags |= XL_FLAG_FUNCREG | XL_FLAG_PHYOK;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_575B)
		sc->xl_flags |= XL_FLAG_INVERT_LED_PWR;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_575C)
		sc->xl_flags |= XL_FLAG_INVERT_MII_PWR;
	if (pci_get_device(dev) == TC_DEVICEID_TORNADO_656C)
		sc->xl_flags |= XL_FLAG_INVERT_MII_PWR;
	if (pci_get_device(dev) == TC_DEVICEID_HURRICANE_656 ||
	    pci_get_device(dev) == TC_DEVICEID_HURRICANE_656B)
		sc->xl_flags |= XL_FLAG_INVERT_MII_PWR |
		    XL_FLAG_INVERT_LED_PWR;

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

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		u_int32_t		iobase, membase, irq;

		/* Save important PCI config data. */
		iobase = pci_read_config(dev, XL_PCI_LOIO, 4);
		membase = pci_read_config(dev, XL_PCI_LOMEM, 4);
		irq = pci_read_config(dev, XL_PCI_INTLINE, 4);

		/* Reset the power state. */
		printf("xl%d: chip is in D%d power mode "
		    "-- setting to D0\n", unit,
		    pci_get_powerstate(dev));

		pci_set_powerstate(dev, PCI_POWERSTATE_D0);

		/* Restore PCI config data. */
		pci_write_config(dev, XL_PCI_LOIO, iobase, 4);
		pci_write_config(dev, XL_PCI_LOMEM, membase, 4);
		pci_write_config(dev, XL_PCI_INTLINE, irq, 4);
	}

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, SYS_RES_IOPORT);
	pci_enable_io(dev, SYS_RES_MEMORY);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

#ifdef XL_USEIOSPACE
	if (!(command & PCIM_CMD_PORTEN)) {
		printf("xl%d: failed to enable I/O ports!\n", unit);
		goto fail;
	}
#else
	if (!(command & PCIM_CMD_MEMEN)) {
		printf("xl%d: failed to enable memory mapping!\n", unit);
		goto fail;
	}
#endif

	rid = XL_RID;
	sc->xl_res = bus_alloc_resource(dev, XL_RES, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->xl_res == NULL) {
		printf ("xl%d: couldn't map ports/memory\n", unit);
		goto fail;
	}

	sc->xl_btag = rman_get_bustag(sc->xl_res);
	sc->xl_bhandle = rman_get_bushandle(sc->xl_res);

	if (sc->xl_flags & XL_FLAG_FUNCREG) {
		rid = XL_PCI_FUNCMEM;
		sc->xl_fres = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
		    0, ~0, 1, RF_ACTIVE);

		if (sc->xl_fres == NULL) {
			printf ("xl%d: couldn't map ports/memory\n", unit);
			goto fail_res;
		}

		sc->xl_ftag = rman_get_bustag(sc->xl_fres);
		sc->xl_fhandle = rman_get_bushandle(sc->xl_fres);
	}

	rid = 0;
	sc->xl_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->xl_irq == NULL) {
		printf("xl%d: couldn't map interrupt\n", unit);
		goto fail_fres;
	}

	error = bus_setup_intr(dev, sc->xl_irq, INTR_TYPE_NET,
	    xl_intr, sc, &sc->xl_intrhand);
	if (error) {
		printf("xl%d: couldn't set up irq\n", unit);
		goto fail_irq;
	}

	/* Reset the adapter. */
	xl_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	if (xl_read_eeprom(sc, (caddr_t)&eaddr, XL_EE_OEM_ADR0, 3, 1)) {
		printf("xl%d: failed to read station address\n", sc->xl_unit);
		goto fail_irq_setup;
	}

	/*
	 * A 3Com chip was detected. Inform the world.
	 */
	printf("xl%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->xl_unit = unit;
	callout_handle_init(&sc->xl_stat_ch);
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/*
	 * Now allocate a tag for the DMA descriptor lists.
	 * All of our lists are allocated as a contiguous block
	 * of memory.
	 */
	error = bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    XL_RX_LIST_SZ, 1, BUS_SPACE_MAXSIZE_32BIT, 0,
	    &sc->xl_ldata.xl_rx_tag);
	if (error) {
		printf("xl%d: failed to allocate dma tag\n", unit);
		goto fail_irq_setup;
	}

	error = bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    XL_TX_LIST_SZ, 1, BUS_SPACE_MAXSIZE_32BIT, 0,
	    &sc->xl_ldata.xl_tx_tag);
	if (error) {
		printf("xl%d: failed to allocate dma tag\n", unit);
		goto fail_rxtag;
	}

	/*
	 * Allocate a DMA tag for the mapping of mbufs.
	 */
	error = bus_dma_tag_create(NULL, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    XL_MAXFRAGS, BUS_SPACE_MAXSIZE_32BIT, 0, &sc->xl_mtag);
	if (error) {
		printf("xl%d: failed to allocate dma tag\n", unit);
		goto fail_txtag;
	}

	/*
	 * Now allocate a chunk of DMA-able memory based on the
	 * tag we just created.
	 */
	error = bus_dmamem_alloc(sc->xl_ldata.xl_tx_tag,
	    (void **)&sc->xl_ldata.xl_tx_list, BUS_DMA_NOWAIT,
	    &sc->xl_ldata.xl_tx_dmamap);
	if (error) {
		printf("xl%d: no memory for list buffers!\n", unit);
		goto fail_mtag;
	}

	error = bus_dmamem_alloc(sc->xl_ldata.xl_rx_tag,
	    (void **)&sc->xl_ldata.xl_rx_list, BUS_DMA_NOWAIT,
	    &sc->xl_ldata.xl_rx_dmamap);
	if (error) {
		printf("xl%d: no memory for list buffers!\n", unit);
		goto fail_txmem;
	}

	bzero(sc->xl_ldata.xl_tx_list, XL_TX_LIST_SZ);
	bzero(sc->xl_ldata.xl_rx_list, XL_RX_LIST_SZ);

	/*
	 * Obtain the DMA address of the RX ring which we'll need later.
	 */
	error = bus_dmamap_load(sc->xl_ldata.xl_rx_tag,
	    sc->xl_ldata.xl_rx_dmamap, &(sc->xl_ldata.xl_rx_list[0]),
	    sizeof(struct xl_list_onefrag), xl_dma_map_addr,
	    &sc->xl_ldata.xl_rx_dmaaddr, 0);
	if (error) {
		printf("xl%d: cannot get dma address of the rx ring!\n", unit);
		goto fail_rxmem;
	}

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

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = unit;
	ifp->if_name = "xl";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = xl_ioctl;
	ifp->if_output = ether_output;
	if (sc->xl_type == XL_TYPE_905B) {
		ifp->if_start = xl_start_90xB;
		ifp->if_hwassist = XL905B_CSUM_FEATURES;
		ifp->if_capabilities = IFCAP_HWCSUM;
	} else
		ifp->if_start = xl_start;
	ifp->if_watchdog = xl_watchdog;
	ifp->if_init = xl_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = XL_TX_LIST_CNT - 1;
	ifp->if_capenable = ifp->if_capabilities;

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
		if (bootverbose)
			printf("xl%d: found MII/AUTO\n", sc->xl_unit);
		xl_setcfg(sc);
		if (mii_phy_probe(dev, &sc->xl_miibus,
		    xl_ifmedia_upd, xl_ifmedia_sts)) {
			printf("xl%d: no PHY found!\n", sc->xl_unit);
			goto fail_rxmem;
		}

		goto done;
	}

	/*
	 * Sanity check. If the user has selected "auto" and this isn't
	 * a 10/100 card of some kind, we need to force the transceiver
	 * type to something sane.
	 */
	if (sc->xl_xcvr == XL_XCVR_AUTO)
		xl_choose_xcvr(sc, bootverbose);

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

	if (sc->xl_media & XL_MEDIAOPT_BFX) {
		if (bootverbose)
			printf("xl%d: found 100baseFX\n", sc->xl_unit);
		ifp->if_baudrate = 100000000;
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_FX, 0, NULL);
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
	case XL_XCVR_100BTX:
	case XL_XCVR_MII:
		/* Chosen by miibus */
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

	if (sc->xl_miibus == NULL)
		ifmedia_set(&sc->ifmedia, media);

done:

	if (sc->xl_flags & XL_FLAG_NO_XCVR_PWR) {
		XL_SEL_WIN(0);
		CSR_WRITE_2(sc, XL_W0_MFG_ID, XL_NO_XCVR_PWR_MAGICBITS);
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	XL_UNLOCK(sc);
	return(0);

fail_rxmem:
	bus_dmamem_free(sc->xl_ldata.xl_rx_tag, sc->xl_ldata.xl_rx_list,
	    sc->xl_ldata.xl_rx_dmamap);
fail_txmem:
	bus_dmamem_free(sc->xl_ldata.xl_tx_tag, sc->xl_ldata.xl_tx_list,
	    sc->xl_ldata.xl_tx_dmamap);
fail_mtag:
	bus_dma_tag_destroy(sc->xl_mtag);
fail_txtag:
	bus_dma_tag_destroy(sc->xl_ldata.xl_tx_tag);
fail_rxtag:
	bus_dma_tag_destroy(sc->xl_ldata.xl_rx_tag);
fail_irq_setup:
	bus_teardown_intr(dev, sc->xl_irq, sc->xl_intrhand);
fail_irq:
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->xl_irq);
fail_fres:
	if (sc->xl_fres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, XL_PCI_FUNCMEM,
		    sc->xl_fres);
fail_res:
	bus_release_resource(dev, XL_RES, XL_RID, sc->xl_res);
fail:
	XL_UNLOCK(sc);
	mtx_destroy(&sc->xl_mtx);

	return(ENXIO);
}

static int
xl_detach(dev)
	device_t		dev;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	XL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	xl_reset(sc);
	xl_stop(sc);
	ether_ifdetach(ifp);

	/* Delete any miibus and phy devices attached to this interface */
	if (sc->xl_miibus != NULL) {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->xl_miibus);
	}

	bus_teardown_intr(dev, sc->xl_irq, sc->xl_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->xl_irq);
	if (sc->xl_fres != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    XL_PCI_FUNCMEM, sc->xl_fres);
	bus_release_resource(dev, XL_RES, XL_RID, sc->xl_res);
	bus_dmamap_unload(sc->xl_ldata.xl_rx_tag, sc->xl_ldata.xl_rx_dmamap);
	bus_dmamap_unload(sc->xl_ldata.xl_tx_tag, sc->xl_ldata.xl_tx_dmamap);
	bus_dmamem_free(sc->xl_ldata.xl_rx_tag, sc->xl_ldata.xl_rx_list,
	    sc->xl_ldata.xl_rx_dmamap);
	bus_dmamem_free(sc->xl_ldata.xl_tx_tag, sc->xl_ldata.xl_tx_list,
	    sc->xl_ldata.xl_tx_dmamap);
	bus_dma_tag_destroy(sc->xl_ldata.xl_rx_tag);
	bus_dma_tag_destroy(sc->xl_ldata.xl_tx_tag);
	bus_dma_tag_destroy(sc->xl_mtag);
	ifmedia_removeall(&sc->ifmedia);

	XL_UNLOCK(sc);
	mtx_destroy(&sc->xl_mtx);

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
xl_list_tx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			error, i;

	cd = &sc->xl_cdata;
	ld = &sc->xl_ldata;
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		cd->xl_tx_chain[i].xl_ptr = &ld->xl_tx_list[i];
		error = bus_dmamap_create(sc->xl_mtag, 0,
		    &cd->xl_tx_chain[i].xl_map);
		if (error)
			return(error);
		error = bus_dmamap_load(ld->xl_tx_tag, ld->xl_tx_dmamap,
		    &ld->xl_tx_list[i], sizeof(struct xl_list),
		    xl_dma_map_addr, &cd->xl_tx_chain[i].xl_phys, 0);
		if (error)
			return(error);
		if (i == (XL_TX_LIST_CNT - 1))
			cd->xl_tx_chain[i].xl_next = NULL;
		else
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[i + 1];
	}

	cd->xl_tx_free = &cd->xl_tx_chain[0];
	cd->xl_tx_tail = cd->xl_tx_head = NULL;

	bus_dmamap_sync(ld->xl_tx_tag, ld->xl_tx_dmamap, BUS_DMASYNC_PREWRITE);
	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
xl_list_tx_init_90xB(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			error, i;

	cd = &sc->xl_cdata;
	ld = &sc->xl_ldata;
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		cd->xl_tx_chain[i].xl_ptr = &ld->xl_tx_list[i];
		error = bus_dmamap_create(sc->xl_mtag, 0,
		    &cd->xl_tx_chain[i].xl_map);
		if (error)
			return(error);
		error = bus_dmamap_load(ld->xl_tx_tag, ld->xl_tx_dmamap,
		    &ld->xl_tx_list[i], sizeof(struct xl_list),
		    xl_dma_map_addr, &cd->xl_tx_chain[i].xl_phys, 0);
		if (error)
			return(error);
		if (i == (XL_TX_LIST_CNT - 1))
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[0];
		else
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[i + 1];
		if (i == 0)
			cd->xl_tx_chain[i].xl_prev =
			    &cd->xl_tx_chain[XL_TX_LIST_CNT - 1];
		else
			cd->xl_tx_chain[i].xl_prev =
			    &cd->xl_tx_chain[i - 1];
	}

	bzero(ld->xl_tx_list, XL_TX_LIST_SZ);
	ld->xl_tx_list[0].xl_status = XL_TXSTAT_EMPTY;

	cd->xl_tx_prod = 1;
	cd->xl_tx_cons = 1;
	cd->xl_tx_cnt = 0;

	bus_dmamap_sync(ld->xl_tx_tag, ld->xl_tx_dmamap, BUS_DMASYNC_PREWRITE);
	return(0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
xl_list_rx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			error, i;

	cd = &sc->xl_cdata;
	ld = &sc->xl_ldata;

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		cd->xl_rx_chain[i].xl_ptr = &ld->xl_rx_list[i];
		error = bus_dmamap_create(sc->xl_mtag, 0,
		    &cd->xl_rx_chain[i].xl_map);
		if (error)
			return(error);
		error = xl_newbuf(sc, &cd->xl_rx_chain[i]);
		if (error)
			return(error);
		if (i == (XL_RX_LIST_CNT - 1)) {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[0];
			error = bus_dmamap_load(ld->xl_rx_tag, ld->xl_rx_dmamap,
			    &ld->xl_rx_list[0], sizeof(struct xl_list_onefrag),
			    xl_dma_map_addr, &ld->xl_rx_list[i].xl_next, 0);
			if (error)
				return(error);
		} else {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[i + 1];
			error = bus_dmamap_load(ld->xl_rx_tag, ld->xl_rx_dmamap,
			    &ld->xl_rx_list[i + 1],
			    sizeof(struct xl_list_onefrag), xl_dma_map_addr,
			    &ld->xl_rx_list[i].xl_next, 0);
			if (error)
				return(error);
		}
	}

	bus_dmamap_sync(ld->xl_rx_tag, ld->xl_rx_dmamap, BUS_DMASYNC_PREWRITE);
	cd->xl_rx_head = &cd->xl_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
xl_newbuf(sc, c)
	struct xl_softc		*sc;
	struct xl_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;
	int			error;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL)
		return(ENOBUFS);

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		m_freem(m_new);
		return(ENOBUFS);
	}

	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	/* Force longword alignment for packet payload. */
	m_adj(m_new, ETHER_ALIGN);

	c->xl_mbuf = m_new;
	c->xl_ptr->xl_frag.xl_len = MCLBYTES | XL_LAST_FRAG;
	c->xl_ptr->xl_status = 0;

	error = bus_dmamap_load(sc->xl_mtag, c->xl_map, mtod(m_new, void *),
	    MCLBYTES, xl_dma_map_addr, &c->xl_ptr->xl_frag.xl_addr, 0);
	if (error) {
		m_freem(m_new);
		printf("xl%d: can't map mbuf (error %d)\n", sc->xl_unit, error);
		return(error);
	}
	bus_dmamap_sync(sc->xl_mtag, c->xl_map, BUS_DMASYNC_PREWRITE);
	return(0);
}

static int
xl_rx_resync(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_onefrag	*pos;
	int			i;

	pos = sc->xl_cdata.xl_rx_head;

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		if (pos->xl_ptr->xl_status)
			break;
		pos = pos->xl_next;
	}

	if (i == XL_RX_LIST_CNT)
		return(0);

	sc->xl_cdata.xl_rx_head = pos;

	return(EAGAIN);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
xl_rxeof(sc)
	struct xl_softc		*sc;
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct xl_chain_onefrag	*cur_rx;
	int			total_len = 0;
	u_int32_t		rxstat;

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
			printf("xl%d: bad receive status -- "
			    "packet dropped\n", sc->xl_unit);
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = 0;
			continue;
		}

		/* No errors; receive the packet. */	
		bus_dmamap_sync(sc->xl_mtag, cur_rx->xl_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->xl_mtag, cur_rx->xl_map);
		m = cur_rx->xl_mbuf;
		total_len = cur_rx->xl_ptr->xl_status & XL_RXSTAT_LENMASK;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (xl_newbuf(sc, cur_rx)) {
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = 0;
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;

		if (sc->xl_type == XL_TYPE_905B) {
			/* Do IP checksum checking. */
			if (rxstat & XL_RXSTAT_IPCKOK)
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if (!(rxstat & XL_RXSTAT_IPCKERR))
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			if ((rxstat & XL_RXSTAT_TCPCOK &&
			     !(rxstat & XL_RXSTAT_TCPCKERR)) ||
			    (rxstat & XL_RXSTAT_UDPCKOK &&
			     !(rxstat & XL_RXSTAT_UDPCKERR))) {
				m->m_pkthdr.csum_flags |=
					CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

		(*ifp->if_input)(ifp, m);
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
		CSR_WRITE_4(sc, XL_UPLIST_PTR, sc->xl_ldata.xl_rx_dmaaddr);
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
static void
xl_txeof(sc)
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

		if (CSR_READ_4(sc, XL_DOWNLIST_PTR))
			break;

		sc->xl_cdata.xl_tx_head = cur_tx->xl_next;
		bus_dmamap_sync(sc->xl_mtag, cur_tx->xl_map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->xl_mtag, cur_tx->xl_map);
		m_freem(cur_tx->xl_mbuf);
		cur_tx->xl_mbuf = NULL;
		ifp->if_opackets++;

		cur_tx->xl_next = sc->xl_cdata.xl_tx_free;
		sc->xl_cdata.xl_tx_free = cur_tx;
	}

	if (sc->xl_cdata.xl_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->xl_cdata.xl_tx_tail = NULL;
	} else {
		if (CSR_READ_4(sc, XL_DMACTL) & XL_DMACTL_DOWN_STALLED ||
			!CSR_READ_4(sc, XL_DOWNLIST_PTR)) {
			CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
				sc->xl_cdata.xl_tx_head->xl_phys);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		}
	}

	return;
}

static void
xl_txeof_90xB(sc)
	struct xl_softc		*sc;
{
	struct xl_chain		*cur_tx = NULL;
	struct ifnet		*ifp;
	int			idx;

	ifp = &sc->arpcom.ac_if;

	idx = sc->xl_cdata.xl_tx_cons;
	while(idx != sc->xl_cdata.xl_tx_prod) {

		cur_tx = &sc->xl_cdata.xl_tx_chain[idx];

		if (!(cur_tx->xl_ptr->xl_status & XL_TXSTAT_DL_COMPLETE))
			break;

		if (cur_tx->xl_mbuf != NULL) {
			bus_dmamap_sync(sc->xl_mtag, cur_tx->xl_map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->xl_mtag, cur_tx->xl_map);
			m_freem(cur_tx->xl_mbuf);
			cur_tx->xl_mbuf = NULL;
		}

		ifp->if_opackets++;

		sc->xl_cdata.xl_tx_cnt--;
		XL_INC(idx, XL_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->xl_cdata.xl_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

/*
 * TX 'end of channel' interrupt handler. Actually, we should
 * only get a 'TX complete' interrupt if there's a transmit error,
 * so this is really TX error handler.
 */
static void
xl_txeoc(sc)
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
			if (sc->xl_type == XL_TYPE_905B) {
				if (sc->xl_cdata.xl_tx_cnt) {
					int			i;
					struct xl_chain		*c;
					i = sc->xl_cdata.xl_tx_cons;
					c = &sc->xl_cdata.xl_tx_chain[i];
					CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
					    c->xl_phys);
					CSR_WRITE_1(sc, XL_DOWN_POLL, 64);
				}
			} else {
				if (sc->xl_cdata.xl_tx_head != NULL)
					CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
					    sc->xl_cdata.xl_tx_head->xl_phys);
			}
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

static void
xl_intr(arg)
	void			*arg;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;

	sc = arg;
	XL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	while((status = CSR_READ_2(sc, XL_STATUS)) & XL_INTRS && status != 0xFFFF) {

		CSR_WRITE_2(sc, XL_COMMAND,
		    XL_CMD_INTR_ACK|(status & XL_INTRS));

		if (status & XL_STAT_UP_COMPLETE) {
			int			curpkts;

			curpkts = ifp->if_ipackets;
			xl_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while (xl_rx_resync(sc))
					xl_rxeof(sc);
			}
		}

		if (status & XL_STAT_DOWN_COMPLETE) {
			if (sc->xl_type == XL_TYPE_905B)
				xl_txeof_90xB(sc);
			else
				xl_txeof(sc);
		}

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

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

	XL_UNLOCK(sc);

	return;
}

static void
xl_stats_update(xsc)
	void			*xsc;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	struct xl_stats		xl_stats;
	u_int8_t		*p;
	int			i;
	struct mii_data		*mii = NULL;

	bzero((char *)&xl_stats, sizeof(struct xl_stats));

	sc = xsc;
	ifp = &sc->arpcom.ac_if;
	if (sc->xl_miibus != NULL)
		mii = device_get_softc(sc->xl_miibus);

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

	if ((mii != NULL) && (!sc->xl_stats_no_timeout))
		mii_tick(mii);

	XL_SEL_WIN(7);

	if (!sc->xl_stats_no_timeout)
		sc->xl_stat_ch = timeout(xl_stats_update, sc, hz);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
xl_encap(sc, c, m_head)
	struct xl_softc		*sc;
	struct xl_chain		*c;
	struct mbuf		*m_head;
{
	struct xl_frag		*f = NULL;
	int			error;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	error = bus_dmamap_load_mbuf(sc->xl_mtag, c->xl_map, m_head,
	    xl_dma_map_txbuf, c->xl_ptr, 0);

	if (error && error != EFBIG) {
		m_freem(m_head);
		printf("xl%d: can't map mbuf (error %d)\n", sc->xl_unit, error);
		return(1);
	}

	/*
	 * Handle special case: we used up all 63 fragments,
	 * but we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (error) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			m_freem(m_head);
			printf("xl%d: no memory for tx list\n", sc->xl_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				m_freem(m_head);
				printf("xl%d: no memory for tx list\n",
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
		error = bus_dmamap_load(sc->xl_mtag, c->xl_map,
		    mtod(m_new, void *), MCLBYTES, xl_dma_map_addr,
		    &f->xl_addr, 0);
		if (error) {
			m_freem(m_new);
			printf("xl%d: can't map mbuf (error %d)\n",
			    sc->xl_unit, error);
			return(1);
		}
		f->xl_len = m_new->m_len | XL_LAST_FRAG;
		c->xl_ptr->xl_status = m_new->m_len;
		c->xl_ptr->xl_next = 0;
	}

	if (sc->xl_type == XL_TYPE_905B) {
		c->xl_ptr->xl_status = XL_TXSTAT_RND_DEFEAT;

		if (m_head->m_pkthdr.csum_flags) {
			if (m_head->m_pkthdr.csum_flags & CSUM_IP)
				c->xl_ptr->xl_status |= XL_TXSTAT_IPCKSUM;
			if (m_head->m_pkthdr.csum_flags & CSUM_TCP)
				c->xl_ptr->xl_status |= XL_TXSTAT_TCPCKSUM;
			if (m_head->m_pkthdr.csum_flags & CSUM_UDP)
				c->xl_ptr->xl_status |= XL_TXSTAT_UDPCKSUM;
		}
	}

	c->xl_mbuf = m_head;
	bus_dmamap_sync(sc->xl_mtag, c->xl_map, BUS_DMASYNC_PREREAD);
	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
static void
xl_start(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct xl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;
	int			error;

	sc = ifp->if_softc;
	XL_LOCK(sc);
	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->xl_cdata.xl_tx_free == NULL) {
		xl_txeoc(sc);
		xl_txeof(sc);
		if (sc->xl_cdata.xl_tx_free == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			XL_UNLOCK(sc);
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

		/* Pack the data into the descriptor. */
		error = xl_encap(sc, cur_tx, m_head);
		if (error)
			continue;

		sc->xl_cdata.xl_tx_free = cur_tx->xl_next;
		cur_tx->xl_next = NULL;

		/* Chain it together. */
		if (prev != NULL) {
			prev->xl_next = cur_tx;
			prev->xl_ptr->xl_next = cur_tx->xl_phys;
		}
		prev = cur_tx;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, cur_tx->xl_mbuf);
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL) {
		XL_UNLOCK(sc);
		return;
	}

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
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_next = start_tx->xl_phys;
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_status &=
					~XL_TXSTAT_DL_INTR;
		sc->xl_cdata.xl_tx_tail = cur_tx;
	} else {
		sc->xl_cdata.xl_tx_head = start_tx;
		sc->xl_cdata.xl_tx_tail = cur_tx;
	}
	if (!CSR_READ_4(sc, XL_DOWNLIST_PTR))
		CSR_WRITE_4(sc, XL_DOWNLIST_PTR, start_tx->xl_phys);

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

	XL_UNLOCK(sc);

	return;
}

static void
xl_start_90xB(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct xl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;
	int			error, idx;

	sc = ifp->if_softc;
	XL_LOCK(sc);

	if (ifp->if_flags & IFF_OACTIVE) {
		XL_UNLOCK(sc);
		return;
	}

	idx = sc->xl_cdata.xl_tx_prod;
	start_tx = &sc->xl_cdata.xl_tx_chain[idx];

	while (sc->xl_cdata.xl_tx_chain[idx].xl_mbuf == NULL) {

		if ((XL_TX_LIST_CNT - sc->xl_cdata.xl_tx_cnt) < 3) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		cur_tx = &sc->xl_cdata.xl_tx_chain[idx];

		/* Pack the data into the descriptor. */
		error = xl_encap(sc, cur_tx, m_head);
		if (error)
			continue;

		/* Chain it together. */
		if (prev != NULL)
			prev->xl_ptr->xl_next = cur_tx->xl_phys;
		prev = cur_tx;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, cur_tx->xl_mbuf);

		XL_INC(idx, XL_TX_LIST_CNT);
		sc->xl_cdata.xl_tx_cnt++;
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL) {
		XL_UNLOCK(sc);
		return;
	}

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interupt once for the whole chain rather than
	 * once for each packet.
	 */
	cur_tx->xl_ptr->xl_status |= XL_TXSTAT_DL_INTR;

	/* Start transmission */
	sc->xl_cdata.xl_tx_prod = idx;
	start_tx->xl_prev->xl_ptr->xl_next = start_tx->xl_phys;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	XL_UNLOCK(sc);

	return;
}

static void
xl_init(xsc)
	void			*xsc;
{
	struct xl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			error, i;
	u_int16_t		rxfilt = 0;
	struct mii_data		*mii = NULL;

	XL_LOCK(sc);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	xl_stop(sc);

	if (sc->xl_miibus == NULL) {
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
		xl_wait(sc);
	}
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
	DELAY(10000);

	if (sc->xl_miibus != NULL)
		mii = device_get_softc(sc->xl_miibus);

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
	error = xl_list_rx_init(sc);
	if (error) {
		printf("xl%d: initialization of the rx ring failed (%d)\n",
		    sc->xl_unit, error);
		xl_stop(sc);
		XL_UNLOCK(sc);
		return;
	}

	/* Init TX descriptors. */
	if (sc->xl_type == XL_TYPE_905B)
		error = xl_list_tx_init_90xB(sc);
	else
		error = xl_list_tx_init(sc);
	if (error) {
		printf("xl%d: initialization of the tx ring failed (%d)\n",
		    sc->xl_unit, error);
		xl_stop(sc);
		XL_UNLOCK(sc);
	}

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
	CSR_WRITE_4(sc, XL_UPLIST_PTR, sc->xl_ldata.xl_rx_dmaaddr);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_UNSTALL);
	xl_wait(sc);


	if (sc->xl_type == XL_TYPE_905B) {
		/* Set polling interval */
		CSR_WRITE_1(sc, XL_DOWN_POLL, 64);
		/* Load the address of the TX list */
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_STALL);
		xl_wait(sc);
		CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
		    sc->xl_cdata.xl_tx_chain[0].xl_phys);
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		xl_wait(sc);
	}

	/*
	 * If the coax transceiver is on, make sure to enable
	 * the DC-DC converter.
 	 */
	XL_SEL_WIN(3);
	if (sc->xl_xcvr == XL_XCVR_COAX)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_START);
	else
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	/* increase packet size to allow reception of 802.1q or ISL packets */
	if (sc->xl_type == XL_TYPE_905B) 
		CSR_WRITE_2(sc, XL_W3_MAXPKTSIZE, XL_PACKET_SIZE);
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
	if (sc->xl_flags & XL_FLAG_FUNCREG)
	    bus_space_write_4(sc->xl_ftag, sc->xl_fhandle, 4, 0x8000);

	/* Set the RX early threshold */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_THRESH|(XL_PACKET_SIZE >>2));
	CSR_WRITE_2(sc, XL_DMACTL, XL_DMACTL_UP_RX_EARLY);

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_ENABLE);
	xl_wait(sc);

	if (mii != NULL)
		mii_mediachg(mii);

	/* Select window 7 for normal operations. */
	XL_SEL_WIN(7);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->xl_stat_ch = timeout(xl_stats_update, sc, hz);

	XL_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
xl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct ifmedia		*ifm = NULL;
	struct mii_data		*mii = NULL;

	sc = ifp->if_softc;
	if (sc->xl_miibus != NULL)
		mii = device_get_softc(sc->xl_miibus);
	if (mii == NULL)
		ifm = &sc->ifmedia;
	else
		ifm = &mii->mii_media;

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_100_FX:
	case IFM_10_FL:
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
		xl_init(sc);
	} else {
		xl_setmode(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
static void
xl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct xl_softc		*sc;
	u_int32_t		icfg;
	struct mii_data		*mii = NULL;

	sc = ifp->if_softc;
	if (sc->xl_miibus != NULL)
		mii = device_get_softc(sc->xl_miibus);

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
		if (mii != NULL) {
			mii_pollstat(mii);
			ifmr->ifm_active = mii->mii_media_active;
			ifmr->ifm_status = mii->mii_media_status;
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

static int
xl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct xl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			error = 0;
	struct mii_data		*mii = NULL;
	u_int8_t		rxfilt;

	XL_LOCK(sc);

	switch(command) {
	case SIOCSIFFLAGS:
		XL_SEL_WIN(5);
		rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->xl_if_flags & IFF_PROMISC)) {
				rxfilt |= XL_RXFILTER_ALLFRAMES;
				CSR_WRITE_2(sc, XL_COMMAND,
				    XL_CMD_RX_SET_FILT|rxfilt);
				XL_SEL_WIN(7);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->xl_if_flags & IFF_PROMISC) {
				rxfilt &= ~XL_RXFILTER_ALLFRAMES;
				CSR_WRITE_2(sc, XL_COMMAND,
				    XL_CMD_RX_SET_FILT|rxfilt);
				XL_SEL_WIN(7);
			} else
				xl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				xl_stop(sc);
		}
		sc->xl_if_flags = ifp->if_flags;
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
		if (sc->xl_miibus != NULL)
			mii = device_get_softc(sc->xl_miibus);
		if (mii == NULL)
			error = ifmedia_ioctl(ifp, ifr,
			    &sc->ifmedia, command);
		else
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	XL_UNLOCK(sc);

	return(error);
}

static void
xl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	u_int16_t		status = 0;

	sc = ifp->if_softc;

	XL_LOCK(sc);

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
	xl_reset(sc);
	xl_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

	XL_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
xl_stop(sc)
	struct xl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	XL_LOCK(sc);

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

#ifdef foo
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
#endif

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ACK|XL_STAT_INTLATCH);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STAT_ENB|0);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB|0);
	if (sc->xl_flags & XL_FLAG_FUNCREG) bus_space_write_4 (sc->xl_ftag, sc->xl_fhandle, 4, 0x8000);

	/* Stop the stats updater. */
	untimeout(xl_stats_update, sc, sc->xl_stat_ch);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_rx_chain[i].xl_mbuf != NULL) {
			bus_dmamap_unload(sc->xl_mtag,
			    sc->xl_cdata.xl_tx_chain[i].xl_map);
			bus_dmamap_destroy(sc->xl_mtag,
			    sc->xl_cdata.xl_tx_chain[i].xl_map);
			m_freem(sc->xl_cdata.xl_rx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_rx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero(sc->xl_ldata.xl_rx_list, XL_RX_LIST_SZ);
	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_tx_chain[i].xl_mbuf != NULL) {
			bus_dmamap_unload(sc->xl_mtag,
			    sc->xl_cdata.xl_tx_chain[i].xl_map);
			bus_dmamap_destroy(sc->xl_mtag,
			    sc->xl_cdata.xl_tx_chain[i].xl_map);
			m_freem(sc->xl_cdata.xl_tx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_tx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero(sc->xl_ldata.xl_tx_list, XL_TX_LIST_SZ);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	XL_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
xl_shutdown(dev)
	device_t		dev;
{
	struct xl_softc		*sc;

	sc = device_get_softc(dev);

	XL_LOCK(sc);
	xl_reset(sc);
	xl_stop(sc);
	XL_UNLOCK(sc);

	return;
}

static int
xl_suspend(dev)
	device_t		dev;
{
	struct xl_softc		*sc;

	sc = device_get_softc(dev);

	XL_LOCK(sc);
	xl_stop(sc);
	XL_UNLOCK(sc);

	return(0);
}

static int
xl_resume(dev)
	device_t		dev;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	XL_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	xl_reset(sc);
	if (ifp->if_flags & IFF_UP)
		xl_init(sc);

	XL_UNLOCK(sc);
	return(0);
}
