/*	$NetBSD: pdq_ifsubr.c,v 1.38 2001/12/21 23:21:47 matt Exp $	*/

/*-
 * Copyright (c) 1995, 1996 Matt Thomas <matt@3am-software.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: pdq_ifsubr.c,v 1.12 1997/06/05 01:56:35 thomas Exp$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 *	This module provide bus independent BSD specific O/S functions.
 *	(ie. it provides an ifnet interface to the rest of the system)
 */


#define PDQ_OSSUPPORT

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h> 
#include <net/if_types.h> 
#include <net/fddi.h>

#include <net/bpf.h>

#include <dev/pdq/pdq_freebsd.h>
#include <dev/pdq/pdqreg.h>

devclass_t pdq_devclass;

static void
pdq_ifinit(
    pdq_softc_t *sc)
{
    if (PDQ_IFNET(sc)->if_flags & IFF_UP) {
	PDQ_IFNET(sc)->if_flags |= IFF_RUNNING;
	if (PDQ_IFNET(sc)->if_flags & IFF_PROMISC) {
	    sc->sc_pdq->pdq_flags |= PDQ_PROMISC;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PROMISC;
	}
	if (PDQ_IFNET(sc)->if_flags & IFF_LINK1) {
	    sc->sc_pdq->pdq_flags |= PDQ_PASS_SMT;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PASS_SMT;
	}
	sc->sc_pdq->pdq_flags |= PDQ_RUNNING;
	pdq_run(sc->sc_pdq);
    } else {
	PDQ_IFNET(sc)->if_flags &= ~IFF_RUNNING;
	sc->sc_pdq->pdq_flags &= ~PDQ_RUNNING;
	pdq_stop(sc->sc_pdq);
    }
}

static void
pdq_ifwatchdog(
    struct ifnet *ifp)
{
    /*
     * No progress was made on the transmit queue for PDQ_OS_TX_TRANSMIT
     * seconds.  Remove all queued packets.
     */

    ifp->if_flags &= ~IFF_OACTIVE;
    ifp->if_timer = 0;
    for (;;) {
	struct mbuf *m;
	IFQ_DEQUEUE(&ifp->if_snd, m);
	if (m == NULL)
	    return;
	PDQ_OS_DATABUF_FREE(PDQ_OS_IFP_TO_SOFTC(ifp)->sc_pdq, m);
    }
}

static void
pdq_ifstart(
    struct ifnet *ifp)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);
    struct mbuf *m;
    int tx = 0;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    if (PDQ_IFNET(sc)->if_timer == 0)
	PDQ_IFNET(sc)->if_timer = PDQ_OS_TX_TIMEOUT;

    if ((sc->sc_pdq->pdq_flags & PDQ_TXOK) == 0) {
	PDQ_IFNET(sc)->if_flags |= IFF_OACTIVE;
	return;
    }
    sc->sc_flags |= PDQIF_DOWNCALL;
    for (;; tx = 1) {
	IF_DEQUEUE(&ifp->if_snd, m);
	if (m == NULL)
	    break;
#if defined(PDQ_BUS_DMA) && !defined(PDQ_BUS_DMA_NOTX)
	if ((m->m_flags & M_HASTXDMAMAP) == 0) {
	    bus_dmamap_t map;
	    if (PDQ_OS_HDR_OFFSET != PDQ_RX_FC_OFFSET) {
		m->m_data[0] = PDQ_FDDI_PH0;
		m->m_data[1] = PDQ_FDDI_PH1;
		m->m_data[2] = PDQ_FDDI_PH2;
	    }
	    if (!bus_dmamap_create(sc->sc_dmatag, m->m_pkthdr.len, 255,
				   m->m_pkthdr.len, 0, BUS_DMA_NOWAIT, &map)) {
		if (!bus_dmamap_load_mbuf(sc->sc_dmatag, map, m,
					  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) {
		    bus_dmamap_sync(sc->sc_dmatag, map, 0, m->m_pkthdr.len,
				    BUS_DMASYNC_PREWRITE);
		    M_SETCTX(m, map);
		    m->m_flags |= M_HASTXDMAMAP;
		}
	    }
	    if ((m->m_flags & M_HASTXDMAMAP) == 0)
		break;
	}
#else
	if (PDQ_OS_HDR_OFFSET != PDQ_RX_FC_OFFSET) {
	    m->m_data[0] = PDQ_FDDI_PH0;
	    m->m_data[1] = PDQ_FDDI_PH1;
	    m->m_data[2] = PDQ_FDDI_PH2;
	}
#endif

	if (pdq_queue_transmit_data(sc->sc_pdq, m) == PDQ_FALSE)
	    break;
    }
    if (m != NULL) {
	ifp->if_flags |= IFF_OACTIVE;
	IF_PREPEND(&ifp->if_snd, m);
    }
    if (tx)
	PDQ_DO_TYPE2_PRODUCER(sc->sc_pdq);
    sc->sc_flags &= ~PDQIF_DOWNCALL;
}

