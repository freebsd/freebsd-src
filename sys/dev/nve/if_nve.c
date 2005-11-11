/*
 * Copyright (c) 2005 by David E. O'Brien <obrien@FreeBSD.org>.
 * Copyright (c) 2003,2004 by Quinton Dolan <q@onthenet.com.au>. 
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $Id: if_nv.c,v 1.19 2004/08/12 14:00:05 q Exp $
 */

/*
 * NVIDIA nForce MCP Networking Adapter driver
 * 
 * This is a port of the NVIDIA MCP Linux ethernet driver distributed by NVIDIA
 * through their web site.
 * 
 * All mainstream nForce and nForce2 motherboards are supported. This module
 * is as stable, sometimes more stable, than the linux version. (Recent
 * Linux stability issues seem to be related to some issues with newer
 * distributions using GCC 3.x, however this don't appear to effect FreeBSD
 * 5.x).
 * 
 * In accordance with the NVIDIA distribution license it is necessary to
 * link this module against the nvlibnet.o binary object included in the
 * Linux driver source distribution. The binary component is not modified in
 * any way and is simply linked against a FreeBSD equivalent of the nvnet.c
 * linux kernel module "wrapper".
 * 
 * The Linux driver uses a common code API that is shared between Win32 and
 * i386 Linux. This abstracts the low level driver functions and uses
 * callbacks and hooks to access the underlying hardware device. By using
 * this same API in a FreeBSD kernel module it is possible to support the
 * hardware without breaching the Linux source distributions licensing
 * requirements, or obtaining the hardware programming specifications.
 * 
 * Although not conventional, it works, and given the relatively small
 * amount of hardware centric code, it's hopefully no more buggy than its
 * linux counterpart.
 *
 * NVIDIA now support the nForce3 AMD64 platform, however I have been
 * unable to access such a system to verify support. However, the code is
 * reported to work with little modification when compiled with the AMD64
 * version of the NVIDIA Linux library. All that should be necessary to make
 * the driver work is to link it directly into the kernel, instead of as a
 * module, and apply the docs/amd64.diff patch in this source distribution to
 * the NVIDIA Linux driver source.
 *
 * This driver should work on all versions of FreeBSD since 4.9/5.1 as well
 * as recent versions of DragonFly.
 *
 * Written by Quinton Dolan <q@onthenet.com.au> 
 * Portions based on existing FreeBSD network drivers. 
 * NVIDIA API usage derived from distributed NVIDIA NVNET driver source files.
 * 
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
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/module.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/if_vlan_var.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/clock.h>	/* for DELAY */
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miibus_if.h"

/* Include NVIDIA Linux driver header files */
#define	linux
#include <contrib/dev/nve/basetype.h>
#include <contrib/dev/nve/phy.h>
#include "os+%DIKED-nve.h"
#include <contrib/dev/nve/drvinfo.h>
#include <contrib/dev/nve/adapter.h>
#undef linux

#include <dev/nve/if_nvereg.h>

MODULE_DEPEND(nve, pci, 1, 1, 1);
MODULE_DEPEND(nve, ether, 1, 1, 1);
MODULE_DEPEND(nve, miibus, 1, 1, 1);

static int      nve_probe(device_t);
static int      nve_attach(device_t);
static int      nve_detach(device_t);
static void     nve_init(void *);
static void     nve_stop(struct nve_softc *);
static void     nve_shutdown(device_t);
static int      nve_init_rings(struct nve_softc *);
static void     nve_free_rings(struct nve_softc *);

static void     nve_ifstart(struct ifnet *);
static int      nve_ioctl(struct ifnet *, u_long, caddr_t);
static void     nve_intr(void *);
static void     nve_tick(void *);
static void     nve_setmulti(struct nve_softc *);
static void     nve_watchdog(struct ifnet *);
static void     nve_update_stats(struct nve_softc *);

static int      nve_ifmedia_upd(struct ifnet *);
static void     nve_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int      nve_miibus_readreg(device_t, int, int);
static void     nve_miibus_writereg(device_t, int, int, int);

static void     nve_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static void     nve_dmamap_tx_cb(void *, bus_dma_segment_t *, int, bus_size_t, int);

static NV_SINT32 nve_osalloc(PNV_VOID, PMEMORY_BLOCK);
static NV_SINT32 nve_osfree(PNV_VOID, PMEMORY_BLOCK);
static NV_SINT32 nve_osallocex(PNV_VOID, PMEMORY_BLOCKEX);
static NV_SINT32 nve_osfreeex(PNV_VOID, PMEMORY_BLOCKEX);
static NV_SINT32 nve_osclear(PNV_VOID, PNV_VOID, NV_SINT32);
static NV_SINT32 nve_osdelay(PNV_VOID, NV_UINT32);
static NV_SINT32 nve_osallocrxbuf(PNV_VOID, PMEMORY_BLOCK, PNV_VOID *);
static NV_SINT32 nve_osfreerxbuf(PNV_VOID, PMEMORY_BLOCK, PNV_VOID);
static NV_SINT32 nve_ospackettx(PNV_VOID, PNV_VOID, NV_UINT32);
static NV_SINT32 nve_ospacketrx(PNV_VOID, PNV_VOID, NV_UINT32, NV_UINT8 *, NV_UINT8);
static NV_SINT32 nve_oslinkchg(PNV_VOID, NV_SINT32);
static NV_SINT32 nve_osalloctimer(PNV_VOID, PNV_VOID *);
static NV_SINT32 nve_osfreetimer(PNV_VOID, PNV_VOID);
static NV_SINT32 nve_osinittimer(PNV_VOID, PNV_VOID, PTIMER_FUNC, PNV_VOID);
static NV_SINT32 nve_ossettimer(PNV_VOID, PNV_VOID, NV_UINT32);
static NV_SINT32 nve_oscanceltimer(PNV_VOID, PNV_VOID);

static NV_SINT32 nve_ospreprocpkt(PNV_VOID, PNV_VOID, PNV_VOID *, NV_UINT8 *, NV_UINT8);
static PNV_VOID  nve_ospreprocpktnopq(PNV_VOID, PNV_VOID);
static NV_SINT32 nve_osindicatepkt(PNV_VOID, PNV_VOID *, NV_UINT32);
static NV_SINT32 nve_oslockalloc(PNV_VOID, NV_SINT32, PNV_VOID *);
static NV_SINT32 nve_oslockacquire(PNV_VOID, NV_SINT32, PNV_VOID);
static NV_SINT32 nve_oslockrelease(PNV_VOID, NV_SINT32, PNV_VOID);
static PNV_VOID  nve_osreturnbufvirt(PNV_VOID, PNV_VOID);

