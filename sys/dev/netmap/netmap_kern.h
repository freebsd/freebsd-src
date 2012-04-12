/*
 * Copyright (C) 2011-2012 Matteo Landi, Luigi Rizzo. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
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

/*
 * $FreeBSD$
 * $Id: netmap_kern.h 10602 2012-02-21 16:47:55Z luigi $
 *
 * The header contains the definitions of constants and function
 * prototypes used only in kernelspace.
 */

#ifndef _NET_NETMAP_KERN_H_
#define _NET_NETMAP_KERN_H_

#if defined(__FreeBSD__)
#define	NM_LOCK_T	struct mtx
#define	NM_SELINFO_T	struct selinfo
#define	MBUF_LEN(m)	((m)->m_pkthdr.len)
#define	NM_SEND_UP(ifp, m)	((ifp)->if_input)(ifp, m)
#elif defined (linux)
#define	NM_LOCK_T	spinlock_t
#define	NM_SELINFO_T	wait_queue_head_t
#define	MBUF_LEN(m)	((m)->len)
#define	NM_SEND_UP(ifp, m)	netif_rx(m)
#else
#error unsupported platform
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NETMAP);
#endif

#define ND(format, ...)
#define D(format, ...)						\
	do {							\
		struct timeval __xxts;				\
		microtime(&__xxts);				\
		printf("%03d.%06d %s [%d] " format "\n",	\
		(int)__xxts.tv_sec % 1000, (int)__xxts.tv_usec,	\
		__FUNCTION__, __LINE__, ##__VA_ARGS__);		\
	} while (0)
 
struct netmap_adapter;

/*
 * private, kernel view of a ring. Keeps track of the status of
 * a ring across system calls.
 *
 *	nr_hwcur	index of the next buffer to refill.
 *			It corresponds to ring->cur - ring->reserved
 *
 *	nr_hwavail	the number of slots "owned" by userspace.
 *			nr_hwavail =:= ring->avail + ring->reserved
 *
 * The indexes in the NIC and netmap rings are offset by nkr_hwofs slots.
 * This is so that, on a reset, buffers owned by userspace are not
 * modified by the kernel. In particular:
 * RX rings: the next empty buffer (hwcur + hwavail + hwofs) coincides with
 * 	the next empty buffer as known by the hardware (next_to_check or so).
 * TX rings: hwcur + hwofs coincides with next_to_send
 */
struct netmap_kring {
	struct netmap_ring *ring;
	u_int nr_hwcur;
	int nr_hwavail;
	u_int nr_kflags;	/* private driver flags */
#define NKR_PENDINTR	0x1	// Pending interrupt.
	u_int nkr_num_slots;

	int	nkr_hwofs;	/* offset between NIC and netmap ring */
	struct netmap_adapter *na;
	NM_SELINFO_T si;	/* poll/select wait queue */
	NM_LOCK_T q_lock;	/* used if no device lock available */
} __attribute__((__aligned__(64)));

/*
 * This struct extends the 'struct adapter' (or
 * equivalent) device descriptor. It contains all fields needed to
 * support netmap operation.
 */
struct netmap_adapter {
	int refcount; /* number of user-space descriptors using this
			 interface, which is equal to the number of
			 struct netmap_if objs in the mapped region. */

	int separate_locks; /* set if the interface suports different
			       locks for rx, tx and core. */

	u_int num_rx_queues; /* number of tx/rx queue pairs: this is
			   a duplicate field needed to simplify the
			   signature of ``netmap_detach``. */
	u_int num_tx_queues;	// if nonzero, overrides num_queues XXX

	u_int num_tx_desc; /* number of descriptor in each queue */
	u_int num_rx_desc;
	u_int buff_size;	// XXX deprecate, use NETMAP_BUF_SIZE

	//u_int	flags;	// XXX unused
	/* tx_rings and rx_rings are private but allocated
	 * as a contiguous chunk of memory. Each array has
	 * N+1 entries, for the adapter queues and for the host queue.
	 */
	struct netmap_kring *tx_rings; /* array of TX rings. */
	struct netmap_kring *rx_rings; /* array of RX rings. */

	NM_SELINFO_T tx_si, rx_si;	/* global wait queues */

	/* copy of if_qflush and if_transmit pointers, to intercept
	 * packets from the network stack when netmap is active.
	 * XXX probably if_qflush is not necessary.
	 */
	//void    (*if_qflush)(struct ifnet *);	// XXX unused
	int     (*if_transmit)(struct ifnet *, struct mbuf *);

	/* references to the ifnet and device routines, used by
	 * the generic netmap functions.
	 */
	struct ifnet *ifp; /* adapter is ifp->if_softc */

	NM_LOCK_T core_lock;	/* used if no device lock available */

	int (*nm_register)(struct ifnet *, int onoff);
	void (*nm_lock)(struct ifnet *, int what, u_int ringid);
	int (*nm_txsync)(struct ifnet *, u_int ring, int lock);
	int (*nm_rxsync)(struct ifnet *, u_int ring, int lock);
#ifdef linux
	struct net_device_ops nm_ndo;
#endif /* linux */
};

/*
 * The combination of "enable" (ifp->if_capabilities &IFCAP_NETMAP)
 * and refcount gives the status of the interface, namely:
 *
 *	enable	refcount	Status
 *
 *	FALSE	0		normal operation
 *	FALSE	!= 0		-- (impossible)
 *	TRUE	1		netmap mode
 *	TRUE	0		being deleted.
 */

#define NETMAP_DELETING(_na)  (  ((_na)->refcount == 0) &&	\
	( (_na)->ifp->if_capenable & IFCAP_NETMAP) )

