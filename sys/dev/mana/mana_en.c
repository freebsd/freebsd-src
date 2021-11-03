/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/eventhandler.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/in_cksum.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#ifdef RSS
#include <net/rss_config.h>
#endif

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "mana.h"
#include "mana_sysctl.h"

static int mana_up(struct mana_port_context *apc);
static int mana_down(struct mana_port_context *apc);

static void
mana_rss_key_fill(void *k, size_t size)
{
	static bool rss_key_generated = false;
	static uint8_t rss_key[MANA_HASH_KEY_SIZE];

	KASSERT(size <= MANA_HASH_KEY_SIZE,
	    ("Request more buytes than MANA RSS key can hold"));

	if (!rss_key_generated) {
		arc4random_buf(rss_key, MANA_HASH_KEY_SIZE);
		rss_key_generated = true;
	}
	memcpy(k, rss_key, size);
}

static int
mana_ifmedia_change(struct ifnet *ifp __unused)
{
	return EOPNOTSUPP;
}

static void
mana_ifmedia_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mana_port_context *apc = if_getsoftc(ifp);

	if (!apc) {
		if_printf(ifp, "Port not available\n");
		return;
	}

	MANA_APC_LOCK_LOCK(apc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!apc->port_is_up) {
		MANA_APC_LOCK_UNLOCK(apc);
		mana_dbg(NULL, "Port %u link is down\n", apc->port_idx);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_100G_DR | IFM_FDX;

	MANA_APC_LOCK_UNLOCK(apc);
}

static uint64_t
mana_get_counter(struct ifnet *ifp, ift_counter cnt)
{
	struct mana_port_context *apc = if_getsoftc(ifp);
	struct mana_port_stats *stats = &apc->port_stats;

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (counter_u64_fetch(stats->rx_packets));
	case IFCOUNTER_OPACKETS:
		return (counter_u64_fetch(stats->tx_packets));
	case IFCOUNTER_IBYTES:
		return (counter_u64_fetch(stats->rx_bytes));
	case IFCOUNTER_OBYTES:
		return (counter_u64_fetch(stats->tx_bytes));
	case IFCOUNTER_IQDROPS:
		return (counter_u64_fetch(stats->rx_drops));
	case IFCOUNTER_OQDROPS:
		return (counter_u64_fetch(stats->tx_drops));
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static void
mana_qflush(struct ifnet *ifp)
{
	if_qflush(ifp);
}

int
mana_restart(struct mana_port_context *apc)
{
	int rc = 0;

	MANA_APC_LOCK_LOCK(apc);
	if (apc->port_is_up)
		 mana_down(apc);

	rc = mana_up(apc);
	MANA_APC_LOCK_UNLOCK(apc);

	return (rc);
}

static int
mana_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct mana_port_context *apc = if_getsoftc(ifp);
	struct ifrsskey *ifrk;
	struct ifrsshash *ifrh;
	struct ifreq *ifr;
	uint16_t new_mtu;
	int rc = 0;

	switch (command) {
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;
		new_mtu = ifr->ifr_mtu;
		if (ifp->if_mtu == new_mtu)
			break;
		if ((new_mtu + 18 > MAX_FRAME_SIZE) ||
		    (new_mtu + 18 < MIN_FRAME_SIZE)) {
			if_printf(ifp, "Invalid MTU. new_mtu: %d, "
			    "max allowed: %d, min allowed: %d\n",
			    new_mtu, MAX_FRAME_SIZE - 18, MIN_FRAME_SIZE - 18);
			return EINVAL;
		}
		MANA_APC_LOCK_LOCK(apc);
		if (apc->port_is_up)
			mana_down(apc);

		apc->frame_size = new_mtu + 18;
		if_setmtu(ifp, new_mtu);
		mana_dbg(NULL, "Set MTU to %d\n", new_mtu);

		rc = mana_up(apc);
		MANA_APC_LOCK_UNLOCK(apc);
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
				MANA_APC_LOCK_LOCK(apc);
				if (!apc->port_is_up)
					rc = mana_up(apc);
				MANA_APC_LOCK_UNLOCK(apc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				MANA_APC_LOCK_LOCK(apc);
				if (apc->port_is_up)
					mana_down(apc);
				MANA_APC_LOCK_UNLOCK(apc);
			}
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
	case SIOCGIFXMEDIA:
		ifr = (struct ifreq *)data;
		rc = ifmedia_ioctl(ifp, ifr, &apc->media, command);
		break;

	case SIOCGIFRSSKEY:
		ifrk = (struct ifrsskey *)data;
		ifrk->ifrk_func = RSS_FUNC_TOEPLITZ;
		ifrk->ifrk_keylen = MANA_HASH_KEY_SIZE;
		memcpy(ifrk->ifrk_key, apc->hashkey, MANA_HASH_KEY_SIZE);
		break;

	case SIOCGIFRSSHASH:
		ifrh = (struct ifrsshash *)data;
		ifrh->ifrh_func = RSS_FUNC_TOEPLITZ;
		ifrh->ifrh_types =
		    RSS_TYPE_TCP_IPV4 |
		    RSS_TYPE_UDP_IPV4 |
		    RSS_TYPE_TCP_IPV6 |
		    RSS_TYPE_UDP_IPV6;
		break;

	default:
		rc = ether_ioctl(ifp, command, data);
		break;
	}

	return (rc);
}

static inline void
mana_alloc_counters(counter_u64_t *begin, int size)
{
	counter_u64_t *end = (counter_u64_t *)((char *)begin + size);

	for (; begin < end; ++begin)
		*begin = counter_u64_alloc(M_WAITOK);
}

static inline void
mana_free_counters(counter_u64_t *begin, int size)
{
	counter_u64_t *end = (counter_u64_t *)((char *)begin + size);

	for (; begin < end; ++begin)
		counter_u64_free(*begin);
}

static bool
mana_can_tx(struct gdma_queue *wq)
{
	return mana_gd_wq_avail_space(wq) >= MAX_TX_WQE_SIZE;
}

static inline int
mana_tx_map_mbuf(struct mana_port_context *apc,
    struct mana_send_buf_info *tx_info,
    struct mbuf **m_head, struct mana_tx_package *tp,
    struct mana_stats *tx_stats)
{
	struct gdma_dev *gd = apc->ac->gdma_dev;
	bus_dma_segment_t segs[MAX_MBUF_FRAGS];
	struct mbuf *m = *m_head;
	int err, nsegs, i;

	err = bus_dmamap_load_mbuf_sg(apc->tx_buf_tag, tx_info->dma_map,
	    m, segs, &nsegs, BUS_DMA_NOWAIT);
	if (err == EFBIG) {
		struct mbuf *m_new;

		counter_u64_add(tx_stats->collapse, 1);
		m_new = m_collapse(m, M_NOWAIT, MAX_MBUF_FRAGS);
		if (unlikely(m_new == NULL)) {
			counter_u64_add(tx_stats->collapse_err, 1);
			return ENOBUFS;
		} else {
			*m_head = m = m_new;
		}

		mana_warn(NULL,
		    "Too many segs in orig mbuf, m_collapse called\n");

		err = bus_dmamap_load_mbuf_sg(apc->tx_buf_tag,
		    tx_info->dma_map, m, segs, &nsegs, BUS_DMA_NOWAIT);
	}
	if (!err) {
		for (i = 0; i < nsegs; i++) {
			tp->wqe_req.sgl[i].address = segs[i].ds_addr;
			tp->wqe_req.sgl[i].mem_key = gd->gpa_mkey;
			tp->wqe_req.sgl[i].size = segs[i].ds_len;
		}
		tp->wqe_req.num_sge = nsegs;

		tx_info->mbuf = *m_head;

		bus_dmamap_sync(apc->tx_buf_tag, tx_info->dma_map,
		    BUS_DMASYNC_PREWRITE);
	}

	return err;
}

static inline void
mana_tx_unmap_mbuf(struct mana_port_context *apc,
    struct mana_send_buf_info *tx_info)
{
	bus_dmamap_sync(apc->tx_buf_tag, tx_info->dma_map,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(apc->tx_buf_tag, tx_info->dma_map);
	if (tx_info->mbuf) {
		m_freem(tx_info->mbuf);
		tx_info->mbuf = NULL;
	}
}

static inline int
mana_load_rx_mbuf(struct mana_port_context *apc, struct mana_rxq *rxq,
    struct mana_recv_buf_oob *rx_oob, bool alloc_mbuf)
{
	bus_dma_segment_t segs[1];
	struct mbuf *mbuf;
	int nsegs, err;
	uint32_t mlen;

	if (alloc_mbuf) {
		mbuf = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, rxq->datasize);
		if (unlikely(mbuf == NULL)) {
			mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
			if (unlikely(mbuf == NULL)) {
				return ENOMEM;
			}
			mlen = MCLBYTES;
		} else {
			mlen = rxq->datasize;
		}

		mbuf->m_pkthdr.len = mbuf->m_len = mlen;
	} else {
		if (rx_oob->mbuf) {
			mbuf = rx_oob->mbuf;
			mlen = rx_oob->mbuf->m_pkthdr.len;
		} else {
			return ENOMEM;
		}
	}

	err = bus_dmamap_load_mbuf_sg(apc->rx_buf_tag, rx_oob->dma_map,
	    mbuf, segs, &nsegs, BUS_DMA_NOWAIT);

	if (unlikely((err != 0) || (nsegs != 1))) {
		mana_warn(NULL, "Failed to map mbuf, error: %d, "
		    "nsegs: %d\n", err, nsegs);
		counter_u64_add(rxq->stats.dma_mapping_err, 1);
		goto error;
	}

	bus_dmamap_sync(apc->rx_buf_tag, rx_oob->dma_map,
	    BUS_DMASYNC_PREREAD);

	rx_oob->mbuf = mbuf;
	rx_oob->num_sge = 1;
	rx_oob->sgl[0].address = segs[0].ds_addr;
	rx_oob->sgl[0].size = mlen;
	rx_oob->sgl[0].mem_key = apc->ac->gdma_dev->gpa_mkey;

	return 0;

error:
	m_freem(mbuf);
	return EFAULT;
}