static device_method_t nve_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, nve_probe),
	DEVMETHOD(device_attach, nve_attach),
	DEVMETHOD(device_detach, nve_detach),
	DEVMETHOD(device_shutdown, nve_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, nve_miibus_readreg),
	DEVMETHOD(miibus_writereg, nve_miibus_writereg),

	{0, 0}
};

static driver_t nve_driver = {
	"nve",
	nve_methods,
	sizeof(struct nve_softc)
};

static devclass_t nve_devclass;

static int      nve_pollinterval = 0;
SYSCTL_INT(_hw, OID_AUTO, nve_pollinterval, CTLFLAG_RW,
	   &nve_pollinterval, 0, "delay between interface polls");

DRIVER_MODULE(nve, pci, nve_driver, nve_devclass, 0, 0);
DRIVER_MODULE(miibus, nve, miibus_driver, miibus_devclass, 0, 0);

static struct nve_type nve_devs[] = {
	{NVIDIA_VENDORID, NFORCE_MCPNET1_DEVICEID,
	"NVIDIA nForce MCP Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET2_DEVICEID,
	"NVIDIA nForce MCP2 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET3_DEVICEID,
	"NVIDIA nForce MCP3 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET4_DEVICEID,
	"NVIDIA nForce MCP4 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET5_DEVICEID,
	"NVIDIA nForce MCP5 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET6_DEVICEID,
	"NVIDIA nForce MCP6 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET7_DEVICEID,
	"NVIDIA nForce MCP7 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET8_DEVICEID,
	"NVIDIA nForce MCP8 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET9_DEVICEID,
	"NVIDIA nForce MCP9 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET10_DEVICEID,
	"NVIDIA nForce MCP10 Networking Adapter"},
	{NVIDIA_VENDORID, NFORCE_MCPNET11_DEVICEID,
	"NVIDIA nForce MCP11 Networking Adapter"},
	{0, 0, NULL}
};

/* DMA MEM map callback function to get data segment physical address */
static void
nve_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nsegs, int error)
{
	if (error)
		return;

	KASSERT(nsegs == 1,
	    ("Too many DMA segments returned when mapping DMA memory"));
	*(bus_addr_t *)arg = segs->ds_addr;
}

/* DMA RX map callback function to get data segment physical address */
static void
nve_dmamap_rx_cb(void *arg, bus_dma_segment_t * segs, int nsegs,
    bus_size_t mapsize, int error)
{
	if (error)
		return;
	*(bus_addr_t *)arg = segs->ds_addr;
}

/*
 * DMA TX buffer callback function to allocate fragment data segment
 * addresses
 */
static void
nve_dmamap_tx_cb(void *arg, bus_dma_segment_t * segs, int nsegs, bus_size_t mapsize, int error)
{
	struct nve_tx_desc *info;

	info = arg;
	if (error)
		return;
	KASSERT(nsegs < NV_MAX_FRAGS,
	    ("Too many DMA segments returned when mapping mbuf"));
	info->numfrags = nsegs;
	bcopy(segs, info->frags, nsegs * sizeof(bus_dma_segment_t));
}

/* Probe for supported hardware ID's */
static int
nve_probe(device_t dev)
{
	struct nve_type *t;

	t = nve_devs;
	/* Check for matching PCI DEVICE ID's */
	while (t->name != NULL) {
		if ((pci_get_vendor(dev) == t->vid_id) &&
		    (pci_get_device(dev) == t->dev_id)) {
			device_set_desc(dev, t->name);
			return (0);
		}
		t++;
	}

	return (ENXIO);
}

