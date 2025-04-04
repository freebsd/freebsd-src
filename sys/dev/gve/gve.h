/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023-2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _GVE_FBSD_H
#define _GVE_FBSD_H

#include "gve_desc.h"
#include "gve_plat.h"
#include "gve_register.h"

#ifndef PCI_VENDOR_ID_GOOGLE
#define PCI_VENDOR_ID_GOOGLE	0x1ae0
#endif

#define PCI_DEV_ID_GVNIC	0x0042
#define GVE_REGISTER_BAR	0
#define GVE_DOORBELL_BAR	2

/* Driver can alloc up to 2 segments for the header and 2 for the payload. */
#define GVE_TX_MAX_DESCS	4
#define GVE_TX_BUFRING_ENTRIES	4096

#define ADMINQ_SIZE PAGE_SIZE

#define GVE_DEFAULT_RX_BUFFER_SIZE 2048
/* Each RX bounce buffer page can fit two packet buffers. */
#define GVE_DEFAULT_RX_BUFFER_OFFSET (PAGE_SIZE / 2)

/* PTYPEs are always 10 bits. */
#define GVE_NUM_PTYPES	1024

/*
 * Number of descriptors per queue page list.
 * Page count AKA QPL size can be derived by dividing the number of elements in
 * a page by the number of descriptors available.
 */
#define GVE_QPL_DIVISOR	16

/* Ring Size Limits */
#define GVE_DEFAULT_MIN_RX_RING_SIZE	512
#define GVE_DEFAULT_MIN_TX_RING_SIZE	256

static MALLOC_DEFINE(M_GVE, "gve", "gve allocations");

struct gve_dma_handle {
	bus_addr_t	bus_addr;
	void		*cpu_addr;
	bus_dma_tag_t	tag;
	bus_dmamap_t	map;
};

union gve_tx_desc {
	struct gve_tx_pkt_desc pkt; /* first desc for a packet */
	struct gve_tx_mtd_desc mtd; /* optional metadata descriptor */
	struct gve_tx_seg_desc seg; /* subsequent descs for a packet */
};

/* Tracks the memory in the fifo occupied by a segment of a packet */
struct gve_tx_iovec {
	uint32_t iov_offset; /* offset into this segment */
	uint32_t iov_len; /* length */
	uint32_t iov_padding; /* padding associated with this segment */
};

/* Tracks allowed and current queue settings */
struct gve_queue_config {
	uint16_t max_queues;
	uint16_t num_queues; /* current */
};

struct gve_irq_db {
	__be32 index;
} __aligned(CACHE_LINE_SIZE);

/*
 * GVE_QUEUE_FORMAT_UNSPECIFIED must be zero since 0 is the default value
 * when the entire configure_device_resources command is zeroed out and the
 * queue_format is not specified.
 */
enum gve_queue_format {
	GVE_QUEUE_FORMAT_UNSPECIFIED	= 0x0,
	GVE_GQI_RDA_FORMAT		= 0x1,
	GVE_GQI_QPL_FORMAT		= 0x2,
	GVE_DQO_RDA_FORMAT		= 0x3,
	GVE_DQO_QPL_FORMAT		= 0x4,
};

enum gve_state_flags_bit {
	GVE_STATE_FLAG_ADMINQ_OK,
	GVE_STATE_FLAG_RESOURCES_OK,
	GVE_STATE_FLAG_QPLREG_OK,
	GVE_STATE_FLAG_RX_RINGS_OK,
	GVE_STATE_FLAG_TX_RINGS_OK,
	GVE_STATE_FLAG_QUEUES_UP,
	GVE_STATE_FLAG_LINK_UP,
	GVE_STATE_FLAG_DO_RESET,
	GVE_STATE_FLAG_IN_RESET,
	GVE_NUM_STATE_FLAGS /* Not part of the enum space */
};

BITSET_DEFINE(gve_state_flags, GVE_NUM_STATE_FLAGS);

