/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * VIA Rhine fast ethernet PCI NIC driver
 *
 * Supports various network adapters based on the VIA Rhine
 * and Rhine II PCI controllers, including the D-Link DFE530TX.
 * Datasheets are available at http://www.via.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The VIA Rhine controllers are similar in some respects to the
 * the DEC tulip chips, except less complicated. The controller
 * uses an MII bus and an external physical layer interface. The
 * receiver has a one entry perfect filter and a 64-bit hash table
 * multicast filter. Transmit and receive descriptors are similar
 * to the tulip.
 *
 * The Rhine has a serious flaw in its transmit DMA mechanism:
 * transmit buffers must be longword aligned. Unfortunately,
 * FreeBSD doesn't guarantee that mbufs will be filled in starting
 * at longword boundaries, so we have to do a buffer copy before
 * transmission.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define VR_USEIOSPACE

#include <pci/if_vrreg.h>

MODULE_DEPEND(vr, pci, 1, 1, 1);
MODULE_DEPEND(vr, ether, 1, 1, 1);
MODULE_DEPEND(vr, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#undef VR_USESWSHIFT

/*
 * Various supported device vendors/types and their names.
 */
static struct vr_type vr_devs[] = {
	{ VIA_VENDORID, VIA_DEVICEID_RHINE,
		"VIA VT3043 Rhine I 10/100BaseTX" },
	{ VIA_VENDORID, VIA_DEVICEID_RHINE_II,
		"VIA VT86C100A Rhine II 10/100BaseTX" },
	{ VIA_VENDORID, VIA_DEVICEID_RHINE_II_2,
		"VIA VT6102 Rhine II 10/100BaseTX" },
	{ VIA_VENDORID, VIA_DEVICEID_RHINE_III,
		"VIA VT6105 Rhine III 10/100BaseTX" },
	{ VIA_VENDORID, VIA_DEVICEID_RHINE_III_M,
		"VIA VT6105M Rhine III 10/100BaseTX" },
	{ DELTA_VENDORID, DELTA_DEVICEID_RHINE_II,
		"Delta Electronics Rhine II 10/100BaseTX" },
	{ ADDTRON_VENDORID, ADDTRON_DEVICEID_RHINE_II,
		"Addtron Technology Rhine II 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int vr_probe(device_t);
static int vr_attach(device_t);
static int vr_detach(device_t);

static int vr_newbuf(struct vr_softc *, struct vr_chain_onefrag *,
		struct mbuf *);
static int vr_encap(struct vr_softc *, struct vr_chain *, struct mbuf * );

static void vr_rxeof(struct vr_softc *);
static void vr_rxeoc(struct vr_softc *);
static void vr_txeof(struct vr_softc *);
static void vr_tick(void *);
static void vr_intr(void *);
static void vr_start(struct ifnet *);
static void vr_start_locked(struct ifnet *);
static int vr_ioctl(struct ifnet *, u_long, caddr_t);
static void vr_init(void *);
static void vr_init_locked(struct vr_softc *);
static void vr_stop(struct vr_softc *);
static void vr_watchdog(struct ifnet *);
static void vr_shutdown(device_t);
static int vr_ifmedia_upd(struct ifnet *);
static void vr_ifmedia_sts(struct ifnet *, struct ifmediareq *);

#ifdef VR_USESWSHIFT
static void vr_mii_sync(struct vr_softc *);
static void vr_mii_send(struct vr_softc *, uint32_t, int);
#endif
static int vr_mii_readreg(struct vr_softc *, struct vr_mii_frame *);
static int vr_mii_writereg(struct vr_softc *, struct vr_mii_frame *);
static int vr_miibus_readreg(device_t, uint16_t, uint16_t);
static int vr_miibus_writereg(device_t, uint16_t, uint16_t, uint16_t);
static void vr_miibus_statchg(device_t);

static void vr_setcfg(struct vr_softc *, int);
static void vr_setmulti(struct vr_softc *);
static void vr_reset(struct vr_softc *);
static int vr_list_rx_init(struct vr_softc *);
static int vr_list_tx_init(struct vr_softc *);

#ifdef VR_USEIOSPACE
#define VR_RES			SYS_RES_IOPORT
#define VR_RID			VR_PCI_LOIO
#else
#define VR_RES			SYS_RES_MEMORY
#define VR_RID			VR_PCI_LOMEM
#endif

static device_method_t vr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vr_probe),
	DEVMETHOD(device_attach,	vr_attach),
	DEVMETHOD(device_detach, 	vr_detach),
	DEVMETHOD(device_shutdown,	vr_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	vr_miibus_readreg),
	DEVMETHOD(miibus_writereg,	vr_miibus_writereg),
	DEVMETHOD(miibus_statchg,	vr_miibus_statchg),

	{ 0, 0 }
};

static driver_t vr_driver = {
	"vr",
	vr_methods,
	sizeof(struct vr_softc)
};

static devclass_t vr_devclass;

DRIVER_MODULE(vr, pci, vr_driver, vr_devclass, 0, 0);
DRIVER_MODULE(miibus, vr, miibus_driver, miibus_devclass, 0, 0);

#define VR_SETBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
		CSR_READ_1(sc, reg) | (x))

#define VR_CLRBIT(sc, reg, x)				\
	CSR_WRITE_1(sc, reg,				\
		CSR_READ_1(sc, reg) & ~(x))

