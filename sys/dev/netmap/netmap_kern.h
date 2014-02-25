/*
 * Copyright (C) 2011-2013 Matteo Landi, Luigi Rizzo. All rights reserved.
 * Copyright (C) 2013 Universita` di Pisa. All rights reserved.
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
 *
 * The header contains the definitions of constants and function
 * prototypes used only in kernelspace.
 */

#ifndef _NET_NETMAP_KERN_H_
#define _NET_NETMAP_KERN_H_

#define WITH_VALE	// comment out to disable VALE support

#if defined(__FreeBSD__)

#define likely(x)	__builtin_expect((long)!!(x), 1L)
#define unlikely(x)	__builtin_expect((long)!!(x), 0L)

#define	NM_LOCK_T	struct mtx
#define	NMG_LOCK_T	struct mtx
#define NMG_LOCK_INIT()	mtx_init(&netmap_global_lock, \
				"netmap global lock", NULL, MTX_DEF)
#define NMG_LOCK_DESTROY()	mtx_destroy(&netmap_global_lock)
#define NMG_LOCK()	mtx_lock(&netmap_global_lock)
#define NMG_UNLOCK()	mtx_unlock(&netmap_global_lock)
#define NMG_LOCK_ASSERT()	mtx_assert(&netmap_global_lock, MA_OWNED)

#define	NM_SELINFO_T	struct selinfo
#define	MBUF_LEN(m)	((m)->m_pkthdr.len)
#define	MBUF_IFP(m)	((m)->m_pkthdr.rcvif)
#define	NM_SEND_UP(ifp, m)	((ifp)->if_input)(ifp, m)

#define NM_ATOMIC_T	volatile int	// XXX ?
/* atomic operations */
#include <machine/atomic.h>
#define NM_ATOMIC_TEST_AND_SET(p)       (!atomic_cmpset_acq_int((p), 0, 1))
#define NM_ATOMIC_CLEAR(p)              atomic_store_rel_int((p), 0)


MALLOC_DECLARE(M_NETMAP);

// XXX linux struct, not used in FreeBSD
struct net_device_ops {
};
struct hrtimer {
};

#elif defined (linux)

#define	NM_LOCK_T	safe_spinlock_t	// see bsd_glue.h
#define	NM_SELINFO_T	wait_queue_head_t
#define	MBUF_LEN(m)	((m)->len)
#define	MBUF_IFP(m)	((m)->dev)
#define	NM_SEND_UP(ifp, m)	netif_rx(m)

#define NM_ATOMIC_T	volatile long unsigned int

// XXX a mtx would suffice here too 20130404 gl
#define NMG_LOCK_T		struct semaphore
#define NMG_LOCK_INIT()		sema_init(&netmap_global_lock, 1)
#define NMG_LOCK_DESTROY()
#define NMG_LOCK()		down(&netmap_global_lock)
#define NMG_UNLOCK()		up(&netmap_global_lock)
#define NMG_LOCK_ASSERT()	//	XXX to be completed

#ifndef DEV_NETMAP
#define DEV_NETMAP
#endif /* DEV_NETMAP */

/*
 * IFCAP_NETMAP goes into net_device's priv_flags (if_capenable).
 * This was 16 bits up to linux 2.6.36, so we need a 16 bit value on older
 * platforms and tolerate the clash with IFF_DYNAMIC and IFF_BRIDGE_PORT.
 * For the 32-bit value, 0x100000 has no clashes until at least 3.5.1
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,37)
#define IFCAP_NETMAP	0x8000
#else
#define IFCAP_NETMAP	0x200000
#endif

#elif defined (__APPLE__)

#warning apple support is incomplete.
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define	NM_LOCK_T	IOLock *
#define	NM_SELINFO_T	struct selinfo
#define	MBUF_LEN(m)	((m)->m_pkthdr.len)
#define	NM_SEND_UP(ifp, m)	((ifp)->if_input)(ifp, m)

#else

#error unsupported platform

#endif /* end - platform-specific code */

