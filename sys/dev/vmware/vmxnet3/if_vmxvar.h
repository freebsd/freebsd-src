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

#define VMXNET3_RX_QUEUES	1
#define VMXNET3_TX_QUEUES	1

#define VMXNET3_TX_NDESC	64	/* Tx ring size */
#define VMXNET3_RX_NDESC	64	/* Rx ring size */
#define VMXNET3_TX_NCOMPDESC	VMXNET3_TX_NDESC
#define VMXNET3_RX_NCOMPDESC	(VMXNET3_RX_NDESC * 2)	/* Both rings */

#define VMXNET3_TX_MAXSEGS	8	/* Tx descriptors per packet */

#define VMXNET3_DRIVER_VERSION 0x00010000

struct vmxnet3_dma_alloc {
	bus_addr_t		dma_paddr;
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;	/* Unused */
	bus_size_t		dma_size;
	int			dma_nseg;	/* Unused */
};

struct vmxnet3_txring {
	struct mbuf		*vxtxr_m[VMXNET3_TX_NDESC];
	bus_dmamap_t		 vxtxr_dmap[VMXNET3_TX_NDESC];
	u_int			 vxtxr_head;
	u_int			 vxtxr_next;
	uint8_t			 vxtxr_gen;
	bus_dma_tag_t		 vxtxr_txtag;
	struct vmxnet3_txdesc	*vxtxr_txd;
	struct vmxnet3_dma_alloc vxtxr_dma;
};

struct vmxnet3_rxring {
	struct mbuf		*vxrxr_m[VMXNET3_RX_NDESC];
	bus_dmamap_t		 vxrxr_dmap[VMXNET3_RX_NDESC];
	struct vmxnet3_rxdesc	*vxrxr_rxd;
	u_int			 vxrxr_fill;
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
	uint8_t			 vxcr_gen;
	struct vmxnet3_dma_alloc vxcr_dma;
};

struct vmxnet3_txqueue {
	struct vmxnet3_txring		 vxtxq_cmd_ring;
	struct vmxnet3_comp_ring	 vxtxq_comp_ring;
	struct vmxnet3_txq_shared	*vxtxq_ts;
};

struct vmxnet3_rxqueue {
	struct vmxnet3_rxring		 vxrxq_cmd_ring[2];
	struct vmxnet3_comp_ring	 vxrxq_comp_ring;
	struct vmxnet3_rxq_shared	*vxrxq_rs;
};

struct vmxnet3_softc {
	device_t			 vmx_dev;
	struct ifnet			*vmx_ifp;
	uint32_t			 vmx_flags;

	struct mtx			 vmx_tx_mtx;
	struct vmxnet3_txqueue		 vmx_txq[VMXNET3_TX_QUEUES];
	struct mtx			 vmx_rx_mtx;
	struct vmxnet3_rxqueue		 vmx_rxq[VMXNET3_RX_QUEUES];

	int				 vmx_watchdog_timer;
	int				 vmx_if_flags;
	int				 vmx_link_active;
	int				 vmx_link_speed;

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
	uint8_t				 vmx_lladdr[ETHER_ADDR_LEN];
};

#define VMXNET3_CORE_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vmx_mtx, _name, "VMXNET3 Lock", MTX_DEF)
#define VMXNET3_CORE_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_LOCK(_sc)		mtx_lock(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_UNLOCK(_sc)		mtx_unlock(&(_sc)->vmx_mtx)
#define VMXNET3_CORE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vmx_mtx, MA_OWNED)
#define VMXNET3_CORE_LOCK_ASSERT_NOTOWNED(_sc) \
    mtx_assert(&(_sc)->vmx_mtx, MA_NOTOWNED)

#define VMXNET3_RX_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vmx_rx_mtx, _name, "VMXNET3 Rx Lock", MTX_DEF)
#define VMXNET3_RX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vmx_rx_mtx)
#define VMXNET3_RX_LOCK(_sc)		mtx_lock(&(_sc)->vmx_rx_mtx)
#define VMXNET3_RX_UNLOCK(_sc)		mtx_unlock(&(_sc)->vmx_rx_mtx)
#define VMXNET3_RX_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vmx_rx_mtx, MA_OWNED)

#define VMXNET3_TX_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vmx_tx_mtx, _name, "VMXNET3 Tx Lock", MTX_DEF)
#define VMXNET3_TX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vmx_tx_mtx)
#define VMXNET3_TX_LOCK(_sc)		mtx_lock(&(_sc)->vmx_tx_mtx)
#define VMXNET3_TX_UNLOCK(_sc)		mtx_unlock(&(_sc)->vmx_tx_mtx)
#define VMXNET3_TX_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vmx_tx_mtx, MA_OWNED)

#define VMXNET3_STAT
#ifdef VMXNET3_STAT
struct {
	uint32_t ntxdesc;
	uint32_t nrxdesc;
	uint32_t txhead;
	uint32_t txdone;
	uint32_t maxtxlen;
	uint32_t rxdone;
	uint32_t rxfill;
	uint32_t intr;
} vmxstat = {
	VMXNET3_TX_NDESC, VMXNET3_RX_NDESC
};
#endif

/*
 * Sort these later.
 */

/*
 * Predetermined size of the multicast MACs filter table. If
 * there are more addresses than supported, the ALL_MULTI mode
 * is used.
 */
#define VMXNET3_MULTICAST_MAX	32

#define VMXNET3_WATCHDOG_TIMEOUT	5

#define VMXNET3_CSUM_FEATURES	(CSUM_UDP | CSUM_TCP)

#define VMXNET3_TXRING_AVAIL(_txr) \
    (((_txr)->vxtxr_head - (_txr)->vxtxr_head - 1) % VMXNET3_TX_NDESC)

#define VMXNET3_TSO_MAXSIZE	65550

#endif /* _IF_VMXVAR_H */
