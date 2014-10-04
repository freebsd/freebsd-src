/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * TI Common Platform Ethernet Switch (CPSW) Driver
 * Found in TI8148 "DaVinci" and AM335x "Sitara" SoCs.
 *
 * This controller is documented in the AM335x Technical Reference
 * Manual, in the TMS320DM814x DaVinci Digital Video Processors TRM
 * and in the TMS320C6452 3 Port Switch Ethernet Subsystem TRM.
 *
 * It is basically a single Ethernet port (port 0) wired internally to
 * a 3-port store-and-forward switch connected to two independent
 * "sliver" controllers (port 1 and port 2).  You can operate the
 * controller in a variety of different ways by suitably configuring
 * the slivers and the Address Lookup Engine (ALE) that routes packets
 * between the ports.
 *
 * This code was developed and tested on a BeagleBone with
 * an AM335x SoC.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "if_cpswreg.h"
#include "if_cpswvar.h"
 
#include <arm/ti/ti_scm.h>

#include "miibus_if.h"

/* Device probe/attach/detach. */
static int cpsw_probe(device_t);
static void cpsw_init_slots(struct cpsw_softc *);
static int cpsw_attach(device_t);
static void cpsw_free_slot(struct cpsw_softc *, struct cpsw_slot *);
static int cpsw_detach(device_t);

/* Device Init/shutdown. */
static void cpsw_init(void *);
static void cpsw_init_locked(void *);
static int cpsw_shutdown(device_t);
static void cpsw_shutdown_locked(struct cpsw_softc *);

/* Device Suspend/Resume. */
static int cpsw_suspend(device_t);
static int cpsw_resume(device_t);

/* Ioctl. */
static int cpsw_ioctl(struct ifnet *, u_long command, caddr_t data);

static int cpsw_miibus_readreg(device_t, int phy, int reg);
static int cpsw_miibus_writereg(device_t, int phy, int reg, int value);

/* Send/Receive packets. */
static void cpsw_intr_rx(void *arg);
static struct mbuf *cpsw_rx_dequeue(struct cpsw_softc *);
static void cpsw_rx_enqueue(struct cpsw_softc *);
static void cpsw_start(struct ifnet *);
static void cpsw_tx_enqueue(struct cpsw_softc *);
static int cpsw_tx_dequeue(struct cpsw_softc *);

/* Misc interrupts and watchdog. */
static void cpsw_intr_rx_thresh(void *);
static void cpsw_intr_misc(void *);
static void cpsw_tick(void *);
static void cpsw_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int cpsw_ifmedia_upd(struct ifnet *);
static void cpsw_tx_watchdog(struct cpsw_softc *);

/* ALE support */
static void cpsw_ale_read_entry(struct cpsw_softc *, uint16_t idx, uint32_t *ale_entry);
static void cpsw_ale_write_entry(struct cpsw_softc *, uint16_t idx, uint32_t *ale_entry);
static int cpsw_ale_mc_entry_set(struct cpsw_softc *, uint8_t portmap, uint8_t *mac);
static int cpsw_ale_update_addresses(struct cpsw_softc *, int purge);
static void cpsw_ale_dump_table(struct cpsw_softc *);

/* Statistics and sysctls. */
static void cpsw_add_sysctls(struct cpsw_softc *);
static void cpsw_stats_collect(struct cpsw_softc *);
static int cpsw_stats_sysctl(SYSCTL_HANDLER_ARGS);

/*
 * Arbitrary limit on number of segments in an mbuf to be transmitted.
 * Packets with more segments than this will be defragmented before
 * they are queued.
 */
#define CPSW_TXFRAGS 8


/*
 * TODO: The CPSW subsystem (CPSW_SS) can drive two independent PHYs
 * as separate Ethernet ports.  To properly support this, we should
 * break this into two separate devices: a CPSW_SS device that owns
 * the interrupts and actually talks to the CPSW hardware, and a
 * separate CPSW Ethernet child device for each Ethernet port.  The RX
 * interrupt, for example, would be part of CPSW_SS; it would receive
 * a packet, note the input port, and then dispatch it to the child
 * device's interface queue.  Similarly for transmit.
 *
 * It's not clear to me whether the device tree should be restructured
 * with a cpsw_ss node and two child nodes.  That would allow specifying
 * MAC addresses for each port, for example, but might be overkill.
 *
 * Unfortunately, I don't have hardware right now that supports two
 * Ethernet ports via CPSW.
 */

static device_method_t cpsw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpsw_probe),
	DEVMETHOD(device_attach,	cpsw_attach),
	DEVMETHOD(device_detach,	cpsw_detach),
	DEVMETHOD(device_shutdown,	cpsw_shutdown),
	DEVMETHOD(device_suspend,	cpsw_suspend),
	DEVMETHOD(device_resume,	cpsw_resume),
	/* MII interface */
	DEVMETHOD(miibus_readreg,	cpsw_miibus_readreg),
	DEVMETHOD(miibus_writereg,	cpsw_miibus_writereg),
	{ 0, 0 }
};

static driver_t cpsw_driver = {
	"cpsw",
	cpsw_methods,
	sizeof(struct cpsw_softc),
};

static devclass_t cpsw_devclass;

DRIVER_MODULE(cpsw, simplebus, cpsw_driver, cpsw_devclass, 0, 0);
DRIVER_MODULE(miibus, cpsw, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(cpsw, ether, 1, 1, 1);
MODULE_DEPEND(cpsw, miibus, 1, 1, 1);

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 1, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 2, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 3, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

/* Number of entries here must match size of stats
 * array in struct cpsw_softc. */
static struct cpsw_stat {
	int	reg;
	char *oid;
} cpsw_stat_sysctls[CPSW_SYSCTL_COUNT] = {
	{0x00, "GoodRxFrames"},
	{0x04, "BroadcastRxFrames"},
	{0x08, "MulticastRxFrames"},
	{0x0C, "PauseRxFrames"},
	{0x10, "RxCrcErrors"},
	{0x14, "RxAlignErrors"},
	{0x18, "OversizeRxFrames"},
	{0x1c, "RxJabbers"},
	{0x20, "ShortRxFrames"},
	{0x24, "RxFragments"},
	{0x30, "RxOctets"},
	{0x34, "GoodTxFrames"},
	{0x38, "BroadcastTxFrames"},
	{0x3c, "MulticastTxFrames"},
	{0x40, "PauseTxFrames"},
	{0x44, "DeferredTxFrames"},
	{0x48, "CollisionsTxFrames"},
	{0x4c, "SingleCollisionTxFrames"},
	{0x50, "MultipleCollisionTxFrames"},
	{0x54, "ExcessiveCollisions"},
	{0x58, "LateCollisions"},
	{0x5c, "TxUnderrun"},
	{0x60, "CarrierSenseErrors"},
	{0x64, "TxOctets"},
	{0x68, "RxTx64OctetFrames"},
	{0x6c, "RxTx65to127OctetFrames"},
	{0x70, "RxTx128to255OctetFrames"},
	{0x74, "RxTx256to511OctetFrames"},
	{0x78, "RxTx512to1024OctetFrames"},
	{0x7c, "RxTx1024upOctetFrames"},
	{0x80, "NetOctets"},
	{0x84, "RxStartOfFrameOverruns"},
	{0x88, "RxMiddleOfFrameOverruns"},
	{0x8c, "RxDmaOverruns"}
};

/*
 * Basic debug support.
 */

#define IF_DEBUG(sc)  if (sc->cpsw_if_flags & IFF_DEBUG)

static void
cpsw_debugf_head(const char *funcname)
{
	int t = (int)(time_second % (24 * 60 * 60));

	printf("%02d:%02d:%02d %s ", t / (60 * 60), (t / 60) % 60, t % 60, funcname);
}

#include <machine/stdarg.h>
static void
cpsw_debugf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

}

#define CPSW_DEBUGF(a) do {					\
	IF_DEBUG(sc) {						\
		cpsw_debugf_head(__func__);			\
		cpsw_debugf a;					\
	}							\
} while (0)


/*
 * Locking macros
 */
#define CPSW_TX_LOCK(sc) do {					\
		mtx_assert(&(sc)->rx.lock, MA_NOTOWNED);		\
		mtx_lock(&(sc)->tx.lock);				\
} while (0)

#define CPSW_TX_UNLOCK(sc)	mtx_unlock(&(sc)->tx.lock)
#define CPSW_TX_LOCK_ASSERT(sc)	mtx_assert(&(sc)->tx.lock, MA_OWNED)

#define CPSW_RX_LOCK(sc) do {					\
		mtx_assert(&(sc)->tx.lock, MA_NOTOWNED);		\
		mtx_lock(&(sc)->rx.lock);				\
} while (0)

#define CPSW_RX_UNLOCK(sc)		mtx_unlock(&(sc)->rx.lock)
#define CPSW_RX_LOCK_ASSERT(sc)	mtx_assert(&(sc)->rx.lock, MA_OWNED)

#define CPSW_GLOBAL_LOCK(sc) do {					\
		if ((mtx_owned(&(sc)->tx.lock) ? 1 : 0) !=	\
		    (mtx_owned(&(sc)->rx.lock) ? 1 : 0)) {		\
			panic("cpsw deadlock possibility detection!");	\
		}							\
		mtx_lock(&(sc)->tx.lock);				\
		mtx_lock(&(sc)->rx.lock);				\
} while (0)

#define CPSW_GLOBAL_UNLOCK(sc) do {					\
		CPSW_RX_UNLOCK(sc);				\
		CPSW_TX_UNLOCK(sc);				\
} while (0)