#define GVE_DEVICE_STATUS_RESET (0x1 << 1)
#define GVE_DEVICE_STATUS_LINK_STATUS (0x1 << 2)

#define GVE_RING_LOCK(ring)	mtx_lock(&(ring)->ring_mtx)
#define GVE_RING_TRYLOCK(ring)	mtx_trylock(&(ring)->ring_mtx)
#define GVE_RING_UNLOCK(ring)	mtx_unlock(&(ring)->ring_mtx)
#define GVE_RING_ASSERT(ring)	mtx_assert(&(ring)->ring_mtx, MA_OWNED)

#define GVE_IFACE_LOCK_INIT(lock)     sx_init(&lock, "gve interface lock")
#define GVE_IFACE_LOCK_DESTROY(lock)  sx_destroy(&lock)
#define GVE_IFACE_LOCK_LOCK(lock)     sx_xlock(&lock)
#define GVE_IFACE_LOCK_UNLOCK(lock)   sx_unlock(&lock)
#define GVE_IFACE_LOCK_ASSERT(lock)   sx_assert(&lock, SA_XLOCKED)

struct gve_queue_page_list {
	uint32_t id;
	uint32_t num_dmas;
	uint32_t num_pages;
	vm_offset_t kva;
	vm_page_t *pages;
	struct gve_dma_handle *dmas;
};

struct gve_irq {
	struct resource *res;
	void *cookie;
};

struct gve_rx_slot_page_info {
	void *page_address;
	vm_page_t page;
	uint32_t page_offset;
	uint16_t pad;
};

/*
 * A single received packet split across multiple buffers may be
 * reconstructed using the information in this structure.
 */
struct gve_rx_ctx {
	/* head and tail of mbuf chain for the current packet */
	struct mbuf *mbuf_head;
	struct mbuf *mbuf_tail;
	uint32_t total_size;
	uint8_t frag_cnt;
	bool is_tcp;
	bool drop_pkt;
};

struct gve_ring_com {
	struct gve_priv *priv;
	uint32_t id;

	/*
	 * BAR2 offset for this ring's doorbell and the
	 * counter-array offset for this ring's counter.
	 * Acquired from the device individually for each
	 * queue in the queue_create adminq command.
	 */
	struct gve_queue_resources *q_resources;
	struct gve_dma_handle q_resources_mem;

	/* Byte offset into BAR2 where this ring's 4-byte irq doorbell lies. */
	uint32_t irq_db_offset;
	/* Byte offset into BAR2 where this ring's 4-byte doorbell lies. */
	uint32_t db_offset;
	/*
	 * Index, not byte-offset, into the counter array where this ring's
	 * 4-byte counter lies.
	 */
	uint32_t counter_idx;

	/*
	 * The index of the MSIX vector that was assigned to
	 * this ring in `gve_alloc_irqs`.
	 *
	 * It is passed to the device in the queue_create adminq
	 * command.
	 *
	 * Additionally, this also serves as the index into
	 * `priv->irq_db_indices` where this ring's irq doorbell's
	 * BAR2 offset, `irq_db_idx`, can be found.
	 */
	int ntfy_id;

	/*
	 * The fixed bounce buffer for this ring.
	 * Once allocated, has to be offered to the device
	 * over the register-page-list adminq command.
	 */
	struct gve_queue_page_list *qpl;

	struct task cleanup_task;
	struct taskqueue *cleanup_tq;
} __aligned(CACHE_LINE_SIZE);

struct gve_rxq_stats {
	counter_u64_t rbytes;
	counter_u64_t rpackets;
	counter_u64_t rx_dropped_pkt;
	counter_u64_t rx_copybreak_cnt;
	counter_u64_t rx_frag_flip_cnt;
	counter_u64_t rx_frag_copy_cnt;
	counter_u64_t rx_dropped_pkt_desc_err;
	counter_u64_t rx_dropped_pkt_buf_post_fail;
	counter_u64_t rx_dropped_pkt_mbuf_alloc_fail;
	counter_u64_t rx_mbuf_dmamap_err;
	counter_u64_t rx_mbuf_mclget_null;
};

