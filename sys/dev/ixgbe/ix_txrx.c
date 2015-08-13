/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD$*/


#ifndef IXGBE_STANDALONE_BUILD
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"
#endif

#include "ixgbe.h"

#ifdef	RSS
#include <net/rss_config.h>
#include <netinet/in_rss.h>
#endif

#ifdef DEV_NETMAP
#include <net/netmap.h>
#include <sys/selinfo.h>
#include <dev/netmap/netmap_kern.h>

extern int ix_crcstrip;
#endif

/*
** HW RSC control:
**  this feature only works with
**  IPv4, and only on 82599 and later.
**  Also this will cause IP forwarding to
**  fail and that can't be controlled by
**  the stack as LRO can. For all these
**  reasons I've deemed it best to leave
**  this off and not bother with a tuneable
**  interface, this would need to be compiled
**  to enable.
*/
static bool ixgbe_rsc_enable = FALSE;

#ifdef IXGBE_FDIR
/*
** For Flow Director: this is the
** number of TX packets we sample
** for the filter pool, this means
** every 20th packet will be probed.
**
** This feature can be disabled by
** setting this to 0.
*/
static int atr_sample_rate = 20;
#endif

/* Shared PCI config read/write */
inline u16
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg)
{
	u16 value;

	value = pci_read_config(((struct ixgbe_osdep *)hw->back)->dev,
	    reg, 2);

	return (value);
}

inline void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	pci_write_config(((struct ixgbe_osdep *)hw->back)->dev,
	    reg, value, 2);

	return;
}

/*********************************************************************
 *  Local Function prototypes
 *********************************************************************/
static void	ixgbe_setup_transmit_ring(struct tx_ring *);
static void     ixgbe_free_transmit_buffers(struct tx_ring *);
static int	ixgbe_setup_receive_ring(struct rx_ring *);
static void     ixgbe_free_receive_buffers(struct rx_ring *);

static void	ixgbe_rx_checksum(u32, struct mbuf *, u32);
static void	ixgbe_refresh_mbufs(struct rx_ring *, int);
static int      ixgbe_xmit(struct tx_ring *, struct mbuf **);
static int	ixgbe_tx_ctx_setup(struct tx_ring *,
		    struct mbuf *, u32 *, u32 *);
static int	ixgbe_tso_setup(struct tx_ring *,
		    struct mbuf *, u32 *, u32 *);
#ifdef IXGBE_FDIR
static void	ixgbe_atr(struct tx_ring *, struct mbuf *);
#endif
static __inline void ixgbe_rx_discard(struct rx_ring *, int);
static __inline void ixgbe_rx_input(struct rx_ring *, struct ifnet *,
		    struct mbuf *, u32);

#ifdef IXGBE_LEGACY_TX
/*********************************************************************
 *  Transmit entry point
 *
 *  ixgbe_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

void
ixgbe_start_locked(struct tx_ring *txr, struct ifnet * ifp)
{
	struct mbuf    *m_head;
	struct adapter *adapter = txr->adapter;

	IXGBE_TX_LOCK_ASSERT(txr);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		if (txr->tx_avail <= IXGBE_QUEUE_MIN_FREE)
			break;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (ixgbe_xmit(txr, &m_head)) {
			if (m_head != NULL)
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);
	}
	return;
}

/*
 * Legacy TX start - called by the stack, this
 * always uses the first tx ring, and should
 * not be used with multiqueue tx enabled.
 */
void
ixgbe_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		IXGBE_TX_LOCK(txr);
		ixgbe_start_locked(txr, ifp);
		IXGBE_TX_UNLOCK(txr);
	}
	return;
}

#else /* ! IXGBE_LEGACY_TX */

/*
** Multiqueue Transmit driver
**
*/
int
ixgbe_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	int 		i, err = 0;
#ifdef	RSS
	uint32_t bucket_id;
#endif

	/*
	 * When doing RSS, map it to the same outbound queue
	 * as the incoming flow would be mapped to.
	 *
	 * If everything is setup correctly, it should be the
	 * same bucket that the current CPU we're on is.
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
#ifdef	RSS
		if (rss_hash2bucket(m->m_pkthdr.flowid,
		    M_HASHTYPE_GET(m), &bucket_id) == 0)
			/* TODO: spit out something if bucket_id > num_queues? */
			i = bucket_id % adapter->num_queues;
		else 
#endif
			i = m->m_pkthdr.flowid % adapter->num_queues;
	} else
		i = curcpu % adapter->num_queues;

	/* Check for a hung queue and pick alternative */
	if (((1 << i) & adapter->active_queues) == 0)
		i = ffsl(adapter->active_queues);

	txr = &adapter->tx_rings[i];
	que = &adapter->queues[i];

	err = drbr_enqueue(ifp, txr->br, m);
	if (err)
		return (err);
	if (IXGBE_TX_TRYLOCK(txr)) {
		ixgbe_mq_start_locked(ifp, txr);
		IXGBE_TX_UNLOCK(txr);
	} else
		taskqueue_enqueue(que->tq, &txr->txq_task);

	return (0);
}

