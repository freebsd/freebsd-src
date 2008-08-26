/*-
 * Copyright (C) 2006-2007 Semihalf
 * All rights reserved.
 *
 * Written by: Piotr Kruszynski <ppk@semihalf.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define TSEC_RX_NUM_DESC	256
#define TSEC_TX_NUM_DESC	256

#define	OCP_TSEC_RID_TXIRQ	0
#define	OCP_TSEC_RID_RXIRQ	1
#define	OCP_TSEC_RID_ERRIRQ	2

struct tsec_softc {
	/* XXX MII bus requires that struct ifnet is first!!! */
	struct ifnet	*tsec_ifp;

	struct mtx	transmit_lock;	/* transmitter lock */
	struct mtx	receive_lock;	/* receiver lock */

	device_t	dev;
	device_t	tsec_miibus;
	struct mii_data	*tsec_mii;	/* MII media control */
	int		tsec_link;

	bus_dma_tag_t	tsec_tx_dtag;	/* TX descriptors tag */
	bus_dmamap_t	tsec_tx_dmap;	/* TX descriptors map */
	struct tsec_desc *tsec_tx_vaddr;/* vadress of TX descriptors */
	uint32_t	tsec_tx_raddr;	/* real adress of TX descriptors */

	bus_dma_tag_t	tsec_rx_dtag;	/* RX descriptors tag */
	bus_dmamap_t	tsec_rx_dmap;	/* RX descriptors map */
	struct tsec_desc *tsec_rx_vaddr; /* vadress of RX descriptors */
	uint32_t	tsec_rx_raddr;	/* real adress of RX descriptors */

	bus_dma_tag_t	tsec_tx_mtag;	/* TX mbufs tag */
	bus_dma_tag_t	tsec_rx_mtag;	/* TX mbufs tag */

	struct rx_data_type {
		bus_dmamap_t	map;	/* mbuf map */
		struct mbuf	*mbuf;
		uint32_t	paddr;	/* DMA addres of buffer */
	} rx_data[TSEC_RX_NUM_DESC];

	uint32_t	tx_cur_desc_cnt;
	uint32_t	tx_dirty_desc_cnt;
	uint32_t	rx_cur_desc_cnt;

	struct resource	*sc_rres;	/* register resource */
	int		sc_rrid;	/* register rid */
	struct {
		bus_space_tag_t bst;
		bus_space_handle_t bsh;
	} sc_bas;

	struct resource *sc_transmit_ires;
	void		*sc_transmit_ihand;
	int		sc_transmit_irid;
	struct resource *sc_receive_ires;
	void		*sc_receive_ihand;
	int		sc_receive_irid;
	struct resource *sc_error_ires;
	void		*sc_error_ihand;
	int		sc_error_irid;

	int		tsec_if_flags;

	/* Watchdog and MII tick related */
	struct callout	tsec_callout;
	int		tsec_watchdog;

	/* TX maps */
	bus_dmamap_t	tx_map_data[TSEC_TX_NUM_DESC];

	/* unused TX maps data */
	uint32_t	tx_map_unused_get_cnt;
	uint32_t	tx_map_unused_put_cnt;
	bus_dmamap_t	*tx_map_unused_data[TSEC_TX_NUM_DESC];

	/* used TX maps data */
	uint32_t	tx_map_used_get_cnt;
	uint32_t	tx_map_used_put_cnt;
	bus_dmamap_t	*tx_map_used_data[TSEC_TX_NUM_DESC];

	/* mbufs in TX queue */
	uint32_t	tx_mbuf_used_get_cnt;
	uint32_t	tx_mbuf_used_put_cnt;
	struct mbuf	*tx_mbuf_used_data[TSEC_TX_NUM_DESC];
};

/* interface to get/put generic objects */
#define TSEC_CNT_INIT(cnt, wrap) ((cnt) = ((wrap) - 1))

#define TSEC_INC(count, wrap) (count = ((count) + 1) & ((wrap) - 1))

#define TSEC_GET_GENERIC(hand, tab, count, wrap) \
		((hand)->tab[TSEC_INC((hand)->count, wrap)])

#define TSEC_PUT_GENERIC(hand, tab, count, wrap, val)	\
		((hand)->tab[TSEC_INC((hand)->count, wrap)] = val)

