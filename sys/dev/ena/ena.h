/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
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
 *
 * $FreeBSD$
 *
 */

#ifndef ENA_H
#define ENA_H

#include "opt_rss.h"

#include "ena-com/ena_com.h"
#include "ena-com/ena_eth_com.h"

#define DRV_MODULE_VER_MAJOR	2
#define DRV_MODULE_VER_MINOR	5
#define DRV_MODULE_VER_SUBMINOR 0

#define DRV_MODULE_NAME		"ena"

#ifndef DRV_MODULE_VERSION
#define DRV_MODULE_VERSION				\
	__XSTRING(DRV_MODULE_VER_MAJOR) "."		\
	__XSTRING(DRV_MODULE_VER_MINOR) "."		\
	__XSTRING(DRV_MODULE_VER_SUBMINOR)
#endif
#define DEVICE_NAME	"Elastic Network Adapter (ENA)"
#define DEVICE_DESC	"ENA adapter"

/* Calculate DMA mask - width for ena cannot exceed 48, so it is safe */
#define ENA_DMA_BIT_MASK(x)		((1ULL << (x)) - 1ULL)

/* 1 for AENQ + ADMIN */
#define	ENA_ADMIN_MSIX_VEC		1
#define	ENA_MAX_MSIX_VEC(io_queues)	(ENA_ADMIN_MSIX_VEC + (io_queues))

#define	ENA_REG_BAR			0
#define	ENA_MEM_BAR			2

#define	ENA_BUS_DMA_SEGS		32

#define	ENA_DEFAULT_BUF_RING_SIZE	4096

#define	ENA_DEFAULT_RING_SIZE		1024
#define	ENA_MIN_RING_SIZE		256

/*
 * Refill Rx queue when number of required descriptors is above
 * QUEUE_SIZE / ENA_RX_REFILL_THRESH_DIVIDER or ENA_RX_REFILL_THRESH_PACKET
 */
#define	ENA_RX_REFILL_THRESH_DIVIDER	8
#define	ENA_RX_REFILL_THRESH_PACKET	256

#define	ENA_IRQNAME_SIZE		40

#define	ENA_PKT_MAX_BUFS 		19

#define	ENA_RX_RSS_TABLE_LOG_SIZE	7
#define	ENA_RX_RSS_TABLE_SIZE		(1 << ENA_RX_RSS_TABLE_LOG_SIZE)

#define	ENA_HASH_KEY_SIZE		40

#define	ENA_MAX_FRAME_LEN		10000
#define	ENA_MIN_FRAME_LEN 		60

#define ENA_TX_RESUME_THRESH		(ENA_PKT_MAX_BUFS + 2)

#define DB_THRESHOLD	64

#define TX_COMMIT	32
 /*
 * TX budget for cleaning. It should be half of the RX budget to reduce amount
 *  of TCP retransmissions.
 */
#define TX_BUDGET	128
/* RX cleanup budget. -1 stands for infinity. */
#define RX_BUDGET	256
/*
 * How many times we can repeat cleanup in the io irq handling routine if the
 * RX or TX budget was depleted.
 */
#define CLEAN_BUDGET	8

#define RX_IRQ_INTERVAL 20
#define TX_IRQ_INTERVAL 50

#define	ENA_MIN_MTU		128

#define	ENA_TSO_MAXSIZE		65536

#define	ENA_MMIO_DISABLE_REG_READ	BIT(0)

#define	ENA_TX_RING_IDX_NEXT(idx, ring_size) (((idx) + 1) & ((ring_size) - 1))

#define	ENA_RX_RING_IDX_NEXT(idx, ring_size) (((idx) + 1) & ((ring_size) - 1))

#define	ENA_IO_TXQ_IDX(q)		(2 * (q))
#define	ENA_IO_RXQ_IDX(q)		(2 * (q) + 1)
#define	ENA_IO_TXQ_IDX_TO_COMBINED_IDX(q)	((q) / 2)
#define	ENA_IO_RXQ_IDX_TO_COMBINED_IDX(q)	(((q) - 1) / 2)

#define	ENA_MGMNT_IRQ_IDX		0
#define	ENA_IO_IRQ_FIRST_IDX		1
#define	ENA_IO_IRQ_IDX(q)		(ENA_IO_IRQ_FIRST_IDX + (q))

#define	ENA_MAX_NO_INTERRUPT_ITERATIONS	3

/*
 * ENA device should send keep alive msg every 1 sec.
 * We wait for 6 sec just to be on the safe side.
 */
#define DEFAULT_KEEP_ALIVE_TO		(SBT_1S * 6)

