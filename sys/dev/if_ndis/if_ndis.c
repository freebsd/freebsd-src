/*
 * Copyright (c) 2003
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

#include "opt_bdg.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

#include "ndis_driver_data.h"

MODULE_DEPEND(ndis, pci, 1, 1, 1);
MODULE_DEPEND(ndis, ether, 1, 1, 1);
MODULE_DEPEND(ndis, ndisapi, 1, 1, 1);

/*
 * Various supported device vendors/types and their names.
 * These are defined in the ndis_driver_data.h file.
 */
static struct ndis_type ndis_devs[] = {
#ifdef NDIS_DEV_TABLE
	NDIS_DEV_TABLE
#endif
	{ 0, 0, 0, NULL }
};

#define __stdcall __attribute__((__stdcall__))

static int ndis_probe		(device_t);
static int ndis_attach		(device_t);
static int ndis_detach		(device_t);

static __stdcall void ndis_txeof	(ndis_handle,
	ndis_packet *, ndis_status);
static __stdcall void ndis_rxeof	(ndis_handle,
	ndis_packet **, uint32_t);
static void ndis_intr		(void *);
static void ndis_tick		(void *);
static void ndis_start		(struct ifnet *);
static int ndis_ioctl		(struct ifnet *, u_long, caddr_t);
static void ndis_init		(void *);
static void ndis_stop		(struct ndis_softc *);
static void ndis_watchdog	(struct ifnet *);
static void ndis_shutdown	(device_t);
static int ndis_ifmedia_upd	(struct ifnet *);
static void ndis_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static void ndis_reset		(struct ndis_softc *);
static void ndis_setmulti	(struct ndis_softc *);
static void ndis_map_sclist	(void *, bus_dma_segment_t *,
	int, bus_size_t, int);

#ifdef NDIS_USEIOSPACE
#define NDIS_RES			SYS_RES_IOPORT
#define NDIS_RID			NDIS_PCI_LOIO
#else
#define NDIS_RES			SYS_RES_MEMORY
#define NDIS_RID			NDIS_PCI_LOMEM
#endif

static device_method_t ndis_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ndis_probe),
	DEVMETHOD(device_attach,	ndis_attach),
	DEVMETHOD(device_detach,	ndis_detach),
	DEVMETHOD(device_shutdown,	ndis_shutdown),

	{ 0, 0 }
};

static driver_t ndis_driver = {
	"ndis",
	ndis_methods,
	sizeof(struct ndis_softc)
};

static devclass_t ndis_devclass;

DRIVER_MODULE(ndis, pci, ndis_driver, ndis_devclass, 0, 0);

/*
 * Program the 64-bit multicast hash filter.
 */
static void
ndis_setmulti(sc)
	struct ndis_softc	*sc;
{
#ifdef notyet
	uint32_t		ndis_filter;
	int			len;
#endif
	return;
}

static void
ndis_reset(sc)
	struct ndis_softc	*sc;
{
	ndis_reset_nic(sc);
        return;
}

