/*-
 * Copyright (c) 2013 Tsubai Masanari
 * Copyright (c) 2013 Bryan Venteicher <bryanv@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#ifndef _IF_VMXVAR_H
#define _IF_VMXVAR_H

struct vmxnet3_softc;

struct vmxnet3_dma_alloc {
	bus_addr_t		dma_paddr;
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_size_t		dma_size;
};

/*
 * The number of Rx/Tx queues this driver prefers.
 */
#define VMXNET3_DEF_RX_QUEUES	8
#define VMXNET3_DEF_TX_QUEUES	8

/*
 * The number of Rx rings in each Rx queue.
 */
#define VMXNET3_RXRINGS_PERQ	2

/*
 * The number of descriptors in each Rx/Tx ring.
 */
#define VMXNET3_DEF_TX_NDESC		512
#define VMXNET3_MAX_TX_NDESC		4096
#define VMXNET3_MIN_TX_NDESC		32
#define VMXNET3_MASK_TX_NDESC		0x1F
#define VMXNET3_DEF_RX_NDESC		256
#define VMXNET3_MAX_RX_NDESC		2048
#define VMXNET3_MIN_RX_NDESC		32
#define VMXNET3_MASK_RX_NDESC		0x1F

#define VMXNET3_MAX_TX_NCOMPDESC	VMXNET3_MAX_TX_NDESC
#define VMXNET3_MAX_RX_NCOMPDESC \
    (VMXNET3_MAX_RX_NDESC * VMXNET3_RXRINGS_PERQ)

struct vmxnet3_txbuf {
	bus_dmamap_t		 vtxb_dmamap;
	struct mbuf		*vtxb_m;
};

struct vmxnet3_txring {
	struct vmxnet3_txbuf	*vxtxr_txbuf;
	u_int			 vxtxr_head;
	u_int			 vxtxr_next;
	u_int			 vxtxr_ndesc;
	int			 vxtxr_gen;
	bus_dma_tag_t		 vxtxr_txtag;
	struct vmxnet3_txdesc	*vxtxr_txd;
	struct vmxnet3_dma_alloc vxtxr_dma;
};

static inline int
VMXNET3_TXRING_AVAIL(struct vmxnet3_txring *txr)
{
	int avail = txr->vxtxr_next - txr->vxtxr_head - 1;
	return (avail < 0 ? txr->vxtxr_ndesc + avail : avail);
}

struct vmxnet3_rxbuf {
	bus_dmamap_t		 vrxb_dmamap;
	struct mbuf		*vrxb_m;
};

struct vmxnet3_rxring {
	struct vmxnet3_rxbuf	*vxrxr_rxbuf;
	struct vmxnet3_rxdesc	*vxrxr_rxd;
	u_int			 vxrxr_fill;
	u_int			 vxrxr_ndesc;
	int			 vxrxr_gen;
	int			 vxrxr_rid;
	bus_dma_tag_t		 vxrxr_rxtag;
	struct vmxnet3_dma_alloc vxrxr_dma;
	bus_dmamap_t		 vxrxr_spare_dmap;
};

static inline void
vmxnet3_rxr_increment_fill(struct vmxnet3_rxring *rxr)
{

	if (++rxr->vxrxr_fill == rxr->vxrxr_ndesc) {
		rxr->vxrxr_fill = 0;
		rxr->vxrxr_gen ^= 1;
	}
}

struct vmxnet3_comp_ring {
	union {
		struct vmxnet3_txcompdesc *txcd;
		struct vmxnet3_rxcompdesc *rxcd;
	}			 vxcr_u;
	u_int			 vxcr_next;
	u_int			 vxcr_ndesc;
	int			 vxcr_gen;
	struct vmxnet3_dma_alloc vxcr_dma;
};

struct vmxnet3_txq_stats {
	uint64_t		vmtxs_opackets;	/* if_opackets */
	uint64_t		vmtxs_obytes;	/* if_obytes */
	uint64_t		vmtxs_omcasts;	/* if_omcasts */
	uint64_t		vmtxs_csum;
	uint64_t		vmtxs_tso;
	uint64_t		vmtxs_full;
	uint64_t		vmtxs_offload_failed;
};

