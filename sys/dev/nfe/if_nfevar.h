/*	$OpenBSD: if_nfevar.h,v 1.11 2006/02/19 13:57:02 damien Exp $	*/

/*-
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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

#define	NFE_IFQ_MAXLEN	64

struct nfe_tx_data {
	bus_dmamap_t	tx_data_map;
	bus_dmamap_t	active;
	int		nsegs;
	struct mbuf	*m;
};

struct nfe_tx_ring {
	bus_dmamap_t		tx_desc_map;
	bus_dma_segment_t	tx_desc_segs;
	bus_addr_t		physaddr;
	struct nfe_desc32	*desc32;
	struct nfe_desc64	*desc64;
	struct nfe_tx_data	data[NFE_TX_RING_COUNT];
	int			queued;
	int			cur;
	int			next;
	bus_addr_t		tx_desc_addr;
	bus_addr_t		tx_data_addr;
	bus_dma_tag_t		tx_desc_tag;
	bus_dma_tag_t		tx_data_tag;
};

struct nfe_jbuf {
	caddr_t			buf;
	bus_addr_t		physaddr;
	SLIST_ENTRY(nfe_jbuf)	jnext;
};

struct nfe_rx_data {
	bus_dmamap_t	rx_data_map;
	bus_dma_tag_t	rx_data_tag;
	bus_addr_t	rx_data_addr;
	bus_dma_segment_t rx_data_segs;
	struct mbuf	*m;
};

struct nfe_rx_ring {
	bus_dmamap_t		rx_desc_map;
	bus_dma_segment_t	rx_desc_segs;
        bus_dma_tag_t		rx_desc_tag;
	bus_addr_t		rx_desc_addr;
#ifndef JMBUF
	bus_dmamap_t		rx_jumbo_map;
	bus_dma_segment_t	rx_jumbo_segs;
        bus_dma_tag_t		rx_jumbo_tag;
	bus_addr_t		rx_jumbo_addr;
	caddr_t			jpool;
	struct nfe_jbuf		jbuf[NFE_JPOOL_COUNT];
	SLIST_HEAD(, nfe_jbuf)	jfreelist;
#endif
	bus_addr_t		physaddr;
	struct nfe_desc32	*desc32;
	struct nfe_desc64	*desc64;
	struct nfe_rx_data	data[NFE_RX_RING_COUNT];
	int			bufsz;
	int			cur;
	int			next;
};

struct nfe_softc {
	struct ifnet		*nfe_ifp;
	device_t		nfe_dev;
	device_t		nfe_miibus;
	struct mtx		nfe_mtx;
	bus_space_handle_t	nfe_memh;
	bus_space_tag_t		nfe_memt;
	struct resource		*nfe_res;
	struct resource		*nfe_irq;
	void			*nfe_intrhand;
	struct mii_data		nfe_mii;
	u_int8_t		nfe_unit;
	struct callout		nfe_stat_ch;

	struct arpcom		nfe_arpcom;
	bus_dma_tag_t		nfe_parent_tag;
  /*	struct timeout		nfe_tick_ch; */
	void			*nfe_powerhook;

	int			nfe_if_flags;
	u_int			nfe_flags;
#define	NFE_JUMBO_SUP	0x01
#define	NFE_40BIT_ADDR	0x02
#define	NFE_HW_CSUM	0x04
#define	NFE_HW_VLAN	0x08
	u_int32_t		rxtxctl;
	u_int32_t		nfe_mtu;
	u_int8_t		mii_phyaddr;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct task		nfe_txtask;
	int			nfe_link;

	struct nfe_tx_ring	txq;
	struct nfe_rx_ring	rxq;

#ifdef DEVICE_POLLING
	int			rxcycles;
#endif
};

struct nfe_type {
	u_int16_t	vid_id;
	u_int16_t	dev_id;
	char		*name;
};