int
ixgbe_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr)
{
	struct adapter  *adapter = txr->adapter;
        struct mbuf     *next;
        int             enqueued = 0, err = 0;

	if (((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) ||
	    adapter->link_active == 0)
		return (ENETDOWN);

	/* Process the queue */
#if __FreeBSD_version < 901504
	next = drbr_dequeue(ifp, txr->br);
	while (next != NULL) {
		if ((err = ixgbe_xmit(txr, &next)) != 0) {
			if (next != NULL)
				err = drbr_enqueue(ifp, txr->br, next);
#else
	while ((next = drbr_peek(ifp, txr->br)) != NULL) {
		if ((err = ixgbe_xmit(txr, &next)) != 0) {
			if (next == NULL) {
				drbr_advance(ifp, txr->br);
			} else {
				drbr_putback(ifp, txr->br, next);
			}
#endif
			break;
		}
#if __FreeBSD_version >= 901504
		drbr_advance(ifp, txr->br);
#endif
		enqueued++;
#if 0 // this is VF-only
#if __FreeBSD_version >= 1100036
		/*
		 * Since we're looking at the tx ring, we can check
		 * to see if we're a VF by examing our tail register
		 * address.
		 */
		if (txr->tail < IXGBE_TDT(0) && next->m_flags & M_MCAST)
			if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);
#endif
#endif
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
#if __FreeBSD_version < 901504
		next = drbr_dequeue(ifp, txr->br);
#endif
	}

	if (txr->tx_avail < IXGBE_TX_CLEANUP_THRESHOLD)
		ixgbe_txeof(txr);

	return (err);
}

/*
 * Called from a taskqueue to drain queued transmit packets.
 */
void
ixgbe_deferred_mq_start(void *arg, int pending)
{
	struct tx_ring *txr = arg;
	struct adapter *adapter = txr->adapter;
	struct ifnet *ifp = adapter->ifp;

	IXGBE_TX_LOCK(txr);
	if (!drbr_empty(ifp, txr->br))
		ixgbe_mq_start_locked(ifp, txr);
	IXGBE_TX_UNLOCK(txr);
}

/*
 * Flush all ring buffers
 */
void
ixgbe_qflush(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	struct mbuf	*m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IXGBE_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}
#endif /* IXGBE_LEGACY_TX */


/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors, allowing the
 *  TX engine to transmit the packets. 
 *  	- return 0 on success, positive on failure
 *
 **********************************************************************/

static int
ixgbe_xmit(struct tx_ring *txr, struct mbuf **m_headp)
{
	struct adapter  *adapter = txr->adapter;
	u32		olinfo_status = 0, cmd_type_len;
	int             i, j, error, nsegs;
	int		first;
	bool		remap = TRUE;
	struct mbuf	*m_head;
	bus_dma_segment_t segs[adapter->num_segs];
	bus_dmamap_t	map;
	struct ixgbe_tx_buf *txbuf;
	union ixgbe_adv_tx_desc *txd = NULL;

	m_head = *m_headp;

	/* Basic descriptor defines */
        cmd_type_len = (IXGBE_ADVTXD_DTYP_DATA |
	    IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT);

	if (m_head->m_flags & M_VLANTAG)
        	cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail_desc;
	txbuf = &txr->tx_buffers[first];
	map = txbuf->map;

	/*
	 * Map the packet for DMA.
	 */
retry:
	error = bus_dmamap_load_mbuf_sg(txr->txtag, map,
	    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

	if (__predict_false(error)) {
		struct mbuf *m;

		switch (error) {
		case EFBIG:
			/* Try it again? - one try */
			if (remap == TRUE) {
				remap = FALSE;
				/*
				 * XXX: m_defrag will choke on
				 * non-MCLBYTES-sized clusters
				 */
				m = m_defrag(*m_headp, M_NOWAIT);
				if (m == NULL) {
					adapter->mbuf_defrag_failed++;
					m_freem(*m_headp);
					*m_headp = NULL;
					return (ENOBUFS);
				}
				*m_headp = m;
				goto retry;
			} else
				return (error);
		case ENOMEM:
			txr->no_tx_dma_setup++;
			return (error);
		default:
			txr->no_tx_dma_setup++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (error);
		}
	}

	/* Make certain there are enough descriptors */
	if (nsegs > txr->tx_avail - 2) {
		txr->no_desc_avail++;
		bus_dmamap_unload(txr->txtag, map);
		return (ENOBUFS);
	}
	m_head = *m_headp;

	/*
	 * Set up the appropriate offload context
	 * this will consume the first descriptor
	 */
	error = ixgbe_tx_ctx_setup(txr, m_head, &cmd_type_len, &olinfo_status);
	if (__predict_false(error)) {
		if (error == ENOBUFS)
			*m_headp = NULL;
		return (error);
	}

#ifdef IXGBE_FDIR
	/* Do the flow director magic */
	if ((txr->atr_sample) && (!adapter->fdir_reinit)) {
		++txr->atr_count;
		if (txr->atr_count >= atr_sample_rate) {
			ixgbe_atr(txr, m_head);
			txr->atr_count = 0;
		}
	}
#endif

	i = txr->next_avail_desc;
	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;
		bus_addr_t segaddr;

		txbuf = &txr->tx_buffers[i];
		txd = &txr->tx_base[i];
		seglen = segs[j].ds_len;
		segaddr = htole64(segs[j].ds_addr);

		txd->read.buffer_addr = segaddr;
		txd->read.cmd_type_len = htole32(txr->txd_cmd |
		    cmd_type_len |seglen);
		txd->read.olinfo_status = htole32(olinfo_status);

		if (++i == txr->num_desc)
			i = 0;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= nsegs;
	txr->next_avail_desc = i;

	txbuf->m_head = m_head;
	/*
	 * Here we swap the map so the last descriptor,
	 * which gets the completion interrupt has the
	 * real map, and the first descriptor gets the
	 * unused map from this descriptor.
	 */
	txr->tx_buffers[first].map = txbuf->map;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

        /* Set the EOP descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
	txbuf->eop = txd;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets;
	IXGBE_WRITE_REG(&adapter->hw, txr->tail, i);

	/* Mark queue as having work */
	if (txr->busy == 0)
		txr->busy = 1;

	return (0);
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/
int
ixgbe_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct ixgbe_tx_buf *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = bus_dma_tag_create(
			       bus_get_dma_tag(adapter->dev),	/* parent */
			       1, 0,		/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IXGBE_TSO_SIZE,		/* maxsize */
			       adapter->num_segs,	/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->txtag))) {
		device_printf(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct ixgbe_tx_buf *) malloc(sizeof(struct ixgbe_tx_buf) *
	    adapter->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

        /* Create the descriptor buffer dma maps */
	txbuf = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, txbuf++) {
		error = bus_dmamap_create(txr->txtag, 0, &txbuf->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
	}

	return 0;
fail:
	/* We free all, it handles case where we are in the middle */
	ixgbe_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
ixgbe_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *txbuf;
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */

	/* Clear the old ring contents */
	IXGBE_TX_LOCK(txr);
#ifdef DEV_NETMAP
	/*
	 * (under lock): if in netmap mode, do some consistency
	 * checks and set slot to entry 0 of the netmap ring.
	 */
	slot = netmap_reset(na, NR_TX, txr->me, 0);
#endif /* DEV_NETMAP */
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * adapter->num_tx_desc);
	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Free any existing tx buffers. */
        txbuf = txr->tx_buffers;
	for (int i = 0; i < txr->num_desc; i++, txbuf++) {
		if (txbuf->m_head != NULL) {
			bus_dmamap_sync(txr->txtag, txbuf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag, txbuf->map);
			m_freem(txbuf->m_head);
			txbuf->m_head = NULL;
		}
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, set the map for the packet buffer.
		 * NOTE: Some drivers (not this one) also need to set
		 * the physical buffer address in the NIC ring.
		 * Slots in the netmap ring (indexed by "si") are
		 * kring->nkr_hwofs positions "ahead" wrt the
		 * corresponding slot in the NIC ring. In some drivers
		 * (not here) nkr_hwofs can be negative. Function
		 * netmap_idx_n2k() handles wraparounds properly.
		 */
		if (slot) {
			int si = netmap_idx_n2k(&na->tx_rings[txr->me], i);
			netmap_load_map(na, txr->txtag,
			    txbuf->map, NMB(na, slot + si));
		}
#endif /* DEV_NETMAP */
		/* Clear the EOP descriptor pointer */
		txbuf->eop = NULL;
        }

#ifdef IXGBE_FDIR
	/* Set the rate at which we sample packets */
	if (adapter->hw.mac.type != ixgbe_mac_82598EB)
		txr->atr_sample = atr_sample_rate;
#endif

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IXGBE_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
int
ixgbe_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		ixgbe_setup_transmit_ring(txr);

	return (0);
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
void
ixgbe_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		ixgbe_free_transmit_buffers(txr);
		ixgbe_dma_free(adapter, &txr->txdma);
		IXGBE_TX_UNLOCK(txr);
		IXGBE_TX_LOCK_DESTROY(txr);
	}
	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
ixgbe_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *tx_buffer;
	int             i;

	INIT_DEBUGOUT("ixgbe_free_transmit_ring: begin");

	if (txr->tx_buffers == NULL)
		return;

	tx_buffer = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
		if (tx_buffer->m_head != NULL) {
			bus_dmamap_sync(txr->txtag, tx_buffer->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag,
			    tx_buffer->map);
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
			if (tx_buffer->map != NULL) {
				bus_dmamap_destroy(txr->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		} else if (tx_buffer->map != NULL) {
			bus_dmamap_unload(txr->txtag,
			    tx_buffer->map);
			bus_dmamap_destroy(txr->txtag,
			    tx_buffer->map);
			tx_buffer->map = NULL;
		}
	}
#ifdef IXGBE_LEGACY_TX
	if (txr->br != NULL)
		buf_ring_free(txr->br, M_DEVBUF);
#endif
	if (txr->tx_buffers != NULL) {
		free(txr->tx_buffers, M_DEVBUF);
		txr->tx_buffers = NULL;
	}
	if (txr->txtag != NULL) {
		bus_dma_tag_destroy(txr->txtag);
		txr->txtag = NULL;
	}
	return;
}

/*********************************************************************
 *
 *  Advanced Context Descriptor setup for VLAN, CSUM or TSO
 *
 **********************************************************************/

static int
ixgbe_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp,
    u32 *cmd_type_len, u32 *olinfo_status)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	int	ehdrlen, ip_hlen = 0;
	u16	etype;
	u8	ipproto = 0;
	int	offload = TRUE;
	int	ctxd = txr->next_avail_desc;
	u16	vtag = 0;

	/* First check if TSO is to be used */
	if (mp->m_pkthdr.csum_flags & CSUM_TSO)
		return (ixgbe_tso_setup(txr, mp, cmd_type_len, olinfo_status));

	if ((mp->m_pkthdr.csum_flags & CSUM_OFFLOAD) == 0)
		offload = FALSE;

	/* Indicate the whole packet as payload when not doing TSO */
       	*olinfo_status |= mp->m_pkthdr.len << IXGBE_ADVTXD_PAYLEN_SHIFT;

	/* Now ready a context descriptor */
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the context descriptor. Hence
	** we need to make one even if not doing offloads.
	*/
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else if (!IXGBE_IS_X550VF(adapter) && (offload == FALSE))
		return (0);

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	/* Set the ether header length */
	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;

	if (offload == FALSE)
		goto no_offloads;

	switch (etype) {
		case ETHERTYPE_IP:
			ip = (struct ip *)(mp->m_data + ehdrlen);
			ip_hlen = ip->ip_hl << 2;
			ipproto = ip->ip_p;
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
			break;
		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
			ip_hlen = sizeof(struct ip6_hdr);
			/* XXX-BZ this will go badly in case of ext hdrs. */
			ipproto = ip6->ip6_nxt;
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
			break;
		default:
			offload = FALSE;
			break;
	}

	vlan_macip_lens |= ip_hlen;

	switch (ipproto) {
		case IPPROTO_TCP:
			if (mp->m_pkthdr.csum_flags & CSUM_TCP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
			break;

		case IPPROTO_UDP:
			if (mp->m_pkthdr.csum_flags & CSUM_UDP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP;
			break;

#if __FreeBSD_version >= 800000
		case IPPROTO_SCTP:
			if (mp->m_pkthdr.csum_flags & CSUM_SCTP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_SCTP;
			break;
#endif
		default:
			offload = FALSE;
			break;
	}

	if (offload) /* For the TX descriptor setup */
		*olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;

no_offloads:
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == txr->num_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return (0);
}

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  adapters using advanced tx descriptors
 *
 **********************************************************************/
static int
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp,
    u32 *cmd_type_len, u32 *olinfo_status)
{
	struct ixgbe_adv_tx_context_desc *TXD;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	u32 mss_l4len_idx = 0, paylen;
	u16 vtag = 0, eh_type;
	int ctxd, ehdrlen, ip_hlen, tcp_hlen;
	struct ether_vlan_header *eh;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
#ifdef INET
	struct ip *ip;
#endif
	struct tcphdr *th;


	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		eh_type = eh->evl_proto;
	} else {
		ehdrlen = ETHER_HDR_LEN;
		eh_type = eh->evl_encap_proto;
	}

	switch (ntohs(eh_type)) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		/* XXX-BZ For now we do not pretend to support ext. hdrs. */
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return (ENXIO);
		ip_hlen = sizeof(struct ip6_hdr);
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		th = (struct tcphdr *)((caddr_t)ip6 + ip_hlen);
		th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		ip = (struct ip *)(mp->m_data + ehdrlen);
		if (ip->ip_p != IPPROTO_TCP)
			return (ENXIO);
		ip->ip_sum = 0;
		ip_hlen = ip->ip_hl << 2;
		th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
		th->th_sum = in_pseudo(ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
		/* Tell transmit desc to also do IPv4 checksum. */
		*olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
		break;
#endif
	default:
		panic("%s: CSUM_TSO but no supported IP version (0x%04x)",
		    __func__, ntohs(eh_type));
		break;
	}

	ctxd = txr->next_avail_desc;
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	tcp_hlen = th->th_off << 2;

	/* This is used in the transmit desc in encap */
	paylen = mp->m_pkthdr.len - ehdrlen - ip_hlen - tcp_hlen;

	/* VLAN MACLEN IPLEN */
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
                vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);

	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.tso_segsz << IXGBE_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << IXGBE_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);

	if (++ctxd == txr->num_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	*cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
	*olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
	*olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
	++txr->tso_tx;
	return (0);
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
void
ixgbe_txeof(struct tx_ring *txr)
{
#ifdef DEV_NETMAP
	struct adapter		*adapter = txr->adapter;
	struct ifnet		*ifp = adapter->ifp;
#endif
	u32			work, processed = 0;
	u16			limit = txr->process_limit;
	struct ixgbe_tx_buf	*buf;
	union ixgbe_adv_tx_desc *txd;

	mtx_assert(&txr->tx_mtx, MA_OWNED);

#ifdef DEV_NETMAP
	if (ifp->if_capenable & IFCAP_NETMAP) {
		struct netmap_adapter *na = NA(ifp);
		struct netmap_kring *kring = &na->tx_rings[txr->me];
		txd = txr->tx_base;
		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
		    BUS_DMASYNC_POSTREAD);
		/*
		 * In netmap mode, all the work is done in the context
		 * of the client thread. Interrupt handlers only wake up
		 * clients, which may be sleeping on individual rings
		 * or on a global resource for all rings.
		 * To implement tx interrupt mitigation, we wake up the client
		 * thread roughly every half ring, even if the NIC interrupts
		 * more frequently. This is implemented as follows:
		 * - ixgbe_txsync() sets kring->nr_kflags with the index of
		 *   the slot that should wake up the thread (nkr_num_slots
		 *   means the user thread should not be woken up);
		 * - the driver ignores tx interrupts unless netmap_mitigate=0
		 *   or the slot has the DD bit set.
		 */
		if (!netmap_mitigate ||
		    (kring->nr_kflags < kring->nkr_num_slots &&
		    txd[kring->nr_kflags].wb.status & IXGBE_TXD_STAT_DD)) {
			netmap_tx_irq(ifp, txr->me);
		}
		return;
	}
#endif /* DEV_NETMAP */

	if (txr->tx_avail == txr->num_desc) {
		txr->busy = 0;
		return;
	}

	/* Get work starting point */
	work = txr->next_to_clean;
	buf = &txr->tx_buffers[work];
	txd = &txr->tx_base[work];
	work -= txr->num_desc; /* The distance to ring end */
        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);

	do {
		union ixgbe_adv_tx_desc *eop= buf->eop;
		if (eop == NULL) /* No work */
			break;

		if ((eop->wb.status & IXGBE_TXD_STAT_DD) == 0)
			break;	/* I/O not complete */

		if (buf->m_head) {
			txr->bytes +=
			    buf->m_head->m_pkthdr.len;
			bus_dmamap_sync(txr->txtag,
			    buf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag,
			    buf->map);
			m_freem(buf->m_head);
			buf->m_head = NULL;
		}
		buf->eop = NULL;
		++txr->tx_avail;

		/* We clean the range if multi segment */
		while (txd != eop) {
			++txd;
			++buf;
			++work;
			/* wrap the ring? */
			if (__predict_false(!work)) {
				work -= txr->num_desc;
				buf = txr->tx_buffers;
				txd = txr->tx_base;
			}
			if (buf->m_head) {
				txr->bytes +=
				    buf->m_head->m_pkthdr.len;
				bus_dmamap_sync(txr->txtag,
				    buf->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(txr->txtag,
				    buf->map);
				m_freem(buf->m_head);
				buf->m_head = NULL;
			}
			++txr->tx_avail;
			buf->eop = NULL;

		}
		++txr->packets;
		++processed;

		/* Try the next packet */
		++txd;
		++buf;
		++work;
		/* reset with a wrap */
		if (__predict_false(!work)) {
			work -= txr->num_desc;
			buf = txr->tx_buffers;
			txd = txr->tx_base;
		}
		prefetch(txd);
	} while (__predict_true(--limit));

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	work += txr->num_desc;
	txr->next_to_clean = work;

	/*
	** Queue Hang detection, we know there's
	** work outstanding or the first return
	** would have been taken, so increment busy
	** if nothing managed to get cleaned, then
	** in local_timer it will be checked and 
	** marked as HUNG if it exceeds a MAX attempt.
	*/
	if ((processed == 0) && (txr->busy != IXGBE_QUEUE_HUNG))
		++txr->busy;
	/*
	** If anything gets cleaned we reset state to 1,
	** note this will turn off HUNG if its set.
	*/
	if (processed)
		txr->busy = 1;

	if (txr->tx_avail == txr->num_desc)
		txr->busy = 0;

	return;
}