#define ND(format, ...)
#define D(format, ...)						\
	do {							\
		struct timeval __xxts;				\
		microtime(&__xxts);				\
		printf("%03d.%06d %s [%d] " format "\n",	\
		(int)__xxts.tv_sec % 1000, (int)__xxts.tv_usec,	\
		__FUNCTION__, __LINE__, ##__VA_ARGS__);		\
	} while (0)

/* rate limited, lps indicates how many per second */
#define RD(lps, format, ...)					\
	do {							\
		static int t0, __cnt;				\
		if (t0 != time_second) {			\
			t0 = time_second;			\
			__cnt = 0;				\
		}						\
		if (__cnt++ < lps)				\
			D(format, ##__VA_ARGS__);		\
	} while (0)

struct netmap_adapter;
struct nm_bdg_fwd;
struct nm_bridge;
struct netmap_priv_d;

const char *nm_dump_buf(char *p, int len, int lim, char *dst);

#include "netmap_mbq.h"

extern NMG_LOCK_T	netmap_global_lock;

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
 *
 * Clients cannot issue concurrent syscall on a ring. The system
 * detects this and reports an error using two flags,
 * NKR_WBUSY and NKR_RBUSY
 * For received packets, slot->flags is set to nkr_slot_flags
 * so we can provide a proper initial value (e.g. set NS_FORWARD
 * when operating in 'transparent' mode).
 *
 * The following fields are used to implement lock-free copy of packets
 * from input to output ports in VALE switch:
 *	nkr_hwlease	buffer after the last one being copied.
 *			A writer in nm_bdg_flush reserves N buffers
 *			from nr_hwlease, advances it, then does the
 *			copy outside the lock.
 *			In RX rings (used for VALE ports),
 *			nkr_hwcur + nkr_hwavail <= nkr_hwlease < nkr_hwcur+N-1
 *			In TX rings (used for NIC or host stack ports)
 *			nkr_hwcur <= nkr_hwlease < nkr_hwcur+ nkr_hwavail
 *	nkr_leases	array of nkr_num_slots where writers can report
 *			completion of their block. NR_NOSLOT (~0) indicates
 *			that the writer has not finished yet
 *	nkr_lease_idx	index of next free slot in nr_leases, to be assigned
 *
 * The kring is manipulated by txsync/rxsync and generic netmap function.
 * q_lock is used to arbitrate access to the kring from within the netmap
 * code, and this and other protections guarantee that there is never
 * more than 1 concurrent call to txsync or rxsync. So we are free
 * to manipulate the kring from within txsync/rxsync without any extra
 * locks.
 */
struct netmap_kring {
	struct netmap_ring *ring;
	uint32_t nr_hwcur;
	uint32_t nr_hwavail;
	uint32_t nr_kflags;	/* private driver flags */
	int32_t nr_hwreserved;
#define NKR_PENDINTR	0x1	// Pending interrupt.
	uint32_t nkr_num_slots;
	int32_t	nkr_hwofs;	/* offset between NIC and netmap ring */

	uint16_t	nkr_slot_flags;	/* initial value for flags */
	struct netmap_adapter *na;
	struct nm_bdg_fwd *nkr_ft;
	uint32_t *nkr_leases;
#define NR_NOSLOT	((uint32_t)~0)
	uint32_t nkr_hwlease;
	uint32_t nkr_lease_idx;

	NM_SELINFO_T si;	/* poll/select wait queue */
	NM_LOCK_T q_lock;	/* protects kring and ring. */
	NM_ATOMIC_T nr_busy;	/* prevent concurrent syscalls */

	volatile int nkr_stopped;

	/* support for adapters without native netmap support.
	 * On tx rings we preallocate an array of tx buffers
	 * (same size as the netmap ring), on rx rings we
	 * store incoming packets in a queue.
	 * XXX who writes to the rx queue ?
	 */
	struct mbuf **tx_pool;
	u_int nr_ntc;                   /* Emulation of a next-to-clean RX ring pointer. */
	struct mbq rx_queue;            /* A queue for intercepted rx mbufs. */

} __attribute__((__aligned__(64)));


/* return the next index, with wraparound */
static inline uint32_t
nm_next(uint32_t i, uint32_t lim)
{
	return unlikely (i == lim) ? 0 : i + 1;
}

/*
 *
 * Here is the layout for the Rx and Tx rings.

       RxRING                            TxRING

      +-----------------+            +-----------------+
      |                 |            |                 |
      |XXX free slot XXX|            |XXX free slot XXX|
      +-----------------+            +-----------------+
      |                 |<-hwcur     |                 |<-hwcur
      | reserved    h   |            | (ready          |
      +-----------  w  -+            |  to be          |
 cur->|             a   |            |  sent)      h   |
      |             v   |            +----------   w   |
      |             a   |       cur->| (being      a   |
      |             i   |            |  prepared)  v   |
      | avail       l   |            |             a   |
      +-----------------+            +  a  ------  i   +
      |                 | ...        |  v          l   |<-hwlease
      | (being          | ...        |  a              | ...
      |  prepared)      | ...        |  i              | ...
      +-----------------+ ...        |  l              | ...
      |                 |<-hwlease   +-----------------+
      |                 |            |                 |
      |                 |            |                 |
      |                 |            |                 |
      |                 |            |                 |
      +-----------------+            +-----------------+

 * The cur/avail (user view) and hwcur/hwavail (kernel view)
 * are used in the normal operation of the card.
 *
 * When a ring is the output of a switch port (Rx ring for
 * a VALE port, Tx ring for the host stack or NIC), slots
 * are reserved in blocks through 'hwlease' which points
 * to the next unused slot.
 * On an Rx ring, hwlease is always after hwavail,
 * and completions cause avail to advance.
 * On a Tx ring, hwlease is always between cur and hwavail,
 * and completions cause cur to advance.
 *
 * nm_kr_space() returns the maximum number of slots that
 * can be assigned.
 * nm_kr_lease() reserves the required number of buffers,
 *    advances nkr_hwlease and also returns an entry in
 *    a circular array where completions should be reported.
 */




enum txrx { NR_RX = 0, NR_TX = 1 };

/*
 * The "struct netmap_adapter" extends the "struct adapter"
 * (or equivalent) device descriptor.
 * It contains all base fields needed to support netmap operation.
 * There are in fact different types of netmap adapters
 * (native, generic, VALE switch...) so a netmap_adapter is
 * just the first field in the derived type.
 */
struct netmap_adapter {
	/*
	 * On linux we do not have a good way to tell if an interface
	 * is netmap-capable. So we always use the following trick:
	 * NA(ifp) points here, and the first entry (which hopefully
	 * always exists and is at least 32 bits) contains a magic
	 * value which we can use to detect that the interface is good.
	 */
	uint32_t magic;
	uint32_t na_flags;	/* enabled, and other flags */
#define NAF_SKIP_INTR	1	/* use the regular interrupt handler.
				 * useful during initialization
				 */
#define NAF_SW_ONLY	2	/* forward packets only to sw adapter */
#define NAF_BDG_MAYSLEEP 4	/* the bridge is allowed to sleep when
				 * forwarding packets coming from this
				 * interface
				 */
#define NAF_MEM_OWNER	8	/* the adapter is responsible for the
				 * deallocation of the memory allocator
				 */
#define NAF_NATIVE_ON   16      /* the adapter is native and the attached
				 * interface is in netmap mode
				 */
#define	NAF_NETMAP_ON	32	/* netmap is active (either native or
				 * emulated. Where possible (e.g. FreeBSD)
				 * IFCAP_NETMAP also mirrors this flag.
				 */
	int active_fds; /* number of user-space descriptors using this
			 interface, which is equal to the number of
			 struct netmap_if objs in the mapped region. */

	u_int num_rx_rings; /* number of adapter receive rings */
	u_int num_tx_rings; /* number of adapter transmit rings */

	u_int num_tx_desc; /* number of descriptor in each queue */
	u_int num_rx_desc;

	/* tx_rings and rx_rings are private but allocated
	 * as a contiguous chunk of memory. Each array has
	 * N+1 entries, for the adapter queues and for the host queue.
	 */
	struct netmap_kring *tx_rings; /* array of TX rings. */
	struct netmap_kring *rx_rings; /* array of RX rings. */
	void *tailroom;		       /* space below the rings array */
				       /* (used for leases) */


	NM_SELINFO_T tx_si, rx_si;	/* global wait queues */

	/* copy of if_qflush and if_transmit pointers, to intercept
	 * packets from the network stack when netmap is active.
	 */
	int     (*if_transmit)(struct ifnet *, struct mbuf *);

	/* references to the ifnet and device routines, used by
	 * the generic netmap functions.
	 */
	struct ifnet *ifp; /* adapter is ifp->if_softc */

	/* private cleanup */
	void (*nm_dtor)(struct netmap_adapter *);

	int (*nm_register)(struct netmap_adapter *, int onoff);

	int (*nm_txsync)(struct netmap_adapter *, u_int ring, int flags);
	int (*nm_rxsync)(struct netmap_adapter *, u_int ring, int flags);
#define NAF_FORCE_READ    1
#define NAF_FORCE_RECLAIM 2
	/* return configuration information */
	int (*nm_config)(struct netmap_adapter *,
		u_int *txr, u_int *txd, u_int *rxr, u_int *rxd);
	int (*nm_krings_create)(struct netmap_adapter *);
	void (*nm_krings_delete)(struct netmap_adapter *);
	int (*nm_notify)(struct netmap_adapter *,
		u_int ring, enum txrx, int flags);
#define NAF_GLOBAL_NOTIFY 4
#define NAF_DISABLE_NOTIFY 8

	/* standard refcount to control the lifetime of the adapter
	 * (it should be equal to the lifetime of the corresponding ifp)
	 */
	int na_refcount;

	/* memory allocator (opaque)
	 * We also cache a pointer to the lut_entry for translating
	 * buffer addresses, and the total number of buffers.
	 */
 	struct netmap_mem_d *nm_mem;
	struct lut_entry *na_lut;
	uint32_t na_lut_objtotal;	/* max buffer index */

	/* used internally. If non-null, the interface cannot be bound
	 * from userspace
	 */
	void *na_private;
};

/*
 * If the NIC is owned by the kernel
 * (i.e., bridge), neither another bridge nor user can use it;
 * if the NIC is owned by a user, only users can share it.
 * Evaluation must be done under NMG_LOCK().
 */
#define NETMAP_OWNED_BY_KERN(na)	(na->na_private)
#define NETMAP_OWNED_BY_ANY(na) \
	(NETMAP_OWNED_BY_KERN(na) || (na->active_fds > 0))


/*
 * derived netmap adapters for various types of ports
 */
struct netmap_vp_adapter {	/* VALE software port */
	struct netmap_adapter up;

	/*
	 * Bridge support:
	 *
	 * bdg_port is the port number used in the bridge;
	 * na_bdg points to the bridge this NA is attached to.
	 */
	int bdg_port;
	struct nm_bridge *na_bdg;
	int retry;

	u_int offset;   /* Offset of ethernet header for each packet. */
};

struct netmap_hw_adapter {	/* physical device */
	struct netmap_adapter up;

	struct net_device_ops nm_ndo;	// XXX linux only
};

struct netmap_generic_adapter {	/* non-native device */
	struct netmap_hw_adapter up;

	/* Pointer to a previously used netmap adapter. */
	struct netmap_adapter *prev;

	/* generic netmap adapters support:
	 * a net_device_ops struct overrides ndo_select_queue(),
	 * save_if_input saves the if_input hook (FreeBSD),
	 * mit_timer and mit_pending implement rx interrupt mitigation,
	 */
	struct net_device_ops generic_ndo;
	void (*save_if_input)(struct ifnet *, struct mbuf *);

	struct hrtimer mit_timer;
	int mit_pending;
};

#ifdef WITH_VALE

/* bridge wrapper for non VALE ports. It is used to connect real devices to the bridge.
 *
 * The real device must already have its own netmap adapter (hwna).  The
 * bridge wrapper and the hwna adapter share the same set of netmap rings and
 * buffers, but they have two separate sets of krings descriptors, with tx/rx
 * meanings swapped:
 *
 *                                  netmap
 *           bwrap     krings       rings      krings      hwna
 *         +------+   +------+     +-----+    +------+   +------+
 *         |tx_rings->|      |\   /|     |----|      |<-tx_rings|
 *         |      |   +------+ \ / +-----+    +------+   |      |
 *         |      |             X                        |      |
 *         |      |            / \                       |      |
 *         |      |   +------+/   \+-----+    +------+   |      |
 *         |rx_rings->|      |     |     |----|      |<-rx_rings|
 *         |      |   +------+     +-----+    +------+   |      |
 *         +------+                                      +------+
 *
 * - packets coming from the bridge go to the brwap rx rings, which are also the
 *   hwna tx rings.  The bwrap notify callback will then complete the hwna tx
 *   (see netmap_bwrap_notify).
 * - packets coming from the outside go to the hwna rx rings, which are also the
 *   bwrap tx rings.  The (overwritten) hwna notify method will then complete
 *   the bridge tx (see netmap_bwrap_intr_notify).
 *
 *   The bridge wrapper may optionally connect the hwna 'host' rings to the
 *   bridge. This is done by using a second port in the bridge and connecting it
 *   to the 'host' netmap_vp_adapter contained in the netmap_bwrap_adapter.
 *   The brwap host adapter cross-links the hwna host rings in the same way as shown above.
 *
 * - packets coming from the bridge and directed to host stack are handled by the
 *   bwrap host notify callback (see netmap_bwrap_host_notify)
 * - packets coming from the host stack are still handled by the overwritten
 *   hwna notify callback (netmap_bwrap_intr_notify), but are diverted to the
 *   host adapter depending on the ring number.
 *
 */
struct netmap_bwrap_adapter {
	struct netmap_vp_adapter up;
	struct netmap_vp_adapter host;  /* for host rings */
	struct netmap_adapter *hwna;	/* the underlying device */

	/* backup of the hwna notify callback */
	int (*save_notify)(struct netmap_adapter *,
			u_int ring, enum txrx, int flags);
	/* When we attach a physical interface to the bridge, we
	 * allow the controlling process to terminate, so we need
	 * a place to store the netmap_priv_d data structure.
	 * This is only done when physical interfaces are attached to a bridge.
	 */
	struct netmap_priv_d *na_kpriv;
};


/*
 * Available space in the ring. Only used in VALE code
 */
static inline uint32_t
nm_kr_space(struct netmap_kring *k, int is_rx)
{
	int space;

	if (is_rx) {
		int busy = k->nkr_hwlease - k->nr_hwcur + k->nr_hwreserved;
		if (busy < 0)
			busy += k->nkr_num_slots;
		space = k->nkr_num_slots - 1 - busy;
	} else {
		space = k->nr_hwcur + k->nr_hwavail - k->nkr_hwlease;
		if (space < 0)
			space += k->nkr_num_slots;
	}
#if 0
	// sanity check
	if (k->nkr_hwlease >= k->nkr_num_slots ||
		k->nr_hwcur >= k->nkr_num_slots ||
		k->nr_hwavail >= k->nkr_num_slots ||
		busy < 0 ||
		busy >= k->nkr_num_slots) {
		D("invalid kring, cur %d avail %d lease %d lease_idx %d lim %d",			k->nr_hwcur, k->nr_hwavail, k->nkr_hwlease,
			k->nkr_lease_idx, k->nkr_num_slots);
	}
#endif
	return space;
}




/* make a lease on the kring for N positions. return the
 * lease index
 */
static inline uint32_t
nm_kr_lease(struct netmap_kring *k, u_int n, int is_rx)
{
	uint32_t lim = k->nkr_num_slots - 1;
	uint32_t lease_idx = k->nkr_lease_idx;

	k->nkr_leases[lease_idx] = NR_NOSLOT;
	k->nkr_lease_idx = nm_next(lease_idx, lim);

	if (n > nm_kr_space(k, is_rx)) {
		D("invalid request for %d slots", n);
		panic("x");
	}
	/* XXX verify that there are n slots */
	k->nkr_hwlease += n;
	if (k->nkr_hwlease > lim)
		k->nkr_hwlease -= lim + 1;

	if (k->nkr_hwlease >= k->nkr_num_slots ||
		k->nr_hwcur >= k->nkr_num_slots ||
		k->nr_hwavail >= k->nkr_num_slots ||
		k->nkr_lease_idx >= k->nkr_num_slots) {
		D("invalid kring %s, cur %d avail %d lease %d lease_idx %d lim %d",
			k->na->ifp->if_xname,
			k->nr_hwcur, k->nr_hwavail, k->nkr_hwlease,
			k->nkr_lease_idx, k->nkr_num_slots);
	}
	return lease_idx;
}

#endif /* WITH_VALE */

/* return update position */
static inline uint32_t
nm_kr_rxpos(struct netmap_kring *k)
{
	uint32_t pos = k->nr_hwcur + k->nr_hwavail;
	if (pos >= k->nkr_num_slots)
		pos -= k->nkr_num_slots;
#if 0
	if (pos >= k->nkr_num_slots ||
		k->nkr_hwlease >= k->nkr_num_slots ||
		k->nr_hwcur >= k->nkr_num_slots ||
		k->nr_hwavail >= k->nkr_num_slots ||
		k->nkr_lease_idx >= k->nkr_num_slots) {
		D("invalid kring, cur %d avail %d lease %d lease_idx %d lim %d",			k->nr_hwcur, k->nr_hwavail, k->nkr_hwlease,
			k->nkr_lease_idx, k->nkr_num_slots);
	}
#endif
	return pos;
}


/*
 * protect against multiple threads using the same ring.
 * also check that the ring has not been stopped.
 * We only care for 0 or !=0 as a return code.
 */
#define NM_KR_BUSY	1
#define NM_KR_STOPPED	2

static __inline void nm_kr_put(struct netmap_kring *kr)
{
	NM_ATOMIC_CLEAR(&kr->nr_busy);
}

static __inline int nm_kr_tryget(struct netmap_kring *kr)
{
	/* check a first time without taking the lock
	 * to avoid starvation for nm_kr_get()
	 */
	if (unlikely(kr->nkr_stopped)) {
		ND("ring %p stopped (%d)", kr, kr->nkr_stopped);
		return NM_KR_STOPPED;
	}
	if (unlikely(NM_ATOMIC_TEST_AND_SET(&kr->nr_busy)))
		return NM_KR_BUSY;
	/* check a second time with lock held */
	if (unlikely(kr->nkr_stopped)) {
		ND("ring %p stopped (%d)", kr, kr->nkr_stopped);
		nm_kr_put(kr);
		return NM_KR_STOPPED;
	}
	return 0;
}


/*
 * The following are support routines used by individual drivers to
 * support netmap operation.
 *
 * netmap_attach() initializes a struct netmap_adapter, allocating the
 * 	struct netmap_ring's and the struct selinfo.
 *
 * netmap_detach() frees the memory allocated by netmap_attach().
 *
 * netmap_transmit() replaces the if_transmit routine of the interface,
 *	and is used to intercept packets coming from the stack.
 *
 * netmap_load_map/netmap_reload_map are helper routines to set/reset
 *	the dmamap for a packet buffer
 *
 * netmap_reset() is a helper routine to be called in the driver
 *	when reinitializing a ring.
 */
int netmap_attach(struct netmap_adapter *);
int netmap_attach_common(struct netmap_adapter *);
void netmap_detach_common(struct netmap_adapter *na);
void netmap_detach(struct ifnet *);
int netmap_transmit(struct ifnet *, struct mbuf *);
struct netmap_slot *netmap_reset(struct netmap_adapter *na,
	enum txrx tx, u_int n, u_int new_cur);
int netmap_ring_reinit(struct netmap_kring *);

/* set/clear native flags. XXX maybe also if_transmit ? */
static inline void
nm_set_native_flags(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;

	na->na_flags |= (NAF_NATIVE_ON | NAF_NETMAP_ON);
#ifdef IFCAP_NETMAP /* or FreeBSD ? */
	ifp->if_capenable |= IFCAP_NETMAP;
#endif
#ifdef __FreeBSD__
	na->if_transmit = ifp->if_transmit;
	ifp->if_transmit = netmap_transmit;
#else
	na->if_transmit = (void *)ifp->netdev_ops;
	ifp->netdev_ops = &((struct netmap_hw_adapter *)na)->nm_ndo;
#endif
}

static inline void
nm_clear_native_flags(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;

#ifdef __FreeBSD__
	ifp->if_transmit = na->if_transmit;
#else
	ifp->netdev_ops = (void *)na->if_transmit;
#endif
	na->na_flags &= ~(NAF_NATIVE_ON | NAF_NETMAP_ON);
#ifdef IFCAP_NETMAP /* or FreeBSD ? */
	ifp->if_capenable &= ~IFCAP_NETMAP;
#endif
}

/*
 * validates parameters in the ring/kring, returns a value for cur,
 * and the 'new_slots' value in the argument.
 * If any error, returns cur > lim to force a reinit.
 */
u_int nm_txsync_prologue(struct netmap_kring *, u_int *);

/*
 * validates parameters in the ring/kring, returns a value for cur,
 * and the 'reserved' value in the argument.
 * If any error, returns cur > lim to force a reinit.
 */
u_int nm_rxsync_prologue(struct netmap_kring *, u_int *);

/*
 * update kring and ring at the end of txsync
 */
static inline void
nm_txsync_finalize(struct netmap_kring *kring, u_int cur)
{
	/* recompute hwreserved */
	kring->nr_hwreserved = cur - kring->nr_hwcur;
	if (kring->nr_hwreserved < 0)
		kring->nr_hwreserved += kring->nkr_num_slots;

	/* update avail and reserved to what the kernel knows */
	kring->ring->avail = kring->nr_hwavail;
	kring->ring->reserved = kring->nr_hwreserved;
}

/* check/fix address and len in tx rings */
#if 1 /* debug version */
#define	NM_CHECK_ADDR_LEN(_a, _l)	do {				\
	if (_a == netmap_buffer_base || _l > NETMAP_BUF_SIZE) {		\
		RD(5, "bad addr/len ring %d slot %d idx %d len %d",	\
			ring_nr, nm_i, slot->buf_idx, len);		\
		if (_l > NETMAP_BUF_SIZE)				\
			_l = NETMAP_BUF_SIZE;				\
	} } while (0)
#else /* no debug version */
#define	NM_CHECK_ADDR_LEN(_a, _l)	do {				\
		if (_l > NETMAP_BUF_SIZE)				\
			_l = NETMAP_BUF_SIZE;				\
	} while (0)
