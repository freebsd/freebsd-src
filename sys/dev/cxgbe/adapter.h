/*-
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
 * $FreeBSD$
 *
 */

#ifndef __T4_ADAPTER_H__
#define __T4_ADAPTER_H__

#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <vm/uma.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/bus.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
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

#if defined(__i386__) || defined(__amd64__)
static __inline void
prefetch(void *x)
{
	__asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
}
#else
#define prefetch(x)
#endif

#ifndef SYSCTL_ADD_UQUAD
#define SYSCTL_ADD_UQUAD SYSCTL_ADD_QUAD
#define sysctl_handle_64 sysctl_handle_quad
#define CTLTYPE_U64 CTLTYPE_QUAD
#endif

#if (__FreeBSD_version >= 900030) || \
    ((__FreeBSD_version >= 802507) && (__FreeBSD_version < 900000))
#define SBUF_DRAIN 1
#endif

#ifdef __amd64__
/* XXX: need systemwide bus_space_read_8/bus_space_write_8 */
static __inline uint64_t
t4_bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{
	KASSERT(tag == X86_BUS_SPACE_MEM,
	    ("%s: can only handle mem space", __func__));

	return (*(volatile uint64_t *)(handle + offset));
}

static __inline void
t4_bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, uint64_t value)
{
	KASSERT(tag == X86_BUS_SPACE_MEM,
	    ("%s: can only handle mem space", __func__));

	*(volatile uint64_t *)(bsh + offset) = value;
}
#else
static __inline uint64_t
t4_bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{
	return (uint64_t)bus_space_read_4(tag, handle, offset) +
	    ((uint64_t)bus_space_read_4(tag, handle, offset + 4) << 32);
}

static __inline void
t4_bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, uint64_t value)
{
	bus_space_write_4(tag, bsh, offset, value);
	bus_space_write_4(tag, bsh, offset + 4, value >> 32);
}
#endif

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
	CTRL_EQ_QSIZE = 128,
	TX_EQ_QSIZE = 1024,

#if MJUMPAGESIZE != MCLBYTES
	SW_ZONE_SIZES = 4,	/* cluster, jumbop, jumbo9k, jumbo16k */
#else
	SW_ZONE_SIZES = 3,	/* cluster, jumbo9k, jumbo16k */
#endif
	CL_METADATA_SIZE = CACHE_LINE_SIZE,

	SGE_MAX_WR_NDESC = SGE_MAX_WR_LEN / EQ_ESIZE, /* max WR size in desc */
	TX_SGL_SEGS = 39,
	TX_SGL_SEGS_TSO = 38,
	TX_WR_FLITS = SGE_MAX_WR_LEN / 8
};

enum {
	/* adapter intr_type */
	INTR_INTX	= (1 << 0),
	INTR_MSI 	= (1 << 1),
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
	/* adapter flags */
	FULL_INIT_DONE	= (1 << 0),
	FW_OK		= (1 << 1),
	/* INTR_DIRECT	= (1 << 2),	No longer used. */
	MASTER_PF	= (1 << 3),
	ADAP_SYSCTL_CTX	= (1 << 4),
	/* TOM_INIT_DONE= (1 << 5),	No longer used */
	BUF_PACKING_OK	= (1 << 6),

	CXGBE_BUSY	= (1 << 9),

	/* port flags */
	HAS_TRACEQ	= (1 << 3),

	/* VI flags */
	DOOMED		= (1 << 0),
	VI_INIT_DONE	= (1 << 1),
	VI_SYSCTL_CTX	= (1 << 2),
	INTR_RXQ	= (1 << 4),	/* All NIC rxq's take interrupts */
	INTR_OFLD_RXQ	= (1 << 5),	/* All TOE rxq's take interrupts */
	INTR_ALL	= (INTR_RXQ | INTR_OFLD_RXQ),
	VI_NETMAP	= (1 << 6),

	/* adapter debug_flags */
	DF_DUMP_MBOX	= (1 << 0),
};

#define IS_DOOMED(vi)	((vi)->flags & DOOMED)
#define SET_DOOMED(vi)	do {(vi)->flags |= DOOMED;} while (0)
#define IS_BUSY(sc)	((sc)->flags & CXGBE_BUSY)
#define SET_BUSY(sc)	do {(sc)->flags |= CXGBE_BUSY;} while (0)
#define CLR_BUSY(sc)	do {(sc)->flags &= ~CXGBE_BUSY;} while (0)