#define CPSW_GLOBAL_LOCK_ASSERT(sc) do {				\
		CPSW_TX_LOCK_ASSERT(sc);				\
		CPSW_RX_LOCK_ASSERT(sc);				\
} while (0)

/*
 * Read/Write macros
 */
#define	cpsw_read_4(sc, reg)		bus_read_4(sc->res[0], reg)
#define	cpsw_write_4(sc, reg, val)	bus_write_4(sc->res[0], reg, val)

#define	cpsw_cpdma_bd_offset(i)	(CPSW_CPPI_RAM_OFFSET + ((i)*16))

#define	cpsw_cpdma_bd_paddr(sc, slot)				\
	BUS_SPACE_PHYSADDR(sc->res[0], slot->bd_offset)
#define	cpsw_cpdma_read_bd(sc, slot, val)				\
	bus_read_region_4(sc->res[0], slot->bd_offset, (uint32_t *) val, 4)
#define	cpsw_cpdma_write_bd(sc, slot, val)				\
	bus_write_region_4(sc->res[0], slot->bd_offset, (uint32_t *) val, 4)
#define	cpsw_cpdma_write_bd_next(sc, slot, next_slot)			\
	cpsw_write_4(sc, slot->bd_offset, cpsw_cpdma_bd_paddr(sc, next_slot))
#define	cpsw_cpdma_read_bd_flags(sc, slot)		\
	bus_read_2(sc->res[0], slot->bd_offset + 14)
#define	cpsw_write_hdp_slot(sc, queue, slot)				\
	cpsw_write_4(sc, (queue)->hdp_offset, cpsw_cpdma_bd_paddr(sc, slot))
#define	CP_OFFSET (CPSW_CPDMA_TX_CP(0) - CPSW_CPDMA_TX_HDP(0))
#define	cpsw_read_cp(sc, queue)				\
	cpsw_read_4(sc, (queue)->hdp_offset + CP_OFFSET) 
#define	cpsw_write_cp(sc, queue, val)				\
	cpsw_write_4(sc, (queue)->hdp_offset + CP_OFFSET, (val))
#define	cpsw_write_cp_slot(sc, queue, slot)		\
	cpsw_write_cp(sc, queue, cpsw_cpdma_bd_paddr(sc, slot))

#if 0
/* XXX temporary function versions for debugging. */
static void
cpsw_write_hdp_slotX(struct cpsw_softc *sc, struct cpsw_queue *queue, struct cpsw_slot *slot)
{
	uint32_t reg = queue->hdp_offset;
	uint32_t v = cpsw_cpdma_bd_paddr(sc, slot);
	CPSW_DEBUGF(("HDP <=== 0x%08x (was 0x%08x)", v, cpsw_read_4(sc, reg)));
	cpsw_write_4(sc, reg, v);
}

static void
cpsw_write_cp_slotX(struct cpsw_softc *sc, struct cpsw_queue *queue, struct cpsw_slot *slot)
{
	uint32_t v = cpsw_cpdma_bd_paddr(sc, slot);
	CPSW_DEBUGF(("CP <=== 0x%08x (expecting 0x%08x)", v, cpsw_read_cp(sc, queue)));
	cpsw_write_cp(sc, queue, v);
}
#endif

/*
 * Expanded dump routines for verbose debugging.
 */
static void
cpsw_dump_slot(struct cpsw_softc *sc, struct cpsw_slot *slot)
{
	static const char *flags[] = {"SOP", "EOP", "Owner", "EOQ",
	    "TDownCmplt", "PassCRC", "Long", "Short", "MacCtl", "Overrun",
	    "PktErr1", "PortEn/PktErr0", "RxVlanEncap", "Port2", "Port1",
	    "Port0"};
	struct cpsw_cpdma_bd bd;
	const char *sep;
	int i;

	cpsw_cpdma_read_bd(sc, slot, &bd);
	printf("BD Addr: 0x%08x   Next: 0x%08x\n", cpsw_cpdma_bd_paddr(sc, slot), bd.next);
	printf("  BufPtr: 0x%08x   BufLen: 0x%08x\n", bd.bufptr, bd.buflen);
	printf("  BufOff: 0x%08x   PktLen: 0x%08x\n", bd.bufoff, bd.pktlen);
	printf("  Flags: ");
	sep = "";
	for (i = 0; i < 16; ++i) {
		if (bd.flags & (1 << (15 - i))) {
			printf("%s%s", sep, flags[i]);
			sep = ",";
		}
	}
	printf("\n");
	if (slot->mbuf) {
		printf("  Ether:  %14D\n",
		    (char *)(slot->mbuf->m_hdr.mh_data), " ");
		printf("  Packet: %16D\n",
		    (char *)(slot->mbuf->m_hdr.mh_data) + 14, " ");
	}
}

#define CPSW_DUMP_SLOT(cs, slot) do {				\
	IF_DEBUG(sc) {						\
		cpsw_dump_slot(sc, slot);			\
	}							\
} while (0)


static void
cpsw_dump_queue(struct cpsw_softc *sc, struct cpsw_slots *q)
{
	struct cpsw_slot *slot;
	int i = 0;
	int others = 0;

	STAILQ_FOREACH(slot, q, next) {
		if (i > 4)
			++others;
		else
			cpsw_dump_slot(sc, slot);
		++i;
	}
	if (others)
		printf(" ... and %d more.\n", others);
	printf("\n");
}

#define CPSW_DUMP_QUEUE(sc, q) do {				\
	IF_DEBUG(sc) {						\
		cpsw_dump_queue(sc, q);				\
	}							\
} while (0)


/*
 *
 * Device Probe, Attach, Detach.
 *
 */

static int
cpsw_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "ti,cpsw"))
		return (ENXIO);

	device_set_desc(dev, "3-port Switch Ethernet Subsystem");
	return (BUS_PROBE_DEFAULT);
}


static void
cpsw_init_slots(struct cpsw_softc *sc)
{
	struct cpsw_slot *slot;
	int i;

	STAILQ_INIT(&sc->avail);

	/* Put the slot descriptors onto the global avail list. */
	for (i = 0; i < sizeof(sc->_slots) / sizeof(sc->_slots[0]); i++) {
		slot = &sc->_slots[i];
		slot->bd_offset = cpsw_cpdma_bd_offset(i);
		STAILQ_INSERT_TAIL(&sc->avail, slot, next);
	}
}

/*
 * bind an interrupt, add the relevant info to sc->interrupts
 */
static int
cpsw_attach_interrupt(struct cpsw_softc *sc, struct resource *res, driver_intr_t *handler, const char *description)
{
	void **pcookie;
	int error;

	sc->interrupts[sc->interrupt_count].res = res;
	sc->interrupts[sc->interrupt_count].description = description;
	pcookie = &sc->interrupts[sc->interrupt_count].ih_cookie;

	error = bus_setup_intr(sc->dev, res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, *handler, sc, pcookie);
	if (error)
		device_printf(sc->dev,
		    "could not setup %s\n", description);
	else
		++sc->interrupt_count;
	return (error);
}

/*
 * teardown everything in sc->interrupts.
 */
static void
cpsw_detach_interrupts(struct cpsw_softc *sc)
{
	int error;
	int i;

	for (i = 0; i < sizeof(sc->interrupts) / sizeof(sc->interrupts[0]); ++i) {
		if (!sc->interrupts[i].ih_cookie)
			continue;
		error = bus_teardown_intr(sc->dev,
		    sc->interrupts[i].res, sc->interrupts[i].ih_cookie);
		if (error)
			device_printf(sc->dev, "could not release %s\n",
			    sc->interrupts[i].description);
		sc->interrupts[i].ih_cookie = NULL;
	}
}

static int
cpsw_add_slots(struct cpsw_softc *sc, struct cpsw_queue *queue, int requested)
{
	const int max_slots = sizeof(sc->_slots) / sizeof(sc->_slots[0]);
	struct cpsw_slot *slot;
	int i;

	if (requested < 0)
		requested = max_slots;

	for (i = 0; i < requested; ++i) {
		slot = STAILQ_FIRST(&sc->avail);
		if (slot == NULL)
			return (0);
		if (bus_dmamap_create(sc->mbuf_dtag, 0, &slot->dmamap)) {
			if_printf(sc->ifp, "failed to create dmamap\n");
			return (ENOMEM);
		}
		STAILQ_REMOVE_HEAD(&sc->avail, next);
		STAILQ_INSERT_TAIL(&queue->avail, slot, next);
		++queue->avail_queue_len;
		++queue->queue_slots;
	}
	return (0);
}