#define VR_SETBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
		CSR_READ_2(sc, reg) | (x))

#define VR_CLRBIT16(sc, reg, x)				\
	CSR_WRITE_2(sc, reg,				\
		CSR_READ_2(sc, reg) & ~(x))

#define VR_SETBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define VR_CLRBIT32(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)					\
	CSR_WRITE_1(sc, VR_MIICMD,			\
		CSR_READ_1(sc, VR_MIICMD) | (x))

#define SIO_CLR(x)					\
	CSR_WRITE_1(sc, VR_MIICMD,			\
		CSR_READ_1(sc, VR_MIICMD) & ~(x))

#ifdef VR_USESWSHIFT
/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void
vr_mii_sync(struct vr_softc *sc)
{
	register int	i;

	SIO_SET(VR_MIICMD_DIR|VR_MIICMD_DATAIN);

	for (i = 0; i < 32; i++) {
		SIO_SET(VR_MIICMD_CLK);
		DELAY(1);
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
	}
}

/*
 * Clock a series of bits through the MII.
 */
static void
vr_mii_send(struct vr_softc *sc, uint32_t bits, int cnt)
{
	int	i;

	SIO_CLR(VR_MIICMD_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
		if (bits & i) {
			SIO_SET(VR_MIICMD_DATAIN);
		} else {
			SIO_CLR(VR_MIICMD_DATAIN);
		}
		DELAY(1);
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
		SIO_SET(VR_MIICMD_CLK);
	}
}
#endif

/*
 * Read an PHY register through the MII.
 */
static int
vr_mii_readreg(struct vr_softc *sc, struct vr_mii_frame *frame)
#ifdef VR_USESWSHIFT
{
	int	i, ack;

	/* Set up frame for RX. */
	frame->mii_stdelim = VR_MII_STARTDELIM;
	frame->mii_opcode = VR_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;

	CSR_WRITE_1(sc, VR_MIICMD, 0);
	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);

	/* Turn on data xmit. */
	SIO_SET(VR_MIICMD_DIR);

	vr_mii_sync(sc);

	/* Send command/address info. */
	vr_mii_send(sc, frame->mii_stdelim, 2);
	vr_mii_send(sc, frame->mii_opcode, 2);
	vr_mii_send(sc, frame->mii_phyaddr, 5);
	vr_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit. */
	SIO_CLR((VR_MIICMD_CLK|VR_MIICMD_DATAIN));
	DELAY(1);
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(VR_MIICMD_DIR);

	/* Check for ack */
	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, VR_MIICMD) & VR_MIICMD_DATAOUT;
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(VR_MIICMD_CLK);
			DELAY(1);
			SIO_SET(VR_MIICMD_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(VR_MIICMD_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, VR_MIICMD) & VR_MIICMD_DATAOUT)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(VR_MIICMD_CLK);
		DELAY(1);
	}

fail:
	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);

	if (ack)
		return (1);
	return (0);
}
#else
{
	int	i;

	/* Set the PHY address. */
	CSR_WRITE_1(sc, VR_PHYADDR, (CSR_READ_1(sc, VR_PHYADDR)& 0xe0)|
	    frame->mii_phyaddr);

	/* Set the register address. */
	CSR_WRITE_1(sc, VR_MIIADDR, frame->mii_regaddr);
	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_READ_ENB);

	for (i = 0; i < 10000; i++) {
		if ((CSR_READ_1(sc, VR_MIICMD) & VR_MIICMD_READ_ENB) == 0)
			break;
		DELAY(1);
	}
	frame->mii_data = CSR_READ_2(sc, VR_MIIDATA);

	return (0);
}
#endif


/*
 * Write to a PHY register through the MII.
 */
static int
vr_mii_writereg(struct vr_softc *sc, struct vr_mii_frame *frame)
#ifdef VR_USESWSHIFT
{
	CSR_WRITE_1(sc, VR_MIICMD, 0);
	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_DIRECTPGM);

	/* Set up frame for TX. */
	frame->mii_stdelim = VR_MII_STARTDELIM;
	frame->mii_opcode = VR_MII_WRITEOP;
	frame->mii_turnaround = VR_MII_TURNAROUND;

	/* Turn on data output. */
	SIO_SET(VR_MIICMD_DIR);

	vr_mii_sync(sc);

	vr_mii_send(sc, frame->mii_stdelim, 2);
	vr_mii_send(sc, frame->mii_opcode, 2);
	vr_mii_send(sc, frame->mii_phyaddr, 5);
	vr_mii_send(sc, frame->mii_regaddr, 5);
	vr_mii_send(sc, frame->mii_turnaround, 2);
	vr_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(VR_MIICMD_CLK);
	DELAY(1);
	SIO_CLR(VR_MIICMD_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(VR_MIICMD_DIR);

	return (0);
}
#else
{
	int	i;

	/* Set the PHY address. */
	CSR_WRITE_1(sc, VR_PHYADDR, (CSR_READ_1(sc, VR_PHYADDR)& 0xe0)|
	    frame->mii_phyaddr);

	/* Set the register address and data to write. */
	CSR_WRITE_1(sc, VR_MIIADDR, frame->mii_regaddr);
	CSR_WRITE_2(sc, VR_MIIDATA, frame->mii_data);

	VR_SETBIT(sc, VR_MIICMD, VR_MIICMD_WRITE_ENB);

	for (i = 0; i < 10000; i++) {
		if ((CSR_READ_1(sc, VR_MIICMD) & VR_MIICMD_WRITE_ENB) == 0)
			break;
		DELAY(1);
	}

	return (0);
}
#endif

