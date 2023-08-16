/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: Navdeep Parhar <np@FreeBSD.org>
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
 *
 */

#ifndef __T4_ADAPTER_H__
#define __T4_ADAPTER_H__

#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/counter.h>
#include <sys/rman.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/seqc.h>
#include <sys/sx.h>
#include <sys/vmem.h>
#include <vm/uma.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/bus.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/pfil.h>
#include <netinet/in.h>
#include <netinet/tcp_lro.h>

#include "offload.h"
#include "t4_ioctl.h"
#include "common/t4_msg.h"
#include "firmware/t4fw_interface.h"

#define KTR_CXGBE	KTR_SPARE3
MALLOC_DECLARE(M_CXGBE);
#define CXGBE_UNIMPLEMENTED(s) \
    panic("%s (%s, line %d) not implemented yet.", s, __FILE__, __LINE__)

/*
 * Same as LIST_HEAD from queue.h.  This is to avoid conflict with LinuxKPI's
 * LIST_HEAD when building iw_cxgbe.
 */
#define	CXGBE_LIST_HEAD(name, type)					\
struct name {								\
	struct type *lh_first;	/* first element */			\
}

#ifndef SYSCTL_ADD_UQUAD
#define SYSCTL_ADD_UQUAD SYSCTL_ADD_QUAD
#define sysctl_handle_64 sysctl_handle_quad
#define CTLTYPE_U64 CTLTYPE_QUAD
#endif

SYSCTL_DECL(_hw_cxgbe);

struct adapter;
typedef struct adapter adapter_t;

enum {
	/*
	 * All ingress queues use this entry size.  Note that the firmware event
	 * queue and any iq expecting CPL_RX_PKT in the descriptor needs this to
	 * be at least 64.
	 */
	IQ_ESIZE = 64,

	/* Default queue sizes for all kinds of ingress queues */
	FW_IQ_QSIZE = 256,
	RX_IQ_QSIZE = 1024,

	/* All egress queues use this entry size */
	EQ_ESIZE = 64,

	/* Default queue sizes for all kinds of egress queues */
	CTRL_EQ_QSIZE = 1024,
	TX_EQ_QSIZE = 1024,

	SW_ZONE_SIZES = 4,	/* cluster, jumbop, jumbo9k, jumbo16k */
	CL_METADATA_SIZE = CACHE_LINE_SIZE,

	SGE_MAX_WR_NDESC = SGE_MAX_WR_LEN / EQ_ESIZE, /* max WR size in desc */
	TX_SGL_SEGS = 39,
	TX_SGL_SEGS_TSO = 38,
	TX_SGL_SEGS_VM = 38,
	TX_SGL_SEGS_VM_TSO = 37,
	TX_SGL_SEGS_EO_TSO = 30,	/* XXX: lower for IPv6. */
	TX_SGL_SEGS_VXLAN_TSO = 37,
	TX_WR_FLITS = SGE_MAX_WR_LEN / 8
};

enum {
	/* adapter intr_type */
	INTR_INTX	= (1 << 0),
	INTR_MSI	= (1 << 1),
	INTR_MSIX	= (1 << 2)
};

enum {
	XGMAC_MTU	= (1 << 0),
	XGMAC_PROMISC	= (1 << 1),
	XGMAC_ALLMULTI	= (1 << 2),
	XGMAC_VLANEX	= (1 << 3),
	XGMAC_UCADDR	= (1 << 4),
	XGMAC_MCADDRS	= (1 << 5),

	XGMAC_ALL	= 0xffff
};

enum {
	/* flags understood by begin_synchronized_op */
	HOLD_LOCK	= (1 << 0),
	SLEEP_OK	= (1 << 1),
	INTR_OK		= (1 << 2),

	/* flags understood by end_synchronized_op */
	LOCK_HELD	= HOLD_LOCK,
};

enum {
	/* adapter flags.  synch_op or adapter_lock. */
	FULL_INIT_DONE	= (1 << 0),
	FW_OK		= (1 << 1),
	CHK_MBOX_ACCESS	= (1 << 2),
	MASTER_PF	= (1 << 3),
	BUF_PACKING_OK	= (1 << 6),
	IS_VF		= (1 << 7),
	KERN_TLS_ON	= (1 << 8),	/* HW is configured for KERN_TLS */
	CXGBE_BUSY	= (1 << 9),

	/* adapter error_flags.  reg_lock for HW_OFF_LIMITS, atomics for the rest. */
	ADAP_STOPPED	= (1 << 0),	/* Adapter has been stopped. */
	ADAP_FATAL_ERR	= (1 << 1),	/* Encountered a fatal error. */
	HW_OFF_LIMITS	= (1 << 2),	/* off limits to all except reset_thread */
	ADAP_CIM_ERR	= (1 << 3),	/* Error was related to FW/CIM. */

	/* port flags */
	HAS_TRACEQ	= (1 << 3),
	FIXED_IFMEDIA	= (1 << 4),	/* ifmedia list doesn't change. */

	/* VI flags */
	DOOMED		= (1 << 0),
	VI_INIT_DONE	= (1 << 1),
	/* 1 << 2 is unused, was VI_SYSCTL_CTX */
	TX_USES_VM_WR	= (1 << 3),
	VI_SKIP_STATS	= (1 << 4),

	/* adapter debug_flags */
	DF_DUMP_MBOX		= (1 << 0),	/* Log all mbox cmd/rpl. */
	DF_LOAD_FW_ANYTIME	= (1 << 1),	/* Allow LOAD_FW after init */
	DF_DISABLE_TCB_CACHE	= (1 << 2),	/* Disable TCB cache (T6+) */
	DF_DISABLE_CFG_RETRY	= (1 << 3),	/* Disable fallback config */
	DF_VERBOSE_SLOWINTR	= (1 << 4),	/* Chatty slow intr handler */
};

#define IS_DOOMED(vi)	((vi)->flags & DOOMED)
#define SET_DOOMED(vi)	do {(vi)->flags |= DOOMED;} while (0)
#define IS_BUSY(sc)	((sc)->flags & CXGBE_BUSY)
#define SET_BUSY(sc)	do {(sc)->flags |= CXGBE_BUSY;} while (0)
#define CLR_BUSY(sc)	do {(sc)->flags &= ~CXGBE_BUSY;} while (0)

struct vi_info {
	device_t dev;
	struct port_info *pi;
	struct adapter *adapter;

	if_t ifp;
	struct pfil_head *pfil;

	unsigned long flags;
	int if_flags;

	uint16_t *rss, *nm_rss;
	uint16_t viid;		/* opaque VI identifier */
	uint16_t smt_idx;
	uint16_t vin;
	uint8_t vfvld;
	int16_t  xact_addr_filt;/* index of exact MAC address filter */
	uint16_t rss_size;	/* size of VI's RSS table slice */
	uint16_t rss_base;	/* start of VI's RSS table slice */
	int hashen;

	int nintr;
	int first_intr;

	/* These need to be int as they are used in sysctl */
	int ntxq;		/* # of tx queues */
	int first_txq;		/* index of first tx queue */
	int rsrv_noflowq;	/* Reserve queue 0 for non-flowid packets */
	int nrxq;		/* # of rx queues */
	int first_rxq;		/* index of first rx queue */
	int nofldtxq;		/* # of offload tx queues */
	int first_ofld_txq;	/* index of first offload tx queue */
	int nofldrxq;		/* # of offload rx queues */
	int first_ofld_rxq;	/* index of first offload rx queue */
	int nnmtxq;
	int first_nm_txq;
	int nnmrxq;
	int first_nm_rxq;
	int tmr_idx;
	int ofld_tmr_idx;
	int pktc_idx;
	int ofld_pktc_idx;
	int qsize_rxq;
	int qsize_txq;