void
pdq_os_receive_pdu(
    pdq_t *pdq,
    struct mbuf *m,
    size_t pktlen,
    int drop)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    struct ifnet *ifp = PDQ_IFNET(sc);
    struct fddi_header *fh;

    ifp->if_ipackets++;
#if defined(PDQ_BUS_DMA)
    {
	/*
	 * Even though the first mbuf start at the first fddi header octet,
	 * the dmamap starts PDQ_OS_HDR_OFFSET octets earlier.  Any additional
	 * mbufs will start normally.
	 */
	int offset = PDQ_OS_HDR_OFFSET;
	struct mbuf *m0;
	for (m0 = m; m0 != NULL; m0 = m0->m_next, offset = 0) {
	    pdq_os_databuf_sync(sc, m0, offset, m0->m_len, BUS_DMASYNC_POSTREAD);
	    bus_dmamap_unload(sc->sc_dmatag, M_GETCTX(m0, bus_dmamap_t));
	    bus_dmamap_destroy(sc->sc_dmatag, M_GETCTX(m0, bus_dmamap_t));
	    m0->m_flags &= ~M_HASRXDMAMAP;
	    M_SETCTX(m0, NULL);
	}
    }
#endif
    m->m_pkthdr.len = pktlen;
    fh = mtod(m, struct fddi_header *);
    if (drop || (fh->fddi_fc & (FDDIFC_L|FDDIFC_F)) != FDDIFC_LLC_ASYNC) {
	ifp->if_iqdrops++;
	ifp->if_ierrors++;
	PDQ_OS_DATABUF_FREE(pdq, m);
	return;
    }

    m->m_pkthdr.rcvif = ifp;
    (*ifp->if_input)(ifp, m);
}

void
pdq_os_restart_transmitter(
    pdq_t *pdq)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    PDQ_IFNET(sc)->if_flags &= ~IFF_OACTIVE;
    if (IFQ_IS_EMPTY(&PDQ_IFNET(sc)->if_snd) == 0) {
	PDQ_IFNET(sc)->if_timer = PDQ_OS_TX_TIMEOUT;
	if ((sc->sc_flags & PDQIF_DOWNCALL) == 0)
	    pdq_ifstart(PDQ_IFNET(sc));
    } else {
	PDQ_IFNET(sc)->if_timer = 0;
    }
}

void
pdq_os_transmit_done(
    pdq_t *pdq,
    struct mbuf *m)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
#if NBPFILTER > 0
    if (PQD_IFNET(sc)->if_bpf != NULL)
	PDQ_BPF_MTAP(sc, m);
#endif
    PDQ_OS_DATABUF_FREE(pdq, m);
    PDQ_IFNET(sc)->if_opackets++;
}