#define NUM_RX_STATS (sizeof(struct gve_rxq_stats) / sizeof(counter_u64_t))

union gve_rx_qpl_buf_id_dqo {
	struct {
		uint16_t buf_id:11; /* Index into rx->dqo.bufs */
		uint8_t frag_num:5; /* Which frag in the QPL page */
	};
	uint16_t all;
} __packed;
_Static_assert(sizeof(union gve_rx_qpl_buf_id_dqo) == 2,
    "gve: bad dqo qpl rx buf id length");

struct gve_rx_buf_dqo {
	union {
		/* RDA */
		struct {
			struct mbuf *mbuf;
			bus_dmamap_t dmamap;
			uint64_t addr;
			bool mapped;
		};
		/* QPL */
		struct {
			uint8_t num_nic_frags; /* number of pending completions */
			uint8_t next_idx;  /* index of the next frag to post */
			/* for chaining rx->dqo.used_bufs */
			STAILQ_ENTRY(gve_rx_buf_dqo) stailq_entry;
		};
	};
	/* for chaining rx->dqo.free_bufs */
	SLIST_ENTRY(gve_rx_buf_dqo) slist_entry;
};

/* power-of-2 sized receive ring */
struct gve_rx_ring {
	struct gve_ring_com com;
	struct gve_dma_handle desc_ring_mem;
	uint32_t cnt; /* free-running total number of completed packets */
	uint32_t fill_cnt; /* free-running total number of descs and buffs posted */

	union {
		/* GQI-only fields */
		struct {
			struct gve_dma_handle data_ring_mem;

			/* accessed in the GQ receive hot path */
			struct gve_rx_desc *desc_ring;
			union gve_rx_data_slot *data_ring;
			struct gve_rx_slot_page_info *page_info;
			uint32_t mask; /* masks the cnt and fill_cnt to the size of the ring */
			uint8_t seq_no; /* helps traverse the descriptor ring */
		};

		/* DQO-only fields */
		struct {
			struct gve_dma_handle compl_ring_mem;

			struct gve_rx_compl_desc_dqo *compl_ring;
			struct gve_rx_desc_dqo *desc_ring;
			struct gve_rx_buf_dqo *bufs; /* Parking place for posted buffers */
			bus_dma_tag_t buf_dmatag; /* To dmamap posted mbufs with */

			uint32_t buf_cnt; /* Size of the bufs array */
			uint32_t mask; /* One less than the sizes of the desc and compl rings */
			uint32_t head; /* The index at which to post the next buffer at */
			uint32_t tail; /* The index at which to receive the next compl at */
			uint8_t cur_gen_bit; /* Gets flipped on every cycle of the compl ring */
			SLIST_HEAD(, gve_rx_buf_dqo) free_bufs;

			/*
			 * Only used in QPL mode. Pages referred to by if_input-ed mbufs
			 * stay parked here till their wire count comes back to 1.
			 * Pages are moved here after there aren't any pending completions.
			 */
			STAILQ_HEAD(, gve_rx_buf_dqo) used_bufs;
		} dqo;
	};

	struct lro_ctrl lro;
	struct gve_rx_ctx ctx;
	struct gve_rxq_stats stats;

} __aligned(CACHE_LINE_SIZE);

/*
 * A contiguous representation of the pages composing the Tx bounce buffer.
 * The xmit taskqueue and the completion taskqueue both simultaneously use it.
 * Both operate on `available`: the xmit tq lowers it and the completion tq
 * raises it. `head` is the last location written at and so only the xmit tq
 * uses it.
 */
struct gve_tx_fifo {
	vm_offset_t base; /* address of base of FIFO */
	uint32_t size; /* total size */
	volatile int available; /* how much space is still available */
	uint32_t head; /* offset to write at */
};