static inline void
mana_unload_rx_mbuf(struct mana_port_context *apc, struct mana_rxq *rxq,
    struct mana_recv_buf_oob *rx_oob, bool free_mbuf)
{
	bus_dmamap_sync(apc->rx_buf_tag, rx_oob->dma_map,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(apc->rx_buf_tag, rx_oob->dma_map);

	if (free_mbuf && rx_oob->mbuf) {
		m_freem(rx_oob->mbuf);
		rx_oob->mbuf = NULL;
	}
}


/* Use couple mbuf PH_loc spaces for l3 and l4 protocal type */
#define MANA_L3_PROTO(_mbuf)	((_mbuf)->m_pkthdr.PH_loc.sixteen[0])
#define MANA_L4_PROTO(_mbuf)	((_mbuf)->m_pkthdr.PH_loc.sixteen[1])

#define MANA_TXQ_FULL	(IFF_DRV_RUNNING | IFF_DRV_OACTIVE)

static void
mana_xmit(struct mana_txq *txq)
{
	enum mana_tx_pkt_format pkt_fmt = MANA_SHORT_PKT_FMT;
	struct mana_send_buf_info *tx_info;
	struct ifnet *ndev = txq->ndev;
	struct mbuf *mbuf;
	struct mana_port_context *apc = if_getsoftc(ndev);
	struct mana_port_stats *port_stats = &apc->port_stats;
	struct gdma_dev *gd = apc->ac->gdma_dev;
	uint64_t packets, bytes;
	uint16_t next_to_use;
	struct mana_tx_package pkg = {};
	struct mana_stats *tx_stats;
	struct gdma_queue *gdma_sq;
	struct mana_cq *cq;
	int err, len;

	gdma_sq = txq->gdma_sq;
	cq = &apc->tx_qp[txq->idx].tx_cq;
	tx_stats = &txq->stats;

	packets = 0;
	bytes = 0;
	next_to_use = txq->next_to_use;

	while ((mbuf = drbr_peek(ndev, txq->txq_br)) != NULL) {
		if (!apc->port_is_up ||
		    (if_getdrvflags(ndev) & MANA_TXQ_FULL) != IFF_DRV_RUNNING) {
			drbr_putback(ndev, txq->txq_br, mbuf);
			break;
		}

		if (!mana_can_tx(gdma_sq)) {
			/* SQ is full. Set the IFF_DRV_OACTIVE flag */
			if_setdrvflagbits(apc->ndev, IFF_DRV_OACTIVE, 0);
			counter_u64_add(tx_stats->stop, 1);
			uint64_t stops = counter_u64_fetch(tx_stats->stop);
			uint64_t wakeups = counter_u64_fetch(tx_stats->wakeup);
#define MANA_TXQ_STOP_THRESHOLD		50
			if (stops > MANA_TXQ_STOP_THRESHOLD && wakeups > 0 &&
			    stops > wakeups && txq->alt_txq_idx == txq->idx) {
				txq->alt_txq_idx =
				    (txq->idx + (stops / wakeups))
				    % apc->num_queues;
				counter_u64_add(tx_stats->alt_chg, 1);
			}

			drbr_putback(ndev, txq->txq_br, mbuf);

			taskqueue_enqueue(cq->cleanup_tq, &cq->cleanup_task);
			break;
		}

		tx_info = &txq->tx_buf_info[next_to_use];

		memset(&pkg, 0, sizeof(struct mana_tx_package));
		pkg.wqe_req.sgl = pkg.sgl_array;

		err = mana_tx_map_mbuf(apc, tx_info, &mbuf, &pkg, tx_stats);
		if (unlikely(err)) {
			mana_dbg(NULL,
			    "Failed to map tx mbuf, err %d\n", err);

			counter_u64_add(tx_stats->dma_mapping_err, 1);

			/* The mbuf is still there. Free it */
			m_freem(mbuf);
			/* Advance the drbr queue */
			drbr_advance(ndev, txq->txq_br);
			continue;
		}

		pkg.tx_oob.s_oob.vcq_num = cq->gdma_id;
		pkg.tx_oob.s_oob.vsq_frame = txq->vsq_frame;

		if (txq->vp_offset > MANA_SHORT_VPORT_OFFSET_MAX) {
			pkg.tx_oob.l_oob.long_vp_offset = txq->vp_offset;
			pkt_fmt = MANA_LONG_PKT_FMT;
		} else {
			pkg.tx_oob.s_oob.short_vp_offset = txq->vp_offset;
		}

		pkg.tx_oob.s_oob.pkt_fmt = pkt_fmt;

		if (pkt_fmt == MANA_SHORT_PKT_FMT)
			pkg.wqe_req.inline_oob_size = sizeof(struct mana_tx_short_oob);
		else
			pkg.wqe_req.inline_oob_size = sizeof(struct mana_tx_oob);

		pkg.wqe_req.inline_oob_data = &pkg.tx_oob;
		pkg.wqe_req.flags = 0;
		pkg.wqe_req.client_data_unit = 0;

		if (mbuf->m_pkthdr.csum_flags & CSUM_TSO) {
			if (MANA_L3_PROTO(mbuf) == ETHERTYPE_IP)
				pkg.tx_oob.s_oob.is_outer_ipv4 = 1;
			else
				pkg.tx_oob.s_oob.is_outer_ipv6 = 1;

			pkg.tx_oob.s_oob.comp_iphdr_csum = 1;
			pkg.tx_oob.s_oob.comp_tcp_csum = 1;
			pkg.tx_oob.s_oob.trans_off = mbuf->m_pkthdr.l3hlen;

			pkg.wqe_req.client_data_unit = mbuf->m_pkthdr.tso_segsz;
			pkg.wqe_req.flags = GDMA_WR_OOB_IN_SGL | GDMA_WR_PAD_BY_SGE0;
		} else if (mbuf->m_pkthdr.csum_flags &
		    (CSUM_IP_UDP | CSUM_IP_TCP | CSUM_IP6_UDP | CSUM_IP6_TCP)) {
			if (MANA_L3_PROTO(mbuf) == ETHERTYPE_IP) {
				pkg.tx_oob.s_oob.is_outer_ipv4 = 1;
				pkg.tx_oob.s_oob.comp_iphdr_csum = 1;
			} else {
				pkg.tx_oob.s_oob.is_outer_ipv6 = 1;
			}

			if (MANA_L4_PROTO(mbuf) == IPPROTO_TCP) {
				pkg.tx_oob.s_oob.comp_tcp_csum = 1;
				pkg.tx_oob.s_oob.trans_off =
				    mbuf->m_pkthdr.l3hlen;
			} else {
				pkg.tx_oob.s_oob.comp_udp_csum = 1;
			}
		} else if (mbuf->m_pkthdr.csum_flags & CSUM_IP) {
			pkg.tx_oob.s_oob.is_outer_ipv4 = 1;
			pkg.tx_oob.s_oob.comp_iphdr_csum = 1;
		} else {
			if (MANA_L3_PROTO(mbuf) == ETHERTYPE_IP)
				pkg.tx_oob.s_oob.is_outer_ipv4 = 1;
			else if (MANA_L3_PROTO(mbuf) == ETHERTYPE_IPV6)
				pkg.tx_oob.s_oob.is_outer_ipv6 = 1;
		}

		len = mbuf->m_pkthdr.len;

		err = mana_gd_post_work_request(gdma_sq, &pkg.wqe_req,
		    (struct gdma_posted_wqe_info *)&tx_info->wqe_inf);
		if (unlikely(err)) {
			/* Should not happen */
			if_printf(ndev, "Failed to post TX OOB: %d\n", err);

			mana_tx_unmap_mbuf(apc, tx_info);

			drbr_advance(ndev, txq->txq_br);
			continue;
		}

		next_to_use =
		    (next_to_use + 1) % MAX_SEND_BUFFERS_PER_QUEUE;

		(void)atomic_inc_return(&txq->pending_sends);

		drbr_advance(ndev, txq->txq_br);

		mana_gd_wq_ring_doorbell(gd->gdma_context, gdma_sq);

		packets++;
		bytes += len;
	}

	counter_enter();
	counter_u64_add_protected(tx_stats->packets, packets);
	counter_u64_add_protected(port_stats->tx_packets, packets);
	counter_u64_add_protected(tx_stats->bytes, bytes);
	counter_u64_add_protected(port_stats->tx_bytes, bytes);
	counter_exit();

	txq->next_to_use = next_to_use;
}

static void
mana_xmit_taskfunc(void *arg, int pending)
{
	struct mana_txq *txq = (struct mana_txq *)arg;
	struct ifnet *ndev = txq->ndev;
	struct mana_port_context *apc = if_getsoftc(ndev);

	while (!drbr_empty(ndev, txq->txq_br) && apc->port_is_up &&
	    (if_getdrvflags(ndev) & MANA_TXQ_FULL) == IFF_DRV_RUNNING) {
		mtx_lock(&txq->txq_mtx);
		mana_xmit(txq);
		mtx_unlock(&txq->txq_mtx);
	}
}

#define PULLUP_HDR(m, len)				\
do {							\
	if (unlikely((m)->m_len < (len))) {		\
		(m) = m_pullup((m), (len));		\
		if ((m) == NULL)			\
			return (NULL);			\
	}						\
} while (0)

/*
 * If this function failed, the mbuf would be freed.
 */
static inline struct mbuf *
mana_tso_fixup(struct mbuf *mbuf)
{
	struct ether_vlan_header *eh = mtod(mbuf, struct ether_vlan_header *);
	struct tcphdr *th;
	uint16_t etype;
	int ehlen;

	if (eh->evl_encap_proto == ntohs(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehlen = ETHER_HDR_LEN;
	}

	if (etype == ETHERTYPE_IP) {
		struct ip *ip;
		int iphlen;

		PULLUP_HDR(mbuf, ehlen + sizeof(*ip));
		ip = mtodo(mbuf, ehlen);
		iphlen = ip->ip_hl << 2;
		mbuf->m_pkthdr.l3hlen = ehlen + iphlen;

		PULLUP_HDR(mbuf, ehlen + iphlen + sizeof(*th));
		th = mtodo(mbuf, ehlen + iphlen);

		ip->ip_len = 0;
		ip->ip_sum = 0;
		th->th_sum = in_pseudo(ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
	} else if (etype == ETHERTYPE_IPV6) {
		struct ip6_hdr *ip6;

		PULLUP_HDR(mbuf, ehlen + sizeof(*ip6) + sizeof(*th));
		ip6 = mtodo(mbuf, ehlen);
		if (ip6->ip6_nxt != IPPROTO_TCP) {
			/* Realy something wrong, just return */
			mana_dbg(NULL, "TSO mbuf not TCP, freed.\n");
			m_freem(mbuf);
			return NULL;
		}
		mbuf->m_pkthdr.l3hlen = ehlen + sizeof(*ip6);

		th = mtodo(mbuf, ehlen + sizeof(*ip6));

		ip6->ip6_plen = 0;
		th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
	} else {
		/* CSUM_TSO is set but not IP protocol. */
		mana_warn(NULL, "TSO mbuf not right, freed.\n");
		m_freem(mbuf);
		return NULL;
	}

	MANA_L3_PROTO(mbuf) = etype;

	return (mbuf);
}

/*
 * If this function failed, the mbuf would be freed.
 */
static inline struct mbuf *
mana_mbuf_csum_check(struct mbuf *mbuf)
{
	struct ether_vlan_header *eh = mtod(mbuf, struct ether_vlan_header *);
	struct mbuf *mbuf_next;
	uint16_t etype;
	int offset;
	int ehlen;

	if (eh->evl_encap_proto == ntohs(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehlen = ETHER_HDR_LEN;
	}

	mbuf_next = m_getptr(mbuf, ehlen, &offset);

	MANA_L4_PROTO(mbuf) = 0;
	if (etype == ETHERTYPE_IP) {
		const struct ip *ip;
		int iphlen;

		ip = (struct ip *)(mtodo(mbuf_next, offset));
		iphlen = ip->ip_hl << 2;
		mbuf->m_pkthdr.l3hlen = ehlen + iphlen;

		MANA_L4_PROTO(mbuf) = ip->ip_p;
	} else if (etype == ETHERTYPE_IPV6) {
		const struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)(mtodo(mbuf_next, offset));
		mbuf->m_pkthdr.l3hlen = ehlen + sizeof(*ip6);

		MANA_L4_PROTO(mbuf) = ip6->ip6_nxt;
	} else {
		MANA_L4_PROTO(mbuf) = 0;
	}

	MANA_L3_PROTO(mbuf) = etype;

	return (mbuf);
}

static int
mana_start_xmit(struct ifnet *ifp, struct mbuf *m)
{
	struct mana_port_context *apc = if_getsoftc(ifp);
	struct mana_txq *txq;
	int is_drbr_empty;
	uint16_t txq_id;
	int err;

	if (unlikely((!apc->port_is_up) ||
	    (if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
		return ENODEV;

	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		m = mana_tso_fixup(m);
		if (unlikely(m == NULL)) {
			counter_enter();
			counter_u64_add_protected(apc->port_stats.tx_drops, 1);
			counter_exit();
			return EIO;
		}
	} else {
		m = mana_mbuf_csum_check(m);
		if (unlikely(m == NULL)) {
			counter_enter();
			counter_u64_add_protected(apc->port_stats.tx_drops, 1);
			counter_exit();
			return EIO;
		}
	}

	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
		uint32_t hash = m->m_pkthdr.flowid;
		txq_id = apc->indir_table[(hash) & MANA_INDIRECT_TABLE_MASK] %
		    apc->num_queues;
	} else {
		txq_id = m->m_pkthdr.flowid % apc->num_queues;
	}

	if (apc->enable_tx_altq)
		txq_id = apc->tx_qp[txq_id].txq.alt_txq_idx;

	txq = &apc->tx_qp[txq_id].txq;

	is_drbr_empty = drbr_empty(ifp, txq->txq_br);
	err = drbr_enqueue(ifp, txq->txq_br, m);
	if (unlikely(err)) {
		mana_warn(NULL, "txq %u failed to enqueue: %d\n",
		    txq_id, err);
		taskqueue_enqueue(txq->enqueue_tq, &txq->enqueue_task);
		return err;
	}

	if (is_drbr_empty && mtx_trylock(&txq->txq_mtx)) {
		mana_xmit(txq);
		mtx_unlock(&txq->txq_mtx);
	} else {
		taskqueue_enqueue(txq->enqueue_tq, &txq->enqueue_task);
	}

	return 0;
}

static void
mana_cleanup_port_context(struct mana_port_context *apc)
{
	bus_dma_tag_destroy(apc->tx_buf_tag);
	bus_dma_tag_destroy(apc->rx_buf_tag);
	apc->rx_buf_tag = NULL;

	free(apc->rxqs, M_DEVBUF);
	apc->rxqs = NULL;

	mana_free_counters((counter_u64_t *)&apc->port_stats,
	    sizeof(struct mana_port_stats));
}

static int
mana_init_port_context(struct mana_port_context *apc)
{
	device_t dev = apc->ac->gdma_dev->gdma_context->dev;
	uint32_t tso_maxsize;
	int err;

	tso_maxsize = MAX_MBUF_FRAGS * MANA_TSO_MAXSEG_SZ -
	    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);

	/* Create DMA tag for tx bufs */
	err = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
	    1, 0,			/* alignment, boundary	*/
	    BUS_SPACE_MAXADDR,		/* lowaddr		*/
	    BUS_SPACE_MAXADDR,		/* highaddr		*/
	    NULL, NULL,			/* filter, filterarg	*/
	    tso_maxsize,		/* maxsize		*/
	    MAX_MBUF_FRAGS,		/* nsegments		*/
	    tso_maxsize,		/* maxsegsize		*/
	    0,				/* flags		*/
	    NULL, NULL,			/* lockfunc, lockfuncarg*/
	    &apc->tx_buf_tag);
	if (unlikely(err)) {
		device_printf(dev, "Feiled to create TX DMA tag\n");
		return err;
	}

	/* Create DMA tag for rx bufs */
	err = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
	    64, 0,			/* alignment, boundary	*/
	    BUS_SPACE_MAXADDR,		/* lowaddr		*/
	    BUS_SPACE_MAXADDR,		/* highaddr		*/
	    NULL, NULL,			/* filter, filterarg	*/
	    MJUMPAGESIZE,		/* maxsize		*/
	    1,				/* nsegments		*/
	    MJUMPAGESIZE,		/* maxsegsize		*/
	    0,				/* flags		*/
	    NULL, NULL,			/* lockfunc, lockfuncarg*/
	    &apc->rx_buf_tag);
	if (unlikely(err)) {
		device_printf(dev, "Feiled to create RX DMA tag\n");
		return err;
	}

	apc->rxqs = mallocarray(apc->num_queues, sizeof(struct mana_rxq *),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	if (!apc->rxqs) {
		bus_dma_tag_destroy(apc->tx_buf_tag);
		bus_dma_tag_destroy(apc->rx_buf_tag);
		apc->rx_buf_tag = NULL;
		return ENOMEM;
	}

	return 0;
}

static int
mana_send_request(struct mana_context *ac, void *in_buf,
    uint32_t in_len, void *out_buf, uint32_t out_len)
{
	struct gdma_context *gc = ac->gdma_dev->gdma_context;
	struct gdma_resp_hdr *resp = out_buf;
	struct gdma_req_hdr *req = in_buf;
	device_t dev = gc->dev;
	static atomic_t activity_id;
	int err;

	req->dev_id = gc->mana.dev_id;
	req->activity_id = atomic_inc_return(&activity_id);

	mana_dbg(NULL, "activity_id  = %u\n", activity_id);

	err = mana_gd_send_request(gc, in_len, in_buf, out_len,
	    out_buf);
	if (err || resp->status) {
		device_printf(dev, "Failed to send mana message: %d, 0x%x\n",
			err, resp->status);
		return err ? err : EPROTO;
	}

	if (req->dev_id.as_uint32 != resp->dev_id.as_uint32 ||
	    req->activity_id != resp->activity_id) {
		device_printf(dev,
		    "Unexpected mana message response: %x,%x,%x,%x\n",
		    req->dev_id.as_uint32, resp->dev_id.as_uint32,
		    req->activity_id, resp->activity_id);
		return EPROTO;
	}

	return 0;
}

static int
mana_verify_resp_hdr(const struct gdma_resp_hdr *resp_hdr,
    const enum mana_command_code expected_code,
    const uint32_t min_size)
{
	if (resp_hdr->response.msg_type != expected_code)
		return EPROTO;

	if (resp_hdr->response.msg_version < GDMA_MESSAGE_V1)
		return EPROTO;

	if (resp_hdr->response.msg_size < min_size)
		return EPROTO;

	return 0;
}

static int
mana_query_device_cfg(struct mana_context *ac, uint32_t proto_major_ver,
    uint32_t proto_minor_ver, uint32_t proto_micro_ver,
    uint16_t *max_num_vports)
{
	struct gdma_context *gc = ac->gdma_dev->gdma_context;
	struct mana_query_device_cfg_resp resp = {};
	struct mana_query_device_cfg_req req = {};
	device_t dev = gc->dev;
	int err = 0;

	mana_gd_init_req_hdr(&req.hdr, MANA_QUERY_DEV_CONFIG,
	    sizeof(req), sizeof(resp));
	req.proto_major_ver = proto_major_ver;
	req.proto_minor_ver = proto_minor_ver;
	req.proto_micro_ver = proto_micro_ver;

	err = mana_send_request(ac, &req, sizeof(req), &resp, sizeof(resp));
	if (err) {
		device_printf(dev, "Failed to query config: %d", err);
		return err;
	}

	err = mana_verify_resp_hdr(&resp.hdr, MANA_QUERY_DEV_CONFIG,
	    sizeof(resp));
	if (err || resp.hdr.status) {
		device_printf(dev, "Invalid query result: %d, 0x%x\n", err,
		    resp.hdr.status);
		if (!err)
			err = EPROTO;
		return err;
	}

	*max_num_vports = resp.max_num_vports;

	mana_dbg(NULL, "mana max_num_vports from device = %d\n",
	    *max_num_vports);

	return 0;
}

static int
mana_query_vport_cfg(struct mana_port_context *apc, uint32_t vport_index,
    uint32_t *max_sq, uint32_t *max_rq, uint32_t *num_indir_entry)
{
	struct mana_query_vport_cfg_resp resp = {};
	struct mana_query_vport_cfg_req req = {};
	int err;

	mana_gd_init_req_hdr(&req.hdr, MANA_QUERY_VPORT_CONFIG,
	    sizeof(req), sizeof(resp));

	req.vport_index = vport_index;

	err = mana_send_request(apc->ac, &req, sizeof(req), &resp,
	    sizeof(resp));
	if (err)
		return err;

	err = mana_verify_resp_hdr(&resp.hdr, MANA_QUERY_VPORT_CONFIG,
	    sizeof(resp));
	if (err)
		return err;

	if (resp.hdr.status)
		return EPROTO;

	*max_sq = resp.max_num_sq;
	*max_rq = resp.max_num_rq;
	*num_indir_entry = resp.num_indirection_ent;

	apc->port_handle = resp.vport;
	memcpy(apc->mac_addr, resp.mac_addr, ETHER_ADDR_LEN);

	return 0;
}

static int
mana_cfg_vport(struct mana_port_context *apc, uint32_t protection_dom_id,
    uint32_t doorbell_pg_id)
{
	struct mana_config_vport_resp resp = {};
	struct mana_config_vport_req req = {};
	int err;

	mana_gd_init_req_hdr(&req.hdr, MANA_CONFIG_VPORT_TX,
	    sizeof(req), sizeof(resp));
	req.vport = apc->port_handle;
	req.pdid = protection_dom_id;
	req.doorbell_pageid = doorbell_pg_id;

	err = mana_send_request(apc->ac, &req, sizeof(req), &resp,
	    sizeof(resp));
	if (err) {
		if_printf(apc->ndev, "Failed to configure vPort: %d\n", err);
		goto out;
	}

	err = mana_verify_resp_hdr(&resp.hdr, MANA_CONFIG_VPORT_TX,
	    sizeof(resp));
	if (err || resp.hdr.status) {
		if_printf(apc->ndev, "Failed to configure vPort: %d, 0x%x\n",
		    err, resp.hdr.status);
		if (!err)
			err = EPROTO;

		goto out;
	}

	apc->tx_shortform_allowed = resp.short_form_allowed;
	apc->tx_vp_offset = resp.tx_vport_offset;
out:
	return err;
}

static int
mana_cfg_vport_steering(struct mana_port_context *apc,
    enum TRI_STATE rx,
    bool update_default_rxobj, bool update_key,
    bool update_tab)
{
	uint16_t num_entries = MANA_INDIRECT_TABLE_SIZE;
	struct mana_cfg_rx_steer_req *req = NULL;
	struct mana_cfg_rx_steer_resp resp = {};
	struct ifnet *ndev = apc->ndev;
	mana_handle_t *req_indir_tab;
	uint32_t req_buf_size;
	int err;

	req_buf_size = sizeof(*req) + sizeof(mana_handle_t) * num_entries;
	req = malloc(req_buf_size, M_DEVBUF, M_WAITOK | M_ZERO);
	if (!req)
		return ENOMEM;

	mana_gd_init_req_hdr(&req->hdr, MANA_CONFIG_VPORT_RX, req_buf_size,
	    sizeof(resp));

	req->vport = apc->port_handle;
	req->num_indir_entries = num_entries;
	req->indir_tab_offset = sizeof(*req);
	req->rx_enable = rx;
	req->rss_enable = apc->rss_state;
	req->update_default_rxobj = update_default_rxobj;
	req->update_hashkey = update_key;
	req->update_indir_tab = update_tab;
	req->default_rxobj = apc->default_rxobj;

	if (update_key)
		memcpy(&req->hashkey, apc->hashkey, MANA_HASH_KEY_SIZE);

	if (update_tab) {
		req_indir_tab = (mana_handle_t *)(req + 1);
		memcpy(req_indir_tab, apc->rxobj_table,
		       req->num_indir_entries * sizeof(mana_handle_t));
	}

	err = mana_send_request(apc->ac, req, req_buf_size, &resp,
	    sizeof(resp));
	if (err) {
		if_printf(ndev, "Failed to configure vPort RX: %d\n", err);
		goto out;
	}

	err = mana_verify_resp_hdr(&resp.hdr, MANA_CONFIG_VPORT_RX,
	    sizeof(resp));
	if (err) {
		if_printf(ndev, "vPort RX configuration failed: %d\n", err);
		goto out;
	}

	if (resp.hdr.status) {
		if_printf(ndev, "vPort RX configuration failed: 0x%x\n",
		    resp.hdr.status);
		err = EPROTO;
	}
out:
	free(req, M_DEVBUF);
	return err;
}

static int
mana_create_wq_obj(struct mana_port_context *apc,
    mana_handle_t vport,
    uint32_t wq_type, struct mana_obj_spec *wq_spec,
    struct mana_obj_spec *cq_spec,
    mana_handle_t *wq_obj)
{
	struct mana_create_wqobj_resp resp = {};
	struct mana_create_wqobj_req req = {};
	struct ifnet *ndev = apc->ndev;
	int err;

	mana_gd_init_req_hdr(&req.hdr, MANA_CREATE_WQ_OBJ,
	    sizeof(req), sizeof(resp));
	req.vport = vport;
	req.wq_type = wq_type;
	req.wq_gdma_region = wq_spec->gdma_region;
	req.cq_gdma_region = cq_spec->gdma_region;
	req.wq_size = wq_spec->queue_size;
	req.cq_size = cq_spec->queue_size;
	req.cq_moderation_ctx_id = cq_spec->modr_ctx_id;
	req.cq_parent_qid = cq_spec->attached_eq;

	err = mana_send_request(apc->ac, &req, sizeof(req), &resp,
	    sizeof(resp));
	if (err) {
		if_printf(ndev, "Failed to create WQ object: %d\n", err);
		goto out;
	}

	err = mana_verify_resp_hdr(&resp.hdr, MANA_CREATE_WQ_OBJ,
	    sizeof(resp));
	if (err || resp.hdr.status) {
		if_printf(ndev, "Failed to create WQ object: %d, 0x%x\n", err,
		    resp.hdr.status);
		if (!err)
			err = EPROTO;
		goto out;
	}

	if (resp.wq_obj == INVALID_MANA_HANDLE) {
		if_printf(ndev, "Got an invalid WQ object handle\n");
		err = EPROTO;
		goto out;
	}

	*wq_obj = resp.wq_obj;
	wq_spec->queue_index = resp.wq_id;
	cq_spec->queue_index = resp.cq_id;

	return 0;
out:
	return err;
}

static void
mana_destroy_wq_obj(struct mana_port_context *apc, uint32_t wq_type,
    mana_handle_t wq_obj)
{
	struct mana_destroy_wqobj_resp resp = {};
	struct mana_destroy_wqobj_req req = {};
	struct ifnet *ndev = apc->ndev;
	int err;

	mana_gd_init_req_hdr(&req.hdr, MANA_DESTROY_WQ_OBJ,
	    sizeof(req), sizeof(resp));
	req.wq_type = wq_type;
	req.wq_obj_handle = wq_obj;

	err = mana_send_request(apc->ac, &req, sizeof(req), &resp,
	    sizeof(resp));
	if (err) {
		if_printf(ndev, "Failed to destroy WQ object: %d\n", err);
		return;
	}

	err = mana_verify_resp_hdr(&resp.hdr, MANA_DESTROY_WQ_OBJ,
	    sizeof(resp));
	if (err || resp.hdr.status)
		if_printf(ndev, "Failed to destroy WQ object: %d, 0x%x\n",
		    err, resp.hdr.status);
}

static void
mana_destroy_eq(struct mana_context *ac)
{
	struct gdma_context *gc = ac->gdma_dev->gdma_context;
	struct gdma_queue *eq;
	int i;

	if (!ac->eqs)
		return;

	for (i = 0; i < gc->max_num_queues; i++) {
		eq = ac->eqs[i].eq;
		if (!eq)
			continue;

		mana_gd_destroy_queue(gc, eq);
	}

	free(ac->eqs, M_DEVBUF);
	ac->eqs = NULL;
}

static int
mana_create_eq(struct mana_context *ac)
{
	struct gdma_dev *gd = ac->gdma_dev;
	struct gdma_context *gc = gd->gdma_context;
	struct gdma_queue_spec spec = {};
	int err;
	int i;

	ac->eqs = mallocarray(gc->max_num_queues, sizeof(struct mana_eq),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (!ac->eqs)
		return ENOMEM;

	spec.type = GDMA_EQ;
	spec.monitor_avl_buf = false;
	spec.queue_size = EQ_SIZE;
	spec.eq.callback = NULL;
	spec.eq.context = ac->eqs;
	spec.eq.log2_throttle_limit = LOG2_EQ_THROTTLE;

	for (i = 0; i < gc->max_num_queues; i++) {
		err = mana_gd_create_mana_eq(gd, &spec, &ac->eqs[i].eq);
		if (err)
			goto out;
	}

	return 0;
out:
	mana_destroy_eq(ac);
	return err;
}

static int
mana_move_wq_tail(struct gdma_queue *wq, uint32_t num_units)
{
	uint32_t used_space_old;
	uint32_t used_space_new;

	used_space_old = wq->head - wq->tail;
	used_space_new = wq->head - (wq->tail + num_units);

	if (used_space_new > used_space_old) {
		mana_err(NULL,
		    "WARNING: new used space %u greater than old one %u\n",
		    used_space_new, used_space_old);
		return ERANGE;
	}

	wq->tail += num_units;
	return 0;
}

static void
mana_poll_tx_cq(struct mana_cq *cq)
{
	struct gdma_comp *completions = cq->gdma_comp_buf;
	struct gdma_posted_wqe_info *wqe_info;
	struct mana_send_buf_info *tx_info;
	unsigned int pkt_transmitted = 0;
	unsigned int wqe_unit_cnt = 0;
	struct mana_txq *txq = cq->txq;
	struct mana_port_context *apc;
	uint16_t next_to_complete;
	struct ifnet *ndev;
	int comp_read;
	int txq_idx = txq->idx;;
	int i;
	int sa_drop = 0;

	struct gdma_queue *gdma_wq;
	unsigned int avail_space;
	bool txq_full = false;

	ndev = txq->ndev;
	apc = if_getsoftc(ndev);

	comp_read = mana_gd_poll_cq(cq->gdma_cq, completions,
	    CQE_POLLING_BUFFER);

	if (comp_read < 1)
		return;

	next_to_complete = txq->next_to_complete;

	for (i = 0; i < comp_read; i++) {
		struct mana_tx_comp_oob *cqe_oob;

		if (!completions[i].is_sq) {
			mana_err(NULL, "WARNING: Not for SQ\n");
			return;
		}

		cqe_oob = (struct mana_tx_comp_oob *)completions[i].cqe_data;
		if (cqe_oob->cqe_hdr.client_type !=
				 MANA_CQE_COMPLETION) {
			mana_err(NULL,
			    "WARNING: Invalid CQE client type %u\n",
			    cqe_oob->cqe_hdr.client_type);
			return;
		}

		switch (cqe_oob->cqe_hdr.cqe_type) {
		case CQE_TX_OKAY:
			break;

		case CQE_TX_SA_DROP:
		case CQE_TX_MTU_DROP:
		case CQE_TX_INVALID_OOB:
		case CQE_TX_INVALID_ETH_TYPE:
		case CQE_TX_HDR_PROCESSING_ERROR:
		case CQE_TX_VF_DISABLED:
		case CQE_TX_VPORT_IDX_OUT_OF_RANGE:
		case CQE_TX_VPORT_DISABLED:
		case CQE_TX_VLAN_TAGGING_VIOLATION:
			sa_drop ++;
			mana_err(NULL,
			    "TX: txq %d CQE error %d, ntc = %d, "
			    "pending sends = %d: err ignored.\n",
			    txq_idx, cqe_oob->cqe_hdr.cqe_type,
			    next_to_complete, txq->pending_sends);
			break;

		default:
			/* If the CQE type is unexpected, log an error,
			 * and go through the error path.
			 */
			mana_err(NULL,
			    "ERROR: TX: Unexpected CQE type %d: HW BUG?\n",
			    cqe_oob->cqe_hdr.cqe_type);
			return;
		}
		if (txq->gdma_txq_id != completions[i].wq_num) {
			mana_dbg(NULL,
			    "txq gdma id not match completion wq num: "
			    "%d != %d\n",
			    txq->gdma_txq_id, completions[i].wq_num);
			break;
		}

		tx_info = &txq->tx_buf_info[next_to_complete];
		if (!tx_info->mbuf) {
			mana_err(NULL,
			    "WARNING: txq %d Empty mbuf on tx_info: %u, "
			    "ntu = %u, pending_sends = %d, "
			    "transmitted = %d, sa_drop = %d, i = %d, comp_read = %d\n",
			    txq_idx, next_to_complete, txq->next_to_use,
			    txq->pending_sends, pkt_transmitted, sa_drop,
			    i, comp_read);
			break;
		}

		wqe_info = &tx_info->wqe_inf;
		wqe_unit_cnt += wqe_info->wqe_size_in_bu;

		mana_tx_unmap_mbuf(apc, tx_info);
		mb();

		next_to_complete =
		    (next_to_complete + 1) % MAX_SEND_BUFFERS_PER_QUEUE;

		pkt_transmitted++;
	}

	txq->next_to_complete = next_to_complete;

	if (wqe_unit_cnt == 0) {
		mana_err(NULL,
		    "WARNING: TX ring not proceeding!\n");
		return;
	}

	mana_move_wq_tail(txq->gdma_sq, wqe_unit_cnt);

	/* Ensure tail updated before checking q stop */
	wmb();

	gdma_wq = txq->gdma_sq;
	avail_space = mana_gd_wq_avail_space(gdma_wq);


	if ((if_getdrvflags(ndev) & MANA_TXQ_FULL) == MANA_TXQ_FULL) {
		txq_full = true;
	}

	/* Ensure checking txq_full before apc->port_is_up. */
	rmb();

	if (txq_full && apc->port_is_up && avail_space >= MAX_TX_WQE_SIZE) {
		/* Grab the txq lock and re-test */
		mtx_lock(&txq->txq_mtx);
		avail_space = mana_gd_wq_avail_space(gdma_wq);

		if ((if_getdrvflags(ndev) & MANA_TXQ_FULL) == MANA_TXQ_FULL &&
		    apc->port_is_up && avail_space >= MAX_TX_WQE_SIZE) {
			/* Clear the Q full flag */
			if_setdrvflagbits(apc->ndev, IFF_DRV_RUNNING,
			    IFF_DRV_OACTIVE);
			counter_u64_add(txq->stats.wakeup, 1);
			if (txq->alt_txq_idx != txq->idx) {
				uint64_t stops = counter_u64_fetch(txq->stats.stop);
				uint64_t wakeups = counter_u64_fetch(txq->stats.wakeup);
				/* Reset alt_txq_idx back if it is not overloaded */
				if (stops < wakeups) {
					txq->alt_txq_idx = txq->idx;
					counter_u64_add(txq->stats.alt_reset, 1);
				}
			}
			rmb();
			/* Schedule a tx enqueue task */
			taskqueue_enqueue(txq->enqueue_tq, &txq->enqueue_task);
		}
		mtx_unlock(&txq->txq_mtx);
	}

	if (atomic_sub_return(pkt_transmitted, &txq->pending_sends) < 0)
		mana_err(NULL,
		    "WARNING: TX %d pending_sends error: %d\n",
		    txq->idx, txq->pending_sends);

	cq->work_done = pkt_transmitted;
}

static void
mana_post_pkt_rxq(struct mana_rxq *rxq)
{
	struct mana_recv_buf_oob *recv_buf_oob;
	uint32_t curr_index;
	int err;

	curr_index = rxq->buf_index++;
	if (rxq->buf_index == rxq->num_rx_buf)
		rxq->buf_index = 0;

	recv_buf_oob = &rxq->rx_oobs[curr_index];

	err = mana_gd_post_and_ring(rxq->gdma_rq, &recv_buf_oob->wqe_req,
	    &recv_buf_oob->wqe_inf);
	if (err) {
		mana_err(NULL, "WARNING: rxq %u post pkt err %d\n",
		    rxq->rxq_idx, err);
		return;
	}

	if (recv_buf_oob->wqe_inf.wqe_size_in_bu != 1) {
		mana_err(NULL, "WARNING: rxq %u wqe_size_in_bu %u\n",
		    rxq->rxq_idx, recv_buf_oob->wqe_inf.wqe_size_in_bu);
	}
}

static void
mana_rx_mbuf(struct mbuf *mbuf, struct mana_rxcomp_oob *cqe,
    struct mana_rxq *rxq)
{
	struct mana_stats *rx_stats = &rxq->stats;
	struct ifnet *ndev = rxq->ndev;
	uint32_t pkt_len = cqe->ppi[0].pkt_len;
	uint16_t rxq_idx = rxq->rxq_idx;
	struct mana_port_context *apc;
	bool do_lro = false;
	bool do_if_input;

	apc = if_getsoftc(ndev);
	rxq->rx_cq.work_done++;

	if (!mbuf) {
		return;
	}

	mbuf->m_flags |= M_PKTHDR;
	mbuf->m_pkthdr.len = pkt_len;
	mbuf->m_len = pkt_len;
	mbuf->m_pkthdr.rcvif = ndev;

	if ((ndev->if_capenable & IFCAP_RXCSUM ||
	    ndev->if_capenable & IFCAP_RXCSUM_IPV6) &&
	    (cqe->rx_iphdr_csum_succeed)) {
		mbuf->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
		mbuf->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		if (cqe->rx_tcp_csum_succeed || cqe->rx_udp_csum_succeed) {
			mbuf->m_pkthdr.csum_flags |=
			    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			mbuf->m_pkthdr.csum_data = 0xffff;

			if (cqe->rx_tcp_csum_succeed)
				do_lro = true;
		}
	}

	if (cqe->rx_hashtype != 0) {
		mbuf->m_pkthdr.flowid = cqe->ppi[0].pkt_hash;

		uint16_t hashtype = cqe->rx_hashtype;
		if (hashtype & NDIS_HASH_IPV4_MASK) {
			hashtype &= NDIS_HASH_IPV4_MASK;
			switch (hashtype) {
			case NDIS_HASH_TCP_IPV4:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV4);
				break;
			case NDIS_HASH_UDP_IPV4:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV4);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV4);
			}
		} else if (hashtype & NDIS_HASH_IPV6_MASK) {
			hashtype &= NDIS_HASH_IPV6_MASK;
			switch (hashtype) {
			case NDIS_HASH_TCP_IPV6:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV6);
				break;
			case NDIS_HASH_TCP_IPV6_EX:
				M_HASHTYPE_SET(mbuf,
				    M_HASHTYPE_RSS_TCP_IPV6_EX);
				break;
			case NDIS_HASH_UDP_IPV6:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV6);
				break;
			case NDIS_HASH_UDP_IPV6_EX:
				M_HASHTYPE_SET(mbuf,
				    M_HASHTYPE_RSS_UDP_IPV6_EX);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV6);
			}
		} else {
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
		}
	} else {
		mbuf->m_pkthdr.flowid = rxq_idx;
		M_HASHTYPE_SET(mbuf, M_HASHTYPE_NONE);
	}

	do_if_input = true;
	if ((ndev->if_capenable & IFCAP_LRO) && do_lro) {
		if (rxq->lro.lro_cnt != 0 &&
		    tcp_lro_rx(&rxq->lro, mbuf, 0) == 0)
			do_if_input = false;
	}
	if (do_if_input) {
		ndev->if_input(ndev, mbuf);
	}

	counter_enter();
	counter_u64_add_protected(rx_stats->packets, 1);
	counter_u64_add_protected(apc->port_stats.rx_packets, 1);
	counter_u64_add_protected(rx_stats->bytes, pkt_len);
	counter_u64_add_protected(apc->port_stats.rx_bytes, pkt_len);
	counter_exit();
}