/* Time in jiffies before concluding the transmitter is hung. */
#define DEFAULT_TX_CMP_TO		(SBT_1S * 5)

/* Number of queues to check for missing queues per timer tick */
#define DEFAULT_TX_MONITORED_QUEUES	(4)

/* Max number of timeouted packets before device reset */
#define DEFAULT_TX_CMP_THRESHOLD	(128)

/*
 * Supported PCI vendor and devices IDs
 */
#define	PCI_VENDOR_ID_AMAZON	0x1d0f

#define	PCI_DEV_ID_ENA_PF		0x0ec2
#define	PCI_DEV_ID_ENA_PF_RSERV0	0x1ec2
#define	PCI_DEV_ID_ENA_VF		0xec20
#define	PCI_DEV_ID_ENA_VF_RSERV0	0xec21

/*
 * Flags indicating current ENA driver state
 */
enum ena_flags_t {
	ENA_FLAG_DEVICE_RUNNING,
	ENA_FLAG_DEV_UP,
	ENA_FLAG_LINK_UP,
	ENA_FLAG_MSIX_ENABLED,
	ENA_FLAG_TRIGGER_RESET,
	ENA_FLAG_ONGOING_RESET,
	ENA_FLAG_DEV_UP_BEFORE_RESET,
	ENA_FLAG_RSS_ACTIVE,
	ENA_FLAGS_NUMBER = ENA_FLAG_RSS_ACTIVE
};

BITSET_DEFINE(_ena_state, ENA_FLAGS_NUMBER);
typedef struct _ena_state ena_state_t;

#define ENA_FLAG_ZERO(adapter)		\
	BIT_ZERO(ENA_FLAGS_NUMBER, &(adapter)->flags)
#define ENA_FLAG_ISSET(bit, adapter)	\
	BIT_ISSET(ENA_FLAGS_NUMBER, (bit), &(adapter)->flags)
#define ENA_FLAG_SET_ATOMIC(bit, adapter)	\
	BIT_SET_ATOMIC(ENA_FLAGS_NUMBER, (bit), &(adapter)->flags)
#define ENA_FLAG_CLEAR_ATOMIC(bit, adapter)	\
	BIT_CLR_ATOMIC(ENA_FLAGS_NUMBER, (bit), &(adapter)->flags)

struct msix_entry {
	int entry;
	int vector;
};

typedef struct _ena_vendor_info_t {
	uint16_t vendor_id;
	uint16_t device_id;
	unsigned int index;
} ena_vendor_info_t;

struct ena_irq {
	/* Interrupt resources */
	struct resource *res;
	driver_filter_t *handler;
	void *data;
	void *cookie;
	unsigned int vector;
	bool requested;
#ifdef RSS
	int cpu;
#endif
	char name[ENA_IRQNAME_SIZE];
};

struct ena_que {
	struct ena_adapter *adapter;
	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;

	struct task cleanup_task;
	struct taskqueue *cleanup_tq;

	uint32_t id;
#ifdef RSS
	int cpu;
	cpuset_t cpu_mask;
#endif
	int domain;
	struct sysctl_oid *oid;
};

struct ena_calc_queue_size_ctx {
	struct ena_com_dev_get_features_ctx *get_feat_ctx;
	struct ena_com_dev *ena_dev;
	device_t pdev;
	uint32_t tx_queue_size;
	uint32_t rx_queue_size;
	uint32_t max_tx_queue_size;
	uint32_t max_rx_queue_size;
	uint16_t max_tx_sgl_size;
	uint16_t max_rx_sgl_size;
};

#ifdef DEV_NETMAP
struct ena_netmap_tx_info {
	uint32_t socket_buf_idx[ENA_PKT_MAX_BUFS];
	bus_dmamap_t map_seg[ENA_PKT_MAX_BUFS];
	unsigned int sockets_used;
};
#endif

struct ena_tx_buffer {
	struct mbuf *mbuf;
	/* # of ena desc for this specific mbuf
	 * (includes data desc and metadata desc) */
	unsigned int tx_descs;
	/* # of buffers used by this mbuf */
	unsigned int num_of_bufs;

	bus_dmamap_t dmamap;

	/* Used to detect missing tx packets */
	struct bintime timestamp;
	bool print_once;

#ifdef DEV_NETMAP
	struct ena_netmap_tx_info nm_info;
#endif /* DEV_NETMAP */

	struct ena_com_buf bufs[ENA_PKT_MAX_BUFS];
} __aligned(CACHE_LINE_SIZE);

struct ena_rx_buffer {
	struct mbuf *mbuf;
	bus_dmamap_t map;
	struct ena_com_buf ena_buf;
#ifdef DEV_NETMAP
	uint32_t netmap_buf_idx;
#endif /* DEV_NETMAP */
} __aligned(CACHE_LINE_SIZE);