struct gve_tx_buffer_state {
	struct mbuf *mbuf;
	struct gve_tx_iovec iov[GVE_TX_MAX_DESCS];
};

struct gve_txq_stats {
	counter_u64_t tbytes;
	counter_u64_t tpackets;
	counter_u64_t tso_packet_cnt;
	counter_u64_t tx_dropped_pkt;
	counter_u64_t tx_delayed_pkt_nospace_device;
	counter_u64_t tx_dropped_pkt_nospace_bufring;
	counter_u64_t tx_delayed_pkt_nospace_descring;
	counter_u64_t tx_delayed_pkt_nospace_compring;
	counter_u64_t tx_delayed_pkt_nospace_qpl_bufs;
	counter_u64_t tx_delayed_pkt_tsoerr;
	counter_u64_t tx_dropped_pkt_vlan;
	counter_u64_t tx_mbuf_collapse;
	counter_u64_t tx_mbuf_defrag;
	counter_u64_t tx_mbuf_defrag_err;
	counter_u64_t tx_mbuf_dmamap_enomem_err;
	counter_u64_t tx_mbuf_dmamap_err;
};

#define NUM_TX_STATS (sizeof(struct gve_txq_stats) / sizeof(counter_u64_t))

struct gve_tx_pending_pkt_dqo {
	struct mbuf *mbuf;
	union {
		/* RDA */
		bus_dmamap_t dmamap;
		/* QPL */
		struct {
			/*
			 * A linked list of entries from qpl_bufs that served
			 * as the bounce buffer for this packet.
			 */
			int32_t qpl_buf_head;
			uint32_t num_qpl_bufs;
		};
	};
	uint8_t state; /* the gve_packet_state enum */
	int next; /* To chain the free_pending_pkts lists */
};

/* power-of-2 sized transmit ring */
struct gve_tx_ring {
	struct gve_ring_com com;
	struct gve_dma_handle desc_ring_mem;

	struct task xmit_task;
	struct taskqueue *xmit_tq;
	bool stopped;

	/* Accessed when writing descriptors */
	struct buf_ring *br;
	struct mtx ring_mtx;

	uint32_t req; /* free-running total number of packets written to the nic */
	uint32_t done; /* free-running total number of completed packets */

	union {
		/* GQI specific stuff */
		struct {
			union gve_tx_desc *desc_ring;
			struct gve_tx_buffer_state *info;

			struct gve_tx_fifo fifo;

			uint32_t mask; /* masks the req and done to the size of the ring */
		};

		/* DQO specific stuff */
		struct {
			struct gve_dma_handle compl_ring_mem;

			/* Accessed when writing descriptors */
			struct {
				union gve_tx_desc_dqo *desc_ring;
				uint32_t desc_mask; /* masks head and tail to the size of desc_ring */
				uint32_t desc_head; /* last desc read by NIC, cached value of hw_tx_head */
				uint32_t desc_tail; /* last desc written by driver */
				uint32_t last_re_idx; /* desc which last had "report event" set */

				/*
				 * The head index of a singly linked list containing pending packet objects
				 * to park mbufs till the NIC sends completions. Once this list is depleted,
				 * the "_prd" suffixed producer list, grown by the completion taskqueue,
				 * is stolen.
				 */
				int32_t free_pending_pkts_csm;

				/*
				 * The head index of a singly linked list representing QPL page fragments
				 * to copy mbuf payload into for the NIC to see. Once this list is depleted,
				 * the "_prd" suffixed producer list, grown by the completion taskqueue,
				 * is stolen.
				 *
				 * Only used in QPL mode. int32_t because atomic_swap_16 doesn't exist.
				 */
				int32_t free_qpl_bufs_csm;
				uint32_t qpl_bufs_consumed; /* Allows quickly checking for buf availability */
				uint32_t qpl_bufs_produced_cached; /* Cached value of qpl_bufs_produced */