static int
cpsw_attach(device_t dev)
{
	bus_dma_segment_t segs[1];
	struct cpsw_softc *sc = device_get_softc(dev);
	struct mii_softc *miisc;
	struct ifnet *ifp;
	void *phy_sc;
	int error, phy, nsegs;
	uint32_t reg;

	CPSW_DEBUGF((""));

	getbinuptime(&sc->attach_uptime);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	/* Get phy address from fdt */
	if (fdt_get_phyaddr(sc->node, sc->dev, &phy, &phy_sc) != 0) {
		device_printf(dev, "failed to get PHY address from FDT\n");
		return (ENXIO);
	}
	/* Initialize mutexes */
	mtx_init(&sc->tx.lock, device_get_nameunit(dev),
	    "cpsw TX lock", MTX_DEF);
	mtx_init(&sc->rx.lock, device_get_nameunit(dev),
	    "cpsw RX lock", MTX_DEF);

	/* Allocate IO and IRQ resources */
	error = bus_alloc_resources(dev, res_spec, sc->res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		cpsw_detach(dev);
		return (ENXIO);
	}

	reg = cpsw_read_4(sc, CPSW_SS_IDVER);
	device_printf(dev, "CPSW SS Version %d.%d (%d)\n", (reg >> 8 & 0x7),
		reg & 0xFF, (reg >> 11) & 0x1F);

	cpsw_add_sysctls(sc);

	/* Allocate a busdma tag and DMA safe memory for mbufs. */
	error = bus_dma_tag_create(
		bus_get_dma_tag(sc->dev),	/* parent */
		1, 0,				/* alignment, boundary */
		BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filtfunc, filtfuncarg */
		MCLBYTES, CPSW_TXFRAGS,		/* maxsize, nsegments */
		MCLBYTES, 0,			/* maxsegsz, flags */
		NULL, NULL,			/* lockfunc, lockfuncarg */
		&sc->mbuf_dtag);		/* dmatag */
	if (error) {
		device_printf(dev, "bus_dma_tag_create failed\n");
		cpsw_detach(dev);
		return (error);
	}

	/* Allocate network interface */
	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "if_alloc() failed\n");
		cpsw_detach(dev);
		return (ENOMEM);
	}

	/* Allocate the null mbuf and pre-sync it. */
	sc->null_mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	memset(sc->null_mbuf->m_hdr.mh_data, 0, sc->null_mbuf->m_ext.ext_size);
	bus_dmamap_create(sc->mbuf_dtag, 0, &sc->null_mbuf_dmamap);
	bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, sc->null_mbuf_dmamap,
	    sc->null_mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
	bus_dmamap_sync(sc->mbuf_dtag, sc->null_mbuf_dmamap,
	    BUS_DMASYNC_PREWRITE);
	sc->null_mbuf_paddr = segs[0].ds_addr;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_MULTICAST | IFF_BROADCAST;
	ifp->if_capabilities = IFCAP_VLAN_MTU | IFCAP_HWCSUM; //FIXME VLAN?
	ifp->if_capenable = ifp->if_capabilities;

	ifp->if_init = cpsw_init;
	ifp->if_start = cpsw_start;
	ifp->if_ioctl = cpsw_ioctl;

	cpsw_init_slots(sc);

	/* Allocate slots to TX and RX queues. */
	STAILQ_INIT(&sc->rx.avail);
	STAILQ_INIT(&sc->rx.active);
	STAILQ_INIT(&sc->tx.avail);
	STAILQ_INIT(&sc->tx.active);
	// For now:  128 slots to TX, rest to RX.
	// XXX TODO: start with 32/64 and grow dynamically based on demand.
	if (cpsw_add_slots(sc, &sc->tx, 128) || cpsw_add_slots(sc, &sc->rx, -1)) {
		device_printf(dev, "failed to allocate dmamaps\n");
		cpsw_detach(dev);
		return (ENOMEM);
	}
	device_printf(dev, "Initial queue size TX=%d RX=%d\n",
	    sc->tx.queue_slots, sc->rx.queue_slots);

	ifp->if_snd.ifq_drv_maxlen = sc->tx.queue_slots;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);

	sc->tx.hdp_offset = CPSW_CPDMA_TX_HDP(0);
	sc->rx.hdp_offset = CPSW_CPDMA_RX_HDP(0);

	/* Get high part of MAC address from control module (mac_id0_hi) */
	/* TODO: Get MAC ID1 as well as MAC ID0. */
	ti_scm_reg_read_4(0x634, &reg);
	sc->mac_addr[0] = reg & 0xFF;
	sc->mac_addr[1] = (reg >>  8) & 0xFF;
	sc->mac_addr[2] = (reg >> 16) & 0xFF;
	sc->mac_addr[3] = (reg >> 24) & 0xFF;

	/* Get low part of MAC address from control module (mac_id0_lo) */
	ti_scm_reg_read_4(0x630, &reg);
	sc->mac_addr[4] = reg & 0xFF;
	sc->mac_addr[5] = (reg >>  8) & 0xFF;

	ether_ifattach(ifp, sc->mac_addr);
	callout_init(&sc->watchdog.callout, 0);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(sc, MDIOCONTROL, 1 << 30 | 1 << 18 | 0xFF);

	/* Clear ALE */
	cpsw_write_4(sc, CPSW_ALE_CONTROL, 1 << 30);

	/* Attach PHY(s) */
	error = mii_attach(dev, &sc->miibus, ifp, cpsw_ifmedia_upd,
	    cpsw_ifmedia_sts, BMSR_DEFCAPMASK, phy, MII_OFFSET_ANY, 0);
	if (error) {
		device_printf(dev, "attaching PHYs failed\n");
		cpsw_detach(dev);
		return (error);
	}
	sc->mii = device_get_softc(sc->miibus);

	/* Tell the MAC where to find the PHY so autoneg works */
	miisc = LIST_FIRST(&sc->mii->mii_phys);

	/* Select PHY and enable interrupts */
	cpsw_write_4(sc, MDIOUSERPHYSEL0, 1 << 6 | (miisc->mii_phy & 0x1F));
	
	/* Note: We don't use sc->res[3] (TX interrupt) */
	if (cpsw_attach_interrupt(sc, sc->res[1],
		cpsw_intr_rx_thresh, "CPSW RX threshold interrupt") ||
	    cpsw_attach_interrupt(sc, sc->res[2],
		cpsw_intr_rx, "CPSW RX interrupt") ||
	    cpsw_attach_interrupt(sc, sc->res[4],
		cpsw_intr_misc, "CPSW misc interrupt")) {
		cpsw_detach(dev);
		return (ENXIO);
	}

	return (0);
}

static void
cpsw_free_slot(struct cpsw_softc *sc, struct cpsw_slot *slot)
{
	int error;

	if (slot->dmamap) {
		error = bus_dmamap_destroy(sc->mbuf_dtag, slot->dmamap);
		KASSERT(error == 0, ("Mapping still active"));
		slot->dmamap = NULL;
	}
	if (slot->mbuf) {
		m_freem(slot->mbuf);
		slot->mbuf = NULL;
	}
}

static int
cpsw_detach(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);
	int error, i;

	CPSW_DEBUGF((""));

	/* Stop controller and free TX queue */
	if (device_is_attached(dev)) {
		ether_ifdetach(sc->ifp);
		CPSW_GLOBAL_LOCK(sc);
		cpsw_shutdown_locked(sc);
		CPSW_GLOBAL_UNLOCK(sc);
		callout_drain(&sc->watchdog.callout);
	}

	bus_generic_detach(dev);
	device_delete_child(dev, sc->miibus);

	/* Stop and release all interrupts */
	cpsw_detach_interrupts(sc);

	/* Free dmamaps and mbufs */
	for (i = 0; i < sizeof(sc->_slots) / sizeof(sc->_slots[0]); ++i) {
		cpsw_free_slot(sc, &sc->_slots[i]);
	}

	/* Free DMA tag */
	error = bus_dma_tag_destroy(sc->mbuf_dtag);
	KASSERT(error == 0, ("Unable to destroy DMA tag"));

	/* Free IO memory handler */
	bus_release_resources(dev, res_spec, sc->res);

	/* Destroy mutexes */
	mtx_destroy(&sc->rx.lock);
	mtx_destroy(&sc->tx.lock);

	return (0);
}

/*
 *
 * Init/Shutdown.
 *
 */