struct vi_info {
	device_t dev;
	struct port_info *pi;

	struct ifnet *ifp;
	struct ifmedia media;

	unsigned long flags;
	int if_flags;

	uint16_t *rss;
	uint16_t viid;
	int16_t  xact_addr_filt;/* index of exact MAC address filter */
	uint16_t rss_size;	/* size of VI's RSS table slice */
	uint16_t rss_base;	/* start of VI's RSS table slice */

	eventhandler_tag vlan_c;

	int nintr;
	int first_intr;

	/* These need to be int as they are used in sysctl */
	int ntxq;	/* # of tx queues */
	int first_txq;	/* index of first tx queue */
	int rsrv_noflowq; /* Reserve queue 0 for non-flowid packets */
	int nrxq;	/* # of rx queues */
	int first_rxq;	/* index of first rx queue */
	int nofldtxq;		/* # of offload tx queues */
	int first_ofld_txq;	/* index of first offload tx queue */
	int nofldrxq;		/* # of offload rx queues */
	int first_ofld_rxq;	/* index of first offload rx queue */
	int tmr_idx;
	int pktc_idx;
	int qsize_rxq;
	int qsize_txq;

	struct timeval last_refreshed;
	struct fw_vi_stats_vf stats;

	struct callout tick;
	struct sysctl_ctx_list ctx;	/* from ifconfig up to driver detach */

	uint8_t hw_addr[ETHER_ADDR_LEN]; /* factory MAC address, won't change */
};

enum {
	/* tx_sched_class flags */
	TX_SC_OK	= (1 << 0),	/* Set up in hardware, active. */
};

struct tx_sched_class {
	int refcount;
	int flags;
	struct t4_sched_class_params params;
};

struct port_info {
	device_t dev;
	struct adapter *adapter;

	struct vi_info *vi;
	int nvi;
	int up_vis;
	int uld_vis;

	struct tx_sched_class *tc;	/* traffic classes for this channel */

	struct mtx pi_lock;
	char lockname[16];
	unsigned long flags;

	uint8_t  lport;		/* associated offload logical port */
	int8_t   mdio_addr;
	uint8_t  port_type;
	uint8_t  mod_type;
	uint8_t  port_id;
	uint8_t  tx_chan;
	uint8_t  rx_chan_map;	/* rx MPS channel bitmap */

	int linkdnrc;
	struct link_config link_cfg;

	struct timeval last_refreshed;
 	struct port_stats stats;
	u_int tnl_cong_drops;
	u_int tx_parse_error;

	struct callout tick;
};

#define	IS_MAIN_VI(vi)		((vi) == &((vi)->pi->vi[0]))

/* Where the cluster came from, how it has been carved up. */
struct cluster_layout {
	int8_t zidx;
	int8_t hwidx;
	uint16_t region1;	/* mbufs laid out within this region */
				/* region2 is the DMA region */
	uint16_t region3;	/* cluster_metadata within this region */
};

struct cluster_metadata {
	u_int refcount;
	struct fl_sdesc *sd;	/* For debug only.  Could easily be stale */
};

struct fl_sdesc {
	caddr_t cl;
	uint16_t nmbuf;	/* # of driver originated mbufs with ref on cluster */
	struct cluster_layout cll;
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
	/* iq flags */
	IQ_ALLOCATED	= (1 << 0),	/* firmware resources allocated */
	IQ_HAS_FL	= (1 << 1),	/* iq associated with a freelist */
	IQ_INTR		= (1 << 2),	/* iq takes direct interrupt */
	IQ_LRO_ENABLED	= (1 << 3),	/* iq is an eth rxq with LRO enabled */

	/* iq state */
	IQS_DISABLED	= 0,
	IQS_BUSY	= 1,
	IQS_IDLE	= 2,
};

/*
 * Ingress Queue: T4 is producer, driver is consumer.
 */
struct sge_iq {
	uint32_t flags;
	volatile int state;
	struct adapter *adapter;
	struct iq_desc  *desc;	/* KVA of descriptor ring */
	int8_t   intr_pktc_idx;	/* packet count threshold index */
	uint8_t  gen;		/* generation bit */
	uint8_t  intr_params;	/* interrupt holdoff parameters */
	uint8_t  intr_next;	/* XXX: holdoff for next interrupt */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t sidx;		/* index of the entry with the status page */
	uint16_t cidx;		/* consumer index */
	uint16_t cntxt_id;	/* SGE context id for the iq */
	uint16_t abs_id;	/* absolute SGE id for the iq */