void
pdq_os_addr_fill(
    pdq_t *pdq,
    pdq_lanaddr_t *addr,
    size_t num_addrs)
{
    pdq_softc_t *sc = pdq->pdq_os_ctx;
    struct ifnet *ifp;
    struct ifmultiaddr *ifma;

    ifp = sc->ifp;

    /*
     * ADDR_FILTER_SET is always issued before FILTER_SET so
     * we can play with PDQ_ALLMULTI and not worry about 
     * queueing a FILTER_SET ourselves.
     */

    pdq->pdq_flags &= ~PDQ_ALLMULTI;
#if defined(IFF_ALLMULTI)
    PDQ_IFNET(sc)->if_flags &= ~IFF_ALLMULTI;
#endif

    IF_ADDR_LOCK(PDQ_IFNET(sc));
    for (ifma = TAILQ_FIRST(&PDQ_IFNET(sc)->if_multiaddrs); ifma && num_addrs > 0;
	 ifma = TAILQ_NEXT(ifma, ifma_link)) {
	    char *mcaddr;
	    if (ifma->ifma_addr->sa_family != AF_LINK)
		    continue;
	    mcaddr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	    ((u_short *) addr->lanaddr_bytes)[0] = ((u_short *) mcaddr)[0];
	    ((u_short *) addr->lanaddr_bytes)[1] = ((u_short *) mcaddr)[1];
	    ((u_short *) addr->lanaddr_bytes)[2] = ((u_short *) mcaddr)[2];
	    addr++;
	    num_addrs--;
    }
    IF_ADDR_UNLOCK(PDQ_IFNET(sc));
    /*
     * If not all the address fit into the CAM, turn on all-multicast mode.
     */
    if (ifma != NULL) {
	pdq->pdq_flags |= PDQ_ALLMULTI;
#if defined(IFF_ALLMULTI)
	PDQ_IFNET(sc)->if_flags |= IFF_ALLMULTI;
#endif
    }
}

#if defined(IFM_FDDI)
static int
pdq_ifmedia_change(
    struct ifnet *ifp)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);

    if (sc->sc_ifmedia.ifm_media & IFM_FDX) {
	if ((sc->sc_pdq->pdq_flags & PDQ_WANT_FDX) == 0) {
	    sc->sc_pdq->pdq_flags |= PDQ_WANT_FDX;
	    if (sc->sc_pdq->pdq_flags & PDQ_RUNNING)
		pdq_run(sc->sc_pdq);
	}
    } else if (sc->sc_pdq->pdq_flags & PDQ_WANT_FDX) {
	sc->sc_pdq->pdq_flags &= ~PDQ_WANT_FDX;
	if (sc->sc_pdq->pdq_flags & PDQ_RUNNING)
	    pdq_run(sc->sc_pdq);
    }

    return 0;
}

static void
pdq_ifmedia_status(
    struct ifnet *ifp,
    struct ifmediareq *ifmr)
{
    pdq_softc_t * const sc = PDQ_OS_IFP_TO_SOFTC(ifp);

    ifmr->ifm_status = IFM_AVALID;
    if (sc->sc_pdq->pdq_flags & PDQ_IS_ONRING)
	ifmr->ifm_status |= IFM_ACTIVE;

    ifmr->ifm_active = (ifmr->ifm_current & ~IFM_FDX);
    if (sc->sc_pdq->pdq_flags & PDQ_IS_FDX)
	ifmr->ifm_active |= IFM_FDX;
}

void
pdq_os_update_status(
    pdq_t *pdq,
    const void *arg)
{
    pdq_softc_t * const sc = pdq->pdq_os_ctx;
    const pdq_response_status_chars_get_t *rsp = arg;
    int media = 0;

    switch (rsp->status_chars_get.pmd_type[0]) {
	case PDQ_PMD_TYPE_ANSI_MUTLI_MODE:         media = IFM_FDDI_MMF; break;
	case PDQ_PMD_TYPE_ANSI_SINGLE_MODE_TYPE_1: media = IFM_FDDI_SMF; break;
	case PDQ_PMD_TYPE_ANSI_SIGNLE_MODE_TYPE_2: media = IFM_FDDI_SMF; break;
	case PDQ_PMD_TYPE_UNSHIELDED_TWISTED_PAIR: media = IFM_FDDI_UTP; break;
	default: media |= IFM_MANUAL;
    }

    if (rsp->status_chars_get.station_type == PDQ_STATION_TYPE_DAS)
	media |= IFM_FDDI_DA;

    sc->sc_ifmedia.ifm_media = media | IFM_FDDI;
}
#endif /* defined(IFM_FDDI) */