static void
cpsw_reset(struct cpsw_softc *sc)
{
	int i;

	/* Reset RMII/RGMII wrapper. */
	cpsw_write_4(sc, CPSW_WR_SOFT_RESET, 1);
	while (cpsw_read_4(sc, CPSW_WR_SOFT_RESET) & 1)
		;

	/* Disable TX and RX interrupts for all cores. */
	for (i = 0; i < 3; ++i) {
		cpsw_write_4(sc, CPSW_WR_C_RX_THRESH_EN(i), 0x00);
		cpsw_write_4(sc, CPSW_WR_C_TX_EN(i), 0x00);
		cpsw_write_4(sc, CPSW_WR_C_RX_EN(i), 0x00);
		cpsw_write_4(sc, CPSW_WR_C_MISC_EN(i), 0x00);
	}

	/* Reset CPSW subsystem. */
	cpsw_write_4(sc, CPSW_SS_SOFT_RESET, 1);
	while (cpsw_read_4(sc, CPSW_SS_SOFT_RESET) & 1)
		;

	/* Reset Sliver port 1 and 2 */
	for (i = 0; i < 2; i++) {
		/* Reset */
		cpsw_write_4(sc, CPSW_SL_SOFT_RESET(i), 1);
		while (cpsw_read_4(sc, CPSW_SL_SOFT_RESET(i)) & 1)
			;
	}

	/* Reset DMA controller. */
	cpsw_write_4(sc, CPSW_CPDMA_SOFT_RESET, 1);
	while (cpsw_read_4(sc, CPSW_CPDMA_SOFT_RESET) & 1)
		;

	/* Disable TX & RX DMA */
	cpsw_write_4(sc, CPSW_CPDMA_TX_CONTROL, 0);
	cpsw_write_4(sc, CPSW_CPDMA_RX_CONTROL, 0);

	/* Clear all queues. */
	for (i = 0; i < 8; i++) {
		cpsw_write_4(sc, CPSW_CPDMA_TX_HDP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_RX_HDP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_TX_CP(i), 0);
		cpsw_write_4(sc, CPSW_CPDMA_RX_CP(i), 0);
	}

	/* Clear all interrupt Masks */
	cpsw_write_4(sc, CPSW_CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	cpsw_write_4(sc, CPSW_CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);
}

static void
cpsw_init(void *arg)
{
	struct cpsw_softc *sc = arg;

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK(sc);
	cpsw_init_locked(arg);
	CPSW_GLOBAL_UNLOCK(sc);
}

static void
cpsw_init_locked(void *arg)
{
	struct ifnet *ifp;
	struct cpsw_softc *sc = arg;
	struct cpsw_slot *slot;
	uint32_t i;

	CPSW_DEBUGF((""));
	ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	getbinuptime(&sc->init_uptime);

	/* Reset the controller. */
	cpsw_reset(sc);

	/* Enable ALE */
	cpsw_write_4(sc, CPSW_ALE_CONTROL, 1 << 31 | 1 << 4);

	/* Init Sliver port 1 and 2 */
	for (i = 0; i < 2; i++) {
		/* Set Slave Mapping */
		cpsw_write_4(sc, CPSW_SL_RX_PRI_MAP(i), 0x76543210);
		cpsw_write_4(sc, CPSW_PORT_P_TX_PRI_MAP(i + 1), 0x33221100);
		cpsw_write_4(sc, CPSW_SL_RX_MAXLEN(i), 0x5f2);
		/* Set MACCONTROL for ports 0,1: IFCTL_B(16), IFCTL_A(15),
		   GMII_EN(5), FULLDUPLEX(1) */
		/* TODO: Docs claim that IFCTL_B and IFCTL_A do the same thing? */
		/* Huh?  Docs call bit 0 "Loopback" some places, "FullDuplex" others. */
		cpsw_write_4(sc, CPSW_SL_MACCONTROL(i), 1 << 15 | 1 << 5 | 1);
	}

	/* Set Host Port Mapping */
	cpsw_write_4(sc, CPSW_PORT_P0_CPDMA_TX_PRI_MAP, 0x76543210);
	cpsw_write_4(sc, CPSW_PORT_P0_CPDMA_RX_CH_MAP, 0);

	/* Initialize ALE: all ports set to forwarding(3), initialize addrs */
	for (i = 0; i < 3; i++)
		cpsw_write_4(sc, CPSW_ALE_PORTCTL(i), 3);
	cpsw_ale_update_addresses(sc, 1);

	cpsw_write_4(sc, CPSW_SS_PTYPE, 0);

	/* Enable statistics for ports 0, 1 and 2 */
	cpsw_write_4(sc, CPSW_SS_STAT_PORT_EN, 7);

	/* Experiment:  Turn off flow control */
	/* This seems to fix the watchdog resets that have plagued
	   earlier versions of this driver; I'm not yet sure if there
	   are negative effects yet. */
	cpsw_write_4(sc, CPSW_SS_FLOW_CONTROL, 0);

	/* Make IP hdr aligned with 4 */
	cpsw_write_4(sc, CPSW_CPDMA_RX_BUFFER_OFFSET, 2);

	/* Initialize RX Buffer Descriptors */
	cpsw_write_4(sc, CPSW_CPDMA_RX_FREEBUFFER(0), 0);

	/* Enable TX & RX DMA */
	cpsw_write_4(sc, CPSW_CPDMA_TX_CONTROL, 1);
	cpsw_write_4(sc, CPSW_CPDMA_RX_CONTROL, 1);

	/* Enable Interrupts for core 0 */
	cpsw_write_4(sc, CPSW_WR_C_RX_THRESH_EN(0), 0xFF);
	cpsw_write_4(sc, CPSW_WR_C_RX_EN(0), 0xFF);
	cpsw_write_4(sc, CPSW_WR_C_MISC_EN(0), 0x3F);

	/* Enable host Error Interrupt */
	cpsw_write_4(sc, CPSW_CPDMA_DMA_INTMASK_SET, 3);

	/* Enable interrupts for RX Channel 0 */
	cpsw_write_4(sc, CPSW_CPDMA_RX_INTMASK_SET, 1);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(sc, MDIOCONTROL, 1 << 30 | 1 << 18 | 0xFF);

	/* Select MII in GMII_SEL, Internal Delay mode */
	//ti_scm_reg_write_4(0x650, 0);

	/* Initialize active queues. */
	slot = STAILQ_FIRST(&sc->tx.active);
	if (slot != NULL)
		cpsw_write_hdp_slot(sc, &sc->tx, slot);
	slot = STAILQ_FIRST(&sc->rx.active);
	if (slot != NULL)
		cpsw_write_hdp_slot(sc, &sc->rx, slot);
	cpsw_rx_enqueue(sc);

	/* Activate network interface */
	sc->rx.running = 1;
	sc->tx.running = 1;
	sc->watchdog.timer = 0;
	callout_reset(&sc->watchdog.callout, hz, cpsw_tick, sc);
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

}

static int
cpsw_shutdown(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK(sc);
	cpsw_shutdown_locked(sc);
	CPSW_GLOBAL_UNLOCK(sc);
	return (0);
}

static void
cpsw_rx_teardown_locked(struct cpsw_softc *sc)
{
	struct mbuf *received, *next;
	int i = 0;

	CPSW_DEBUGF(("starting RX teardown"));
	cpsw_write_4(sc, CPSW_CPDMA_RX_TEARDOWN, 0);
	for (;;) {
		received = cpsw_rx_dequeue(sc);
		CPSW_GLOBAL_UNLOCK(sc);
		while (received != NULL) {
			next = received->m_nextpkt;
			received->m_nextpkt = NULL;
			(*sc->ifp->if_input)(sc->ifp, received);
			received = next;
		}
		CPSW_GLOBAL_LOCK(sc);
		if (!sc->rx.running) {
			CPSW_DEBUGF(("finished RX teardown (%d retries)", i));
			return;
		}
		if (++i > 10) {
			if_printf(sc->ifp, "Unable to cleanly shutdown receiver\n");
			return;
		}
		DELAY(10);
	}
}

static void
cpsw_tx_teardown_locked(struct cpsw_softc *sc)
{
	int i = 0;

	CPSW_DEBUGF(("starting TX teardown"));
	cpsw_write_4(sc, CPSW_CPDMA_TX_TEARDOWN, 0);
	cpsw_tx_dequeue(sc);
	while (sc->tx.running && ++i < 10) {
		DELAY(10);
		cpsw_tx_dequeue(sc);
	}
	if (sc->tx.running)
		if_printf(sc->ifp, "Unable to cleanly shutdown transmitter\n");
	CPSW_DEBUGF(("finished TX teardown (%d retries, %d idle buffers)",
	    i, sc->tx.active_queue_len));
}

static void
cpsw_shutdown_locked(struct cpsw_softc *sc)
{
	struct ifnet *ifp;

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK_ASSERT(sc);
	ifp = sc->ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	/* Disable interface */
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	/* Stop ticker */
	callout_stop(&sc->watchdog.callout);

	/* Tear down the RX/TX queues. */
	cpsw_rx_teardown_locked(sc);
	cpsw_tx_teardown_locked(sc);

	/* Capture stats before we reset controller. */
	cpsw_stats_collect(sc);

	cpsw_reset(sc);
}

/*
 *  Suspend/Resume.
 */

static int
cpsw_suspend(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK(sc);
	cpsw_shutdown_locked(sc);
	CPSW_GLOBAL_UNLOCK(sc);
	return (0);
}

static int
cpsw_resume(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_DEBUGF(("UNIMPLEMENTED"));
	return (0);
}

/*
 *
 *  IOCTL
 *
 */

static void
cpsw_set_promisc(struct cpsw_softc *sc, int set)
{
	/*
	 * Enabling promiscuous mode requires two bits of work: First,
	 * ALE_BYPASS needs to be enabled.  That disables the ALE
	 * forwarding logic and causes every packet to be sent to the
	 * host port.  That makes us promiscuous wrt received packets.
	 *
	 * With ALE forwarding disabled, the transmitter needs to set
	 * an explicit output port on every packet to route it to the
	 * correct egress.  This should be doable for systems such as
	 * BeagleBone where only one egress port is actually wired to
	 * a PHY.  If you have both egress ports wired up, life gets a
	 * lot more interesting.
	 *
	 * Hmmm.... NetBSD driver uses ALE_BYPASS always and doesn't
	 * seem to set explicit egress ports.  Does that mean they
	 * are always promiscuous?
	 */
	if (set) {
		printf("Promiscuous mode unimplemented\n");
	}
}

static void
cpsw_set_allmulti(struct cpsw_softc *sc, int set)
{
	if (set) {
		printf("All-multicast mode unimplemented\n");
	}
}

static int
cpsw_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error;
	uint32_t changed;

	error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		CPSW_GLOBAL_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				changed = ifp->if_flags ^ sc->cpsw_if_flags;
				CPSW_DEBUGF(("SIOCSIFFLAGS: UP & RUNNING (changed=0x%x)", changed));
				if (changed & IFF_PROMISC)
					cpsw_set_promisc(sc,
					    ifp->if_flags & IFF_PROMISC);
				if (changed & IFF_ALLMULTI)
					cpsw_set_allmulti(sc,
					    ifp->if_flags & IFF_ALLMULTI);
			} else {
				CPSW_DEBUGF(("SIOCSIFFLAGS: UP but not RUNNING; starting up"));
				cpsw_init_locked(sc);
			}
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			CPSW_DEBUGF(("SIOCSIFFLAGS: not UP but RUNNING; shutting down"));
			cpsw_shutdown_locked(sc);
		}

		sc->cpsw_if_flags = ifp->if_flags;
		CPSW_GLOBAL_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
		cpsw_ale_update_addresses(sc, 0);
		break;
	case SIOCDELMULTI:
		/* Ugh.  DELMULTI doesn't provide the specific address
		   being removed, so the best we can do is remove
		   everything and rebuild it all. */
		cpsw_ale_update_addresses(sc, 1);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
	}
	return (error);
}

