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
#include <sys/malloc.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <machine/bus.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/tcp_lro.h>

#include "offload.h"
#include "firmware/t4fw_interface.h"

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
	FW_IQ_QSIZE = 256,
	FW_IQ_ESIZE = 64,	/* At least 64 mandated by the firmware spec */

	RX_IQ_QSIZE = 1024,
	RX_IQ_ESIZE = 64,	/* At least 64 so CPL_RX_PKT will fit */

	EQ_ESIZE = 64,		/* All egress queues use this entry size */

	RX_FL_ESIZE = EQ_ESIZE,	/* 8 64bit addresses */
#if MJUMPAGESIZE != MCLBYTES
	FL_BUF_SIZES = 4,	/* cluster, jumbop, jumbo9k, jumbo16k */
#else
	FL_BUF_SIZES = 3,	/* cluster, jumbo9k, jumbo16k */
#endif
	OFLD_BUF_SIZE = MJUM16BYTES,	/* size of fl buffer for TOE rxq */

	CTRL_EQ_QSIZE = 128,

	TX_EQ_QSIZE = 1024,
	TX_SGL_SEGS = 36,
	TX_WR_FLITS = SGE_MAX_WR_LEN / 8
};

enum {
	/* adapter intr_type */
	INTR_INTX	= (1 << 0),
	INTR_MSI 	= (1 << 1),
	INTR_MSIX	= (1 << 2)
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
	INTR_DIRECT	= (1 << 2),	/* direct interrupts for everything */
	MASTER_PF	= (1 << 3),
	ADAP_SYSCTL_CTX	= (1 << 4),
	TOM_INIT_DONE	= (1 << 5),

	CXGBE_BUSY	= (1 << 9),

	/* port flags */
	DOOMED		= (1 << 0),
	PORT_INIT_DONE	= (1 << 1),
	PORT_SYSCTL_CTX	= (1 << 2),
};

#define IS_DOOMED(pi)	((pi)->flags & DOOMED)
#define SET_DOOMED(pi)	do {(pi)->flags |= DOOMED;} while (0)
#define IS_BUSY(sc)	((sc)->flags & CXGBE_BUSY)
#define SET_BUSY(sc)	do {(sc)->flags |= CXGBE_BUSY;} while (0)
#define CLR_BUSY(sc)	do {(sc)->flags &= ~CXGBE_BUSY;} while (0)

struct port_info {
	device_t dev;
	struct adapter *adapter;

	struct ifnet *ifp;
	struct ifmedia media;

	struct mtx pi_lock;
	char lockname[16];
	unsigned long flags;
	int if_flags;

	uint16_t viid;
	int16_t  xact_addr_filt;/* index of exact MAC address filter */
	uint16_t rss_size;	/* size of VI's RSS table slice */
	uint8_t  lport;		/* associated offload logical port */
	int8_t   mdio_addr;
	uint8_t  port_type;
	uint8_t  mod_type;
	uint8_t  port_id;
	uint8_t  tx_chan;

	/* These need to be int as they are used in sysctl */
	int ntxq;	/* # of tx queues */
	int first_txq;	/* index of first tx queue */
	int nrxq;	/* # of rx queues */
	int first_rxq;	/* index of first rx queue */
#ifdef TCP_OFFLOAD
	int nofldtxq;		/* # of offload tx queues */
	int first_ofld_txq;	/* index of first offload tx queue */
	int nofldrxq;		/* # of offload rx queues */
	int first_ofld_rxq;	/* index of first offload rx queue */
#endif
	int tmr_idx;
	int pktc_idx;
	int qsize_rxq;
	int qsize_txq;

	struct link_config link_cfg;
	struct port_stats stats;

	eventhandler_tag vlan_c;

	struct callout tick;
	struct sysctl_ctx_list ctx;	/* from ifconfig up to driver detach */

	uint8_t hw_addr[ETHER_ADDR_LEN]; /* factory MAC address, won't change */
};

struct fl_sdesc {
	struct mbuf *m;
	bus_dmamap_t map;
	caddr_t cl;
	uint8_t tag_idx;	/* the sc->fl_tag this map comes from */
#ifdef INVARIANTS
	__be64 ba_tag;
#endif
};

struct tx_desc {
	__be64 flit[8];
};

struct tx_map {
	struct mbuf *m;
	bus_dmamap_t map;
};