	STAILQ_ENTRY(sge_iq) link;

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;		/* bus address of descriptor ring */
};

enum {
	EQ_CTRL		= 1,
	EQ_ETH		= 2,
	EQ_OFLD		= 3,

	/* eq flags */
	EQ_TYPEMASK	= 0x3,		/* 2 lsbits hold the type (see above) */
	EQ_ALLOCATED	= (1 << 2),	/* firmware resources allocated */
	EQ_ENABLED	= (1 << 3),	/* open for business */
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
	struct mtx eq_lock;

	struct tx_desc *desc;	/* KVA of descriptor ring */
	uint16_t doorbells;
	volatile uint32_t *udb;	/* KVA of doorbell (lies within BAR2) */
	u_int udb_qid;		/* relative qid within the doorbell page */
	uint16_t sidx;		/* index of the entry with the status page */
	uint16_t cidx;		/* consumer idx (desc idx) */
	uint16_t pidx;		/* producer idx (desc idx) */
	uint16_t equeqidx;	/* EQUEQ last requested at this pidx */
	uint16_t dbidx;		/* pidx of the most recent doorbell */
	uint16_t iqid;		/* iq that gets egr_update for the eq */
	uint8_t tx_chan;	/* tx channel used by the eq */
	volatile u_int equiq;	/* EQUIQ outstanding */

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;		/* bus address of descriptor ring */
	char lockname[16];
};

struct sw_zone_info {
	uma_zone_t zone;	/* zone that this cluster comes from */
	int size;		/* size of cluster: 2K, 4K, 9K, 16K, etc. */
	int type;		/* EXT_xxx type of the cluster */
	int8_t head_hwidx;
	int8_t tail_hwidx;
};

struct hw_buf_info {
	int8_t zidx;		/* backpointer to zone; -ve means unused */
	int8_t next;		/* next hwidx for this zone; -1 means no more */
	int size;
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
	struct cluster_layout cll_def;	/* default refill zone, layout */
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

	uint64_t mbuf_allocated;/* # of mbuf allocated from zone_mbuf */
	uint64_t mbuf_inlined;	/* # of mbuf created within clusters */
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
	struct cluster_layout cll_alt;	/* alternate refill zone, layout */
};

struct mp_ring;

/* txq: SGE egress queue + what's needed for Ethernet NIC */
struct sge_txq {
	struct sge_eq eq;	/* MUST be first */

	struct ifnet *ifp;	/* the interface this txq belongs to */
	struct mp_ring *r;	/* tx software ring */
	struct tx_sdesc *sdesc;	/* KVA of software descriptor ring */
	struct sglist *gl;
	__be32 cpl_ctrl0;	/* for convenience */
	int tc_idx;		/* traffic class */

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

	/* stats for not-that-common events */
} __aligned(CACHE_LINE_SIZE);

/* rxq: SGE ingress queue + SGE free list + miscellaneous items */
struct sge_rxq {
	struct sge_iq iq;	/* MUST be first */
	struct sge_fl fl;	/* MUST follow iq */

	struct ifnet *ifp;	/* the interface this rxq belongs to */
#if defined(INET) || defined(INET6)
	struct lro_ctrl lro;	/* LRO state */
#endif

	/* stats for common events first */

	uint64_t rxcsum;	/* # of times hardware assisted with checksum */
	uint64_t vlan_extraction;/* # of times VLAN tag was extracted */

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
 * wrq: SGE egress queue that is given prebuilt work requests.  Both the control
 * and offload tx queues are of this type.
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


struct sge_nm_rxq {
	struct vi_info *vi;

	struct iq_desc *iq_desc;
	uint16_t iq_abs_id;
	uint16_t iq_cntxt_id;
	uint16_t iq_cidx;
	uint16_t iq_sidx;
	uint8_t iq_gen;

	__be64  *fl_desc;
	uint16_t fl_cntxt_id;
	uint32_t fl_cidx;
	uint32_t fl_pidx;
	uint32_t fl_sidx;
	uint32_t fl_db_val;
	u_int fl_hwidx:4;

	u_int nid;		/* netmap ring # for this queue */

	/* infrequently used items after this */

