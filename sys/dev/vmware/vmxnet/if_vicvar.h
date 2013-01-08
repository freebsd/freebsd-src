/*-
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2012 Bryan Venteicher <bryanv@freebsd.org>
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
 * $OpenBSD: if_vic.c,v 1.77 2011/11/29 11:53:25 jsing Exp $
 *
 * $FreeBSD$
 */

#ifndef _IF_VICVAR_H
#define _IF_VICVAR_H

struct vic_rxbuf {
	bus_dmamap_t		 rxb_dmamap;
	struct mbuf		*rxb_m;
};

struct vic_txbuf {
	bus_dmamap_t		 txb_dmamap;
	struct mbuf		*txb_m;
};

struct vic_rxqueue {
	struct vic_rxbuf	*bufs;
	struct vic_rxdesc	*slots;
	u_int		 	 nbufs;
	u_int			 pktlen;
	bus_dma_tag_t		 tag;
	bus_dmamap_t		 spare_dmamap;
};

struct vic_softc {
	device_t		 vic_dev;
	struct ifnet		*vic_ifp;
	struct mtx		 vic_mtx;
	int			 vic_flags;
#define VIC_FLAGS_MORPHED_PCNET	 0x0001
#define VIC_FLAGS_ENHANCED	 0x0002
#define VIC_FLAGS_WDTIMEOUT	 0x0004
#define VIC_FLAGS_JUMBO		 0x0008
#define VIC_FLAGS_TSO		 0x0010
#define VIC_FLAGS_LRO		 0x0020

	struct vic_data		*vic_data;

	struct mtx		 vic_rx_mtx;
	struct vic_rxqueue	 vic_rxq[VIC_NRXRINGS];

	struct mtx		 vic_tx_mtx;
	struct vic_txbuf	*vic_txbuf;
	struct vic_txdesc	*vic_txq;
	bus_dma_tag_t		 vic_tx_tag;
	uint32_t		 vic_tx_nbufs;
	int			 vic_tx_pending;
	int			 vic_watchdog_timer;

	struct resource		*vic_res;
	bus_space_tag_t		 vic_iot;
	bus_space_handle_t	 vic_ioh;
	bus_space_handle_t	 vic_orig_ioh;

	bus_dma_tag_t		 vic_dma_tag;
	bus_dmamap_t		 vic_dma_map;
	size_t			 vic_dma_size;
	caddr_t			 vic_dma_kva;
	bus_addr_t		 vic_dma_paddr;

	struct callout		 vic_tick;
	struct resource		*vic_irq;
	void			*vic_intrhand;
	uint32_t		 vic_cap;
	uint32_t		 vic_feature;
	int			 vic_if_flags;
	int			 vic_sg_max;
	struct ifmedia		 vic_media;
	uint8_t			 vic_lladdr[ETHER_ADDR_LEN];
};

/*
 * These features are only available on vmxnet2 devices.
 */
#define VIC_VMXNET2_FLAGS	(VIC_FLAGS_JUMBO | VIC_FLAGS_TSO)

#define VIC_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vic_mtx, _name, "VIC Lock", MTX_DEF)
#define VIC_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vic_mtx)
#define VIC_LOCK(_sc)		mtx_lock(&(_sc)->vic_mtx)
#define VIC_UNLOCK(_sc)		mtx_unlock(&(_sc)->vic_mtx)
#define VIC_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->vic_mtx, MA_OWNED)

#define VIC_RX_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vic_rx_mtx, _name, "VIC Rx Lock", MTX_DEF)
#define VIC_RX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vic_rx_mtx)
#define VIC_RX_LOCK(_sc)		mtx_lock(&(_sc)->vic_rx_mtx)
#define VIC_RX_UNLOCK(_sc)		mtx_unlock(&(_sc)->vic_rx_mtx)
#define VIC_RX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->vic_rx_mtx, MA_OWNED)

#define VIC_TX_LOCK_INIT(_sc, _name) \
    mtx_init(&(_sc)->vic_tx_mtx, _name, "VIC Tx Lock", MTX_DEF)
#define VIC_TX_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->vic_tx_mtx)
#define VIC_TX_LOCK(_sc)		mtx_lock(&(_sc)->vic_tx_mtx)
#define VIC_TX_UNLOCK(_sc)		mtx_unlock(&(_sc)->vic_tx_mtx)
#define VIC_TX_LOCK_ASSERT(_sc)		mtx_assert(&(_sc)->vic_tx_mtx, MA_OWNED)

#define VIC_TX_MAXSEGSIZE	PAGE_SIZE
#define VIC_TSO_MAXSIZE		(65536 + sizeof(struct ether_vlan_header))
#define VIC_TSO_MAX_CHAINED	4
#define VIC_TSO_MAXSEGS		(VIC_TSO_MAX_CHAINED * VIC_SG_MAX)

#define VIC_WATCHDOG_TIMEOUT	5

#define VIC_CSUM_FEATURES	(CSUM_UDP | CSUM_TCP)

#endif /* _IF_VICVAR_H */