/* DMA maps used for tx */
struct tx_maps {
	struct tx_map *maps;
	uint32_t map_total;	/* # of DMA maps */
	uint32_t map_pidx;	/* next map to be used */
	uint32_t map_cidx;	/* reclaimed up to this index */
	uint32_t map_avail;	/* # of available maps */
};

struct tx_sdesc {
	uint8_t desc_used;	/* # of hardware descriptors used by the WR */
	uint8_t credits;	/* NIC txq: # of frames sent out in the WR */
};

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
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_addr_t ba;		/* bus address of descriptor ring */
	uint32_t flags;
	uint16_t abs_id;	/* absolute SGE id for the iq */
	int8_t   intr_pktc_idx;	/* packet count threshold index */
	int8_t   pad0;
	__be64  *desc;		/* KVA of descriptor ring */

	volatile int state;
	struct adapter *adapter;
	const __be64 *cdesc;	/* current descriptor */
	uint8_t  gen;		/* generation bit */
	uint8_t  intr_params;	/* interrupt holdoff parameters */
	uint8_t  intr_next;	/* XXX: holdoff for next interrupt */
	uint8_t  esize;		/* size (bytes) of each entry in the queue */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer index */
	uint16_t cntxt_id;	/* SGE context id for the iq */

	STAILQ_ENTRY(sge_iq) link;
};

enum {
	EQ_CTRL		= 1,
	EQ_ETH		= 2,
#ifdef TCP_OFFLOAD
	EQ_OFLD		= 3,
#endif

	/* eq flags */
	EQ_TYPEMASK	= 7,		/* 3 lsbits hold the type */
	EQ_ALLOCATED	= (1 << 3),	/* firmware resources allocated */
	EQ_DOOMED	= (1 << 4),	/* about to be destroyed */
	EQ_CRFLUSHED	= (1 << 5),	/* expecting an update from SGE */
	EQ_STALLED	= (1 << 6),	/* out of hw descriptors or dmamaps */
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
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	char lockname[16];
	struct mtx eq_lock;

	struct tx_desc *desc;	/* KVA of descriptor ring */
	bus_addr_t ba;		/* bus address of descriptor ring */
	struct sge_qstat *spg;	/* status page, for convenience */
	int doorbells;
	volatile uint32_t *udb;	/* KVA of doorbell (lies within BAR2) */
	u_int udb_qid;		/* relative qid within the doorbell page */
	uint16_t cap;		/* max # of desc, for convenience */
	uint16_t avail;		/* available descriptors, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer idx (desc idx) */
	uint16_t pidx;		/* producer idx (desc idx) */
	uint16_t pending;	/* # of descriptors used since last doorbell */
	uint16_t iqid;		/* iq that gets egr_update for the eq */
	uint8_t tx_chan;	/* tx channel used by the eq */
	struct task tx_task;
	struct callout tx_callout;

	/* stats */

	uint32_t egr_update;	/* # of SGE_EGR_UPDATE notifications for eq */
	uint32_t unstalled;	/* recovered from stall */
};

enum {
	FL_STARVING	= (1 << 0), /* on the adapter's list of starving fl's */
	FL_DOOMED	= (1 << 1), /* about to be destroyed */
};

#define FL_RUNNING_LOW(fl)	(fl->cap - fl->needed <= fl->lowat)
#define FL_NOT_RUNNING_LOW(fl)	(fl->cap - fl->needed >= 2 * fl->lowat)

struct sge_fl {
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_dma_tag_t tag[FL_BUF_SIZES];
	uint8_t tag_idx;
	struct mtx fl_lock;
	char lockname[16];
	int flags;

	__be64 *desc;		/* KVA of descriptor ring, ptr to addresses */
	bus_addr_t ba;		/* bus address of descriptor ring */
	struct fl_sdesc *sdesc;	/* KVA of software descriptor ring */
	uint32_t cap;		/* max # of buffers, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cntxt_id;	/* SGE context id for the freelist */
	uint32_t cidx;		/* consumer idx (buffer idx, NOT hw desc idx) */
	uint32_t pidx;		/* producer idx (buffer idx, NOT hw desc idx) */
	uint32_t needed;	/* # of buffers needed to fill up fl. */
	uint32_t lowat;		/* # of buffers <= this means fl needs help */
	uint32_t pending;	/* # of bufs allocated since last doorbell */
	unsigned int dmamap_failed;
	TAILQ_ENTRY(sge_fl) link; /* All starving freelists */
};