#ifdef IXGBE_FDIR
/*
** This routine parses packet headers so that Flow
** Director can make a hashed filter table entry 
** allowing traffic flows to be identified and kept
** on the same cpu.  This would be a performance
** hit, but we only do it at IXGBE_FDIR_RATE of
** packets.
*/
static void
ixgbe_atr(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter			*adapter = txr->adapter;
	struct ix_queue			*que;
	struct ip			*ip;
	struct tcphdr			*th;
	struct udphdr			*uh;
	struct ether_vlan_header	*eh;
	union ixgbe_atr_hash_dword	input = {.dword = 0}; 
	union ixgbe_atr_hash_dword	common = {.dword = 0}; 
	int  				ehdrlen, ip_hlen;
	u16				etype;

	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		etype = eh->evl_proto;
	} else {
		ehdrlen = ETHER_HDR_LEN;
		etype = eh->evl_encap_proto;
	}

	/* Only handling IPv4 */
	if (etype != htons(ETHERTYPE_IP))
		return;

	ip = (struct ip *)(mp->m_data + ehdrlen);
	ip_hlen = ip->ip_hl << 2;

	/* check if we're UDP or TCP */
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
		/* src and dst are inverted */
		common.port.dst ^= th->th_sport;
		common.port.src ^= th->th_dport;
		input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_TCPV4;
		break;
	case IPPROTO_UDP:
		uh = (struct udphdr *)((caddr_t)ip + ip_hlen);
		/* src and dst are inverted */
		common.port.dst ^= uh->uh_sport;
		common.port.src ^= uh->uh_dport;
		input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_UDPV4;
		break;
	default:
		return;
	}

	input.formatted.vlan_id = htobe16(mp->m_pkthdr.ether_vtag);
	if (mp->m_pkthdr.ether_vtag)
		common.flex_bytes ^= htons(ETHERTYPE_VLAN);
	else
		common.flex_bytes ^= etype;
	common.ip ^= ip->ip_src.s_addr ^ ip->ip_dst.s_addr;

	que = &adapter->queues[txr->me];
	/*
	** This assumes the Rx queue and Tx
	** queue are bound to the same CPU
	*/
	ixgbe_fdir_add_signature_filter_82599(&adapter->hw,
	    input, common, que->msix);
}
#endif /* IXGBE_FDIR */