	bus_dma_tag_t iq_desc_tag;
	bus_dmamap_t iq_desc_map;
	bus_addr_t iq_ba;
	int intr_idx;

	bus_dma_tag_t fl_desc_tag;
	bus_dmamap_t fl_desc_map;
	bus_addr_t fl_ba;
} __aligned(CACHE_LINE_SIZE);

struct sge_nm_txq {
	struct tx_desc *desc;
	uint16_t cidx;
	uint16_t pidx;
	uint16_t sidx;
	uint16_t equiqidx;	/* EQUIQ last requested at this pidx */
	uint16_t equeqidx;	/* EQUEQ last requested at this pidx */
	uint16_t dbidx;		/* pidx of the most recent doorbell */
	uint16_t doorbells;
	volatile uint32_t *udb;
	u_int udb_qid;
	u_int cntxt_id;
	__be32 cpl_ctrl0;	/* for convenience */
	u_int nid;		/* netmap ring # for this queue */

	/* infrequently used items after this */

	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;
	int iqidx;
} __aligned(CACHE_LINE_SIZE);

struct sge {
	int nrxq;	/* total # of Ethernet rx queues */
	int ntxq;	/* total # of Ethernet tx tx queues */
	int nofldrxq;	/* total # of TOE rx queues */
	int nofldtxq;	/* total # of TOE tx queues */
	int nnmrxq;	/* total # of netmap rx queues */
	int nnmtxq;	/* total # of netmap tx queues */
	int niq;	/* total # of ingress queues */
	int neq;	/* total # of egress queues */

	struct sge_iq fwq;	/* Firmware event queue */
	struct sge_wrq mgmtq;	/* Management queue (control queue) */
	struct sge_wrq *ctrlq;	/* Control queues */
	struct sge_txq *txq;	/* NIC tx queues */
	struct sge_rxq *rxq;	/* NIC rx queues */
	struct sge_wrq *ofld_txq;	/* TOE tx queues */
	struct sge_ofld_rxq *ofld_rxq;	/* TOE rx queues */
	struct sge_nm_txq *nm_txq;	/* netmap tx queues */
	struct sge_nm_rxq *nm_rxq;	/* netmap rx queues */

	uint16_t iq_start;
	int eq_start;
	struct sge_iq **iqmap;	/* iq->cntxt_id to iq mapping */
	struct sge_eq **eqmap;	/* eq->cntxt_id to eq mapping */

	int8_t safe_hwidx1;	/* may not have room for metadata */
	int8_t safe_hwidx2;	/* with room for metadata and maybe more */
	struct sw_zone_info sw_zone_info[SW_ZONE_SIZES];
	struct hw_buf_info hw_buf_info[SGE_FLBUF_SIZES];
};

struct rss_header;
typedef int (*cpl_handler_t)(struct sge_iq *, const struct rss_header *,
    struct mbuf *);
typedef int (*an_handler_t)(struct sge_iq *, const struct rsp_ctrl *);
typedef int (*fw_msg_handler_t)(struct adapter *, const __be64 *);

struct adapter {
	SLIST_ENTRY(adapter) link;
	device_t dev;
	struct cdev *cdev;

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
	} *irq;

	bus_dma_tag_t dmat;	/* Parent DMA tag */

	struct sge sge;
	int lro_timeout;

	struct taskqueue *tq[MAX_NCHAN];	/* General purpose taskqueues */
	struct port_info *port[MAX_NPORTS];
	uint8_t chan_map[MAX_NCHAN];

	void *tom_softc;	/* (struct tom_data *) */
	struct tom_tunables tt;
	void *iwarp_softc;	/* (struct c4iw_dev *) */
	void *iscsi_ulp_softc;	/* (struct cxgbei_data *) */
	struct l2t_data *l2t;	/* L2 table */
	struct tid_info tids;

	uint16_t doorbells;
	int offload_map;	/* ports with IFCAP_TOE enabled */
	int active_ulds;	/* ULDs activated on this adapter */
	int flags;
	int debug_flags;

	char ifp_lockname[16];
	struct mtx ifp_lock;
	struct ifnet *ifp;	/* tracer ifp */
	struct ifmedia media;
	int traceq;		/* iq used by all tracers, -1 if none */
	int tracer_valid;	/* bitmap of valid tracers */
	int tracer_enabled;	/* bitmap of enabled tracers */