/* Attach driver and initialise hardware for use */
static int
nve_attach(device_t dev)
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct nve_softc	*sc;
	struct ifnet		*ifp;
	OS_API			*osapi;
	ADAPTER_OPEN_PARAMS	OpenParams;
	int			error = 0, i, rid, unit;

	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_attach - entry\n");

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	/* Allocate mutex */
	mtx_init(&sc->mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	mtx_init(&sc->osmtx, device_get_nameunit(dev), NULL, MTX_SPIN);

	sc->dev = dev;
	sc->unit = unit;

	/* Preinitialize data structures */
	bzero(&OpenParams, sizeof(ADAPTER_OPEN_PARAMS));

	/* Enable bus mastering */
	pci_enable_busmaster(dev);

	/* Allocate memory mapped address space */
	rid = NV_RID;
	sc->res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1,
	    RF_ACTIVE);

	if (sc->res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}
	sc->sc_st = rman_get_bustag(sc->res);
	sc->sc_sh = rman_get_bushandle(sc->res);

	/* Allocate interrupt */
	rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}
	/* Allocate DMA tags */
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
		     BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES * NV_MAX_FRAGS,
				   NV_MAX_FRAGS, MCLBYTES, 0,
				   busdma_lock_mutex, &Giant,
				   &sc->mtag);
	if (error) {
		device_printf(dev, "couldn't allocate dma tag\n");
		goto fail;
	}
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct nve_rx_desc) * RX_RING_SIZE, 1,
	    sizeof(struct nve_rx_desc) * RX_RING_SIZE, 0,
	    busdma_lock_mutex, &Giant,
	    &sc->rtag);
	if (error) {
		device_printf(dev, "couldn't allocate dma tag\n");
		goto fail;
	}
	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct nve_tx_desc) * TX_RING_SIZE, 1,
	    sizeof(struct nve_tx_desc) * TX_RING_SIZE, 0,
	    busdma_lock_mutex, &Giant,
	    &sc->ttag);
	if (error) {
		device_printf(dev, "couldn't allocate dma tag\n");
		goto fail;
	}
	/* Allocate DMA safe memory and get the DMA addresses. */
	error = bus_dmamem_alloc(sc->ttag, (void **)&sc->tx_desc,
	    BUS_DMA_WAITOK, &sc->tmap);
	if (error) {
		device_printf(dev, "couldn't allocate dma memory\n");
		goto fail;
	}
	bzero(sc->tx_desc, sizeof(struct nve_tx_desc) * TX_RING_SIZE);
	error = bus_dmamap_load(sc->ttag, sc->tmap, sc->tx_desc,
		    sizeof(struct nve_tx_desc) * TX_RING_SIZE, nve_dmamap_cb,
		    &sc->tx_addr, 0);
	if (error) {
		device_printf(dev, "couldn't map dma memory\n");
		goto fail;
	}
	error = bus_dmamem_alloc(sc->rtag, (void **)&sc->rx_desc,
	    BUS_DMA_WAITOK, &sc->rmap);
	if (error) {
		device_printf(dev, "couldn't allocate dma memory\n");
		goto fail;
	}
	bzero(sc->rx_desc, sizeof(struct nve_rx_desc) * RX_RING_SIZE);
	error = bus_dmamap_load(sc->rtag, sc->rmap, sc->rx_desc,
	    sizeof(struct nve_rx_desc) * RX_RING_SIZE, nve_dmamap_cb,
	    &sc->rx_addr, 0);
	if (error) {
		device_printf(dev, "couldn't map dma memory\n");
		goto fail;
	}
	/* Initialize rings. */
	if (nve_init_rings(sc)) {
		device_printf(dev, "failed to init rings\n");
		error = ENXIO;
		goto fail;
	}
	/* Setup NVIDIA API callback routines */
	osapi				= &sc->osapi;
	osapi->pOSCX			= sc;
	osapi->pfnAllocMemory		= nve_osalloc;
	osapi->pfnFreeMemory		= nve_osfree;
	osapi->pfnAllocMemoryEx		= nve_osallocex;
	osapi->pfnFreeMemoryEx		= nve_osfreeex;
	osapi->pfnClearMemory		= nve_osclear;
	osapi->pfnStallExecution	= nve_osdelay;
	osapi->pfnAllocReceiveBuffer	= nve_osallocrxbuf;
	osapi->pfnFreeReceiveBuffer	= nve_osfreerxbuf;
	osapi->pfnPacketWasSent		= nve_ospackettx;
	osapi->pfnPacketWasReceived	= nve_ospacketrx;
	osapi->pfnLinkStateHasChanged	= nve_oslinkchg;
	osapi->pfnAllocTimer		= nve_osalloctimer;
	osapi->pfnFreeTimer		= nve_osfreetimer;
	osapi->pfnInitializeTimer	= nve_osinittimer;
	osapi->pfnSetTimer		= nve_ossettimer;
	osapi->pfnCancelTimer		= nve_oscanceltimer;
	osapi->pfnPreprocessPacket	= nve_ospreprocpkt;
	osapi->pfnPreprocessPacketNopq	= nve_ospreprocpktnopq;
	osapi->pfnIndicatePackets	= nve_osindicatepkt;
	osapi->pfnLockAlloc		= nve_oslockalloc;
	osapi->pfnLockAcquire		= nve_oslockacquire;
	osapi->pfnLockRelease		= nve_oslockrelease;
	osapi->pfnReturnBufferVirtual	= nve_osreturnbufvirt;

	sc->linkup = FALSE;
	sc->max_frame_size = ETHERMTU + ETHER_HDR_LEN + FCS_LEN;

	/* TODO - We don't support hardware offload yet */
	sc->hwmode = 1;
	sc->media = 0;

	/* Set NVIDIA API startup parameters */
	OpenParams.MaxDpcLoop = 2;
	OpenParams.MaxRxPkt = RX_RING_SIZE;
	OpenParams.MaxTxPkt = TX_RING_SIZE;
	OpenParams.SentPacketStatusSuccess = 1;
	OpenParams.SentPacketStatusFailure = 0;
	OpenParams.MaxRxPktToAccumulate = 6;
	OpenParams.ulPollInterval = nve_pollinterval;
	OpenParams.SetForcedModeEveryNthRxPacket = 0;
	OpenParams.SetForcedModeEveryNthTxPacket = 0;
	OpenParams.RxForcedInterrupt = 0;
	OpenParams.TxForcedInterrupt = 0;
	OpenParams.pOSApi = osapi;
	OpenParams.pvHardwareBaseAddress = rman_get_virtual(sc->res);
	OpenParams.bASFEnabled = 0;
	OpenParams.ulDescriptorVersion = sc->hwmode;
	OpenParams.ulMaxPacketSize = sc->max_frame_size;
	OpenParams.DeviceId = pci_get_device(dev);

	/* Open NVIDIA Hardware API */
	error = ADAPTER_Open(&OpenParams, (void **)&(sc->hwapi), &sc->phyaddr);
	if (error) {
		device_printf(dev,
		    "failed to open NVIDIA Hardware API: 0x%x\n", error);
		goto fail;
	}
	
	/* TODO - Add support for MODE2 hardware offload */ 
	
	bzero(&sc->adapterdata, sizeof(sc->adapterdata));
	
	sc->adapterdata.ulMediaIF = sc->media;
	sc->adapterdata.ulModeRegTxReadCompleteEnable = 1;
	sc->hwapi->pfnSetCommonData(sc->hwapi->pADCX, &sc->adapterdata);
	
	/* MAC is loaded backwards into h/w reg */
	sc->hwapi->pfnGetNodeAddress(sc->hwapi->pADCX, sc->original_mac_addr);
	for (i = 0; i < 6; i++) {
		eaddr[i] = sc->original_mac_addr[5 - i];
	}
	sc->hwapi->pfnSetNodeAddress(sc->hwapi->pADCX, eaddr);

	/* Display ethernet address ,... */
	device_printf(dev, "Ethernet address %6D\n", eaddr, ":");

	/* Allocate interface structures */
	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}

	/* Probe device for MII interface to PHY */
	DEBUGOUT(NVE_DEBUG_INIT, "nve: do mii_phy_probe\n");
	if (mii_phy_probe(dev, &sc->miibus, nve_ifmedia_upd, nve_ifmedia_sts)) {
		device_printf(dev, "MII without any phy!\n");
		error = ENXIO;
		goto fail;
	}

	/* Setup interface parameters */
	ifp->if_softc = sc;
	if_initname(ifp, "nve", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nve_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = nve_ifstart;
	ifp->if_watchdog = nve_watchdog;
	ifp->if_timer = 0;
	ifp->if_init = nve_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = IF_Mbps(100);
	ifp->if_snd.ifq_maxlen = TX_RING_SIZE - 1;
	ifp->if_capabilities |= IFCAP_VLAN_MTU;

	/* Attach to OS's managers. */
	ether_ifattach(ifp, eaddr);
	callout_handle_init(&sc->stat_ch);

	/* Activate our interrupt handler. - attach last to avoid lock */
	error = bus_setup_intr(sc->dev, sc->irq, INTR_TYPE_NET, nve_intr,
	    sc, &sc->sc_ih);
	if (error) {
		device_printf(sc->dev, "couldn't set up interrupt handler\n");
		goto fail;
	}
	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_attach - exit\n");

fail:
	if (error)
		nve_detach(dev);

	return (error);
}