/*
** Used to detect a descriptor that has
** been merged by Hardware RSC.
*/
static inline u32
ixgbe_rsc_count(union ixgbe_adv_rx_desc *rx)
{
	return (le32toh(rx->wb.lower.lo_dword.data) &
	    IXGBE_RXDADV_RSCCNT_MASK) >> IXGBE_RXDADV_RSCCNT_SHIFT;
}

/*********************************************************************
 *
 *  Initialize Hardware RSC (LRO) feature on 82599
 *  for an RX ring, this is toggled by the LRO capability
 *  even though it is transparent to the stack.
 *
 *  NOTE: since this HW feature only works with IPV4 and 
 *        our testing has shown soft LRO to be as effective
 *        I have decided to disable this by default.
 *
 **********************************************************************/
static void
ixgbe_setup_hw_rsc(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	struct	ixgbe_hw	*hw = &adapter->hw;
	u32			rscctrl, rdrxctl;

	/* If turning LRO/RSC off we need to disable it */
	if ((adapter->ifp->if_capenable & IFCAP_LRO) == 0) {
		rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(rxr->me));
		rscctrl &= ~IXGBE_RSCCTL_RSCEN;
		return;
	}

	rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
	rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
#ifdef DEV_NETMAP /* crcstrip is optional in netmap */
	if (adapter->ifp->if_capenable & IFCAP_NETMAP && !ix_crcstrip)