static void
mana_process_rx_cqe(struct mana_rxq *rxq, struct mana_cq *cq,
    struct gdma_comp *cqe)
{
	struct mana_rxcomp_oob *oob = (struct mana_rxcomp_oob *)cqe->cqe_data;
	struct mana_recv_buf_oob *rxbuf_oob;
	struct ifnet *ndev = rxq->ndev;
	struct mana_port_context *apc;
	struct mbuf *old_mbuf;
	uint32_t curr, pktlen;
	int err;

	switch (oob->cqe_hdr.cqe_type) {
	case CQE_RX_OKAY:
		break;

	case CQE_RX_TRUNCATED:
		if_printf(ndev, "Dropped a truncated packet\n");
		return;

	case CQE_RX_COALESCED_4:
		if_printf(ndev, "RX coalescing is unsupported\n");
		return;

	case CQE_RX_OBJECT_FENCE:
		if_printf(ndev, "RX Fencing is unsupported\n");
		return;

	default:
		if_printf(ndev, "Unknown RX CQE type = %d\n",
		    oob->cqe_hdr.cqe_type);
		return;
	}

	if (oob->cqe_hdr.cqe_type != CQE_RX_OKAY)
		return;

	pktlen = oob->ppi[0].pkt_len;

	if (pktlen == 0) {
		/* data packets should never have packetlength of zero */
#if defined(__amd64__)
		if_printf(ndev, "RX pkt len=0, rq=%u, cq=%u, rxobj=0x%lx\n",
		    rxq->gdma_id, cq->gdma_id, rxq->rxobj);
#else
		if_printf(ndev, "RX pkt len=0, rq=%u, cq=%u, rxobj=0x%llx\n",
		    rxq->gdma_id, cq->gdma_id, rxq->rxobj);
#endif
		return;
	}