static int
vr_miibus_readreg(device_t dev, uint16_t phy, uint16_t reg)
{
	struct vr_mii_frame	frame;
	struct vr_softc		*sc = device_get_softc(dev);

	switch (sc->vr_revid) {
	case REV_ID_VT6102_APOLLO:
		if (phy != 1) {
			frame.mii_data = 0;
			goto out;
		}
	default:
		break;
	}

	bzero((char *)&frame, sizeof(frame));
	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	vr_mii_readreg(sc, &frame);

out:
	return (frame.mii_data);
}

static int
vr_miibus_writereg(device_t dev, uint16_t phy, uint16_t reg, uint16_t data)
{
	struct vr_mii_frame	frame;
	struct vr_softc		*sc = device_get_softc(dev);

	switch (sc->vr_revid) {
	case REV_ID_VT6102_APOLLO:
		if (phy != 1)
			return (0);
	default:
		break;
	}

	bzero((char *)&frame, sizeof(frame));
	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;
	vr_mii_writereg(sc, &frame);

	return (0);
}

static void
vr_miibus_statchg(device_t dev)
{
	struct mii_data		*mii;
	struct vr_softc		*sc = device_get_softc(dev);

	mii = device_get_softc(sc->vr_miibus);
	vr_setcfg(sc, mii->mii_media_active);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void
vr_setmulti(struct vr_softc *sc)
{
	struct ifnet		*ifp = sc->vr_ifp;
	int			h = 0;
	uint32_t		hashes[2] = { 0, 0 };
	struct ifmultiaddr	*ifma;
	uint8_t			rxfilt;
	int			mcnt = 0;

	VR_LOCK_ASSERT(sc);

	rxfilt = CSR_READ_1(sc, VR_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= VR_RXCFG_RX_MULTI;
		CSR_WRITE_1(sc, VR_RXCFG, rxfilt);
		CSR_WRITE_4(sc, VR_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, VR_MAR1, 0xFFFFFFFF);
		return;
	}

	/* First, zero out all the existing hash bits. */
	CSR_WRITE_4(sc, VR_MAR0, 0);
	CSR_WRITE_4(sc, VR_MAR1, 0);

	/* Now program new ones. */
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
		rxfilt |= VR_RXCFG_RX_MULTI;
	else
		rxfilt &= ~VR_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, VR_MAR0, hashes[0]);
	CSR_WRITE_4(sc, VR_MAR1, hashes[1]);
	CSR_WRITE_1(sc, VR_RXCFG, rxfilt);
}

/*
 * In order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
static void
vr_setcfg(struct vr_softc *sc, int media)
{
	int	restart = 0;

	VR_LOCK_ASSERT(sc);

	if (CSR_READ_2(sc, VR_COMMAND) & (VR_CMD_TX_ON|VR_CMD_RX_ON)) {
		restart = 1;
		VR_CLRBIT16(sc, VR_COMMAND, (VR_CMD_TX_ON|VR_CMD_RX_ON));
	}

	if ((media & IFM_GMASK) == IFM_FDX)
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);
	else
		VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_FULLDUPLEX);

	if (restart)
		VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON|VR_CMD_RX_ON);
}

static void
vr_reset(struct vr_softc *sc)
{
	register int	i;

	/*VR_LOCK_ASSERT(sc);*/ /* XXX: Called during detach w/o lock. */

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RESET);

	for (i = 0; i < VR_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, VR_COMMAND) & VR_CMD_RESET))
			break;
	}
	if (i == VR_TIMEOUT) {
		if (sc->vr_revid < REV_ID_VT3065_A)
			printf("vr%d: reset never completed!\n", sc->vr_unit);
		else {
			/* Use newer force reset command */
			printf("vr%d: Using force reset command.\n",
			    sc->vr_unit);
			VR_SETBIT(sc, VR_MISC_CR1, VR_MISCCR1_FORSRST);
		}
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
}