/* Detach interface for module unload */
static int
nve_detach(device_t dev)
{
	struct nve_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;

	KASSERT(mtx_initialized(&sc->mtx), ("mutex not initialized"));
	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: nve_detach - entry\n");

	ifp = sc->ifp;

	if (device_is_attached(dev)) {
		nve_stop(sc);
		/* XXX shouldn't hold lock over call to ether_ifdetch */
		ether_ifdetach(ifp);
	}

	if (sc->miibus)
		device_delete_child(dev, sc->miibus);
	bus_generic_detach(dev);

	/* Reload unreversed address back into MAC in original state */
	if (sc->original_mac_addr)
		sc->hwapi->pfnSetNodeAddress(sc->hwapi->pADCX,
		    sc->original_mac_addr);

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: do pfnClose\n");
	/* Detach from NVIDIA hardware API */
	if (sc->hwapi->pfnClose)
		sc->hwapi->pfnClose(sc->hwapi->pADCX, FALSE);
	/* Release resources */
	if (sc->sc_ih)
		bus_teardown_intr(sc->dev, sc->irq, sc->sc_ih);
	if (sc->irq)
		bus_release_resource(sc->dev, SYS_RES_IRQ, 0, sc->irq);
	if (sc->res)
		bus_release_resource(sc->dev, SYS_RES_MEMORY, NV_RID, sc->res);

	nve_free_rings(sc);

	if (sc->tx_desc) {
		bus_dmamap_unload(sc->rtag, sc->rmap);
		bus_dmamem_free(sc->rtag, sc->rx_desc, sc->rmap);
		bus_dmamap_destroy(sc->rtag, sc->rmap);
	}
	if (sc->mtag)
		bus_dma_tag_destroy(sc->mtag);
	if (sc->ttag)
		bus_dma_tag_destroy(sc->ttag);
	if (sc->rtag)
		bus_dma_tag_destroy(sc->rtag);

	NVE_UNLOCK(sc);
	if (ifp)
		if_free(ifp);
	mtx_destroy(&sc->mtx);
	mtx_destroy(&sc->osmtx);

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: nve_detach - exit\n");

	return (0);
}

/* Initialise interface and start it "RUNNING" */
static void
nve_init(void *xsc)
{
	struct nve_softc *sc = xsc;
	struct ifnet *ifp;
	int error;

	NVE_LOCK(sc);
	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_init - entry (%d)\n", sc->linkup);

	ifp = sc->ifp;

	/* Do nothing if already running */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		goto fail;

	nve_stop(sc);
	DEBUGOUT(NVE_DEBUG_INIT, "nve: do pfnInit\n");

	/* Setup Hardware interface and allocate memory structures */
	error = sc->hwapi->pfnInit(sc->hwapi->pADCX, 
	    0, /* force speed */ 
	    0, /* force full duplex */
	    0, /* force mode */
	    0, /* force async mode */
	    &sc->linkup);

	if (error) {
		device_printf(sc->dev,
		    "failed to start NVIDIA Hardware interface\n");
		goto fail;
	}
	/* Set the MAC address */
	sc->hwapi->pfnSetNodeAddress(sc->hwapi->pADCX, IF_LLADDR(sc->ifp));
	sc->hwapi->pfnEnableInterrupts(sc->hwapi->pADCX);
	sc->hwapi->pfnStart(sc->hwapi->pADCX);

	/* Setup multicast filter */
	nve_setmulti(sc);
	nve_ifmedia_upd(ifp);

	/* Update interface parameters */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->stat_ch = timeout(nve_tick, sc, hz);

	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_init - exit\n");

fail:
	NVE_UNLOCK(sc);

	return;
}

/* Stop interface activity ie. not "RUNNING" */
static void
nve_stop(struct nve_softc *sc)
{
	struct ifnet *ifp;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_stop - entry\n");

	ifp = sc->ifp;
	ifp->if_timer = 0;

	/* Cancel tick timer */
	untimeout(nve_tick, sc, sc->stat_ch);

	/* Stop hardware activity */
	sc->hwapi->pfnDisableInterrupts(sc->hwapi->pADCX);
	sc->hwapi->pfnStop(sc->hwapi->pADCX, 0);

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: do pfnDeinit\n");
	/* Shutdown interface and deallocate memory buffers */
	if (sc->hwapi->pfnDeinit)
		sc->hwapi->pfnDeinit(sc->hwapi->pADCX, 0);

	sc->linkup = 0;
	sc->cur_rx = 0;
	sc->pending_rxs = 0;
	sc->pending_txs = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_stop - exit\n");

	NVE_UNLOCK(sc);

	return;
}

/* Shutdown interface for unload/reboot */
static void
nve_shutdown(device_t dev)
{
	struct nve_softc *sc;

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: nve_shutdown\n");

	sc = device_get_softc(dev);

	/* Stop hardware activity */
	nve_stop(sc);
}

/* Allocate TX ring buffers */
static int
nve_init_rings(struct nve_softc *sc)
{
	int error, i;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_init_rings - entry\n");

	sc->cur_rx = sc->cur_tx = sc->pending_rxs = sc->pending_txs = 0;
	/* Initialise RX ring */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct nve_rx_desc *desc = sc->rx_desc + i;
		struct nve_map_buffer *buf = &desc->buf;

		buf->mbuf = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (buf->mbuf == NULL) {
			device_printf(sc->dev, "couldn't allocate mbuf\n");
			nve_free_rings(sc);
			error = ENOBUFS;
			goto fail;
		}
		buf->mbuf->m_len = buf->mbuf->m_pkthdr.len = MCLBYTES;
		m_adj(buf->mbuf, ETHER_ALIGN);

		error = bus_dmamap_create(sc->mtag, 0, &buf->map);
		if (error) {
			device_printf(sc->dev, "couldn't create dma map\n");
			nve_free_rings(sc);
			goto fail;
		}
		error = bus_dmamap_load_mbuf(sc->mtag, buf->map, buf->mbuf,
					  nve_dmamap_rx_cb, &desc->paddr, 0);
		if (error) {
			device_printf(sc->dev, "couldn't dma map mbuf\n");
			nve_free_rings(sc);
			goto fail;
		}
		bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_PREREAD);

		desc->buflength = buf->mbuf->m_len;
		desc->vaddr = mtod(buf->mbuf, caddr_t);
	}
	bus_dmamap_sync(sc->rtag, sc->rmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Initialize TX ring */
	for (i = 0; i < TX_RING_SIZE; i++) {
		struct nve_tx_desc *desc = sc->tx_desc + i;
		struct nve_map_buffer *buf = &desc->buf;

		buf->mbuf = NULL;

		error = bus_dmamap_create(sc->mtag, 0, &buf->map);
		if (error) {
			device_printf(sc->dev, "couldn't create dma map\n");
			nve_free_rings(sc);
			goto fail;
		}
	}
	bus_dmamap_sync(sc->ttag, sc->tmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	DEBUGOUT(NVE_DEBUG_INIT, "nve: nve_init_rings - exit\n");

fail:
	NVE_UNLOCK(sc);

	return (error);
}

/* Free the TX ring buffers */
static void
nve_free_rings(struct nve_softc *sc)
{
	int i;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: nve_free_rings - entry\n");

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct nve_rx_desc *desc = sc->rx_desc + i;
		struct nve_map_buffer *buf = &desc->buf;

		if (buf->mbuf) {
			bus_dmamap_unload(sc->mtag, buf->map);
			bus_dmamap_destroy(sc->mtag, buf->map);
			m_freem(buf->mbuf);
		}
		buf->mbuf = NULL;
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct nve_tx_desc *desc = sc->tx_desc + i;
		struct nve_map_buffer *buf = &desc->buf;

		if (buf->mbuf) {
			bus_dmamap_unload(sc->mtag, buf->map);
			bus_dmamap_destroy(sc->mtag, buf->map);
			m_freem(buf->mbuf);
		}
		buf->mbuf = NULL;
	}

	DEBUGOUT(NVE_DEBUG_DEINIT, "nve: nve_free_rings - exit\n");

	NVE_UNLOCK(sc);
}