	struct timeval last_refreshed;
	struct fw_vi_stats_vf stats;
	struct mtx tick_mtx;
	struct callout tick;

	struct sysctl_ctx_list ctx;
	struct sysctl_oid *rxq_oid;
	struct sysctl_oid *txq_oid;
	struct sysctl_oid *nm_rxq_oid;
	struct sysctl_oid *nm_txq_oid;
	struct sysctl_oid *ofld_rxq_oid;
	struct sysctl_oid *ofld_txq_oid;

	uint8_t hw_addr[ETHER_ADDR_LEN]; /* factory MAC address, won't change */
	u_int txq_rr;
	u_int rxq_rr;
};

struct tx_ch_rl_params {
	enum fw_sched_params_rate ratemode;	/* %port (REL) or kbps (ABS) */
	uint32_t maxrate;
};

/* CLRL state */
enum clrl_state {
	CS_UNINITIALIZED = 0,
	CS_PARAMS_SET,			/* sw parameters have been set. */
	CS_HW_UPDATE_REQUESTED,		/* async HW update requested. */
	CS_HW_UPDATE_IN_PROGRESS,	/* sync hw update in progress. */
	CS_HW_CONFIGURED		/* configured in the hardware. */
};

/* CLRL flags */
enum {
	CF_USER		= (1 << 0),	/* was configured by driver ioctl. */
};

struct tx_cl_rl_params {
	enum clrl_state state;
	int refcount;
	uint8_t flags;
	enum fw_sched_params_rate ratemode;	/* %port REL or ABS value */
	enum fw_sched_params_unit rateunit;	/* kbps or pps (when ABS) */
	enum fw_sched_params_mode mode;		/* aggr or per-flow */
	uint32_t maxrate;
	uint16_t pktsize;
	uint16_t burstsize;
};

/* Tx scheduler parameters for a channel/port */
struct tx_sched_params {
	/* Channel Rate Limiter */
	struct tx_ch_rl_params ch_rl;

	/* Class WRR */
	/* XXX */

	/* Class Rate Limiter (including the default pktsize and burstsize). */
	int pktsize;
	int burstsize;
	struct tx_cl_rl_params cl_rl[];
};

struct port_info {
	device_t dev;
	struct adapter *adapter;

	struct vi_info *vi;
	int nvi;
	int up_vis;
	int uld_vis;
	bool vxlan_tcam_entry;

	struct tx_sched_params *sched_params;

	struct mtx pi_lock;
	char lockname[16];
	unsigned long flags;

	uint8_t  lport;		/* associated offload logical port */
	int8_t   mdio_addr;
	uint8_t  port_type;
	uint8_t  mod_type;
	uint8_t  port_id;
	uint8_t  tx_chan;
	uint8_t  mps_bg_map;	/* rx MPS buffer group bitmap */
	uint8_t  rx_e_chan_map;	/* rx TP e-channel bitmap */
	uint8_t  rx_c_chan;	/* rx TP c-channel */

	struct link_config link_cfg;
	struct ifmedia media;

	struct port_stats stats;
	u_int tnl_cong_drops;
	u_int tx_parse_error;
	int fcs_reg;
	uint64_t fcs_base;

	struct sysctl_ctx_list ctx;
};

#define	IS_MAIN_VI(vi)		((vi) == &((vi)->pi->vi[0]))

struct cluster_metadata {
	uma_zone_t zone;
	caddr_t cl;
	u_int refcount;
};

struct fl_sdesc {
	caddr_t cl;
	uint16_t nmbuf;	/* # of driver originated mbufs with ref on cluster */
	int16_t moff;	/* offset of metadata from cl */
	uint8_t zidx;
};

struct tx_desc {
	__be64 flit[8];
};

struct tx_sdesc {
	struct mbuf *m;		/* m_nextpkt linked chain of frames */
	uint8_t desc_used;	/* # of hardware descriptors used by the WR */
};


#define IQ_PAD (IQ_ESIZE - sizeof(struct rsp_ctrl) - sizeof(struct rss_header))
struct iq_desc {
	struct rss_header rss;
	uint8_t cpl[IQ_PAD];
	struct rsp_ctrl rsp;
};
#undef IQ_PAD
CTASSERT(sizeof(struct iq_desc) == IQ_ESIZE);

enum {
	/* iq type */
	IQ_OTHER	= FW_IQ_IQTYPE_OTHER,
	IQ_ETH		= FW_IQ_IQTYPE_NIC,
	IQ_OFLD		= FW_IQ_IQTYPE_OFLD,

	/* iq flags */
	IQ_SW_ALLOCATED	= (1 << 0),	/* sw resources allocated */
	IQ_HAS_FL	= (1 << 1),	/* iq associated with a freelist */
	IQ_RX_TIMESTAMP	= (1 << 2),	/* provide the SGE rx timestamp */
	IQ_LRO_ENABLED	= (1 << 3),	/* iq is an eth rxq with LRO enabled */
	IQ_ADJ_CREDIT	= (1 << 4),	/* hw is off by 1 credit for this iq */
	IQ_HW_ALLOCATED	= (1 << 5),	/* fw/hw resources allocated */

	/* iq state */
	IQS_DISABLED	= 0,
	IQS_BUSY	= 1,
	IQS_IDLE	= 2,

	/* netmap related flags */
	NM_OFF	= 0,
	NM_ON	= 1,
	NM_BUSY	= 2,
};

enum {
	CPL_COOKIE_RESERVED = 0,
	CPL_COOKIE_FILTER,
	CPL_COOKIE_DDP0,
	CPL_COOKIE_DDP1,
	CPL_COOKIE_TOM,
	CPL_COOKIE_HASHFILTER,
	CPL_COOKIE_ETHOFLD,
	CPL_COOKIE_KERN_TLS,

	NUM_CPL_COOKIES = 8	/* Limited by M_COOKIE.  Do not increase. */
};

