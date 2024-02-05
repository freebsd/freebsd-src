/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifndef _IF_VTNETVAR_H
#define _IF_VTNETVAR_H

#ifdef ALTQ
#define	VTNET_LEGACY_TX
#endif

struct vtnet_softc;

struct vtnet_statistics {
	uint64_t	mbuf_alloc_failed;

	uint64_t	rx_frame_too_large;
	uint64_t	rx_enq_replacement_failed;
	uint64_t	rx_mergeable_failed;
	uint64_t	rx_csum_bad_ethtype;
	uint64_t	rx_csum_bad_ipproto;
	uint64_t	rx_csum_bad_offset;
	uint64_t	rx_csum_bad_proto;
	uint64_t	tx_csum_unknown_ethtype;
	uint64_t	tx_csum_proto_mismatch;
	uint64_t	tx_tso_not_tcp;
	uint64_t	tx_tso_without_csum;
	uint64_t	tx_defragged;
	uint64_t	tx_defrag_failed;

	/*
	 * These are accumulated from each Rx/Tx queue.
	 */
	uint64_t	rx_csum_failed;
	uint64_t	rx_csum_offloaded;
	uint64_t	rx_task_rescheduled;
	uint64_t	tx_csum_offloaded;
	uint64_t	tx_tso_offloaded;
	uint64_t	tx_task_rescheduled;
};

struct vtnet_rxq_stats {
	uint64_t	vrxs_ipackets;	/* if_ipackets */
	uint64_t	vrxs_ibytes;	/* if_ibytes */
	uint64_t	vrxs_iqdrops;	/* if_iqdrops */
	uint64_t	vrxs_ierrors;	/* if_ierrors */
	uint64_t	vrxs_csum;
	uint64_t	vrxs_csum_failed;
	uint64_t	vrxs_host_lro;
	uint64_t	vrxs_rescheduled;
};

struct vtnet_rxq {
	struct mtx		 vtnrx_mtx;
	struct vtnet_softc	*vtnrx_sc;
	struct virtqueue	*vtnrx_vq;
	struct sglist		*vtnrx_sg;
	int			 vtnrx_id;
	struct vtnet_rxq_stats	 vtnrx_stats;
	struct taskqueue	*vtnrx_tq;
	struct task		 vtnrx_intrtask;
	struct lro_ctrl		 vtnrx_lro;
#ifdef DEV_NETMAP
	uint32_t		 vtnrx_nm_refill;
	struct virtio_net_hdr_mrg_rxbuf vtnrx_shrhdr;
#endif  /* DEV_NETMAP */
	char			 vtnrx_name[16];
} __aligned(CACHE_LINE_SIZE);

#define VTNET_RXQ_LOCK(_rxq)	mtx_lock(&(_rxq)->vtnrx_mtx)
#define VTNET_RXQ_UNLOCK(_rxq)	mtx_unlock(&(_rxq)->vtnrx_mtx)
#define VTNET_RXQ_LOCK_ASSERT(_rxq)		\
    mtx_assert(&(_rxq)->vtnrx_mtx, MA_OWNED)
#define VTNET_RXQ_LOCK_ASSERT_NOTOWNED(_rxq)	\
    mtx_assert(&(_rxq)->vtnrx_mtx, MA_NOTOWNED)

struct vtnet_txq_stats {
	uint64_t vtxs_opackets;	/* if_opackets */
	uint64_t vtxs_obytes;	/* if_obytes */
	uint64_t vtxs_omcasts;	/* if_omcasts */
	uint64_t vtxs_csum;
	uint64_t vtxs_tso;
	uint64_t vtxs_rescheduled;
};

struct vtnet_txq {
	struct mtx		 vtntx_mtx;
	struct vtnet_softc	*vtntx_sc;
	struct virtqueue	*vtntx_vq;
	struct sglist		*vtntx_sg;
#ifndef VTNET_LEGACY_TX
	struct buf_ring		*vtntx_br;
#endif
	int			 vtntx_id;
	int			 vtntx_watchdog;
	int			 vtntx_intr_threshold;
	struct vtnet_txq_stats	 vtntx_stats;
	struct taskqueue	*vtntx_tq;
	struct task		 vtntx_intrtask;
#ifndef VTNET_LEGACY_TX
	struct task		 vtntx_defrtask;
#endif
#ifdef DEV_NETMAP
	struct virtio_net_hdr_mrg_rxbuf vtntx_shrhdr;
#endif  /* DEV_NETMAP */
	char			 vtntx_name[16];
} __aligned(CACHE_LINE_SIZE);