/* Main loop for sending packets from OS to interface */
static void
nve_ifstart(struct ifnet *ifp)
{
	struct nve_softc *sc = ifp->if_softc;
	struct nve_map_buffer *buf;
	struct mbuf    *m0, *m;
	struct nve_tx_desc *desc;
	ADAPTER_WRITE_DATA txdata;
	int error, i;

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_ifstart - entry\n");

	/* If link is down/busy or queue is empty do nothing */
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE || ifp->if_snd.ifq_head == NULL)
		return;

	/* Transmit queued packets until sent or TX ring is full */
	while (sc->pending_txs < TX_RING_SIZE) {
		desc = sc->tx_desc + sc->cur_tx;
		buf = &desc->buf;

		/* Get next packet to send. */
		IF_DEQUEUE(&ifp->if_snd, m0);

		/* If nothing to send, return. */
		if (m0 == NULL)
			return;

		/* Map MBUF for DMA access */
		error = bus_dmamap_load_mbuf(sc->mtag, buf->map, m0,
		    nve_dmamap_tx_cb, desc, BUS_DMA_NOWAIT);

		if (error && error != EFBIG) {
			m_freem(m0);
			sc->tx_errors++;
			continue;
		}
		/*
		 * Packet has too many fragments - defrag into new mbuf
		 * cluster
		 */
		if (error) {
			m = m_defrag(m0, M_DONTWAIT);
			if (m == NULL) {
				m_freem(m0);
				sc->tx_errors++;
				continue;
			}
			m0 = m;

			error = bus_dmamap_load_mbuf(sc->mtag, buf->map, m,
			    nve_dmamap_tx_cb, desc, BUS_DMA_NOWAIT);
			if (error) {
				m_freem(m);
				sc->tx_errors++;
				continue;
			}
		}
		/* Do sync on DMA bounce buffer */
		bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_PREWRITE);

		buf->mbuf = m0;
		txdata.ulNumberOfElements = desc->numfrags;
		txdata.pvID = (PVOID)desc;

		/* Put fragments into API element list */
		txdata.ulTotalLength = buf->mbuf->m_len;
		for (i = 0; i < desc->numfrags; i++) {
			txdata.sElement[i].ulLength =
			    (ulong)desc->frags[i].ds_len;
			txdata.sElement[i].pPhysical =
			    (PVOID)desc->frags[i].ds_addr;
		}

		/* Send packet to Nvidia API for transmission */
		error = sc->hwapi->pfnWrite(sc->hwapi->pADCX, &txdata);

		switch (error) {
		case ADAPTERERR_NONE:
			/* Packet was queued in API TX queue successfully */
			sc->pending_txs++;
			sc->cur_tx = (sc->cur_tx + 1) % TX_RING_SIZE;
			break;

		case ADAPTERERR_TRANSMIT_QUEUE_FULL:
			/* The API TX queue is full - requeue the packet */
			device_printf(sc->dev,
			    "nve_ifstart: transmit queue is full\n");
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			bus_dmamap_unload(sc->mtag, buf->map);
			IF_PREPEND(&ifp->if_snd, buf->mbuf);
			buf->mbuf = NULL;
			return;

		default:
			/* The API failed to queue/send the packet so dump it */
			device_printf(sc->dev, "nve_ifstart: transmit error\n");
			bus_dmamap_unload(sc->mtag, buf->map);
			m_freem(buf->mbuf);
			buf->mbuf = NULL;
			sc->tx_errors++;
			return;
		}
		/* Set watchdog timer. */
		ifp->if_timer = 8;

		/* Copy packet to BPF tap */
		BPF_MTAP(ifp, m0);
	}
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_ifstart - exit\n");
}

/* Handle IOCTL events */
static int
nve_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct nve_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct mii_data *mii;
	int error = 0;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_IOCTL, "nve: nve_ioctl - entry\n");

	switch (command) {
	case SIOCSIFMTU:
		/* Set MTU size */
		if (ifp->if_mtu == ifr->ifr_mtu)
			break;
		if (ifr->ifr_mtu + ifp->if_hdrlen <= MAX_PACKET_SIZE_1518) {
			ifp->if_mtu = ifr->ifr_mtu;
			nve_stop(sc);
			nve_init(sc);
		} else
			error = EINVAL;
		break;

	case SIOCSIFFLAGS:
		/* Setup interface flags */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				nve_init(sc);
				break;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				nve_stop(sc);
				break;
			}
		}
		/* Handle IFF_PROMISC and IFF_ALLMULTI flags. */
		nve_setmulti(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Setup multicast filter */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			nve_setmulti(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		/* Get/Set interface media parameters */
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;

	default:
		/* Everything else we forward to generic ether ioctl */
		error = ether_ioctl(ifp, (int)command, data);
		break;
	}

	DEBUGOUT(NVE_DEBUG_IOCTL, "nve: nve_ioctl - exit\n");

	NVE_UNLOCK(sc);

	return (error);
}

/* Interrupt service routine */
static void
nve_intr(void *arg)
{
	struct nve_softc *sc = arg;
	struct ifnet *ifp = sc->ifp;

	DEBUGOUT(NVE_DEBUG_INTERRUPT, "nve: nve_intr - entry\n");

	if (!ifp->if_flags & IFF_UP) {
		nve_stop(sc);
		return;
	}
	/* Handle interrupt event */
	if (sc->hwapi->pfnQueryInterrupt(sc->hwapi->pADCX)) {
		sc->hwapi->pfnHandleInterrupt(sc->hwapi->pADCX);
		sc->hwapi->pfnEnableInterrupts(sc->hwapi->pADCX);
	}
	if (ifp->if_snd.ifq_head != NULL)
		nve_ifstart(ifp);

	/* If no pending packets we don't need a timeout */
	if (sc->pending_txs == 0)
		sc->ifp->if_timer = 0;

	DEBUGOUT(NVE_DEBUG_INTERRUPT, "nve: nve_intr - exit\n");

	return;
}