#define TSEC_BACK_GENERIC(sc, count, wrap) do {				\
		if ((sc)->count > 0)					\
			(sc)->count--;					\
		else							\
			(sc)->count = (wrap) - 1;			\
} while (0)

/* TX maps interface */
#define TSEC_TX_MAP_CNT_INIT(sc) do {						\
		TSEC_CNT_INIT((sc)->tx_map_unused_get_cnt, TSEC_TX_NUM_DESC);	\
		TSEC_CNT_INIT((sc)->tx_map_unused_put_cnt, TSEC_TX_NUM_DESC);	\
		TSEC_CNT_INIT((sc)->tx_map_used_get_cnt, TSEC_TX_NUM_DESC);	\
		TSEC_CNT_INIT((sc)->tx_map_used_put_cnt, TSEC_TX_NUM_DESC);	\
} while (0)

/* interface to get/put unused TX maps */
#define TSEC_ALLOC_TX_MAP(sc)							\
		TSEC_GET_GENERIC(sc, tx_map_unused_data, tx_map_unused_get_cnt,	\
		TSEC_TX_NUM_DESC)

#define TSEC_FREE_TX_MAP(sc, val)						\
		TSEC_PUT_GENERIC(sc, tx_map_unused_data, tx_map_unused_put_cnt,	\
		TSEC_TX_NUM_DESC, val)

/* interface to get/put used TX maps */
#define TSEC_GET_TX_MAP(sc)							\
		TSEC_GET_GENERIC(sc, tx_map_used_data, tx_map_used_get_cnt,	\
		TSEC_TX_NUM_DESC)

#define TSEC_PUT_TX_MAP(sc, val)						\
		TSEC_PUT_GENERIC(sc, tx_map_used_data, tx_map_used_put_cnt,	\
		TSEC_TX_NUM_DESC, val)

/* interface to get/put TX mbufs in send queue */
#define TSEC_TX_MBUF_CNT_INIT(sc) do {						\
		TSEC_CNT_INIT((sc)->tx_mbuf_used_get_cnt, TSEC_TX_NUM_DESC);	\
		TSEC_CNT_INIT((sc)->tx_mbuf_used_put_cnt, TSEC_TX_NUM_DESC);	\
} while (0)

#define TSEC_GET_TX_MBUF(sc)							\
		TSEC_GET_GENERIC(sc, tx_mbuf_used_data, tx_mbuf_used_get_cnt,	\
		TSEC_TX_NUM_DESC)

#define TSEC_PUT_TX_MBUF(sc, val)						\
		TSEC_PUT_GENERIC(sc, tx_mbuf_used_data, tx_mbuf_used_put_cnt,	\
		TSEC_TX_NUM_DESC, val)

#define TSEC_EMPTYQ_TX_MBUF(sc) \
		((sc)->tx_mbuf_used_get_cnt == (sc)->tx_mbuf_used_put_cnt)

/* interface for manage tx tsec_desc */
#define TSEC_TX_DESC_CNT_INIT(sc) do {						\
		TSEC_CNT_INIT((sc)->tx_cur_desc_cnt, TSEC_TX_NUM_DESC);		\
		TSEC_CNT_INIT((sc)->tx_dirty_desc_cnt, TSEC_TX_NUM_DESC);	\
} while (0)

#define TSEC_GET_CUR_TX_DESC(sc)					\
		&TSEC_GET_GENERIC(sc, tsec_tx_vaddr, tx_cur_desc_cnt,	\
		TSEC_TX_NUM_DESC)

#define TSEC_GET_DIRTY_TX_DESC(sc)					\
		&TSEC_GET_GENERIC(sc, tsec_tx_vaddr, tx_dirty_desc_cnt,	\
		TSEC_TX_NUM_DESC)

#define TSEC_BACK_DIRTY_TX_DESC(sc) \
		TSEC_BACK_GENERIC(sc, tx_dirty_desc_cnt, TSEC_TX_NUM_DESC)

#define TSEC_CUR_DIFF_DIRTY_TX_DESC(sc) \
		((sc)->tx_cur_desc_cnt != (sc)->tx_dirty_desc_cnt)

#define TSEC_FREE_TX_DESC(sc)						\
		(((sc)->tx_cur_desc_cnt < (sc)->tx_dirty_desc_cnt) ?	\
		((sc)->tx_dirty_desc_cnt - (sc)->tx_cur_desc_cnt - 1)	\
		:							\
		(TSEC_TX_NUM_DESC - (sc)->tx_cur_desc_cnt		\
		+ (sc)->tx_dirty_desc_cnt - 1))