#endif


/*---------------------------------------------------------------*/
/*
 * Support routines to be used with the VALE switch
 */
int netmap_update_config(struct netmap_adapter *na);
int netmap_krings_create(struct netmap_adapter *na, u_int ntx, u_int nrx, u_int tailroom);
void netmap_krings_delete(struct netmap_adapter *na);

struct netmap_if *
netmap_do_regif(struct netmap_priv_d *priv, struct netmap_adapter *na,
	uint16_t ringid, int *err);



u_int nm_bound_var(u_int *v, u_int dflt, u_int lo, u_int hi, const char *msg);
int netmap_get_na(struct nmreq *nmr, struct netmap_adapter **na, int create);
int netmap_get_hw_na(struct ifnet *ifp, struct netmap_adapter **na);

#ifdef WITH_VALE
/*
 * The following bridge-related interfaces are used by other kernel modules
 * In the version that only supports unicast or broadcast, the lookup
 * function can return 0 .. NM_BDG_MAXPORTS-1 for regular ports,
 * NM_BDG_MAXPORTS for broadcast, NM_BDG_MAXPORTS+1 for unknown.
 * XXX in practice "unknown" might be handled same as broadcast.
 */
typedef u_int (*bdg_lookup_fn_t)(char *buf, u_int len,
		uint8_t *ring_nr, struct netmap_vp_adapter *);