/* Setup multicast filters */
static void
nve_setmulti(struct nve_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	PACKET_FILTER hwfilter;
	int i;
	u_int8_t andaddr[6], oraddr[6];

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_setmulti - entry\n");

	ifp = sc->ifp;

	/* Initialize filter */
	hwfilter.ulFilterFlags = 0;
	for (i = 0; i < 6; i++) {
		hwfilter.acMulticastAddress[i] = 0;
		hwfilter.acMulticastMask[i] = 0;
	}

	if (ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI)) {
		/* Accept all packets */
		hwfilter.ulFilterFlags |= ACCEPT_ALL_PACKETS;
		sc->hwapi->pfnSetPacketFilter(sc->hwapi->pADCX, &hwfilter);
		NVE_UNLOCK(sc);
		return;
	}
	/* Setup multicast filter */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		u_char *addrp;

		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		addrp = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
		for (i = 0; i < 6; i++) {
			u_int8_t mcaddr = addrp[i];
			andaddr[i] &= mcaddr;
			oraddr[i] |= mcaddr;
		}
	}
	IF_ADDR_UNLOCK(ifp);
	for (i = 0; i < 6; i++) {
		hwfilter.acMulticastAddress[i] = andaddr[i] & oraddr[i];
		hwfilter.acMulticastMask[i] = andaddr[i] | (~oraddr[i]);
	}

	/* Send filter to NVIDIA API */
	sc->hwapi->pfnSetPacketFilter(sc->hwapi->pADCX, &hwfilter);

	NVE_UNLOCK(sc);

	DEBUGOUT(NVE_DEBUG_RUNNING, "nve: nve_setmulti - exit\n");

	return;
}

/* Change the current media/mediaopts */
static int
nve_ifmedia_upd(struct ifnet *ifp)
{
	struct nve_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_ifmedia_upd\n");

	mii = device_get_softc(sc->miibus);

	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list)) {
			mii_phy_reset(miisc);
		}
	}
	mii_mediachg(mii);

	return (0);
}

/* Update current miibus PHY status of media */
static void
nve_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nve_softc *sc;
	struct mii_data *mii;

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_ifmedia_sts\n");

	sc = ifp->if_softc;
	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

/* miibus tick timer - maintain link status */
static void
nve_tick(void *xsc)
{
	struct nve_softc *sc = xsc;
	struct mii_data *mii;
	struct ifnet *ifp;

	NVE_LOCK(sc);

	ifp = sc->ifp;
	nve_update_stats(sc);

	mii = device_get_softc(sc->miibus);
	mii_tick(mii);

	if (mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		if (ifp->if_snd.ifq_head != NULL)
			nve_ifstart(ifp);
	}
	sc->stat_ch = timeout(nve_tick, sc, hz);

	NVE_UNLOCK(sc);

	return;
}

/* Update ifnet data structure with collected interface stats from API */
static void
nve_update_stats(struct nve_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	ADAPTER_STATS stats;

	NVE_LOCK(sc);

	if (sc->hwapi) {
		sc->hwapi->pfnGetStatistics(sc->hwapi->pADCX, &stats);

		ifp->if_ipackets = stats.ulSuccessfulReceptions;
		ifp->if_ierrors = stats.ulMissedFrames +
			stats.ulFailedReceptions +
			stats.ulCRCErrors +
			stats.ulFramingErrors +
			stats.ulOverFlowErrors;

		ifp->if_opackets = stats.ulSuccessfulTransmissions;
		ifp->if_oerrors = sc->tx_errors +
			stats.ulFailedTransmissions +
			stats.ulRetryErrors +
			stats.ulUnderflowErrors +
			stats.ulLossOfCarrierErrors +
			stats.ulLateCollisionErrors;

		ifp->if_collisions = stats.ulLateCollisionErrors;
	}
	NVE_UNLOCK(sc);

	return;
}

/* miibus Read PHY register wrapper - calls Nvidia API entry point */
static int
nve_miibus_readreg(device_t dev, int phy, int reg)
{
	struct nve_softc *sc = device_get_softc(dev);
	ULONG data;

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_miibus_readreg - entry\n");

	ADAPTER_ReadPhy(sc->hwapi->pADCX, phy, reg, &data);

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_miibus_readreg - exit\n");

	return (data);
}

/* miibus Write PHY register wrapper - calls Nvidia API entry point */
static void
nve_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct nve_softc *sc = device_get_softc(dev);

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_miibus_writereg - entry\n");

	ADAPTER_WritePhy(sc->hwapi->pADCX, phy, reg, (ulong)data);

	DEBUGOUT(NVE_DEBUG_MII, "nve: nve_miibus_writereg - exit\n");

	return;
}

/* Watchdog timer to prevent PHY lockups */
static void
nve_watchdog(struct ifnet *ifp)
{
	struct nve_softc *sc = ifp->if_softc;

	device_printf(sc->dev, "device timeout (%d)\n", sc->pending_txs);

	sc->tx_errors++;

	nve_stop(sc);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	nve_init(sc);

	if (ifp->if_snd.ifq_head != NULL)
		nve_ifstart(ifp);

	return;
}

/* --- Start of NVOSAPI interface --- */

/* Allocate DMA enabled general use memory for API */
static NV_SINT32
nve_osalloc(PNV_VOID ctx, PMEMORY_BLOCK mem)
{
	struct nve_softc *sc;
	bus_addr_t mem_physical;

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osalloc - %d\n", mem->uiLength);

	sc = (struct nve_softc *)ctx;

	mem->pLogical = (PVOID)contigmalloc(mem->uiLength, M_DEVBUF,
	    M_NOWAIT | M_ZERO, 0, ~0, PAGE_SIZE, 0);

	if (!mem->pLogical) {
		device_printf(sc->dev, "memory allocation failed\n");
		return (0);
	}
	memset(mem->pLogical, 0, (ulong)mem->uiLength);
	mem_physical = vtophys(mem->pLogical);
	mem->pPhysical = (PVOID)mem_physical;

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osalloc 0x%x/0x%x - %d\n",
	    (uint)mem->pLogical, (uint)mem->pPhysical, (uint)mem->uiLength);

	return (1);
}