	char fw_version[16];
	char tp_version[16];
	char exprom_version[16];
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
	uint16_t tlscaps;
	uint16_t iscsicaps;
	uint16_t fcoecaps;

	struct sysctl_ctx_list ctx; /* from adapter_full_init to full_uninit */

	struct mtx sc_lock;
	char lockname[16];

	/* Starving free lists */
	struct mtx sfl_lock;	/* same cache-line as sc_lock? but that's ok */
	TAILQ_HEAD(, sge_fl) sfl;
	struct callout sfl_callout;

	struct mtx reg_lock;	/* for indirect register access */

	struct memwin memwin[NUM_MEMWIN];	/* memory windows */

	an_handler_t an_handler __aligned(CACHE_LINE_SIZE);
	fw_msg_handler_t fw_msg_handler[7];	/* NUM_FW6_TYPES */
	cpl_handler_t cpl_handler[0xef];	/* NUM_CPL_CMDS */

	const char *last_op;
	const void *last_op_thr;
	int last_op_flags;

	int sc_do_rxcopy;
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

#define CH_DUMP_MBOX(sc, mbox, data_reg) \
	do { \
		if (sc->debug_flags & DF_DUMP_MBOX) { \
			log(LOG_NOTICE, \
			    "%s mbox %u: %016llx %016llx %016llx %016llx " \
			    "%016llx %016llx %016llx %016llx\n", \
			    device_get_nameunit(sc->dev), mbox, \
			    (unsigned long long)t4_read_reg64(sc, data_reg), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 8), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 16), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 24), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 32), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 40), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 48), \
			    (unsigned long long)t4_read_reg64(sc, data_reg + 56)); \
		} \
	} while (0)

#define for_each_txq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.txq[vi->first_txq], iter = 0; \
	    iter < vi->ntxq; ++iter, ++q)
#define for_each_rxq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.rxq[vi->first_rxq], iter = 0; \
	    iter < vi->nrxq; ++iter, ++q)
#define for_each_ofld_txq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.ofld_txq[vi->first_ofld_txq], iter = 0; \
	    iter < vi->nofldtxq; ++iter, ++q)
#define for_each_ofld_rxq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.ofld_rxq[vi->first_ofld_rxq], iter = 0; \
	    iter < vi->nofldrxq; ++iter, ++q)
#define for_each_nm_txq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.nm_txq[vi->first_txq], iter = 0; \
	    iter < vi->ntxq; ++iter, ++q)
#define for_each_nm_rxq(vi, iter, q) \
	for (q = &vi->pi->adapter->sge.nm_rxq[vi->first_rxq], iter = 0; \
	    iter < vi->nrxq; ++iter, ++q)
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

static inline uint32_t
t4_read_reg(struct adapter *sc, uint32_t reg)
{

	return bus_space_read_4(sc->bt, sc->bh, reg);
}

static inline void
t4_write_reg(struct adapter *sc, uint32_t reg, uint32_t val)
{

	bus_space_write_4(sc->bt, sc->bh, reg, val);
}

static inline uint64_t
t4_read_reg64(struct adapter *sc, uint32_t reg)
{

	return t4_bus_space_read_8(sc->bt, sc->bh, reg);
}

static inline void
t4_write_reg64(struct adapter *sc, uint32_t reg, uint64_t val)
{

	t4_bus_space_write_8(sc->bt, sc->bh, reg, val);
}

static inline void
t4_os_pci_read_cfg1(struct adapter *sc, int reg, uint8_t *val)
{

	*val = pci_read_config(sc->dev, reg, 1);
}

static inline void
t4_os_pci_write_cfg1(struct adapter *sc, int reg, uint8_t val)
{

	pci_write_config(sc->dev, reg, val, 1);
}

static inline void
t4_os_pci_read_cfg2(struct adapter *sc, int reg, uint16_t *val)
{

	*val = pci_read_config(sc->dev, reg, 2);
}

static inline void
t4_os_pci_write_cfg2(struct adapter *sc, int reg, uint16_t val)
{

	pci_write_config(sc->dev, reg, val, 2);
}

static inline void
t4_os_pci_read_cfg4(struct adapter *sc, int reg, uint32_t *val)
{

	*val = pci_read_config(sc->dev, reg, 4);
}

static inline void
t4_os_pci_write_cfg4(struct adapter *sc, int reg, uint32_t val)
{

	pci_write_config(sc->dev, reg, val, 4);
}