				/* DMA params for mapping Tx mbufs. Only used in RDA mode. */
				bus_dma_tag_t buf_dmatag;
			} __aligned(CACHE_LINE_SIZE);

			/* Accessed when processing completions */
			struct {
				struct gve_tx_compl_desc_dqo *compl_ring;
				uint32_t compl_mask; /* masks head to the size of compl_ring */
				uint32_t compl_head; /* last completion read by driver */
				uint8_t cur_gen_bit; /* NIC flips a bit on every pass */
				uint32_t hw_tx_head; /* last desc read by NIC */

				/*
				 * The completion taskqueue moves pending-packet objects to this
				 * list after freeing the mbuf. The "_prd" denotes that this is
				 * a producer list. The transmit taskqueue steals this list once
				 * its consumer list, with the "_csm" suffix, is depleted.
				 */
				int32_t free_pending_pkts_prd;

				/*
				 * The completion taskqueue moves the QPL pages corresponding to a
				 * completed packet into this list. It is only used in QPL mode.
				 * The "_prd" denotes that this is a producer list. The transmit
				 * taskqueue steals this list once its consumer list, with the "_csm"
				 * suffix, is depleted.
				 *
				 * Only used in QPL mode. int32_t because atomic_swap_16 doesn't exist.
				 */
				int32_t free_qpl_bufs_prd;
				uint32_t qpl_bufs_produced;
			} __aligned(CACHE_LINE_SIZE);

			/* Accessed by both the completion and xmit loops */
			struct {
				/* completion tags index into this array */
				struct gve_tx_pending_pkt_dqo *pending_pkts;
				uint16_t num_pending_pkts;

				/*
				 * Represents QPL page fragments. An index into this array
				 * always represents the same QPL page fragment. The value
				 * is also an index into this array and servers as a means
				 * to chain buffers into linked lists whose heads are
				 * either free_qpl_bufs_prd or free_qpl_bufs_csm or
				 * qpl_bufs_head.
				 */
				int32_t *qpl_bufs;
			} __aligned(CACHE_LINE_SIZE);
		} dqo;
	};
	struct gve_txq_stats stats;
} __aligned(CACHE_LINE_SIZE);

enum gve_packet_state {
	/*
	 * Packet does not yet have a dmamap created.
	 * This should always be zero since state is not explicitly initialized.
	 */
	GVE_PACKET_STATE_UNALLOCATED,
	/* Packet has a dmamap and is in free list, available to be allocated. */
	GVE_PACKET_STATE_FREE,
	/* Packet is expecting a regular data completion */
	GVE_PACKET_STATE_PENDING_DATA_COMPL,
};

struct gve_ptype {
	uint8_t l3_type;  /* `gve_l3_type` in gve_adminq.h */
	uint8_t l4_type;  /* `gve_l4_type` in gve_adminq.h */
};

struct gve_ptype_lut {
	struct gve_ptype ptypes[GVE_NUM_PTYPES];
};

struct gve_priv {
	if_t ifp;
	device_t dev;
	struct ifmedia media;

	uint8_t mac[ETHER_ADDR_LEN];

	struct gve_dma_handle aq_mem;

	struct resource *reg_bar; /* BAR0 */
	struct resource *db_bar; /* BAR2 */
	struct resource *msix_table;

	uint32_t mgmt_msix_idx;
	uint32_t rx_copybreak;

	uint16_t num_event_counters;
	uint16_t default_num_queues;
	uint16_t tx_desc_cnt;
	uint16_t max_tx_desc_cnt;
	uint16_t min_tx_desc_cnt;
	uint16_t rx_desc_cnt;
	uint16_t max_rx_desc_cnt;
	uint16_t min_rx_desc_cnt;
	uint16_t rx_pages_per_qpl;
	uint64_t max_registered_pages;
	uint64_t num_registered_pages;
	uint32_t supported_features;
	uint16_t max_mtu;
	bool modify_ringsize_enabled;