#endif /* DEV_NETMAP */
	rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
	rdrxctl |= IXGBE_RDRXCTL_RSCACKC;
	IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);

	rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(rxr->me));
	rscctrl |= IXGBE_RSCCTL_RSCEN;
	/*
	** Limit the total number of descriptors that
	** can be combined, so it does not exceed 64K
	*/
	if (rxr->mbuf_sz == MCLBYTES)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
	else if (rxr->mbuf_sz == MJUMPAGESIZE)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
	else if (rxr->mbuf_sz == MJUM9BYTES)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
	else  /* Using 16K cluster */
		rscctrl |= IXGBE_RSCCTL_MAXDESC_1;

	IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(rxr->me), rscctrl);

	/* Enable TCP header recognition */
	IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0),
	    (IXGBE_READ_REG(hw, IXGBE_PSRTYPE(0)) |
	    IXGBE_PSRTYPE_TCPHDR));

	/* Disable RSC for ACK packets */
	IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
	    (IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));

	rxr->hw_rsc = TRUE;
}
/*********************************************************************
 *
 *  Refresh mbuf buffers for RX descriptor rings
 *   - now keeps its own state so discards due to resource
 *     exhaustion are unnecessary, if an mbuf cannot be obtained
 *     it just returns, keeping its placeholder, thus it can simply
 *     be recalled to try again.
 *
 **********************************************************************/
static void
ixgbe_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	bus_dma_segment_t	seg[1];
	struct ixgbe_rx_buf	*rxbuf;
	struct mbuf		*mp;
	int			i, j, nsegs, error;
	bool			refreshed = FALSE;

	i = j = rxr->next_to_refresh;
	/* Control the loop with one beyond */
	if (++j == rxr->num_desc)
		j = 0;

	while (j != limit) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->buf == NULL) {
			mp = m_getjcl(M_NOWAIT, MT_DATA,
			    M_PKTHDR, rxr->mbuf_sz);
			if (mp == NULL)
				goto update;
			if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
		} else
			mp = rxbuf->buf;

		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;

		/* If we're dealing with an mbuf that was copied rather
		 * than replaced, there's no need to go through busdma.
		 */
		if ((rxbuf->flags & IXGBE_RX_COPY) == 0) {
			/* Get the memory mapping */
			bus_dmamap_unload(rxr->ptag, rxbuf->pmap);
			error = bus_dmamap_load_mbuf_sg(rxr->ptag,
			    rxbuf->pmap, mp, seg, &nsegs, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("Refresh mbufs: payload dmamap load"
				    " failure - %d\n", error);
				m_free(mp);
				rxbuf->buf = NULL;
				goto update;
			}
			rxbuf->buf = mp;
			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
			    BUS_DMASYNC_PREREAD);
			rxbuf->addr = rxr->rx_base[i].read.pkt_addr =
			    htole64(seg[0].ds_addr);
		} else {
			rxr->rx_base[i].read.pkt_addr = rxbuf->addr;
			rxbuf->flags &= ~IXGBE_RX_COPY;
		}

		refreshed = TRUE;
		/* Next is precalculated */
		i = j;
		rxr->next_to_refresh = i;
		if (++j == rxr->num_desc)
			j = 0;
	}
update:
	if (refreshed) /* Update hardware tail index */
		IXGBE_WRITE_REG(&adapter->hw,
		    rxr->tail, rxr->next_to_refresh);
	return;
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one
 *  rx_buffer per received packet, the maximum number of rx_buffer's
 *  that we'll need is equal to the number of receive descriptors
 *  that we've allocated.
 *
 **********************************************************************/
