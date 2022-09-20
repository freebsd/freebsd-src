/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
 * Copyright © 2022 Mathew McBride
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#ifndef	_DPAA2_NI_H
#define	_DPAA2_NI_H

#include <sys/rman.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/mbuf.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/buf_ring.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include "dpaa2_types.h"
#include "dpaa2_mcp.h"
#include "dpaa2_swp.h"
#include "dpaa2_io.h"
#include "dpaa2_mac.h"
#include "dpaa2_ni_dpkg.h"

/* Name of the DPAA2 network interface. */
#define DPAA2_NI_IFNAME		"dpni"

/* Maximum resources per DPNI: 16 DPIOs + 16 DPCONs + 1 DPBP + 1 DPMCP. */
#define DPAA2_NI_MAX_RESOURCES	34

#define DPAA2_NI_MSI_COUNT	1  /* MSIs per DPNI */
#define DPAA2_NI_MAX_CHANNELS	16 /* to distribute ingress traffic to cores */
#define DPAA2_NI_MAX_TCS	8  /* traffic classes per DPNI */
#define DPAA2_NI_MAX_POOLS	8  /* buffer pools per DPNI */

/* Maximum number of Rx buffers. */
#define DPAA2_NI_BUFS_INIT	(50u * DPAA2_SWP_BUFS_PER_CMD)
#define DPAA2_NI_BUFS_MAX	(1 << 15) /* 15 bits for buffer index max. */

/* Maximum number of buffers allocated per Tx ring. */
#define DPAA2_NI_BUFS_PER_TX	(1 << 7)
#define DPAA2_NI_MAX_BPTX	(1 << 8) /* 8 bits for buffer index max. */

/* Number of the DPNI statistics counters. */
#define DPAA2_NI_STAT_COUNTERS	7u
#define	DPAA2_NI_STAT_SYSCTLS	9u

/* Error and status bits in the frame annotation status word. */
#define DPAA2_NI_FAS_DISC	0x80000000 /* debug frame */
#define DPAA2_NI_FAS_MS		0x40000000 /* MACSEC frame */
#define DPAA2_NI_FAS_PTP	0x08000000
#define DPAA2_NI_FAS_MC		0x04000000 /* Ethernet multicast frame */
#define DPAA2_NI_FAS_BC		0x02000000 /* Ethernet broadcast frame */
#define DPAA2_NI_FAS_KSE	0x00040000
#define DPAA2_NI_FAS_EOFHE	0x00020000
#define DPAA2_NI_FAS_MNLE	0x00010000
#define DPAA2_NI_FAS_TIDE	0x00008000
#define DPAA2_NI_FAS_PIEE	0x00004000
#define DPAA2_NI_FAS_FLE	0x00002000 /* Frame length error */
#define DPAA2_NI_FAS_FPE	0x00001000 /* Frame physical error */
#define DPAA2_NI_FAS_PTE	0x00000080
#define DPAA2_NI_FAS_ISP	0x00000040
#define DPAA2_NI_FAS_PHE	0x00000020
#define DPAA2_NI_FAS_BLE	0x00000010
#define DPAA2_NI_FAS_L3CV	0x00000008 /* L3 csum validation performed */
#define DPAA2_NI_FAS_L3CE	0x00000004 /* L3 csum error */
#define DPAA2_NI_FAS_L4CV	0x00000002 /* L4 csum validation performed */
#define DPAA2_NI_FAS_L4CE	0x00000001 /* L4 csum error */

/* Mask for errors on the ingress path. */
#define DPAA2_NI_FAS_RX_ERR_MASK (DPAA2_NI_FAS_KSE |	\
    DPAA2_NI_FAS_EOFHE |				\
    DPAA2_NI_FAS_MNLE |					\
    DPAA2_NI_FAS_TIDE |					\
    DPAA2_NI_FAS_PIEE |					\
    DPAA2_NI_FAS_FLE |					\
    DPAA2_NI_FAS_FPE |					\
    DPAA2_NI_FAS_PTE |					\
    DPAA2_NI_FAS_ISP |					\
    DPAA2_NI_FAS_PHE |					\
    DPAA2_NI_FAS_BLE |					\
    DPAA2_NI_FAS_L3CE |					\
    DPAA2_NI_FAS_L4CE					\
)