/*
 * parameters for (*nm_lock)(adapter, what, index)
 */
enum {
	NETMAP_NO_LOCK = 0,
	NETMAP_CORE_LOCK, NETMAP_CORE_UNLOCK,
	NETMAP_TX_LOCK, NETMAP_TX_UNLOCK,
	NETMAP_RX_LOCK, NETMAP_RX_UNLOCK,
#ifdef __FreeBSD__
#define	NETMAP_REG_LOCK		NETMAP_CORE_LOCK
#define	NETMAP_REG_UNLOCK	NETMAP_CORE_UNLOCK
#else
	NETMAP_REG_LOCK, NETMAP_REG_UNLOCK
#endif
};

/*
 * The following are support routines used by individual drivers to
 * support netmap operation.
 *
 * netmap_attach() initializes a struct netmap_adapter, allocating the
 * 	struct netmap_ring's and the struct selinfo.
 *
 * netmap_detach() frees the memory allocated by netmap_attach().
 *
 * netmap_start() replaces the if_transmit routine of the interface,
 *	and is used to intercept packets coming from the stack.
 *
 * netmap_load_map/netmap_reload_map are helper routines to set/reset
 *	the dmamap for a packet buffer
 *
 * netmap_reset() is a helper routine to be called in the driver
 *	when reinitializing a ring.
 */
int netmap_attach(struct netmap_adapter *, int);
void netmap_detach(struct ifnet *);
int netmap_start(struct ifnet *, struct mbuf *);
enum txrx { NR_RX = 0, NR_TX = 1 };
struct netmap_slot *netmap_reset(struct netmap_adapter *na,
	enum txrx tx, int n, u_int new_cur);
int netmap_ring_reinit(struct netmap_kring *);

extern int netmap_buf_size;
#define NETMAP_BUF_SIZE	netmap_buf_size
extern int netmap_mitigate;
extern int netmap_no_pendintr;
extern u_int netmap_total_buffers;
extern char *netmap_buffer_base;
extern int netmap_verbose;	// XXX debugging
enum {                                  /* verbose flags */
	NM_VERB_ON = 1,                 /* generic verbose */
	NM_VERB_HOST = 0x2,             /* verbose host stack */
	NM_VERB_RXSYNC = 0x10,          /* verbose on rxsync/txsync */
	NM_VERB_TXSYNC = 0x20,
	NM_VERB_RXINTR = 0x100,         /* verbose on rx/tx intr (driver) */
	NM_VERB_TXINTR = 0x200,
	NM_VERB_NIC_RXSYNC = 0x1000,    /* verbose on rx/tx intr (driver) */
	NM_VERB_NIC_TXSYNC = 0x2000,
};

/*
 * NA returns a pointer to the struct netmap adapter from the ifp,
 * WNA is used to write it.
 */
#ifndef WNA
#define	WNA(_ifp)	(_ifp)->if_pspare[0]
#endif
#define	NA(_ifp)	((struct netmap_adapter *)WNA(_ifp))


/* Callback invoked by the dma machinery after a successfull dmamap_load */
static void netmap_dmamap_cb(__unused void *arg,
    __unused bus_dma_segment_t * segs, __unused int nseg, __unused int error)
{
}

/* bus_dmamap_load wrapper: call aforementioned function if map != NULL.
 * XXX can we do it without a callback ?
 */
static inline void
netmap_load_map(bus_dma_tag_t tag, bus_dmamap_t map, void *buf)
{
	if (map)
		bus_dmamap_load(tag, map, buf, NETMAP_BUF_SIZE,
		    netmap_dmamap_cb, NULL, BUS_DMA_NOWAIT);
}

/* update the map when a buffer changes. */
static inline void
netmap_reload_map(bus_dma_tag_t tag, bus_dmamap_t map, void *buf)
{
	if (map) {
		bus_dmamap_unload(tag, map);
		bus_dmamap_load(tag, map, buf, NETMAP_BUF_SIZE,
		    netmap_dmamap_cb, NULL, BUS_DMA_NOWAIT);
	}
}

/*
 * functions to map NIC to KRING indexes (n2k) and vice versa (k2n)
 */
static inline int
netmap_idx_n2k(struct netmap_kring *kr, int idx)
{
	int n = kr->nkr_num_slots;
	idx += kr->nkr_hwofs;
	if (idx < 0)
		return idx + n;
	else if (idx < n)
		return idx;
	else
		return idx - n;
}


static inline int
netmap_idx_k2n(struct netmap_kring *kr, int idx)
{
	int n = kr->nkr_num_slots;
	idx -= kr->nkr_hwofs;
	if (idx < 0)
		return idx + n;
	else if (idx < n)
		return idx;
	else
		return idx - n;
}


/*
 * NMB return the virtual address of a buffer (buffer 0 on bad index)
 * PNMB also fills the physical address
 */
static inline void *
NMB(struct netmap_slot *slot)
{
	uint32_t i = slot->buf_idx;
	return (i >= netmap_total_buffers) ? netmap_buffer_base :
		netmap_buffer_base + (i * NETMAP_BUF_SIZE);
}

static inline void *
PNMB(struct netmap_slot *slot, uint64_t *pp)
{
	uint32_t i = slot->buf_idx;
	void *ret = (i >= netmap_total_buffers) ? netmap_buffer_base :
		netmap_buffer_base + (i * NETMAP_BUF_SIZE);
	*pp = vtophys(ret);
	return ret;
}

/* default functions to handle rx/tx interrupts */
int netmap_rx_irq(struct ifnet *, int, int *);
#define netmap_tx_irq(_n, _q) netmap_rx_irq(_n, _q, NULL)
#endif /* _NET_NETMAP_KERN_H_ */