u_int netmap_bdg_learning(char *, u_int, uint8_t *,
		struct netmap_vp_adapter *);

#define	NM_BDG_MAXPORTS		254	/* up to 254 */
#define	NM_BDG_BROADCAST	NM_BDG_MAXPORTS
#define	NM_BDG_NOPORT		(NM_BDG_MAXPORTS+1)

#define	NM_NAME			"vale"	/* prefix for bridge port name */


/* these are redefined in case of no VALE support */
int netmap_get_bdg_na(struct nmreq *nmr, struct netmap_adapter **na, int create);
void netmap_init_bridges(void);
int netmap_bdg_ctl(struct nmreq *nmr, bdg_lookup_fn_t func);

#else /* !WITH_VALE */
#define	netmap_get_bdg_na(_1, _2, _3)	0
#define netmap_init_bridges(_1)
#define	netmap_bdg_ctl(_1, _2)	EINVAL
#endif /* !WITH_VALE */

/* Various prototypes */
int netmap_poll(struct cdev *dev, int events, struct thread *td);


int netmap_init(void);
void netmap_fini(void);
int netmap_get_memory(struct netmap_priv_d* p);
void netmap_dtor(void *data);
int netmap_dtor_locked(struct netmap_priv_d *priv);

int netmap_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td);