struct sge_iq;
struct rss_header;
typedef int (*cpl_handler_t)(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
typedef int (*an_handler_t)(struct sge_iq *, const struct rsp_ctrl *);
typedef int (*fw_msg_handler_t)(struct adapter *, const __be64 *);

/*
 * Ingress Queue: T4 is producer, driver is consumer.
 */
struct sge_iq {
	uint16_t flags;
	uint8_t qtype;
	volatile int state;
	struct adapter *adapter;
	struct iq_desc  *desc;	/* KVA of descriptor ring */
	int8_t   intr_pktc_idx;	/* packet count threshold index */
	uint8_t  gen;		/* generation bit */
	uint8_t  intr_params;	/* interrupt holdoff parameters */
	int8_t   cong_drop;	/* congestion drop settings for the queue */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t sidx;		/* index of the entry with the status page */
	uint16_t cidx;		/* consumer index */
	uint16_t cntxt_id;	/* SGE context id for the iq */
	uint16_t abs_id;	/* absolute SGE id for the iq */
	int16_t intr_idx;	/* interrupt used by the queue */

	STAILQ_ENTRY(sge_iq) link;

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;		/* bus address of descriptor ring */
};

enum {
	/* eq type */
	EQ_CTRL		= 1,
	EQ_ETH		= 2,
	EQ_OFLD		= 3,

	/* eq flags */
	EQ_SW_ALLOCATED	= (1 << 0),	/* sw resources allocated */
	EQ_HW_ALLOCATED	= (1 << 1),	/* hw/fw resources allocated */
	EQ_ENABLED	= (1 << 3),	/* open for business */
	EQ_QFLUSH	= (1 << 4),	/* if_qflush in progress */
};

/* Listed in order of preference.  Update t4_sysctls too if you change these */
enum {DOORBELL_UDB, DOORBELL_WCWR, DOORBELL_UDBWC, DOORBELL_KDB};

/*
 * Egress Queue: driver is producer, T4 is consumer.
 *
 * Note: A free list is an egress queue (driver produces the buffers and T4
 * consumes them) but it's special enough to have its own struct (see sge_fl).
 */
struct sge_eq {
	unsigned int flags;	/* MUST be first */
	unsigned int cntxt_id;	/* SGE context id for the eq */
	unsigned int abs_id;	/* absolute SGE id for the eq */
	uint8_t type;		/* EQ_CTRL/EQ_ETH/EQ_OFLD */
	uint8_t doorbells;
	uint8_t tx_chan;	/* tx channel used by the eq */
	struct mtx eq_lock;

	struct tx_desc *desc;	/* KVA of descriptor ring */
	volatile uint32_t *udb;	/* KVA of doorbell (lies within BAR2) */
	u_int udb_qid;		/* relative qid within the doorbell page */
	uint16_t sidx;		/* index of the entry with the status page */
	uint16_t cidx;		/* consumer idx (desc idx) */
	uint16_t pidx;		/* producer idx (desc idx) */
	uint16_t equeqidx;	/* EQUEQ last requested at this pidx */
	uint16_t dbidx;		/* pidx of the most recent doorbell */
	uint16_t iqid;		/* cached iq->cntxt_id (see iq below) */
	volatile u_int equiq;	/* EQUIQ outstanding */
	struct sge_iq *iq;	/* iq that receives egr_update for the eq */

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;		/* bus address of descriptor ring */
	char lockname[16];
};

struct rx_buf_info {
	uma_zone_t zone;	/* zone that this cluster comes from */
	uint16_t size1;		/* same as size of cluster: 2K/4K/9K/16K.
				 * hwsize[hwidx1] = size1.  No spare. */
	uint16_t size2;		/* hwsize[hwidx2] = size2.
				 * spare in cluster = size1 - size2. */
	int8_t hwidx1;		/* SGE bufsize idx for size1 */
	int8_t hwidx2;		/* SGE bufsize idx for size2 */
	uint8_t type;		/* EXT_xxx type of the cluster */
};

enum {
	NUM_MEMWIN = 3,

	MEMWIN0_APERTURE = 2048,
	MEMWIN0_BASE     = 0x1b800,

	MEMWIN1_APERTURE = 32768,
	MEMWIN1_BASE     = 0x28000,

	MEMWIN2_APERTURE_T4 = 65536,
	MEMWIN2_BASE_T4     = 0x30000,

	MEMWIN2_APERTURE_T5 = 128 * 1024,
	MEMWIN2_BASE_T5     = 0x60000,
};

struct memwin {
	struct rwlock mw_lock __aligned(CACHE_LINE_SIZE);
	uint32_t mw_base;	/* constant after setup_memwin */
	uint32_t mw_aperture;	/* ditto */
	uint32_t mw_curpos;	/* protected by mw_lock */
};

enum {
	FL_STARVING	= (1 << 0), /* on the adapter's list of starving fl's */
	FL_DOOMED	= (1 << 1), /* about to be destroyed */
	FL_BUF_PACKING	= (1 << 2), /* buffer packing enabled */
	FL_BUF_RESUME	= (1 << 3), /* resume from the middle of the frame */
};

#define FL_RUNNING_LOW(fl) \
    (IDXDIFF(fl->dbidx * 8, fl->cidx, fl->sidx * 8) <= fl->lowat)
#define FL_NOT_RUNNING_LOW(fl) \
    (IDXDIFF(fl->dbidx * 8, fl->cidx, fl->sidx * 8) >= 2 * fl->lowat)

struct sge_fl {
	struct mtx fl_lock;
	__be64 *desc;		/* KVA of descriptor ring, ptr to addresses */
	struct fl_sdesc *sdesc;	/* KVA of software descriptor ring */
	uint16_t zidx;		/* refill zone idx */
	uint16_t safe_zidx;
	uint16_t lowat;		/* # of buffers <= this means fl needs help */
	int flags;
	uint16_t buf_boundary;

	/* The 16b idx all deal with hw descriptors */
	uint16_t dbidx;		/* hw pidx after last doorbell */
	uint16_t sidx;		/* index of status page */
	volatile uint16_t hw_cidx;

	/* The 32b idx are all buffer idx, not hardware descriptor idx */
	uint32_t cidx;		/* consumer index */
	uint32_t pidx;		/* producer index */

	uint32_t dbval;
	u_int rx_offset;	/* offset in fl buf (when buffer packing) */
	volatile uint32_t *udb;

	uint64_t cl_allocated;	/* # of clusters allocated */
	uint64_t cl_recycled;	/* # of clusters recycled */
	uint64_t cl_fast_recycled; /* # of clusters recycled (fast) */

	/* These 3 are valid when FL_BUF_RESUME is set, stale otherwise. */
	struct mbuf *m0;
	struct mbuf **pnext;
	u_int remaining;

	uint16_t qsize;		/* # of hw descriptors (status page included) */
	uint16_t cntxt_id;	/* SGE context id for the freelist */
	TAILQ_ENTRY(sge_fl) link; /* All starving freelists */
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	char lockname[16];
	bus_addr_t ba;		/* bus address of descriptor ring */
};

struct mp_ring;

struct txpkts {
	uint8_t wr_type;	/* type 0 or type 1 */
	uint8_t npkt;		/* # of packets in this work request */
	uint8_t len16;		/* # of 16B pieces used by this work request */
	uint8_t score;
	uint8_t max_npkt;	/* maximum number of packets allowed */
	uint16_t plen;		/* total payload (sum of all packets) */

	/* straight from fw_eth_tx_pkts_vm_wr. */
	__u8   ethmacdst[6];
	__u8   ethmacsrc[6];
	__be16 ethtype;
	__be16 vlantci;

	struct mbuf *mb[15];
};

/* txq: SGE egress queue + what's needed for Ethernet NIC */
struct sge_txq {
	struct sge_eq eq;	/* MUST be first */

	if_t ifp;		/* the interface this txq belongs to */
	struct mp_ring *r;	/* tx software ring */
	struct tx_sdesc *sdesc;	/* KVA of software descriptor ring */
	struct sglist *gl;
	__be32 cpl_ctrl0;	/* for convenience */
	int tc_idx;		/* traffic class */
	uint64_t last_tx;	/* cycle count when eth_tx was last called */
	struct txpkts txp;

	struct task tx_reclaim_task;
	/* stats for common events first */

	uint64_t txcsum;	/* # of times hardware assisted with checksum */
	uint64_t tso_wrs;	/* # of TSO work requests */
	uint64_t vlan_insertion;/* # of times VLAN tag was inserted */
	uint64_t imm_wrs;	/* # of work requests with immediate data */
	uint64_t sgl_wrs;	/* # of work requests with direct SGL */
	uint64_t txpkt_wrs;	/* # of txpkt work requests (not coalesced) */
	uint64_t txpkts0_wrs;	/* # of type0 coalesced tx work requests */
	uint64_t txpkts1_wrs;	/* # of type1 coalesced tx work requests */
	uint64_t txpkts0_pkts;	/* # of frames in type0 coalesced tx WRs */
	uint64_t txpkts1_pkts;	/* # of frames in type1 coalesced tx WRs */
	uint64_t txpkts_flush;	/* # of times txp had to be sent by tx_update */
	uint64_t raw_wrs;	/* # of raw work requests (alloc_wr_mbuf) */
	uint64_t vxlan_tso_wrs;	/* # of VXLAN TSO work requests */
	uint64_t vxlan_txcsum;

	uint64_t kern_tls_records;
	uint64_t kern_tls_short;
	uint64_t kern_tls_partial;
	uint64_t kern_tls_full;
	uint64_t kern_tls_octets;
	uint64_t kern_tls_waste;
	uint64_t kern_tls_options;
	uint64_t kern_tls_header;
	uint64_t kern_tls_fin;
	uint64_t kern_tls_fin_short;
	uint64_t kern_tls_cbc;
	uint64_t kern_tls_gcm;

	/* stats for not-that-common events */

	/* Optional scratch space for constructing work requests. */
	uint8_t ss[SGE_MAX_WR_LEN] __aligned(16);
} __aligned(CACHE_LINE_SIZE);

/* rxq: SGE ingress queue + SGE free list + miscellaneous items */
struct sge_rxq {
	struct sge_iq iq;	/* MUST be first */
	struct sge_fl fl;	/* MUST follow iq */

	if_t ifp;		/* the interface this rxq belongs to */
	struct lro_ctrl lro;	/* LRO state */

	/* stats for common events first */

	uint64_t rxcsum;	/* # of times hardware assisted with checksum */
	uint64_t vlan_extraction;/* # of times VLAN tag was extracted */
	uint64_t vxlan_rxcsum;

	/* stats for not-that-common events */

} __aligned(CACHE_LINE_SIZE);

static inline struct sge_rxq *
iq_to_rxq(struct sge_iq *iq)
{

	return (__containerof(iq, struct sge_rxq, iq));
}

/* ofld_rxq: SGE ingress queue + SGE free list + miscellaneous items */
struct sge_ofld_rxq {
	struct sge_iq iq;	/* MUST be first */
	struct sge_fl fl;	/* MUST follow iq */
	counter_u64_t rx_iscsi_ddp_setup_ok;
	counter_u64_t rx_iscsi_ddp_setup_error;
	uint64_t rx_iscsi_ddp_pdus;
	uint64_t rx_iscsi_ddp_octets;
	uint64_t rx_iscsi_fl_pdus;
	uint64_t rx_iscsi_fl_octets;
	uint64_t rx_iscsi_padding_errors;
	uint64_t rx_iscsi_header_digest_errors;
	uint64_t rx_iscsi_data_digest_errors;
	u_long	rx_toe_tls_records;
	u_long	rx_toe_tls_octets;
} __aligned(CACHE_LINE_SIZE);

static inline struct sge_ofld_rxq *
iq_to_ofld_rxq(struct sge_iq *iq)
{

	return (__containerof(iq, struct sge_ofld_rxq, iq));
}

struct wrqe {
	STAILQ_ENTRY(wrqe) link;
	struct sge_wrq *wrq;
	int wr_len;
	char wr[] __aligned(16);
};

struct wrq_cookie {
	TAILQ_ENTRY(wrq_cookie) link;
	int ndesc;
	int pidx;
};

/*
 * wrq: SGE egress queue that is given prebuilt work requests.  Control queues
 * are of this type.
 */
struct sge_wrq {
	struct sge_eq eq;	/* MUST be first */

	struct adapter *adapter;
	struct task wrq_tx_task;

	/* Tx desc reserved but WR not "committed" yet. */
	TAILQ_HEAD(wrq_incomplete_wrs , wrq_cookie) incomplete_wrs;

	/* List of WRs ready to go out as soon as descriptors are available. */
	STAILQ_HEAD(, wrqe) wr_list;
	u_int nwr_pending;
	u_int ndesc_needed;

	/* stats for common events first */

	uint64_t tx_wrs_direct;	/* # of WRs written directly to desc ring. */
	uint64_t tx_wrs_ss;	/* # of WRs copied from scratch space. */
	uint64_t tx_wrs_copied;	/* # of WRs queued and copied to desc ring. */

	/* stats for not-that-common events */

	/*
	 * Scratch space for work requests that wrap around after reaching the
	 * status page, and some information about the last WR that used it.
	 */
	uint16_t ss_pidx;
	uint16_t ss_len;
	uint8_t ss[SGE_MAX_WR_LEN];

} __aligned(CACHE_LINE_SIZE);

/* ofld_txq: SGE egress queue + miscellaneous items */
struct sge_ofld_txq {
	struct sge_wrq wrq;
	counter_u64_t tx_iscsi_pdus;
	counter_u64_t tx_iscsi_octets;
	counter_u64_t tx_iscsi_iso_wrs;
	counter_u64_t tx_toe_tls_records;
	counter_u64_t tx_toe_tls_octets;
} __aligned(CACHE_LINE_SIZE);

#define INVALID_NM_RXQ_CNTXT_ID ((uint16_t)(-1))
struct sge_nm_rxq {
	/* Items used by the driver rx ithread are in this cacheline. */
	volatile int nm_state __aligned(CACHE_LINE_SIZE);	/* NM_OFF, NM_ON, or NM_BUSY */
	u_int nid;		/* netmap ring # for this queue */
	struct vi_info *vi;

	struct iq_desc *iq_desc;
	uint16_t iq_abs_id;
	uint16_t iq_cntxt_id;
	uint16_t iq_cidx;
	uint16_t iq_sidx;
	uint8_t iq_gen;
	uint32_t fl_sidx;

	/* Items used by netmap rxsync are in this cacheline. */
	__be64  *fl_desc __aligned(CACHE_LINE_SIZE);
	uint16_t fl_cntxt_id;
	uint32_t fl_pidx;
	uint32_t fl_sidx2;	/* copy of fl_sidx */
	uint32_t fl_db_val;
	u_int fl_db_saved;
	u_int fl_db_threshold;	/* in descriptors */
	u_int fl_hwidx:4;

	/*
	 * fl_cidx is used by both the ithread and rxsync, the rest are not used
	 * in the rx fast path.
	 */
	uint32_t fl_cidx __aligned(CACHE_LINE_SIZE);

	bus_dma_tag_t iq_desc_tag;
	bus_dmamap_t iq_desc_map;
	bus_addr_t iq_ba;
	int intr_idx;

	bus_dma_tag_t fl_desc_tag;
	bus_dmamap_t fl_desc_map;
	bus_addr_t fl_ba;
};

#define INVALID_NM_TXQ_CNTXT_ID ((u_int)(-1))
struct sge_nm_txq {
	struct tx_desc *desc;
	uint16_t cidx;
	uint16_t pidx;
	uint16_t sidx;
	uint16_t equiqidx;	/* EQUIQ last requested at this pidx */
	uint16_t equeqidx;	/* EQUEQ last requested at this pidx */
	uint16_t dbidx;		/* pidx of the most recent doorbell */
	uint8_t doorbells;
	volatile uint32_t *udb;
	u_int udb_qid;
	u_int cntxt_id;
	__be32 cpl_ctrl0;	/* for convenience */
	__be32 op_pkd;		/* ditto */
	u_int nid;		/* netmap ring # for this queue */

	/* infrequently used items after this */

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;
	int iqidx;
} __aligned(CACHE_LINE_SIZE);

struct sge {
	int nrxq;	/* total # of Ethernet rx queues */
	int ntxq;	/* total # of Ethernet tx queues */
	int nofldrxq;	/* total # of TOE rx queues */
	int nofldtxq;	/* total # of TOE tx queues */
	int nnmrxq;	/* total # of netmap rx queues */
	int nnmtxq;	/* total # of netmap tx queues */
	int niq;	/* total # of ingress queues */
	int neq;	/* total # of egress queues */

	struct sge_iq fwq;	/* Firmware event queue */
	struct sge_wrq *ctrlq;	/* Control queues */
	struct sge_txq *txq;	/* NIC tx queues */
	struct sge_rxq *rxq;	/* NIC rx queues */
	struct sge_ofld_txq *ofld_txq;	/* TOE tx queues */
	struct sge_ofld_rxq *ofld_rxq;	/* TOE rx queues */
	struct sge_nm_txq *nm_txq;	/* netmap tx queues */
	struct sge_nm_rxq *nm_rxq;	/* netmap rx queues */

	uint16_t iq_start;	/* first cntxt_id */
	uint16_t iq_base;	/* first abs_id */
	int eq_start;		/* first cntxt_id */
	int eq_base;		/* first abs_id */
	int iqmap_sz;
	int eqmap_sz;
	struct sge_iq **iqmap;	/* iq->cntxt_id to iq mapping */
	struct sge_eq **eqmap;	/* eq->cntxt_id to eq mapping */

	int8_t safe_zidx;
	struct rx_buf_info rx_buf_info[SW_ZONE_SIZES];
};

struct devnames {
	const char *nexus_name;
	const char *ifnet_name;
	const char *vi_ifnet_name;
	const char *pf03_drv_name;
	const char *vf_nexus_name;
	const char *vf_ifnet_name;
};

struct clip_entry;

#define CNT_CAL_INFO 3
struct clock_sync {
	uint64_t hw_cur;
	uint64_t hw_prev;
	sbintime_t sbt_cur;
	sbintime_t sbt_prev;
	seqc_t gen;
};

struct adapter {
	SLIST_ENTRY(adapter) link;
	device_t dev;
	struct cdev *cdev;
	const struct devnames *names;

	/* PCIe register resources */
	int regs_rid;
	struct resource *regs_res;
	int msix_rid;
	struct resource *msix_res;
	bus_space_handle_t bh;
	bus_space_tag_t bt;
	bus_size_t mmio_len;
	int udbs_rid;
	struct resource *udbs_res;
	volatile uint8_t *udbs_base;

	unsigned int pf;
	unsigned int mbox;
	unsigned int vpd_busy;
	unsigned int vpd_flag;

	/* Interrupt information */
	int intr_type;
	int intr_count;
	struct irq {
		struct resource *res;
		int rid;
		void *tag;
		struct sge_rxq *rxq;
		struct sge_nm_rxq *nm_rxq;
	} __aligned(CACHE_LINE_SIZE) *irq;
	int sge_gts_reg;
	int sge_kdoorbell_reg;

	bus_dma_tag_t dmat;	/* Parent DMA tag */

	struct sge sge;
	int lro_timeout;
	int sc_do_rxcopy;

	int vxlan_port;
	u_int vxlan_refcount;
	int rawf_base;
	int nrawf;

	struct taskqueue *tq[MAX_NCHAN];	/* General purpose taskqueues */
	struct port_info *port[MAX_NPORTS];
	uint8_t chan_map[MAX_NCHAN];		/* channel -> port */

	CXGBE_LIST_HEAD(, clip_entry) *clip_table;
	TAILQ_HEAD(, clip_entry) clip_pending;	/* these need hw update. */
	u_long clip_mask;
	int clip_gen;
	struct timeout_task clip_task;

	void *tom_softc;	/* (struct tom_data *) */
	struct tom_tunables tt;
	struct t4_offload_policy *policy;
	struct rwlock policy_lock;

	void *iwarp_softc;	/* (struct c4iw_dev *) */
	struct iw_tunables iwt;
	void *iscsi_ulp_softc;	/* (struct cxgbei_data *) */
	struct l2t_data *l2t;	/* L2 table */
	struct smt_data *smt;	/* Source MAC Table */
	struct tid_info tids;
	vmem_t *key_map;
	struct tls_tunables tlst;

	uint8_t doorbells;
	int offload_map;	/* port_id's with IFCAP_TOE enabled */
	int bt_map;		/* tx_chan's with BASE-T */
	int active_ulds;	/* ULDs activated on this adapter */
	int flags;
	int debug_flags;
	int error_flags;	/* Used by error handler and live reset. */

	char ifp_lockname[16];
	struct mtx ifp_lock;
	if_t ifp;		/* tracer ifp */
	struct ifmedia media;
	int traceq;		/* iq used by all tracers, -1 if none */
	int tracer_valid;	/* bitmap of valid tracers */
	int tracer_enabled;	/* bitmap of enabled tracers */

	char fw_version[16];
	char tp_version[16];
	char er_version[16];
	char bs_version[16];
	char cfg_file[32];
	u_int cfcsum;
	struct adapter_params params;
	const struct chip_params *chip_params;
	struct t4_virt_res vres;

	uint16_t nbmcaps;
	uint16_t linkcaps;
	uint16_t switchcaps;
	uint16_t niccaps;
	uint16_t toecaps;
	uint16_t rdmacaps;
	uint16_t cryptocaps;
	uint16_t iscsicaps;
	uint16_t fcoecaps;

	struct sysctl_ctx_list ctx;
	struct sysctl_oid *ctrlq_oid;
	struct sysctl_oid *fwq_oid;

	struct mtx sc_lock;
	char lockname[16];

	/* Starving free lists */
	struct mtx sfl_lock;	/* same cache-line as sc_lock? but that's ok */
	TAILQ_HEAD(, sge_fl) sfl;
	struct callout sfl_callout;
	struct callout cal_callout;
	struct clock_sync cal_info[CNT_CAL_INFO];
	int cal_current;
	int cal_count;
	uint32_t cal_gen;

	/*
	 * Driver code that can run when the adapter is suspended must use this
	 * lock or a synchronized_op and check for HW_OFF_LIMITS before
	 * accessing hardware.
	 *
	 * XXX: could be changed to rwlock.  wlock in suspend/resume and for
	 * indirect register access, rlock everywhere else.
	 */
	struct mtx reg_lock;

	struct memwin memwin[NUM_MEMWIN];	/* memory windows */

	struct mtx tc_lock;
	struct task tc_task;

	struct task fatal_error_task;
	struct task reset_task;
	const void *reset_thread;
	int num_resets;
	int incarnation;

	const char *last_op;
	const void *last_op_thr;
	int last_op_flags;

	int swintr;
	int sensor_resets;

	struct callout ktls_tick;
};

#define ADAPTER_LOCK(sc)		mtx_lock(&(sc)->sc_lock)
#define ADAPTER_UNLOCK(sc)		mtx_unlock(&(sc)->sc_lock)
#define ADAPTER_LOCK_ASSERT_OWNED(sc)	mtx_assert(&(sc)->sc_lock, MA_OWNED)
#define ADAPTER_LOCK_ASSERT_NOTOWNED(sc) mtx_assert(&(sc)->sc_lock, MA_NOTOWNED)

#define ASSERT_SYNCHRONIZED_OP(sc)	\
    KASSERT(IS_BUSY(sc) && \
	(mtx_owned(&(sc)->sc_lock) || sc->last_op_thr == curthread), \
	("%s: operation not synchronized.", __func__))

#define PORT_LOCK(pi)			mtx_lock(&(pi)->pi_lock)
#define PORT_UNLOCK(pi)			mtx_unlock(&(pi)->pi_lock)
#define PORT_LOCK_ASSERT_OWNED(pi)	mtx_assert(&(pi)->pi_lock, MA_OWNED)
#define PORT_LOCK_ASSERT_NOTOWNED(pi)	mtx_assert(&(pi)->pi_lock, MA_NOTOWNED)

#define FL_LOCK(fl)			mtx_lock(&(fl)->fl_lock)
#define FL_TRYLOCK(fl)			mtx_trylock(&(fl)->fl_lock)
#define FL_UNLOCK(fl)			mtx_unlock(&(fl)->fl_lock)
#define FL_LOCK_ASSERT_OWNED(fl)	mtx_assert(&(fl)->fl_lock, MA_OWNED)
#define FL_LOCK_ASSERT_NOTOWNED(fl)	mtx_assert(&(fl)->fl_lock, MA_NOTOWNED)

#define RXQ_FL_LOCK(rxq)		FL_LOCK(&(rxq)->fl)
#define RXQ_FL_UNLOCK(rxq)		FL_UNLOCK(&(rxq)->fl)
#define RXQ_FL_LOCK_ASSERT_OWNED(rxq)	FL_LOCK_ASSERT_OWNED(&(rxq)->fl)
#define RXQ_FL_LOCK_ASSERT_NOTOWNED(rxq) FL_LOCK_ASSERT_NOTOWNED(&(rxq)->fl)

#define EQ_LOCK(eq)			mtx_lock(&(eq)->eq_lock)
#define EQ_TRYLOCK(eq)			mtx_trylock(&(eq)->eq_lock)
#define EQ_UNLOCK(eq)			mtx_unlock(&(eq)->eq_lock)
#define EQ_LOCK_ASSERT_OWNED(eq)	mtx_assert(&(eq)->eq_lock, MA_OWNED)
#define EQ_LOCK_ASSERT_NOTOWNED(eq)	mtx_assert(&(eq)->eq_lock, MA_NOTOWNED)

#define TXQ_LOCK(txq)			EQ_LOCK(&(txq)->eq)
#define TXQ_TRYLOCK(txq)		EQ_TRYLOCK(&(txq)->eq)
#define TXQ_UNLOCK(txq)			EQ_UNLOCK(&(txq)->eq)
#define TXQ_LOCK_ASSERT_OWNED(txq)	EQ_LOCK_ASSERT_OWNED(&(txq)->eq)
#define TXQ_LOCK_ASSERT_NOTOWNED(txq)	EQ_LOCK_ASSERT_NOTOWNED(&(txq)->eq)

#define for_each_txq(vi, iter, q) \
	for (q = &vi->adapter->sge.txq[vi->first_txq], iter = 0; \
	    iter < vi->ntxq; ++iter, ++q)
#define for_each_rxq(vi, iter, q) \
	for (q = &vi->adapter->sge.rxq[vi->first_rxq], iter = 0; \
	    iter < vi->nrxq; ++iter, ++q)
#define for_each_ofld_txq(vi, iter, q) \
	for (q = &vi->adapter->sge.ofld_txq[vi->first_ofld_txq], iter = 0; \
	    iter < vi->nofldtxq; ++iter, ++q)
#define for_each_ofld_rxq(vi, iter, q) \
	for (q = &vi->adapter->sge.ofld_rxq[vi->first_ofld_rxq], iter = 0; \
	    iter < vi->nofldrxq; ++iter, ++q)
#define for_each_nm_txq(vi, iter, q) \
	for (q = &vi->adapter->sge.nm_txq[vi->first_nm_txq], iter = 0; \
	    iter < vi->nnmtxq; ++iter, ++q)
#define for_each_nm_rxq(vi, iter, q) \
	for (q = &vi->adapter->sge.nm_rxq[vi->first_nm_rxq], iter = 0; \
	    iter < vi->nnmrxq; ++iter, ++q)
#define for_each_vi(_pi, _iter, _vi) \
	for ((_vi) = (_pi)->vi, (_iter) = 0; (_iter) < (_pi)->nvi; \
	     ++(_iter), ++(_vi))

#define IDXINCR(idx, incr, wrap) do { \
	idx = wrap - idx > incr ? idx + incr : incr - (wrap - idx); \
} while (0)
#define IDXDIFF(head, tail, wrap) \
	((head) >= (tail) ? (head) - (tail) : (wrap) - (tail) + (head))

