/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef	__QCOM_ESS_EDMA_VAR_H__
#define	__QCOM_ESS_EDMA_VAR_H__

#define	EDMA_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	EDMA_UNLOCK(_sc)	mtx_unlock(&(_sc)->sc_mtx)
#define	EDMA_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

#define	EDMA_RING_LOCK(_ring)		mtx_lock(&(_ring)->mtx)
#define	EDMA_RING_UNLOCK(_ring)		mtx_unlock(&(_ring)->mtx)
#define	EDMA_RING_LOCK_ASSERT(_ring)	mtx_assert(&(_ring)->mtx, MA_OWNED)

/*
 * register space access macros
 */
#define EDMA_REG_WRITE(sc, reg, val)	do {	\
		bus_write_4(sc->sc_mem_res, (reg), (val)); \
	} while (0)

#define EDMA_REG_READ(sc, reg)	 bus_read_4(sc->sc_mem_res, (reg))

#define EDMA_REG_SET_BITS(sc, reg, bits)	\
	EDMA_REG_WRITE(sc, reg, EDMA_REG_READ(sc, (reg)) | (bits))

#define EDMA_REG_CLEAR_BITS(sc, reg, bits)	\
	EDMA_REG_WRITE(sc, reg, EDMA_REG_READ(sc, (reg)) & ~(bits))

#define	EDMA_REG_BARRIER_WRITE(sc)	bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size, BUS_SPACE_BARRIER_WRITE)
#define EDMA_REG_BARRIER_READ(sc)	bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size, BUS_SPACE_BARRIER_READ)
#define EDMA_REG_BARRIER_RW(sc)		bus_barrier((sc)->sc_mem_res,	\
	    0, (sc)->sc_mem_res_size,					\
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)


/*
 * Fixed number of interrupts - 16 TX, 8 RX.
 *
 * The Linux driver supports 4 or 8 RX queues.
 */

#define	QCOM_ESS_EDMA_NUM_TX_IRQS	16
#define	QCOM_ESS_EDMA_NUM_RX_IRQS	8

#define	QCOM_ESS_EDMA_NUM_TX_RINGS	16
#define	QCOM_ESS_EDMA_NUM_RX_RINGS	8

#define	EDMA_TX_RING_SIZE		128
#define	EDMA_RX_RING_SIZE		128

#define	EDMA_TX_BUFRING_SIZE		512

/* Maximum number of GMAC instances */
#define	QCOM_ESS_EDMA_MAX_NUM_GMACS	5

/* Maximum number of ports to support mapping to GMACs */
#define	QCOM_ESS_EDMA_MAX_NUM_PORTS	8

#define	QCOM_ESS_EDMA_MAX_TXFRAGS	8

struct qcom_ess_edma_softc;

/*
 * An instance of an interrupt queue.
 */
struct qcom_ess_edma_intr {
	struct qcom_ess_edma_softc	*sc;
	struct resource			*irq_res;
	int				irq_rid;
	void				*irq_intr;

	struct {
		uint64_t	num_intr;
	} stats;
};

/*
 * A TX/RX descriptor ring.
 */
struct qcom_ess_edma_desc_ring {
	bus_dma_tag_t		hw_ring_dma_tag; /* tag for hw ring */
	bus_dma_tag_t		buffer_dma_tag; /* tag for mbufs */
	char			*label;

	struct mtx		mtx;

	bus_dmamap_t		hw_desc_map;
	bus_addr_t		hw_desc_paddr;
	void			*hw_desc;

	void			*sw_desc;
	int			hw_entry_size; /* hw desc entry size */
	int			sw_entry_size; /* sw desc entry size */
	int			ring_count; /* Number of entries */
	int			buffer_align;
	int			ring_align;

	uint16_t		next_to_fill;
	uint16_t		next_to_clean;
	uint16_t		pending_fill;

	struct {
		uint64_t	num_added;
		uint64_t	num_cleaned;
		uint64_t	num_dropped;
		uint64_t	num_enqueue_full;
		uint64_t	num_rx_no_gmac;
		uint64_t	num_rx_ok;
		uint64_t	num_tx_ok;
		uint64_t	num_tx_maxfrags;
		uint64_t	num_tx_mapfail;
		uint64_t	num_rx_csum_ok;
		uint64_t	num_rx_csum_fail;
		uint64_t	num_tx_complete;
		uint64_t	num_tx_xmit_defer;
		uint64_t	num_tx_xmit_task;
	} stats;
};