struct ena_stats_tx {
	counter_u64_t cnt;
	counter_u64_t bytes;
	counter_u64_t prepare_ctx_err;
	counter_u64_t dma_mapping_err;
	counter_u64_t doorbells;
	counter_u64_t missing_tx_comp;
	counter_u64_t bad_req_id;
	counter_u64_t collapse;
	counter_u64_t collapse_err;
	counter_u64_t queue_wakeup;
	counter_u64_t queue_stop;
	counter_u64_t llq_buffer_copy;
	counter_u64_t unmask_interrupt_num;
};

struct ena_stats_rx {
	counter_u64_t cnt;
	counter_u64_t bytes;
	counter_u64_t refil_partial;
	counter_u64_t csum_bad;
	counter_u64_t mjum_alloc_fail;
	counter_u64_t mbuf_alloc_fail;
	counter_u64_t dma_mapping_err;
	counter_u64_t bad_desc_num;
	counter_u64_t bad_req_id;
	counter_u64_t empty_rx_ring;
	counter_u64_t csum_good;
};

struct ena_ring {
	/* Holds the empty requests for TX/RX out of order completions */
	union {
		uint16_t *free_tx_ids;
		uint16_t *free_rx_ids;
	};
	struct ena_com_dev *ena_dev;
	struct ena_adapter *adapter;
	struct ena_com_io_cq *ena_com_io_cq;
	struct ena_com_io_sq *ena_com_io_sq;

	uint16_t qid;

	/* Determines if device will use LLQ or normal mode for TX */
	enum ena_admin_placement_policy_type tx_mem_queue_type;
	union {
		/* The maximum length the driver can push to the device (For LLQ) */
		uint8_t tx_max_header_size;
		/* The maximum (and default) mbuf size for the Rx descriptor. */
		uint16_t rx_mbuf_sz;

	};

	bool first_interrupt;
	uint16_t no_interrupt_event_cnt;

	struct ena_com_rx_buf_info ena_bufs[ENA_PKT_MAX_BUFS];

	struct ena_que *que;
	struct lro_ctrl lro;

	uint16_t next_to_use;
	uint16_t next_to_clean;

	union {
		struct ena_tx_buffer *tx_buffer_info; /* contex of tx packet */
		struct ena_rx_buffer *rx_buffer_info; /* contex of rx packet */
	};
	int ring_size; /* number of tx/rx_buffer_info's entries */

	struct buf_ring *br; /* only for TX */
	uint32_t buf_ring_size;

	struct mtx ring_mtx;
	char mtx_name[16];

	struct {
		struct task enqueue_task;
		struct taskqueue *enqueue_tq;
	};

	union {
		struct ena_stats_tx tx_stats;
		struct ena_stats_rx rx_stats;
	};

	union {
		int empty_rx_queue;
		/* For Tx ring to indicate if it's running or not */
		bool running;
	};

	/* How many packets are sent in one Tx loop, used for doorbells */
	uint32_t acum_pkts;

	/* Used for LLQ */
	uint8_t *push_buf_intermediate_buf;

#ifdef DEV_NETMAP
	bool initialized;
#endif /* DEV_NETMAP */
} __aligned(CACHE_LINE_SIZE);

struct ena_stats_dev {
	counter_u64_t wd_expired;
	counter_u64_t interface_up;
	counter_u64_t interface_down;
	counter_u64_t admin_q_pause;
};

struct ena_hw_stats {
	counter_u64_t rx_packets;
	counter_u64_t tx_packets;

	counter_u64_t rx_bytes;
	counter_u64_t tx_bytes;

	counter_u64_t rx_drops;
	counter_u64_t tx_drops;
};

/* Board specific private data structure */
struct ena_adapter {
	struct ena_com_dev *ena_dev;

	/* OS defined structs */
	if_t ifp;
	device_t pdev;
	struct ifmedia	media;

	/* OS resources */
	struct resource *memory;
	struct resource *registers;
	struct resource *msix;
	int msix_rid;

	/* MSI-X */
	struct msix_entry *msix_entries;
	int msix_vecs;

	/* DMA tags used throughout the driver adapter for Tx and Rx */
	bus_dma_tag_t tx_buf_tag;
	bus_dma_tag_t rx_buf_tag;
	int dma_width;

	uint32_t max_mtu;

	uint32_t num_io_queues;
	uint32_t max_num_io_queues;

	uint32_t requested_tx_ring_size;
	uint32_t requested_rx_ring_size;

	uint32_t max_tx_ring_size;
	uint32_t max_rx_ring_size;