/* txq: SGE egress queue + what's needed for Ethernet NIC */
struct sge_txq {
	struct sge_eq eq;	/* MUST be first */

	struct ifnet *ifp;	/* the interface this txq belongs to */
	bus_dma_tag_t tx_tag;	/* tag for transmit buffers */
	struct buf_ring *br;	/* tx buffer ring */
	struct tx_sdesc *sdesc;	/* KVA of software descriptor ring */
	struct mbuf *m;		/* held up due to temporary resource shortage */

	struct tx_maps txmaps;

	/* stats for common events first */

	uint64_t txcsum;	/* # of times hardware assisted with checksum */
	uint64_t tso_wrs;	/* # of TSO work requests */
	uint64_t vlan_insertion;/* # of times VLAN tag was inserted */
	uint64_t imm_wrs;	/* # of work requests with immediate data */
	uint64_t sgl_wrs;	/* # of work requests with direct SGL */
	uint64_t txpkt_wrs;	/* # of txpkt work requests (not coalesced) */
	uint64_t txpkts_wrs;	/* # of coalesced tx work requests */
	uint64_t txpkts_pkts;	/* # of frames in coalesced tx work requests */

	/* stats for not-that-common events */

	uint32_t no_dmamap;	/* no DMA map to load the mbuf */
	uint32_t no_desc;	/* out of hardware descriptors */
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


#ifdef TCP_OFFLOAD
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
#endif

struct wrqe {
	STAILQ_ENTRY(wrqe) link;
	struct sge_wrq *wrq;
	int wr_len;
	uint64_t wr[] __aligned(16);
};

/*
 * wrq: SGE egress queue that is given prebuilt work requests.  Both the control
 * and offload tx queues are of this type.
 */
struct sge_wrq {
	struct sge_eq eq;	/* MUST be first */

	struct adapter *adapter;

	/* List of WRs held up due to lack of tx descriptors */
	STAILQ_HEAD(, wrqe) wr_list;

	/* stats for common events first */

	uint64_t tx_wrs;	/* # of tx work requests */

	/* stats for not-that-common events */

	uint32_t no_desc;	/* out of hardware descriptors */
} __aligned(CACHE_LINE_SIZE);

struct sge {
	int timer_val[SGE_NTIMERS];
	int counter_val[SGE_NCOUNTERS];
	int fl_starve_threshold;
	int s_qpp;

	int nrxq;	/* total # of Ethernet rx queues */
	int ntxq;	/* total # of Ethernet tx tx queues */
#ifdef TCP_OFFLOAD
	int nofldrxq;	/* total # of TOE rx queues */
	int nofldtxq;	/* total # of TOE tx queues */
#endif
	int niq;	/* total # of ingress queues */
	int neq;	/* total # of egress queues */

	struct sge_iq fwq;	/* Firmware event queue */
	struct sge_wrq mgmtq;	/* Management queue (control queue) */
	struct sge_wrq *ctrlq;	/* Control queues */
	struct sge_txq *txq;	/* NIC tx queues */
	struct sge_rxq *rxq;	/* NIC rx queues */
#ifdef TCP_OFFLOAD
	struct sge_wrq *ofld_txq;	/* TOE tx queues */
	struct sge_ofld_rxq *ofld_rxq;	/* TOE rx queues */
#endif

	uint16_t iq_start;
	int eq_start;
	struct sge_iq **iqmap;	/* iq->cntxt_id to iq mapping */
	struct sge_eq **eqmap;	/* eq->cntxt_id to eq mapping */
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

	struct taskqueue *tq[NCHAN];	/* taskqueues that flush data out */
	struct port_info *port[MAX_NPORTS];
	uint8_t chan_map[NCHAN];
	uint32_t filter_mode;

#ifdef TCP_OFFLOAD
	void *tom_softc;	/* (struct tom_data *) */
	struct tom_tunables tt;
#endif
	struct l2t_data *l2t;	/* L2 table */
	struct tid_info tids;

	int doorbells;
	int open_device_map;
#ifdef TCP_OFFLOAD
	int offload_map;
#endif
	int flags;

	char fw_version[32];
	char cfg_file[32];
	u_int cfcsum;
	struct adapter_params params;
	struct t4_virt_res vres;

	uint16_t linkcaps;
	uint16_t niccaps;
	uint16_t toecaps;
	uint16_t rdmacaps;
	uint16_t iscsicaps;
	uint16_t fcoecaps;

