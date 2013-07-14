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
 * The number of Rx/Tx queues this driver supports.
 */
#define VMXNET3_RX_QUEUES	1
#define VMXNET3_TX_QUEUES	1

/*
 * The number of Rx rings in each Rx queue.
 */
#define VMXNET3_RXRINGS_PERQ	2

/*
 * The maximum number of descriptors in each Rx/Tx ring.
 */
#define VMXNET3_MAX_TX_NDESC		128
#define VMXNET3_MAX_RX_NDESC		128
#define VMXNET3_MAX_TX_NCOMPDESC	VMXNET3_MAX_TX_NDESC
#define VMXNET3_MAX_RX_NCOMPDESC \
    (VMXNET3_MAX_RX_NDESC * VMXNET3_RXRINGS_PERQ)

struct vmxnet3_txring {
	struct mbuf		*vxtxr_m[VMXNET3_MAX_TX_NDESC];
	bus_dmamap_t		 vxtxr_dmap[VMXNET3_MAX_TX_NDESC];
	u_int			 vxtxr_head;
	u_int			 vxtxr_next;
	u_int			 vxtxr_ndesc;
	uint8_t			 vxtxr_gen;
	bus_dma_tag_t		 vxtxr_txtag;
	struct vmxnet3_txdesc	*vxtxr_txd;
	struct vmxnet3_dma_alloc vxtxr_dma;
};

#define VMXNET3_TXRING_AVAIL(_txr) \
    (((_txr)->vxtxr_head - (_txr)->vxtxr_head - 1) % VMXNET3_MAX_TX_NDESC)

struct vmxnet3_rxring {
	struct mbuf		*vxrxr_m[VMXNET3_MAX_RX_NDESC];
	bus_dmamap_t		 vxrxr_dmap[VMXNET3_MAX_RX_NDESC];
	struct vmxnet3_rxdesc	*vxrxr_rxd;
	u_int			 vxrxr_fill;
	u_int			 vxrxr_ndesc;
	uint8_t			 vxrxr_gen;
	uint8_t			 vxrxr_rid;
	bus_dma_tag_t		 vxrxr_rxtag;
	struct vmxnet3_dma_alloc vxrxr_dma;
	bus_dmamap_t		 vxrxr_spare_dmap;
};

struct vmxnet3_comp_ring {
	union {
		struct vmxnet3_txcompdesc *txcd;
		struct vmxnet3_rxcompdesc *rxcd;
	}			 vxcr_u;
	u_int			 vxcr_next;
	u_int			 vxcr_ndesc;
	uint8_t			 vxcr_gen;
	struct vmxnet3_dma_alloc vxcr_dma;
};

struct vmxnet3_txqueue {
	struct mtx			 vxtxq_mtx;
	struct vmxnet3_softc		*vxtxq_sc;
	int				 vxtxq_id;
	struct vmxnet3_txring		 vxtxq_cmd_ring;
	struct vmxnet3_comp_ring	 vxtxq_comp_ring;
	struct vmxnet3_txq_shared	*vxtxq_ts;
	char				 vxtxq_name[16];
};

#define VMXNET3_TXQ_LOCK(_txq)		mtx_lock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_TRYLOCK(_txq)	mtx_trylock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_UNLOCK(_txq)	mtx_unlock(&(_txq)->vxtxq_mtx)
#define VMXNET3_TXQ_LOCK_ASSERT(_txq)		\
    mtx_assert(&(_txq)->vxtxq_mtx, MA_OWNED)
#define VMXNET3_TXQ_LOCK_ASSERT_NOTOWNED(_txq)	\
    mtx_assert(&(_txq)->vxtxq_mtx, MA_NOTOWNED)

struct vmxnet3_rxqueue {
	struct mtx			 vxrxq_mtx;
	struct vmxnet3_softc		*vxrxq_sc;
	int				 vxrxq_id;
	struct vmxnet3_rxring		 vxrxq_cmd_ring[VMXNET3_RXRINGS_PERQ];
	struct vmxnet3_comp_ring	 vxrxq_comp_ring;
	struct vmxnet3_rxq_shared	*vxrxq_rs;
	char				 vxrxq_name[16];
};

#define VMXNET3_RXQ_LOCK(_rxq)		mtx_lock(&(_rxq)->vxrxq_mtx)
#define VMXNET3_RXQ_UNLOCK(_rxq)	mtx_unlock(&(_rxq)->vxrxq_mtx)
#define VMXNET3_RXQ_LOCK_ASSERT(_rxq)		\
    mtx_assert(&(_rxq)->vxrxq_mtx, MA_OWNED)
#define VMXNET3_RXQ_LOCK_ASSERT_NOTOWNED(_rxq)	\
    mtx_assert(&(_rxq)->vxrxq_mtx, MA_NOTOWNED)

struct vmxnet3_softc {
	device_t			 vmx_dev;
	struct ifnet			*vmx_ifp;
	uint32_t			 vmx_flags;

	struct vmxnet3_rxqueue		*vmx_rxq;
	struct vmxnet3_txqueue		*vmx_txq;

	int				 vmx_watchdog_timer;
	int				 vmx_if_flags;
	int				 vmx_link_active;
	int				 vmx_link_speed;

	int				 vmx_ntxqueues;
	int				 vmx_nrxqueues;
	int				 vmx_ntxdescs;
	int				 vmx_nrxdescs;
	int				 vmx_max_rxsegs;

	struct resource			*vmx_res0;
	bus_space_tag_t			 vmx_iot0;
	bus_space_handle_t		 vmx_ioh0;
	struct resource			*vmx_res1;
	bus_space_tag_t			 vmx_iot1;
	bus_space_handle_t		 vmx_ioh1;

	struct mtx			 vmx_mtx;
	struct callout			 vmx_tick;
	struct vmxnet3_driver_shared	*vmx_ds;
	struct vmxnet3_dma_alloc	 vmx_ds_dma;
	void				*vmx_qs;
	struct vmxnet3_dma_alloc	 vmx_qs_dma;
	struct resource			*vmx_irq;
	void				*vmx_intrhand;
	uint8_t				*vmx_mcast;
	struct vmxnet3_dma_alloc	 vmx_mcast_dma;
	struct ifmedia			 vmx_media;
	eventhandler_tag		 vmx_vlan_attach;
	eventhandler_tag		 vmx_vlan_detach;
	uint8_t				 vmx_vlan_filter[4096/32];
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
 * Convert the FreeBSD version in to something the hypervisor
 * understands. This is apparently what VMware's driver reports
 * so mimic it even though it probably is not required.
 */
#define VMXNET3_GUEST_OS_VERSION \
   (((__FreeBSD_version / 100000) << 14)	| \
    (((__FreeBSD_version / 1000) % 100)	<< 6 )	| \
    (((__FreeBSD_version / 100) % 10) << 30)	| \
    ((__FreeBSD_version % 100) << 22))

/*
 * Max descriptors per Tx packet. We must limit the size of the
 * any TSO packets based on the number of segments.
 */
#define VMXNET3_TX_MAXSEGS		16
#define VMXNET3_TSO_MAXSIZE		65550

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
 * IP protocols that we can perform Tx checksum offloading of.
 */
#define VMXNET3_CSUM_FEATURES		(CSUM_UDP | CSUM_TCP)

#endif /* _IF_VMXVAR_H */