	struct gve_dma_handle counter_array_mem;
	__be32 *counters;
	struct gve_dma_handle irqs_db_mem;
	struct gve_irq_db *irq_db_indices;

	enum gve_queue_format queue_format;
	struct gve_queue_config tx_cfg;
	struct gve_queue_config rx_cfg;
	uint32_t num_queues;

	struct gve_irq *irq_tbl;
	struct gve_tx_ring *tx;
	struct gve_rx_ring *rx;

	struct gve_ptype_lut *ptype_lut_dqo;

	/*
	 * Admin queue - see gve_adminq.h
	 * Since AQ cmds do not run in steady state, 32 bit counters suffice
	 */
	struct gve_adminq_command *adminq;
	vm_paddr_t adminq_bus_addr;
	uint32_t adminq_mask; /* masks prod_cnt to adminq size */
	uint32_t adminq_prod_cnt; /* free-running count of AQ cmds executed */
	uint32_t adminq_cmd_fail; /* free-running count of AQ cmds failed */
	uint32_t adminq_timeouts; /* free-running count of AQ cmds timeouts */
	/* free-running count of each distinct AQ cmd executed */
	uint32_t adminq_describe_device_cnt;
	uint32_t adminq_cfg_device_resources_cnt;
	uint32_t adminq_register_page_list_cnt;
	uint32_t adminq_unregister_page_list_cnt;
	uint32_t adminq_create_tx_queue_cnt;
	uint32_t adminq_create_rx_queue_cnt;
	uint32_t adminq_destroy_tx_queue_cnt;
	uint32_t adminq_destroy_rx_queue_cnt;
	uint32_t adminq_dcfg_device_resources_cnt;
	uint32_t adminq_set_driver_parameter_cnt;
	uint32_t adminq_verify_driver_compatibility_cnt;
	uint32_t adminq_get_ptype_map_cnt;

	uint32_t interface_up_cnt;
	uint32_t interface_down_cnt;
	uint32_t reset_cnt;

	struct task service_task;
	struct taskqueue *service_tq;

	struct gve_state_flags state_flags;
	struct sx gve_iface_lock;
};

static inline bool
gve_get_state_flag(struct gve_priv *priv, int pos)
{
	return (BIT_ISSET(GVE_NUM_STATE_FLAGS, pos, &priv->state_flags));
}

static inline void
gve_set_state_flag(struct gve_priv *priv, int pos)
{
	BIT_SET_ATOMIC(GVE_NUM_STATE_FLAGS, pos, &priv->state_flags);
}

static inline void
gve_clear_state_flag(struct gve_priv *priv, int pos)
{
	BIT_CLR_ATOMIC(GVE_NUM_STATE_FLAGS, pos, &priv->state_flags);
}

static inline bool
gve_is_gqi(struct gve_priv *priv)
{
	return (priv->queue_format == GVE_GQI_QPL_FORMAT);
}

static inline bool
gve_is_qpl(struct gve_priv *priv)
{
	return (priv->queue_format == GVE_GQI_QPL_FORMAT ||
	    priv->queue_format == GVE_DQO_QPL_FORMAT);
}

/* Defined in gve_main.c */
void gve_schedule_reset(struct gve_priv *priv);
int gve_adjust_tx_queues(struct gve_priv *priv, uint16_t new_queue_cnt);
int gve_adjust_rx_queues(struct gve_priv *priv, uint16_t new_queue_cnt);
int gve_adjust_ring_sizes(struct gve_priv *priv, uint16_t new_desc_cnt, bool is_rx);

/* Register access functions defined in gve_utils.c */
uint32_t gve_reg_bar_read_4(struct gve_priv *priv, bus_size_t offset);
void gve_reg_bar_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val);
void gve_db_bar_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val);
void gve_db_bar_dqo_write_4(struct gve_priv *priv, bus_size_t offset, uint32_t val);