int
ixgbe_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	device_t 		dev = adapter->dev;
	struct ixgbe_rx_buf 	*rxbuf;
	int             	bsize, error;

	bsize = sizeof(struct ixgbe_rx_buf) * rxr->num_desc;
	if (!(rxr->rx_buffers =
	    (struct ixgbe_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
				   1, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MJUM16BYTES,		/* maxsize */
				   1,			/* nsegments */
				   MJUM16BYTES,		/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->ptag))) {
		device_printf(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	for (int i = 0; i < rxr->num_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->ptag, 0, &rxbuf->pmap);
		if (error) {
			device_printf(dev, "Unable to create RX dma map\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	ixgbe_free_receive_structures(adapter);
	return (error);
}


static void     
ixgbe_free_receive_ring(struct rx_ring *rxr)
{ 
	struct ixgbe_rx_buf       *rxbuf;

	for (int i = 0; i < rxr->num_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->buf != NULL) {
			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->ptag, rxbuf->pmap);
			rxbuf->buf->m_flags |= M_PKTHDR;
			m_freem(rxbuf->buf);
			rxbuf->buf = NULL;
			rxbuf->flags = 0;
		}
	}
}


/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
ixgbe_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter 	*adapter;
	struct ifnet		*ifp;
	device_t		dev;
	struct ixgbe_rx_buf	*rxbuf;
	bus_dma_segment_t	seg[1];
	struct lro_ctrl		*lro = &rxr->lro;
	int			rsize, nsegs, error = 0;
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(rxr->adapter->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */

	adapter = rxr->adapter;
	ifp = adapter->ifp;
	dev = adapter->dev;

	/* Clear the ring contents */
	IXGBE_RX_LOCK(rxr);
#ifdef DEV_NETMAP
	/* same as in ixgbe_setup_transmit_ring() */
	slot = netmap_reset(na, NR_RX, rxr->me, 0);
#endif /* DEV_NETMAP */
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);
	/* Cache the size */
	rxr->mbuf_sz = adapter->rx_mbuf_sz;

	/* Free current RX buffer structs and their mbufs */
	ixgbe_free_receive_ring(rxr);

	/* Now replenish the mbufs */
	for (int j = 0; j != rxr->num_desc; ++j) {
		struct mbuf	*mp;

		rxbuf = &rxr->rx_buffers[j];
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, fill the map and set the buffer
		 * address in the NIC ring, considering the offset
		 * between the netmap and NIC rings (see comment in
		 * ixgbe_setup_transmit_ring() ). No need to allocate
		 * an mbuf, so end the block with a continue;
		 */
		if (slot) {
			int sj = netmap_idx_n2k(&na->rx_rings[rxr->me], j);
			uint64_t paddr;
			void *addr;

			addr = PNMB(na, slot + sj, &paddr);
			netmap_load_map(na, rxr->ptag, rxbuf->pmap, addr);
			/* Update descriptor and the cached value */
			rxr->rx_base[j].read.pkt_addr = htole64(paddr);
			rxbuf->addr = htole64(paddr);
			continue;
		}
#endif /* DEV_NETMAP */
		rxbuf->flags = 0; 
		rxbuf->buf = m_getjcl(M_NOWAIT, MT_DATA,
		    M_PKTHDR, adapter->rx_mbuf_sz);
		if (rxbuf->buf == NULL) {
			error = ENOBUFS;
                        goto fail;
		}
		mp = rxbuf->buf;
		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->ptag,
		    rxbuf->pmap, mp, seg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0)
                        goto fail;
		bus_dmamap_sync(rxr->ptag,
		    rxbuf->pmap, BUS_DMASYNC_PREREAD);
		/* Update the descriptor and the cached value */
		rxr->rx_base[j].read.pkt_addr = htole64(seg[0].ds_addr);
		rxbuf->addr = htole64(seg[0].ds_addr);
	}


	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;
	rxr->lro_enabled = FALSE;
	rxr->rx_copies = 0;
	rxr->rx_bytes = 0;
	rxr->vtag_strip = FALSE;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	** Now set up the LRO interface:
	*/
	if (ixgbe_rsc_enable)
		ixgbe_setup_hw_rsc(rxr);
	else if (ifp->if_capenable & IFCAP_LRO) {
		int err = tcp_lro_init(lro);
		if (err) {
			device_printf(dev, "LRO Initialization failed!\n");
			goto fail;
		}
		INIT_DEBUGOUT("RX Soft LRO Initialized\n");
		rxr->lro_enabled = TRUE;
		lro->ifp = adapter->ifp;
	}

	IXGBE_RX_UNLOCK(rxr);
	return (0);

fail:
	ixgbe_free_receive_ring(rxr);
	IXGBE_RX_UNLOCK(rxr);
	return (error);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
int
ixgbe_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int j;

	for (j = 0; j < adapter->num_queues; j++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'j' failed, so its the terminus.
	 */
	for (int i = 0; i < j; ++i) {
		rxr = &adapter->rx_rings[i];
		ixgbe_free_receive_ring(rxr);
	}

	return (ENOBUFS);
}


/*********************************************************************
 *
 *  Free all receive rings.
 *
 **********************************************************************/
void
ixgbe_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	INIT_DEBUGOUT("ixgbe_free_receive_structures: begin");

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		struct lro_ctrl		*lro = &rxr->lro;
		ixgbe_free_receive_buffers(rxr);
		/* Free LRO memory */
		tcp_lro_free(lro);
		/* Free the ring memory as well */
		ixgbe_dma_free(adapter, &rxr->rxdma);
	}

	free(adapter->rx_rings, M_DEVBUF);
}


/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
void
ixgbe_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixgbe_rx_buf	*rxbuf;

	INIT_DEBUGOUT("ixgbe_free_receive_buffers: begin");

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->buf != NULL) {
				bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->ptag, rxbuf->pmap);
				rxbuf->buf->m_flags |= M_PKTHDR;
				m_freem(rxbuf->buf);
			}
			rxbuf->buf = NULL;
			if (rxbuf->pmap != NULL) {
				bus_dmamap_destroy(rxr->ptag, rxbuf->pmap);
				rxbuf->pmap = NULL;
			}
		}
		if (rxr->rx_buffers != NULL) {
			free(rxr->rx_buffers, M_DEVBUF);
			rxr->rx_buffers = NULL;
		}
	}

	if (rxr->ptag != NULL) {
		bus_dma_tag_destroy(rxr->ptag);
		rxr->ptag = NULL;
	}

	return;
}