/* Free allocated memory */
static NV_SINT32
nve_osfree(PNV_VOID ctx, PMEMORY_BLOCK mem)
{
	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osfree - 0x%x - %d\n",
	    (uint)mem->pLogical, (uint) mem->uiLength);

	contigfree(mem->pLogical, PAGE_SIZE, M_DEVBUF);
	return (1);
}

/* Copied directly from nvnet.c */
static NV_SINT32
nve_osallocex(PNV_VOID ctx, PMEMORY_BLOCKEX mem_block_ex)
{
	MEMORY_BLOCK mem_block;

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osallocex\n");

	mem_block_ex->pLogical = NULL;
	mem_block_ex->uiLengthOrig = mem_block_ex->uiLength;

	if ((mem_block_ex->AllocFlags & ALLOC_MEMORY_ALIGNED) &&
	    (mem_block_ex->AlignmentSize > 1)) {
		DEBUGOUT(NVE_DEBUG_API, "     aligning on %d\n",
		    mem_block_ex->AlignmentSize);
		mem_block_ex->uiLengthOrig += mem_block_ex->AlignmentSize;
	}
	mem_block.uiLength = mem_block_ex->uiLengthOrig;

	if (nve_osalloc(ctx, &mem_block) == 0) {
		return (0);
	}
	mem_block_ex->pLogicalOrig = mem_block.pLogical;
	mem_block_ex->pPhysicalOrigLow = (unsigned long)mem_block.pPhysical;
	mem_block_ex->pPhysicalOrigHigh = 0;

	mem_block_ex->pPhysical = mem_block.pPhysical;
	mem_block_ex->pLogical = mem_block.pLogical;

	if (mem_block_ex->uiLength != mem_block_ex->uiLengthOrig) {
		unsigned int offset;
		offset = mem_block_ex->pPhysicalOrigLow &
		    (mem_block_ex->AlignmentSize - 1);

		if (offset) {
			mem_block_ex->pPhysical =
			    (PVOID)((ulong)mem_block_ex->pPhysical +
			    mem_block_ex->AlignmentSize - offset);
			mem_block_ex->pLogical =
			    (PVOID)((ulong)mem_block_ex->pLogical +
			    mem_block_ex->AlignmentSize - offset);
		} /* if (offset) */
	} /* if (mem_block_ex->uiLength != *mem_block_ex->uiLengthOrig) */
	return (1);
}

/* Copied directly from nvnet.c */
static NV_SINT32
nve_osfreeex(PNV_VOID ctx, PMEMORY_BLOCKEX mem_block_ex)
{
	MEMORY_BLOCK mem_block;

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osfreeex\n");

	mem_block.pLogical = mem_block_ex->pLogicalOrig;
	mem_block.pPhysical = (PVOID)((ulong)mem_block_ex->pPhysicalOrigLow);
	mem_block.uiLength = mem_block_ex->uiLengthOrig;

	return (nve_osfree(ctx, &mem_block));
}

/* Clear memory region */
static NV_SINT32
nve_osclear(PNV_VOID ctx, PNV_VOID mem, NV_SINT32 length)
{
	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osclear\n");
	memset(mem, 0, length);
	return (1);
}

/* Sleep for a tick */
static NV_SINT32
nve_osdelay(PNV_VOID ctx, NV_UINT32 usec)
{
	DELAY(usec);
	return (1);
}

/* Allocate memory for rx buffer */
static NV_SINT32
nve_osallocrxbuf(PNV_VOID ctx, PMEMORY_BLOCK mem, PNV_VOID *id)
{
	struct nve_softc *sc = ctx;
	struct nve_rx_desc *desc;
	struct nve_map_buffer *buf;
	int error;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osallocrxbuf\n");

	if (sc->pending_rxs == RX_RING_SIZE) {
		device_printf(sc->dev, "rx ring buffer is full\n");
		goto fail;
	}
	desc = sc->rx_desc + sc->cur_rx;
	buf = &desc->buf;

	if (buf->mbuf == NULL) {
		buf->mbuf = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (buf->mbuf == NULL) {
			device_printf(sc->dev, "failed to allocate memory\n");
			goto fail;
		}
		buf->mbuf->m_len = buf->mbuf->m_pkthdr.len = MCLBYTES;
		m_adj(buf->mbuf, ETHER_ALIGN);

		error = bus_dmamap_load_mbuf(sc->mtag, buf->map, buf->mbuf,
		    nve_dmamap_rx_cb, &desc->paddr, 0);
		if (error) {
			device_printf(sc->dev, "failed to dmamap mbuf\n");
			m_freem(buf->mbuf);
			buf->mbuf = NULL;
			goto fail;
		}
		bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_PREREAD);
		desc->buflength = buf->mbuf->m_len;
		desc->vaddr = mtod(buf->mbuf, caddr_t);
	}
	sc->pending_rxs++;
	sc->cur_rx = (sc->cur_rx + 1) % RX_RING_SIZE;

	mem->pLogical = (void *)desc->vaddr;
	mem->pPhysical = (void *)desc->paddr;
	mem->uiLength = desc->buflength;
	*id = (void *)desc;

	NVE_UNLOCK(sc);
	return (1);
	
fail:
	NVE_UNLOCK(sc);
	return (0);
}

/* Free the rx buffer */
static NV_SINT32
nve_osfreerxbuf(PNV_VOID ctx, PMEMORY_BLOCK mem, PNV_VOID id)
{
	struct nve_softc *sc = ctx;
	struct nve_rx_desc *desc;
	struct nve_map_buffer *buf;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_osfreerxbuf\n");

	desc = (struct nve_rx_desc *) id;
	buf = &desc->buf;

	if (buf->mbuf) {
		bus_dmamap_unload(sc->mtag, buf->map);
		bus_dmamap_destroy(sc->mtag, buf->map);
		m_freem(buf->mbuf);
	}
	sc->pending_rxs--;
	buf->mbuf = NULL;

	NVE_UNLOCK(sc);

	return (1);
}

/* This gets called by the Nvidia API after our TX packet has been sent */
static NV_SINT32
nve_ospackettx(PNV_VOID ctx, PNV_VOID id, NV_UINT32 success)
{
	struct nve_softc *sc = ctx;
	struct nve_map_buffer *buf;
	struct nve_tx_desc *desc = (struct nve_tx_desc *) id;
	struct ifnet *ifp;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_ospackettx\n");

	ifp = sc->ifp;
	buf = &desc->buf;
	sc->pending_txs--;

	/* Unload and free mbuf cluster */
	if (buf->mbuf == NULL)
		goto fail;

	bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->mtag, buf->map);
	m_freem(buf->mbuf);
	buf->mbuf = NULL;

	/* Send more packets if we have them */
	if (sc->pending_txs < TX_RING_SIZE)
		sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (ifp->if_snd.ifq_head != NULL && sc->pending_txs < TX_RING_SIZE)
		nve_ifstart(ifp);