	struct sysctl_ctx_list ctx; /* from adapter_full_init to full_uninit */

	struct mtx sc_lock;
	char lockname[16];

	/* Starving free lists */
	struct mtx sfl_lock;	/* same cache-line as sc_lock? but that's ok */
	TAILQ_HEAD(, sge_fl) sfl;
	struct callout sfl_callout;

	an_handler_t an_handler __aligned(CACHE_LINE_SIZE);
	fw_msg_handler_t fw_msg_handler[5];	/* NUM_FW6_TYPES */
	cpl_handler_t cpl_handler[0xef];	/* NUM_CPL_CMDS */

#ifdef INVARIANTS
	const char *last_op;
	const void *last_op_thr;
#endif
};

#define ADAPTER_LOCK(sc)		mtx_lock(&(sc)->sc_lock)
#define ADAPTER_UNLOCK(sc)		mtx_unlock(&(sc)->sc_lock)
#define ADAPTER_LOCK_ASSERT_OWNED(sc)	mtx_assert(&(sc)->sc_lock, MA_OWNED)
#define ADAPTER_LOCK_ASSERT_NOTOWNED(sc) mtx_assert(&(sc)->sc_lock, MA_NOTOWNED)

/* XXX: not bulletproof, but much better than nothing */
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

#define for_each_txq(pi, iter, q) \
	for (q = &pi->adapter->sge.txq[pi->first_txq], iter = 0; \
	    iter < pi->ntxq; ++iter, ++q)
#define for_each_rxq(pi, iter, q) \
	for (q = &pi->adapter->sge.rxq[pi->first_rxq], iter = 0; \
	    iter < pi->nrxq; ++iter, ++q)
#define for_each_ofld_txq(pi, iter, q) \
	for (q = &pi->adapter->sge.ofld_txq[pi->first_ofld_txq], iter = 0; \
	    iter < pi->nofldtxq; ++iter, ++q)
#define for_each_ofld_rxq(pi, iter, q) \
	for (q = &pi->adapter->sge.ofld_rxq[pi->first_ofld_rxq], iter = 0; \
	    iter < pi->nofldrxq; ++iter, ++q)

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

	bcopy(hw_addr, sc->port[idx]->hw_addr, ETHER_ADDR_LEN);
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
tx_resume_threshold(struct sge_eq *eq)
{

	return (eq->qsize / 4);
}

/* t4_main.c */
void t4_tx_task(void *, int);
void t4_tx_callout(void *);
int t4_os_find_pci_capability(struct adapter *, int);
int t4_os_pci_save_state(struct adapter *);
int t4_os_pci_restore_state(struct adapter *);
void t4_os_portmod_changed(const struct adapter *, int);
void t4_os_link_changed(struct adapter *, int, int);
void t4_iterate(void (*)(struct adapter *, void *), void *);
int t4_register_cpl_handler(struct adapter *, int, cpl_handler_t);
int t4_register_an_handler(struct adapter *, an_handler_t);
int t4_register_fw_msg_handler(struct adapter *, int, fw_msg_handler_t);
int t4_filter_rpl(struct sge_iq *, const struct rss_header *, struct mbuf *);
int begin_synchronized_op(struct adapter *, struct port_info *, int, char *);
void end_synchronized_op(struct adapter *, int);

/* t4_sge.c */
void t4_sge_modload(void);
void t4_init_sge_cpl_handlers(struct adapter *);
void t4_tweak_chip_settings(struct adapter *);
int t4_read_chip_settings(struct adapter *);
int t4_create_dma_tag(struct adapter *);
int t4_destroy_dma_tag(struct adapter *);
int t4_setup_adapter_queues(struct adapter *);
int t4_teardown_adapter_queues(struct adapter *);
int t4_setup_port_queues(struct port_info *);
int t4_teardown_port_queues(struct port_info *);
int t4_alloc_tx_maps(struct tx_maps *, bus_dma_tag_t, int, int);
void t4_free_tx_maps(struct tx_maps *, bus_dma_tag_t);
void t4_intr_all(void *);
void t4_intr(void *);
void t4_intr_err(void *);
void t4_intr_evt(void *);
void t4_wrq_tx_locked(struct adapter *, struct sge_wrq *, struct wrqe *);
int t4_eth_tx(struct ifnet *, struct sge_txq *, struct mbuf *);
void t4_update_fl_bufsize(struct ifnet *);
int can_resume_tx(struct sge_eq *);

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