/* QPL (Queue Page List) functions defined in gve_qpl.c */
struct gve_queue_page_list *gve_alloc_qpl(struct gve_priv *priv, uint32_t id,
    int npages, bool single_kva);
void gve_free_qpl(struct gve_priv *priv, struct gve_queue_page_list *qpl);
int gve_register_qpls(struct gve_priv *priv);
int gve_unregister_qpls(struct gve_priv *priv);
void gve_mextadd_free(struct mbuf *mbuf);

/* TX functions defined in gve_tx.c */
int gve_alloc_tx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx);
void gve_free_tx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx);
int gve_create_tx_rings(struct gve_priv *priv);
int gve_destroy_tx_rings(struct gve_priv *priv);
int gve_tx_intr(void *arg);
int gve_xmit_ifp(if_t ifp, struct mbuf *mbuf);
void gve_qflush(if_t ifp);
void gve_xmit_tq(void *arg, int pending);
void gve_tx_cleanup_tq(void *arg, int pending);

/* TX functions defined in gve_tx_dqo.c */
int gve_tx_alloc_ring_dqo(struct gve_priv *priv, int i);
void gve_tx_free_ring_dqo(struct gve_priv *priv, int i);
void gve_clear_tx_ring_dqo(struct gve_priv *priv, int i);
int gve_tx_intr_dqo(void *arg);
int gve_xmit_dqo(struct gve_tx_ring *tx, struct mbuf **mbuf_ptr);
int gve_xmit_dqo_qpl(struct gve_tx_ring *tx, struct mbuf *mbuf);
void gve_tx_cleanup_tq_dqo(void *arg, int pending);

/* RX functions defined in gve_rx.c */
int gve_alloc_rx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx);
void gve_free_rx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx);
int gve_create_rx_rings(struct gve_priv *priv);
int gve_destroy_rx_rings(struct gve_priv *priv);
int gve_rx_intr(void *arg);
void gve_rx_cleanup_tq(void *arg, int pending);

/* RX functions defined in gve_rx_dqo.c */
int gve_rx_alloc_ring_dqo(struct gve_priv *priv, int i);
void gve_rx_free_ring_dqo(struct gve_priv *priv, int i);
void gve_rx_prefill_buffers_dqo(struct gve_rx_ring *rx);
void gve_clear_rx_ring_dqo(struct gve_priv *priv, int i);
int gve_rx_intr_dqo(void *arg);
void gve_rx_cleanup_tq_dqo(void *arg, int pending);

/* DMA functions defined in gve_utils.c */
int gve_dma_alloc_coherent(struct gve_priv *priv, int size, int align,
    struct gve_dma_handle *dma);
void gve_dma_free_coherent(struct gve_dma_handle *dma);
int gve_dmamap_create(struct gve_priv *priv, int size, int align,
    struct gve_dma_handle *dma);
void gve_dmamap_destroy(struct gve_dma_handle *dma);

/* IRQ functions defined in gve_utils.c */
void gve_free_irqs(struct gve_priv *priv);
int gve_alloc_irqs(struct gve_priv *priv);
void gve_unmask_all_queue_irqs(struct gve_priv *priv);
void gve_mask_all_queue_irqs(struct gve_priv *priv);

/* Systcl functions defined in gve_sysctl.c */
extern bool gve_disable_hw_lro;
extern char gve_queue_format[8];
extern char gve_version[8];
void gve_setup_sysctl(struct gve_priv *priv);
void gve_accum_stats(struct gve_priv *priv, uint64_t *rpackets,
    uint64_t *rbytes, uint64_t *rx_dropped_pkt, uint64_t *tpackets,
    uint64_t *tbytes, uint64_t *tx_dropped_pkt);

/* Stats functions defined in gve_utils.c */
void gve_alloc_counters(counter_u64_t *stat, int num_stats);
void gve_free_counters(counter_u64_t *stat, int num_stats);

#endif /* _GVE_FBSD_H_ */
