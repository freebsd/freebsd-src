/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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

/*
**	IXL driver TX/RX Routines:
**	    This was seperated to allow usage by
** 	    both the BASE and the VF drivers.
*/

#ifndef IXL_STANDALONE_BUILD
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"
#endif

#include "ixl.h"

#ifdef RSS
#include <net/rss_config.h>
#endif

/* Local Prototypes */
static void	ixl_rx_checksum(struct mbuf *, u32, u32, u8);
static void	ixl_refresh_mbufs(struct ixl_queue *, int);
static int      ixl_xmit(struct ixl_queue *, struct mbuf **);
static int	ixl_tx_setup_offload(struct ixl_queue *,
		    struct mbuf *, u32 *, u32 *);
static bool	ixl_tso_setup(struct ixl_queue *, struct mbuf *);

static __inline void ixl_rx_discard(struct rx_ring *, int);
static __inline void ixl_rx_input(struct rx_ring *, struct ifnet *,
		    struct mbuf *, u8);

#ifdef DEV_NETMAP
#include <dev/netmap/if_ixl_netmap.h>
#endif /* DEV_NETMAP */

/*
** Multiqueue Transmit driver
*/
int
ixl_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct ixl_vsi		*vsi = ifp->if_softc;
	struct ixl_queue	*que;
	struct tx_ring		*txr;
	int 			err, i;
#ifdef RSS
	u32			bucket_id;
#endif

	/*
	** Which queue to use:
	**
	** When doing RSS, map it to the same outbound
	** queue as the incoming flow would be mapped to.
	** If everything is setup correctly, it should be
	** the same bucket that the current CPU we're on is.
	*/
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
#ifdef  RSS
		if (rss_hash2bucket(m->m_pkthdr.flowid,
		    M_HASHTYPE_GET(m), &bucket_id) == 0) {
			i = bucket_id % vsi->num_queues;
                } else
#endif
                        i = m->m_pkthdr.flowid % vsi->num_queues;
        } else
		i = curcpu % vsi->num_queues;
	/*
	** This may not be perfect, but until something
	** better comes along it will keep from scheduling
	** on stalled queues.
	*/
	if (((1 << i) & vsi->active_queues) == 0)
		i = ffsl(vsi->active_queues);

	que = &vsi->queues[i];
	txr = &que->txr;

	err = drbr_enqueue(ifp, txr->br, m);
	if (err)
		return (err);
	if (IXL_TX_TRYLOCK(txr)) {
		ixl_mq_start_locked(ifp, txr);
		IXL_TX_UNLOCK(txr);
	} else
		taskqueue_enqueue(que->tq, &que->tx_task);

	return (0);
}