/* netmap_adapter creation/destruction */
#define NM_IFPNAME(ifp) ((ifp) ? (ifp)->if_xname : "zombie")
#define NM_DEBUG_PUTGET 1

#ifdef NM_DEBUG_PUTGET

#define NM_DBG(f) __##f

void __netmap_adapter_get(struct netmap_adapter *na);

#define netmap_adapter_get(na) 				\
	do {						\
		struct netmap_adapter *__na = na;	\
		D("getting %p:%s (%d)", __na, NM_IFPNAME(__na->ifp), __na->na_refcount);	\
		__netmap_adapter_get(__na);		\
	} while (0)

int __netmap_adapter_put(struct netmap_adapter *na);

#define netmap_adapter_put(na)				\
	do {						\
		struct netmap_adapter *__na = na;	\
		D("putting %p:%s (%d)", __na, NM_IFPNAME(__na->ifp), __na->na_refcount);	\
		__netmap_adapter_put(__na);		\
	} while (0)

#else /* !NM_DEBUG_PUTGET */

#define NM_DBG(f) f
void netmap_adapter_get(struct netmap_adapter *na);
int netmap_adapter_put(struct netmap_adapter *na);

#endif /* !NM_DEBUG_PUTGET */


extern u_int netmap_buf_size;
#define NETMAP_BUF_SIZE	netmap_buf_size	// XXX remove
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