	curr = rxq->buf_index;
	rxbuf_oob = &rxq->rx_oobs[curr];
	if (rxbuf_oob->wqe_inf.wqe_size_in_bu != 1) {
		mana_err(NULL, "WARNING: Rx Incorrect complete "
		    "WQE size %u\n",
		    rxbuf_oob->wqe_inf.wqe_size_in_bu);
	}

	apc = if_getsoftc(ndev);

	old_mbuf = rxbuf_oob->mbuf;

	/* Unload DMA map for the old mbuf */
	mana_unload_rx_mbuf(apc, rxq, rxbuf_oob, false);

	/* Load a new mbuf to replace the old one */
	err = mana_load_rx_mbuf(apc, rxq, rxbuf_oob, true);
	if (err) {
		mana_dbg(NULL,
		    "failed to load rx mbuf, err = %d, packet dropped.\n",
		    err);
		counter_u64_add(rxq->stats.mbuf_alloc_fail, 1);
		/*
		 * Failed to load new mbuf, rxbuf_oob->mbuf is still
		 * pointing to the old one. Drop the packet.
		 */
		 old_mbuf = NULL;
		 /* Reload the existing mbuf */
		 mana_load_rx_mbuf(apc, rxq, rxbuf_oob, false);
	}

	mana_rx_mbuf(old_mbuf, oob, rxq);