/*
 * Probe for an NDIS device. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
ndis_probe(dev)
	device_t		dev;
{
	struct ndis_type		*t;

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did) &&
		    ((pci_read_config(dev, PCIR_SUBVEND_0, 4) ==
		    t->ndis_subsys) || t->ndis_subsys == 0)) {
			device_set_desc(dev, t->ndis_name);
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
static int
ndis_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ndis_softc		*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid, len;
	void			*img;
	struct ndis_type	*t;
	int			devidx = 0, defidx = 0;


	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->ndis_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	/* Try to map iospace */

	sc->ndis_io_rid = NDIS_PCI_LOIO;
	sc->ndis_res_io = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->ndis_io_rid, 0, ~0, 1, RF_ACTIVE);

	/*
	 * Sometimes the iospace and memspace BARs are swapped.
	 * Make one more try to map I/O space using a different
	 * RID.
	 */
	if (sc->ndis_res_io == NULL) {
		sc->ndis_io_rid = NDIS_PCI_LOMEM;
		sc->ndis_res_io = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->ndis_io_rid, 0, ~0, 1, RF_ACTIVE);
	}

	if (sc->ndis_res_io != NULL)
		sc->ndis_rescnt++;

	/* Now try to mem memory space */
	sc->ndis_mem_rid = NDIS_PCI_LOMEM;
	sc->ndis_res_mem = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &sc->ndis_mem_rid, 0, ~0, 1, RF_ACTIVE);

	/*
	 * If the first attempt fails, try again with another
	 * BAR.
	 */
	if (sc->ndis_res_mem == NULL) {
		sc->ndis_mem_rid = NDIS_PCI_LOIO;
		sc->ndis_res_mem = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &sc->ndis_mem_rid, 0, ~0, 1, RF_ACTIVE);
	}

	if (sc->ndis_res_mem != NULL)
		sc->ndis_rescnt++;

	if (!sc->ndis_rescnt) {
		printf("ndis%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}
#ifdef notdef
	sc->ndis_btag = rman_get_bustag(sc->ndis_res);
	sc->ndis_bhandle = rman_get_bushandle(sc->ndis_res);
#endif

	/* Allocate interrupt */
	rid = 0;
	sc->ndis_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->ndis_irq == NULL) {
		printf("ndis%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->ndis_rescnt++;

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define NDIS_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
                        BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, NDIS_NSEG_NEW,/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,       /* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->ndis_parent_tag);

        if (error)
                goto fail;

	img = drv_data;
	sc->ndis_dev = dev;
	sc->ndis_regvals = ndis_regvals;
	sc->ndis_iftype = PCIBus;

	/* Figure out exactly which device we matched. */

	t = ndis_devs;

	while(t->ndis_name != NULL) {
		if ((pci_get_vendor(dev) == t->ndis_vid) &&
		    (pci_get_device(dev) == t->ndis_did)) {
			if (t->ndis_subsys == 0)
				defidx = devidx;
			else {
				if (t->ndis_subsys ==
				    pci_read_config(dev, PCIR_SUBVEND_0, 4))
					break;
			}
		}
		t++;
		devidx++;
	}

	if (ndis_devs[devidx].ndis_name == NULL)
		sc->ndis_devidx = defidx;
	else
		sc->ndis_devidx = devidx;

	sysctl_ctx_init(&sc->ndis_ctx);

	/* Create sysctl registry nodes */
	ndis_create_sysctls(sc);

	/* Set up driver image in memory. */
	ndis_load_driver((vm_offset_t)img, sc);

	/* Do resource conversion. */
	ndis_convert_res(sc);

	/* Install our RX and TX interrupt handlers. */
	sc->ndis_block.nmb_senddone_func = ndis_txeof;
	sc->ndis_block.nmb_pktind_func = ndis_rxeof;

	/* Call driver's init routine. */
	if (ndis_init_nic(sc)) {
		printf ("ndis%d: init handler failed\n", sc->ndis_unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	ndis_reset(sc);

	/*
	 * Get station address from the driver.
	 */
	len = sizeof(eaddr);
	ndis_get_info(sc, OID_802_3_CURRENT_ADDRESS, &eaddr, &len);

	/*
	 * An NDIS device was detected. Inform the world.
	 */
	printf("ndis%d: Ethernet address: %6D\n", unit, eaddr, ":");

	sc->ndis_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/*
	 * Figure out of we're allowed to use multipacket sends
	 * with this driver, and if so, how many.
	 */

	if (sc->ndis_chars.nmc_sendsingle_func)
		sc->ndis_maxpkts = 1;
	else {
		len = sizeof(sc->ndis_maxpkts);
		ndis_get_info(sc, OID_GEN_MAXIMUM_SEND_PACKETS,
		    &sc->ndis_maxpkts, &len);
		sc->ndis_txarray = malloc(sizeof(ndis_packet *) *
		    sc->ndis_maxpkts, M_DEVBUF, M_NOWAIT);
		bzero((char *)sc->ndis_txarray, sizeof(ndis_packet *) *
		    sc->ndis_maxpkts);
	}

	sc->ndis_txpending = sc->ndis_maxpkts;

	sc->ndis_oidcnt = 0;
	/* Get supported oid list. */
	ndis_get_supported_oids(sc, &sc->ndis_oids, &sc->ndis_oidcnt);

	/* If the NDIS module requested scatter/gather, init maps. */
	if (sc->ndis_sc)
		ndis_init_dma(sc);

	ifmedia_init(&sc->ifmedia, IFM_IMASK, ndis_ifmedia_upd,
	    ndis_ifmedia_sts);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
	ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ndis_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = ndis_start;
	ifp->if_watchdog = ndis_watchdog;
	ifp->if_init = ndis_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = 50;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->ndis_irq, INTR_TYPE_NET,
	    ndis_intr, sc, &sc->ndis_intrhand);

	if (error) {
		printf("ndis%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		ndis_detach(dev);

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
ndis_detach(dev)
	device_t		dev;
{
	struct ndis_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->ndis_mtx), ("ndis mutex not initialized"));
	NDIS_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	if (device_is_attached(dev)) {
		NDIS_UNLOCK(sc);
		ndis_stop(sc);
		ether_ifdetach(ifp);
		NDIS_LOCK(sc);
	}

	bus_generic_detach(dev);

	if (sc->ndis_intrhand)
		bus_teardown_intr(dev, sc->ndis_irq, sc->ndis_intrhand);
	if (sc->ndis_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->ndis_irq);
	if (sc->ndis_res_io)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->ndis_io_rid, sc->ndis_res_io);
	if (sc->ndis_res_mem)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->ndis_mem_rid, sc->ndis_res_mem);

	if (sc->ndis_sc)
		ndis_destroy_dma(sc);

	ndis_unload_driver((void *)ifp);

	bus_dma_tag_destroy(sc->ndis_parent_tag);

	sysctl_ctx_free(&sc->ndis_ctx);

	NDIS_UNLOCK(sc);
	mtx_destroy(&sc->ndis_mtx);

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
__stdcall static void
ndis_rxeof(adapter, packets, pktcnt)
	ndis_handle		adapter;
	ndis_packet		**packets;
	uint32_t		pktcnt;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	ndis_packet		*p;
	struct ifnet		*ifp;
	struct mbuf		*m0;
	int			i;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)(block->nmb_ifp);
	ifp = block->nmb_ifp;

	for (i = 0; i < pktcnt; i++) {
		p = packets[i];
		/* Stash the softc here so ptom can use it. */
		p->np_rsvd[0] = (uint32_t *)sc;
		if (ndis_ptom(&m0, p)) {
			printf ("ndis%d: ptom failed\n", sc->ndis_unit);
			ndis_return_packet(sc, p);
		} else {
			m0->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			(*ifp->if_input)(ifp, m0);
		}
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
__stdcall static void
ndis_txeof(adapter, packet, status)
	ndis_handle		adapter;
	ndis_packet		*packet;
	ndis_status		status;

{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	struct ifnet		*ifp;
	int			idx;
	struct mbuf		*m;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;
	ifp = block->nmb_ifp;

	if (packet->np_rsvd[1] == NULL)
		panic("NDIS driver corrupted reserved packet fields");

	NDIS_LOCK(sc);

	m = (struct mbuf *)packet->np_rsvd[1];
	idx = (int)packet->np_rsvd[0];
	ifp->if_opackets++;
	m_freem(m);
	if (sc->ndis_sc)
		bus_dmamap_unload(sc->ndis_ttag, sc->ndis_tmaps[idx]);

	ndis_free_packet(packet);
	sc->ndis_txarray[idx] = NULL;
	sc->ndis_txpending++;

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	NDIS_UNLOCK(sc);

	if (ifp->if_snd.ifq_head != NULL)
		ndis_start(ifp);

	return;
}

static void
ndis_intr(arg)
	void			*arg;
{
	struct ndis_softc		*sc;
	struct ifnet		*ifp;
	int			is_our_intr = 0;
	int			call_isr = 0;

	sc = arg;
	/*NDIS_LOCK(sc);*/
	ifp = &sc->arpcom.ac_if;

/*
	if (!(ifp->if_flags & IFF_UP)) {
		NDIS_UNLOCK(sc);
		return;
	}
*/
	ndis_isr(sc, &is_our_intr, &call_isr);

	if (is_our_intr || call_isr)
		ndis_intrhand(sc);

	if (ifp->if_snd.ifq_head != NULL)
		ndis_start(ifp);

	/*NDIS_UNLOCK(sc);*/

	return;
}

static void
ndis_tick(xsc)
	void			*xsc;
{
	struct ndis_softc		*sc;
	__stdcall ndis_checkforhang_handler hangfunc;
	uint8_t			rval;

	sc = xsc;
	NDIS_LOCK(sc);

	hangfunc = sc->ndis_chars.nmc_checkhang_func;

	if (hangfunc != NULL) {
		rval = hangfunc(sc->ndis_block.nmb_miniportadapterctx);
		if (rval == TRUE)
			ndis_reset_nic(sc);
	}

	sc->ndis_stat_ch = timeout(ndis_tick, sc, hz *
	    sc->ndis_block.nmb_checkforhangsecs);

	NDIS_UNLOCK(sc);

	return;
}

static void
ndis_map_sclist(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;

{
	struct ndis_sc_list	*sclist;
	int			i;

	if (error || arg == NULL)
		return;

	sclist = arg;

	sclist->nsl_frags = nseg;

	for (i = 0; i < nseg; i++) {
		sclist->nsl_elements[i].nse_addr.np_quad = segs[i].ds_addr;
		sclist->nsl_elements[i].nse_len = segs[i].ds_len;
	}

	return;
}

/*
 * Main transmit routine. To make NDIS drivers happy, we need to
 * transform mbuf chains into NDIS packets and feed them to the
 * send packet routines. Most drivers allow you to send several
 * packets at once (up to the maxpkts limit). Unfortunately, rather
 * that accepting them in the form of a linked list, they expect
 * a contiguous array of pointers to packets.
 *
 * For those drivers which use the NDIS scatter/gather DMA mechanism,
 * we need to perform busdma work here. Those that use map registers
 * will do the mapping themselves on a buffer by buffer basis.
 */

static void
ndis_start(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc	*sc;
	struct mbuf		*m = NULL;
	ndis_packet		**p0 = NULL, *p = NULL;
	int			pcnt = 0;

	sc = ifp->if_softc;

	NDIS_LOCK(sc);

	p0 = &sc->ndis_txarray[sc->ndis_txidx];

	while(sc->ndis_txpending) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		sc->ndis_txarray[sc->ndis_txidx] = NULL;

		if (ndis_mtop(m, &sc->ndis_txarray[sc->ndis_txidx])) {
			NDIS_UNLOCK(sc);
			IF_PREPEND(&ifp->if_snd, m);
			return;
		}

		/*
		 * Save pointer to original mbuf
		 * so we can free it later.
		 */

		(sc->ndis_txarray[sc->ndis_txidx])->np_rsvd[0] =
		    (uint32_t *)sc->ndis_txidx;
		(sc->ndis_txarray[sc->ndis_txidx])->np_rsvd[1] = (uint32_t *)m;

		/*
		 * Do scatter/gather processing, if driver requested it.
		 */
		if (sc->ndis_sc) {
			p = sc->ndis_txarray[sc->ndis_txidx];
			bus_dmamap_load_mbuf(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx], m,
			    ndis_map_sclist, &p->np_sclist, BUS_DMA_NOWAIT);
			bus_dmamap_sync(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx],
			    BUS_DMASYNC_PREREAD);
			p->np_ext.npe_info[ndis_sclist_info] = &p->np_sclist;
		}

		NDIS_INC(sc);
		sc->ndis_txpending--;

		pcnt++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */

		BPF_MTAP(ifp, m);

		/*
		 * The array that p0 points to must appear contiguous,
		 * so we must not wrap past the end of sc->ndis_txarray[].
		 * If it looks like we're about to wrap, break out here
		 * so the this batch of packets can be transmitted, then
		 * wait for txeof to ask us to send the rest.
		 */

		if (sc->ndis_txidx == 0)
			break;
	}

	if (sc->ndis_txpending == 0)
		ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	NDIS_UNLOCK(sc);

	ndis_send_packets(sc, p0, pcnt);

	return;
}

static void
ndis_init(xsc)
	void			*xsc;
{
	struct ndis_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			i, error;
	uint32_t		ndis_filter = 0;

	/*NDIS_LOCK(sc);*/

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	ndis_reset(sc);
	ndis_stop(sc);
	ndis_init_nic(sc);

	/* Init our MAC address */
#ifdef notdef
	/*
	 * Program the multicast filter, if necessary.
	 */
	ndis_setmulti(sc);
#endif

	/* Program the packet filter */

	ndis_filter = NDIS_PACKET_TYPE_DIRECTED;

	if (ifp->if_flags & IFF_BROADCAST)
		ndis_filter |= NDIS_PACKET_TYPE_BROADCAST;

	if (ifp->if_flags & IFF_PROMISC)
		ndis_filter |= NDIS_PACKET_TYPE_PROMISCUOUS;

	if (ifp->if_flags & IFF_MULTICAST)
		ndis_filter |= NDIS_PACKET_TYPE_MULTICAST;

	i = sizeof(ndis_filter);

	error = ndis_set_info(sc, OID_GEN_CURRENT_PACKET_FILTER,
	    &ndis_filter, &i);

	if (error)
		printf ("set filter failed: %d\n", error);

	sc->ndis_txidx = 0;
	sc->ndis_txpending = sc->ndis_maxpkts;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->ndis_chars.nmc_checkhang_func != NULL)
		sc->ndis_stat_ch = timeout(ndis_tick, sc,
		    hz * sc->ndis_block.nmb_checkforhangsecs);

	/*NDIS_UNLOCK(sc);*/

	return;
}