/* Option bits to select specific queue configuration options to apply. */
#define DPAA2_NI_QUEUE_OPT_USER_CTX	0x00000001
#define DPAA2_NI_QUEUE_OPT_DEST		0x00000002
#define DPAA2_NI_QUEUE_OPT_FLC		0x00000004
#define DPAA2_NI_QUEUE_OPT_HOLD_ACTIVE	0x00000008
#define DPAA2_NI_QUEUE_OPT_SET_CGID	0x00000040
#define DPAA2_NI_QUEUE_OPT_CLEAR_CGID	0x00000080

/* DPNI link configuration options. */
#define DPAA2_NI_LINK_OPT_AUTONEG	((uint64_t) 0x01u)
#define DPAA2_NI_LINK_OPT_HALF_DUPLEX	((uint64_t) 0x02u)
#define DPAA2_NI_LINK_OPT_PAUSE		((uint64_t) 0x04u)
#define DPAA2_NI_LINK_OPT_ASYM_PAUSE	((uint64_t) 0x08u)
#define DPAA2_NI_LINK_OPT_PFC_PAUSE	((uint64_t) 0x10u)

/*
 * Number of times to retry a frame enqueue before giving up. Value determined
 * empirically, in order to minimize the number of frames dropped on Tx.
 */
#define DPAA2_NI_ENQUEUE_RETRIES	10

enum dpaa2_ni_queue_type {
	DPAA2_NI_QUEUE_RX = 0,
	DPAA2_NI_QUEUE_TX,
	DPAA2_NI_QUEUE_TX_CONF,
	DPAA2_NI_QUEUE_RX_ERR
};

enum dpaa2_ni_dest_type {
	DPAA2_NI_DEST_NONE = 0,
	DPAA2_NI_DEST_DPIO,
	DPAA2_NI_DEST_DPCON
};

enum dpaa2_ni_ofl_type {
	DPAA2_NI_OFL_RX_L3_CSUM = 0,
	DPAA2_NI_OFL_RX_L4_CSUM,
	DPAA2_NI_OFL_TX_L3_CSUM,
	DPAA2_NI_OFL_TX_L4_CSUM,
	DPAA2_NI_OFL_FLCTYPE_HASH /* FD flow context for AIOP/CTLU */
};

/**
 * @brief DPNI ingress traffic distribution mode.
 */
enum dpaa2_ni_dist_mode {
	DPAA2_NI_DIST_MODE_NONE = 0,
	DPAA2_NI_DIST_MODE_HASH,
	DPAA2_NI_DIST_MODE_FS
};

/**
 * @brief DPNI behavior in case of errors.
 */
enum dpaa2_ni_err_action {
	DPAA2_NI_ERR_DISCARD = 0,
	DPAA2_NI_ERR_CONTINUE,
	DPAA2_NI_ERR_SEND_TO_ERROR_QUEUE
};

struct dpaa2_ni_channel;
struct dpaa2_ni_fq;

/**
 * @brief Attributes of the DPNI object.
 *
 * options:	 ...
 * wriop_ver:	 Revision of the underlying WRIOP hardware block.
 */
struct dpaa2_ni_attr {
	uint32_t		 options;
	uint16_t		 wriop_ver;
	struct {
		uint16_t	 fs;
		uint8_t		 mac;
		uint8_t		 vlan;
		uint8_t		 qos;
	} entries;
	struct {
		uint8_t		 queues;
		uint8_t		 rx_tcs;
		uint8_t		 tx_tcs;
		uint8_t		 channels;
		uint8_t		 cgs;
	} num;
	struct {
		uint8_t		 fs;
		uint8_t		 qos;
	} key_size;
};