extern int netmap_txsync_retry;
extern int netmap_generic_mit;
extern int netmap_generic_ringsize;

/*
 * NA returns a pointer to the struct netmap adapter from the ifp,
 * WNA is used to write it.
 */
#ifndef WNA
#define	WNA(_ifp)	(_ifp)->if_pspare[0]
#endif
#define	NA(_ifp)	((struct netmap_adapter *)WNA(_ifp))

/*
 * Macros to determine if an interface is netmap capable or netmap enabled.
 * See the magic field in struct netmap_adapter.
 */
#ifdef __FreeBSD__
/*
 * on FreeBSD just use if_capabilities and if_capenable.
 */
#define NETMAP_CAPABLE(ifp)	(NA(ifp) &&		\
	(ifp)->if_capabilities & IFCAP_NETMAP )

#define	NETMAP_SET_CAPABLE(ifp)				\
	(ifp)->if_capabilities |= IFCAP_NETMAP

#else	/* linux */

/*
 * on linux:
 * we check if NA(ifp) is set and its first element has a related
 * magic value. The capenable is within the struct netmap_adapter.
 */
#define	NETMAP_MAGIC	0x52697a7a

#define NETMAP_CAPABLE(ifp)	(NA(ifp) &&		\
	((uint32_t)(uintptr_t)NA(ifp) ^ NA(ifp)->magic) == NETMAP_MAGIC )