/* One for errors, one for firmware events */
#define T4_EXTRA_INTR 2

/* One for firmware events */
#define T4VF_EXTRA_INTR 1

static inline int
forwarding_intr_to_fwq(struct adapter *sc)
{

	return (sc->intr_count == 1);
}

/* Works reliably inside a sync_op or with reg_lock held. */
static inline bool
hw_off_limits(struct adapter *sc)
{
	int off_limits = atomic_load_int(&sc->error_flags) & HW_OFF_LIMITS;

	return (__predict_false(off_limits != 0));
}

static inline int
mbuf_nsegs(struct mbuf *m)
{
	M_ASSERTPKTHDR(m);
	KASSERT(m->m_pkthdr.inner_l5hlen > 0,
	    ("%s: mbuf %p missing information on # of segments.", __func__, m));

	return (m->m_pkthdr.inner_l5hlen);
}

static inline void
set_mbuf_nsegs(struct mbuf *m, uint8_t nsegs)
{
	M_ASSERTPKTHDR(m);
	m->m_pkthdr.inner_l5hlen = nsegs;
}

/* Internal mbuf flags stored in PH_loc.eight[1]. */
#define	MC_NOMAP		0x01
#define	MC_RAW_WR		0x02
#define	MC_TLS			0x04

static inline int
mbuf_cflags(struct mbuf *m)
{
	M_ASSERTPKTHDR(m);
	return (m->m_pkthdr.PH_loc.eight[4]);
}

