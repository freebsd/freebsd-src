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
#include <netinet/tcp_lro.h>

#include "offload.h"
#include "common/t4fw_interface.h"

#define T4_FWNAME "t4fw"

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

#ifdef __amd64__
/* XXX: need systemwide bus_space_read_8/bus_space_write_8 */
static __inline uint64_t
t4_bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{
	KASSERT(tag == AMD64_BUS_SPACE_MEM,
	    ("%s: can only handle mem space", __func__));

	return (*(volatile uint64_t *)(handle + offset));
}

static __inline void
t4_bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, uint64_t value)
{
	KASSERT(tag == AMD64_BUS_SPACE_MEM,
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

	RX_FL_ESIZE = 64,	/* 8 64bit addresses */

#if MJUMPAGESIZE != MCLBYTES
	FL_BUF_SIZES = 4,	/* cluster, jumbop, jumbo9k, jumbo16k */
#else
	FL_BUF_SIZES = 3,	/* cluster, jumbo9k, jumbo16k */
#endif

	TX_EQ_QSIZE = 1024,
	TX_EQ_ESIZE = 64,
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
	/* adapter flags */
	FULL_INIT_DONE	= (1 << 0),
	FW_OK		= (1 << 1),
	INTR_FWD	= (1 << 2),

	CXGBE_BUSY	= (1 << 9),

	/* port flags */
	DOOMED		= (1 << 0),
	VI_ENABLED	= (1 << 1),
};

#define IS_DOOMED(pi)	(pi->flags & DOOMED)
#define SET_DOOMED(pi)	do {pi->flags |= DOOMED;} while (0)
#define IS_BUSY(sc)	(sc->flags & CXGBE_BUSY)
#define SET_BUSY(sc)	do {sc->flags |= CXGBE_BUSY;} while (0)
#define CLR_BUSY(sc)	do {sc->flags &= ~CXGBE_BUSY;} while (0)

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
	int tmr_idx;
	int pktc_idx;
	int qsize_rxq;
	int qsize_txq;

	struct link_config link_cfg;
	struct port_stats stats;

	struct taskqueue *tq;
	struct callout tick;
	struct sysctl_ctx_list ctx;	/* lives from ifconfig up to down */
	struct sysctl_oid *oid_rxq;
	struct sysctl_oid *oid_txq;

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

struct tx_sdesc {
	uint8_t desc_used;	/* # of hardware descriptors used by the WR */
	uint8_t map_used;	/* # of frames sent out in the WR */
};

typedef void (iq_intr_handler_t)(void *);

enum {
	/* iq flags */
	IQ_ALLOCATED	= (1 << 1),	/* firmware resources allocated */
	IQ_STARTED	= (1 << 2),	/* started */

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
	char lockname[16];
	uint32_t flags;
	uint16_t abs_id;	/* absolute SGE id for the iq */
	int8_t   intr_pktc_idx;	/* packet count threshold index */
	int8_t   pad0;
	iq_intr_handler_t *handler;
	__be64  *desc;		/* KVA of descriptor ring */

	volatile uint32_t state;
	struct adapter *adapter;
	const __be64 *cdesc;	/* current descriptor */
	uint8_t  gen;		/* generation bit */
	uint8_t  intr_params;	/* interrupt holdoff parameters */
	uint8_t  intr_next;	/* holdoff for next interrupt */
	uint8_t  esize;		/* size (bytes) of each entry in the queue */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer index */
	uint16_t cntxt_id;	/* SGE context id  for the iq */
};

enum {
	/* eq flags */
	EQ_ALLOCATED	= (1 << 1),	/* firmware resources allocated */
	EQ_STARTED	= (1 << 2),	/* started */
	EQ_CRFLUSHED	= (1 << 3),	/* expecting an update from SGE */
};

/*
 * Egress Queue: driver is producer, T4 is consumer.
 *
 * Note: A free list is an egress queue (driver produces the buffers and T4
 * consumes them) but it's special enough to have its own struct (see sge_fl).
 */
struct sge_eq {
	bus_dma_tag_t tx_tag;	/* tag for transmit buffers */
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	char lockname[16];
	unsigned int flags;
	struct mtx eq_lock;

	struct tx_desc *desc;	/* KVA of descriptor ring */
	bus_addr_t ba;		/* bus address of descriptor ring */
	struct tx_sdesc *sdesc;	/* KVA of software descriptor ring */
	struct buf_ring *br;	/* tx buffer ring */
	struct sge_qstat *spg;	/* status page, for convenience */
	uint16_t cap;		/* max # of desc, for convenience */
	uint16_t avail;		/* available descriptors, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cidx;		/* consumer idx (desc idx) */
	uint16_t pidx;		/* producer idx (desc idx) */
	uint16_t pending;	/* # of descriptors used since last doorbell */
	uint16_t iqid;		/* iq that gets egr_update for the eq */
	uint32_t cntxt_id;	/* SGE context id for the eq */

	/* DMA maps used for tx */
	struct tx_map *maps;
	uint32_t map_total;	/* # of DMA maps */
	uint32_t map_pidx;	/* next map to be used */
	uint32_t map_cidx;	/* reclaimed up to this index */
	uint32_t map_avail;	/* # of available maps */
} __aligned(CACHE_LINE_SIZE);

struct sge_fl {
	bus_dma_tag_t desc_tag;
	bus_dmamap_t desc_map;
	bus_dma_tag_t tag[FL_BUF_SIZES];
	uint8_t tag_idx;
	struct mtx fl_lock;
	char lockname[16];

	__be64 *desc;		/* KVA of descriptor ring, ptr to addresses */
	bus_addr_t ba;		/* bus address of descriptor ring */
	struct fl_sdesc *sdesc;	/* KVA of software descriptor ring */
	uint32_t cap;		/* max # of buffers, for convenience */
	uint16_t qsize;		/* size (# of entries) of the queue */
	uint16_t cntxt_id;	/* SGE context id for the freelist */
	uint32_t cidx;		/* consumer idx (buffer idx, NOT hw desc idx) */
	uint32_t pidx;		/* producer idx (buffer idx, NOT hw desc idx) */
	uint32_t needed;	/* # of buffers needed to fill up fl. */
	uint32_t pending;	/* # of bufs allocated since last doorbell */
	unsigned int dmamap_failed;
};

/* txq: SGE egress queue + miscellaneous items */
struct sge_txq {
	struct sge_eq eq;	/* MUST be first */
	struct mbuf *m;		/* held up due to temporary resource shortage */
	struct task resume_tx;

	struct ifnet *ifp;	/* the interface this txq belongs to */

	/* stats for common events first */

	uint64_t txcsum;	/* # of times hardware assisted with checksum */
	uint64_t tso_wrs;	/* # of IPv4 TSO work requests */
	uint64_t vlan_insertion;/* # of times VLAN tag was inserted */
	uint64_t imm_wrs;	/* # of work requests with immediate data */
	uint64_t sgl_wrs;	/* # of work requests with direct SGL */
	uint64_t txpkt_wrs;	/* # of txpkt work requests (not coalesced) */
	uint64_t txpkts_wrs;	/* # of coalesced tx work requests */
	uint64_t txpkts_pkts;	/* # of frames in coalesced tx work requests */

	/* stats for not-that-common events */

	uint32_t no_dmamap;	/* no DMA map to load the mbuf */
	uint32_t no_desc;	/* out of hardware descriptors */
	uint32_t egr_update;	/* # of SGE_EGR_UPDATE notifications for txq */
};

enum {
	RXQ_LRO_ENABLED	= (1 << 0)
};
/* rxq: SGE ingress queue + SGE free list + miscellaneous items */
struct sge_rxq {
	struct sge_iq iq;	/* MUST be first */
	struct sge_fl fl;

	struct ifnet *ifp;	/* the interface this rxq belongs to */
	unsigned int flags;
#ifdef INET
	struct lro_ctrl lro;	/* LRO state */
#endif

	/* stats for common events first */

	uint64_t rxcsum;	/* # of times hardware assisted with checksum */
	uint64_t vlan_extraction;/* # of times VLAN tag was extracted */

	/* stats for not-that-common events */

} __aligned(CACHE_LINE_SIZE);

struct sge {
	uint16_t timer_val[SGE_NTIMERS];
	uint8_t  counter_val[SGE_NCOUNTERS];

	int nrxq;	/* total rx queues (all ports and the rest) */
	int ntxq;	/* total tx queues (all ports and the rest) */
	int niq;	/* total ingress queues */
	int neq;	/* total egress queues */

	struct sge_iq fwq;	/* Firmware event queue */
	struct sge_iq *fiq;	/* Forwarded interrupt queues (INTR_FWD) */
	struct sge_txq *txq;	/* NIC tx queues */
	struct sge_rxq *rxq;	/* NIC rx queues */

	uint16_t iq_start;
	int eq_start;
	struct sge_iq **iqmap;	/* iq->cntxt_id to iq mapping */
	struct sge_eq **eqmap;	/* eq->cntxt_id to eq mapping */
};

struct adapter {
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

	struct port_info *port[MAX_NPORTS];
	uint8_t chan_map[NCHAN];

	struct tid_info tids;

	int registered_device_map;
	int open_device_map;
	int flags;

	char fw_version[32];
	struct adapter_params params;
	struct t4_virt_res vres;

	struct mtx sc_lock;
	char lockname[16];
};

#define ADAPTER_LOCK(sc)		mtx_lock(&(sc)->sc_lock)
#define ADAPTER_UNLOCK(sc)		mtx_unlock(&(sc)->sc_lock)
#define ADAPTER_LOCK_ASSERT_OWNED(sc)	mtx_assert(&(sc)->sc_lock, MA_OWNED)
#define ADAPTER_LOCK_ASSERT_NOTOWNED(sc) mtx_assert(&(sc)->sc_lock, MA_NOTOWNED)

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

#define for_each_txq(pi, iter, txq) \
	txq = &pi->adapter->sge.txq[pi->first_txq]; \
	for (iter = 0; iter < pi->ntxq; ++iter, ++txq)
#define for_each_rxq(pi, iter, rxq) \
	rxq = &pi->adapter->sge.rxq[pi->first_rxq]; \
	for (iter = 0; iter < pi->nrxq; ++iter, ++rxq)

#define NFIQ(sc) ((sc)->intr_count > 1 ? (sc)->intr_count - 1 : 1)

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

static inline bool is_10G_port(const struct port_info *pi)
{
	return ((pi->link_cfg.supported & FW_PORT_CAP_SPEED_10G) != 0);
}

/* t4_main.c */
void cxgbe_txq_start(void *, int);
int t4_os_find_pci_capability(struct adapter *, int);
int t4_os_pci_save_state(struct adapter *);
int t4_os_pci_restore_state(struct adapter *);
void t4_os_portmod_changed(const struct adapter *, int);
void t4_os_link_changed(struct adapter *, int, int);

/* t4_sge.c */
void t4_sge_modload(void);
void t4_sge_init(struct adapter *);
int t4_create_dma_tag(struct adapter *);
int t4_destroy_dma_tag(struct adapter *);
int t4_setup_adapter_iqs(struct adapter *);
int t4_teardown_adapter_iqs(struct adapter *);
int t4_setup_eth_queues(struct port_info *);
int t4_teardown_eth_queues(struct port_info *);
void t4_intr_all(void *);
void t4_intr_fwd(void *);
void t4_intr_err(void *);
void t4_intr_evt(void *);
void t4_intr_data(void *);
void t4_evt_rx(void *);
void t4_eth_rx(void *);
int t4_eth_tx(struct ifnet *, struct sge_txq *, struct mbuf *);
void t4_update_fl_bufsize(struct ifnet *);

#endif