static int
pdq_ifioctl(
    struct ifnet *ifp,
    u_long cmd,
    caddr_t data)
{
    pdq_softc_t *sc = PDQ_OS_IFP_TO_SOFTC(ifp);
    int error = 0;

    PDQ_LOCK(sc);

    switch (cmd) {
	case SIOCSIFFLAGS: {
	    pdq_ifinit(sc);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    if (PDQ_IFNET(sc)->if_flags & IFF_RUNNING) {
		    pdq_run(sc->sc_pdq);
		error = 0;
	    }
	    break;
	}

#if defined(IFM_FDDI) && defined(SIOCSIFMEDIA)
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA: {
	    struct ifreq *ifr = (struct ifreq *)data;
	    error = ifmedia_ioctl(ifp, ifr, &sc->sc_ifmedia, cmd);
	    break;
	}
#endif

	default: {
	    error = fddi_ioctl(ifp, cmd, data);
	    break;
	}
    }

    PDQ_UNLOCK(sc);
    return error;
}

#ifndef IFF_NOTRAILERS
#define	IFF_NOTRAILERS	0
#endif

void
pdq_ifattach(pdq_softc_t *sc)
{
    struct ifnet *ifp;

    ifp = PDQ_IFNET(sc) = if_alloc(IFT_FDDI);
    if (ifp == NULL)
	panic("%s: can not if_alloc()", device_get_nameunit(sc->dev));

    mtx_init(&sc->mtx, device_get_nameunit(sc->dev), MTX_NETWORK_LOCK,
	MTX_DEF | MTX_RECURSE);

    ifp->if_softc = sc;
    ifp->if_init = (if_init_f_t *)pdq_ifinit;
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_NOTRAILERS|IFF_MULTICAST;

    ifp->if_watchdog = pdq_ifwatchdog;

    ifp->if_ioctl = pdq_ifioctl;
    ifp->if_start = pdq_ifstart;

#if defined(IFM_FDDI)
    {
	const int media = sc->sc_ifmedia.ifm_media;
	ifmedia_init(&sc->sc_ifmedia, IFM_FDX,
		     pdq_ifmedia_change, pdq_ifmedia_status);
	ifmedia_add(&sc->sc_ifmedia, media, 0, 0);
	ifmedia_set(&sc->sc_ifmedia, media);
    }
#endif
  
    fddi_ifattach(ifp, FDDI_BPF_SUPPORTED);
}

void
pdq_ifdetach (pdq_softc_t *sc)
{
    struct ifnet *ifp;

    ifp = sc->ifp;

    fddi_ifdetach(ifp, FDDI_BPF_SUPPORTED);
    if_free(ifp);
    pdq_stop(sc->sc_pdq);
    pdq_free(sc->dev);

    return;
}

void
pdq_free (device_t dev)
{
	pdq_softc_t *sc;

	sc = device_get_softc(dev);

	if (sc->io)
		bus_release_resource(dev, sc->io_type, sc->io_rid, sc->io);
	if (sc->mem)
		bus_release_resource(dev, sc->mem_type, sc->mem_rid, sc->mem);
	if (sc->irq_ih)
		bus_teardown_intr(dev, sc->irq, sc->irq_ih);
	if (sc->irq)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);

	/*
	 * Destroy the mutex.
	 */
	if (mtx_initialized(&sc->mtx) != 0) {
		mtx_destroy(&sc->mtx);
	}

	return;
}

