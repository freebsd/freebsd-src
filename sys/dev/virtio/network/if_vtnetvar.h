/*-
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
 *
 * $FreeBSD$
 */

#ifndef _IF_VTNETVAR_H
#define _IF_VTNETVAR_H

struct vtnet_statistics {
	unsigned long	mbuf_alloc_failed;

	unsigned long	rx_frame_too_large;
	unsigned long	rx_enq_replacement_failed;
	unsigned long	rx_mergeable_failed;
	unsigned long	rx_csum_bad_ethtype;
	unsigned long	rx_csum_bad_start;
	unsigned long	rx_csum_bad_ipproto;
	unsigned long	rx_csum_bad_offset;
	unsigned long	rx_csum_failed;
	unsigned long	rx_csum_offloaded;
	unsigned long	rx_task_rescheduled;

	unsigned long	tx_csum_offloaded;
	unsigned long	tx_tso_offloaded;
	unsigned long	tx_csum_bad_ethtype;
	unsigned long	tx_tso_bad_ethtype;
	unsigned long	tx_task_rescheduled;
};

struct vtnet_softc {
	device_t		 vtnet_dev;
	struct ifnet		*vtnet_ifp;
	struct mtx		 vtnet_mtx;

	uint32_t		 vtnet_flags;
#define VTNET_FLAG_LINK		 0x0001
#define VTNET_FLAG_SUSPENDED	 0x0002
#define VTNET_FLAG_CTRL_VQ	 0x0004
#define VTNET_FLAG_CTRL_RX	 0x0008
#define VTNET_FLAG_VLAN_FILTER	 0x0010
#define VTNET_FLAG_TSO_ECN	 0x0020
#define VTNET_FLAG_MRG_RXBUFS	 0x0040
#define VTNET_FLAG_LRO_NOMRG	 0x0080

	struct virtqueue	*vtnet_rx_vq;
	struct virtqueue	*vtnet_tx_vq;
	struct virtqueue	*vtnet_ctrl_vq;

	int			 vtnet_hdr_size;
	int			 vtnet_tx_size;
	int			 vtnet_rx_size;
	int			 vtnet_rx_process_limit;
	int			 vtnet_rx_mbuf_size;
	int			 vtnet_rx_mbuf_count;
	int			 vtnet_if_flags;
	int			 vtnet_watchdog_timer;
	uint64_t		 vtnet_features;

	struct vtnet_statistics	 vtnet_stats;

	struct callout		 vtnet_tick_ch;

	eventhandler_tag	 vtnet_vlan_attach;
	eventhandler_tag	 vtnet_vlan_detach;

	struct ifmedia		 vtnet_media;
	/*
	 * Fake media type; the host does not provide us with
	 * any real media information.
	 */
#define VTNET_MEDIATYPE		 (IFM_ETHER | IFM_1000_T | IFM_FDX)
	char			 vtnet_hwaddr[ETHER_ADDR_LEN];

	struct vtnet_mac_filter	*vtnet_mac_filter;
	/*
	 * During reset, the host's VLAN filtering table is lost. The
	 * array below is used to restore all the VLANs configured on
	 * this interface after a reset.
	 */
#define VTNET_VLAN_SHADOW_SIZE	 (4096 / 32)
	int			 vtnet_nvlans;
	uint32_t		 vtnet_vlan_shadow[VTNET_VLAN_SHADOW_SIZE];

	char			 vtnet_mtx_name[16];
};

/*
 * When mergeable buffers are not negotiated, the vtnet_rx_header structure
 * below is placed at the beginning of the mbuf data. Use 4 bytes of pad to
 * both keep the VirtIO header and the data non-contiguous and to keep the
 * frame's payload 4 byte aligned.
 *
 * When mergeable buffers are negotiated, the host puts the VirtIO header in
 * the beginning of the first mbuf's data.
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

#define VTNET_WATCHDOG_TIMEOUT	5
#define VTNET_CSUM_OFFLOAD	(CSUM_TCP | CSUM_UDP | CSUM_SCTP)

/* Features desired/implemented by this driver. */
#define VTNET_FEATURES \
    (VIRTIO_NET_F_MAC			| \
     VIRTIO_NET_F_STATUS		| \
     VIRTIO_NET_F_CTRL_VQ		| \
     VIRTIO_NET_F_CTRL_RX		| \
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
     VIRTIO_RING_F_INDIRECT_DESC)

/*
 * The VIRTIO_NET_F_GUEST_TSO[46] features permit the host to send us
 * frames larger than 1514 bytes. We do not yet support software LRO
 * via tcp_lro_rx().
 */
#define VTNET_LRO_FEATURES (VIRTIO_NET_F_GUEST_TSO4 | \
    VIRTIO_NET_F_GUEST_TSO6 | VIRTIO_NET_F_GUEST_ECN)

#define VTNET_MAX_MTU		65536
#define VTNET_MAX_RX_SIZE	65550

/*
 * Used to preallocate the Vq indirect descriptors. The first segment
 * is reserved for the header.
 */
#define VTNET_MIN_RX_SEGS	2
#define VTNET_MAX_RX_SEGS	34
#define VTNET_MAX_TX_SEGS	34

/*
 * Assert we can receive and transmit the maximum with regular
 * size clusters.
 */
CTASSERT(((VTNET_MAX_RX_SEGS - 1) * MCLBYTES) >= VTNET_MAX_RX_SIZE);
CTASSERT(((VTNET_MAX_TX_SEGS - 1) * MCLBYTES) >= VTNET_MAX_MTU);

/*
 * Determine how many mbufs are in each receive buffer. For LRO without
 * mergeable descriptors, we must allocate an mbuf chain large enough to
 * hold both the vtnet_rx_header and the maximum receivable data.
 */
#define VTNET_NEEDED_RX_MBUFS(_sc)					\
	((_sc)->vtnet_flags & VTNET_FLAG_LRO_NOMRG) == 0 ? 1 :		\
	    howmany(sizeof(struct vtnet_rx_header) + VTNET_MAX_RX_SIZE,	\
	        (_sc)->vtnet_rx_mbuf_size)

#define VTNET_MTX(_sc)		&(_sc)->vtnet_mtx
#define VTNET_LOCK(_sc)		mtx_lock(VTNET_MTX((_sc)))
#define VTNET_UNLOCK(_sc)	mtx_unlock(VTNET_MTX((_sc)))
#define VTNET_LOCK_DESTROY(_sc)	mtx_destroy(VTNET_MTX((_sc)))
#define VTNET_LOCK_ASSERT(_sc)	mtx_assert(VTNET_MTX((_sc)), MA_OWNED)
#define VTNET_LOCK_ASSERT_NOTOWNED(_sc)	\
	 			mtx_assert(VTNET_MTX((_sc)), MA_NOTOWNED)

#define VTNET_LOCK_INIT(_sc) do {					\
    snprintf((_sc)->vtnet_mtx_name, sizeof((_sc)->vtnet_mtx_name),	\
        "%s", device_get_nameunit((_sc)->vtnet_dev));			\
    mtx_init(VTNET_MTX((_sc)), (_sc)->vtnet_mtx_name,			\
        "VTNET Core Lock", MTX_DEF);					\
} while (0)

#endif /* _IF_VTNETVAR_H */