static __inline void
ixgbe_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u32 ptype)
{
                 
        /*
         * ATM LRO is only for IP/TCP packets and TCP checksum of the packet
         * should be computed by hardware. Also it should not have VLAN tag in
         * ethernet header.  In case of IPv6 we do not yet support ext. hdrs.
         */
        if (rxr->lro_enabled &&
            (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0 &&
            (ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
            ((ptype & (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP) ||
            (ptype & (IXGBE_RXDADV_PKTTYPE_IPV6 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV6 | IXGBE_RXDADV_PKTTYPE_TCP)) &&
            (m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) ==
            (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) {
                /*
                 * Send to the stack if:
                 **  - LRO not enabled, or
                 **  - no LRO resources, or
                 **  - lro enqueue fails
                 */
                if (rxr->lro.lro_cnt != 0)
                        if (tcp_lro_rx(&rxr->lro, m, 0) == 0)
                                return;
        }
	IXGBE_RX_UNLOCK(rxr);
        (*ifp->if_input)(ifp, m);
	IXGBE_RX_LOCK(rxr);
}

static __inline void
ixgbe_rx_discard(struct rx_ring *rxr, int i)
{
	struct ixgbe_rx_buf	*rbuf;

	rbuf = &rxr->rx_buffers[i];


	/*
	** With advanced descriptors the writeback
	** clobbers the buffer addrs, so its easier
	** to just free the existing mbufs and take
	** the normal refresh path to get new buffers
	** and mapping.
	*/

	if (rbuf->fmp != NULL) {/* Partial chain ? */
		rbuf->fmp->m_flags |= M_PKTHDR;
		m_freem(rbuf->fmp);
		rbuf->fmp = NULL;
		rbuf->buf = NULL; /* rbuf->buf is part of fmp's chain */
	} else if (rbuf->buf) {
		m_free(rbuf->buf);
		rbuf->buf = NULL;
	}
	bus_dmamap_unload(rxr->ptag, rbuf->pmap);

	rbuf->flags = 0;
 
	return;
}


/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *  Return TRUE for more work, FALSE for all clean.
 *********************************************************************/
bool
ixgbe_rxeof(struct ix_queue *que)
{
	struct adapter		*adapter = que->adapter;
	struct rx_ring		*rxr = que->rxr;
	struct ifnet		*ifp = adapter->ifp;
	struct lro_ctrl		*lro = &rxr->lro;
	struct lro_entry	*queued;
	int			i, nextp, processed = 0;
	u32			staterr = 0;
	u16			count = rxr->process_limit;
	union ixgbe_adv_rx_desc	*cur;
	struct ixgbe_rx_buf	*rbuf, *nbuf;
	u16			pkt_info;

	IXGBE_RX_LOCK(rxr);

#ifdef DEV_NETMAP
	/* Same as the txeof routine: wakeup clients on intr. */
	if (netmap_rx_irq(ifp, rxr->me, &processed)) {
		IXGBE_RX_UNLOCK(rxr);
		return (FALSE);
	}
#endif /* DEV_NETMAP */

	for (i = rxr->next_to_check; count != 0;) {
		struct mbuf	*sendmp, *mp;
		u32		rsc, ptype;
		u16		len;
		u16		vtag = 0;
		bool		eop;
 
		/* Sync the ring. */
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->rx_base[i];
		staterr = le32toh(cur->wb.upper.status_error);
		pkt_info = le16toh(cur->wb.lower.lo_dword.hs_rss.pkt_info);

		if ((staterr & IXGBE_RXD_STAT_DD) == 0)
			break;
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		count--;
		sendmp = NULL;
		nbuf = NULL;
		rsc = 0;
		cur->wb.upper.status_error = 0;
		rbuf = &rxr->rx_buffers[i];
		mp = rbuf->buf;

		len = le16toh(cur->wb.upper.length);
		ptype = le32toh(cur->wb.lower.lo_dword.data) &
		    IXGBE_RXDADV_PKTTYPE_MASK;
		eop = ((staterr & IXGBE_RXD_STAT_EOP) != 0);

		/* Make sure bad packets are discarded */
		if (eop && (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) != 0) {
#if __FreeBSD_version >= 1100036
			if (IXGBE_IS_VF(adapter))
				if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
#endif
			rxr->rx_discarded++;
			ixgbe_rx_discard(rxr, i);
			goto next_desc;
		}

		/*
		** On 82599 which supports a hardware
		** LRO (called HW RSC), packets need
		** not be fragmented across sequential
		** descriptors, rather the next descriptor
		** is indicated in bits of the descriptor.
		** This also means that we might proceses
		** more than one packet at a time, something
		** that has never been true before, it
		** required eliminating global chain pointers
		** in favor of what we are doing here.  -jfv
		*/
		if (!eop) {
			/*
			** Figure out the next descriptor
			** of this frame.
			*/
			if (rxr->hw_rsc == TRUE) {
				rsc = ixgbe_rsc_count(cur);
				rxr->rsc_num += (rsc - 1);
			}
			if (rsc) { /* Get hardware index */
				nextp = ((staterr &
				    IXGBE_RXDADV_NEXTP_MASK) >>
				    IXGBE_RXDADV_NEXTP_SHIFT);
			} else { /* Just sequential */
				nextp = i + 1;
				if (nextp == adapter->num_rx_desc)
					nextp = 0;
			}
			nbuf = &rxr->rx_buffers[nextp];
			prefetch(nbuf);
		}
		/*
		** Rather than using the fmp/lmp global pointers
		** we now keep the head of a packet chain in the
		** buffer struct and pass this along from one
		** descriptor to the next, until we get EOP.
		*/
		mp->m_len = len;
		/*
		** See if there is a stored head
		** that determines what we are
		*/
		sendmp = rbuf->fmp;
		if (sendmp != NULL) {  /* secondary frag */
			rbuf->buf = rbuf->fmp = NULL;
			mp->m_flags &= ~M_PKTHDR;
			sendmp->m_pkthdr.len += mp->m_len;
		} else {
			/*
			 * Optimize.  This might be a small packet,
			 * maybe just a TCP ACK.  Do a fast copy that
			 * is cache aligned into a new mbuf, and
			 * leave the old mbuf+cluster for re-use.
			 */
			if (eop && len <= IXGBE_RX_COPY_LEN) {
				sendmp = m_gethdr(M_NOWAIT, MT_DATA);
				if (sendmp != NULL) {
					sendmp->m_data +=
					    IXGBE_RX_COPY_ALIGN;
					ixgbe_bcopy(mp->m_data,
					    sendmp->m_data, len);
					sendmp->m_len = len;
					rxr->rx_copies++;
					rbuf->flags |= IXGBE_RX_COPY;
				}
			}
			if (sendmp == NULL) {
				rbuf->buf = rbuf->fmp = NULL;
				sendmp = mp;
			}

			/* first desc of a non-ps chain */
			sendmp->m_flags |= M_PKTHDR;
			sendmp->m_pkthdr.len = mp->m_len;
		}
		++processed;

		/* Pass the head pointer on */
		if (eop == 0) {
			nbuf->fmp = sendmp;
			sendmp = NULL;
			mp->m_next = nbuf->buf;
		} else { /* Sending this frame */
			sendmp->m_pkthdr.rcvif = ifp;
			rxr->rx_packets++;
			/* capture data for AIM */
			rxr->bytes += sendmp->m_pkthdr.len;
			rxr->rx_bytes += sendmp->m_pkthdr.len;
			/* Process vlan info */
			if ((rxr->vtag_strip) &&
			    (staterr & IXGBE_RXD_STAT_VP))
				vtag = le16toh(cur->wb.upper.vlan);
			if (vtag) {
				sendmp->m_pkthdr.ether_vtag = vtag;
				sendmp->m_flags |= M_VLANTAG;
			}
			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
				ixgbe_rx_checksum(staterr, sendmp, ptype);

                        /*
                         * In case of multiqueue, we have RXCSUM.PCSD bit set
                         * and never cleared. This means we have RSS hash
                         * available to be used.   
                         */
                        if (adapter->num_queues > 1) {
                                sendmp->m_pkthdr.flowid =
                                    le32toh(cur->wb.lower.hi_dword.rss);
                                switch (pkt_info & IXGBE_RXDADV_RSSTYPE_MASK) {  
                                    case IXGBE_RXDADV_RSSTYPE_IPV4_TCP:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_TCP_IPV4);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV4:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_IPV4);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6_TCP:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_TCP_IPV6);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6_EX:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_IPV6_EX);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_IPV6);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6_TCP_EX:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_TCP_IPV6_EX);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV4_UDP:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_UDP_IPV4);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6_UDP:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_UDP_IPV6);
                                        break;
                                    case IXGBE_RXDADV_RSSTYPE_IPV6_UDP_EX:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_RSS_UDP_IPV6_EX);
                                        break;
                                    default:
                                        M_HASHTYPE_SET(sendmp,
                                            M_HASHTYPE_OPAQUE);
                                }
                        } else {
                                sendmp->m_pkthdr.flowid = que->msix;
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_OPAQUE);
			}
		}