#if defined(PDQ_BUS_DMA) 
int
pdq_os_memalloc_contig(
    pdq_t *pdq)
{
    pdq_softc_t * const sc = pdq->pdq_os_ctx;
    bus_dma_segment_t db_segs[1], ui_segs[1], cb_segs[1];
    int db_nsegs = 0, ui_nsegs = 0;
    int steps = 0;
    int not_ok;

    not_ok = bus_dmamem_alloc(sc->sc_dmatag,
			 sizeof(*pdq->pdq_dbp), sizeof(*pdq->pdq_dbp),
			 sizeof(*pdq->pdq_dbp), db_segs, 1, &db_nsegs,
			 BUS_DMA_NOWAIT);
    if (!not_ok) {
	steps = 1;
	not_ok = bus_dmamem_map(sc->sc_dmatag, db_segs, db_nsegs,
				sizeof(*pdq->pdq_dbp), (caddr_t *) &pdq->pdq_dbp,
				BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 2;
	not_ok = bus_dmamap_create(sc->sc_dmatag, db_segs[0].ds_len, 1,
				   0x2000, 0, BUS_DMA_NOWAIT, &sc->sc_dbmap);
    }
    if (!not_ok) {
	steps = 3;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_dbmap,
				 pdq->pdq_dbp, sizeof(*pdq->pdq_dbp),
				 NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 4;
	pdq->pdq_pa_descriptor_block = sc->sc_dbmap->dm_segs[0].ds_addr;
	not_ok = bus_dmamem_alloc(sc->sc_dmatag,
			 PDQ_OS_PAGESIZE, PDQ_OS_PAGESIZE, PDQ_OS_PAGESIZE,
			 ui_segs, 1, &ui_nsegs, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 5;
	not_ok = bus_dmamem_map(sc->sc_dmatag, ui_segs, ui_nsegs,
			    PDQ_OS_PAGESIZE,
			    (caddr_t *) &pdq->pdq_unsolicited_info.ui_events,
			    BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 6;
	not_ok = bus_dmamap_create(sc->sc_dmatag, ui_segs[0].ds_len, 1,
				   PDQ_OS_PAGESIZE, 0, BUS_DMA_NOWAIT,
				   &sc->sc_uimap);
    }
    if (!not_ok) {
	steps = 7;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_uimap,
				 pdq->pdq_unsolicited_info.ui_events,
				 PDQ_OS_PAGESIZE, NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	steps = 8;
	pdq->pdq_unsolicited_info.ui_pa_bufstart = sc->sc_uimap->dm_segs[0].ds_addr;
	cb_segs[0] = db_segs[0];
	cb_segs[0].ds_addr += offsetof(pdq_descriptor_block_t, pdqdb_consumer);
	cb_segs[0].ds_len = sizeof(pdq_consumer_block_t);
	not_ok = bus_dmamem_map(sc->sc_dmatag, cb_segs, 1,
				sizeof(*pdq->pdq_cbp), (caddr_t *) &pdq->pdq_cbp,
				BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
    }
    if (!not_ok) {
	steps = 9;
	not_ok = bus_dmamap_create(sc->sc_dmatag, cb_segs[0].ds_len, 1,
				   0x2000, 0, BUS_DMA_NOWAIT, &sc->sc_cbmap);
    }
    if (!not_ok) {
	steps = 10;
	not_ok = bus_dmamap_load(sc->sc_dmatag, sc->sc_cbmap,
				 (caddr_t) pdq->pdq_cbp, sizeof(*pdq->pdq_cbp),
				 NULL, BUS_DMA_NOWAIT);
    }
    if (!not_ok) {
	pdq->pdq_pa_consumer_block = sc->sc_cbmap->dm_segs[0].ds_addr;
	return not_ok;
    }

    switch (steps) {
	case 11: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_cbmap);
	    /* FALL THROUGH */
	}
	case 10: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_cbmap);
	    /* FALL THROUGH */
	}
	case 9: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (caddr_t) pdq->pdq_cbp, sizeof(*pdq->pdq_cbp));
	    /* FALL THROUGH */
	}
	case 8: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_uimap);
	    /* FALL THROUGH */
	}
	case 7: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_uimap);
	    /* FALL THROUGH */
	}
	case 6: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (caddr_t) pdq->pdq_unsolicited_info.ui_events,
			     PDQ_OS_PAGESIZE);
	    /* FALL THROUGH */
	}
	case 5: {
	    bus_dmamem_free(sc->sc_dmatag, ui_segs, ui_nsegs);
	    /* FALL THROUGH */
	}
	case 4: {
	    bus_dmamap_unload(sc->sc_dmatag, sc->sc_dbmap);
	    /* FALL THROUGH */
	}
	case 3: {
	    bus_dmamap_destroy(sc->sc_dmatag, sc->sc_dbmap);
	    /* FALL THROUGH */
	}
	case 2: {
	    bus_dmamem_unmap(sc->sc_dmatag,
			     (caddr_t) pdq->pdq_dbp,
			     sizeof(*pdq->pdq_dbp));
	    /* FALL THROUGH */
	}
	case 1: {
	    bus_dmamem_free(sc->sc_dmatag, db_segs, db_nsegs);
	    /* FALL THROUGH */
	}
    }

    return not_ok;
}