/*
 * Set media options.
 */
static int
ndis_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc		*sc;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP)
		ndis_init(sc);

	return(0);
}

/*
 * Report current media status.
 */
static void
ndis_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct ndis_softc	*sc;
	uint32_t		media_info;
	ndis_media_state	linkstate;
	int			error, len;

	sc = ifp->if_softc;

	len = sizeof(linkstate);
	error = ndis_get_info(sc, OID_GEN_MEDIA_CONNECT_STATUS,
	    (void *)&linkstate, &len);

	len = sizeof(media_info);
	error = ndis_get_info(sc, OID_GEN_LINK_SPEED,
	    (void *)&media_info, &len);

        ifmr->ifm_status = IFM_AVALID;
        ifmr->ifm_active = IFM_ETHER;

	if (linkstate == nmc_connected)
		ifmr->ifm_status |= IFM_ACTIVE;

	switch(media_info) {
	case 100000:
		ifmr->ifm_active |= IFM_10_T;
		break;
	case 1000000:
		ifmr->ifm_active |= IFM_100_TX;
		break;
	case 10000000:
		ifmr->ifm_active |= IFM_1000_T;
		break;
	default:
		printf("ndis%d: unknown speed: %d\n",
		    sc->ndis_unit, media_info);
		break;
	}

	return;
}

static int
ndis_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct ndis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			error = 0;

	/*NDIS_LOCK(sc);*/

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			ndis_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ndis_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ndis_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	/*NDIS_UNLOCK(sc);*/

	return(error);
}

static void
ndis_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct ndis_softc		*sc;

	sc = ifp->if_softc;

	NDIS_LOCK(sc);
	ifp->if_oerrors++;
	printf("ndis%d: watchdog timeout\n", sc->ndis_unit);

	ndis_reset(sc);

	if (ifp->if_snd.ifq_head != NULL)
		ndis_start(ifp);
	NDIS_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
ndis_stop(sc)
	struct ndis_softc		*sc;
{
	struct ifnet		*ifp;

/*	NDIS_LOCK(sc);*/
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	untimeout(ndis_tick, sc, sc->ndis_stat_ch);

	ndis_halt_nic(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	/*NDIS_UNLOCK(sc);*/

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
ndis_shutdown(dev)
	device_t		dev;
{
	struct ndis_softc		*sc;

	sc = device_get_softc(dev);
	ndis_shutdown_nic(sc);

	return;
}