/**
 * @brief Tx ring.
 *
 * fq:		Parent (TxConf) frame queue.
 * fqid:	ID of the logical Tx queue.
 * mbuf_br:	Ring buffer for mbufs to transmit.
 * mbuf_lock:	Lock for the ring buffer.
 */
struct dpaa2_ni_tx_ring {
	struct dpaa2_ni_fq	*fq;
	uint32_t		 fqid;
	uint32_t		 txid; /* Tx ring index */

	/* Ring buffer for indexes in "buf" array. */
	struct buf_ring		*idx_br;
	struct mtx		 lock;

	/* Buffers to DMA load/unload Tx mbufs. */
	struct dpaa2_buf	 buf[DPAA2_NI_BUFS_PER_TX];
};

/**
 * @brief A Frame Queue is the basic queuing structure used by the QMan.
 *
 * It comprises a list of frame descriptors (FDs), so it can be thought of
 * as a queue of frames.
 *
 * NOTE: When frames on a FQ are ready to be processed, the FQ is enqueued
 *	 onto a work queue (WQ).
 *
 * fqid:	Frame queue ID, can be used to enqueue/dequeue or execute other
 *		commands on the queue through DPIO.
 * txq_n:	Number of configured Tx queues.
 * tx_fqid:	Frame queue IDs of the Tx queues which belong to the same flowid.
 *		Note that Tx queues are logical queues and not all management
 *		commands are available on these queue types.
 * qdbin:	Queue destination bin. Can be used with the DPIO enqueue
 *		operation based on QDID, QDBIN and QPRI. Note that all Tx queues
 *		with the same flowid have the same destination bin.
 */
struct dpaa2_ni_fq {
	int (*consume)(struct dpaa2_ni_channel *,
	    struct dpaa2_ni_fq *, struct dpaa2_fd *);

	struct dpaa2_ni_channel	*chan;
	uint32_t		 fqid;
	uint16_t		 flowid;
	uint8_t			 tc;
	enum dpaa2_ni_queue_type type;

	/* Optional fields (for TxConf queue). */
	struct dpaa2_ni_tx_ring	 tx_rings[DPAA2_NI_MAX_TCS];
	uint32_t		 tx_qdbin;
} __aligned(CACHE_LINE_SIZE);

/**
 * @brief QBMan channel to process ingress traffic (Rx, Tx conf).
 *
 * NOTE: Several WQs are organized into a single WQ Channel.
 */
struct dpaa2_ni_channel {
	device_t		 ni_dev;
	device_t		 io_dev;
	device_t		 con_dev;
	uint16_t		 id;
	uint16_t		 flowid;

	/* For debug purposes only! */
	uint64_t		 tx_frames;
	uint64_t		 tx_dropped;

	/* Context to configure CDAN. */
	struct dpaa2_io_notif_ctx ctx;

	/* Channel storage (to keep responses from VDQ command). */
	struct dpaa2_buf	 store;
	uint32_t		 store_sz; /* in frames */
	uint32_t		 store_idx; /* frame index */

	/* Recycled buffers to release back to the pool. */
	uint32_t		 recycled_n;
	bus_addr_t		 recycled[DPAA2_SWP_BUFS_PER_CMD];

	/* Frame queues */
	uint32_t		 rxq_n;
	struct dpaa2_ni_fq	 rx_queues[DPAA2_NI_MAX_TCS];
	struct dpaa2_ni_fq	 txc_queue;
};