/*
 *
 * MIIBUS
 *
 */
static int
cpsw_miibus_ready(struct cpsw_softc *sc)
{
	uint32_t r, retries = CPSW_MIIBUS_RETRIES;

	while (--retries) {
		r = cpsw_read_4(sc, MDIOUSERACCESS0);
		if ((r & 1 << 31) == 0)
			return 1;
		DELAY(CPSW_MIIBUS_DELAY);
	}
	return 0;
}

static int
cpsw_miibus_readreg(device_t dev, int phy, int reg)
{
	struct cpsw_softc *sc = device_get_softc(dev);
	uint32_t cmd, r;

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO not ready to read\n");
		return 0;
	}

	/* Set GO, reg, phy */
	cmd = 1 << 31 | (reg & 0x1F) << 21 | (phy & 0x1F) << 16;
	cpsw_write_4(sc, MDIOUSERACCESS0, cmd);

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO timed out during read\n");
		return 0;
	}

	r = cpsw_read_4(sc, MDIOUSERACCESS0);
	if((r & 1 << 29) == 0) {
		device_printf(dev, "Failed to read from PHY.\n");
		r = 0;
	}
	return (r & 0xFFFF);
}

static int
cpsw_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct cpsw_softc *sc = device_get_softc(dev);
	uint32_t cmd;

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO not ready to write\n");
		return 0;
	}

	/* Set GO, WRITE, reg, phy, and value */
	cmd = 3 << 30 | (reg & 0x1F) << 21 | (phy & 0x1F) << 16
	    | (value & 0xFFFF);
	cpsw_write_4(sc, MDIOUSERACCESS0, cmd);

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO timed out during write\n");
		return 0;
	}

	if((cpsw_read_4(sc, MDIOUSERACCESS0) & (1 << 29)) == 0)
		device_printf(dev, "Failed to write to PHY.\n");

	return 0;
}

/*
 *
 * Transmit/Receive Packets.
 *
 */


static void
cpsw_intr_rx(void *arg)
{
	struct cpsw_softc *sc = arg;
	struct mbuf *received, *next;

	CPSW_RX_LOCK(sc);
	received = cpsw_rx_dequeue(sc);
	cpsw_rx_enqueue(sc);
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, 1);
	CPSW_RX_UNLOCK(sc);
	
	while (received != NULL) {
		next = received->m_nextpkt;
		received->m_nextpkt = NULL;
		(*sc->ifp->if_input)(sc->ifp, received);
		received = next;
	}
}

static struct mbuf *
cpsw_rx_dequeue(struct cpsw_softc *sc)
{
	struct cpsw_cpdma_bd bd;
	struct cpsw_slot *slot;
	struct ifnet *ifp;
	struct mbuf *mb_head, *mb_tail;
	int removed = 0;

	ifp = sc->ifp;
	mb_head = mb_tail = NULL;

	/* Pull completed packets off hardware RX queue. */
	while ((slot = STAILQ_FIRST(&sc->rx.active)) != NULL) {
		cpsw_cpdma_read_bd(sc, slot, &bd);
		if (bd.flags & CPDMA_BD_OWNER)
			break; /* Still in use by hardware */

		CPSW_DEBUGF(("Removing received packet from RX queue"));
		++removed;
		STAILQ_REMOVE_HEAD(&sc->rx.active, next);
		STAILQ_INSERT_TAIL(&sc->rx.avail, slot, next);

		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);

		if (bd.flags & CPDMA_BD_TDOWNCMPLT) {
			CPSW_DEBUGF(("RX teardown in progress"));
			m_freem(slot->mbuf);
			slot->mbuf = NULL;
			cpsw_write_cp(sc, &sc->rx, 0xfffffffc);
			sc->rx.running = 0;
			break;
		}

		cpsw_write_cp_slot(sc, &sc->rx, slot);

		/* Set up mbuf */
		/* TODO: track SOP/EOP bits to assemble a full mbuf
		   out of received fragments. */
		slot->mbuf->m_hdr.mh_data += bd.bufoff;
		slot->mbuf->m_hdr.mh_len = bd.pktlen - 4;
		slot->mbuf->m_pkthdr.len = bd.pktlen - 4;
		slot->mbuf->m_flags |= M_PKTHDR;
		slot->mbuf->m_pkthdr.rcvif = ifp;
		slot->mbuf->m_nextpkt = NULL;

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/* check for valid CRC by looking into pkt_err[5:4] */
			if ((bd.flags & CPDMA_BD_PKT_ERR_MASK) == 0) {
				slot->mbuf->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				slot->mbuf->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				slot->mbuf->m_pkthdr.csum_data = 0xffff;
			}
		}

		/* Add mbuf to packet list to be returned. */
		if (mb_tail) {
			mb_tail->m_nextpkt = slot->mbuf;
		} else {
			mb_head = slot->mbuf;
		}
		mb_tail = slot->mbuf;
		slot->mbuf = NULL;
	}

	if (removed != 0) {
		sc->rx.queue_removes += removed;
		sc->rx.active_queue_len -= removed;
		sc->rx.avail_queue_len += removed;
		if (sc->rx.avail_queue_len > sc->rx.max_avail_queue_len)
			sc->rx.max_avail_queue_len = sc->rx.avail_queue_len;
	}
	return (mb_head);
}

static void
cpsw_rx_enqueue(struct cpsw_softc *sc)
{
	bus_dma_segment_t seg[1];
	struct cpsw_cpdma_bd bd;
	struct ifnet *ifp = sc->ifp;
	struct cpsw_slots tmpqueue = STAILQ_HEAD_INITIALIZER(tmpqueue);
	struct cpsw_slot *slot, *prev_slot = NULL;
	struct cpsw_slot *last_old_slot, *first_new_slot;
	int error, nsegs, added = 0;

	/* Register new mbufs with hardware. */
	while ((slot = STAILQ_FIRST(&sc->rx.avail)) != NULL) {
		if (slot->mbuf == NULL) {
			slot->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
			if (slot->mbuf == NULL) {
				if_printf(sc->ifp, "Unable to fill RX queue\n");
				break;
			}
			slot->mbuf->m_len =
			    slot->mbuf->m_pkthdr.len =
			    slot->mbuf->m_ext.ext_size;
		}

		error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, slot->dmamap,
		    slot->mbuf, seg, &nsegs, BUS_DMA_NOWAIT);

		KASSERT(nsegs == 1, ("More than one segment (nsegs=%d)", nsegs));
		KASSERT(error == 0, ("DMA error (error=%d)", error));
		if (error != 0 || nsegs != 1) {
			if_printf(ifp,
			    "%s: Can't prep RX buf for DMA (nsegs=%d, error=%d)\n",
			    __func__, nsegs, error);
			bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
			m_freem(slot->mbuf);
			slot->mbuf = NULL;
			break;
		}

		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap, BUS_DMASYNC_PREREAD);

		/* Create and submit new rx descriptor*/
		bd.next = 0;
		bd.bufptr = seg->ds_addr;
		bd.bufoff = 0;
		bd.buflen = MCLBYTES - 1;
		bd.pktlen = bd.buflen;
		bd.flags = CPDMA_BD_OWNER;
		cpsw_cpdma_write_bd(sc, slot, &bd);
		++added;

		if (prev_slot != NULL)
			cpsw_cpdma_write_bd_next(sc, prev_slot, slot);
		prev_slot = slot;
		STAILQ_REMOVE_HEAD(&sc->rx.avail, next);
		sc->rx.avail_queue_len--;
		STAILQ_INSERT_TAIL(&tmpqueue, slot, next);
	}

	if (added == 0)
		return;

	CPSW_DEBUGF(("Adding %d buffers to RX queue", added));

	/* Link new entries to hardware RX queue. */
	last_old_slot = STAILQ_LAST(&sc->rx.active, cpsw_slot, next);
	first_new_slot = STAILQ_FIRST(&tmpqueue);
	STAILQ_CONCAT(&sc->rx.active, &tmpqueue);
	if (first_new_slot == NULL) {
		return;
	} else if (last_old_slot == NULL) {
		/* Start a fresh queue. */
		cpsw_write_hdp_slot(sc, &sc->rx, first_new_slot);
	} else {
		/* Add buffers to end of current queue. */
		cpsw_cpdma_write_bd_next(sc, last_old_slot, first_new_slot);
		/* If underrun, restart queue. */
		if (cpsw_cpdma_read_bd_flags(sc, last_old_slot) & CPDMA_BD_EOQ) {
			cpsw_write_hdp_slot(sc, &sc->rx, first_new_slot);
		}
	}
	sc->rx.queue_adds += added;
	sc->rx.active_queue_len += added;
	if (sc->rx.active_queue_len > sc->rx.max_active_queue_len) {
		sc->rx.max_active_queue_len = sc->rx.active_queue_len;
	}
}

static void
cpsw_start(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;

	CPSW_TX_LOCK(sc);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) && sc->tx.running) {
		cpsw_tx_enqueue(sc);
		cpsw_tx_dequeue(sc);
	}
	CPSW_TX_UNLOCK(sc);
}