/*
 * Structs for transmit and receive software
 * ring entries.
 */
struct qcom_ess_edma_sw_desc_tx {
	struct mbuf		*m;
	bus_dmamap_t		m_dmamap;
	uint32_t		is_first:1;
	uint32_t		is_last:1;
};

struct qcom_ess_edma_sw_desc_rx {
	struct mbuf		*m;
	bus_dmamap_t		m_dmamap;
	bus_addr_t		m_physaddr;
};

#define	QCOM_ESS_EDMA_LABEL_SZ	16

/*
 * Per transmit ring TX state for TX queue / buf_ring stuff.
 */
struct qcom_ess_edma_tx_state {
	struct task completion_task;
	struct task xmit_task;
	struct buf_ring *br;
	struct taskqueue *completion_tq;
	struct qcom_ess_edma_softc *sc;
	char label[QCOM_ESS_EDMA_LABEL_SZ];
	int enqueue_is_running;
	int queue_id;
};

/*
 * Per receive ring RX state for taskqueue stuff.
 */
struct qcom_ess_edma_rx_state {
	struct task completion_task;
	struct taskqueue *completion_tq;
	struct qcom_ess_edma_softc *sc;
	char label[QCOM_ESS_EDMA_LABEL_SZ];
	int queue_id;
};

struct qcom_ess_edma_gmac {
	struct qcom_ess_edma_softc	*sc;
	int				id;
	bool				enabled;
	/* Native VLAN ID */
	int				vlan_id;
	/* Switch portmask for this instance */
	int				port_mask;
	/* MAC address for this ifnet (from device tree) */
	struct ether_addr		eaddr;
	/* ifnet interface! */
	if_t				ifp;
	/* media interface */
	struct ifmedia			ifm;
};

struct qcom_ess_edma_softc {
	device_t		sc_dev;
	struct mtx		sc_mtx;
	struct resource		*sc_mem_res;
	size_t			sc_mem_res_size;
	int			sc_mem_rid;
	uint32_t		sc_debug;
	bus_dma_tag_t		sc_dma_tag;

	struct qcom_ess_edma_intr sc_tx_irq[QCOM_ESS_EDMA_NUM_TX_IRQS];
	struct qcom_ess_edma_intr sc_rx_irq[QCOM_ESS_EDMA_NUM_RX_IRQS];

	struct qcom_ess_edma_desc_ring sc_tx_ring[QCOM_ESS_EDMA_NUM_TX_RINGS];
	struct qcom_ess_edma_desc_ring sc_rx_ring[QCOM_ESS_EDMA_NUM_RX_RINGS];
	struct qcom_ess_edma_tx_state sc_tx_state[QCOM_ESS_EDMA_NUM_TX_RINGS];
	struct qcom_ess_edma_rx_state sc_rx_state[QCOM_ESS_EDMA_NUM_RX_RINGS];
	struct qcom_ess_edma_gmac	sc_gmac[QCOM_ESS_EDMA_MAX_NUM_GMACS];

	int			sc_gmac_port_map[QCOM_ESS_EDMA_MAX_NUM_PORTS];

	struct {
		uint32_t num_gmac;
		uint32_t mdio_supported;
		uint32_t poll_required;
		uint32_t rss_type;

		uint32_t rx_buf_size;
		bool rx_buf_ether_align;

		uint32_t tx_intr_mask;
		uint32_t rx_intr_mask;

		/* number of tx/rx descriptor entries in each ring */
		uint32_t rx_ring_count;
		uint32_t tx_ring_count;

		/* how many queues for each CPU */
		uint32_t num_tx_queue_per_cpu;
	} sc_config;

	struct {
		uint32_t misc_intr_mask;
		uint32_t wol_intr_mask;
		uint32_t intr_sw_idx_w;
	} sc_state;
};

#endif	/* __QCOM_ESS_EDMA_VAR_H__ */