struct vmxnet3_txqueue {
	struct mtx			 vxtxq_mtx;
	struct vmxnet3_softc		*vxtxq_sc;
#ifndef VMXNET3_LEGACY_TX
	struct buf_ring			*vxtxq_br;
#endif
	int				 vxtxq_id;
	int				 vxtxq_intr_idx;
	int				 vxtxq_watchdog;
	struct vmxnet3_txring		 vxtxq_cmd_ring;
	struct vmxnet3_comp_ring	 vxtxq_comp_ring;
	struct vmxnet3_txq_stats	 vxtxq_stats;
	struct vmxnet3_txq_shared	*vxtxq_ts;
	struct sysctl_oid_list		*vxtxq_sysctl;
#ifndef VMXNET3_LEGACY_TX
	struct task			 vxtxq_defrtask;
#endif
	char				 vxtxq_name[16];
} __aligned(CACHE_LINE_SIZE);

#define VMXNET3_TXQ_LOCK(_txq)		mtx_lock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_TRYLOCK(_txq)	mtx_trylock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_UNLOCK(_txq)	mtx_unlock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_LOCK_ASSERT(_txq)		\
    mtx_assert(&(_txq)->vxtxq_mtx, MA_OWNED)
#define VMXNET3_TXQ_LOCK_ASSERT_NOTOWNED(_txq)	\
    mtx_assert(&(_txq)->vxtxq_mtx, MA_NOTOWNED)

struct vmxnet3_rxq_stats {
	uint64_t		vmrxs_ipackets;	/* if_ipackets */
	uint64_t		vmrxs_ibytes;	/* if_ibytes */
	uint64_t		vmrxs_iqdrops;	/* if_iqdrops */
	uint64_t		vmrxs_ierrors;	/* if_ierrors */
};

struct vmxnet3_rxqueue {
	struct mtx			 vxrxq_mtx;
	struct vmxnet3_softc		*vxrxq_sc;
	int				 vxrxq_id;
	int				 vxrxq_intr_idx;
	struct mbuf			*vxrxq_mhead;
	struct mbuf			*vxrxq_mtail;
	struct vmxnet3_rxring		 vxrxq_cmd_ring[VMXNET3_RXRINGS_PERQ];
	struct vmxnet3_comp_ring	 vxrxq_comp_ring;
	struct vmxnet3_rxq_stats	 vxrxq_stats;
	struct vmxnet3_rxq_shared	*vxrxq_rs;
	struct sysctl_oid_list		*vxrxq_sysctl;
	char				 vxrxq_name[16];
} __aligned(CACHE_LINE_SIZE);

#define VMXNET3_RXQ_LOCK(_rxq)		mtx_lock(&(_rxq)->vxrxq_mtx)
#define VMXNET3_RXQ_UNLOCK(_rxq)	mtx_unlock(&(_rxq)->vxrxq_mtx)
#define VMXNET3_RXQ_LOCK_ASSERT(_rxq)		\
    mtx_assert(&(_rxq)->vxrxq_mtx, MA_OWNED)
#define VMXNET3_RXQ_LOCK_ASSERT_NOTOWNED(_rxq)	\
    mtx_assert(&(_rxq)->vxrxq_mtx, MA_NOTOWNED)

struct vmxnet3_statistics {
	uint32_t		vmst_defragged;
	uint32_t		vmst_defrag_failed;
	uint32_t		vmst_mgetcl_failed;
	uint32_t		vmst_mbuf_load_failed;
};

struct vmxnet3_interrupt {
	struct resource		*vmxi_irq;
	int			 vmxi_rid;
	void			*vmxi_handler;
};

struct vmxnet3_softc {
	device_t			 vmx_dev;
	struct ifnet			*vmx_ifp;
	struct vmxnet3_driver_shared	*vmx_ds;
	uint32_t			 vmx_flags;
#define VMXNET3_FLAG_NO_MSIX	0x0001
#define VMXNET3_FLAG_RSS	0x0002

	struct vmxnet3_rxqueue		*vmx_rxq;
	struct vmxnet3_txqueue		*vmx_txq;

	struct resource			*vmx_res0;
	bus_space_tag_t			 vmx_iot0;
	bus_space_handle_t		 vmx_ioh0;
	struct resource			*vmx_res1;
	bus_space_tag_t			 vmx_iot1;
	bus_space_handle_t		 vmx_ioh1;
	struct resource			*vmx_msix_res;

	int				 vmx_link_active;
	int				 vmx_link_speed;
	int				 vmx_if_flags;
	int				 vmx_ntxqueues;
	int				 vmx_nrxqueues;
	int				 vmx_ntxdescs;
	int				 vmx_nrxdescs;
	int				 vmx_max_rxsegs;
	int				 vmx_rx_max_chain;

	struct vmxnet3_statistics	 vmx_stats;