#define VTNET_TXQ_LOCK(_txq)	mtx_lock(&(_txq)->vtntx_mtx)
#define VTNET_TXQ_TRYLOCK(_txq)	mtx_trylock(&(_txq)->vtntx_mtx)
#define VTNET_TXQ_UNLOCK(_txq)	mtx_unlock(&(_txq)->vtntx_mtx)
#define VTNET_TXQ_LOCK_ASSERT(_txq)		\
    mtx_assert(&(_txq)->vtntx_mtx, MA_OWNED)
#define VTNET_TXQ_LOCK_ASSERT_NOTOWNED(_txq)	\
    mtx_assert(&(_txq)->vtntx_mtx, MA_NOTOWNED)

struct vtnet_softc {
	device_t		 vtnet_dev;
	struct ifnet		*vtnet_ifp;
	struct vtnet_rxq	*vtnet_rxqs;
	struct vtnet_txq	*vtnet_txqs;
	pfil_head_t		 vtnet_pfil;
	uint64_t		 vtnet_features;

	uint32_t		 vtnet_flags;
#define VTNET_FLAG_MODERN	 0x0001
#define VTNET_FLAG_MAC		 0x0002
#define VTNET_FLAG_CTRL_VQ	 0x0004
#define VTNET_FLAG_CTRL_RX	 0x0008
#define VTNET_FLAG_CTRL_MAC	 0x0010
#define VTNET_FLAG_VLAN_FILTER	 0x0020
#define VTNET_FLAG_TSO_ECN	 0x0040
#define VTNET_FLAG_MRG_RXBUFS	 0x0080
#define VTNET_FLAG_LRO_NOMRG	 0x0100
#define VTNET_FLAG_MQ		 0x0200
#define VTNET_FLAG_INDIRECT	 0x0400
#define VTNET_FLAG_EVENT_IDX	 0x0800
#define VTNET_FLAG_SUSPENDED	 0x1000
#define VTNET_FLAG_FIXUP_NEEDS_CSUM 0x2000
#define VTNET_FLAG_SW_LRO	 0x4000

	u_int			 vtnet_hdr_size;
	int			 vtnet_rx_nmbufs;
	int			 vtnet_rx_clustersz;
	int			 vtnet_rx_nsegs;
	int			 vtnet_rx_process_limit;
	int			 vtnet_link_active;
	int			 vtnet_act_vq_pairs;
	int			 vtnet_req_vq_pairs;
	int			 vtnet_max_vq_pairs;
	int			 vtnet_tx_nsegs;
	int			 vtnet_if_flags;
	u_int			 vtnet_max_mtu;
	int			 vtnet_lro_entry_count;
	int			 vtnet_lro_mbufq_depth;

	struct virtqueue	*vtnet_ctrl_vq;
	struct vtnet_mac_filter	*vtnet_mac_filter;
	uint32_t		*vtnet_vlan_filter;

	uint64_t		 vtnet_negotiated_features;
	struct vtnet_statistics	 vtnet_stats;
	struct callout		 vtnet_tick_ch;
	struct ifmedia		 vtnet_media;
	eventhandler_tag	 vtnet_vlan_attach;
	eventhandler_tag	 vtnet_vlan_detach;

	struct mtx		 vtnet_mtx;
	char			 vtnet_mtx_name[16];
	uint8_t			 vtnet_hwaddr[ETHER_ADDR_LEN];
};

static bool
vtnet_modern(struct vtnet_softc *sc)
{
	return ((sc->vtnet_flags & VTNET_FLAG_MODERN) != 0);
}

static bool
vtnet_software_lro(struct vtnet_softc *sc)
{
	return ((sc->vtnet_flags & VTNET_FLAG_SW_LRO) != 0);
}

/*
 * Maximum number of queue pairs we will autoconfigure to.
 */
#define VTNET_MAX_QUEUE_PAIRS	32

/*
 * Additional completed entries can appear in a virtqueue before we can
 * reenable interrupts. Number of times to retry before scheduling the
 * taskqueue to process the completed entries.
 */
#define VTNET_INTR_DISABLE_RETRIES	4