/**
 * @brief Configuration of the network interface queue.
 *
 * NOTE: This configuration is used to obtain information of a queue by
 *	 DPNI_GET_QUEUE command and update it by DPNI_SET_QUEUE one.
 *
 * It includes binding of the queue to a DPIO or DPCON object to receive
 * notifications and traffic on the CPU.
 *
 * user_ctx:	(r/w) User defined data, presented along with the frames
 *		being dequeued from this queue.
 * flow_ctx:	(r/w) Set default FLC value for traffic dequeued from this queue.
 *		Please check description of FD structure for more information.
 *		Note that FLC values set using DPNI_ADD_FS_ENTRY, if any, take
 *		precedence over values per queue.
 * dest_id:	(r/w) The ID of a DPIO or DPCON object, depending on
 *		DEST_TYPE (in flags) value. This field is ignored for DEST_TYPE
 *		set to 0 (DPNI_DEST_NONE).
 * fqid:	(r) Frame queue ID, can be used to enqueue/dequeue or execute
 *		other commands on the queue through DPIO. Note that Tx queues
 *		are logical queues and not all management commands are available
 *		on these queue types.
 * qdbin:	(r) Queue destination bin. Can be used with the DPIO enqueue
 *		operation based on QDID, QDBIN and QPRI.
 * type:	Type of the queue to set configuration to.
 * tc:		Traffic class. Ignored for QUEUE_TYPE 2 and 3 (Tx confirmation
 *		and Rx error queues).
 * idx:		Selects a specific queue out of the set of queues in a TC.
 *		Accepted values are in range 0 to NUM_QUEUES–1. This field is
 *		ignored for QUEUE_TYPE 3 (Rx error queue). For access to the
 *		shared Tx confirmation queue (for Tx confirmation mode 1), this
 *		field must be set to 0xff.
 * cgid:	(r/w) Congestion group ID.
 * chan_id:	(w) Channel index to be configured. Used only when QUEUE_TYPE is
 *		set to DPNI_QUEUE_TX.
 * priority:	(r/w) Sets the priority in the destination DPCON or DPIO for
 *		dequeued traffic. Supported values are 0 to # of priorities in
 *		destination DPCON or DPIO - 1. This field is ignored for
 *		DEST_TYPE set to 0 (DPNI_DEST_NONE), except if this DPNI is in
 *		AIOP context. In that case the DPNI_SET_QUEUE can be used to
 *		override the default assigned priority of the FQ from the TC.
 * options:	Option bits selecting specific configuration options to apply.
 *		See DPAA2_NI_QUEUE_OPT_* for details.
 * dest_type:	Type of destination for dequeued traffic.
 * cgid_valid:	(r) Congestion group ID is valid.
 * stash_control: (r/w) If true, lowest 6 bits of FLC are used for stash control.
 *		Please check description of FD structure for more information.
 * hold_active:	(r/w) If true, this flag prevents the queue from being
 *		rescheduled between DPIOs while it carries traffic and is active
 *		on one DPIO. Can help reduce reordering if one queue is services
 *		on multiple CPUs, but the queue is also more likely to be trapped
 *		in one DPIO, especially when congested.
 */
struct dpaa2_ni_queue_cfg {
	uint64_t		 user_ctx;
	uint64_t		 flow_ctx;
	uint32_t		 dest_id;
	uint32_t		 fqid;
	uint16_t		 qdbin;
	enum dpaa2_ni_queue_type type;
	uint8_t			 tc;
	uint8_t			 idx;
	uint8_t			 cgid;
	uint8_t			 chan_id;
	uint8_t			 priority;
	uint8_t			 options;

	enum dpaa2_ni_dest_type	 dest_type;
	bool			 cgid_valid;
	bool			 stash_control;
	bool			 hold_active;
};

/**
 * @brief Buffer layout attributes.
 *
 * pd_size:		Size kept for private data (in bytes).
 * fd_align:		Frame data alignment.
 * head_size:		Data head room.
 * tail_size:		Data tail room.
 * options:		...
 * pass_timestamp:	Timestamp is included in the buffer layout.
 * pass_parser_result:	Parsing results are included in the buffer layout.
 * pass_frame_status:	Frame status is included in the buffer layout.
 * pass_sw_opaque:	SW annotation is activated.
 * queue_type:		Type of a queue this configuration applies to.
 */