	mana_move_wq_tail(rxq->gdma_rq, rxbuf_oob->wqe_inf.wqe_size_in_bu);

	mana_post_pkt_rxq(rxq);
}

static void
mana_poll_rx_cq(struct mana_cq *cq)
{
	struct gdma_comp *comp = cq->gdma_comp_buf;
	int comp_read, i;

	comp_read = mana_gd_poll_cq(cq->gdma_cq, comp, CQE_POLLING_BUFFER);
	KASSERT(comp_read <= CQE_POLLING_BUFFER,
	    ("comp_read %d great than buf size %d",
	    comp_read, CQE_POLLING_BUFFER));

	for (i = 0; i < comp_read; i++) {
		if (comp[i].is_sq == true) {
			mana_err(NULL,
			    "WARNING: CQE not for receive queue\n");
			return;
		}

		/* verify recv cqe references the right rxq */
		if (comp[i].wq_num != cq->rxq->gdma_id) {
			mana_err(NULL,
			    "WARNING: Received CQE %d  not for "
			    "this receive queue %d\n",
			    comp[i].wq_num,  cq->rxq->gdma_id);
			return;
		}

		mana_process_rx_cqe(cq->rxq, cq, &comp[i]);
	}

	tcp_lro_flush_all(&cq->rxq->lro);
}

static void
mana_cq_handler(void *context, struct gdma_queue *gdma_queue)
{
	struct mana_cq *cq = context;
	uint8_t arm_bit;

	KASSERT(cq->gdma_cq == gdma_queue,
	    ("cq do not match %p, %p", cq->gdma_cq, gdma_queue));

	if (cq->type == MANA_CQ_TYPE_RX) {
		mana_poll_rx_cq(cq);
	} else {
		mana_poll_tx_cq(cq);
	}

	if (cq->work_done < cq->budget && cq->do_not_ring_db == false)
		arm_bit = SET_ARM_BIT;
	else
		arm_bit = 0;

	mana_gd_ring_cq(gdma_queue, arm_bit);
}

#define MANA_POLL_BUDGET	8
#define MANA_RX_BUDGET		256
#define MANA_TX_BUDGET		MAX_SEND_BUFFERS_PER_QUEUE

static void
mana_poll(void *arg, int pending)
{
	struct mana_cq *cq = arg;
	int i;

	cq->work_done = 0;
	if (cq->type == MANA_CQ_TYPE_RX) {
		cq->budget = MANA_RX_BUDGET;
	} else {
		cq->budget = MANA_TX_BUDGET;
	}

	for (i = 0; i < MANA_POLL_BUDGET; i++) {
		/*
		 * If this is the last loop, set the budget big enough
		 * so it will arm the CQ any way.
		 */
		if (i == (MANA_POLL_BUDGET - 1))
			cq->budget = CQE_POLLING_BUFFER + 1;

		mana_cq_handler(cq, cq->gdma_cq);

		if (cq->work_done < cq->budget)
			break;

		cq->work_done = 0;
	}
}

static void
mana_schedule_task(void *arg, struct gdma_queue *gdma_queue)
{
	struct mana_cq *cq = arg;

	taskqueue_enqueue(cq->cleanup_tq, &cq->cleanup_task);
}