/* interface for manage rx tsec_desc */
#define TSEC_RX_DESC_CNT_INIT(sc) do {					\
		TSEC_CNT_INIT((sc)->rx_cur_desc_cnt, TSEC_RX_NUM_DESC);	\
} while (0)

#define TSEC_GET_CUR_RX_DESC(sc)					\
		&TSEC_GET_GENERIC(sc, tsec_rx_vaddr, rx_cur_desc_cnt,	\
		TSEC_RX_NUM_DESC)

#define TSEC_BACK_CUR_RX_DESC(sc) \
		TSEC_BACK_GENERIC(sc, rx_cur_desc_cnt, TSEC_RX_NUM_DESC)

#define TSEC_GET_CUR_RX_DESC_CNT(sc) \
		((sc)->rx_cur_desc_cnt)

/* init all counters (for init only!) */
#define TSEC_TX_RX_COUNTERS_INIT(sc) do {	\
		TSEC_TX_MAP_CNT_INIT(sc);	\
		TSEC_TX_MBUF_CNT_INIT(sc);	\
		TSEC_TX_DESC_CNT_INIT(sc);	\
		TSEC_RX_DESC_CNT_INIT(sc);	\
} while (0)

/* read/write bus functions */
#define TSEC_READ(sc, reg)		\
		bus_space_read_4((sc)->sc_bas.bst, (sc)->sc_bas.bsh, (reg))
#define TSEC_WRITE(sc, reg, val)	\
		bus_space_write_4((sc)->sc_bas.bst, (sc)->sc_bas.bsh, (reg), (val))

/* Lock for transmitter */
#define TSEC_TRANSMIT_LOCK(sc) do {					\
		mtx_assert(&(sc)->receive_lock, MA_NOTOWNED);		\
		mtx_lock(&(sc)->transmit_lock);				\
} while (0)

#define TSEC_TRANSMIT_UNLOCK(sc)	mtx_unlock(&(sc)->transmit_lock)
#define TSEC_TRANSMIT_LOCK_ASSERT(sc)	mtx_assert(&(sc)->transmit_lock, MA_OWNED)

/* Lock for receiver */
#define TSEC_RECEIVE_LOCK(sc) do {				\
		mtx_assert(&(sc)->transmit_lock, MA_NOTOWNED);	\
		mtx_lock(&(sc)->receive_lock);			\
} while (0)

#define TSEC_RECEIVE_UNLOCK(sc)		mtx_unlock(&(sc)->receive_lock)
#define TSEC_RECEIVE_LOCK_ASSERT(sc)	mtx_assert(&(sc)->receive_lock, MA_OWNED)

/* Global tsec lock (with all locks) */
#define TSEC_GLOBAL_LOCK(sc) do {					\
		if ((mtx_owned(&(sc)->transmit_lock) ? 1 : 0) !=	\
			(mtx_owned(&(sc)->receive_lock) ? 1 : 0)) {	\
			panic("tsec deadlock possibility detection!");	\
		}							\
		mtx_lock(&(sc)->transmit_lock);				\
		mtx_lock(&(sc)->receive_lock);				\
} while (0)

#define TSEC_GLOBAL_UNLOCK(sc) do {		\
		TSEC_RECEIVE_UNLOCK(sc);	\
		TSEC_TRANSMIT_UNLOCK(sc);	\
} while (0)

#define TSEC_GLOBAL_LOCK_ASSERT(sc) do {	\
		TSEC_TRANSMIT_LOCK_ASSERT(sc);	\
		TSEC_RECEIVE_LOCK_ASSERT(sc);	\
} while (0)

/* From global to {transmit,receive} */
#define TSEC_GLOBAL_TO_TRANSMIT_LOCK(sc) do {	\
		mtx_unlock(&(sc)->receive_lock);\
} while (0)

#define TSEC_GLOBAL_TO_RECEIVE_LOCK(sc) do {	\
		mtx_unlock(&(sc)->transmit_lock);\
} while (0)

struct tsec_desc {
	volatile uint16_t	flags; /* descriptor flags */
	volatile uint16_t	length; /* buffer length */
	volatile uint32_t	bufptr; /* buffer pointer */
};

#define TSEC_READ_RETRY	10000
#define TSEC_READ_DELAY	100