next_desc:
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == rxr->num_desc)
			i = 0;

		/* Now send to the stack or do LRO */
		if (sendmp != NULL) {
			rxr->next_to_check = i;
			ixgbe_rx_input(rxr, ifp, sendmp, ptype);
			i = rxr->next_to_check;
		}

               /* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
			ixgbe_refresh_mbufs(rxr, i);
			processed = 0;
		}
	}

	/* Refresh any remaining buf structs */
	if (ixgbe_rx_unrefreshed(rxr))
		ixgbe_refresh_mbufs(rxr, i);

	rxr->next_to_check = i;

	/*
	 * Flush any outstanding LRO work
	 */
	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}

	IXGBE_RX_UNLOCK(rxr);

	/*
	** Still have cleaning to do?
	*/
	if ((staterr & IXGBE_RXD_STAT_DD) != 0)
		return (TRUE);
	else
		return (FALSE);
}


/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixgbe_rx_checksum(u32 staterr, struct mbuf * mp, u32 ptype)
{
	u16	status = (u16) staterr;
	u8	errors = (u8) (staterr >> 24);
	bool	sctp = FALSE;

	if ((ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
	    (ptype & IXGBE_RXDADV_PKTTYPE_SCTP) != 0)
		sctp = TRUE;

	if (status & IXGBE_RXD_STAT_IPCS) {
		if (!(errors & IXGBE_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else
			mp->m_pkthdr.csum_flags = 0;
	}
	if (status & IXGBE_RXD_STAT_L4CS) {
		u64 type = (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
#if __FreeBSD_version >= 800000
		if (sctp)
			type = CSUM_SCTP_VALID;
#endif
		if (!(errors & IXGBE_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |= type;
			if (!sctp)
				mp->m_pkthdr.csum_data = htons(0xffff);
		} 
	}
	return;
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/
static void
ixgbe_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

int
ixgbe_dma_malloc(struct adapter *adapter, bus_size_t size,
		struct ixgbe_dma_alloc *dma, int mapflags)
{
	device_t dev = adapter->dev;
	int             r;

	r = bus_dma_tag_create(bus_get_dma_tag(adapter->dev),	/* parent */
			       DBA_ALIGN, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
			       &dma->dma_tag);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dma_tag_create failed; "
		       "error %u\n", r);
		goto fail_0;
	}
	r = bus_dmamem_alloc(dma->dma_tag, (void **)&dma->dma_vaddr,
			     BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dmamem_alloc failed; "
		       "error %u\n", r);
		goto fail_1;
	}
	r = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
			    size,
			    ixgbe_dmamap_cb,
			    &dma->dma_paddr,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dmamap_load failed; "
		       "error %u\n", r);
		goto fail_2;
	}
	dma->dma_size = size;
	return (0);
fail_2:
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
fail_1:
	bus_dma_tag_destroy(dma->dma_tag);
fail_0:
	dma->dma_tag = NULL;
	return (r);
}

void
ixgbe_dma_free(struct adapter *adapter, struct ixgbe_dma_alloc *dma)
{
	bus_dmamap_sync(dma->dma_tag, dma->dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
	bus_dma_tag_destroy(dma->dma_tag);
}


/*********************************************************************
 *
 *  Allocate memory for the transmit and receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
int
ixgbe_allocate_queues(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	struct rx_ring	*rxr;
	int rsize, tsize, error = IXGBE_SUCCESS;
	int txconf = 0, rxconf = 0;
#ifdef PCI_IOV
	enum ixgbe_iov_mode iov_mode;
#endif

        /* First allocate the top level queue structs */
        if (!(adapter->queues =
            (struct ix_queue *) malloc(sizeof(struct ix_queue) *
            adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                device_printf(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                goto fail;
        }

	/* First allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto tx_fail;
	}

	/* Next allocate the RX */
	if (!(adapter->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto rx_fail;
	}

	/* For the ring itself */
	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), DBA_ALIGN);

#ifdef PCI_IOV
	iov_mode = ixgbe_get_iov_mode(adapter);
	adapter->pool = ixgbe_max_vfs(iov_mode);
#else
	adapter->pool = 0;
#endif
	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (int i = 0; i < adapter->num_queues; i++, txconf++) {
		/* Set up some basics */
		txr = &adapter->tx_rings[i];
		txr->adapter = adapter;
#ifdef PCI_IOV
		txr->me = ixgbe_pf_que_index(iov_mode, i);
#else
		txr->me = i;
#endif
		txr->num_desc = adapter->num_tx_desc;

		/* Initialize the TX side lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(dev), txr->me);
		mtx_init(&txr->tx_mtx, txr->mtx_name, NULL, MTX_DEF);

		if (ixgbe_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (union ixgbe_adv_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	/* Now allocate transmit buffers for the ring */
        	if (ixgbe_allocate_transmit_buffers(txr)) {
			device_printf(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#ifndef IXGBE_LEGACY_TX
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(IXGBE_BR_SIZE, M_DEVBUF,
		    M_WAITOK, &txr->tx_mtx);
		if (txr->br == NULL) {
			device_printf(dev,
			    "Critical Failure setting up buf ring\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#endif
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	for (int i = 0; i < adapter->num_queues; i++, rxconf++) {
		rxr = &adapter->rx_rings[i];
		/* Set up some basics */
		rxr->adapter = adapter;
#ifdef PCI_IOV
		rxr->me = ixgbe_pf_que_index(iov_mode, i);
#else
		rxr->me = i;
#endif
		rxr->num_desc = adapter->num_rx_desc;

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), rxr->me);
		mtx_init(&rxr->rx_mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (ixgbe_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (union ixgbe_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (ixgbe_allocate_receive_buffers(rxr)) {
			device_printf(dev,
			    "Critical Failure setting up receive buffers\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
	}

	/*
	** Finally set up the queue holding structs
	*/
	for (int i = 0; i < adapter->num_queues; i++) {
		que = &adapter->queues[i];
		que->adapter = adapter;
		que->me = i;
		que->txr = &adapter->tx_rings[i];
		que->rxr = &adapter->rx_rings[i];
	}

	return (0);

err_rx_desc:
	for (rxr = adapter->rx_rings; rxconf > 0; rxr++, rxconf--)
		ixgbe_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		ixgbe_dma_free(adapter, &txr->txdma);
	free(adapter->rx_rings, M_DEVBUF);
rx_fail:
	free(adapter->tx_rings, M_DEVBUF);
tx_fail:
	free(adapter->queues, M_DEVBUF);
fail:
	return (error);
}