static inline void
set_mbuf_cflags(struct mbuf *m, uint8_t flags)
{
	M_ASSERTPKTHDR(m);
	m->m_pkthdr.PH_loc.eight[4] = flags;
}

static inline int
mbuf_len16(struct mbuf *m)
{
	int n;

	M_ASSERTPKTHDR(m);
	n = m->m_pkthdr.PH_loc.eight[0];
	if (!(mbuf_cflags(m) & MC_TLS))
		MPASS(n > 0 && n <= SGE_MAX_WR_LEN / 16);

	return (n);
}

static inline void
set_mbuf_len16(struct mbuf *m, uint8_t len16)
{
	M_ASSERTPKTHDR(m);
	if (!(mbuf_cflags(m) & MC_TLS))
		MPASS(len16 > 0 && len16 <= SGE_MAX_WR_LEN / 16);
	m->m_pkthdr.PH_loc.eight[0] = len16;
}

static inline uint32_t
t4_read_reg(struct adapter *sc, uint32_t reg)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	return bus_space_read_4(sc->bt, sc->bh, reg);
}

static inline void
t4_write_reg(struct adapter *sc, uint32_t reg, uint32_t val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	bus_space_write_4(sc->bt, sc->bh, reg, val);
}

static inline uint64_t
t4_read_reg64(struct adapter *sc, uint32_t reg)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
#ifdef __LP64__
	return bus_space_read_8(sc->bt, sc->bh, reg);