	int				 vmx_intr_type;
	int				 vmx_intr_mask_mode;
	int				 vmx_event_intr_idx;
	int				 vmx_nintrs;
	struct vmxnet3_interrupt	 vmx_intrs[VMXNET3_MAX_INTRS];

	struct mtx			 vmx_mtx;
#ifndef VMXNET3_LEGACY_TX
	struct taskqueue		*vmx_tq;
#endif
	uint8_t				*vmx_mcast;
	void				*vmx_qs;
	struct vmxnet3_rss_shared	*vmx_rss;
	struct callout			 vmx_tick;
	struct vmxnet3_dma_alloc	 vmx_ds_dma;
	struct vmxnet3_dma_alloc	 vmx_qs_dma;
	struct vmxnet3_dma_alloc	 vmx_mcast_dma;
	struct vmxnet3_dma_alloc	 vmx_rss_dma;
	struct ifmedia			 vmx_media;
	int				 vmx_max_ntxqueues;
	int				 vmx_max_nrxqueues;
	eventhandler_tag		 vmx_vlan_attach;
	eventhandler_tag		 vmx_vlan_detach;
	uint32_t			 vmx_vlan_filter[4096/32];
	uint8_t				 vmx_lladdr[ETHER_ADDR_LEN];
};

#define VMXNET3_CORE_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vmx_mtx, _name, "VMXNET3 Lock", MTX_DEF)
#define VMXNET3_CORE_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_LOCK(_sc)		mtx_lock(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_UNLOCK(_sc)	mtx_unlock(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vmx_mtx, MA_OWNED)
#define VMXNET3_CORE_LOCK_ASSERT_NOTOWNED(_sc) \
    mtx_assert(&(_sc)->vmx_mtx, MA_NOTOWNED)

/*
 * Our driver version we report to the hypervisor; we just keep
 * this value constant.
 */
#define VMXNET3_DRIVER_VERSION 0x00010000

/*
 * Max descriptors per Tx packet. We must limit the size of the
 * any TSO packets based on the number of segments.
 */
#define VMXNET3_TX_MAXSEGS		32
#define VMXNET3_TX_MAXSIZE		(VMXNET3_TX_MAXSEGS * MCLBYTES)

/*
 * Maximum support Tx segments size. The length field in the
 * Tx descriptor is 14 bits.
 */
#define VMXNET3_TX_MAXSEGSIZE		(1 << 14)

/*
 * The maximum number of Rx segments we accept. When LRO is enabled,
 * this allows us to receive the maximum sized frame with one MCLBYTES
 * cluster followed by 16 MJUMPAGESIZE clusters.
 */
#define VMXNET3_MAX_RX_SEGS		17

/*
 * Predetermined size of the multicast MACs filter table. If the
 * number of multicast addresses exceeds this size, then the
 * ALL_MULTI mode is use instead.
 */
#define VMXNET3_MULTICAST_MAX		32

/*
 * Our Tx watchdog timeout.
 */
#define VMXNET3_WATCHDOG_TIMEOUT	5

/*
 * Number of slots in the Tx bufrings. This value matches most other
 * multiqueue drivers.
 */
#define VMXNET3_DEF_BUFRING_SIZE	4096

/*
 * IP protocols that we can perform Tx checksum offloading of.
 */
#define VMXNET3_CSUM_OFFLOAD		(CSUM_TCP | CSUM_UDP)
#define VMXNET3_CSUM_OFFLOAD_IPV6	(CSUM_TCP_IPV6 | CSUM_UDP_IPV6)

#define VMXNET3_CSUM_ALL_OFFLOAD	\
    (VMXNET3_CSUM_OFFLOAD | VMXNET3_CSUM_OFFLOAD_IPV6 | CSUM_TSO)

/*
 * Compat macros to keep this driver compiling on old releases.
 */

#if !defined(SYSCTL_ADD_UQUAD)
#define SYSCTL_ADD_UQUAD SYSCTL_ADD_QUAD
#endif

#if !defined(IFCAP_TXCSUM_IPV6)
#define IFCAP_TXCSUM_IPV6 0
#endif

#if !defined(IFCAP_RXCSUM_IPV6)
#define IFCAP_RXCSUM_IPV6 0
#endif

#if !defined(CSUM_TCP_IPV6)
#define CSUM_TCP_IPV6 0
#endif

#if !defined(CSUM_UDP_IPV6)
#define CSUM_UDP_IPV6	0
#endif

#endif /* _IF_VMXVAR_H */