static void
mana_deinit_cq(struct mana_port_context *apc, struct mana_cq *cq)
{
	struct gdma_dev *gd = apc->ac->gdma_dev;

	if (!cq->gdma_cq)
		return;

	/* Drain cleanup taskqueue */
	if (cq->cleanup_tq) {
		while (taskqueue_cancel(cq->cleanup_tq,
		    &cq->cleanup_task, NULL)) {
			taskqueue_drain(cq->cleanup_tq,
			    &cq->cleanup_task);
		}

		taskqueue_free(cq->cleanup_tq);
	}

	mana_gd_destroy_queue(gd->gdma_context, cq->gdma_cq);
}

static void
mana_deinit_txq(struct mana_port_context *apc, struct mana_txq *txq)
{
	struct gdma_dev *gd = apc->ac->gdma_dev;
	struct mana_send_buf_info *txbuf_info;
	uint32_t pending_sends;
	int i;

	if (!txq->gdma_sq)
		return;

	if ((pending_sends = atomic_read(&txq->pending_sends)) > 0) {
		mana_err(NULL,
		    "WARNING: txq pending sends not zero: %u\n",
		    pending_sends);
	}

	if (txq->next_to_use != txq->next_to_complete) {
		mana_err(NULL,
		    "WARNING: txq buf not completed, "
		    "next use %u, next complete %u\n",
		    txq->next_to_use, txq->next_to_complete);
	}

	/* Flush buf ring. Grab txq mtx lock */
	if (txq->txq_br) {
		mtx_lock(&txq->txq_mtx);
		drbr_flush(apc->ndev, txq->txq_br);
		mtx_unlock(&txq->txq_mtx);
		buf_ring_free(txq->txq_br, M_DEVBUF);
	}

	/* Drain taskqueue */
	if (txq->enqueue_tq) {
		while (taskqueue_cancel(txq->enqueue_tq,
		    &txq->enqueue_task, NULL)) {
			taskqueue_drain(txq->enqueue_tq,
			    &txq->enqueue_task);
		}

		taskqueue_free(txq->enqueue_tq);
	}

	if (txq->tx_buf_info) {
		/* Free all mbufs which are still in-flight */
		for (i = 0; i < MAX_SEND_BUFFERS_PER_QUEUE; i++) {
			txbuf_info = &txq->tx_buf_info[i];
			if (txbuf_info->mbuf) {
				mana_tx_unmap_mbuf(apc, txbuf_info);
			}
		}

		free(txq->tx_buf_info, M_DEVBUF);
	}

	mana_free_counters((counter_u64_t *)&txq->stats,
	    sizeof(txq->stats));

	mana_gd_destroy_queue(gd->gdma_context, txq->gdma_sq);

	mtx_destroy(&txq->txq_mtx);
}

static void
mana_destroy_txq(struct mana_port_context *apc)
{
	int i;

	if (!apc->tx_qp)
		return;

	for (i = 0; i < apc->num_queues; i++) {
		mana_destroy_wq_obj(apc, GDMA_SQ, apc->tx_qp[i].tx_object);

		mana_deinit_cq(apc, &apc->tx_qp[i].tx_cq);

		mana_deinit_txq(apc, &apc->tx_qp[i].txq);
	}

	free(apc->tx_qp, M_DEVBUF);
	apc->tx_qp = NULL;
}