/*
 * Probe for a VIA Rhine chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
vr_probe(device_t dev)
{
	struct vr_type	*t = vr_devs;

	while (t->vr_name != NULL) {
		if ((pci_get_vendor(dev) == t->vr_vid) &&
		    (pci_get_device(dev) == t->vr_did)) {
			device_set_desc(dev, t->vr_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
vr_attach(dev)
	device_t		dev;
{
	int			i;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct vr_softc		*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->vr_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);
	sc->vr_revid = pci_read_config(dev, VR_PCI_REVID, 4) & 0x000000FF;

	rid = VR_RID;
	sc->vr_res = bus_alloc_resource_any(dev, VR_RES, &rid, RF_ACTIVE);

	if (sc->vr_res == NULL) {
		printf("vr%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->vr_btag = rman_get_bustag(sc->vr_res);
	sc->vr_bhandle = rman_get_bushandle(sc->vr_res);

	/* Allocate interrupt */
	rid = 0;
	sc->vr_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->vr_irq == NULL) {
		printf("vr%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Windows may put the chip in suspend mode when it
	 * shuts down. Be sure to kick it in the head to wake it
	 * up again.
	 */
	VR_CLRBIT(sc, VR_STICKHW, (VR_STICKHW_DS0|VR_STICKHW_DS1));

	/* Reset the adapter. */
	vr_reset(sc);

	/*
	 * Turn on bit2 (MIION) in PCI configuration register 0x53 during
	 * initialization and disable AUTOPOLL.
	 */
	pci_write_config(dev, VR_PCI_MODE,
	    pci_read_config(dev, VR_PCI_MODE, 4) | (VR_MODE3_MIION << 24), 4);
	VR_CLRBIT(sc, VR_MIICMD, VR_MIICMD_AUTOPOLL);

	/*
	 * Get station address. The way the Rhine chips work,
	 * you're not allowed to directly access the EEPROM once
	 * they've been programmed a special way. Consequently,
	 * we need to read the node address from the PAR0 and PAR1
	 * registers.
	 */
	VR_SETBIT(sc, VR_EECSR, VR_EECSR_LOAD);
	DELAY(200);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		eaddr[i] = CSR_READ_1(sc, VR_PAR0 + i);

	sc->vr_unit = unit;

	sc->vr_ldata = contigmalloc(sizeof(struct vr_list_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->vr_ldata == NULL) {
		printf("vr%d: no memory for list buffers!\n", unit);
		error = ENXIO;
		goto fail;
	}

	bzero(sc->vr_ldata, sizeof(struct vr_list_data));

	ifp = sc->vr_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("vr%d: can not if_alloc()\n", unit);
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vr_ioctl;
	ifp->if_start = vr_start;
	ifp->if_watchdog = vr_watchdog;
	ifp->if_init = vr_init;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, VR_TX_LIST_CNT - 1);
	ifp->if_snd.ifq_maxlen = VR_TX_LIST_CNT - 1;
	IFQ_SET_READY(&ifp->if_snd);
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
	ifp->if_capenable = ifp->if_capabilities;

	/* Do MII setup. */
	if (mii_phy_probe(dev, &sc->vr_miibus,
	    vr_ifmedia_upd, vr_ifmedia_sts)) {
		printf("vr%d: MII without any phy!\n", sc->vr_unit);
		error = ENXIO;
		goto fail;
	}

	callout_handle_init(&sc->vr_stat_ch);

	/* Call MI attach routine. */
	ether_ifattach(ifp, eaddr);

	sc->suspended = 0;

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->vr_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    vr_intr, sc, &sc->vr_intrhand);

	if (error) {
		printf("vr%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		vr_detach(dev);

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
vr_detach(device_t dev)
{
	struct vr_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp = sc->vr_ifp;

	KASSERT(mtx_initialized(&sc->vr_mtx), ("vr mutex not initialized"));

	VR_LOCK(sc);

	sc->suspended = 1;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		vr_stop(sc);
		VR_UNLOCK(sc);		/* XXX: Avoid recursive acquire. */
		ether_ifdetach(ifp);
		VR_LOCK(sc);
	}
	if (ifp)
		if_free(ifp);
	if (sc->vr_miibus)
		device_delete_child(dev, sc->vr_miibus);
	bus_generic_detach(dev);

	if (sc->vr_intrhand)
		bus_teardown_intr(dev, sc->vr_irq, sc->vr_intrhand);
	if (sc->vr_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vr_irq);
	if (sc->vr_res)
		bus_release_resource(dev, VR_RES, VR_RID, sc->vr_res);

	if (sc->vr_ldata)
		contigfree(sc->vr_ldata, sizeof(struct vr_list_data), M_DEVBUF);

	VR_UNLOCK(sc);
	mtx_destroy(&sc->vr_mtx);

	return (0);
}

/*
 * Initialize the transmit descriptors.
 */
static int
vr_list_tx_init(struct vr_softc *sc)
{
	struct vr_chain_data	*cd;
	struct vr_list_data	*ld;
	int			i;

	cd = &sc->vr_cdata;
	ld = sc->vr_ldata;
	for (i = 0; i < VR_TX_LIST_CNT; i++) {
		cd->vr_tx_chain[i].vr_ptr = &ld->vr_tx_list[i];
		if (i == (VR_TX_LIST_CNT - 1))
			cd->vr_tx_chain[i].vr_nextdesc =
				&cd->vr_tx_chain[0];
		else
			cd->vr_tx_chain[i].vr_nextdesc =
				&cd->vr_tx_chain[i + 1];
	}
	cd->vr_tx_cons = cd->vr_tx_prod = &cd->vr_tx_chain[0];

	return (0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
vr_list_rx_init(struct vr_softc *sc)
{
	struct vr_chain_data	*cd;
	struct vr_list_data	*ld;
	int			i;

	VR_LOCK_ASSERT(sc);

	cd = &sc->vr_cdata;
	ld = sc->vr_ldata;

	for (i = 0; i < VR_RX_LIST_CNT; i++) {
		cd->vr_rx_chain[i].vr_ptr =
			(struct vr_desc *)&ld->vr_rx_list[i];
		if (vr_newbuf(sc, &cd->vr_rx_chain[i], NULL) == ENOBUFS)
			return (ENOBUFS);
		if (i == (VR_RX_LIST_CNT - 1)) {
			cd->vr_rx_chain[i].vr_nextdesc =
					&cd->vr_rx_chain[0];
			ld->vr_rx_list[i].vr_next =
					vtophys(&ld->vr_rx_list[0]);
		} else {
			cd->vr_rx_chain[i].vr_nextdesc =
					&cd->vr_rx_chain[i + 1];
			ld->vr_rx_list[i].vr_next =
					vtophys(&ld->vr_rx_list[i + 1]);
		}
	}

	cd->vr_rx_head = &cd->vr_rx_chain[0];

	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 * Note: the length fields are only 11 bits wide, which means the
 * largest size we can specify is 2047. This is important because
 * MCLBYTES is 2048, so we have to subtract one otherwise we'll
 * overflow the field and make a mess.
 */
static int
vr_newbuf(struct vr_softc *sc, struct vr_chain_onefrag *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(uint64_t));

	c->vr_mbuf = m_new;
	c->vr_ptr->vr_status = VR_RXSTAT;
	c->vr_ptr->vr_data = vtophys(mtod(m_new, caddr_t));
	c->vr_ptr->vr_ctl = VR_RXCTL | VR_RXLEN;

	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
vr_rxeof(struct vr_softc *sc)
{
	struct mbuf		*m, *m0;
	struct ifnet		*ifp;
	struct vr_chain_onefrag	*cur_rx;
	int			total_len = 0;
	uint32_t		rxstat;

	VR_LOCK_ASSERT(sc);
	ifp = sc->vr_ifp;

	while (!((rxstat = sc->vr_cdata.vr_rx_head->vr_ptr->vr_status) &
	    VR_RXSTAT_OWN)) {
#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */
		m0 = NULL;
		cur_rx = sc->vr_cdata.vr_rx_head;
		sc->vr_cdata.vr_rx_head = cur_rx->vr_nextdesc;
		m = cur_rx->vr_mbuf;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
		 * comes up in the ring.
		 */
		if (rxstat & VR_RXSTAT_RXERR) {
			ifp->if_ierrors++;
			printf("vr%d: rx error (%02x):", sc->vr_unit,
			    rxstat & 0x000000ff);
			if (rxstat & VR_RXSTAT_CRCERR)
				printf(" crc error");
			if (rxstat & VR_RXSTAT_FRAMEALIGNERR)
				printf(" frame alignment error\n");
			if (rxstat & VR_RXSTAT_FIFOOFLOW)
				printf(" FIFO overflow");
			if (rxstat & VR_RXSTAT_GIANT)
				printf(" received giant packet");
			if (rxstat & VR_RXSTAT_RUNT)
				printf(" received runt packet");
			if (rxstat & VR_RXSTAT_BUSERR)
				printf(" system bus error");
			if (rxstat & VR_RXSTAT_BUFFERR)
				printf("rx buffer error");
			printf("\n");
			vr_newbuf(sc, cur_rx, m);
			continue;
		}

		/* No errors; receive the packet. */
		total_len = VR_RXBYTES(cur_rx->vr_ptr->vr_status);

		/*
		 * XXX The VIA Rhine chip includes the CRC with every
		 * received frame, and there's no way to turn this
		 * behavior off (at least, I can't find anything in
		 * the manual that explains how to do it) so we have
		 * to trim off the CRC manually.
		 */
		total_len -= ETHER_CRC_LEN;

		m0 = m_devget(mtod(m, char *), total_len, ETHER_ALIGN, ifp,
		    NULL);
		vr_newbuf(sc, cur_rx, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m = m0;

		ifp->if_ipackets++;
		VR_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		VR_LOCK(sc);
	}
}

static void
vr_rxeoc(struct vr_softc *sc)
{
	struct ifnet		*ifp = sc->vr_ifp;
	int			i;

	VR_LOCK_ASSERT(sc);

	ifp->if_ierrors++;

	VR_CLRBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	DELAY(10000);

	/* Wait for receiver to stop */
	for (i = 0x400;
	     i && (CSR_READ_2(sc, VR_COMMAND) & VR_CMD_RX_ON);
	     i--) {
		;
	}

	if (!i) {
		printf("vr%d: rx shutdown error!\n", sc->vr_unit);
		sc->vr_flags |= VR_F_RESTART;
		return;
	}

	vr_rxeof(sc);

	CSR_WRITE_4(sc, VR_RXADDR, vtophys(sc->vr_cdata.vr_rx_head->vr_ptr));
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_ON);
	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_RX_GO);
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void
vr_txeof(struct vr_softc *sc)
{
	struct vr_chain		*cur_tx;
	struct ifnet		*ifp = sc->vr_ifp;

	VR_LOCK_ASSERT(sc);

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	cur_tx = sc->vr_cdata.vr_tx_cons;
	while (cur_tx->vr_mbuf != NULL) {
		uint32_t		txstat;
		int			i;

		txstat = cur_tx->vr_ptr->vr_status;

		if ((txstat & VR_TXSTAT_ABRT) ||
		    (txstat & VR_TXSTAT_UDF)) {
			for (i = 0x400;
			     i && (CSR_READ_2(sc, VR_COMMAND) & VR_CMD_TX_ON);
			     i--)
				;	/* Wait for chip to shutdown */
			if (!i) {
				printf("vr%d: tx shutdown timeout\n",
				    sc->vr_unit);
				sc->vr_flags |= VR_F_RESTART;
				break;
			}
			VR_TXOWN(cur_tx) = VR_TXSTAT_OWN;
			CSR_WRITE_4(sc, VR_TXADDR, vtophys(cur_tx->vr_ptr));
			break;
		}

		if (txstat & VR_TXSTAT_OWN)
			break;

		if (txstat & VR_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & VR_TXSTAT_DEFER)
				ifp->if_collisions++;
			if (txstat & VR_TXSTAT_LATECOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=(txstat & VR_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		m_freem(cur_tx->vr_mbuf);
		cur_tx->vr_mbuf = NULL;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		cur_tx = cur_tx->vr_nextdesc;
	}
	sc->vr_cdata.vr_tx_cons = cur_tx;
	if (cur_tx->vr_mbuf == NULL)
		ifp->if_timer = 0;
}

static void
vr_tick(void *xsc)
{
	struct vr_softc		*sc = xsc;
	struct mii_data		*mii;

	VR_LOCK(sc);

	if (sc->vr_flags & VR_F_RESTART) {
		printf("vr%d: restarting\n", sc->vr_unit);
		vr_stop(sc);
		vr_reset(sc);
		vr_init_locked(sc);
		sc->vr_flags &= ~VR_F_RESTART;
	}

	mii = device_get_softc(sc->vr_miibus);
	mii_tick(mii);
	sc->vr_stat_ch = timeout(vr_tick, sc, hz);

	VR_UNLOCK(sc);
}

#ifdef DEVICE_POLLING
static poll_handler_t vr_poll;
static poll_handler_t vr_poll_locked;

static void
vr_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct vr_softc *sc = ifp->if_softc;

	VR_LOCK(sc);
	vr_poll_locked(ifp, cmd, count);
	VR_UNLOCK(sc);
}

static void
vr_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct vr_softc *sc = ifp->if_softc;

	VR_LOCK_ASSERT(sc);

	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}

	if (cmd == POLL_DEREGISTER) {
		/* Final call, enable interrupts. */
		CSR_WRITE_2(sc, VR_IMR, VR_INTRS);
		return;
	}

	sc->rxcycles = count;
	vr_rxeof(sc);
	vr_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vr_start_locked(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) {
		uint16_t status;

		/* Also check status register. */
		status = CSR_READ_2(sc, VR_ISR);
		if (status)
			CSR_WRITE_2(sc, VR_ISR, status);

		if ((status & VR_INTRS) == 0)
			return;

		if (status & VR_ISR_RX_DROPPED) {
			printf("vr%d: rx packet lost\n", sc->vr_unit);
			ifp->if_ierrors++;
		}

		if ((status & VR_ISR_RX_ERR) || (status & VR_ISR_RX_NOBUF) ||
		    (status & VR_ISR_RX_NOBUF) || (status & VR_ISR_RX_OFLOW)) {
			printf("vr%d: receive error (%04x)",
			       sc->vr_unit, status);
			if (status & VR_ISR_RX_NOBUF)
				printf(" no buffers");
			if (status & VR_ISR_RX_OFLOW)
				printf(" overflow");
			if (status & VR_ISR_RX_DROPPED)
				printf(" packet lost");
			printf("\n");
			vr_rxeoc(sc);
		}

		if ((status & VR_ISR_BUSERR) ||
		    (status & VR_ISR_TX_UNDERRUN)) {
			vr_reset(sc);
			vr_init_locked(sc);
			return;
		}

		if ((status & VR_ISR_UDFI) ||
		    (status & VR_ISR_TX_ABRT2) ||
		    (status & VR_ISR_TX_ABRT)) {
			ifp->if_oerrors++;
			if (sc->vr_cdata.vr_tx_cons->vr_mbuf != NULL) {
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_ON);
				VR_SETBIT16(sc, VR_COMMAND, VR_CMD_TX_GO);
			}
		}
	}
}
#endif /* DEVICE_POLLING */

static void
vr_intr(void *arg)
{
	struct vr_softc		*sc = arg;
	struct ifnet		*ifp = sc->vr_ifp;
	uint16_t		status;

	VR_LOCK(sc);

	if (sc->suspended) {
		/*
		 * Forcibly disable interrupts.
		 * XXX: Mobile VIA based platforms may need
		 * interrupt re-enable on resume.
		 */
		CSR_WRITE_2(sc, VR_IMR, 0x0000);
		goto done_locked;
	}

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		goto done_locked;

	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(vr_poll, ifp)) {
		/* OK, disable interrupts. */
		CSR_WRITE_2(sc, VR_IMR, 0x0000);
		vr_poll_locked(ifp, 0, 1);
		goto done_locked;
	}
#endif /* DEVICE_POLLING */

	/* Suppress unwanted interrupts. */
	if (!(ifp->if_flags & IFF_UP)) {
		vr_stop(sc);
		goto done_locked;
	}

	/* Disable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, 0x0000);

	for (;;) {
		status = CSR_READ_2(sc, VR_ISR);
		if (status)
			CSR_WRITE_2(sc, VR_ISR, status);

		if ((status & VR_INTRS) == 0)
			break;

		if (status & VR_ISR_RX_OK)
			vr_rxeof(sc);

		if (status & VR_ISR_RX_DROPPED) {
			printf("vr%d: rx packet lost\n", sc->vr_unit);
			ifp->if_ierrors++;
		}

		if ((status & VR_ISR_RX_ERR) || (status & VR_ISR_RX_NOBUF) ||
		    (status & VR_ISR_RX_NOBUF) || (status & VR_ISR_RX_OFLOW)) {
			printf("vr%d: receive error (%04x)",
			       sc->vr_unit, status);
			if (status & VR_ISR_RX_NOBUF)
				printf(" no buffers");
			if (status & VR_ISR_RX_OFLOW)
				printf(" overflow");
			if (status & VR_ISR_RX_DROPPED)
				printf(" packet lost");
			printf("\n");
			vr_rxeoc(sc);
		}

		if ((status & VR_ISR_BUSERR) || (status & VR_ISR_TX_UNDERRUN)) {
			vr_reset(sc);
			vr_init_locked(sc);
			break;
		}

		if ((status & VR_ISR_TX_OK) || (status & VR_ISR_TX_ABRT) ||
		    (status & VR_ISR_TX_ABRT2) || (status & VR_ISR_UDFI)) {
			vr_txeof(sc);
			if ((status & VR_ISR_UDFI) ||
			    (status & VR_ISR_TX_ABRT2) ||
			    (status & VR_ISR_TX_ABRT)) {
				ifp->if_oerrors++;
				if (sc->vr_cdata.vr_tx_cons->vr_mbuf != NULL) {
					VR_SETBIT16(sc, VR_COMMAND,
					    VR_CMD_TX_ON);
					VR_SETBIT16(sc, VR_COMMAND,
					    VR_CMD_TX_GO);
				}
			}
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vr_start_locked(ifp);

done_locked:
	VR_UNLOCK(sc);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
vr_encap(struct vr_softc *sc, struct vr_chain *c, struct mbuf *m_head)
{
	struct vr_desc		*f = NULL;
	struct mbuf		*m;

	VR_LOCK_ASSERT(sc);
	/*
	 * The VIA Rhine wants packet buffers to be longword
	 * aligned, but very often our mbufs aren't. Rather than
	 * waste time trying to decide when to copy and when not
	 * to copy, just do it all the time.
	 */
	m = m_defrag(m_head, M_DONTWAIT);
	if (m == NULL)
		return (1);

	/*
	 * The Rhine chip doesn't auto-pad, so we have to make
	 * sure to pad short frames out to the minimum frame length
	 * ourselves.
	 */
	if (m->m_len < VR_MIN_FRAMELEN) {
		m->m_pkthdr.len += VR_MIN_FRAMELEN - m->m_len;
		m->m_len = m->m_pkthdr.len;
	}

	c->vr_mbuf = m;
	f = c->vr_ptr;
	f->vr_data = vtophys(mtod(m, caddr_t));
	f->vr_ctl = m->m_len;
	f->vr_ctl |= VR_TXCTL_TLINK|VR_TXCTL_FIRSTFRAG;
	f->vr_status = 0;
	f->vr_ctl |= VR_TXCTL_LASTFRAG|VR_TXCTL_FINT;
	f->vr_next = vtophys(c->vr_nextdesc->vr_ptr);

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

static void
vr_start(struct ifnet *ifp)
{
	struct vr_softc		*sc = ifp->if_softc;

	VR_LOCK(sc);
	vr_start_locked(ifp);
	VR_UNLOCK(sc);
}

static void
vr_start_locked(struct ifnet *ifp)
{
	struct vr_softc		*sc = ifp->if_softc;
	struct mbuf		*m_head;
	struct vr_chain		*cur_tx;

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
		return;

	cur_tx = sc->vr_cdata.vr_tx_prod;
	while (cur_tx->vr_mbuf == NULL) {
       	        IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pack the data into the descriptor. */
		if (vr_encap(sc, cur_tx, m_head)) {
			/* Rollback, send what we were able to encap. */
               		IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		VR_TXOWN(cur_tx) = VR_TXSTAT_OWN;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, cur_tx->vr_mbuf);

		cur_tx = cur_tx->vr_nextdesc;
	}
	if (cur_tx != sc->vr_cdata.vr_tx_prod || cur_tx->vr_mbuf != NULL) {
		sc->vr_cdata.vr_tx_prod = cur_tx;

		/* Tell the chip to start transmitting. */
		VR_SETBIT16(sc, VR_COMMAND, /*VR_CMD_TX_ON|*/ VR_CMD_TX_GO);

		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = 5;

		if (cur_tx->vr_mbuf != NULL)
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}
}

static void
vr_init(void *xsc)
{
	struct vr_softc		*sc = xsc;

	VR_LOCK(sc);
	vr_init_locked(sc);
	VR_UNLOCK(sc);
}

static void
vr_init_locked(struct vr_softc *sc)
{
	struct ifnet		*ifp = sc->vr_ifp;
	struct mii_data		*mii;
	int			i;

	VR_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->vr_miibus);

	/* Cancel pending I/O and free all RX/TX buffers. */
	vr_stop(sc);
	vr_reset(sc);

	/* Set our station address. */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		CSR_WRITE_1(sc, VR_PAR0 + i, IFP2ENADDR(sc->vr_ifp)[i]);

	/* Set DMA size. */
	VR_CLRBIT(sc, VR_BCR0, VR_BCR0_DMA_LENGTH);
	VR_SETBIT(sc, VR_BCR0, VR_BCR0_DMA_STORENFWD);

	/*
	 * BCR0 and BCR1 can override the RXCFG and TXCFG registers,
	 * so we must set both.
	 */
	VR_CLRBIT(sc, VR_BCR0, VR_BCR0_RX_THRESH);
	VR_SETBIT(sc, VR_BCR0, VR_BCR0_RXTHRESH128BYTES);

	VR_CLRBIT(sc, VR_BCR1, VR_BCR1_TX_THRESH);
	VR_SETBIT(sc, VR_BCR1, VR_BCR1_TXTHRESHSTORENFWD);

	VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_THRESH);
	VR_SETBIT(sc, VR_RXCFG, VR_RXTHRESH_128BYTES);

	VR_CLRBIT(sc, VR_TXCFG, VR_TXCFG_TX_THRESH);
	VR_SETBIT(sc, VR_TXCFG, VR_TXTHRESH_STORENFWD);

	/* Init circular RX list. */
	if (vr_list_rx_init(sc) == ENOBUFS) {
		printf(
"vr%d: initialization failed: no memory for rx buffers\n", sc->vr_unit);
		vr_stop(sc);
		return;
	}

	/* Init tx descriptors. */
	vr_list_tx_init(sc);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_PROMISC);

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST)
		VR_SETBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);
	else
		VR_CLRBIT(sc, VR_RXCFG, VR_RXCFG_RX_BROAD);

	/*
	 * Program the multicast filter, if necessary.
	 */
	vr_setmulti(sc);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, VR_RXADDR, vtophys(sc->vr_cdata.vr_rx_head->vr_ptr));

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, VR_COMMAND, VR_CMD_TX_NOPOLL|VR_CMD_START|
				    VR_CMD_TX_ON|VR_CMD_RX_ON|
				    VR_CMD_RX_GO);

	CSR_WRITE_4(sc, VR_TXADDR, vtophys(&sc->vr_ldata->vr_tx_list[0]));

	CSR_WRITE_2(sc, VR_ISR, 0xFFFF);