extern void
pdq_os_descriptor_block_sync(
    pdq_os_ctx_t *sc,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_dbmap, offset, length, ops);
}

extern void
pdq_os_consumer_block_sync(
    pdq_os_ctx_t *sc,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_cbmap, 0, sizeof(pdq_consumer_block_t), ops);
}

extern void
pdq_os_unsolicited_event_sync(
    pdq_os_ctx_t *sc,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, sc->sc_uimap, offset, length, ops);
}

extern void
pdq_os_databuf_sync(
    pdq_os_ctx_t *sc,
    struct mbuf *m,
    size_t offset,
    size_t length,
    int ops)
{
    bus_dmamap_sync(sc->sc_dmatag, M_GETCTX(m, bus_dmamap_t), offset, length, ops);
}

extern void
pdq_os_databuf_free(
    pdq_os_ctx_t *sc,
    struct mbuf *m)
{
    if (m->m_flags & (M_HASRXDMAMAP|M_HASTXDMAMAP)) {
	bus_dmamap_t map = M_GETCTX(m, bus_dmamap_t);
	bus_dmamap_unload(sc->sc_dmatag, map);
	bus_dmamap_destroy(sc->sc_dmatag, map);
	m->m_flags &= ~(M_HASRXDMAMAP|M_HASTXDMAMAP);
    }
    m_freem(m);
}

extern struct mbuf *
pdq_os_databuf_alloc(
    pdq_os_ctx_t *sc)
{
    struct mbuf *m;
    bus_dmamap_t map;

    MGETHDR(m, M_DONTWAIT, MT_DATA);
    if (m == NULL) {
	printf("%s: can't alloc small buf\n", sc->sc_dev.dv_xname);
	return NULL;
    }
    MCLGET(m, M_DONTWAIT);
    if ((m->m_flags & M_EXT) == 0) {
	printf("%s: can't alloc cluster\n", sc->sc_dev.dv_xname);
        m_free(m);
	return NULL;
    }
    m->m_pkthdr.len = m->m_len = PDQ_OS_DATABUF_SIZE;

    if (bus_dmamap_create(sc->sc_dmatag, PDQ_OS_DATABUF_SIZE,
			   1, PDQ_OS_DATABUF_SIZE, 0, BUS_DMA_NOWAIT, &map)) {
	printf("%s: can't create dmamap\n", sc->sc_dev.dv_xname);
	m_free(m);
	return NULL;
    }
    if (bus_dmamap_load_mbuf(sc->sc_dmatag, map, m,
    			     BUS_DMA_READ|BUS_DMA_NOWAIT)) {
	printf("%s: can't load dmamap\n", sc->sc_dev.dv_xname);
	bus_dmamap_destroy(sc->sc_dmatag, map);
	m_free(m);
	return NULL;
    }
    m->m_flags |= M_HASRXDMAMAP;
    M_SETCTX(m, map);
    return m;
}
#endif