fail:
	NVE_UNLOCK(sc);

	return (1);
}

/* This gets called by the Nvidia API when a new packet has been received */
/* XXX What is newbuf used for? XXX */
static NV_SINT32
nve_ospacketrx(PNV_VOID ctx, PNV_VOID data, NV_UINT32 success, NV_UINT8 *newbuf,
    NV_UINT8 priority)
{
	struct nve_softc *sc = ctx;
	struct ifnet *ifp;
	struct nve_rx_desc *desc;
	struct nve_map_buffer *buf;
	ADAPTER_READ_DATA *readdata;

	NVE_LOCK(sc);

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_ospacketrx\n");

	ifp = sc->ifp;

	readdata = (ADAPTER_READ_DATA *) data;
	desc = readdata->pvID;
	buf = &desc->buf;
	bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_POSTREAD);

	if (success) {
		/* Sync DMA bounce buffer. */
		bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_POSTREAD);

		/* First mbuf in packet holds the ethernet and packet headers */
		buf->mbuf->m_pkthdr.rcvif = ifp;
		buf->mbuf->m_pkthdr.len = buf->mbuf->m_len =
		    readdata->ulTotalLength;

		bus_dmamap_unload(sc->mtag, buf->map);

		/* Give mbuf to OS. */
		(*ifp->if_input) (ifp, buf->mbuf);
		if (readdata->ulFilterMatch & ADREADFL_MULTICAST_MATCH)
			ifp->if_imcasts++;

		/* Blat the mbuf pointer, kernel will free the mbuf cluster */
		buf->mbuf = NULL;
	} else {
		bus_dmamap_sync(sc->mtag, buf->map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mtag, buf->map);
		m_freem(buf->mbuf);
		buf->mbuf = NULL;
	}

	sc->cur_rx = desc - sc->rx_desc;
	sc->pending_rxs--;

	NVE_UNLOCK(sc);

	return (1);
}

/* This gets called by NVIDIA API when the PHY link state changes */
static NV_SINT32
nve_oslinkchg(PNV_VOID ctx, NV_SINT32 enabled)
{
	struct nve_softc *sc = (struct nve_softc *)ctx;
	struct ifnet *ifp;

	DEBUGOUT(NVE_DEBUG_API, "nve: nve_oslinkchg\n");

	ifp = sc->ifp;

	if (enabled)
		ifp->if_flags |= IFF_UP;
	else
		ifp->if_flags &= ~IFF_UP;

	return (1);
}

/* Setup a watchdog timer */
static NV_SINT32
nve_osalloctimer(PNV_VOID ctx, PNV_VOID *timer)
{
	struct nve_softc *sc = (struct nve_softc *)ctx;

	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_osalloctimer\n");

	callout_handle_init(&sc->ostimer);
	*timer = &sc->ostimer;

	return (1);
}

/* Free the timer */
static NV_SINT32
nve_osfreetimer(PNV_VOID ctx, PNV_VOID timer)
{

	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_osfreetimer\n");

	return (1);
}

/* Setup timer parameters */
static NV_SINT32
nve_osinittimer(PNV_VOID ctx, PNV_VOID timer, PTIMER_FUNC func, PNV_VOID parameters)
{
	struct nve_softc *sc = (struct nve_softc *)ctx;

	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_osinittimer\n");

	sc->ostimer_func = func;
	sc->ostimer_params = parameters;

	return (1);
}

/* Set the timer to go off */
static NV_SINT32
nve_ossettimer(PNV_VOID ctx, PNV_VOID timer, NV_UINT32 delay)
{
	struct nve_softc *sc = ctx;

	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_ossettimer\n");

	*(struct callout_handle *)timer = timeout(sc->ostimer_func,
	    sc->ostimer_params, delay);

	return (1);
}

/* Cancel the timer */
static NV_SINT32
nve_oscanceltimer(PNV_VOID ctx, PNV_VOID timer)
{
	struct nve_softc *sc = ctx;

	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_oscanceltimer\n");

	untimeout(sc->ostimer_func, sc->ostimer_params,
	    *(struct callout_handle *)timer);

	return (1);
}

static NV_SINT32
nve_ospreprocpkt(PNV_VOID ctx, PNV_VOID readdata, PNV_VOID *id,
    NV_UINT8 *newbuffer, NV_UINT8 priority)
{

	/* Not implemented */
	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_ospreprocpkt\n");

	return (1);
}

static PNV_VOID
nve_ospreprocpktnopq(PNV_VOID ctx, PNV_VOID readdata)
{

	/* Not implemented */
	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_ospreprocpkt\n");

	return (NULL);
}

static NV_SINT32
nve_osindicatepkt(PNV_VOID ctx, PNV_VOID *id, NV_UINT32 pktno)
{

	/* Not implemented */
	DEBUGOUT(NVE_DEBUG_BROKEN, "nve: nve_osindicatepkt\n");

	return (1);
}

/* Allocate mutex context (already done in nve_attach) */
static NV_SINT32
nve_oslockalloc(PNV_VOID ctx, NV_SINT32 type, PNV_VOID *pLock)
{
	struct nve_softc *sc = (struct nve_softc *)ctx;

	DEBUGOUT(NVE_DEBUG_LOCK, "nve: nve_oslockalloc\n");

	*pLock = (void **)sc;

	return (1);
}

/* Obtain a spin lock */
static NV_SINT32
nve_oslockacquire(PNV_VOID ctx, NV_SINT32 type, PNV_VOID lock)
{

	DEBUGOUT(NVE_DEBUG_LOCK, "nve: nve_oslockacquire\n");

	NVE_OSLOCK((struct nve_softc *)lock);

	return (1);
}

/* Release lock */
static NV_SINT32
nve_oslockrelease(PNV_VOID ctx, NV_SINT32 type, PNV_VOID lock)
{

	DEBUGOUT(NVE_DEBUG_LOCK, "nve: nve_oslockrelease\n");

	NVE_OSUNLOCK((struct nve_softc *)lock);

	return (1);
}

/* I have no idea what this is for */
static PNV_VOID
nve_osreturnbufvirt(PNV_VOID ctx, PNV_VOID readdata)
{

	/* Not implemented */
	DEBUGOUT(NVE_DEBUG_LOCK, "nve: nve_osreturnbufvirt\n");
	panic("nve: nve_osreturnbufvirtual not implemented\n");

	return (NULL);
}

/* --- End on NVOSAPI interface --- */