static void
cpsw_tx_enqueue(struct cpsw_softc *sc)
{
	bus_dma_segment_t segs[CPSW_TXFRAGS];
	struct cpsw_cpdma_bd bd;
	struct cpsw_slots tmpqueue = STAILQ_HEAD_INITIALIZER(tmpqueue);
	struct cpsw_slot *slot, *prev_slot = NULL;
	struct cpsw_slot *last_old_slot, *first_new_slot;
	struct mbuf *m0;
	int error, nsegs, seg, added = 0, padlen;

	/* Pull pending packets from IF queue and prep them for DMA. */
	while ((slot = STAILQ_FIRST(&sc->tx.avail)) != NULL) {
		IF_DEQUEUE(&sc->ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		slot->mbuf = m0;
		padlen = ETHER_MIN_LEN - slot->mbuf->m_pkthdr.len;
		if (padlen < 0)
			padlen = 0;

		/* Create mapping in DMA memory */
		error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, slot->dmamap,
		    slot->mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
		/* If the packet is too fragmented, try to simplify. */
		if (error == EFBIG ||
		    (error == 0 &&
			nsegs + (padlen > 0 ? 1 : 0) > sc->tx.avail_queue_len)) {
			bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
			if (padlen > 0) /* May as well add padding. */
				m_append(slot->mbuf, padlen,
				    sc->null_mbuf->m_hdr.mh_data);
			m0 = m_defrag(slot->mbuf, M_NOWAIT);
			if (m0 == NULL) {
				if_printf(sc->ifp,
				    "Can't defragment packet; dropping\n");
				m_freem(slot->mbuf);
			} else {
				CPSW_DEBUGF(("Requeueing defragmented packet"));
				IF_PREPEND(&sc->ifp->if_snd, m0);
			}
			slot->mbuf = NULL;
			continue;
		}
		if (error != 0) {
			if_printf(sc->ifp,
			    "%s: Can't setup DMA (error=%d), dropping packet\n",
			    __func__, error);
			bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
			m_freem(slot->mbuf);
			slot->mbuf = NULL;
			break;
		}

		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap,
				BUS_DMASYNC_PREWRITE);


		CPSW_DEBUGF(("Queueing TX packet: %d segments + %d pad bytes",
			nsegs, padlen));

		/* If there is only one segment, the for() loop
		 * gets skipped and the single buffer gets set up
		 * as both SOP and EOP. */
		/* Start by setting up the first buffer */
		bd.next = 0;
		bd.bufptr = segs[0].ds_addr;
		bd.bufoff = 0;
		bd.buflen = segs[0].ds_len;
		bd.pktlen = m_length(slot->mbuf, NULL) + padlen;
		bd.flags =  CPDMA_BD_SOP | CPDMA_BD_OWNER;
		for (seg = 1; seg < nsegs; ++seg) {
			/* Save the previous buffer (which isn't EOP) */
			cpsw_cpdma_write_bd(sc, slot, &bd);
			if (prev_slot != NULL)
				cpsw_cpdma_write_bd_next(sc, prev_slot, slot);
			prev_slot = slot;
			STAILQ_REMOVE_HEAD(&sc->tx.avail, next);
			sc->tx.avail_queue_len--;
			STAILQ_INSERT_TAIL(&tmpqueue, slot, next);
			++added;
			slot = STAILQ_FIRST(&sc->tx.avail);

			/* Setup next buffer (which isn't SOP) */
			bd.next = 0;
			bd.bufptr = segs[seg].ds_addr;
			bd.bufoff = 0;
			bd.buflen = segs[seg].ds_len;
			bd.pktlen = 0;
			bd.flags = CPDMA_BD_OWNER;
		}
		/* Save the final buffer. */
		if (padlen <= 0)
			bd.flags |= CPDMA_BD_EOP;
		cpsw_cpdma_write_bd(sc, slot, &bd);
		if (prev_slot != NULL)
			cpsw_cpdma_write_bd_next(sc, prev_slot, slot);
		prev_slot = slot;
		STAILQ_REMOVE_HEAD(&sc->tx.avail, next);
		sc->tx.avail_queue_len--;
		STAILQ_INSERT_TAIL(&tmpqueue, slot, next);
		++added;

		if (padlen > 0) {
			slot = STAILQ_FIRST(&sc->tx.avail);
			STAILQ_REMOVE_HEAD(&sc->tx.avail, next);
			sc->tx.avail_queue_len--;
			STAILQ_INSERT_TAIL(&tmpqueue, slot, next);
			++added;

			/* Setup buffer of null pad bytes (definitely EOP) */
			cpsw_cpdma_write_bd_next(sc, prev_slot, slot);
			prev_slot = slot;
			bd.next = 0;
			bd.bufptr = sc->null_mbuf_paddr;
			bd.bufoff = 0;
			bd.buflen = padlen;
			bd.pktlen = 0;
			bd.flags = CPDMA_BD_EOP | CPDMA_BD_OWNER;
			cpsw_cpdma_write_bd(sc, slot, &bd);
			++nsegs;
		}

		if (nsegs > sc->tx.longest_chain)
			sc->tx.longest_chain = nsegs;

		// TODO: Should we defer the BPF tap until
		// after all packets are queued?
		BPF_MTAP(sc->ifp, m0);
	}

	/* Attach the list of new buffers to the hardware TX queue. */
	last_old_slot = STAILQ_LAST(&sc->tx.active, cpsw_slot, next);
	first_new_slot = STAILQ_FIRST(&tmpqueue);
	STAILQ_CONCAT(&sc->tx.active, &tmpqueue);
	if (first_new_slot == NULL) {
		return;
	} else if (last_old_slot == NULL) {
		/* Start a fresh queue. */
		cpsw_write_hdp_slot(sc, &sc->tx, first_new_slot);
	} else {
		/* Add buffers to end of current queue. */
		cpsw_cpdma_write_bd_next(sc, last_old_slot, first_new_slot);
		/* If underrun, restart queue. */
		if (cpsw_cpdma_read_bd_flags(sc, last_old_slot) & CPDMA_BD_EOQ) {
			cpsw_write_hdp_slot(sc, &sc->tx, first_new_slot);
		}
	}
	sc->tx.queue_adds += added;
	sc->tx.active_queue_len += added;
	if (sc->tx.active_queue_len > sc->tx.max_active_queue_len) {
		sc->tx.max_active_queue_len = sc->tx.active_queue_len;
	}
}

static int
cpsw_tx_dequeue(struct cpsw_softc *sc)
{
	struct cpsw_slot *slot, *last_removed_slot = NULL;
	uint32_t flags, removed = 0;

	slot = STAILQ_FIRST(&sc->tx.active);
	if (slot == NULL && cpsw_read_cp(sc, &sc->tx) == 0xfffffffc) {
		CPSW_DEBUGF(("TX teardown of an empty queue"));
		cpsw_write_cp(sc, &sc->tx, 0xfffffffc);
		sc->tx.running = 0;
		return (0);
	}

	/* Pull completed buffers off the hardware TX queue. */
	while (slot != NULL) {
		flags = cpsw_cpdma_read_bd_flags(sc, slot);
		if (flags & CPDMA_BD_OWNER)
			break; /* Hardware is still using this packet. */

		CPSW_DEBUGF(("TX removing completed packet"));
		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
		m_freem(slot->mbuf);
		slot->mbuf = NULL;

		/* Dequeue any additional buffers used by this packet. */
		while (slot != NULL && slot->mbuf == NULL) {
			STAILQ_REMOVE_HEAD(&sc->tx.active, next);
			STAILQ_INSERT_TAIL(&sc->tx.avail, slot, next);
			++removed;
			last_removed_slot = slot;
			slot = STAILQ_FIRST(&sc->tx.active);
		}

		/* TearDown complete is only marked on the SOP for the packet. */
		if (flags & CPDMA_BD_TDOWNCMPLT) {
			CPSW_DEBUGF(("TX teardown in progress"));
			cpsw_write_cp(sc, &sc->tx, 0xfffffffc);
			// TODO: Increment a count of dropped TX packets
			sc->tx.running = 0;
			break;
		}
	}

	if (removed != 0) {
		cpsw_write_cp_slot(sc, &sc->tx, last_removed_slot);
		sc->tx.queue_removes += removed;
		sc->tx.active_queue_len -= removed;
		sc->tx.avail_queue_len += removed;
		if (sc->tx.avail_queue_len > sc->tx.max_avail_queue_len)
			sc->tx.max_avail_queue_len = sc->tx.avail_queue_len;
	}
	return (removed);
}

/*
 *
 * Miscellaneous interrupts.
 *
 */

static void
cpsw_intr_rx_thresh(void *arg)
{
	struct cpsw_softc *sc = arg;
	uint32_t stat = cpsw_read_4(sc, CPSW_WR_C_RX_THRESH_STAT(0));

	CPSW_DEBUGF(("stat=%x", stat));
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, 0);
}