#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, VR_IMR, 0);
	else
#endif /* DEVICE_POLLING */
	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, VR_IMR, VR_INTRS);

	mii_mediachg(mii);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->vr_stat_ch = timeout(vr_tick, sc, hz);
}

/*
 * Set media options.
 */
static int
vr_ifmedia_upd(struct ifnet *ifp)
{
	struct vr_softc		*sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		vr_init(sc);

	return (0);
}

/*
 * Report current media status.
 */
static void
vr_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct vr_softc		*sc = ifp->if_softc;
	struct mii_data		*mii;

	mii = device_get_softc(sc->vr_miibus);
	VR_LOCK(sc);
	mii_pollstat(mii);
	VR_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static int
vr_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct vr_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		VR_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			vr_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				vr_stop(sc);
		}
		VR_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		VR_LOCK(sc);
		vr_setmulti(sc);
		VR_UNLOCK(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->vr_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
		ifp->if_capenable = ifr->ifr_reqcap;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
vr_watchdog(struct ifnet *ifp)
{
	struct vr_softc		*sc = ifp->if_softc;

	VR_LOCK(sc);

	ifp->if_oerrors++;
	printf("vr%d: watchdog timeout\n", sc->vr_unit);

	vr_stop(sc);
	vr_reset(sc);
	vr_init_locked(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vr_start_locked(ifp);

	VR_UNLOCK(sc);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
vr_stop(struct vr_softc *sc)
{
	register int	i;
	struct ifnet	*ifp;

	VR_LOCK_ASSERT(sc);

	ifp = sc->vr_ifp;
	ifp->if_timer = 0;

	untimeout(vr_tick, sc, sc->vr_stat_ch);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	VR_SETBIT16(sc, VR_COMMAND, VR_CMD_STOP);
	VR_CLRBIT16(sc, VR_COMMAND, (VR_CMD_RX_ON|VR_CMD_TX_ON));
	CSR_WRITE_2(sc, VR_IMR, 0x0000);
	CSR_WRITE_4(sc, VR_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, VR_RXADDR, 0x00000000);

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < VR_RX_LIST_CNT; i++) {
		if (sc->vr_cdata.vr_rx_chain[i].vr_mbuf != NULL) {
			m_freem(sc->vr_cdata.vr_rx_chain[i].vr_mbuf);
			sc->vr_cdata.vr_rx_chain[i].vr_mbuf = NULL;
		}
	}
	bzero((char *)&sc->vr_ldata->vr_rx_list,
	    sizeof(sc->vr_ldata->vr_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < VR_TX_LIST_CNT; i++) {
		if (sc->vr_cdata.vr_tx_chain[i].vr_mbuf != NULL) {
			m_freem(sc->vr_cdata.vr_tx_chain[i].vr_mbuf);
			sc->vr_cdata.vr_tx_chain[i].vr_mbuf = NULL;
		}
	}
	bzero((char *)&sc->vr_ldata->vr_tx_list,
	    sizeof(sc->vr_ldata->vr_tx_list));
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
vr_shutdown(device_t dev)
{

	vr_detach(dev);
}