int
ixl_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr)
{
	struct ixl_queue	*que = txr->que;
	struct ixl_vsi		*vsi = que->vsi;
        struct mbuf		*next;
        int			err = 0;


	if (((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) ||
	    vsi->link_active == 0)
		return (ENETDOWN);

	/* Process the transmit queue */
	while ((next = drbr_peek(ifp, txr->br)) != NULL) {
		if ((err = ixl_xmit(que, &next)) != 0) {
			if (next == NULL)
				drbr_advance(ifp, txr->br);
			else
				drbr_putback(ifp, txr->br, next);
			break;
		}
		drbr_advance(ifp, txr->br);
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
	}

	if (txr->avail < IXL_TX_CLEANUP_THRESHOLD)
		ixl_txeof(que);

	return (err);
}

/*
 * Called from a taskqueue to drain queued transmit packets.
 */
void
ixl_deferred_mq_start(void *arg, int pending)
{
	struct ixl_queue	*que = arg;
        struct tx_ring		*txr = &que->txr;
	struct ixl_vsi		*vsi = que->vsi;
        struct ifnet		*ifp = vsi->ifp;
        
	IXL_TX_LOCK(txr);
	if (!drbr_empty(ifp, txr->br))
		ixl_mq_start_locked(ifp, txr);
	IXL_TX_UNLOCK(txr);
}

/*
** Flush all queue ring buffers
*/
void
ixl_qflush(struct ifnet *ifp)
{
	struct ixl_vsi	*vsi = ifp->if_softc;

        for (int i = 0; i < vsi->num_queues; i++) {
		struct ixl_queue *que = &vsi->queues[i];
		struct tx_ring	*txr = &que->txr;
		struct mbuf	*m;
		IXL_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IXL_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}

/*
** Find mbuf chains passed to the driver 
** that are 'sparse', using more than 8
** mbufs to deliver an mss-size chunk of data
*/
static inline bool
ixl_tso_detect_sparse(struct mbuf *mp)
{
	struct mbuf	*m;
	int		num = 0, mss;
	bool		ret = FALSE;

	mss = mp->m_pkthdr.tso_segsz;
	for (m = mp->m_next; m != NULL; m = m->m_next) {
		num++;
		mss -= m->m_len;
		if (mss < 1)
			break;
		if (m->m_next == NULL)
			break;
	}
	if (num > IXL_SPARSE_CHAIN)
		ret = TRUE;

	return (ret);
}


/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors, allowing the
 *  TX engine to transmit the packets. 
 *  	- return 0 on success, positive on failure
 *
 **********************************************************************/
#define IXL_TXD_CMD (I40E_TX_DESC_CMD_EOP | I40E_TX_DESC_CMD_RS)

static int
ixl_xmit(struct ixl_queue *que, struct mbuf **m_headp)
{
	struct ixl_vsi		*vsi = que->vsi;
	struct i40e_hw		*hw = vsi->hw;
	struct tx_ring		*txr = &que->txr;
	struct ixl_tx_buf	*buf;
	struct i40e_tx_desc	*txd = NULL;
	struct mbuf		*m_head, *m;
	int             	i, j, error, nsegs, maxsegs;
	int			first, last = 0;
	u16			vtag = 0;
	u32			cmd, off;
	bus_dmamap_t		map;
	bus_dma_tag_t		tag;
	bus_dma_segment_t	segs[IXL_MAX_TSO_SEGS];

	cmd = off = 0;
	m_head = *m_headp;

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail;
	buf = &txr->buffers[first];
	map = buf->map;
	tag = txr->tx_tag;
	maxsegs = IXL_MAX_TX_SEGS;

	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		/* Use larger mapping for TSO */
		tag = txr->tso_tag;
		maxsegs = IXL_MAX_TSO_SEGS;
		if (ixl_tso_detect_sparse(m_head)) {
			m = m_defrag(m_head, M_NOWAIT);
			if (m == NULL) {
				m_freem(*m_headp);
				*m_headp = NULL;
				return (ENOBUFS);
			}
			*m_headp = m;
		}
	}

	/*
	 * Map the packet for DMA.
	 */
	error = bus_dmamap_load_mbuf_sg(tag, map,
	    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

	if (error == EFBIG) {
		struct mbuf *m;

		m = m_defrag(*m_headp, M_NOWAIT);
		if (m == NULL) {
			que->mbuf_defrag_failed++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (ENOBUFS);
		}
		*m_headp = m;

		/* Try it again */
		error = bus_dmamap_load_mbuf_sg(tag, map,
		    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

		if (error == ENOMEM) {
			que->tx_dma_setup++;
			return (error);
		} else if (error != 0) {
			que->tx_dma_setup++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (error);
		}
	} else if (error == ENOMEM) {
		que->tx_dma_setup++;
		return (error);
	} else if (error != 0) {
		que->tx_dma_setup++;
		m_freem(*m_headp);
		*m_headp = NULL;
		return (error);
	}

	/* Make certain there are enough descriptors */
	if (nsegs > txr->avail - 2) {
		txr->no_desc++;
		error = ENOBUFS;
		goto xmit_fail;
	}
	m_head = *m_headp;

	/* Set up the TSO/CSUM offload */
	if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD) {
		error = ixl_tx_setup_offload(que, m_head, &cmd, &off);
		if (error)
			goto xmit_fail;
	}

	cmd |= I40E_TX_DESC_CMD_ICRC;
	/* Grab the VLAN tag */
	if (m_head->m_flags & M_VLANTAG) {
		cmd |= I40E_TX_DESC_CMD_IL2TAG1;
		vtag = htole16(m_head->m_pkthdr.ether_vtag);
	}

	i = txr->next_avail;
	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;

		buf = &txr->buffers[i];
		buf->tag = tag; /* Keep track of the type tag */
		txd = &txr->base[i];
		seglen = segs[j].ds_len;

		txd->buffer_addr = htole64(segs[j].ds_addr);
		txd->cmd_type_offset_bsz =
		    htole64(I40E_TX_DESC_DTYPE_DATA
		    | ((u64)cmd  << I40E_TXD_QW1_CMD_SHIFT)
		    | ((u64)off << I40E_TXD_QW1_OFFSET_SHIFT)
		    | ((u64)seglen  << I40E_TXD_QW1_TX_BUF_SZ_SHIFT)
		    | ((u64)vtag  << I40E_TXD_QW1_L2TAG1_SHIFT));

		last = i; /* descriptor that will get completion IRQ */

		if (++i == que->num_desc)
			i = 0;

		buf->m_head = NULL;
		buf->eop_index = -1;
	}
	/* Set the last descriptor for report */
	txd->cmd_type_offset_bsz |=
	    htole64(((u64)IXL_TXD_CMD << I40E_TXD_QW1_CMD_SHIFT));
	txr->avail -= nsegs;
	txr->next_avail = i;

	buf->m_head = m_head;
	/* Swap the dma map between the first and last descriptor */
	txr->buffers[first].map = buf->map;
	buf->map = map;
	bus_dmamap_sync(tag, map, BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        buf = &txr->buffers[first];
	buf->eop_index = last;

        bus_dmamap_sync(txr->dma.tag, txr->dma.map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets;
	wr32(hw, txr->tail, i);

	/* Mark outstanding work */
	if (que->busy == 0)
		que->busy = 1;
	return (0);

xmit_fail:
	bus_dmamap_unload(tag, buf->map);
	return (error);
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/
int
ixl_allocate_tx_data(struct ixl_queue *que)
{
	struct tx_ring		*txr = &que->txr;
	struct ixl_vsi		*vsi = que->vsi;
	device_t		dev = vsi->dev;
	struct ixl_tx_buf	*buf;
	int			error = 0;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IXL_TSO_SIZE,		/* maxsize */
			       IXL_MAX_TX_SEGS,		/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->tx_tag))) {
		device_printf(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	/* Make a special tag for TSO */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IXL_TSO_SIZE,		/* maxsize */
			       IXL_MAX_TSO_SEGS,	/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->tso_tag))) {
		device_printf(dev,"Unable to allocate TX TSO DMA tag\n");
		goto fail;
	}

	if (!(txr->buffers =
	    (struct ixl_tx_buf *) malloc(sizeof(struct ixl_tx_buf) *
	    que->num_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

        /* Create the descriptor buffer default dma maps */
	buf = txr->buffers;
	for (int i = 0; i < que->num_desc; i++, buf++) {
		buf->tag = txr->tx_tag;
		error = bus_dmamap_create(buf->tag, 0, &buf->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
	}
fail:
	return (error);
}


/*********************************************************************
 *
 *  (Re)Initialize a queue transmit ring.
 *	- called by init, it clears the descriptor ring,
 *	  and frees any stale mbufs 
 *
 **********************************************************************/
void
ixl_init_tx_ring(struct ixl_queue *que)
{
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(que->vsi->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */
	struct tx_ring		*txr = &que->txr;
	struct ixl_tx_buf	*buf;

	/* Clear the old ring contents */
	IXL_TX_LOCK(txr);

#ifdef DEV_NETMAP
	/*
	 * (under lock): if in netmap mode, do some consistency
	 * checks and set slot to entry 0 of the netmap ring.
	 */
	slot = netmap_reset(na, NR_TX, que->me, 0);
#endif /* DEV_NETMAP */

	bzero((void *)txr->base,
	      (sizeof(struct i40e_tx_desc)) * que->num_desc);

	/* Reset indices */
	txr->next_avail = 0;
	txr->next_to_clean = 0;

#ifdef IXL_FDIR
	/* Initialize flow director */
	txr->atr_rate = ixl_atr_rate;
	txr->atr_count = 0;
#endif

	/* Free any existing tx mbufs. */
        buf = txr->buffers;
	for (int i = 0; i < que->num_desc; i++, buf++) {
		if (buf->m_head != NULL) {
			bus_dmamap_sync(buf->tag, buf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(buf->tag, buf->map);
			m_freem(buf->m_head);
			buf->m_head = NULL;
		}
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, set the map for the packet buffer.
		 * NOTE: Some drivers (not this one) also need to set
		 * the physical buffer address in the NIC ring.
		 * netmap_idx_n2k() maps a nic index, i, into the corresponding
		 * netmap slot index, si
		 */
		if (slot) {
			int si = netmap_idx_n2k(&na->tx_rings[que->me], i);
			netmap_load_map(na, buf->tag, buf->map, NMB(na, slot + si));
		}
#endif /* DEV_NETMAP */
		/* Clear the EOP index */
		buf->eop_index = -1;
        }

	/* Set number of descriptors available */
	txr->avail = que->num_desc;

	bus_dmamap_sync(txr->dma.tag, txr->dma.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IXL_TX_UNLOCK(txr);
}


/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
void
ixl_free_que_tx(struct ixl_queue *que)
{
	struct tx_ring *txr = &que->txr;
	struct ixl_tx_buf *buf;

	INIT_DBG_IF(que->vsi->ifp, "queue %d: begin", que->me);

	for (int i = 0; i < que->num_desc; i++) {
		buf = &txr->buffers[i];
		if (buf->m_head != NULL) {
			bus_dmamap_sync(buf->tag, buf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(buf->tag,
			    buf->map);
			m_freem(buf->m_head);
			buf->m_head = NULL;
			if (buf->map != NULL) {
				bus_dmamap_destroy(buf->tag,
				    buf->map);
				buf->map = NULL;
			}
		} else if (buf->map != NULL) {
			bus_dmamap_unload(buf->tag,
			    buf->map);
			bus_dmamap_destroy(buf->tag,
			    buf->map);
			buf->map = NULL;
		}
	}
	if (txr->br != NULL)
		buf_ring_free(txr->br, M_DEVBUF);
	if (txr->buffers != NULL) {
		free(txr->buffers, M_DEVBUF);
		txr->buffers = NULL;
	}
	if (txr->tx_tag != NULL) {
		bus_dma_tag_destroy(txr->tx_tag);
		txr->tx_tag = NULL;
	}
	if (txr->tso_tag != NULL) {
		bus_dma_tag_destroy(txr->tso_tag);
		txr->tso_tag = NULL;
	}

	INIT_DBG_IF(que->vsi->ifp, "queue %d: end", que->me);
	return;
}

/*********************************************************************
 *
 *  Setup descriptor for hw offloads 
 *
 **********************************************************************/

static int
ixl_tx_setup_offload(struct ixl_queue *que,
    struct mbuf *mp, u32 *cmd, u32 *off)
{
	struct ether_vlan_header	*eh;
#ifdef INET
	struct ip			*ip = NULL;
#endif
	struct tcphdr			*th = NULL;
#ifdef INET6
	struct ip6_hdr			*ip6;
#endif
	int				elen, ip_hlen = 0, tcp_hlen;
	u16				etype;
	u8				ipproto = 0;
	bool				tso = FALSE;

	/* Set up the TSO context descriptor if required */
	if (mp->m_pkthdr.csum_flags & CSUM_TSO) {
		tso = ixl_tso_setup(que, mp);
		if (tso)
			++que->tso;
		else
			return (ENXIO);
	}

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		elen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		elen = ETHER_HDR_LEN;
	}

	switch (etype) {
#ifdef INET
		case ETHERTYPE_IP:
			ip = (struct ip *)(mp->m_data + elen);
			ip_hlen = ip->ip_hl << 2;
			ipproto = ip->ip_p;
			th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
			/* The IP checksum must be recalculated with TSO */
			if (tso)
				*cmd |= I40E_TX_DESC_CMD_IIPT_IPV4_CSUM;
			else
				*cmd |= I40E_TX_DESC_CMD_IIPT_IPV4;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + elen);
			ip_hlen = sizeof(struct ip6_hdr);
			ipproto = ip6->ip6_nxt;
			th = (struct tcphdr *)((caddr_t)ip6 + ip_hlen);
			*cmd |= I40E_TX_DESC_CMD_IIPT_IPV6;
			break;
#endif
		default:
			break;
	}

	*off |= (elen >> 1) << I40E_TX_DESC_LENGTH_MACLEN_SHIFT;
	*off |= (ip_hlen >> 2) << I40E_TX_DESC_LENGTH_IPLEN_SHIFT;

	switch (ipproto) {
		case IPPROTO_TCP:
			tcp_hlen = th->th_off << 2;
			if (mp->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_TCP_IPV6)) {
				*cmd |= I40E_TX_DESC_CMD_L4T_EOFT_TCP;
				*off |= (tcp_hlen >> 2) <<
				    I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
			}
#ifdef IXL_FDIR
			ixl_atr(que, th, etype);
#endif
			break;
		case IPPROTO_UDP:
			if (mp->m_pkthdr.csum_flags & (CSUM_UDP|CSUM_UDP_IPV6)) {
				*cmd |= I40E_TX_DESC_CMD_L4T_EOFT_UDP;
				*off |= (sizeof(struct udphdr) >> 2) <<
				    I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
			}
			break;

		case IPPROTO_SCTP:
			if (mp->m_pkthdr.csum_flags & (CSUM_SCTP|CSUM_SCTP_IPV6)) {
				*cmd |= I40E_TX_DESC_CMD_L4T_EOFT_SCTP;
				*off |= (sizeof(struct sctphdr) >> 2) <<
				    I40E_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
			}
			/* Fall Thru */
		default:
			break;
	}

        return (0);
}


/**********************************************************************
 *
 *  Setup context for hardware segmentation offload (TSO)
 *
 **********************************************************************/
static bool
ixl_tso_setup(struct ixl_queue *que, struct mbuf *mp)
{
	struct tx_ring			*txr = &que->txr;
	struct i40e_tx_context_desc	*TXD;
	struct ixl_tx_buf		*buf;
	u32				cmd, mss, type, tsolen;
	u16				etype;
	int				idx, elen, ip_hlen, tcp_hlen;
	struct ether_vlan_header	*eh;
#ifdef INET
	struct ip			*ip;
#endif
#ifdef INET6
	struct ip6_hdr			*ip6;
#endif
#if defined(INET6) || defined(INET)
	struct tcphdr			*th;
#endif
	u64				type_cmd_tso_mss;

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		elen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		etype = eh->evl_proto;
	} else {
		elen = ETHER_HDR_LEN;
		etype = eh->evl_encap_proto;
	}

        switch (ntohs(etype)) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + elen);
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return (ENXIO);
		ip_hlen = sizeof(struct ip6_hdr);
		th = (struct tcphdr *)((caddr_t)ip6 + ip_hlen);
		th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
		tcp_hlen = th->th_off << 2;
		/*
		 * The corresponding flag is set by the stack in the IPv4
		 * TSO case, but not in IPv6 (at least in FreeBSD 10.2).
		 * So, set it here because the rest of the flow requires it.
		 */
		mp->m_pkthdr.csum_flags |= CSUM_TCP_IPV6;
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		ip = (struct ip *)(mp->m_data + elen);
		if (ip->ip_p != IPPROTO_TCP)
			return (ENXIO);
		ip->ip_sum = 0;
		ip_hlen = ip->ip_hl << 2;
		th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
		th->th_sum = in_pseudo(ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		tcp_hlen = th->th_off << 2;
		break;
#endif
	default:
		printf("%s: CSUM_TSO but no supported IP version (0x%04x)",
		    __func__, ntohs(etype));
		return FALSE;
        }

        /* Ensure we have at least the IP+TCP header in the first mbuf. */
        if (mp->m_len < elen + ip_hlen + sizeof(struct tcphdr))
		return FALSE;

	idx = txr->next_avail;
	buf = &txr->buffers[idx];
	TXD = (struct i40e_tx_context_desc *) &txr->base[idx];
	tsolen = mp->m_pkthdr.len - (elen + ip_hlen + tcp_hlen);

	type = I40E_TX_DESC_DTYPE_CONTEXT;
	cmd = I40E_TX_CTX_DESC_TSO;
	mss = mp->m_pkthdr.tso_segsz;

	type_cmd_tso_mss = ((u64)type << I40E_TXD_CTX_QW1_DTYPE_SHIFT) |
	    ((u64)cmd << I40E_TXD_CTX_QW1_CMD_SHIFT) |
	    ((u64)tsolen << I40E_TXD_CTX_QW1_TSO_LEN_SHIFT) |
	    ((u64)mss << I40E_TXD_CTX_QW1_MSS_SHIFT);
	TXD->type_cmd_tso_mss = htole64(type_cmd_tso_mss);

	TXD->tunneling_params = htole32(0);
	buf->m_head = NULL;
	buf->eop_index = -1;

	if (++idx == que->num_desc)
		idx = 0;

	txr->avail--;
	txr->next_avail = idx;

	return TRUE;
}

/*             
** ixl_get_tx_head - Retrieve the value from the 
**    location the HW records its HEAD index
*/
static inline u32
ixl_get_tx_head(struct ixl_queue *que)
{
	struct tx_ring  *txr = &que->txr;
	void *head = &txr->base[que->num_desc];
	return LE32_TO_CPU(*(volatile __le32 *)head);
}

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
bool
ixl_txeof(struct ixl_queue *que)
{
	struct tx_ring		*txr = &que->txr;
	u32			first, last, head, done, processed;
	struct ixl_tx_buf	*buf;
	struct i40e_tx_desc	*tx_desc, *eop_desc;


	mtx_assert(&txr->mtx, MA_OWNED);

#ifdef DEV_NETMAP
	// XXX todo: implement moderation
	if (netmap_tx_irq(que->vsi->ifp, que->me))
		return FALSE;
#endif /* DEF_NETMAP */

	/* These are not the descriptors you seek, move along :) */
	if (txr->avail == que->num_desc) {
		que->busy = 0;
		return FALSE;
	}

	processed = 0;
	first = txr->next_to_clean;
	buf = &txr->buffers[first];
	tx_desc = (struct i40e_tx_desc *)&txr->base[first];
	last = buf->eop_index;
	if (last == -1)
		return FALSE;
	eop_desc = (struct i40e_tx_desc *)&txr->base[last];

	/* Get the Head WB value */
	head = ixl_get_tx_head(que);

	/*
	** Get the index of the first descriptor
	** BEYOND the EOP and call that 'done'.
	** I do this so the comparison in the
	** inner while loop below can be simple
	*/
	if (++last == que->num_desc) last = 0;
	done = last;

        bus_dmamap_sync(txr->dma.tag, txr->dma.map,
            BUS_DMASYNC_POSTREAD);
	/*
	** The HEAD index of the ring is written in a 
	** defined location, this rather than a done bit
	** is what is used to keep track of what must be
	** 'cleaned'.
	*/
	while (first != head) {
		/* We clean the range of the packet */
		while (first != done) {
			++txr->avail;
			++processed;

			if (buf->m_head) {
				txr->bytes += /* for ITR adjustment */
				    buf->m_head->m_pkthdr.len;
				txr->tx_bytes += /* for TX stats */
				    buf->m_head->m_pkthdr.len;
				bus_dmamap_sync(buf->tag,
				    buf->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(buf->tag,
				    buf->map);
				m_freem(buf->m_head);
				buf->m_head = NULL;
				buf->map = NULL;
			}
			buf->eop_index = -1;

			if (++first == que->num_desc)
				first = 0;

			buf = &txr->buffers[first];
			tx_desc = &txr->base[first];
		}
		++txr->packets;
		/* See if there is more work now */
		last = buf->eop_index;
		if (last != -1) {
			eop_desc = &txr->base[last];
			/* Get next done point */
			if (++last == que->num_desc) last = 0;
			done = last;
		} else
			break;
	}
	bus_dmamap_sync(txr->dma.tag, txr->dma.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	txr->next_to_clean = first;


	/*
	** Hang detection, we know there's
	** work outstanding or the first return
	** would have been taken, so indicate an
	** unsuccessful pass, in local_timer if
	** the value is too great the queue will
	** be considered hung. If anything has been
	** cleaned then reset the state.
	*/
	if ((processed == 0) && (que->busy != IXL_QUEUE_HUNG))
		++que->busy;

	if (processed)
		que->busy = 1; /* Note this turns off HUNG */

	/*
	 * If there are no pending descriptors, clear the timeout.
	 */
	if (txr->avail == que->num_desc) {
		que->busy = 0;
		return FALSE;
	}

	return TRUE;
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
ixl_refresh_mbufs(struct ixl_queue *que, int limit)
{
	struct ixl_vsi		*vsi = que->vsi;
	struct rx_ring		*rxr = &que->rxr;
	bus_dma_segment_t	hseg[1];
	bus_dma_segment_t	pseg[1];
	struct ixl_rx_buf	*buf;
	struct mbuf		*mh, *mp;
	int			i, j, nsegs, error;
	bool			refreshed = FALSE;

	i = j = rxr->next_refresh;
	/* Control the loop with one beyond */
	if (++j == que->num_desc)
		j = 0;

	while (j != limit) {
		buf = &rxr->buffers[i];
		if (rxr->hdr_split == FALSE)
			goto no_split;

		if (buf->m_head == NULL) {
			mh = m_gethdr(M_NOWAIT, MT_DATA);
			if (mh == NULL)
				goto update;
		} else
			mh = buf->m_head;

		mh->m_pkthdr.len = mh->m_len = MHLEN;
		mh->m_len = MHLEN;
		mh->m_flags |= M_PKTHDR;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->htag,
		    buf->hmap, mh, hseg, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("Refresh mbufs: hdr dmamap load"
			    " failure - %d\n", error);
			m_free(mh);
			buf->m_head = NULL;
			goto update;
		}
		buf->m_head = mh;
		bus_dmamap_sync(rxr->htag, buf->hmap,
		    BUS_DMASYNC_PREREAD);
		rxr->base[i].read.hdr_addr =
		   htole64(hseg[0].ds_addr);

no_split:
		if (buf->m_pack == NULL) {
			mp = m_getjcl(M_NOWAIT, MT_DATA,
			    M_PKTHDR, rxr->mbuf_sz);
			if (mp == NULL)
				goto update;
		} else
			mp = buf->m_pack;

		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->ptag,
		    buf->pmap, mp, pseg, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("Refresh mbufs: payload dmamap load"
			    " failure - %d\n", error);
			m_free(mp);
			buf->m_pack = NULL;
			goto update;
		}
		buf->m_pack = mp;
		bus_dmamap_sync(rxr->ptag, buf->pmap,
		    BUS_DMASYNC_PREREAD);
		rxr->base[i].read.pkt_addr =
		   htole64(pseg[0].ds_addr);
		/* Used only when doing header split */
		rxr->base[i].read.hdr_addr = 0;

		refreshed = TRUE;
		/* Next is precalculated */
		i = j;
		rxr->next_refresh = i;
		if (++j == que->num_desc)
			j = 0;
	}
update:
	if (refreshed) /* Update hardware tail index */
		wr32(vsi->hw, rxr->tail, rxr->next_refresh);
	return;
}


/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one
 *  rx_buffer per descriptor, the maximum number of rx_buffer's
 *  that we'll need is equal to the number of receive descriptors
 *  that we've defined.
 *
 **********************************************************************/
int
ixl_allocate_rx_data(struct ixl_queue *que)
{
	struct rx_ring		*rxr = &que->rxr;
	struct ixl_vsi		*vsi = que->vsi;
	device_t 		dev = vsi->dev;
	struct ixl_rx_buf 	*buf;
	int             	i, bsize, error;

	bsize = sizeof(struct ixl_rx_buf) * que->num_desc;
	if (!(rxr->buffers =
	    (struct ixl_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		return (error);
	}

	if ((error = bus_dma_tag_create(NULL,	/* parent */
				   1, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MSIZE,		/* maxsize */
				   1,			/* nsegments */
				   MSIZE,		/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->htag))) {
		device_printf(dev, "Unable to create RX DMA htag\n");
		return (error);
	}

	if ((error = bus_dma_tag_create(NULL,	/* parent */
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
		device_printf(dev, "Unable to create RX DMA ptag\n");
		return (error);
	}

	for (i = 0; i < que->num_desc; i++) {
		buf = &rxr->buffers[i];
		error = bus_dmamap_create(rxr->htag,
		    BUS_DMA_NOWAIT, &buf->hmap);
		if (error) {
			device_printf(dev, "Unable to create RX head map\n");
			break;
		}
		error = bus_dmamap_create(rxr->ptag,
		    BUS_DMA_NOWAIT, &buf->pmap);
		if (error) {
			device_printf(dev, "Unable to create RX pkt map\n");
			break;
		}
	}

	return (error);
}


/*********************************************************************
 *
 *  (Re)Initialize the queue receive ring and its buffers.
 *
 **********************************************************************/
int
ixl_init_rx_ring(struct ixl_queue *que)
{
	struct	rx_ring 	*rxr = &que->rxr;
	struct ixl_vsi		*vsi = que->vsi;
#if defined(INET6) || defined(INET)
	struct ifnet		*ifp = vsi->ifp;
	struct lro_ctrl		*lro = &rxr->lro;
#endif
	struct ixl_rx_buf	*buf;
	bus_dma_segment_t	pseg[1], hseg[1];
	int			rsize, nsegs, error = 0;
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(que->vsi->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */

	IXL_RX_LOCK(rxr);
#ifdef DEV_NETMAP
	/* same as in ixl_init_tx_ring() */
	slot = netmap_reset(na, NR_RX, que->me, 0);
#endif /* DEV_NETMAP */
	/* Clear the ring contents */
	rsize = roundup2(que->num_desc *
	    sizeof(union i40e_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->base, rsize);
	/* Cleanup any existing buffers */
	for (int i = 0; i < que->num_desc; i++) {
		buf = &rxr->buffers[i];
		if (buf->m_head != NULL) {
			bus_dmamap_sync(rxr->htag, buf->hmap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->htag, buf->hmap);
			buf->m_head->m_flags |= M_PKTHDR;
			m_freem(buf->m_head);
		}
		if (buf->m_pack != NULL) {
			bus_dmamap_sync(rxr->ptag, buf->pmap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->ptag, buf->pmap);
			buf->m_pack->m_flags |= M_PKTHDR;
			m_freem(buf->m_pack);
		}
		buf->m_head = NULL;
		buf->m_pack = NULL;
	}

	/* header split is off */
	rxr->hdr_split = FALSE;

	/* Now replenish the mbufs */
	for (int j = 0; j != que->num_desc; ++j) {
		struct mbuf	*mh, *mp;

		buf = &rxr->buffers[j];
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, fill the map and set the buffer
		 * address in the NIC ring, considering the offset
		 * between the netmap and NIC rings (see comment in
		 * ixgbe_setup_transmit_ring() ). No need to allocate
		 * an mbuf, so end the block with a continue;
		 */
		if (slot) {
			int sj = netmap_idx_n2k(&na->rx_rings[que->me], j);
			uint64_t paddr;
			void *addr;

			addr = PNMB(na, slot + sj, &paddr);
			netmap_load_map(na, rxr->dma.tag, buf->pmap, addr);
			/* Update descriptor and the cached value */
			rxr->base[j].read.pkt_addr = htole64(paddr);
			rxr->base[j].read.hdr_addr = 0;
			continue;
		}
#endif /* DEV_NETMAP */
		/*
		** Don't allocate mbufs if not
		** doing header split, its wasteful
		*/ 
		if (rxr->hdr_split == FALSE)
			goto skip_head;

		/* First the header */
		buf->m_head = m_gethdr(M_NOWAIT, MT_DATA);
		if (buf->m_head == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m_adj(buf->m_head, ETHER_ALIGN);
		mh = buf->m_head;
		mh->m_len = mh->m_pkthdr.len = MHLEN;
		mh->m_flags |= M_PKTHDR;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->htag,
		    buf->hmap, buf->m_head, hseg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) /* Nothing elegant to do here */
			goto fail;
		bus_dmamap_sync(rxr->htag,
		    buf->hmap, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->base[j].read.hdr_addr = htole64(hseg[0].ds_addr);

skip_head:
		/* Now the payload cluster */
		buf->m_pack = m_getjcl(M_NOWAIT, MT_DATA,
		    M_PKTHDR, rxr->mbuf_sz);
		if (buf->m_pack == NULL) {
			error = ENOBUFS;
                        goto fail;
		}
		mp = buf->m_pack;
		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->ptag,
		    buf->pmap, mp, pseg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0)
                        goto fail;
		bus_dmamap_sync(rxr->ptag,
		    buf->pmap, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->base[j].read.pkt_addr = htole64(pseg[0].ds_addr);
		rxr->base[j].read.hdr_addr = 0;
	}


	/* Setup our descriptor indices */
	rxr->next_check = 0;
	rxr->next_refresh = 0;
	rxr->lro_enabled = FALSE;
	rxr->split = 0;
	rxr->bytes = 0;
	rxr->discard = FALSE;

	wr32(vsi->hw, rxr->tail, que->num_desc - 1);
	ixl_flush(vsi->hw);

#if defined(INET6) || defined(INET)
	/*
	** Now set up the LRO interface:
	*/
	if (ifp->if_capenable & IFCAP_LRO) {
		int err = tcp_lro_init(lro);
		if (err) {
			if_printf(ifp, "queue %d: LRO Initialization failed!\n", que->me);
			goto fail;
		}
		INIT_DBG_IF(ifp, "queue %d: RX Soft LRO Initialized", que->me);
		rxr->lro_enabled = TRUE;
		lro->ifp = vsi->ifp;
	}
#endif

	bus_dmamap_sync(rxr->dma.tag, rxr->dma.map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

fail:
	IXL_RX_UNLOCK(rxr);
	return (error);
}


/*********************************************************************
 *
 *  Free station receive ring data structures
 *
 **********************************************************************/
void
ixl_free_que_rx(struct ixl_queue *que)
{
	struct rx_ring		*rxr = &que->rxr;
	struct ixl_rx_buf	*buf;

	INIT_DBG_IF(que->vsi->ifp, "queue %d: begin", que->me);

	/* Cleanup any existing buffers */
	if (rxr->buffers != NULL) {
		for (int i = 0; i < que->num_desc; i++) {
			buf = &rxr->buffers[i];
			if (buf->m_head != NULL) {
				bus_dmamap_sync(rxr->htag, buf->hmap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->htag, buf->hmap);
				buf->m_head->m_flags |= M_PKTHDR;
				m_freem(buf->m_head);
			}
			if (buf->m_pack != NULL) {
				bus_dmamap_sync(rxr->ptag, buf->pmap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->ptag, buf->pmap);
				buf->m_pack->m_flags |= M_PKTHDR;
				m_freem(buf->m_pack);
			}
			buf->m_head = NULL;
			buf->m_pack = NULL;
			if (buf->hmap != NULL) {
				bus_dmamap_destroy(rxr->htag, buf->hmap);
				buf->hmap = NULL;
			}
			if (buf->pmap != NULL) {
				bus_dmamap_destroy(rxr->ptag, buf->pmap);
				buf->pmap = NULL;
			}
		}
		if (rxr->buffers != NULL) {
			free(rxr->buffers, M_DEVBUF);
			rxr->buffers = NULL;
		}
	}

	if (rxr->htag != NULL) {
		bus_dma_tag_destroy(rxr->htag);
		rxr->htag = NULL;
	}
	if (rxr->ptag != NULL) {
		bus_dma_tag_destroy(rxr->ptag);
		rxr->ptag = NULL;
	}

	INIT_DBG_IF(que->vsi->ifp, "queue %d: end", que->me);
	return;
}

static __inline void
ixl_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u8 ptype)
{

#if defined(INET6) || defined(INET)
        /*
         * ATM LRO is only for IPv4/TCP packets and TCP checksum of the packet
         * should be computed by hardware. Also it should not have VLAN tag in
         * ethernet header.
         */
        if (rxr->lro_enabled &&
            (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0 &&
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
#endif
	IXL_RX_UNLOCK(rxr);
        (*ifp->if_input)(ifp, m);
	IXL_RX_LOCK(rxr);
}


static __inline void
ixl_rx_discard(struct rx_ring *rxr, int i)
{
	struct ixl_rx_buf	*rbuf;

	rbuf = &rxr->buffers[i];

        if (rbuf->fmp != NULL) {/* Partial chain ? */
		rbuf->fmp->m_flags |= M_PKTHDR;
                m_freem(rbuf->fmp);
                rbuf->fmp = NULL;
	}

	/*
	** With advanced descriptors the writeback
	** clobbers the buffer addrs, so its easier
	** to just free the existing mbufs and take
	** the normal refresh path to get new buffers
	** and mapping.
	*/
	if (rbuf->m_head) {
		m_free(rbuf->m_head);
		rbuf->m_head = NULL;
	}
 
	if (rbuf->m_pack) {
		m_free(rbuf->m_pack);
		rbuf->m_pack = NULL;
	}

	return;
}

#ifdef RSS
/*
** i40e_ptype_to_hash: parse the packet type
** to determine the appropriate hash.
*/
static inline int
ixl_ptype_to_hash(u8 ptype)
{
        struct i40e_rx_ptype_decoded	decoded;
	u8				ex = 0;

	decoded = decode_rx_desc_ptype(ptype);
	ex = decoded.outer_frag;

	if (!decoded.known)
		return M_HASHTYPE_OPAQUE_HASH;

	if (decoded.outer_ip == I40E_RX_PTYPE_OUTER_L2) 
		return M_HASHTYPE_OPAQUE_HASH;

	/* Note: anything that gets to this point is IP */
        if (decoded.outer_ip_ver == I40E_RX_PTYPE_OUTER_IPV6) { 
		switch (decoded.inner_prot) {
			case I40E_RX_PTYPE_INNER_PROT_TCP:
				if (ex)
					return M_HASHTYPE_RSS_TCP_IPV6_EX;
				else
					return M_HASHTYPE_RSS_TCP_IPV6;
			case I40E_RX_PTYPE_INNER_PROT_UDP:
				if (ex)
					return M_HASHTYPE_RSS_UDP_IPV6_EX;
				else
					return M_HASHTYPE_RSS_UDP_IPV6;
			default:
				if (ex)
					return M_HASHTYPE_RSS_IPV6_EX;
				else
					return M_HASHTYPE_RSS_IPV6;
		}
	}
        if (decoded.outer_ip_ver == I40E_RX_PTYPE_OUTER_IPV4) { 
		switch (decoded.inner_prot) {
			case I40E_RX_PTYPE_INNER_PROT_TCP:
					return M_HASHTYPE_RSS_TCP_IPV4;
			case I40E_RX_PTYPE_INNER_PROT_UDP:
				if (ex)
					return M_HASHTYPE_RSS_UDP_IPV4_EX;
				else
					return M_HASHTYPE_RSS_UDP_IPV4;
			default:
					return M_HASHTYPE_RSS_IPV4;
		}
	}
	/* We should never get here!! */
	return M_HASHTYPE_OPAQUE_HASH;
}
#endif /* RSS */

/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *  We loop at most count times if count is > 0, or until done if
 *  count < 0.
 *
 *  Return TRUE for more work, FALSE for all clean.
 *********************************************************************/
bool
ixl_rxeof(struct ixl_queue *que, int count)
{
	struct ixl_vsi		*vsi = que->vsi;
	struct rx_ring		*rxr = &que->rxr;
	struct ifnet		*ifp = vsi->ifp;
#if defined(INET6) || defined(INET)
	struct lro_ctrl		*lro = &rxr->lro;
#endif
	int			i, nextp, processed = 0;
	union i40e_rx_desc	*cur;
	struct ixl_rx_buf	*rbuf, *nbuf;


	IXL_RX_LOCK(rxr);

#ifdef DEV_NETMAP
	if (netmap_rx_irq(ifp, que->me, &count)) {
		IXL_RX_UNLOCK(rxr);
		return (FALSE);
	}
#endif /* DEV_NETMAP */

	for (i = rxr->next_check; count != 0;) {
		struct mbuf	*sendmp, *mh, *mp;
		u32		rsc, status, error;
		u16		hlen, plen, vtag;
		u64		qword;
		u8		ptype;
		bool		eop;
 
		/* Sync the ring. */
		bus_dmamap_sync(rxr->dma.tag, rxr->dma.map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->base[i];
		qword = le64toh(cur->wb.qword1.status_error_len);
		status = (qword & I40E_RXD_QW1_STATUS_MASK)
		    >> I40E_RXD_QW1_STATUS_SHIFT;
		error = (qword & I40E_RXD_QW1_ERROR_MASK)
		    >> I40E_RXD_QW1_ERROR_SHIFT;
		plen = (qword & I40E_RXD_QW1_LENGTH_PBUF_MASK)
		    >> I40E_RXD_QW1_LENGTH_PBUF_SHIFT;
		hlen = (qword & I40E_RXD_QW1_LENGTH_HBUF_MASK)
		    >> I40E_RXD_QW1_LENGTH_HBUF_SHIFT;
		ptype = (qword & I40E_RXD_QW1_PTYPE_MASK)
		    >> I40E_RXD_QW1_PTYPE_SHIFT;

		if ((status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)) == 0) {
			++rxr->not_done;
			break;
		}
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		count--;
		sendmp = NULL;
		nbuf = NULL;
		rsc = 0;
		cur->wb.qword1.status_error_len = 0;
		rbuf = &rxr->buffers[i];
		mh = rbuf->m_head;
		mp = rbuf->m_pack;
		eop = (status & (1 << I40E_RX_DESC_STATUS_EOF_SHIFT));
		if (status & (1 << I40E_RX_DESC_STATUS_L2TAG1P_SHIFT))
			vtag = le16toh(cur->wb.qword0.lo_dword.l2tag1);
		else
			vtag = 0;

		/*
		** Make sure bad packets are discarded,
		** note that only EOP descriptor has valid
		** error results.
		*/
                if (eop && (error & (1 << I40E_RX_DESC_ERROR_RXE_SHIFT))) {
			rxr->desc_errs++;
			ixl_rx_discard(rxr, i);
			goto next_desc;
		}

		/* Prefetch the next buffer */
		if (!eop) {
			nextp = i + 1;
			if (nextp == que->num_desc)
				nextp = 0;
			nbuf = &rxr->buffers[nextp];
			prefetch(nbuf);
		}

		/*
		** The header mbuf is ONLY used when header 
		** split is enabled, otherwise we get normal 
		** behavior, ie, both header and payload
		** are DMA'd into the payload buffer.
		**
		** Rather than using the fmp/lmp global pointers
		** we now keep the head of a packet chain in the
		** buffer struct and pass this along from one
		** descriptor to the next, until we get EOP.
		*/
		if (rxr->hdr_split && (rbuf->fmp == NULL)) {
			if (hlen > IXL_RX_HDR)
				hlen = IXL_RX_HDR;
			mh->m_len = hlen;
			mh->m_flags |= M_PKTHDR;
			mh->m_next = NULL;
			mh->m_pkthdr.len = mh->m_len;
			/* Null buf pointer so it is refreshed */
			rbuf->m_head = NULL;
			/*
			** Check the payload length, this
			** could be zero if its a small
			** packet.
			*/
			if (plen > 0) {
				mp->m_len = plen;
				mp->m_next = NULL;
				mp->m_flags &= ~M_PKTHDR;
				mh->m_next = mp;
				mh->m_pkthdr.len += mp->m_len;
				/* Null buf pointer so it is refreshed */
				rbuf->m_pack = NULL;
				rxr->split++;
			}
			/*
			** Now create the forward
			** chain so when complete 
			** we wont have to.
			*/
                        if (eop == 0) {
				/* stash the chain head */
                                nbuf->fmp = mh;
				/* Make forward chain */
                                if (plen)
                                        mp->m_next = nbuf->m_pack;
                                else
                                        mh->m_next = nbuf->m_pack;
                        } else {
				/* Singlet, prepare to send */
                                sendmp = mh;
                                if (vtag) {
                                        sendmp->m_pkthdr.ether_vtag = vtag;
                                        sendmp->m_flags |= M_VLANTAG;
                                }
                        }
		} else {
			/*
			** Either no header split, or a
			** secondary piece of a fragmented
			** split packet.
			*/
			mp->m_len = plen;
			/*
			** See if there is a stored head
			** that determines what we are
			*/
			sendmp = rbuf->fmp;
			rbuf->m_pack = rbuf->fmp = NULL;

			if (sendmp != NULL) /* secondary frag */
				sendmp->m_pkthdr.len += mp->m_len;
			else {
				/* first desc of a non-ps chain */
				sendmp = mp;
				sendmp->m_flags |= M_PKTHDR;
				sendmp->m_pkthdr.len = mp->m_len;
				if (vtag) {
					sendmp->m_pkthdr.ether_vtag = vtag;
					sendmp->m_flags |= M_VLANTAG;
				}
                        }
			/* Pass the head pointer on */
			if (eop == 0) {
				nbuf->fmp = sendmp;
				sendmp = NULL;
				mp->m_next = nbuf->m_pack;
			}
		}
		++processed;
		/* Sending this frame? */
		if (eop) {
			sendmp->m_pkthdr.rcvif = ifp;
			/* gather stats */
			rxr->rx_packets++;
			rxr->rx_bytes += sendmp->m_pkthdr.len;
			/* capture data for dynamic ITR adjustment */
			rxr->packets++;
			rxr->bytes += sendmp->m_pkthdr.len;
			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
				ixl_rx_checksum(sendmp, status, error, ptype);
#ifdef RSS
			sendmp->m_pkthdr.flowid =
			    le32toh(cur->wb.qword0.hi_dword.rss);
			M_HASHTYPE_SET(sendmp, ixl_ptype_to_hash(ptype));
#else
			sendmp->m_pkthdr.flowid = que->msix;
			M_HASHTYPE_SET(sendmp, M_HASHTYPE_OPAQUE);
#endif
		}
next_desc:
		bus_dmamap_sync(rxr->dma.tag, rxr->dma.map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == que->num_desc)
			i = 0;

		/* Now send to the stack or do LRO */
		if (sendmp != NULL) {
			rxr->next_check = i;
			ixl_rx_input(rxr, ifp, sendmp, ptype);
			i = rxr->next_check;
		}

               /* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
			ixl_refresh_mbufs(que, i);
			processed = 0;
		}
	}

	/* Refresh any remaining buf structs */
	if (ixl_rx_unrefreshed(que))
		ixl_refresh_mbufs(que, i);

	rxr->next_check = i;

#if defined(INET6) || defined(INET)
	/*
	 * Flush any outstanding LRO work
	 */
	tcp_lro_flush_all(lro);
#endif

	IXL_RX_UNLOCK(rxr);
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
ixl_rx_checksum(struct mbuf * mp, u32 status, u32 error, u8 ptype)
{
	struct i40e_rx_ptype_decoded decoded;

	decoded = decode_rx_desc_ptype(ptype);

	/* Errors? */
 	if (error & ((1 << I40E_RX_DESC_ERROR_IPE_SHIFT) |
	    (1 << I40E_RX_DESC_ERROR_L4E_SHIFT))) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}

	/* IPv6 with extension headers likely have bad csum */
	if (decoded.outer_ip == I40E_RX_PTYPE_OUTER_IP &&
	    decoded.outer_ip_ver == I40E_RX_PTYPE_OUTER_IPV6)
		if (status &
		    (1 << I40E_RX_DESC_STATUS_IPV6EXADD_SHIFT)) {
			mp->m_pkthdr.csum_flags = 0;
			return;
		}

 
	/* IP Checksum Good */
	mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
	mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

	if (status & (1 << I40E_RX_DESC_STATUS_L3L4P_SHIFT)) {
		mp->m_pkthdr.csum_flags |= 
		    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
		mp->m_pkthdr.csum_data |= htons(0xffff);
	}
	return;
}

#if __FreeBSD_version >= 1100000
uint64_t
ixl_get_counter(if_t ifp, ift_counter cnt)
{
	struct ixl_vsi *vsi;

	vsi = if_getsoftc(ifp);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (vsi->ipackets);
	case IFCOUNTER_IERRORS:
		return (vsi->ierrors);
	case IFCOUNTER_OPACKETS:
		return (vsi->opackets);
	case IFCOUNTER_OERRORS:
		return (vsi->oerrors);
	case IFCOUNTER_COLLISIONS:
		/* Collisions are by standard impossible in 40G/10G Ethernet */
		return (0);
	case IFCOUNTER_IBYTES:
		return (vsi->ibytes);
	case IFCOUNTER_OBYTES:
		return (vsi->obytes);
	case IFCOUNTER_IMCASTS:
		return (vsi->imcasts);
	case IFCOUNTER_OMCASTS:
		return (vsi->omcasts);
	case IFCOUNTER_IQDROPS:
		return (vsi->iqdrops);
	case IFCOUNTER_OQDROPS:
		return (vsi->oqdrops);
	case IFCOUNTER_NOPROTO:
		return (vsi->noproto);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}
#endif