/*
 * Similarly, additional completed entries can appear in a virtqueue
 * between when lasted checked and before notifying the host. Number
 * of times to retry before scheduling the taskqueue to process the
 * queue.
 */
#define VTNET_NOTIFY_RETRIES		4

/*
 * Number of words to allocate for the VLAN shadow table. There is one
 * bit for each VLAN.
 */
#define VTNET_VLAN_FILTER_NWORDS	(4096 / 32)

/*
 * We depend on all of the hdr structures being even, and matching the standard
 * length. As well, we depend on two being identally sized (with the same
 * layout).
 */
CTASSERT(sizeof(struct virtio_net_hdr_v1) == 12);
CTASSERT(sizeof(struct virtio_net_hdr) == 10);
CTASSERT(sizeof(struct virtio_net_hdr_mrg_rxbuf) ==
    sizeof(struct virtio_net_hdr_v1));

/*
 * In legacy VirtIO when mergeable buffers are not negotiated, this structure
 * is placed at the beginning of the mbuf data. Use 4 bytes of pad to keep
 * both the VirtIO header and the data non-contiguous and the frame's payload
 * 4 byte aligned. Note this padding would not be necessary if the
 * VIRTIO_F_ANY_LAYOUT feature was negotiated (but we don't support that yet).
 *
 * In modern VirtIO or when mergeable buffers are negotiated, the host puts
 * the VirtIO header in the beginning of the first mbuf's data.
 */
#define VTNET_RX_HEADER_PAD	4
struct vtnet_rx_header {
	struct virtio_net_hdr	vrh_hdr;
	char			vrh_pad[VTNET_RX_HEADER_PAD];
} __packed;

/*
 * For each outgoing frame, the vtnet_tx_header below is allocated from
 * the vtnet_tx_header_zone.
 */
struct vtnet_tx_header {
	union {
		struct virtio_net_hdr		hdr;
		struct virtio_net_hdr_mrg_rxbuf	mhdr;
		struct virtio_net_hdr_v1	v1hdr;
	} vth_uhdr;

	struct mbuf *vth_mbuf;
};

/*
 * The VirtIO specification does not place a limit on the number of MAC
 * addresses the guest driver may request to be filtered. In practice,
 * the host is constrained by available resources. To simplify this driver,
 * impose a reasonably high limit of MAC addresses we will filter before
 * falling back to promiscuous or all-multicast modes.
 */
#define VTNET_MAX_MAC_ENTRIES	128

/*
 * The driver version of struct virtio_net_ctrl_mac but with our predefined
 * number of MAC addresses allocated. This structure is shared with the host,
 * so nentries field is in the correct VirtIO endianness.
 */
struct vtnet_mac_table {
	uint32_t	nentries;
	uint8_t		macs[VTNET_MAX_MAC_ENTRIES][ETHER_ADDR_LEN];
} __packed;

struct vtnet_mac_filter {
	struct vtnet_mac_table	vmf_unicast;
	uint32_t		vmf_pad; /* Make tables non-contiguous. */
	struct vtnet_mac_table	vmf_multicast;
};

/*
 * The MAC filter table is malloc(9)'d when needed. Ensure it will
 * always fit in one segment.
 */
CTASSERT(sizeof(struct vtnet_mac_filter) <= PAGE_SIZE);

#define VTNET_TX_TIMEOUT	5
#define VTNET_CSUM_OFFLOAD	(CSUM_TCP | CSUM_UDP)
#define VTNET_CSUM_OFFLOAD_IPV6	(CSUM_TCP_IPV6 | CSUM_UDP_IPV6)

#define VTNET_CSUM_ALL_OFFLOAD	\
    (VTNET_CSUM_OFFLOAD | VTNET_CSUM_OFFLOAD_IPV6 | CSUM_TSO)