static void
cpsw_intr_misc_host_error(struct cpsw_softc *sc)
{
	uint32_t intstat;
	uint32_t dmastat;
	int txerr, rxerr, txchan, rxchan;

	printf("\n\n");
	device_printf(sc->dev,
	    "HOST ERROR:  PROGRAMMING ERROR DETECTED BY HARDWARE\n");
	printf("\n\n");
	intstat = cpsw_read_4(sc, CPSW_CPDMA_DMA_INTSTAT_MASKED);
	device_printf(sc->dev, "CPSW_CPDMA_DMA_INTSTAT_MASKED=0x%x\n", intstat);
	dmastat = cpsw_read_4(sc, CPSW_CPDMA_DMASTATUS);
	device_printf(sc->dev, "CPSW_CPDMA_DMASTATUS=0x%x\n", dmastat);

	txerr = (dmastat >> 20) & 15;
	txchan = (dmastat >> 16) & 7;
	rxerr = (dmastat >> 12) & 15;
	rxchan = (dmastat >> 8) & 7;

	switch (txerr) {
	case 0: break;
	case 1:	printf("SOP error on TX channel %d\n", txchan);
		break;
	case 2:	printf("Ownership bit not set on SOP buffer on TX channel %d\n", txchan);
		break;
	case 3:	printf("Zero Next Buffer but not EOP on TX channel %d\n", txchan);
		break;
	case 4:	printf("Zero Buffer Pointer on TX channel %d\n", txchan);
		break;
	case 5:	printf("Zero Buffer Length on TX channel %d\n", txchan);
		break;
	case 6:	printf("Packet length error on TX channel %d\n", txchan);
		break;
	default: printf("Unknown error on TX channel %d\n", txchan);
		break;
	}

	if (txerr != 0) {
		printf("CPSW_CPDMA_TX%d_HDP=0x%x\n",
		    txchan, cpsw_read_4(sc, CPSW_CPDMA_TX_HDP(txchan)));
		printf("CPSW_CPDMA_TX%d_CP=0x%x\n",
		    txchan, cpsw_read_4(sc, CPSW_CPDMA_TX_CP(txchan)));
		cpsw_dump_queue(sc, &sc->tx.active);
	}

	switch (rxerr) {
	case 0: break;
	case 2:	printf("Ownership bit not set on RX channel %d\n", rxchan);
		break;
	case 4:	printf("Zero Buffer Pointer on RX channel %d\n", rxchan);
		break;
	case 5:	printf("Zero Buffer Length on RX channel %d\n", rxchan);
		break;
	case 6:	printf("Buffer offset too big on RX channel %d\n", rxchan);
		break;
	default: printf("Unknown RX error on RX channel %d\n", rxchan);
		break;
	}

	if (rxerr != 0) {
		printf("CPSW_CPDMA_RX%d_HDP=0x%x\n",
		    rxchan, cpsw_read_4(sc,CPSW_CPDMA_RX_HDP(rxchan)));
		printf("CPSW_CPDMA_RX%d_CP=0x%x\n",
		    rxchan, cpsw_read_4(sc, CPSW_CPDMA_RX_CP(rxchan)));
		cpsw_dump_queue(sc, &sc->rx.active);
	}

	printf("\nALE Table\n");
	cpsw_ale_dump_table(sc);

	// XXX do something useful here??
	panic("CPSW HOST ERROR INTERRUPT");

	// Suppress this interrupt in the future.
	cpsw_write_4(sc, CPSW_CPDMA_DMA_INTMASK_CLEAR, intstat);
	printf("XXX HOST ERROR INTERRUPT SUPPRESSED\n");
	// The watchdog will probably reset the controller
	// in a little while.  It will probably fail again.
}

static void
cpsw_intr_misc(void *arg)
{
	struct cpsw_softc *sc = arg;
	uint32_t stat = cpsw_read_4(sc, CPSW_WR_C_MISC_STAT(0));

	if (stat & 16)
		CPSW_DEBUGF(("Time sync event interrupt unimplemented"));
	if (stat & 8)
		cpsw_stats_collect(sc);
	if (stat & 4)
		cpsw_intr_misc_host_error(sc);
	if (stat & 2)
		CPSW_DEBUGF(("MDIO link change interrupt unimplemented"));
	if (stat & 1)
		CPSW_DEBUGF(("MDIO operation completed interrupt unimplemented"));
	cpsw_write_4(sc, CPSW_CPDMA_CPDMA_EOI_VECTOR, 3);
}

/*
 *
 * Periodic Checks and Watchdog.
 *
 */

static void
cpsw_tick(void *msc)
{
	struct cpsw_softc *sc = msc;

	/* Check for TX timeout */
	cpsw_tx_watchdog(sc);

	/* Check for media type change */
	mii_tick(sc->mii);
	if(sc->cpsw_media_status != sc->mii->mii_media.ifm_media) {
		printf("%s: media type changed (ifm_media=%x)\n", __func__, 
			sc->mii->mii_media.ifm_media);
		cpsw_ifmedia_upd(sc->ifp);
	}

	/* Schedule another timeout one second from now */
	callout_reset(&sc->watchdog.callout, hz, cpsw_tick, sc);
}

static void
cpsw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	CPSW_DEBUGF((""));
	CPSW_TX_LOCK(sc);

	mii = sc->mii;
	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	CPSW_TX_UNLOCK(sc);
}

static int
cpsw_ifmedia_upd(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;

	CPSW_DEBUGF((""));
	if (ifp->if_flags & IFF_UP) {
		CPSW_GLOBAL_LOCK(sc);
		sc->cpsw_media_status = sc->mii->mii_media.ifm_media;
		mii_mediachg(sc->mii);
		cpsw_init_locked(sc);
		CPSW_GLOBAL_UNLOCK(sc);
	}

	return (0);
}

static void
cpsw_tx_watchdog_full_reset(struct cpsw_softc *sc)
{
	cpsw_debugf_head("CPSW watchdog");
	if_printf(sc->ifp, "watchdog timeout\n");
	cpsw_shutdown_locked(sc);
	cpsw_init_locked(sc);
}

static void
cpsw_tx_watchdog(struct cpsw_softc *sc)
{
	struct ifnet *ifp = sc->ifp;

	CPSW_GLOBAL_LOCK(sc);
	if (sc->tx.active_queue_len == 0 || (ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || !sc->tx.running) {
		sc->watchdog.timer = 0; /* Nothing to do. */
	} else if (sc->tx.queue_removes > sc->tx.queue_removes_at_last_tick) {
		sc->watchdog.timer = 0;  /* Stuff done while we weren't looking. */
	} else if (cpsw_tx_dequeue(sc) > 0) {
		sc->watchdog.timer = 0;  /* We just did something. */
	} else {
		/* There was something to do but it didn't get done. */
		++sc->watchdog.timer;
		if (sc->watchdog.timer > 2) {
			sc->watchdog.timer = 0;
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			++sc->watchdog.resets;
			cpsw_tx_watchdog_full_reset(sc);
		}
	}
	sc->tx.queue_removes_at_last_tick = sc->tx.queue_removes;
	CPSW_GLOBAL_UNLOCK(sc);
}

/*
 *
 * ALE support routines.
 *
 */

static void
cpsw_ale_read_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry)
{
	cpsw_write_4(sc, CPSW_ALE_TBLCTL, idx & 1023);
	ale_entry[0] = cpsw_read_4(sc, CPSW_ALE_TBLW0);
	ale_entry[1] = cpsw_read_4(sc, CPSW_ALE_TBLW1);
	ale_entry[2] = cpsw_read_4(sc, CPSW_ALE_TBLW2);
}

static void
cpsw_ale_write_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry)
{
	cpsw_write_4(sc, CPSW_ALE_TBLW0, ale_entry[0]);
	cpsw_write_4(sc, CPSW_ALE_TBLW1, ale_entry[1]);
	cpsw_write_4(sc, CPSW_ALE_TBLW2, ale_entry[2]);
	cpsw_write_4(sc, CPSW_ALE_TBLCTL, 1 << 31 | (idx & 1023));
}

static int
cpsw_ale_remove_all_mc_entries(struct cpsw_softc *sc)
{
	int i;
	uint32_t ale_entry[3];

	/* First two entries are link address and broadcast. */
	for (i = 2; i < CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		if (((ale_entry[1] >> 28) & 3) == 1 && /* Address entry */
		    ((ale_entry[1] >> 8) & 1) == 1) { /* MCast link addr */
			ale_entry[0] = ale_entry[1] = ale_entry[2] = 0;
			cpsw_ale_write_entry(sc, i, ale_entry);
		}
	}
	return CPSW_MAX_ALE_ENTRIES;
}

static int
cpsw_ale_mc_entry_set(struct cpsw_softc *sc, uint8_t portmap, uint8_t *mac)
{
	int free_index = -1, matching_index = -1, i;
	uint32_t ale_entry[3];

	/* Find a matching entry or a free entry. */
	for (i = 0; i < CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);

		/* Entry Type[61:60] is 0 for free entry */ 
		if (free_index < 0 && ((ale_entry[1] >> 28) & 3) == 0) {
			free_index = i;
		}

		if ((((ale_entry[1] >> 8) & 0xFF) == mac[0]) &&
		    (((ale_entry[1] >> 0) & 0xFF) == mac[1]) &&
		    (((ale_entry[0] >>24) & 0xFF) == mac[2]) &&
		    (((ale_entry[0] >>16) & 0xFF) == mac[3]) &&
		    (((ale_entry[0] >> 8) & 0xFF) == mac[4]) &&
		    (((ale_entry[0] >> 0) & 0xFF) == mac[5])) {
			matching_index = i;
			break;
		}
	}

	if (matching_index < 0) {
		if (free_index < 0)
			return (ENOMEM);
		i = free_index;
	}

	/* Set MAC address */
	ale_entry[0] = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
	ale_entry[1] = mac[0] << 8 | mac[1];

	/* Entry type[61:60] is addr entry(1), Mcast fwd state[63:62] is fw(3)*/
	ale_entry[1] |= 0xd0 << 24;

	/* Set portmask [68:66] */
	ale_entry[2] = (portmap & 7) << 2;

	cpsw_ale_write_entry(sc, i, ale_entry);

	return 0;
}

static void
cpsw_ale_dump_table(struct cpsw_softc *sc) {
	int i;
	uint32_t ale_entry[3];
	for (i = 0; i < CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		if (ale_entry[0] || ale_entry[1] || ale_entry[2]) {
			printf("ALE[%4u] %08x %08x %08x ", i, ale_entry[0],
				ale_entry[1], ale_entry[2]);
			printf("mac: %02x:%02x:%02x:%02x:%02x:%02x ",
				(ale_entry[1] >> 8) & 0xFF,
				(ale_entry[1] >> 0) & 0xFF,
				(ale_entry[0] >>24) & 0xFF,
				(ale_entry[0] >>16) & 0xFF,
				(ale_entry[0] >> 8) & 0xFF,
				(ale_entry[0] >> 0) & 0xFF);
			printf(((ale_entry[1] >> 8) & 1) ? "mcast " : "ucast ");
			printf("type: %u ", (ale_entry[1] >> 28) & 3);
			printf("port: %u ", (ale_entry[2] >> 2) & 7);
			printf("\n");
		}
	}
	printf("\n");
}