static inline struct port_info *
adap2pinfo(struct adapter *sc, int idx)
{

	return (sc->port[idx]);
}

static inline void
t4_os_set_hw_addr(struct adapter *sc, int idx, uint8_t hw_addr[])
{

	bcopy(hw_addr, sc->port[idx]->vi[0].hw_addr, ETHER_ADDR_LEN);
}

static inline bool
is_10G_port(const struct port_info *pi)
{

	return ((pi->link_cfg.supported & FW_PORT_CAP_SPEED_10G) != 0);
}

static inline bool
is_40G_port(const struct port_info *pi)
{

	return ((pi->link_cfg.supported & FW_PORT_CAP_SPEED_40G) != 0);
}

static inline int
port_top_speed(const struct port_info *pi)
{

	if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_100G)
		return (100);
	if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_40G)
		return (40);
	if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_10G)
		return (10);
	if (pi->link_cfg.supported & FW_PORT_CAP_SPEED_1G)
		return (1);

	return (0);
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

/* t4_main.c */
int t4_os_find_pci_capability(struct adapter *, int);
int t4_os_pci_save_state(struct adapter *);
int t4_os_pci_restore_state(struct adapter *);
void t4_os_portmod_changed(const struct adapter *, int);
void t4_os_link_changed(struct adapter *, int, int, int);
void t4_iterate(void (*)(struct adapter *, void *), void *);
int t4_register_cpl_handler(struct adapter *, int, cpl_handler_t);
int t4_register_an_handler(struct adapter *, an_handler_t);
int t4_register_fw_msg_handler(struct adapter *, int, fw_msg_handler_t);
int t4_filter_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
int begin_synchronized_op(struct adapter *, struct vi_info *, int, char *);
void doom_vi(struct adapter *, struct vi_info *);
void end_synchronized_op(struct adapter *, int);
int update_mac_settings(struct ifnet *, int);
int adapter_full_init(struct adapter *);
int adapter_full_uninit(struct adapter *);
uint64_t cxgbe_get_counter(struct ifnet *, ift_counter);
int vi_full_init(struct vi_info *);
int vi_full_uninit(struct vi_info *);
void vi_sysctls(struct vi_info *);
void vi_tick(void *);

#ifdef DEV_NETMAP
/* t4_netmap.c */
int create_netmap_ifnet(struct port_info *);
int destroy_netmap_ifnet(struct port_info *);
void t4_nm_intr(void *);
#endif

/* t4_sge.c */
void t4_sge_modload(void);
void t4_sge_modunload(void);
uint64_t t4_sge_extfree_refs(void);
void t4_init_sge_cpl_handlers(struct adapter *);
void t4_tweak_chip_settings(struct adapter *);
int t4_read_chip_settings(struct adapter *);
int t4_create_dma_tag(struct adapter *);
void t4_sge_sysctls(struct adapter *, struct sysctl_ctx_list *,
    struct sysctl_oid_list *);
int t4_destroy_dma_tag(struct adapter *);
int t4_setup_adapter_queues(struct adapter *);
int t4_teardown_adapter_queues(struct adapter *);
int t4_setup_vi_queues(struct vi_info *);
int t4_teardown_vi_queues(struct vi_info *);
void t4_intr_all(void *);
void t4_intr(void *);
void t4_intr_err(void *);
void t4_intr_evt(void *);
void t4_wrq_tx_locked(struct adapter *, struct sge_wrq *, struct wrqe *);
void t4_update_fl_bufsize(struct ifnet *);
int parse_pkt(struct mbuf **);
void *start_wrq_wr(struct sge_wrq *, int, struct wrq_cookie *);
void commit_wrq_wr(struct sge_wrq *, void *, struct wrq_cookie *);
int tnl_cong(struct port_info *, int);

/* t4_tracer.c */
struct t4_tracer;
void t4_tracer_modload(void);
void t4_tracer_modunload(void);
void t4_tracer_port_detach(struct adapter *);
int t4_get_tracer(struct adapter *, struct t4_tracer *);
int t4_set_tracer(struct adapter *, struct t4_tracer *);
int t4_trace_pkt(struct sge_iq *, const struct rss_header *, struct mbuf *);
int t5_trace_pkt(struct sge_iq *, const struct rss_header *, struct mbuf *);

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

#endif