#define	NETMAP_SET_CAPABLE(ifp)				\
	NA(ifp)->magic = ((uint32_t)(uintptr_t)NA(ifp)) ^ NETMAP_MAGIC

#endif	/* linux */

#ifdef __FreeBSD__

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

#else /* linux */

/*
 * XXX How do we redefine these functions:
 *
 * on linux we need
 *	dma_map_single(&pdev->dev, virt_addr, len, direction)
 *	dma_unmap_single(&adapter->pdev->dev, phys_addr, len, direction
 * The len can be implicit (on netmap it is NETMAP_BUF_SIZE)
 * unfortunately the direction is not, so we need to change
 * something to have a cross API
 */
#define netmap_load_map(_t, _m, _b)
#define netmap_reload_map(_t, _m, _b)
#if 0
	struct e1000_buffer *buffer_info =  &tx_ring->buffer_info[l];
	/* set time_stamp *before* dma to help avoid a possible race */
	buffer_info->time_stamp = jiffies;
	buffer_info->mapped_as_page = false;
	buffer_info->length = len;
	//buffer_info->next_to_watch = l;
	/* reload dma map */
	dma_unmap_single(&adapter->pdev->dev, buffer_info->dma,
			NETMAP_BUF_SIZE, DMA_TO_DEVICE);
	buffer_info->dma = dma_map_single(&adapter->pdev->dev,
			addr, NETMAP_BUF_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(&adapter->pdev->dev, buffer_info->dma)) {
		D("dma mapping error");
		/* goto dma_error; See e1000_put_txbuf() */
		/* XXX reset */
	}
	tx_desc->buffer_addr = htole64(buffer_info->dma); //XXX

#endif

/*
 * The bus_dmamap_sync() can be one of wmb() or rmb() depending on direction.
 */
#define bus_dmamap_sync(_a, _b, _c)

#endif /* linux */


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


/* Entries of the look-up table. */
struct lut_entry {
	void *vaddr;		/* virtual address. */
	vm_paddr_t paddr;	/* physical address. */
};