static int
cpsw_ale_update_addresses(struct cpsw_softc *sc, int purge)
{
	uint8_t *mac;
	uint32_t ale_entry[3];
	struct ifnet *ifp = sc->ifp;
	struct ifmultiaddr *ifma;
	int i;

	/* Route incoming packets for our MAC address to Port 0 (host). */
	/* For simplicity, keep this entry at table index 0 in the ALE. */
        if_addr_rlock(ifp);
	mac = LLADDR((struct sockaddr_dl *)ifp->if_addr->ifa_addr);
	ale_entry[0] = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
	ale_entry[1] = 0x10 << 24 | mac[0] << 8 | mac[1]; /* addr entry + mac */
	ale_entry[2] = 0; /* port = 0 */
	cpsw_ale_write_entry(sc, 0, ale_entry);

	/* Set outgoing MAC Address for Ports 1 and 2. */
	for (i = 1; i < 3; ++i) {
		cpsw_write_4(sc, CPSW_PORT_P_SA_HI(i),
		    mac[3] << 24 | mac[2] << 16 | mac[1] << 8 | mac[0]);
		cpsw_write_4(sc, CPSW_PORT_P_SA_LO(i),
		    mac[5] << 8 | mac[4]);
	}
        if_addr_runlock(ifp);

	/* Keep the broadcast address at table entry 1. */
	ale_entry[0] = 0xffffffff; /* Lower 32 bits of MAC */
	ale_entry[1] = 0xd000ffff; /* FW (3 << 30), Addr entry (1 << 24), upper 16 bits of Mac */ 
	ale_entry[2] = 0x0000001c; /* Forward to all ports */
	cpsw_ale_write_entry(sc, 1, ale_entry);

	/* SIOCDELMULTI doesn't specify the particular address
	   being removed, so we have to remove all and rebuild. */
	if (purge)
		cpsw_ale_remove_all_mc_entries(sc);

        /* Set other multicast addrs desired. */
        if_maddr_rlock(ifp);
        TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
                if (ifma->ifma_addr->sa_family != AF_LINK)
                        continue;
		cpsw_ale_mc_entry_set(sc, 7,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
        }
        if_maddr_runlock(ifp);

	return (0);
}

/*
 *
 * Statistics and Sysctls.
 *
 */

#if 0
static void
cpsw_stats_dump(struct cpsw_softc *sc)
{
	int i;
	uint32_t r;

	for (i = 0; i < CPSW_SYSCTL_COUNT; ++i) {
		r = cpsw_read_4(sc, CPSW_STATS_OFFSET +
		    cpsw_stat_sysctls[i].reg);
		CPSW_DEBUGF(("%s: %ju + %u = %ju", cpsw_stat_sysctls[i].oid,
			     (intmax_t)sc->shadow_stats[i], r,
			     (intmax_t)sc->shadow_stats[i] + r));
	}
}
#endif

static void
cpsw_stats_collect(struct cpsw_softc *sc)
{
	int i;
	uint32_t r;

	CPSW_DEBUGF(("Controller shadow statistics updated."));

	for (i = 0; i < CPSW_SYSCTL_COUNT; ++i) {
		r = cpsw_read_4(sc, CPSW_STATS_OFFSET +
		    cpsw_stat_sysctls[i].reg);
		sc->shadow_stats[i] += r;
		cpsw_write_4(sc, CPSW_STATS_OFFSET + cpsw_stat_sysctls[i].reg, r);
	}
}

static int
cpsw_stats_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct cpsw_softc *sc;
	struct cpsw_stat *stat;
	uint64_t result;

	sc = (struct cpsw_softc *)arg1;
	stat = &cpsw_stat_sysctls[oidp->oid_number];
	result = sc->shadow_stats[oidp->oid_number];
	result += cpsw_read_4(sc, CPSW_STATS_OFFSET + stat->reg);
	return (sysctl_handle_64(oidp, &result, 0, req));
}

static int
cpsw_stat_attached(SYSCTL_HANDLER_ARGS)
{
	struct cpsw_softc *sc;
	struct bintime t;
	unsigned result;

	sc = (struct cpsw_softc *)arg1;
	getbinuptime(&t);
	bintime_sub(&t, &sc->attach_uptime);
	result = t.sec;
	return (sysctl_handle_int(oidp, &result, 0, req));
}

static int
cpsw_stat_uptime(SYSCTL_HANDLER_ARGS)
{
	struct cpsw_softc *sc;
	struct bintime t;
	unsigned result;

	sc = (struct cpsw_softc *)arg1;
	if (sc->ifp->if_drv_flags & IFF_DRV_RUNNING) {
		getbinuptime(&t);
		bintime_sub(&t, &sc->init_uptime);
		result = t.sec;
	} else
		result = 0;
	return (sysctl_handle_int(oidp, &result, 0, req));
}

static void
cpsw_add_queue_sysctls(struct sysctl_ctx_list *ctx, struct sysctl_oid *node, struct cpsw_queue *queue)
{
	struct sysctl_oid_list *parent;

	parent = SYSCTL_CHILDREN(node);
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "totalBuffers",
	    CTLFLAG_RD, &queue->queue_slots, 0,
	    "Total buffers currently assigned to this queue");
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "activeBuffers",
	    CTLFLAG_RD, &queue->active_queue_len, 0,
	    "Buffers currently registered with hardware controller");
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "maxActiveBuffers",
	    CTLFLAG_RD, &queue->max_active_queue_len, 0,
	    "Max value of activeBuffers since last driver reset");
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "availBuffers",
	    CTLFLAG_RD, &queue->avail_queue_len, 0,
	    "Buffers allocated to this queue but not currently "
	    "registered with hardware controller");
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "maxAvailBuffers",
	    CTLFLAG_RD, &queue->max_avail_queue_len, 0,
	    "Max value of availBuffers since last driver reset");
	SYSCTL_ADD_UINT(ctx, parent, OID_AUTO, "totalEnqueued",
	    CTLFLAG_RD, &queue->queue_adds, 0,
	    "Total buffers added to queue");
	SYSCTL_ADD_UINT(ctx, parent, OID_AUTO, "totalDequeued",
	    CTLFLAG_RD, &queue->queue_removes, 0,
	    "Total buffers removed from queue");
	SYSCTL_ADD_UINT(ctx, parent, OID_AUTO, "longestChain",
	    CTLFLAG_RD, &queue->longest_chain, 0,
	    "Max buffers used for a single packet");
}

static void
cpsw_add_watchdog_sysctls(struct sysctl_ctx_list *ctx, struct sysctl_oid *node, struct cpsw_softc *sc)
{
	struct sysctl_oid_list *parent;

	parent = SYSCTL_CHILDREN(node);
	SYSCTL_ADD_INT(ctx, parent, OID_AUTO, "resets",
	    CTLFLAG_RD, &sc->watchdog.resets, 0,
	    "Total number of watchdog resets");
}

static void
cpsw_add_sysctls(struct cpsw_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *stats_node, *queue_node, *node;
	struct sysctl_oid_list *parent, *stats_parent, *queue_parent;
	int i;

	ctx = device_get_sysctl_ctx(sc->dev);
	parent = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev));

	SYSCTL_ADD_PROC(ctx, parent, OID_AUTO, "attachedSecs",
	    CTLTYPE_UINT | CTLFLAG_RD, sc, 0, cpsw_stat_attached, "IU",
	    "Time since driver attach");

	SYSCTL_ADD_PROC(ctx, parent, OID_AUTO, "uptime",
	    CTLTYPE_UINT | CTLFLAG_RD, sc, 0, cpsw_stat_uptime, "IU",
	    "Seconds since driver init");

	stats_node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, "stats",
				     CTLFLAG_RD, NULL, "CPSW Statistics");
	stats_parent = SYSCTL_CHILDREN(stats_node);
	for (i = 0; i < CPSW_SYSCTL_COUNT; ++i) {
		SYSCTL_ADD_PROC(ctx, stats_parent, i,
				cpsw_stat_sysctls[i].oid,
				CTLTYPE_U64 | CTLFLAG_RD, sc, 0,
				cpsw_stats_sysctl, "IU",
				cpsw_stat_sysctls[i].oid);
	}

	queue_node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, "queue",
	    CTLFLAG_RD, NULL, "CPSW Queue Statistics");
	queue_parent = SYSCTL_CHILDREN(queue_node);

	node = SYSCTL_ADD_NODE(ctx, queue_parent, OID_AUTO, "tx",
	    CTLFLAG_RD, NULL, "TX Queue Statistics");
	cpsw_add_queue_sysctls(ctx, node, &sc->tx);

	node = SYSCTL_ADD_NODE(ctx, queue_parent, OID_AUTO, "rx",
	    CTLFLAG_RD, NULL, "RX Queue Statistics");
	cpsw_add_queue_sysctls(ctx, node, &sc->rx);

	node = SYSCTL_ADD_NODE(ctx, parent, OID_AUTO, "watchdog",
	    CTLFLAG_RD, NULL, "Watchdog Statistics");
	cpsw_add_watchdog_sysctls(ctx, node, sc);
}