static int
mana_create_txq(struct mana_port_context *apc, struct ifnet *net)
{
	struct mana_context *ac = apc->ac;
	struct gdma_dev *gd = ac->gdma_dev;
	struct mana_obj_spec wq_spec;
	struct mana_obj_spec cq_spec;
	struct gdma_queue_spec spec;
	struct gdma_context *gc;
	struct mana_txq *txq;
	struct mana_cq *cq;
	uint32_t txq_size;
	uint32_t cq_size;
	int err;
	int i;

	apc->tx_qp = mallocarray(apc->num_queues, sizeof(struct mana_tx_qp),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (!apc->tx_qp)
		return ENOMEM;

	/*  The minimum size of the WQE is 32 bytes, hence
	 *  MAX_SEND_BUFFERS_PER_QUEUE represents the maximum number of WQEs
	 *  the SQ can store. This value is then used to size other queues
	 *  to prevent overflow.
	 */
	txq_size = MAX_SEND_BUFFERS_PER_QUEUE * 32;
	KASSERT(IS_ALIGNED(txq_size, PAGE_SIZE),
	    ("txq size not page aligned"));

	cq_size = MAX_SEND_BUFFERS_PER_QUEUE * COMP_ENTRY_SIZE;
	cq_size = ALIGN(cq_size, PAGE_SIZE);

	gc = gd->gdma_context;

	for (i = 0; i < apc->num_queues; i++) {
		apc->tx_qp[i].tx_object = INVALID_MANA_HANDLE;

		/* Create SQ */
		txq = &apc->tx_qp[i].txq;

		txq->ndev = net;
		txq->vp_offset = apc->tx_vp_offset;
		txq->idx = i;
		txq->alt_txq_idx = i;

		memset(&spec, 0, sizeof(spec));
		spec.type = GDMA_SQ;
		spec.monitor_avl_buf = true;
		spec.queue_size = txq_size;
		err = mana_gd_create_mana_wq_cq(gd, &spec, &txq->gdma_sq);
		if (err)
			goto out;

		/* Create SQ's CQ */
		cq = &apc->tx_qp[i].tx_cq;
		cq->type = MANA_CQ_TYPE_TX;

		cq->txq = txq;

		memset(&spec, 0, sizeof(spec));
		spec.type = GDMA_CQ;
		spec.monitor_avl_buf = false;
		spec.queue_size = cq_size;
		spec.cq.callback = mana_schedule_task;
		spec.cq.parent_eq = ac->eqs[i].eq;
		spec.cq.context = cq;
		err = mana_gd_create_mana_wq_cq(gd, &spec, &cq->gdma_cq);
		if (err)
			goto out;

		memset(&wq_spec, 0, sizeof(wq_spec));
		memset(&cq_spec, 0, sizeof(cq_spec));

		wq_spec.gdma_region = txq->gdma_sq->mem_info.gdma_region;
		wq_spec.queue_size = txq->gdma_sq->queue_size;

		cq_spec.gdma_region = cq->gdma_cq->mem_info.gdma_region;
		cq_spec.queue_size = cq->gdma_cq->queue_size;
		cq_spec.modr_ctx_id = 0;
		cq_spec.attached_eq = cq->gdma_cq->cq.parent->id;

		err = mana_create_wq_obj(apc, apc->port_handle, GDMA_SQ,
		    &wq_spec, &cq_spec, &apc->tx_qp[i].tx_object);

		if (err)
			goto out;

		txq->gdma_sq->id = wq_spec.queue_index;
		cq->gdma_cq->id = cq_spec.queue_index;

		txq->gdma_sq->mem_info.gdma_region = GDMA_INVALID_DMA_REGION;
		cq->gdma_cq->mem_info.gdma_region = GDMA_INVALID_DMA_REGION;

		txq->gdma_txq_id = txq->gdma_sq->id;

		cq->gdma_id = cq->gdma_cq->id;

		mana_dbg(NULL,
		    "txq %d, txq gdma id %d, txq cq gdma id %d\n",
		    i, txq->gdma_txq_id, cq->gdma_id);;

		if (cq->gdma_id >= gc->max_num_cqs) {
			if_printf(net, "CQ id %u too large.\n", cq->gdma_id);
			return EINVAL;
		}

		gc->cq_table[cq->gdma_id] = cq->gdma_cq;

		/* Initialize tx specific data */
		txq->tx_buf_info = malloc(MAX_SEND_BUFFERS_PER_QUEUE *
		    sizeof(struct mana_send_buf_info),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (unlikely(txq->tx_buf_info == NULL)) {
			if_printf(net,
			    "Failed to allocate tx buf info for SQ %u\n",
			    txq->gdma_sq->id);
			err = ENOMEM;
			goto out;
		}


		snprintf(txq->txq_mtx_name, nitems(txq->txq_mtx_name),
		    "mana:tx(%d)", i);
		mtx_init(&txq->txq_mtx, txq->txq_mtx_name, NULL, MTX_DEF);

		txq->txq_br = buf_ring_alloc(4 * MAX_SEND_BUFFERS_PER_QUEUE,
		    M_DEVBUF, M_WAITOK, &txq->txq_mtx);
		if (unlikely(txq->txq_br == NULL)) {
			if_printf(net,
			    "Failed to allocate buf ring for SQ %u\n",
			    txq->gdma_sq->id);
			err = ENOMEM;
			goto out;
		}

		/* Allocate taskqueue for deferred send */
		TASK_INIT(&txq->enqueue_task, 0, mana_xmit_taskfunc, txq);
		txq->enqueue_tq = taskqueue_create_fast("mana_tx_enque",
		    M_NOWAIT, taskqueue_thread_enqueue, &txq->enqueue_tq);
		if (unlikely(txq->enqueue_tq == NULL)) {
			if_printf(net,
			    "Unable to create tx %d enqueue task queue\n", i);
			err = ENOMEM;
			goto out;
		}
		taskqueue_start_threads(&txq->enqueue_tq, 1, PI_NET,
		    "mana txq p%u-tx%d", apc->port_idx, i);

		mana_alloc_counters((counter_u64_t *)&txq->stats,
		    sizeof(txq->stats));

		/* Allocate and start the cleanup task on CQ */
		cq->do_not_ring_db = false;

		NET_TASK_INIT(&cq->cleanup_task, 0, mana_poll, cq);
		cq->cleanup_tq =
		    taskqueue_create_fast("mana tx cq cleanup",
		    M_WAITOK, taskqueue_thread_enqueue,
		    &cq->cleanup_tq);

		if (apc->last_tx_cq_bind_cpu < 0)
			apc->last_tx_cq_bind_cpu = CPU_FIRST();
		cq->cpu = apc->last_tx_cq_bind_cpu;
		apc->last_tx_cq_bind_cpu = CPU_NEXT(apc->last_tx_cq_bind_cpu);

		if (apc->bind_cleanup_thread_cpu) {
			cpuset_t cpu_mask;
			CPU_SETOF(cq->cpu, &cpu_mask);
			taskqueue_start_threads_cpuset(&cq->cleanup_tq,
			    1, PI_NET, &cpu_mask,
			    "mana cq p%u-tx%u-cpu%d",
			    apc->port_idx, txq->idx, cq->cpu);
		} else {
			taskqueue_start_threads(&cq->cleanup_tq, 1,
			    PI_NET, "mana cq p%u-tx%u",
			    apc->port_idx, txq->idx);
		}

		mana_gd_ring_cq(cq->gdma_cq, SET_ARM_BIT);
	}

	return 0;
out:
	mana_destroy_txq(apc);
	return err;
}

static void
mana_destroy_rxq(struct mana_port_context *apc, struct mana_rxq *rxq,
    bool validate_state)
{
	struct gdma_context *gc = apc->ac->gdma_dev->gdma_context;
	struct mana_recv_buf_oob *rx_oob;
	int i;

	if (!rxq)
		return;

	if (validate_state) {
		/*
		 * XXX Cancel and drain cleanup task queue here.
		 */
		;
	}

	mana_destroy_wq_obj(apc, GDMA_RQ, rxq->rxobj);

	mana_deinit_cq(apc, &rxq->rx_cq);

	mana_free_counters((counter_u64_t *)&rxq->stats,
	    sizeof(rxq->stats));

	/* Free LRO resources */
	tcp_lro_free(&rxq->lro);

	for (i = 0; i < rxq->num_rx_buf; i++) {
		rx_oob = &rxq->rx_oobs[i];

		if (rx_oob->mbuf)
			mana_unload_rx_mbuf(apc, rxq, rx_oob, true);

		bus_dmamap_destroy(apc->rx_buf_tag, rx_oob->dma_map);
	}

	if (rxq->gdma_rq)
		mana_gd_destroy_queue(gc, rxq->gdma_rq);

	free(rxq, M_DEVBUF);
}

#define MANA_WQE_HEADER_SIZE 16
#define MANA_WQE_SGE_SIZE 16

static int
mana_alloc_rx_wqe(struct mana_port_context *apc,
    struct mana_rxq *rxq, uint32_t *rxq_size, uint32_t *cq_size)
{
	struct mana_recv_buf_oob *rx_oob;
	uint32_t buf_idx;
	int err;

	if (rxq->datasize == 0 || rxq->datasize > PAGE_SIZE) {
		mana_err(NULL,
		    "WARNING: Invalid rxq datasize %u\n", rxq->datasize);
	}

	*rxq_size = 0;
	*cq_size = 0;

	for (buf_idx = 0; buf_idx < rxq->num_rx_buf; buf_idx++) {
		rx_oob = &rxq->rx_oobs[buf_idx];
		memset(rx_oob, 0, sizeof(*rx_oob));

		err = bus_dmamap_create(apc->rx_buf_tag, 0,
		    &rx_oob->dma_map);
		if (err) {
			mana_err(NULL,
			    "Failed to  create rx DMA map for buf %d\n",
			    buf_idx);
			return err;
		}

		err = mana_load_rx_mbuf(apc, rxq, rx_oob, true);
		if (err) {
			mana_err(NULL,
			    "Failed to  create rx DMA map for buf %d\n",
			    buf_idx);
			bus_dmamap_destroy(apc->rx_buf_tag, rx_oob->dma_map);
			return err;
		}

		rx_oob->wqe_req.sgl = rx_oob->sgl;
		rx_oob->wqe_req.num_sge = rx_oob->num_sge;
		rx_oob->wqe_req.inline_oob_size = 0;
		rx_oob->wqe_req.inline_oob_data = NULL;
		rx_oob->wqe_req.flags = 0;
		rx_oob->wqe_req.client_data_unit = 0;

		*rxq_size += ALIGN(MANA_WQE_HEADER_SIZE +
				   MANA_WQE_SGE_SIZE * rx_oob->num_sge, 32);
		*cq_size += COMP_ENTRY_SIZE;
	}

	return 0;
}

static int
mana_push_wqe(struct mana_rxq *rxq)
{
	struct mana_recv_buf_oob *rx_oob;
	uint32_t buf_idx;
	int err;

	for (buf_idx = 0; buf_idx < rxq->num_rx_buf; buf_idx++) {
		rx_oob = &rxq->rx_oobs[buf_idx];

		err = mana_gd_post_and_ring(rxq->gdma_rq, &rx_oob->wqe_req,
		    &rx_oob->wqe_inf);
		if (err)
			return ENOSPC;
	}

	return 0;
}

static struct mana_rxq *
mana_create_rxq(struct mana_port_context *apc, uint32_t rxq_idx,
    struct mana_eq *eq, struct ifnet *ndev)
{
	struct gdma_dev *gd = apc->ac->gdma_dev;
	struct mana_obj_spec wq_spec;
	struct mana_obj_spec cq_spec;
	struct gdma_queue_spec spec;
	struct mana_cq *cq = NULL;
	uint32_t cq_size, rq_size;
	struct gdma_context *gc;
	struct mana_rxq *rxq;
	int err;

	gc = gd->gdma_context;

	rxq = malloc(sizeof(*rxq) +
	    RX_BUFFERS_PER_QUEUE * sizeof(struct mana_recv_buf_oob),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (!rxq)
		return NULL;

	rxq->ndev = ndev;
	rxq->num_rx_buf = RX_BUFFERS_PER_QUEUE;
	rxq->rxq_idx = rxq_idx;
	/*
	 * Minimum size is MCLBYTES(2048) bytes for a mbuf cluster.
	 * Now we just allow maximum size of 4096.
	 */
	rxq->datasize = ALIGN(apc->frame_size, MCLBYTES);
	if (rxq->datasize > MAX_FRAME_SIZE)
		rxq->datasize = MAX_FRAME_SIZE;

	mana_dbg(NULL, "Setting rxq %d datasize %d\n",
	    rxq_idx, rxq->datasize);

	rxq->rxobj = INVALID_MANA_HANDLE;

	err = mana_alloc_rx_wqe(apc, rxq, &rq_size, &cq_size);
	if (err)
		goto out;

	/* Create LRO for the RQ */
	if (ndev->if_capenable & IFCAP_LRO) {
		err = tcp_lro_init(&rxq->lro);
		if (err) {
			if_printf(ndev, "Failed to create LRO for rxq %d\n",
			    rxq_idx);
		} else {
			rxq->lro.ifp = ndev;
		}
	}

	mana_alloc_counters((counter_u64_t *)&rxq->stats,
	    sizeof(rxq->stats));

	rq_size = ALIGN(rq_size, PAGE_SIZE);
	cq_size = ALIGN(cq_size, PAGE_SIZE);

	/* Create RQ */
	memset(&spec, 0, sizeof(spec));
	spec.type = GDMA_RQ;
	spec.monitor_avl_buf = true;
	spec.queue_size = rq_size;
	err = mana_gd_create_mana_wq_cq(gd, &spec, &rxq->gdma_rq);
	if (err)
		goto out;

	/* Create RQ's CQ */
	cq = &rxq->rx_cq;
	cq->type = MANA_CQ_TYPE_RX;
	cq->rxq = rxq;

	memset(&spec, 0, sizeof(spec));
	spec.type = GDMA_CQ;
	spec.monitor_avl_buf = false;
	spec.queue_size = cq_size;
	spec.cq.callback = mana_schedule_task;
	spec.cq.parent_eq = eq->eq;
	spec.cq.context = cq;
	err = mana_gd_create_mana_wq_cq(gd, &spec, &cq->gdma_cq);
	if (err)
		goto out;

	memset(&wq_spec, 0, sizeof(wq_spec));
	memset(&cq_spec, 0, sizeof(cq_spec));
	wq_spec.gdma_region = rxq->gdma_rq->mem_info.gdma_region;
	wq_spec.queue_size = rxq->gdma_rq->queue_size;

	cq_spec.gdma_region = cq->gdma_cq->mem_info.gdma_region;
	cq_spec.queue_size = cq->gdma_cq->queue_size;
	cq_spec.modr_ctx_id = 0;
	cq_spec.attached_eq = cq->gdma_cq->cq.parent->id;

	err = mana_create_wq_obj(apc, apc->port_handle, GDMA_RQ,
	    &wq_spec, &cq_spec, &rxq->rxobj);
	if (err)
		goto out;

	rxq->gdma_rq->id = wq_spec.queue_index;
	cq->gdma_cq->id = cq_spec.queue_index;

	rxq->gdma_rq->mem_info.gdma_region = GDMA_INVALID_DMA_REGION;
	cq->gdma_cq->mem_info.gdma_region = GDMA_INVALID_DMA_REGION;

	rxq->gdma_id = rxq->gdma_rq->id;
	cq->gdma_id = cq->gdma_cq->id;

	err = mana_push_wqe(rxq);
	if (err)
		goto out;

	if (cq->gdma_id >= gc->max_num_cqs)
		goto out;

	gc->cq_table[cq->gdma_id] = cq->gdma_cq;

	/* Allocate and start the cleanup task on CQ */
	cq->do_not_ring_db = false;

	NET_TASK_INIT(&cq->cleanup_task, 0, mana_poll, cq);
	cq->cleanup_tq =
	    taskqueue_create_fast("mana rx cq cleanup",
	    M_WAITOK, taskqueue_thread_enqueue,
	    &cq->cleanup_tq);

	if (apc->last_rx_cq_bind_cpu < 0)
		apc->last_rx_cq_bind_cpu = CPU_FIRST();
	cq->cpu = apc->last_rx_cq_bind_cpu;
	apc->last_rx_cq_bind_cpu = CPU_NEXT(apc->last_rx_cq_bind_cpu);

	if (apc->bind_cleanup_thread_cpu) {
		cpuset_t cpu_mask;
		CPU_SETOF(cq->cpu, &cpu_mask);
		taskqueue_start_threads_cpuset(&cq->cleanup_tq,
		    1, PI_NET, &cpu_mask,
		    "mana cq p%u-rx%u-cpu%d",
		    apc->port_idx, rxq->rxq_idx, cq->cpu);
	} else {
		taskqueue_start_threads(&cq->cleanup_tq, 1,
		    PI_NET, "mana cq p%u-rx%u",
		    apc->port_idx, rxq->rxq_idx);
	}

	mana_gd_ring_cq(cq->gdma_cq, SET_ARM_BIT);
out:
	if (!err)
		return rxq;

	if_printf(ndev, "Failed to create RXQ: err = %d\n", err);

	mana_destroy_rxq(apc, rxq, false);

	if (cq)
		mana_deinit_cq(apc, cq);

	return NULL;
}

static int
mana_add_rx_queues(struct mana_port_context *apc, struct ifnet *ndev)
{
	struct mana_context *ac = apc->ac;
	struct mana_rxq *rxq;
	int err = 0;
	int i;

	for (i = 0; i < apc->num_queues; i++) {
		rxq = mana_create_rxq(apc, i, &ac->eqs[i], ndev);
		if (!rxq) {
			err = ENOMEM;
			goto out;
		}

		apc->rxqs[i] = rxq;
	}

	apc->default_rxobj = apc->rxqs[0]->rxobj;
out:
	return err;
}

static void
mana_destroy_vport(struct mana_port_context *apc)
{
	struct mana_rxq *rxq;
	uint32_t rxq_idx;

	for (rxq_idx = 0; rxq_idx < apc->num_queues; rxq_idx++) {
		rxq = apc->rxqs[rxq_idx];
		if (!rxq)
			continue;

		mana_destroy_rxq(apc, rxq, true);
		apc->rxqs[rxq_idx] = NULL;
	}

	mana_destroy_txq(apc);
}

static int
mana_create_vport(struct mana_port_context *apc, struct ifnet *net)
{
	struct gdma_dev *gd = apc->ac->gdma_dev;
	int err;

	apc->default_rxobj = INVALID_MANA_HANDLE;

	err = mana_cfg_vport(apc, gd->pdid, gd->doorbell);
	if (err)
		return err;

	return mana_create_txq(apc, net);
}


static void mana_rss_table_init(struct mana_port_context *apc)
{
	int i;

	for (i = 0; i < MANA_INDIRECT_TABLE_SIZE; i++)
		apc->indir_table[i] = i % apc->num_queues;
}

int mana_config_rss(struct mana_port_context *apc, enum TRI_STATE rx,
		    bool update_hash, bool update_tab)
{
	uint32_t queue_idx;
	int i;

	if (update_tab) {
		for (i = 0; i < MANA_INDIRECT_TABLE_SIZE; i++) {
			queue_idx = apc->indir_table[i];
			apc->rxobj_table[i] = apc->rxqs[queue_idx]->rxobj;
		}
	}

	return mana_cfg_vport_steering(apc, rx, true, update_hash, update_tab);
}

static int
mana_init_port(struct ifnet *ndev)
{
	struct mana_port_context *apc = if_getsoftc(ndev);
	uint32_t max_txq, max_rxq, max_queues;
	int port_idx = apc->port_idx;
	uint32_t num_indirect_entries;
	int err;

	err = mana_init_port_context(apc);
	if (err)
		return err;

	err = mana_query_vport_cfg(apc, port_idx, &max_txq, &max_rxq,
	    &num_indirect_entries);
	if (err) {
		if_printf(ndev, "Failed to query info for vPort 0\n");
		goto reset_apc;
	}

	max_queues = min_t(uint32_t, max_txq, max_rxq);
	if (apc->max_queues > max_queues)
		apc->max_queues = max_queues;

	if (apc->num_queues > apc->max_queues)
		apc->num_queues = apc->max_queues;

	return 0;

reset_apc:
	bus_dma_tag_destroy(apc->rx_buf_tag);
	apc->rx_buf_tag = NULL;
	free(apc->rxqs, M_DEVBUF);
	apc->rxqs = NULL;
	return err;
}

int
mana_alloc_queues(struct ifnet *ndev)
{
	struct mana_port_context *apc = if_getsoftc(ndev);
	int err;

	err = mana_create_vport(apc, ndev);
	if (err)
		return err;

	err = mana_add_rx_queues(apc, ndev);
	if (err)
		goto destroy_vport;

	apc->rss_state = apc->num_queues > 1 ? TRI_STATE_TRUE : TRI_STATE_FALSE;

	mana_rss_table_init(apc);

	err = mana_config_rss(apc, TRI_STATE_TRUE, true, true);
	if (err)
		goto destroy_vport;

	return 0;

destroy_vport:
	mana_destroy_vport(apc);
	return err;
}

static int
mana_up(struct mana_port_context *apc)
{
	int err;

	mana_dbg(NULL, "mana_up called\n");

	err = mana_alloc_queues(apc->ndev);
	if (err) {
		mana_err(NULL, "Faile alloc mana queues: %d\n", err);
		return err;
	}

	/* Add queue specific sysctl */
	mana_sysctl_add_queues(apc);

	apc->port_is_up = true;

	/* Ensure port state updated before txq state */
	wmb();

	if_link_state_change(apc->ndev, LINK_STATE_UP);
	if_setdrvflagbits(apc->ndev, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	return 0;
}


static void
mana_init(void *arg)
{
	struct mana_port_context *apc = (struct mana_port_context *)arg;

	MANA_APC_LOCK_LOCK(apc);
	if (!apc->port_is_up) {
		mana_up(apc);
	}
	MANA_APC_LOCK_UNLOCK(apc);
}

static int
mana_dealloc_queues(struct ifnet *ndev)
{
	struct mana_port_context *apc = if_getsoftc(ndev);
	struct mana_txq *txq;
	int i, err;

	if (apc->port_is_up)
		return EINVAL;

	/* No packet can be transmitted now since apc->port_is_up is false.
	 * There is still a tiny chance that mana_poll_tx_cq() can re-enable
	 * a txq because it may not timely see apc->port_is_up being cleared
	 * to false, but it doesn't matter since mana_start_xmit() drops any
	 * new packets due to apc->port_is_up being false.
	 *
	 * Drain all the in-flight TX packets
	 */
	for (i = 0; i < apc->num_queues; i++) {
		txq = &apc->tx_qp[i].txq;

		struct mana_cq *tx_cq = &apc->tx_qp[i].tx_cq;
		struct mana_cq *rx_cq = &(apc->rxqs[i]->rx_cq);

		tx_cq->do_not_ring_db = true;
		rx_cq->do_not_ring_db = true;

		/* Schedule a cleanup task */
		taskqueue_enqueue(tx_cq->cleanup_tq, &tx_cq->cleanup_task);

		while (atomic_read(&txq->pending_sends) > 0)
			usleep_range(1000, 2000);
	}

	/* We're 100% sure the queues can no longer be woken up, because
	 * we're sure now mana_poll_tx_cq() can't be running.
	 */

	apc->rss_state = TRI_STATE_FALSE;
	err = mana_config_rss(apc, TRI_STATE_FALSE, false, false);
	if (err) {
		if_printf(ndev, "Failed to disable vPort: %d\n", err);
		return err;
	}

	/* TODO: Implement RX fencing */
	gdma_msleep(1000);

	mana_destroy_vport(apc);

	return 0;
}

static int
mana_down(struct mana_port_context *apc)
{
	int err = 0;

	apc->port_st_save = apc->port_is_up;
	apc->port_is_up = false;

	/* Ensure port state updated before txq state */
	wmb();

	if (apc->port_st_save) {
		if_setdrvflagbits(apc->ndev, IFF_DRV_OACTIVE,
		    IFF_DRV_RUNNING);
		if_link_state_change(apc->ndev, LINK_STATE_DOWN);

		mana_sysctl_free_queues(apc);

		err = mana_dealloc_queues(apc->ndev);
		if (err) {
			if_printf(apc->ndev,
			    "Failed to bring down mana interface: %d\n", err);
		}
	}

	return err;
}

int
mana_detach(struct ifnet *ndev)
{
	struct mana_port_context *apc = if_getsoftc(ndev);
	int err;

	ether_ifdetach(ndev);

	if (!apc)
		return 0;

	MANA_APC_LOCK_LOCK(apc);
	err = mana_down(apc);
	MANA_APC_LOCK_UNLOCK(apc);

	mana_cleanup_port_context(apc);

	MANA_APC_LOCK_DESTROY(apc);

	free(apc, M_DEVBUF);

	return err;
}

static int
mana_probe_port(struct mana_context *ac, int port_idx,
    struct ifnet **ndev_storage)
{
	struct gdma_context *gc = ac->gdma_dev->gdma_context;
	struct mana_port_context *apc;
	struct ifnet *ndev;
	int err;

	ndev = if_alloc_dev(IFT_ETHER, gc->dev);
	if (!ndev) {
		mana_err(NULL, "Failed to allocate ifnet struct\n");
		return ENOMEM;
	}

	*ndev_storage = ndev;

	apc = malloc(sizeof(*apc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!apc) {
		mana_err(NULL, "Failed to allocate port context\n");
		err = ENOMEM;
		goto free_net;
	}

	apc->ac = ac;
	apc->ndev = ndev;
	apc->max_queues = gc->max_num_queues;
	apc->num_queues = min_t(unsigned int,
	    gc->max_num_queues, MANA_MAX_NUM_QUEUES);
	apc->port_handle = INVALID_MANA_HANDLE;
	apc->port_idx = port_idx;
	apc->frame_size = DEFAULT_FRAME_SIZE;
	apc->last_tx_cq_bind_cpu = -1;
	apc->last_rx_cq_bind_cpu = -1;

	MANA_APC_LOCK_INIT(apc);

	if_initname(ndev, device_get_name(gc->dev), port_idx);
	if_setdev(ndev,gc->dev);
	if_setsoftc(ndev, apc);

	if_setflags(ndev, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setinitfn(ndev, mana_init);
	if_settransmitfn(ndev, mana_start_xmit);
	if_setqflushfn(ndev, mana_qflush);
	if_setioctlfn(ndev, mana_ioctl);
	if_setgetcounterfn(ndev, mana_get_counter);

	if_setmtu(ndev, ETHERMTU);
	if_setbaudrate(ndev, IF_Gbps(100));

	mana_rss_key_fill(apc->hashkey, MANA_HASH_KEY_SIZE);

	err = mana_init_port(ndev);
	if (err)
		goto reset_apc;

	ndev->if_capabilities |= IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6;
	ndev->if_capabilities |= IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6;
	ndev->if_capabilities |= IFCAP_TSO4 | IFCAP_TSO6;

	ndev->if_capabilities |= IFCAP_LRO | IFCAP_LINKSTATE;

	/* Enable all available capabilities by default. */
	ndev->if_capenable = ndev->if_capabilities;

	/* TSO parameters */
	ndev->if_hw_tsomax = MAX_MBUF_FRAGS * MANA_TSO_MAXSEG_SZ -
	    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
	ndev->if_hw_tsomaxsegcount = MAX_MBUF_FRAGS;
	ndev->if_hw_tsomaxsegsize = PAGE_SIZE;

	ifmedia_init(&apc->media, IFM_IMASK,
	    mana_ifmedia_change, mana_ifmedia_status);
	ifmedia_add(&apc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&apc->media, IFM_ETHER | IFM_AUTO);

	ether_ifattach(ndev, apc->mac_addr);

	/* Initialize statistics */
	mana_alloc_counters((counter_u64_t *)&apc->port_stats,
	    sizeof(struct mana_port_stats));
	mana_sysctl_add_port(apc);

	/* Tell the stack that the interface is not active */
	if_setdrvflagbits(ndev, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);

	return 0;

reset_apc:
	free(apc, M_DEVBUF);
free_net:
	*ndev_storage = NULL;
	if_printf(ndev, "Failed to probe vPort %d: %d\n", port_idx, err);
	if_free(ndev);
	return err;
}

int mana_probe(struct gdma_dev *gd)
{
	struct gdma_context *gc = gd->gdma_context;
	device_t dev = gc->dev;
	struct mana_context *ac;
	int err;
	int i;

	device_printf(dev, "%s protocol version: %d.%d.%d\n", DEVICE_NAME,
		 MANA_MAJOR_VERSION, MANA_MINOR_VERSION, MANA_MICRO_VERSION);

	err = mana_gd_register_device(gd);
	if (err)
		return err;

	ac = malloc(sizeof(*ac), M_DEVBUF, M_WAITOK | M_ZERO);
	if (!ac)
		return ENOMEM;

	ac->gdma_dev = gd;
	ac->num_ports = 1;
	gd->driver_data = ac;

	err = mana_create_eq(ac);
	if (err)
		goto out;

	err = mana_query_device_cfg(ac, MANA_MAJOR_VERSION, MANA_MINOR_VERSION,
	    MANA_MICRO_VERSION, &ac->num_ports);
	if (err)
		goto out;

	if (ac->num_ports > MAX_PORTS_IN_MANA_DEV)
		ac->num_ports = MAX_PORTS_IN_MANA_DEV;

	for (i = 0; i < ac->num_ports; i++) {
		err = mana_probe_port(ac, i, &ac->ports[i]);
		if (err) {
			device_printf(dev,
			    "Failed to probe mana port %d\n", i);
			break;
		}
	}

out:
	if (err)
		mana_remove(gd);

	return err;
}

void
mana_remove(struct gdma_dev *gd)
{
	struct gdma_context *gc = gd->gdma_context;
	struct mana_context *ac = gd->driver_data;
	device_t dev = gc->dev;
	struct ifnet *ndev;
	int i;

	for (i = 0; i < ac->num_ports; i++) {
		ndev = ac->ports[i];
		if (!ndev) {
			if (i == 0)
				device_printf(dev, "No net device to remove\n");
			goto out;
		}

		mana_detach(ndev);

		if_free(ndev);
	}

	mana_destroy_eq(ac);

out:
	mana_gd_deregister_device(gd);
	gd->driver_data = NULL;
	gd->gdma_context = NULL;
	free(ac, M_DEVBUF);
}