#else
	return (uint64_t)bus_space_read_4(sc->bt, sc->bh, reg) +
	    ((uint64_t)bus_space_read_4(sc->bt, sc->bh, reg + 4) << 32);

#endif
}

static inline void
t4_write_reg64(struct adapter *sc, uint32_t reg, uint64_t val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
#ifdef __LP64__
	bus_space_write_8(sc->bt, sc->bh, reg, val);
#else
	bus_space_write_4(sc->bt, sc->bh, reg, val);
	bus_space_write_4(sc->bt, sc->bh, reg + 4, val>> 32);
#endif
}

static inline void
t4_os_pci_read_cfg1(struct adapter *sc, int reg, uint8_t *val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	*val = pci_read_config(sc->dev, reg, 1);
}

static inline void
t4_os_pci_write_cfg1(struct adapter *sc, int reg, uint8_t val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	pci_write_config(sc->dev, reg, val, 1);
}

static inline void
t4_os_pci_read_cfg2(struct adapter *sc, int reg, uint16_t *val)
{

	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	*val = pci_read_config(sc->dev, reg, 2);
}

static inline void
t4_os_pci_write_cfg2(struct adapter *sc, int reg, uint16_t val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	pci_write_config(sc->dev, reg, val, 2);
}

static inline void
t4_os_pci_read_cfg4(struct adapter *sc, int reg, uint32_t *val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	*val = pci_read_config(sc->dev, reg, 4);
}