struct netmap_obj_pool;
extern struct lut_entry *netmap_buffer_lut;
#define NMB_VA(i)	(netmap_buffer_lut[i].vaddr)
#define NMB_PA(i)	(netmap_buffer_lut[i].paddr)

/*
 * NMB return the virtual address of a buffer (buffer 0 on bad index)
 * PNMB also fills the physical address
 */
static inline void *
NMB(struct netmap_slot *slot)
{
	uint32_t i = slot->buf_idx;
	return (unlikely(i >= netmap_total_buffers)) ?  NMB_VA(0) : NMB_VA(i);
}

static inline void *
PNMB(struct netmap_slot *slot, uint64_t *pp)
{
	uint32_t i = slot->buf_idx;
	void *ret = (i >= netmap_total_buffers) ? NMB_VA(0) : NMB_VA(i);

	*pp = (i >= netmap_total_buffers) ? NMB_PA(0) : NMB_PA(i);
	return ret;
}

/* Generic version of NMB, which uses device-specific memory. */
static inline void *
BDG_NMB(struct netmap_adapter *na, struct netmap_slot *slot)
{
	struct lut_entry *lut = na->na_lut;
	uint32_t i = slot->buf_idx;
	return (unlikely(i >= na->na_lut_objtotal)) ?
		lut[0].vaddr : lut[i].vaddr;
}

/* default functions to handle rx/tx interrupts */
int netmap_rx_irq(struct ifnet *, u_int, u_int *);
#define netmap_tx_irq(_n, _q) netmap_rx_irq(_n, _q, NULL)
void netmap_common_irq(struct ifnet *, u_int, u_int *work_done);


void netmap_txsync_to_host(struct netmap_adapter *na);
void netmap_disable_all_rings(struct ifnet *);
void netmap_enable_all_rings(struct ifnet *);
void netmap_disable_ring(struct netmap_kring *kr);


/* Structure associated to each thread which registered an interface.
 *
 * The first 4 fields of this structure are written by NIOCREGIF and
 * read by poll() and NIOC?XSYNC.
 * There is low contention among writers (actually, a correct user program
 * should have no contention among writers) and among writers and readers,
 * so we use a single global lock to protect the structure initialization.
 * Since initialization involves the allocation of memory, we reuse the memory
 * allocator lock.
 * Read access to the structure is lock free. Readers must check that
 * np_nifp is not NULL before using the other fields.
 * If np_nifp is NULL initialization has not been performed, so they should
 * return an error to userlevel.
 *
 * The ref_done field is used to regulate access to the refcount in the
 * memory allocator. The refcount must be incremented at most once for
 * each open("/dev/netmap"). The increment is performed by the first
 * function that calls netmap_get_memory() (currently called by
 * mmap(), NIOCGINFO and NIOCREGIF).
 * If the refcount is incremented, it is then decremented when the
 * private structure is destroyed.
 */
struct netmap_priv_d {
	struct netmap_if * volatile np_nifp;	/* netmap if descriptor. */

	struct netmap_adapter	*np_na;
	int		        np_ringid;	/* from the ioctl */
	u_int		        np_qfirst, np_qlast;	/* range of rings to scan */
	uint16_t	        np_txpoll;

	struct netmap_mem_d     *np_mref;	/* use with NMG_LOCK held */
	/* np_refcount is only used on FreeBSD */
	int		        np_refcount;	/* use with NMG_LOCK held */
};


/*
 * generic netmap emulation for devices that do not have
 * native netmap support.
 * XXX generic_netmap_register() is only exported to implement
 *	nma_is_generic().
 */
int generic_netmap_register(struct netmap_adapter *na, int enable);
int generic_netmap_attach(struct ifnet *ifp);

int netmap_catch_rx(struct netmap_adapter *na, int intercept);
void generic_rx_handler(struct ifnet *ifp, struct mbuf *m);;
void netmap_catch_packet_steering(struct netmap_generic_adapter *na, int enable);
int generic_xmit_frame(struct ifnet *ifp, struct mbuf *m, void *addr, u_int len, u_int ring_nr);
int generic_find_num_desc(struct ifnet *ifp, u_int *tx, u_int *rx);
void generic_find_num_queues(struct ifnet *ifp, u_int *txq, u_int *rxq);

static __inline int
nma_is_generic(struct netmap_adapter *na)
{
	return na->nm_register == generic_netmap_register;
}

/*
 * netmap_mitigation API. This is used by the generic adapter
 * to reduce the number of interrupt requests/selwakeup
 * to clients on incoming packets.
 */
void netmap_mitigation_init(struct netmap_generic_adapter *na);
void netmap_mitigation_start(struct netmap_generic_adapter *na);
void netmap_mitigation_restart(struct netmap_generic_adapter *na);
int netmap_mitigation_active(struct netmap_generic_adapter *na);
void netmap_mitigation_cleanup(struct netmap_generic_adapter *na);

// int generic_timer_handler(struct hrtimer *t);

#endif /* _NET_NETMAP_KERN_H_ */
