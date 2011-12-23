/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
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
 * $Id: netmap_kern.h 9795 2011-12-02 11:39:08Z luigi $
 *
 * The header contains the definitions of constants and function
 * prototypes used only in kernelspace.
 */

#ifndef _NET_NETMAP_KERN_H_
#define _NET_NETMAP_KERN_H_

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NETMAP);
#endif

#define ND(format, ...)
#define D(format, ...)					\
	do {						\
		struct timeval __xxts;			\
		microtime(&__xxts);				\
		printf("%03d.%06d %s [%d] " format "\n",\
		(int)__xxts.tv_sec % 1000, (int)__xxts.tv_usec,		\
		__FUNCTION__, __LINE__, ##__VA_ARGS__);	\
	} while (0)
 
struct netmap_adapter;

/*
 * private, kernel view of a ring.
 *
 * XXX 20110627-todo
 * The index in the NIC and netmap ring is offset by nkr_hwofs slots.
 * This is so that, on a reset, buffers owned by userspace are not
 * modified by the kernel. In particular:
 * RX rings: the next empty buffer (hwcur + hwavail + hwofs) coincides
 * 	the next empty buffer as known by the hardware (next_to_check or so).
 * TX rings: hwcur + hwofs coincides with next_to_send
 */
struct netmap_kring {
	struct netmap_ring *ring;
	u_int nr_hwcur;
	int nr_hwavail;
	u_int nr_kflags;
	u_int nkr_num_slots;

	int	nkr_hwofs;	/* offset between NIC and netmap ring */
	struct netmap_adapter *na;	 // debugging
	struct selinfo si; /* poll/select wait queue */
};

/*
 * This struct is part of and extends the 'struct adapter' (or
 * equivalent) device descriptor. It contains all fields needed to
 * support netmap operation.
 */
struct netmap_adapter {
	int refcount; /* number of user-space descriptors using this
			 interface, which is equal to the number of
			 struct netmap_if objs in the mapped region. */

	int separate_locks; /* set if the interface suports different
			       locks for rx, tx and core. */

	u_int num_queues; /* number of tx/rx queue pairs: this is
			   a duplicate field needed to simplify the
			   signature of ``netmap_detach``. */

	u_int num_tx_desc; /* number of descriptor in each queue */
	u_int num_rx_desc;
	u_int buff_size;

	u_int	flags;
	/* tx_rings and rx_rings are private but allocated
	 * as a contiguous chunk of memory. Each array has
	 * N+1 entries, for the adapter queues and for the host queue.
	 */
	struct netmap_kring *tx_rings; /* array of TX rings. */
	struct netmap_kring *rx_rings; /* array of RX rings. */

	/* copy of if_qflush and if_transmit pointers, to intercept
	 * packets from the network stack when netmap is active.
	 * XXX probably if_qflush is not necessary.
	 */
	void    (*if_qflush)(struct ifnet *);
	int     (*if_transmit)(struct ifnet *, struct mbuf *);

	/* references to the ifnet and device routines, used by
	 * the generic netmap functions.
	 */
	struct ifnet *ifp; /* adapter is ifp->if_softc */

	int (*nm_register)(struct ifnet *, int onoff);
	void (*nm_lock)(void *, int what, u_int ringid);
	int (*nm_txsync)(void *, u_int ring, int lock);
	int (*nm_rxsync)(void *, u_int ring, int lock);
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
void netmap_load_map(bus_dma_tag_t tag, bus_dmamap_t map,
        void *buf, bus_size_t buflen);
void netmap_reload_map(bus_dma_tag_t tag, bus_dmamap_t map,
        void *buf, bus_size_t buflen);
int netmap_ring_reinit(struct netmap_kring *);

/*
 * XXX eventually, get rid of netmap_total_buffers and netmap_buffer_base
 * in favour of the structure
 */
// struct netmap_buf_pool;
// extern struct netmap_buf_pool nm_buf_pool;
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


/*
 * return the address of a buffer.
 * XXX this is a special version with hardwired 2k bufs
 * On error return netmap_buffer_base which is detected as a bad pointer.
 */
static inline char *
NMB(struct netmap_slot *slot)
{
	uint32_t i = slot->buf_idx;
	return (i >= netmap_total_buffers) ? netmap_buffer_base :
#if NETMAP_BUF_SIZE == 2048
		netmap_buffer_base + (i << 11);
#else
		netmap_buffer_base + (i *NETMAP_BUF_SIZE);
#endif
}

#endif /* _NET_NETMAP_KERN_H_ */