static inline void
t4_os_pci_write_cfg4(struct adapter *sc, int reg, uint32_t val)
{
	if (hw_off_limits(sc))
		MPASS(curthread == sc->reset_thread);
	pci_write_config(sc->dev, reg, val, 4);
}

static inline struct port_info *
adap2pinfo(struct adapter *sc, int idx)
{

	return (sc->port[idx]);
}

static inline void
t4_os_set_hw_addr(struct port_info *pi, uint8_t hw_addr[])
{

	bcopy(hw_addr, pi->vi[0].hw_addr, ETHER_ADDR_LEN);
}

static inline int
tx_resume_threshold(struct sge_eq *eq)
{

	/* not quite the same as qsize / 4, but this will do. */
	return (eq->sidx / 4);
}

static inline int
t4_use_ldst(struct adapter *sc)
{

#ifdef notyet
	return (sc->flags & FW_OK || !sc->use_bd);
#else
	return (0);
#endif
}

static inline void
CH_DUMP_MBOX(struct adapter *sc, int mbox, const int reg,
    const char *msg, const __be64 *const p, const bool err)
{

	if (!(sc->debug_flags & DF_DUMP_MBOX) && !err)
		return;
	if (p != NULL) {
		log(err ? LOG_ERR : LOG_DEBUG,
		    "%s: mbox %u %s %016llx %016llx %016llx %016llx "
		    "%016llx %016llx %016llx %016llx\n",
		    device_get_nameunit(sc->dev), mbox, msg,
		    (long long)be64_to_cpu(p[0]), (long long)be64_to_cpu(p[1]),
		    (long long)be64_to_cpu(p[2]), (long long)be64_to_cpu(p[3]),
		    (long long)be64_to_cpu(p[4]), (long long)be64_to_cpu(p[5]),
		    (long long)be64_to_cpu(p[6]), (long long)be64_to_cpu(p[7]));
	} else {
		log(err ? LOG_ERR : LOG_DEBUG,
		    "%s: mbox %u %s %016llx %016llx %016llx %016llx "
		    "%016llx %016llx %016llx %016llx\n",
		    device_get_nameunit(sc->dev), mbox, msg,
		    (long long)t4_read_reg64(sc, reg),
		    (long long)t4_read_reg64(sc, reg + 8),
		    (long long)t4_read_reg64(sc, reg + 16),
		    (long long)t4_read_reg64(sc, reg + 24),
		    (long long)t4_read_reg64(sc, reg + 32),
		    (long long)t4_read_reg64(sc, reg + 40),
		    (long long)t4_read_reg64(sc, reg + 48),
		    (long long)t4_read_reg64(sc, reg + 56));
	}
}

/* t4_main.c */
extern int t4_ntxq;
extern int t4_nrxq;
extern int t4_intr_types;
extern int t4_tmr_idx;
extern int t4_pktc_idx;
extern unsigned int t4_qsize_rxq;
extern unsigned int t4_qsize_txq;
extern device_method_t cxgbe_methods[];

int t4_os_find_pci_capability(struct adapter *, int);
int t4_os_pci_save_state(struct adapter *);
int t4_os_pci_restore_state(struct adapter *);
void t4_os_portmod_changed(struct port_info *);
void t4_os_link_changed(struct port_info *);
void t4_iterate(void (*)(struct adapter *, void *), void *);
void t4_init_devnames(struct adapter *);
void t4_add_adapter(struct adapter *);
int t4_detach_common(device_t);
int t4_map_bars_0_and_4(struct adapter *);
int t4_map_bar_2(struct adapter *);
int t4_setup_intr_handlers(struct adapter *);
void t4_sysctls(struct adapter *);
int begin_synchronized_op(struct adapter *, struct vi_info *, int, char *);
void doom_vi(struct adapter *, struct vi_info *);
void end_synchronized_op(struct adapter *, int);
int update_mac_settings(if_t, int);
int adapter_init(struct adapter *);
int vi_init(struct vi_info *);
void vi_sysctls(struct vi_info *);
int rw_via_memwin(struct adapter *, int, uint32_t, uint32_t *, int, int);
int alloc_atid(struct adapter *, void *);
void *lookup_atid(struct adapter *, int);
void free_atid(struct adapter *, int);
void release_tid(struct adapter *, int, struct sge_wrq *);
int cxgbe_media_change(if_t);
void cxgbe_media_status(if_t, struct ifmediareq *);
void t4_os_cim_err(struct adapter *);

#ifdef KERN_TLS
/* t6_kern_tls.c */
int t6_tls_tag_alloc(if_t, union if_snd_tag_alloc_params *,
    struct m_snd_tag **);
void t6_ktls_modload(void);
void t6_ktls_modunload(void);
int t6_ktls_try(if_t, struct socket *, struct ktls_session *);
int t6_ktls_parse_pkt(struct mbuf *);
int t6_ktls_write_wr(struct sge_txq *, void *, struct mbuf *, u_int);
#endif

/* t4_keyctx.c */
struct auth_hash;
union authctx;
#ifdef KERN_TLS
struct ktls_session;
struct tls_key_req;
struct tls_keyctx;
#endif