struct dpaa2_ni_buf_layout {
	uint16_t	pd_size;
	uint16_t	fd_align;
	uint16_t	head_size;
	uint16_t	tail_size;
	uint16_t	options;
	bool		pass_timestamp;
	bool		pass_parser_result;
	bool		pass_frame_status;
	bool		pass_sw_opaque;
	enum dpaa2_ni_queue_type queue_type;
};

/**
 * @brief Buffer pools configuration for a network interface.
 */
struct dpaa2_ni_pools_cfg {
	uint8_t		pools_num;
	struct {
		uint32_t bp_obj_id;
		uint16_t buf_sz;
		int	 backup_flag; /* 0 - regular pool, 1 - backup pool */
	} pools[DPAA2_NI_MAX_POOLS];
};

/**
 * @brief Errors behavior configuration for a network interface.
 *
 * err_mask:		The errors mask to configure.
 * action:		Desired action for the errors selected in the mask.
 * set_err_fas:		Set to true to mark the errors in frame annotation
 * 			status (FAS); relevant for non-discard actions only.
 */
struct dpaa2_ni_err_cfg {
	uint32_t	err_mask;
	enum dpaa2_ni_err_action action;
	bool		set_err_fas;
};

/**
 * @brief Link configuration.
 *
 * options:	Mask of available options.
 * adv_speeds:	Speeds that are advertised for autoneg.
 * rate:	Rate in Mbps.
 */
struct dpaa2_ni_link_cfg {
	uint64_t	options;
	uint64_t	adv_speeds;
	uint32_t	rate;
};

/**
 * @brief Link state.
 *
 * options:	Mask of available options.
 * adv_speeds:	Speeds that are advertised for autoneg.
 * sup_speeds:	Speeds capability of the PHY.
 * rate:	Rate in Mbps.
 * link_up:	Link state (true if link is up, false otherwise).
 * state_valid:	Ignore/Update the state of the link.
 */
struct dpaa2_ni_link_state {
	uint64_t	options;
	uint64_t	adv_speeds;
	uint64_t	sup_speeds;
	uint32_t	rate;
	bool		link_up;
	bool		state_valid;
};

/**
 * @brief QoS table configuration.
 *
 * kcfg_busaddr:	Address of the buffer in I/O virtual address space which
 *			holds the QoS table key configuration.
 * default_tc:		Default traffic class to use in case of a lookup miss in
 *			the QoS table.
 * discard_on_miss:	Set to true to discard frames in case of no match.
 *			Default traffic class will be used otherwise.
 * keep_entries:	Set to true to keep existing QoS table entries. This
 *			option will work properly only for DPNI objects created
 *			with DPNI_OPT_HAS_KEY_MASKING option.
 */
struct dpaa2_ni_qos_table {
	uint64_t	kcfg_busaddr;
	uint8_t		default_tc;
	bool		discard_on_miss;
	bool		keep_entries;
};

/**
 * @brief Context to add multicast physical addresses to the filter table.
 *
 * ifp:		Network interface associated with the context.
 * error:	Result of the last MC command.
 * nent:	Number of entries added.
 */
struct dpaa2_ni_mcaddr_ctx {
	struct ifnet	*ifp;
	int		 error;
	int		 nent;
};

struct dpaa2_eth_dist_fields {
	uint64_t	rxnfc_field;
	enum net_prot	cls_prot;
	int		cls_field;
	int		size;
	uint64_t	id;
};

struct dpni_mask_cfg {
	uint8_t		mask;
	uint8_t		offset;
} __packed;