#define VTNET_COMMON_FEATURES \
    (VIRTIO_NET_F_MAC			| \
     VIRTIO_NET_F_STATUS		| \
     VIRTIO_NET_F_CTRL_GUEST_OFFLOADS	| \
     VIRTIO_NET_F_MTU			| \
     VIRTIO_NET_F_CTRL_VQ		| \
     VIRTIO_NET_F_CTRL_RX		| \
     VIRTIO_NET_F_CTRL_MAC_ADDR		| \
     VIRTIO_NET_F_CTRL_VLAN		| \
     VIRTIO_NET_F_CSUM			| \
     VIRTIO_NET_F_HOST_TSO4		| \
     VIRTIO_NET_F_HOST_TSO6		| \
     VIRTIO_NET_F_HOST_ECN		| \
     VIRTIO_NET_F_GUEST_CSUM		| \
     VIRTIO_NET_F_GUEST_TSO4		| \
     VIRTIO_NET_F_GUEST_TSO6		| \
     VIRTIO_NET_F_GUEST_ECN		| \
     VIRTIO_NET_F_MRG_RXBUF		| \
     VIRTIO_NET_F_MQ			| \
     VIRTIO_NET_F_SPEED_DUPLEX		| \
     VIRTIO_RING_F_EVENT_IDX		| \
     VIRTIO_RING_F_INDIRECT_DESC)

#define VTNET_MODERN_FEATURES (VTNET_COMMON_FEATURES)
#define VTNET_LEGACY_FEATURES (VTNET_COMMON_FEATURES | VIRTIO_NET_F_GSO)

/*
 * The VIRTIO_NET_F_HOST_TSO[46] features permit us to send the host
 * frames larger than 1514 bytes.
 */
#define VTNET_TSO_FEATURES (VIRTIO_NET_F_GSO | VIRTIO_NET_F_HOST_TSO4 | \
    VIRTIO_NET_F_HOST_TSO6 | VIRTIO_NET_F_HOST_ECN)

/*
 * The VIRTIO_NET_F_GUEST_TSO[46] features permit the host to send us
 * frames larger than 1514 bytes.
 */
#define VTNET_LRO_FEATURES (VIRTIO_NET_F_GUEST_TSO4 | \
    VIRTIO_NET_F_GUEST_TSO6 | VIRTIO_NET_F_GUEST_ECN)

#define VTNET_MIN_MTU		68
#define VTNET_MAX_MTU		65536
#define VTNET_MAX_RX_SIZE	65550

/*
 * Used to preallocate the VQ indirect descriptors. Modern and mergeable
 * buffers do not required one segment for the VirtIO header since it is
 * placed inline at the beginning of the receive buffer.
 */
#define VTNET_RX_SEGS_HDR_INLINE	1
#define VTNET_RX_SEGS_HDR_SEPARATE	2
#define VTNET_RX_SEGS_LRO_NOMRG		34
#define VTNET_TX_SEGS_MIN		32
#define VTNET_TX_SEGS_MAX		64

CTASSERT(((VTNET_RX_SEGS_LRO_NOMRG - 1) * MCLBYTES) >= VTNET_MAX_RX_SIZE);
CTASSERT(((VTNET_TX_SEGS_MAX - 1) * MCLBYTES) >= VTNET_MAX_MTU);

/*
 * Number of slots in the Tx bufrings. This value matches most other
 * multiqueue drivers.
 */
#define VTNET_DEFAULT_BUFRING_SIZE	4096

#define VTNET_CORE_MTX(_sc)		&(_sc)->vtnet_mtx
#define VTNET_CORE_LOCK(_sc)		mtx_lock(VTNET_CORE_MTX((_sc)))
#define VTNET_CORE_UNLOCK(_sc)		mtx_unlock(VTNET_CORE_MTX((_sc)))
#define VTNET_CORE_LOCK_DESTROY(_sc)	mtx_destroy(VTNET_CORE_MTX((_sc)))
#define VTNET_CORE_LOCK_ASSERT(_sc)		\
    mtx_assert(VTNET_CORE_MTX((_sc)), MA_OWNED)
#define VTNET_CORE_LOCK_ASSERT_NOTOWNED(_sc)	\
    mtx_assert(VTNET_CORE_MTX((_sc)), MA_NOTOWNED)

#define VTNET_CORE_LOCK_INIT(_sc) do {					\
    snprintf((_sc)->vtnet_mtx_name, sizeof((_sc)->vtnet_mtx_name),	\
        "%s", device_get_nameunit((_sc)->vtnet_dev));			\
    mtx_init(VTNET_CORE_MTX((_sc)), (_sc)->vtnet_mtx_name,		\
        "VTNET Core Lock", MTX_DEF);					\
} while (0)

/*
 * Values for the init_mode argument of vtnet_init_locked().
 */
#define VTNET_INIT_NETMAP_ENTER		1
#define VTNET_INIT_NETMAP_EXIT		2

#endif /* _IF_VTNETVAR_H */