	uint16_t max_tx_sgl_size;
	uint16_t max_rx_sgl_size;

	uint32_t tx_offload_cap;

	uint32_t buf_ring_size;

	/* RSS*/
	int first_bind;
	struct ena_indir *rss_indir;

	uint8_t mac_addr[ETHER_ADDR_LEN];
	/* mdio and phy*/

	ena_state_t flags;

	/* Queue will represent one TX and one RX ring */
	struct ena_que que[ENA_MAX_NUM_IO_QUEUES]
	    __aligned(CACHE_LINE_SIZE);

	/* TX */
	struct ena_ring tx_ring[ENA_MAX_NUM_IO_QUEUES]
	    __aligned(CACHE_LINE_SIZE);

	/* RX */
	struct ena_ring rx_ring[ENA_MAX_NUM_IO_QUEUES]
	    __aligned(CACHE_LINE_SIZE);

	struct ena_irq irq_tbl[ENA_MAX_MSIX_VEC(ENA_MAX_NUM_IO_QUEUES)];

	/* Timer service */
	struct callout timer_service;
	sbintime_t keep_alive_timestamp;
	uint32_t next_monitored_tx_qid;
	struct task reset_task;
	struct taskqueue *reset_tq;
	int wd_active;
	sbintime_t keep_alive_timeout;
	sbintime_t missing_tx_timeout;
	uint32_t missing_tx_max_queues;
	uint32_t missing_tx_threshold;
	bool disable_meta_caching;

	uint16_t eni_metrics_sample_interval;
	uint16_t eni_metrics_sample_interval_cnt;

	/* Statistics */
	struct ena_stats_dev dev_stats;
	struct ena_hw_stats hw_stats;
	struct ena_admin_eni_stats eni_metrics;

	enum ena_regs_reset_reason_types reset_reason;
};

#define	ENA_RING_MTX_LOCK(_ring)		mtx_lock(&(_ring)->ring_mtx)
#define	ENA_RING_MTX_TRYLOCK(_ring)		mtx_trylock(&(_ring)->ring_mtx)
#define	ENA_RING_MTX_UNLOCK(_ring)		mtx_unlock(&(_ring)->ring_mtx)
#define ENA_RING_MTX_ASSERT(_ring)		\
	mtx_assert(&(_ring)->ring_mtx, MA_OWNED)

#define ENA_LOCK_INIT()					\
	sx_init(&ena_global_lock,	"ENA global lock")
#define ENA_LOCK_DESTROY()		sx_destroy(&ena_global_lock)
#define ENA_LOCK_LOCK()			sx_xlock(&ena_global_lock)
#define ENA_LOCK_UNLOCK()		sx_unlock(&ena_global_lock)
#define ENA_LOCK_ASSERT()		sx_assert(&ena_global_lock, SA_XLOCKED)

#define	ENA_TIMER_INIT(_adapter)					\
	callout_init(&(_adapter)->timer_service, true)
#define	ENA_TIMER_DRAIN(_adapter)					\
	callout_drain(&(_adapter)->timer_service)
#define	ENA_TIMER_RESET(_adapter)					\
	callout_reset_sbt(&(_adapter)->timer_service, SBT_1S, SBT_1S,	\
			ena_timer_service, (void*)(_adapter), 0)

#define clamp_t(type, _x, min, max)	min_t(type, max_t(type, _x, min), max)
#define clamp_val(val, lo, hi)		clamp_t(__typeof(val), val, lo, hi)

extern struct sx ena_global_lock;

static inline int ena_mbuf_count(struct mbuf *mbuf)
{
	int count = 1;

	while ((mbuf = mbuf->m_next) != NULL)
		++count;

	return count;
}

int	ena_up(struct ena_adapter *adapter);
void	ena_down(struct ena_adapter *adapter);
int	ena_restore_device(struct ena_adapter *adapter);
void	ena_destroy_device(struct ena_adapter *adapter, bool graceful);
int	ena_refill_rx_bufs(struct ena_ring *rx_ring, uint32_t num);
int	ena_update_buf_ring_size(struct ena_adapter *adapter,
    uint32_t new_buf_ring_size);
int	ena_update_queue_size(struct ena_adapter *adapter, uint32_t new_tx_size,
    uint32_t new_rx_size);
int	ena_update_io_queue_nb(struct ena_adapter *adapter, uint32_t new_num);

static inline void
ena_trigger_reset(struct ena_adapter *adapter,
    enum ena_regs_reset_reason_types reset_reason)
{
	if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		adapter->reset_reason = reset_reason;
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	}
}

#endif /* !(ENA_H) */