struct dpni_dist_extract {
	uint8_t		prot;
	uint8_t		efh_type; /* EFH type is in the 4 LSBs. */
	uint8_t		size;
	uint8_t		offset;
	uint32_t	field;
	uint8_t		hdr_index;
	uint8_t		constant;
	uint8_t		num_of_repeats;
	uint8_t		num_of_byte_masks;
	uint8_t		extract_type; /* Extraction type is in the 4 LSBs */
	uint8_t		_reserved[3];
	struct dpni_mask_cfg masks[4];
} __packed;

struct dpni_ext_set_rx_tc_dist {
	uint8_t		num_extracts;
	uint8_t		_reserved[7];
	struct dpni_dist_extract extracts[DPKG_MAX_NUM_OF_EXTRACTS];
} __packed;

/**
 * @brief Software context for the DPAA2 Network Interface driver.
 */
struct dpaa2_ni_softc {
	device_t		 dev;
	struct resource 	*res[DPAA2_NI_MAX_RESOURCES];
	uint16_t		 api_major;
	uint16_t		 api_minor;
	uint64_t		 rx_hash_fields;
	uint16_t		 tx_data_off;
	uint16_t		 tx_qdid;
	uint32_t		 link_options;
	int			 link_state;

	uint16_t		 buf_align;
	uint16_t		 buf_sz;

	/* For debug purposes only! */
	uint64_t		 rx_anomaly_frames;
	uint64_t		 rx_single_buf_frames;
	uint64_t		 rx_sg_buf_frames;
	uint64_t		 rx_enq_rej_frames;
	uint64_t		 rx_ieoi_err_frames;
	uint64_t		 tx_single_buf_frames;
	uint64_t		 tx_sg_frames;

	/* Attributes of the DPAA2 network interface. */
	struct dpaa2_ni_attr	 attr;

	/* Helps to send commands to MC. */
	struct dpaa2_cmd	*cmd;
	uint16_t		 rc_token;
	uint16_t		 ni_token;

	/* For network interface and miibus. */
	struct ifnet		*ifp;
	uint32_t		 if_flags;
	struct mtx		 lock;
	device_t		 miibus;
	struct mii_data		*mii;
	boolean_t		 fixed_link;
	struct ifmedia		 fixed_ifmedia;
	int			 media_status;

	/* DMA resources */
	bus_dma_tag_t		 bp_dmat;  /* for buffer pool */
	bus_dma_tag_t		 tx_dmat;  /* for Tx buffers */
	bus_dma_tag_t		 st_dmat;  /* for channel storage */
	bus_dma_tag_t		 rxd_dmat; /* for Rx distribution key */
	bus_dma_tag_t		 qos_dmat; /* for QoS table key */
	bus_dma_tag_t		 sgt_dmat; /* for scatter/gather tables */

	struct dpaa2_buf	 qos_kcfg; /* QoS table key config. */
	struct dpaa2_buf	 rxd_kcfg; /* Rx distribution key config. */

	/* Channels and RxError frame queue */
	uint32_t		 chan_n;
	struct dpaa2_ni_channel	*channels[DPAA2_NI_MAX_CHANNELS];
	struct dpaa2_ni_fq	 rxe_queue; /* one per network interface */

	/* Rx buffers for buffer pool. */
	struct dpaa2_atomic	 buf_num;
	struct dpaa2_atomic	 buf_free; /* for sysctl(9) only */
	struct dpaa2_buf	 buf[DPAA2_NI_BUFS_MAX];

	/* Interrupts */
	int			 irq_rid[DPAA2_NI_MSI_COUNT];
	struct resource		*irq_res;
	void			*intr; /* interrupt handle */

	/* Tasks */
	struct taskqueue	*bp_taskq;
	struct task		 bp_task;

	/* Callouts */
	struct callout		 mii_callout;

	struct {
		uint32_t	 dpmac_id;
		uint8_t		 addr[ETHER_ADDR_LEN];
		device_t	 phy_dev;
		int		 phy_loc;
	} mac; /* Info about connected DPMAC (if exists). */
};

extern struct resource_spec dpaa2_ni_spec[];

#endif /* _DPAA2_NI_H */