void t4_aes_getdeckey(void *, const void *, unsigned int);
void t4_copy_partial_hash(int, union authctx *, void *);
void t4_init_gmac_hash(const char *, int, char *);
void t4_init_hmac_digest(const struct auth_hash *, u_int, const char *, int,
    char *);
#ifdef KERN_TLS
u_int t4_tls_key_info_size(const struct ktls_session *);
int t4_tls_proto_ver(const struct ktls_session *);
int t4_tls_cipher_mode(const struct ktls_session *);
int t4_tls_auth_mode(const struct ktls_session *);
int t4_tls_hmac_ctrl(const struct ktls_session *);
void t4_tls_key_ctx(const struct ktls_session *, int, struct tls_keyctx *);
int t4_alloc_tls_keyid(struct adapter *);
void t4_free_tls_keyid(struct adapter *, int);
void t4_write_tlskey_wr(const struct ktls_session *, int, int, int, int,
    struct tls_key_req *);
#endif

#ifdef DEV_NETMAP
/* t4_netmap.c */
struct sge_nm_rxq;
void cxgbe_nm_attach(struct vi_info *);
void cxgbe_nm_detach(struct vi_info *);
void service_nm_rxq(struct sge_nm_rxq *);
int alloc_nm_rxq(struct vi_info *, struct sge_nm_rxq *, int, int);
int free_nm_rxq(struct vi_info *, struct sge_nm_rxq *);
int alloc_nm_txq(struct vi_info *, struct sge_nm_txq *, int, int);
int free_nm_txq(struct vi_info *, struct sge_nm_txq *);
#endif

/* t4_sge.c */
void t4_sge_modload(void);
void t4_sge_modunload(void);
uint64_t t4_sge_extfree_refs(void);
void t4_tweak_chip_settings(struct adapter *);
int t4_verify_chip_settings(struct adapter *);
void t4_init_rx_buf_info(struct adapter *);
int t4_create_dma_tag(struct adapter *);
void t4_sge_sysctls(struct adapter *, struct sysctl_ctx_list *,
    struct sysctl_oid_list *);
int t4_destroy_dma_tag(struct adapter *);
int alloc_ring(struct adapter *, size_t, bus_dma_tag_t *, bus_dmamap_t *,
    bus_addr_t *, void **);
int free_ring(struct adapter *, bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
    void *);
void free_fl_buffers(struct adapter *, struct sge_fl *);
int t4_setup_adapter_queues(struct adapter *);
int t4_teardown_adapter_queues(struct adapter *);
int t4_setup_vi_queues(struct vi_info *);
int t4_teardown_vi_queues(struct vi_info *);
void t4_intr_all(void *);
void t4_intr(void *);
#ifdef DEV_NETMAP
void t4_nm_intr(void *);
void t4_vi_intr(void *);
#endif
void t4_intr_err(void *);
void t4_intr_evt(void *);
void t4_wrq_tx_locked(struct adapter *, struct sge_wrq *, struct wrqe *);
void t4_update_fl_bufsize(if_t);
struct mbuf *alloc_wr_mbuf(int, int);
int parse_pkt(struct mbuf **, bool);
void *start_wrq_wr(struct sge_wrq *, int, struct wrq_cookie *);
void commit_wrq_wr(struct sge_wrq *, void *, struct wrq_cookie *);
int t4_sge_set_conm_context(struct adapter *, int, int, int);
void t4_register_an_handler(an_handler_t);
void t4_register_fw_msg_handler(int, fw_msg_handler_t);
void t4_register_cpl_handler(int, cpl_handler_t);
void t4_register_shared_cpl_handler(int, cpl_handler_t, int);
#ifdef RATELIMIT
void send_etid_flush_wr(struct cxgbe_rate_tag *);
#endif

/* t4_tracer.c */
struct t4_tracer;
void t4_tracer_modload(void);
void t4_tracer_modunload(void);
void t4_tracer_port_detach(struct adapter *);
int t4_get_tracer(struct adapter *, struct t4_tracer *);
int t4_set_tracer(struct adapter *, struct t4_tracer *);
int t4_trace_pkt(struct sge_iq *, const struct rss_header *, struct mbuf *);
int t5_trace_pkt(struct sge_iq *, const struct rss_header *, struct mbuf *);

/* t4_sched.c */
int t4_set_sched_class(struct adapter *, struct t4_sched_params *);
int t4_set_sched_queue(struct adapter *, struct t4_sched_queue *);
int t4_init_tx_sched(struct adapter *);
int t4_free_tx_sched(struct adapter *);
void t4_update_tx_sched(struct adapter *);
int t4_reserve_cl_rl_kbps(struct adapter *, int, u_int, int *);
void t4_release_cl_rl(struct adapter *, int, int);
int sysctl_tc(SYSCTL_HANDLER_ARGS);
int sysctl_tc_params(SYSCTL_HANDLER_ARGS);
#ifdef RATELIMIT
void t4_init_etid_table(struct adapter *);
void t4_free_etid_table(struct adapter *);
struct cxgbe_rate_tag *lookup_etid(struct adapter *, int);
int cxgbe_rate_tag_alloc(if_t, union if_snd_tag_alloc_params *,
    struct m_snd_tag **);
void cxgbe_rate_tag_free_locked(struct cxgbe_rate_tag *);
void cxgbe_ratelimit_query(if_t, struct if_ratelimit_query_results *);
#endif

/* t4_filter.c */
int get_filter_mode(struct adapter *, uint32_t *);
int set_filter_mode(struct adapter *, uint32_t);
int set_filter_mask(struct adapter *, uint32_t);
int get_filter(struct adapter *, struct t4_filter *);
int set_filter(struct adapter *, struct t4_filter *);
int del_filter(struct adapter *, struct t4_filter *);
int t4_filter_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
int t4_hashfilter_ao_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
int t4_hashfilter_tcb_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
int t4_del_hashfilter_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
void free_hftid_hash(struct tid_info *);

static inline struct wrqe *
alloc_wrqe(int wr_len, struct sge_wrq *wrq)
{
	int len = offsetof(struct wrqe, wr) + wr_len;
	struct wrqe *wr;

	wr = malloc(len, M_CXGBE, M_NOWAIT);
	if (__predict_false(wr == NULL))
		return (NULL);
	wr->wr_len = wr_len;
	wr->wrq = wrq;
	return (wr);
}

static inline void *
wrtod(struct wrqe *wr)
{
	return (&wr->wr[0]);
}

static inline void
free_wrqe(struct wrqe *wr)
{
	free(wr, M_CXGBE);
}

static inline void
t4_wrq_tx(struct adapter *sc, struct wrqe *wr)
{
	struct sge_wrq *wrq = wr->wrq;

	TXQ_LOCK(wrq);
	t4_wrq_tx_locked(sc, wrq, wr);
	TXQ_UNLOCK(wrq);
}

static inline int
read_via_memwin(struct adapter *sc, int idx, uint32_t addr, uint32_t *val,
    int len)
{

	return (rw_via_memwin(sc, idx, addr, val, len, 0));
}

static inline int
write_via_memwin(struct adapter *sc, int idx, uint32_t addr,
    const uint32_t *val, int len)
{

	return (rw_via_memwin(sc, idx, addr, (void *)(uintptr_t)val, len, 1));
}

/* Number of len16 -> number of descriptors */
static inline int
tx_len16_to_desc(int len16)
{

	return (howmany(len16, EQ_ESIZE / 16));
}
#endif
