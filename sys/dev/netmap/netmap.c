/*
 * Copyright (C) 2011-2013 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
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
 * This module supports memory mapped access to network devices,
 * see netmap(4).
 *
 * The module uses a large, memory pool allocated by the kernel
 * and accessible as mmapped memory by multiple userspace threads/processes.
 * The memory pool contains packet buffers and "netmap rings",
 * i.e. user-accessible copies of the interface's queues.
 *
 * Access to the network card works like this:
 * 1. a process/thread issues one or more open() on /dev/netmap, to create
 *    select()able file descriptor on which events are reported.
 * 2. on each descriptor, the process issues an ioctl() to identify
 *    the interface that should report events to the file descriptor.
 * 3. on each descriptor, the process issues an mmap() request to
 *    map the shared memory region within the process' address space.
 *    The list of interesting queues is indicated by a location in
 *    the shared memory region.
 * 4. using the functions in the netmap(4) userspace API, a process
 *    can look up the occupation state of a queue, access memory buffers,
 *    and retrieve received packets or enqueue packets to transmit.
 * 5. using some ioctl()s the process can synchronize the userspace view
 *    of the queue with the actual status in the kernel. This includes both
 *    receiving the notification of new packets, and transmitting new
 *    packets on the output interface.
 * 6. select() or poll() can be used to wait for events on individual
 *    transmit or receive queues (or all queues for a given interface).
 *

		SYNCHRONIZATION (USER)

The netmap rings and data structures may be shared among multiple
user threads or even independent processes.
Any synchronization among those threads/processes is delegated
to the threads themselves. Only one thread at a time can be in
a system call on the same netmap ring. The OS does not enforce
this and only guarantees against system crashes in case of
invalid usage.

		LOCKING (INTERNAL)

Within the kernel, access to the netmap rings is protected as follows:

- a spinlock on each ring, to handle producer/consumer races on
  RX rings attached to the host stack (against multiple host
  threads writing from the host stack to the same ring),
  and on 'destination' rings attached to a VALE switch
  (i.e. RX rings in VALE ports, and TX rings in NIC/host ports)
  protecting multiple active senders for the same destination)

- an atomic variable to guarantee that there is at most one
  instance of *_*xsync() on the ring at any time.
  For rings connected to user file
  descriptors, an atomic_test_and_set() protects this, and the
  lock on the ring is not actually used.
  For NIC RX rings connected to a VALE switch, an atomic_test_and_set()
  is also used to prevent multiple executions (the driver might indeed
  already guarantee this).
  For NIC TX rings connected to a VALE switch, the lock arbitrates
  access to the queue (both when allocating buffers and when pushing
  them out).

- *xsync() should be protected against initializations of the card.
  On FreeBSD most devices have the reset routine protected by
  a RING lock (ixgbe, igb, em) or core lock (re). lem is missing
  the RING protection on rx_reset(), this should be added.

  On linux there is an external lock on the tx path, which probably
  also arbitrates access to the reset routine. XXX to be revised

- a per-interface core_lock protecting access from the host stack
  while interfaces may be detached from netmap mode.
  XXX there should be no need for this lock if we detach the interfaces
  only while they are down.


--- VALE SWITCH ---

NMG_LOCK() serializes all modifications to switches and ports.
A switch cannot be deleted until all ports are gone.

For each switch, an SX lock (RWlock on linux) protects
deletion of ports. When configuring or deleting a new port, the
lock is acquired in exclusive mode (after holding NMG_LOCK).
When forwarding, the lock is acquired in shared mode (without NMG_LOCK).
The lock is held throughout the entire forwarding cycle,
during which the thread may incur in a page fault.
Hence it is important that sleepable shared locks are used.

On the rx ring, the per-port lock is grabbed initially to reserve
a number of slot in the ring, then the lock is released,
packets are copied from source to destination, and then
the lock is acquired again and the receive ring is updated.
(A similar thing is done on the tx ring for NIC and host stack
ports attached to the switch)

 */

/*
 * OS-specific code that is used only within this file.
 * Other OS-specific code that must be accessed by drivers
 * is present in netmap_kern.h
 */

#if defined(__FreeBSD__)
#include <sys/cdefs.h> /* prerequisite */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/conf.h>	/* cdevsw struct, UID, GID */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/rwlock.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/bpf.h>		/* BIOCIMMEDIATE */
#include <machine/bus.h>	/* bus_dmamap_* */
#include <sys/endian.h>
#include <sys/refcount.h>


/* reduce conditional code */
#define init_waitqueue_head(x)	// only needed in linux



#elif defined(linux)

#include "bsd_glue.h"



#elif defined(__APPLE__)

#warning OSX support is only partial
#include "osx_glue.h"

#else

#error	Unsupported platform

#endif /* unsupported */

/*
 * common headers
 */
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>


MALLOC_DEFINE(M_NETMAP, "netmap", "Network memory map");

/*
 * The following variables are used by the drivers and replicate
 * fields in the global memory pool. They only refer to buffers
 * used by physical interfaces.
 */
u_int netmap_total_buffers;
u_int netmap_buf_size;
char *netmap_buffer_base;	/* also address of an invalid buffer */

/* user-controlled variables */
int netmap_verbose;

static int netmap_no_timestamp; /* don't timestamp on rxsync */

SYSCTL_NODE(_dev, OID_AUTO, netmap, CTLFLAG_RW, 0, "Netmap args");
SYSCTL_INT(_dev_netmap, OID_AUTO, verbose,
    CTLFLAG_RW, &netmap_verbose, 0, "Verbose mode");
SYSCTL_INT(_dev_netmap, OID_AUTO, no_timestamp,
    CTLFLAG_RW, &netmap_no_timestamp, 0, "no_timestamp");
int netmap_mitigate = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, mitigate, CTLFLAG_RW, &netmap_mitigate, 0, "");
int netmap_no_pendintr = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, no_pendintr,
    CTLFLAG_RW, &netmap_no_pendintr, 0, "Always look for new received packets.");
int netmap_txsync_retry = 2;
SYSCTL_INT(_dev_netmap, OID_AUTO, txsync_retry, CTLFLAG_RW,
    &netmap_txsync_retry, 0 , "Number of txsync loops in bridge's flush.");

int netmap_flags = 0;	/* debug flags */
int netmap_fwd = 0;	/* force transparent mode */
int netmap_mmap_unreg = 0; /* allow mmap of unregistered fds */

/*
 * netmap_admode selects the netmap mode to use.
 * Invalid values are reset to NETMAP_ADMODE_BEST
 */
enum { NETMAP_ADMODE_BEST = 0,	/* use native, fallback to generic */
	NETMAP_ADMODE_NATIVE,	/* either native or none */
	NETMAP_ADMODE_GENERIC,	/* force generic */
	NETMAP_ADMODE_LAST };
#define NETMAP_ADMODE_NATIVE        1  /* Force native netmap adapter. */
#define NETMAP_ADMODE_GENERIC       2  /* Force generic netmap adapter. */
#define NETMAP_ADMODE_BEST          0  /* Priority to native netmap adapter. */
static int netmap_admode = NETMAP_ADMODE_BEST;

int netmap_generic_mit = 100*1000;   /* Generic mitigation interval in nanoseconds. */
int netmap_generic_ringsize = 1024;   /* Generic ringsize. */

SYSCTL_INT(_dev_netmap, OID_AUTO, flags, CTLFLAG_RW, &netmap_flags, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, fwd, CTLFLAG_RW, &netmap_fwd, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, mmap_unreg, CTLFLAG_RW, &netmap_mmap_unreg, 0, "");
SYSCTL_INT(_dev_netmap, OID_AUTO, admode, CTLFLAG_RW, &netmap_admode, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, generic_mit, CTLFLAG_RW, &netmap_generic_mit, 0 , "");
SYSCTL_INT(_dev_netmap, OID_AUTO, generic_ringsize, CTLFLAG_RW, &netmap_generic_ringsize, 0 , "");

NMG_LOCK_T	netmap_global_lock;


static void
nm_kr_get(struct netmap_kring *kr)
{
	while (NM_ATOMIC_TEST_AND_SET(&kr->nr_busy))
		tsleep(kr, 0, "NM_KR_GET", 4);
}


void
netmap_disable_ring(struct netmap_kring *kr)
{
	kr->nkr_stopped = 1;
	nm_kr_get(kr);
	mtx_lock(&kr->q_lock);
	mtx_unlock(&kr->q_lock);
	nm_kr_put(kr);
}


static void
netmap_set_all_rings(struct ifnet *ifp, int stopped)
{
	struct netmap_adapter *na;
	int i;

	if (!(ifp->if_capenable & IFCAP_NETMAP))
		return;

	na = NA(ifp);

	for (i = 0; i <= na->num_tx_rings; i++) {
		if (stopped)
			netmap_disable_ring(na->tx_rings + i);
		else
			na->tx_rings[i].nkr_stopped = 0;
		na->nm_notify(na, i, NR_TX, NAF_DISABLE_NOTIFY |
			(i == na->num_tx_rings ? NAF_GLOBAL_NOTIFY: 0));
	}

	for (i = 0; i <= na->num_rx_rings; i++) {
		if (stopped)
			netmap_disable_ring(na->rx_rings + i);
		else
			na->rx_rings[i].nkr_stopped = 0;
		na->nm_notify(na, i, NR_RX, NAF_DISABLE_NOTIFY |
			(i == na->num_rx_rings ? NAF_GLOBAL_NOTIFY: 0));
	}
}


void
netmap_disable_all_rings(struct ifnet *ifp)
{
	netmap_set_all_rings(ifp, 1 /* stopped */);
}


void
netmap_enable_all_rings(struct ifnet *ifp)
{
	netmap_set_all_rings(ifp, 0 /* enabled */);
}


/*
 * generic bound_checking function
 */
u_int
nm_bound_var(u_int *v, u_int dflt, u_int lo, u_int hi, const char *msg)
{
	u_int oldv = *v;
	const char *op = NULL;

	if (dflt < lo)
		dflt = lo;
	if (dflt > hi)
		dflt = hi;
	if (oldv < lo) {
		*v = dflt;
		op = "Bump";
	} else if (oldv > hi) {
		*v = hi;
		op = "Clamp";
	}
	if (op && msg)
		printf("%s %s to %d (was %d)\n", op, msg, *v, oldv);
	return *v;
}


/*
 * packet-dump function, user-supplied or static buffer.
 * The destination buffer must be at least 30+4*len
 */
const char *
nm_dump_buf(char *p, int len, int lim, char *dst)
{
	static char _dst[8192];
	int i, j, i0;
	static char hex[] ="0123456789abcdef";
	char *o;	/* output position */

#define P_HI(x)	hex[((x) & 0xf0)>>4]
#define P_LO(x)	hex[((x) & 0xf)]
#define P_C(x)	((x) >= 0x20 && (x) <= 0x7e ? (x) : '.')
	if (!dst)
		dst = _dst;
	if (lim <= 0 || lim > len)
		lim = len;
	o = dst;
	sprintf(o, "buf 0x%p len %d lim %d\n", p, len, lim);
	o += strlen(o);
	/* hexdump routine */
	for (i = 0; i < lim; ) {
		sprintf(o, "%5d: ", i);
		o += strlen(o);
		memset(o, ' ', 48);
		i0 = i;
		for (j=0; j < 16 && i < lim; i++, j++) {
			o[j*3] = P_HI(p[i]);
			o[j*3+1] = P_LO(p[i]);
		}
		i = i0;
		for (j=0; j < 16 && i < lim; i++, j++)
			o[j + 48] = P_C(p[i]);
		o[j+48] = '\n';
		o += j+49;
	}
	*o = '\0';
#undef P_HI
#undef P_LO
#undef P_C
	return dst;
}



/*
 * Fetch configuration from the device, to cope with dynamic
 * reconfigurations after loading the module.
 */
int
netmap_update_config(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;
	u_int txr, txd, rxr, rxd;

	txr = txd = rxr = rxd = 0;
	if (na->nm_config) {
		na->nm_config(na, &txr, &txd, &rxr, &rxd);
	} else {
		/* take whatever we had at init time */
		txr = na->num_tx_rings;
		txd = na->num_tx_desc;
		rxr = na->num_rx_rings;
		rxd = na->num_rx_desc;
	}

	if (na->num_tx_rings == txr && na->num_tx_desc == txd &&
	    na->num_rx_rings == rxr && na->num_rx_desc == rxd)
		return 0; /* nothing changed */
	if (netmap_verbose || na->active_fds > 0) {
		D("stored config %s: txring %d x %d, rxring %d x %d",
			NM_IFPNAME(ifp),
			na->num_tx_rings, na->num_tx_desc,
			na->num_rx_rings, na->num_rx_desc);
		D("new config %s: txring %d x %d, rxring %d x %d",
			NM_IFPNAME(ifp), txr, txd, rxr, rxd);
	}
	if (na->active_fds == 0) {
		D("configuration changed (but fine)");
		na->num_tx_rings = txr;
		na->num_tx_desc = txd;
		na->num_rx_rings = rxr;
		na->num_rx_desc = rxd;
		return 0;
	}
	D("configuration changed while active, this is bad...");
	return 1;
}


int
netmap_krings_create(struct netmap_adapter *na, u_int ntx, u_int nrx, u_int tailroom)
{
	u_int i, len, ndesc;
	struct netmap_kring *kring;

	len = (ntx + nrx) * sizeof(struct netmap_kring) + tailroom;

	na->tx_rings = malloc((size_t)len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (na->tx_rings == NULL) {
		D("Cannot allocate krings");
		return ENOMEM;
	}
	na->rx_rings = na->tx_rings + ntx;

	ndesc = na->num_tx_desc;
	for (i = 0; i < ntx; i++) { /* Transmit rings */
		kring = &na->tx_rings[i];
		bzero(kring, sizeof(*kring));
		kring->na = na;
		kring->nkr_num_slots = ndesc;
		/*
		 * IMPORTANT:
		 * Always keep one slot empty, so we can detect new
		 * transmissions comparing cur and nr_hwcur (they are
		 * the same only if there are no new transmissions).
		 */
		kring->nr_hwavail = ndesc - 1;
		mtx_init(&kring->q_lock, "nm_txq_lock", NULL, MTX_DEF);
		init_waitqueue_head(&kring->si);
	}

	ndesc = na->num_rx_desc;
	for (i = 0; i < nrx; i++) { /* Receive rings */
		kring = &na->rx_rings[i];
		bzero(kring, sizeof(*kring));
		kring->na = na;
		kring->nkr_num_slots = ndesc;
		mtx_init(&kring->q_lock, "nm_rxq_lock", NULL, MTX_DEF);
		init_waitqueue_head(&kring->si);
	}
	init_waitqueue_head(&na->tx_si);
	init_waitqueue_head(&na->rx_si);

	na->tailroom = na->rx_rings + nrx;

	return 0;

}


void
netmap_krings_delete(struct netmap_adapter *na)
{
	int i;

	for (i = 0; i < na->num_tx_rings + 1; i++) {
		mtx_destroy(&na->tx_rings[i].q_lock);
	}
	for (i = 0; i < na->num_rx_rings + 1; i++) {
		mtx_destroy(&na->rx_rings[i].q_lock);
	}
	free(na->tx_rings, M_DEVBUF);
	na->tx_rings = na->rx_rings = na->tailroom = NULL;
}


static struct netmap_if*
netmap_if_new(const char *ifname, struct netmap_adapter *na)
{
	struct netmap_if *nifp;

	if (netmap_update_config(na)) {
		/* configuration mismatch, report and fail */
		return NULL;
	}

	if (na->active_fds)
		goto final;

	if (na->nm_krings_create(na))
		goto cleanup;

	if (netmap_mem_rings_create(na))
		goto cleanup;

final:

	nifp = netmap_mem_if_new(ifname, na);
	if (nifp == NULL)
		goto cleanup;

	return (nifp);

cleanup:

	if (na->active_fds == 0) {
		netmap_mem_rings_delete(na);
		na->nm_krings_delete(na);
	}

	return NULL;
}


/* grab a reference to the memory allocator, if we don't have one already.  The
 * reference is taken from the netmap_adapter registered with the priv.
 *
 */
static int
netmap_get_memory_locked(struct netmap_priv_d* p)
{
	struct netmap_mem_d *nmd;
	int error = 0;

	if (p->np_na == NULL) {
		if (!netmap_mmap_unreg)
			return ENODEV;
		/* for compatibility with older versions of the API
 		 * we use the global allocator when no interface has been
 		 * registered
 		 */
		nmd = &nm_mem;
	} else {
		nmd = p->np_na->nm_mem;
	}
	if (p->np_mref == NULL) {
		error = netmap_mem_finalize(nmd);
		if (!error)
			p->np_mref = nmd;
	} else if (p->np_mref != nmd) {
		/* a virtual port has been registered, but previous
 		 * syscalls already used the global allocator.
 		 * We cannot continue
 		 */
		error = ENODEV;
	}
	return error;
}


int
netmap_get_memory(struct netmap_priv_d* p)
{
	int error;
	NMG_LOCK();
	error = netmap_get_memory_locked(p);
	NMG_UNLOCK();
	return error;
}


static int
netmap_have_memory_locked(struct netmap_priv_d* p)
{
	return p->np_mref != NULL;
}


static void
netmap_drop_memory_locked(struct netmap_priv_d* p)
{
	if (p->np_mref) {
		netmap_mem_deref(p->np_mref);
		p->np_mref = NULL;
	}
}


/*
 * File descriptor's private data destructor.
 *
 * Call nm_register(ifp,0) to stop netmap mode on the interface and
 * revert to normal operation. We expect that np_na->ifp has not gone.
 * The second argument is the nifp to work on. In some cases it is
 * not attached yet to the netmap_priv_d so we need to pass it as
 * a separate argument.
 */
/* call with NMG_LOCK held */
static void
netmap_do_unregif(struct netmap_priv_d *priv, struct netmap_if *nifp)
{
	struct netmap_adapter *na = priv->np_na;
	struct ifnet *ifp = na->ifp;

	NMG_LOCK_ASSERT();
	na->active_fds--;
	if (na->active_fds <= 0) {	/* last instance */

		if (netmap_verbose)
			D("deleting last instance for %s", NM_IFPNAME(ifp));
		/*
		 * (TO CHECK) This function is only called
		 * when the last reference to this file descriptor goes
		 * away. This means we cannot have any pending poll()
		 * or interrupt routine operating on the structure.
		 * XXX The file may be closed in a thread while
		 * another thread is using it.
		 * Linux keeps the file opened until the last reference
		 * by any outstanding ioctl/poll or mmap is gone.
		 * FreeBSD does not track mmap()s (but we do) and
		 * wakes up any sleeping poll(). Need to check what
		 * happens if the close() occurs while a concurrent
		 * syscall is running.
		 */
		if (ifp)
			na->nm_register(na, 0); /* off, clear flags */
		/* Wake up any sleeping threads. netmap_poll will
		 * then return POLLERR
		 * XXX The wake up now must happen during *_down(), when
		 * we order all activities to stop. -gl
		 */
		/* XXX kqueue(9) needed; these will mirror knlist_init. */
		/* knlist_destroy(&na->tx_si.si_note); */
		/* knlist_destroy(&na->rx_si.si_note); */

		/* delete rings and buffers */
		netmap_mem_rings_delete(na);
		na->nm_krings_delete(na);
	}
	/* delete the nifp */
	netmap_mem_if_delete(na, nifp);
}


/*
 * returns 1 if this is the last instance and we can free priv
 */
int
netmap_dtor_locked(struct netmap_priv_d *priv)
{
	struct netmap_adapter *na = priv->np_na;

#ifdef __FreeBSD__
	/*
	 * np_refcount is the number of active mmaps on
	 * this file descriptor
	 */
	if (--priv->np_refcount > 0) {
		return 0;
	}
#endif /* __FreeBSD__ */
	if (!na) {
	    return 1; //XXX is it correct?
	}
	netmap_do_unregif(priv, priv->np_nifp);
	priv->np_nifp = NULL;
	netmap_drop_memory_locked(priv);
	if (priv->np_na) {
		netmap_adapter_put(na);
		priv->np_na = NULL;
	}
	return 1;
}


void
netmap_dtor(void *data)
{
	struct netmap_priv_d *priv = data;
	int last_instance;

	NMG_LOCK();
	last_instance = netmap_dtor_locked(priv);
	NMG_UNLOCK();
	if (last_instance) {
		bzero(priv, sizeof(*priv));	/* for safety */
		free(priv, M_DEVBUF);
	}
}




/*
 * Handlers for synchronization of the queues from/to the host.
 * Netmap has two operating modes:
 * - in the default mode, the rings connected to the host stack are
 *   just another ring pair managed by userspace;
 * - in transparent mode (XXX to be defined) incoming packets
 *   (from the host or the NIC) are marked as NS_FORWARD upon
 *   arrival, and the user application has a chance to reset the
 *   flag for packets that should be dropped.
 *   On the RXSYNC or poll(), packets in RX rings between
 *   kring->nr_kcur and ring->cur with NS_FORWARD still set are moved
 *   to the other side.
 * The transfer NIC --> host is relatively easy, just encapsulate
 * into mbufs and we are done. The host --> NIC side is slightly
 * harder because there might not be room in the tx ring so it
 * might take a while before releasing the buffer.
 */


/*
 * pass a chain of buffers to the host stack as coming from 'dst'
 */
static void
netmap_send_up(struct ifnet *dst, struct mbq *q)
{
	struct mbuf *m;

	/* send packets up, outside the lock */
	while ((m = mbq_dequeue(q)) != NULL) {
		if (netmap_verbose & NM_VERB_HOST)
			D("sending up pkt %p size %d", m, MBUF_LEN(m));
		NM_SEND_UP(dst, m);
	}
	mbq_destroy(q);
}


/*
 * put a copy of the buffers marked NS_FORWARD into an mbuf chain.
 * Run from hwcur to cur - reserved
 */
static void
netmap_grab_packets(struct netmap_kring *kring, struct mbq *q, int force)
{
	/* Take packets from hwcur to cur-reserved and pass them up.
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 * XXX handle reserved
	 */
	u_int lim = kring->nkr_num_slots - 1;
	struct mbuf *m;
	u_int k = kring->ring->cur, n = kring->ring->reserved;
	struct netmap_adapter *na = kring->na;

	/* compute the final position, ring->cur - ring->reserved */
	if (n > 0) {
		if (k < n)
			k += kring->nkr_num_slots;
		k += n;
	}
	for (n = kring->nr_hwcur; n != k;) {
		struct netmap_slot *slot = &kring->ring->slot[n];

		n = nm_next(n, lim);
		if ((slot->flags & NS_FORWARD) == 0 && !force)
			continue;
		if (slot->len < 14 || slot->len > NETMAP_BDG_BUF_SIZE(na->nm_mem)) {
			D("bad pkt at %d len %d", n, slot->len);
			continue;
		}
		slot->flags &= ~NS_FORWARD; // XXX needed ?
		/* XXX adapt to the case of a multisegment packet */
		m = m_devget(BDG_NMB(na, slot), slot->len, 0, na->ifp, NULL);

		if (m == NULL)
			break;
		mbq_enqueue(q, m);
	}
}


/*
 * The host ring has packets from nr_hwcur to (cur - reserved)
 * to be sent down to the NIC.
 * We need to use the queue lock on the source (host RX ring)
 * to protect against netmap_transmit.
 * If the user is well behaved we do not need to acquire locks
 * on the destination(s),
 * so we only need to make sure that there are no panics because
 * of user errors.
 * XXX verify
 *
 * We scan the tx rings, which have just been
 * flushed so nr_hwcur == cur. Pushing packets down means
 * increment cur and decrement avail.
 * XXX to be verified
 */
static void
netmap_sw_to_nic(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_kring *k1 = &na->tx_rings[0];
	u_int i, howmany, src_lim, dst_lim;

	/* XXX we should also check that the carrier is on */
	if (kring->nkr_stopped)
		return;

	mtx_lock(&kring->q_lock);

	if (kring->nkr_stopped)
		goto out;

	howmany = kring->nr_hwavail;	/* XXX otherwise cur - reserved - nr_hwcur */

	src_lim = kring->nkr_num_slots - 1;
	for (i = 0; howmany > 0 && i < na->num_tx_rings; i++, k1++) {
		ND("%d packets left to ring %d (space %d)", howmany, i, k1->nr_hwavail);
		dst_lim = k1->nkr_num_slots - 1;
		while (howmany > 0 && k1->ring->avail > 0) {
			struct netmap_slot *src, *dst, tmp;
			src = &kring->ring->slot[kring->nr_hwcur];
			dst = &k1->ring->slot[k1->ring->cur];
			tmp = *src;
			src->buf_idx = dst->buf_idx;
			src->flags = NS_BUF_CHANGED;

			dst->buf_idx = tmp.buf_idx;
			dst->len = tmp.len;
			dst->flags = NS_BUF_CHANGED;
			ND("out len %d buf %d from %d to %d",
				dst->len, dst->buf_idx,
				kring->nr_hwcur, k1->ring->cur);

			kring->nr_hwcur = nm_next(kring->nr_hwcur, src_lim);
			howmany--;
			kring->nr_hwavail--;
			k1->ring->cur = nm_next(k1->ring->cur, dst_lim);
			k1->ring->avail--;
		}
		kring->ring->cur = kring->nr_hwcur; // XXX
		k1++; // XXX why?
	}
out:
	mtx_unlock(&kring->q_lock);
}


/*
 * netmap_txsync_to_host() passes packets up. We are called from a
 * system call in user process context, and the only contention
 * can be among multiple user threads erroneously calling
 * this routine concurrently.
 */
void
netmap_txsync_to_host(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->tx_rings[na->num_tx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int k, lim = kring->nkr_num_slots - 1;
	struct mbq q;
	int error;

	error = nm_kr_tryget(kring);
	if (error) {
		if (error == NM_KR_BUSY)
			D("ring %p busy (user error)", kring);
		return;
	}
	k = ring->cur;
	if (k > lim) {
		D("invalid ring index in stack TX kring %p", kring);
		netmap_ring_reinit(kring);
		nm_kr_put(kring);
		return;
	}

	/* Take packets from hwcur to cur and pass them up.
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 */
	mbq_init(&q);
	netmap_grab_packets(kring, &q, 1);
	kring->nr_hwcur = k;
	kring->nr_hwavail = ring->avail = lim;

	nm_kr_put(kring);
	netmap_send_up(na->ifp, &q);
}


/*
 * rxsync backend for packets coming from the host stack.
 * They have been put in the queue by netmap_transmit() so we
 * need to protect access to the kring using a lock.
 *
 * This routine also does the selrecord if called from the poll handler
 * (we know because td != NULL).
 *
 * NOTE: on linux, selrecord() is defined as a macro and uses pwait
 *     as an additional hidden argument.
 */
static void
netmap_rxsync_from_host(struct netmap_adapter *na, struct thread *td, void *pwait)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int j, n, lim = kring->nkr_num_slots;
	u_int k = ring->cur, resvd = ring->reserved;

	(void)pwait;	/* disable unused warnings */

	if (kring->nkr_stopped) /* check a first time without lock */
		return;

	mtx_lock(&kring->q_lock);

	if (kring->nkr_stopped)  /* check again with lock held */
		goto unlock_out;

	if (k >= lim) {
		netmap_ring_reinit(kring);
		goto unlock_out;
	}
	/* new packets are already set in nr_hwavail */
	/* skip past packets that userspace has released */
	j = kring->nr_hwcur;
	if (resvd > 0) {
		if (resvd + ring->avail >= lim + 1) {
			D("XXX invalid reserve/avail %d %d", resvd, ring->avail);
			ring->reserved = resvd = 0; // XXX panic...
		}
		k = (k >= resvd) ? k - resvd : k + lim - resvd;
	}
	if (j != k) {
		n = k >= j ? k - j : k + lim - j;
		kring->nr_hwavail -= n;
		kring->nr_hwcur = k;
	}
	k = ring->avail = kring->nr_hwavail - resvd;
	if (k == 0 && td)
		selrecord(td, &kring->si);
	if (k && (netmap_verbose & NM_VERB_HOST))
		D("%d pkts from stack", k);
unlock_out:

	mtx_unlock(&kring->q_lock);
}


/* Get a netmap adapter for the port.
 *
 * If it is possible to satisfy the request, return 0
 * with *na containing the netmap adapter found.
 * Otherwise return an error code, with *na containing NULL.
 *
 * When the port is attached to a bridge, we always return
 * EBUSY.
 * Otherwise, if the port is already bound to a file descriptor,
 * then we unconditionally return the existing adapter into *na.
 * In all the other cases, we return (into *na) either native,
 * generic or NULL, according to the following table:
 *
 *					native_support
 * active_fds   dev.netmap.admode         YES     NO
 * -------------------------------------------------------
 *    >0              *                 NA(ifp) NA(ifp)
 *
 *     0        NETMAP_ADMODE_BEST      NATIVE  GENERIC
 *     0        NETMAP_ADMODE_NATIVE    NATIVE   NULL
 *     0        NETMAP_ADMODE_GENERIC   GENERIC GENERIC
 *
 */

int
netmap_get_hw_na(struct ifnet *ifp, struct netmap_adapter **na)
{
	/* generic support */
	int i = netmap_admode;	/* Take a snapshot. */
	int error = 0;
	struct netmap_adapter *prev_na;
	struct netmap_generic_adapter *gna;

	*na = NULL; /* default */

	/* reset in case of invalid value */
	if (i < NETMAP_ADMODE_BEST || i >= NETMAP_ADMODE_LAST)
		i = netmap_admode = NETMAP_ADMODE_BEST;

	if (NETMAP_CAPABLE(ifp)) {
		/* If an adapter already exists, but is
		 * attached to a vale port, we report that the
		 * port is busy.
		 */
		if (NETMAP_OWNED_BY_KERN(NA(ifp)))
			return EBUSY;

		/* If an adapter already exists, return it if
		 * there are active file descriptors or if
		 * netmap is not forced to use generic
		 * adapters.
		 */
		if (NA(ifp)->active_fds > 0 ||
				i != NETMAP_ADMODE_GENERIC) {
			*na = NA(ifp);
			return 0;
		}
	}

	/* If there isn't native support and netmap is not allowed
	 * to use generic adapters, we cannot satisfy the request.
	 */
	if (!NETMAP_CAPABLE(ifp) && i == NETMAP_ADMODE_NATIVE)
		return EINVAL;

	/* Otherwise, create a generic adapter and return it,
	 * saving the previously used netmap adapter, if any.
	 *
	 * Note that here 'prev_na', if not NULL, MUST be a
	 * native adapter, and CANNOT be a generic one. This is
	 * true because generic adapters are created on demand, and
	 * destroyed when not used anymore. Therefore, if the adapter
	 * currently attached to an interface 'ifp' is generic, it
	 * must be that
	 * (NA(ifp)->active_fds > 0 || NETMAP_OWNED_BY_KERN(NA(ifp))).
	 * Consequently, if NA(ifp) is generic, we will enter one of
	 * the branches above. This ensures that we never override
	 * a generic adapter with another generic adapter.
	 */
	prev_na = NA(ifp);
	error = generic_netmap_attach(ifp);
	if (error)
		return error;

	*na = NA(ifp);
	gna = (struct netmap_generic_adapter*)NA(ifp);
	gna->prev = prev_na; /* save old na */
	if (prev_na != NULL) {
		ifunit_ref(ifp->if_xname);
		// XXX add a refcount ?
		netmap_adapter_get(prev_na);
	}
	D("Created generic NA %p (prev %p)", gna, gna->prev);

	return 0;
}


/*
 * MUST BE CALLED UNDER NMG_LOCK()
 *
 * get a refcounted reference to an interface.
 * This is always called in the execution of an ioctl().
 *
 * Return ENXIO if the interface does not exist, EINVAL if netmap
 * is not supported by the interface.
 * If successful, hold a reference.
 *
 * When the NIC is attached to a bridge, reference is managed
 * at na->na_bdg_refcount using ADD/DROP_BDG_REF() as well as
 * virtual ports.  Hence, on the final DROP_BDG_REF(), the NIC
 * is detached from the bridge, then ifp's refcount is dropped (this
 * is equivalent to that ifp is destroyed in case of virtual ports.
 *
 * This function uses if_rele() when we want to prevent the NIC from
 * being detached from the bridge in error handling.  But once refcount
 * is acquired by this function, it must be released using nm_if_rele().
 */
int
netmap_get_na(struct nmreq *nmr, struct netmap_adapter **na, int create)
{
	struct ifnet *ifp;
	int error = 0;
	struct netmap_adapter *ret;

	*na = NULL;     /* default return value */

	/* first try to see if this is a bridge port. */
	NMG_LOCK_ASSERT();

	error = netmap_get_bdg_na(nmr, na, create);
	if (error || *na != NULL) /* valid match in netmap_get_bdg_na() */
		return error;

	ifp = ifunit_ref(nmr->nr_name);
	if (ifp == NULL) {
	        return ENXIO;
	}

	error = netmap_get_hw_na(ifp, &ret);
	if (error)
		goto out;

	if (ret != NULL) {
		/* Users cannot use the NIC attached to a bridge directly */
		if (NETMAP_OWNED_BY_KERN(ret)) {
			error = EINVAL;
			goto out;
		}
		error = 0;
		*na = ret;
		netmap_adapter_get(ret);
	}
out:
	if_rele(ifp);

	return error;
}


/*
 * validate parameters on entry for *_txsync()
 * Returns ring->cur if ok, or something >= kring->nkr_num_slots
 * in case of error. The extra argument is a pointer to
 * 'new_bufs'. XXX this may be deprecated at some point.
 *
 * Below is a correct configuration on input. ring->cur
 * must be in the region covered by kring->hwavail,
 * and ring->avail and kring->avail should end at the same slot.
 *
 *         +-hwcur
 *         |
 *         v<--hwres-->|<-----hwavail---->
 *   ------+------------------------------+-------- ring
 *                          |
 *                          |<---avail--->
 *                          +--cur
 *
 */
u_int
nm_txsync_prologue(struct netmap_kring *kring, u_int *new_slots)
{
	struct netmap_ring *ring = kring->ring;
	u_int cur = ring->cur; /* read only once */
	u_int avail = ring->avail; /* read only once */
	u_int n = kring->nkr_num_slots;
	u_int kstart, kend, a;

#if 1 /* kernel sanity checks */
	if (kring->nr_hwcur >= n ||
	    kring->nr_hwreserved >= n || kring->nr_hwavail >= n ||
	    kring->nr_hwreserved + kring->nr_hwavail >= n)
		goto error;
#endif /* kernel sanity checks */
	kstart = kring->nr_hwcur + kring->nr_hwreserved;
	if (kstart >= n)
		kstart -= n;
	kend = kstart + kring->nr_hwavail;
	/* user sanity checks. a is the expected avail */
	if (cur < kstart) {
		/* too low, but maybe wraparound */
		if (cur + n > kend)
			goto error;
		*new_slots = cur + n - kstart;
		a = kend - cur - n;
	} else {
		if (cur > kend)
			goto error;
		*new_slots = cur - kstart;
		a = kend - cur;
	}
	if (a != avail) {
		RD(5, "wrong but fixable avail have %d need %d",
			avail, a);
		ring->avail = avail = a;
	}
	return cur;

error:
	RD(5, "kring error: hwcur %d hwres %d hwavail %d cur %d av %d",
		kring->nr_hwcur,
		kring->nr_hwreserved, kring->nr_hwavail,
		cur, avail);
	return n;
}


/*
 * validate parameters on entry for *_rxsync()
 * Returns ring->cur - ring->reserved if ok,
 * or something >= kring->nkr_num_slots
 * in case of error. The extra argument is a pointer to
 * 'resvd'. XXX this may be deprecated at some point.
 *
 * Below is a correct configuration on input. ring->cur and
 * ring->reserved must be in the region covered by kring->hwavail,
 * and ring->avail and kring->avail should end at the same slot.
 *
 *            +-hwcur
 *            |
 *            v<-------hwavail---------->
 *   ---------+--------------------------+-------- ring
 *               |<--res-->|
 *                         |<---avail--->
 *                         +--cur
 *
 */
u_int
nm_rxsync_prologue(struct netmap_kring *kring, u_int *resvd)
{
	struct netmap_ring *ring = kring->ring;
	u_int cur = ring->cur; /* read only once */
	u_int avail = ring->avail; /* read only once */
	u_int res = ring->reserved; /* read only once */
	u_int n = kring->nkr_num_slots;
	u_int kend = kring->nr_hwcur + kring->nr_hwavail;
	u_int a;

#if 1 /* kernel sanity checks */
	if (kring->nr_hwcur >= n || kring->nr_hwavail >= n)
		goto error;
#endif /* kernel sanity checks */
	/* user sanity checks */
	if (res >= n)
		goto error;
	/* check that cur is valid, a is the expected value of avail */
	if (cur < kring->nr_hwcur) {
		/* too low, but maybe wraparound */
		if (cur + n > kend)
			goto error;
		a = kend - (cur + n);
	} else  {
		if (cur > kend)
			goto error;
		a = kend - cur;
	}
	if (a != avail) {
		RD(5, "wrong but fixable avail have %d need %d",
			avail, a);
		ring->avail = avail = a;
	}
	if (res != 0) {
		/* then repeat the check for cur + res */
		cur = (cur >= res) ? cur - res : n + cur - res;
		if (cur < kring->nr_hwcur) {
			/* too low, but maybe wraparound */
			if (cur + n > kend)
				goto error;
		} else if (cur > kend) {
			goto error;
		}
	}
	*resvd = res;
	return cur;

error:
	RD(5, "kring error: hwcur %d hwres %d hwavail %d cur %d av %d res %d",
		kring->nr_hwcur,
		kring->nr_hwreserved, kring->nr_hwavail,
		ring->cur, avail, res);
	return n;
}

/*
 * Error routine called when txsync/rxsync detects an error.
 * Can't do much more than resetting cur = hwcur, avail = hwavail.
 * Return 1 on reinit.
 *
 * This routine is only called by the upper half of the kernel.
 * It only reads hwcur (which is changed only by the upper half, too)
 * and hwavail (which may be changed by the lower half, but only on
 * a tx ring and only to increase it, so any error will be recovered
 * on the next call). For the above, we don't strictly need to call
 * it under lock.
 */
int
netmap_ring_reinit(struct netmap_kring *kring)
{
	struct netmap_ring *ring = kring->ring;
	u_int i, lim = kring->nkr_num_slots - 1;
	int errors = 0;

	// XXX KASSERT nm_kr_tryget
	RD(10, "called for %s", NM_IFPNAME(kring->na->ifp));
	if (ring->cur > lim)
		errors++;
	for (i = 0; i <= lim; i++) {
		u_int idx = ring->slot[i].buf_idx;
		u_int len = ring->slot[i].len;
		if (idx < 2 || idx >= netmap_total_buffers) {
			if (!errors++)
				D("bad buffer at slot %d idx %d len %d ", i, idx, len);
			ring->slot[i].buf_idx = 0;
			ring->slot[i].len = 0;
		} else if (len > NETMAP_BDG_BUF_SIZE(kring->na->nm_mem)) {
			ring->slot[i].len = 0;
			if (!errors++)
				D("bad len %d at slot %d idx %d",
					len, i, idx);
		}
	}
	if (errors) {
		int pos = kring - kring->na->tx_rings;
		int n = kring->na->num_tx_rings + 1;

		RD(10, "total %d errors", errors);
		errors++;
		RD(10, "%s %s[%d] reinit, cur %d -> %d avail %d -> %d",
			NM_IFPNAME(kring->na->ifp),
			pos < n ?  "TX" : "RX", pos < n ? pos : pos - n,
			ring->cur, kring->nr_hwcur,
			ring->avail, kring->nr_hwavail);
		ring->cur = kring->nr_hwcur;
		ring->avail = kring->nr_hwavail;
	}
	return (errors ? 1 : 0);
}


/*
 * Set the ring ID. For devices with a single queue, a request
 * for all rings is the same as a single ring.
 */
static int
netmap_set_ringid(struct netmap_priv_d *priv, u_int ringid)
{
	struct netmap_adapter *na = priv->np_na;
	struct ifnet *ifp = na->ifp;
	u_int i = ringid & NETMAP_RING_MASK;
	/* initially (np_qfirst == np_qlast) we don't want to lock */
	u_int lim = na->num_rx_rings;

	if (na->num_tx_rings > lim)
		lim = na->num_tx_rings;
	if ( (ringid & NETMAP_HW_RING) && i >= lim) {
		D("invalid ring id %d", i);
		return (EINVAL);
	}
	priv->np_ringid = ringid;
	if (ringid & NETMAP_SW_RING) {
		priv->np_qfirst = NETMAP_SW_RING;
		priv->np_qlast = 0;
	} else if (ringid & NETMAP_HW_RING) {
		priv->np_qfirst = i;
		priv->np_qlast = i + 1;
	} else {
		priv->np_qfirst = 0;
		priv->np_qlast = NETMAP_HW_RING ;
	}
	priv->np_txpoll = (ringid & NETMAP_NO_TX_POLL) ? 0 : 1;
    if (netmap_verbose) {
	if (ringid & NETMAP_SW_RING)
		D("ringid %s set to SW RING", NM_IFPNAME(ifp));
	else if (ringid & NETMAP_HW_RING)
		D("ringid %s set to HW RING %d", NM_IFPNAME(ifp),
			priv->np_qfirst);
	else
		D("ringid %s set to all %d HW RINGS", NM_IFPNAME(ifp), lim);
    }
	return 0;
}


/*
 * possibly move the interface to netmap-mode.
 * If success it returns a pointer to netmap_if, otherwise NULL.
 * This must be called with NMG_LOCK held.
 */
struct netmap_if *
netmap_do_regif(struct netmap_priv_d *priv, struct netmap_adapter *na,
	uint16_t ringid, int *err)
{
	struct ifnet *ifp = na->ifp;
	struct netmap_if *nifp = NULL;
	int error, need_mem = 0;

	NMG_LOCK_ASSERT();
	/* ring configuration may have changed, fetch from the card */
	netmap_update_config(na);
	priv->np_na = na;     /* store the reference */
	error = netmap_set_ringid(priv, ringid);
	if (error)
		goto out;
	/* ensure allocators are ready */
	need_mem = !netmap_have_memory_locked(priv);
	if (need_mem) {
		error = netmap_get_memory_locked(priv);
		ND("get_memory returned %d", error);
		if (error)
			goto out;
	}
	nifp = netmap_if_new(NM_IFPNAME(ifp), na);
	if (nifp == NULL) { /* allocation failed */
		/* we should drop the allocator, but only
		 * if we were the ones who grabbed it
		 */
		error = ENOMEM;
		goto out;
	}
	na->active_fds++;
	if (ifp->if_capenable & IFCAP_NETMAP) {
		/* was already set */
	} else {
		/* Otherwise set the card in netmap mode
		 * and make it use the shared buffers.
		 *
		 * do not core lock because the race is harmless here,
		 * there cannot be any traffic to netmap_transmit()
		 */
		na->na_lut = na->nm_mem->pools[NETMAP_BUF_POOL].lut;
		ND("%p->na_lut == %p", na, na->na_lut);
		na->na_lut_objtotal = na->nm_mem->pools[NETMAP_BUF_POOL].objtotal;
		error = na->nm_register(na, 1); /* mode on */
		if (error) {
			netmap_do_unregif(priv, nifp);
			nifp = NULL;
		}
	}
out:
	*err = error;
	if (error) {
		priv->np_na = NULL;
		if (need_mem)
			netmap_drop_memory_locked(priv);
	}
	if (nifp != NULL) {
		/*
		 * advertise that the interface is ready bt setting ni_nifp.
		 * The barrier is needed because readers (poll and *SYNC)
		 * check for priv->np_nifp != NULL without locking
		 */
		wmb(); /* make sure previous writes are visible to all CPUs */
		priv->np_nifp = nifp;
	}
	return nifp;
}



/*
 * ioctl(2) support for the "netmap" device.
 *
 * Following a list of accepted commands:
 * - NIOCGINFO
 * - SIOCGIFADDR	just for convenience
 * - NIOCREGIF
 * - NIOCUNREGIF
 * - NIOCTXSYNC
 * - NIOCRXSYNC
 *
 * Return 0 on success, errno otherwise.
 */
int
netmap_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
	int fflag, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct ifnet *ifp = NULL;
	struct nmreq *nmr = (struct nmreq *) data;
	struct netmap_adapter *na = NULL;
	int error;
	u_int i, lim;
	struct netmap_if *nifp;
	struct netmap_kring *krings;

	(void)dev;	/* UNUSED */
	(void)fflag;	/* UNUSED */
#ifdef linux
#define devfs_get_cdevpriv(pp)				\
	({ *(struct netmap_priv_d **)pp = ((struct file *)td)->private_data; 	\
		(*pp ? 0 : ENOENT); })

/* devfs_set_cdevpriv cannot fail on linux */
#define devfs_set_cdevpriv(p, fn)				\
	({ ((struct file *)td)->private_data = p; (p ? 0 : EINVAL); })


#define devfs_clear_cdevpriv()	do {				\
		netmap_dtor(priv); ((struct file *)td)->private_data = 0;	\
	} while (0)
#endif /* linux */

	CURVNET_SET(TD_TO_VNET(td));

	error = devfs_get_cdevpriv((void **)&priv);
	if (error) {
		CURVNET_RESTORE();
		/* XXX ENOENT should be impossible, since the priv
		 * is now created in the open */
		return (error == ENOENT ? ENXIO : error);
	}

	nmr->nr_name[sizeof(nmr->nr_name) - 1] = '\0';	/* truncate name */
	switch (cmd) {
	case NIOCGINFO:		/* return capabilities etc */
		if (nmr->nr_version != NETMAP_API) {
			D("API mismatch got %d have %d",
				nmr->nr_version, NETMAP_API);
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		if (nmr->nr_cmd == NETMAP_BDG_LIST) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		}

		NMG_LOCK();
		do {
			/* memsize is always valid */
			struct netmap_mem_d *nmd = &nm_mem;
			u_int memflags;

			if (nmr->nr_name[0] != '\0') {
				/* get a refcount */
				error = netmap_get_na(nmr, &na, 1 /* create */);
				if (error)
					break;
				nmd = na->nm_mem; /* get memory allocator */
			}

			error = netmap_mem_get_info(nmd, &nmr->nr_memsize, &memflags);
			if (error)
				break;
			if (na == NULL) /* only memory info */
				break;
			nmr->nr_offset = 0;
			nmr->nr_rx_slots = nmr->nr_tx_slots = 0;
			netmap_update_config(na);
			nmr->nr_rx_rings = na->num_rx_rings;
			nmr->nr_tx_rings = na->num_tx_rings;
			nmr->nr_rx_slots = na->num_rx_desc;
			nmr->nr_tx_slots = na->num_tx_desc;
			if (memflags & NETMAP_MEM_PRIVATE)
				nmr->nr_ringid |= NETMAP_PRIV_MEM;
			netmap_adapter_put(na);
		} while (0);
		NMG_UNLOCK();
		break;

	case NIOCREGIF:
		if (nmr->nr_version != NETMAP_API) {
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		/* possibly attach/detach NIC and VALE switch */
		i = nmr->nr_cmd;
		if (i == NETMAP_BDG_ATTACH || i == NETMAP_BDG_DETACH
				|| i == NETMAP_BDG_OFFSET) {
			error = netmap_bdg_ctl(nmr, NULL);
			break;
		} else if (i != 0) {
			D("nr_cmd must be 0 not %d", i);
			error = EINVAL;
			break;
		}

		/* protect access to priv from concurrent NIOCREGIF */
		NMG_LOCK();
		do {
			u_int memflags;

			if (priv->np_na != NULL) {	/* thread already registered */
				error = netmap_set_ringid(priv, nmr->nr_ringid);
				break;
			}
			/* find the interface and a reference */
			error = netmap_get_na(nmr, &na, 1 /* create */); /* keep reference */
			if (error)
				break;
			ifp = na->ifp;
			if (NETMAP_OWNED_BY_KERN(na)) {
				netmap_adapter_put(na);
				error = EBUSY;
				break;
			}
			nifp = netmap_do_regif(priv, na, nmr->nr_ringid, &error);
			if (!nifp) {    /* reg. failed, release priv and ref */
				netmap_adapter_put(na);
				priv->np_nifp = NULL;
				break;
			}

			/* return the offset of the netmap_if object */
			nmr->nr_rx_rings = na->num_rx_rings;
			nmr->nr_tx_rings = na->num_tx_rings;
			nmr->nr_rx_slots = na->num_rx_desc;
			nmr->nr_tx_slots = na->num_tx_desc;
			error = netmap_mem_get_info(na->nm_mem, &nmr->nr_memsize, &memflags);
			if (error) {
				netmap_adapter_put(na);
				break;
			}
			if (memflags & NETMAP_MEM_PRIVATE) {
				nmr->nr_ringid |= NETMAP_PRIV_MEM;
				*(uint32_t *)(uintptr_t)&nifp->ni_flags |= NI_PRIV_MEM;
			}
			nmr->nr_offset = netmap_mem_if_offset(na->nm_mem, nifp);
		} while (0);
		NMG_UNLOCK();
		break;

	case NIOCUNREGIF:
		// XXX we have no data here ?
		D("deprecated, data is %p", nmr);
		error = EINVAL;
		break;

	case NIOCTXSYNC:
	case NIOCRXSYNC:
		nifp = priv->np_nifp;

		if (nifp == NULL) {
			error = ENXIO;
			break;
		}
		rmb(); /* make sure following reads are not from cache */

		na = priv->np_na;      /* we have a reference */

		if (na == NULL) {
			D("Internal error: nifp != NULL && na == NULL");
			error = ENXIO;
			break;
		}

		ifp = na->ifp;
		if (ifp == NULL) {
			RD(1, "the ifp is gone");
			error = ENXIO;
			break;
		}

		if (priv->np_qfirst == NETMAP_SW_RING) { /* host rings */
			if (cmd == NIOCTXSYNC)
				netmap_txsync_to_host(na);
			else
				netmap_rxsync_from_host(na, NULL, NULL);
			break;
		}
		/* find the last ring to scan */
		lim = priv->np_qlast;
		if (lim == NETMAP_HW_RING)
			lim = (cmd == NIOCTXSYNC) ?
			    na->num_tx_rings : na->num_rx_rings;

		krings = (cmd == NIOCTXSYNC) ? na->tx_rings : na->rx_rings;
		for (i = priv->np_qfirst; i < lim; i++) {
			struct netmap_kring *kring = krings + i;
			if (nm_kr_tryget(kring)) {
				error = EBUSY;
				goto out;
			}
			if (cmd == NIOCTXSYNC) {
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("pre txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
				na->nm_txsync(na, i, NAF_FORCE_RECLAIM);
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("post txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
			} else {
				na->nm_rxsync(na, i, NAF_FORCE_READ);
				microtime(&na->rx_rings[i].ring->ts);
			}
			nm_kr_put(kring);
		}

		break;

#ifdef __FreeBSD__
	case BIOCIMMEDIATE:
	case BIOCGHDRCMPLT:
	case BIOCSHDRCMPLT:
	case BIOCSSEESENT:
		D("ignore BIOCIMMEDIATE/BIOCSHDRCMPLT/BIOCSHDRCMPLT/BIOCSSEESENT");
		break;

	default:	/* allow device-specific ioctls */
	    {
		struct socket so;

		bzero(&so, sizeof(so));
		NMG_LOCK();
		error = netmap_get_na(nmr, &na, 0 /* don't create */); /* keep reference */
		if (error) {
			netmap_adapter_put(na);
			NMG_UNLOCK();
			break;
		}
		ifp = na->ifp;
		so.so_vnet = ifp->if_vnet;
		// so->so_proto not null.
		error = ifioctl(&so, cmd, data, td);
		netmap_adapter_put(na);
		NMG_UNLOCK();
		break;
	    }

#else /* linux */
	default:
		error = EOPNOTSUPP;
#endif /* linux */
	}
out:

	CURVNET_RESTORE();
	return (error);
}


/*
 * select(2) and poll(2) handlers for the "netmap" device.
 *
 * Can be called for one or more queues.
 * Return true the event mask corresponding to ready events.
 * If there are no ready events, do a selrecord on either individual
 * selinfo or on the global one.
 * Device-dependent parts (locking and sync of tx/rx rings)
 * are done through callbacks.
 *
 * On linux, arguments are really pwait, the poll table, and 'td' is struct file *
 * The first one is remapped to pwait as selrecord() uses the name as an
 * hidden argument.
 */
int
netmap_poll(struct cdev *dev, int events, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct netmap_adapter *na;
	struct ifnet *ifp;
	struct netmap_kring *kring;
	u_int i, check_all_tx, check_all_rx, want_tx, want_rx, revents = 0;
	u_int lim_tx, lim_rx, host_forwarded = 0;
	struct mbq q;
	void *pwait = dev;	/* linux compatibility */

	/*
	 * In order to avoid nested locks, we need to "double check"
	 * txsync and rxsync if we decide to do a selrecord().
	 * retry_tx (and retry_rx, later) prevent looping forever.
	 */
	int retry_tx = 1;

	(void)pwait;
	mbq_init(&q);

	if (devfs_get_cdevpriv((void **)&priv) != 0 || priv == NULL)
		return POLLERR;

	if (priv->np_nifp == NULL) {
		D("No if registered");
		return POLLERR;
	}
	rmb(); /* make sure following reads are not from cache */

	na = priv->np_na;
	ifp = na->ifp;
	// check for deleted
	if (ifp == NULL) {
		RD(1, "the ifp is gone");
		return POLLERR;
	}

	if ( (ifp->if_capenable & IFCAP_NETMAP) == 0)
		return POLLERR;

	if (netmap_verbose & 0x8000)
		D("device %s events 0x%x", NM_IFPNAME(ifp), events);
	want_tx = events & (POLLOUT | POLLWRNORM);
	want_rx = events & (POLLIN | POLLRDNORM);

	lim_tx = na->num_tx_rings;
	lim_rx = na->num_rx_rings;

	if (priv->np_qfirst == NETMAP_SW_RING) {
		/* handle the host stack ring */
		if (priv->np_txpoll || want_tx) {
			/* push any packets up, then we are always ready */
			netmap_txsync_to_host(na);
			revents |= want_tx;
		}
		if (want_rx) {
			kring = &na->rx_rings[lim_rx];
			if (kring->ring->avail == 0)
				netmap_rxsync_from_host(na, td, dev);
			if (kring->ring->avail > 0) {
				revents |= want_rx;
			}
		}
		return (revents);
	}

	/*
	 * If we are in transparent mode, check also the host rx ring
	 * XXX Transparent mode at the moment requires to bind all
 	 * rings to a single file descriptor.
	 */
	kring = &na->rx_rings[lim_rx];
	if ( (priv->np_qlast == NETMAP_HW_RING) // XXX check_all
			&& want_rx
			&& (netmap_fwd || kring->ring->flags & NR_FORWARD) ) {
		if (kring->ring->avail == 0)
			netmap_rxsync_from_host(na, td, dev);
		if (kring->ring->avail > 0)
			revents |= want_rx;
	}

	/*
	 * check_all_{tx|rx} are set if the card has more than one queue AND
	 * the file descriptor is bound to all of them. If so, we sleep on
	 * the "global" selinfo, otherwise we sleep on individual selinfo
	 * (FreeBSD only allows two selinfo's per file descriptor).
	 * The interrupt routine in the driver wake one or the other
	 * (or both) depending on which clients are active.
	 *
	 * rxsync() is only called if we run out of buffers on a POLLIN.
	 * txsync() is called if we run out of buffers on POLLOUT, or
	 * there are pending packets to send. The latter can be disabled
	 * passing NETMAP_NO_TX_POLL in the NIOCREG call.
	 */
	check_all_tx = (priv->np_qlast == NETMAP_HW_RING) && (lim_tx > 1);
	check_all_rx = (priv->np_qlast == NETMAP_HW_RING) && (lim_rx > 1);

	if (priv->np_qlast != NETMAP_HW_RING) {
		lim_tx = lim_rx = priv->np_qlast;
	}

	/*
	 * We start with a lock free round which is cheap if we have
	 * slots available. If this fails, then lock and call the sync
	 * routines.
	 * XXX rather than ring->avail >0 should check that
	 * ring->cur has not reached hwcur+hwavail
	 */
	for (i = priv->np_qfirst; want_rx && i < lim_rx; i++) {
		kring = &na->rx_rings[i];
		if (kring->ring->avail > 0) {
			revents |= want_rx;
			want_rx = 0;	/* also breaks the loop */
		}
	}
	for (i = priv->np_qfirst; want_tx && i < lim_tx; i++) {
		kring = &na->tx_rings[i];
		if (kring->ring->avail > 0) {
			revents |= want_tx;
			want_tx = 0;	/* also breaks the loop */
		}
	}

	/*
	 * If we to push packets out (priv->np_txpoll) or want_tx is
	 * still set, we do need to run the txsync calls (on all rings,
	 * to avoid that the tx rings stall).
	 * XXX should also check cur != hwcur on the tx rings.
	 * Fortunately, normal tx mode has np_txpoll set.
	 */
	if (priv->np_txpoll || want_tx) {
		/* If we really want to be woken up (want_tx),
		 * do a selrecord, either on the global or on
		 * the private structure.  Then issue the txsync
		 * so there is no race in the selrecord/selwait
		 */
flush_tx:
		for (i = priv->np_qfirst; i < lim_tx; i++) {
			kring = &na->tx_rings[i];
			/*
			 * Skip this ring if want_tx == 0
			 * (we have already done a successful sync on
			 * a previous ring) AND kring->cur == kring->hwcur
			 * (there are no pending transmissions for this ring).
			 */
			if (!want_tx && kring->ring->cur == kring->nr_hwcur)
				continue;
			/* make sure only one user thread is doing this */
			if (nm_kr_tryget(kring)) {
				ND("ring %p busy is %d",
				    kring, (int)kring->nr_busy);
				revents |= POLLERR;
				goto out;
			}

			if (netmap_verbose & NM_VERB_TXSYNC)
				D("send %d on %s %d",
					kring->ring->cur, NM_IFPNAME(ifp), i);
			if (na->nm_txsync(na, i, 0))
				revents |= POLLERR;

			/* Check avail and call selrecord only if
			 * called with POLLOUT and run out of bufs.
			 * XXX Note, we cannot trust much ring->avail
			 * as it is exposed to userspace (even though
			 * just updated by txsync). We should really
			 * check kring->nr_hwavail or better have
			 * txsync set a flag telling if we need
			 * to do a selrecord().
			 */
			if (want_tx) {
				if (kring->ring->avail > 0) {
					/* stop at the first ring. We don't risk
					 * starvation.
					 */
					revents |= want_tx;
					want_tx = 0;
				}
			}
			nm_kr_put(kring);
		}
		if (want_tx && retry_tx) {
			selrecord(td, check_all_tx ?
			    &na->tx_si : &na->tx_rings[priv->np_qfirst].si);
			retry_tx = 0;
			goto flush_tx;
		}
	}

	/*
	 * now if want_rx is still set we need to lock and rxsync.
	 * Do it on all rings because otherwise we starve.
	 */
	if (want_rx) {
		int retry_rx = 1;
do_retry_rx:
		for (i = priv->np_qfirst; i < lim_rx; i++) {
			kring = &na->rx_rings[i];

			if (nm_kr_tryget(kring)) {
				revents |= POLLERR;
				goto out;
			}

			/* XXX NR_FORWARD should only be read on
			 * physical or NIC ports
			 */
			if (netmap_fwd ||kring->ring->flags & NR_FORWARD) {
				ND(10, "forwarding some buffers up %d to %d",
				    kring->nr_hwcur, kring->ring->cur);
				netmap_grab_packets(kring, &q, netmap_fwd);
			}

			if (na->nm_rxsync(na, i, 0))
				revents |= POLLERR;
			if (netmap_no_timestamp == 0 ||
					kring->ring->flags & NR_TIMESTAMP) {
				microtime(&kring->ring->ts);
			}

			if (kring->ring->avail > 0) {
				revents |= want_rx;
				retry_rx = 0;
			}
			nm_kr_put(kring);
		}
		if (retry_rx) {
			retry_rx = 0;
			selrecord(td, check_all_rx ?
			    &na->rx_si : &na->rx_rings[priv->np_qfirst].si);
			goto do_retry_rx;
		}
	}

	/* forward host to the netmap ring.
	 * I am accessing nr_hwavail without lock, but netmap_transmit
	 * can only increment it, so the operation is safe.
	 */
	kring = &na->rx_rings[lim_rx];
	if ( (priv->np_qlast == NETMAP_HW_RING) // XXX check_all
			&& (netmap_fwd || kring->ring->flags & NR_FORWARD)
			 && kring->nr_hwavail > 0 && !host_forwarded) {
		netmap_sw_to_nic(na);
		host_forwarded = 1; /* prevent another pass */
		want_rx = 0;
		goto flush_tx;
	}

	if (q.head)
		netmap_send_up(na->ifp, &q);

out:

	return (revents);
}

/*------- driver support routines ------*/

static int netmap_hw_krings_create(struct netmap_adapter *);

static int
netmap_notify(struct netmap_adapter *na, u_int n_ring, enum txrx tx, int flags)
{
	struct netmap_kring *kring;

	if (tx == NR_TX) {
		kring = na->tx_rings + n_ring;
		selwakeuppri(&kring->si, PI_NET);
		if (flags & NAF_GLOBAL_NOTIFY)
			selwakeuppri(&na->tx_si, PI_NET);
	} else {
		kring = na->rx_rings + n_ring;
		selwakeuppri(&kring->si, PI_NET);
		if (flags & NAF_GLOBAL_NOTIFY)
			selwakeuppri(&na->rx_si, PI_NET);
	}
	return 0;
}


// XXX check handling of failures
int
netmap_attach_common(struct netmap_adapter *na)
{
	struct ifnet *ifp = na->ifp;

	if (na->num_tx_rings == 0 || na->num_rx_rings == 0) {
		D("%s: invalid rings tx %d rx %d",
			ifp->if_xname, na->num_tx_rings, na->num_rx_rings);
		return EINVAL;
	}
	WNA(ifp) = na;
	NETMAP_SET_CAPABLE(ifp);
	if (na->nm_krings_create == NULL) {
		na->nm_krings_create = netmap_hw_krings_create;
		na->nm_krings_delete = netmap_krings_delete;
	}
	if (na->nm_notify == NULL)
		na->nm_notify = netmap_notify;
	na->active_fds = 0;

	if (na->nm_mem == NULL)
		na->nm_mem = &nm_mem;
	return 0;
}


void
netmap_detach_common(struct netmap_adapter *na)
{
	if (na->ifp)
		WNA(na->ifp) = NULL; /* XXX do we need this? */

	if (na->tx_rings) { /* XXX should not happen */
		D("freeing leftover tx_rings");
		na->nm_krings_delete(na);
	}
	if (na->na_flags & NAF_MEM_OWNER)
		netmap_mem_private_delete(na->nm_mem);
	bzero(na, sizeof(*na));
	free(na, M_DEVBUF);
}


/*
 * Initialize a ``netmap_adapter`` object created by driver on attach.
 * We allocate a block of memory with room for a struct netmap_adapter
 * plus two sets of N+2 struct netmap_kring (where N is the number
 * of hardware rings):
 * krings	0..N-1	are for the hardware queues.
 * kring	N	is for the host stack queue
 * kring	N+1	is only used for the selinfo for all queues.
 * Return 0 on success, ENOMEM otherwise.
 *
 * By default the receive and transmit adapter ring counts are both initialized
 * to num_queues.  na->num_tx_rings can be set for cards with different tx/rx
 * setups.
 */
int
netmap_attach(struct netmap_adapter *arg)
{
	struct netmap_hw_adapter *hwna = NULL;
	// XXX when is arg == NULL ?
	struct ifnet *ifp = arg ? arg->ifp : NULL;

	if (arg == NULL || ifp == NULL)
		goto fail;
	hwna = malloc(sizeof(*hwna), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (hwna == NULL)
		goto fail;
	hwna->up = *arg;
	if (netmap_attach_common(&hwna->up)) {
		free(hwna, M_DEVBUF);
		goto fail;
	}
	netmap_adapter_get(&hwna->up);

#ifdef linux
	if (ifp->netdev_ops) {
		/* prepare a clone of the netdev ops */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
		hwna->nm_ndo.ndo_start_xmit = ifp->netdev_ops;
#else
		hwna->nm_ndo = *ifp->netdev_ops;
#endif
	}
	hwna->nm_ndo.ndo_start_xmit = linux_netmap_start_xmit;
#endif /* linux */

	D("success for %s", NM_IFPNAME(ifp));
	return 0;

fail:
	D("fail, arg %p ifp %p na %p", arg, ifp, hwna);
	netmap_detach(ifp);
	return (hwna ? EINVAL : ENOMEM);
}


void
NM_DBG(netmap_adapter_get)(struct netmap_adapter *na)
{
	if (!na) {
		return;
	}

	refcount_acquire(&na->na_refcount);
}


/* returns 1 iff the netmap_adapter is destroyed */
int
NM_DBG(netmap_adapter_put)(struct netmap_adapter *na)
{
	if (!na)
		return 1;

	if (!refcount_release(&na->na_refcount))
		return 0;

	if (na->nm_dtor)
		na->nm_dtor(na);

	netmap_detach_common(na);

	return 1;
}


int
netmap_hw_krings_create(struct netmap_adapter *na)
{
	return netmap_krings_create(na,
		na->num_tx_rings + 1, na->num_rx_rings + 1, 0);
}



/*
 * Free the allocated memory linked to the given ``netmap_adapter``
 * object.
 */
void
netmap_detach(struct ifnet *ifp)
{
	struct netmap_adapter *na = NA(ifp);

	if (!na)
		return;

	NMG_LOCK();
	netmap_disable_all_rings(ifp);
	netmap_adapter_put(na);
	na->ifp = NULL;
	netmap_enable_all_rings(ifp);
	NMG_UNLOCK();
}


/*
 * Intercept packets from the network stack and pass them
 * to netmap as incoming packets on the 'software' ring.
 * We rely on the OS to make sure that the ifp and na do not go
 * away (typically the caller checks for IFF_DRV_RUNNING or the like).
 * In nm_register() or whenever there is a reinitialization,
 * we make sure to make the mode change visible here.
 */
int
netmap_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring;
	u_int i, len = MBUF_LEN(m);
	u_int error = EBUSY, lim;
	struct netmap_slot *slot;

	// XXX [Linux] we do not need this lock
	// if we follow the down/configure/up protocol -gl
	// mtx_lock(&na->core_lock);
	if ( (ifp->if_capenable & IFCAP_NETMAP) == 0) {
		/* interface not in netmap mode anymore */
		error = ENXIO;
		goto done;
	}

	kring = &na->rx_rings[na->num_rx_rings];
	lim = kring->nkr_num_slots - 1;
	if (netmap_verbose & NM_VERB_HOST)
		D("%s packet %d len %d from the stack", NM_IFPNAME(ifp),
			kring->nr_hwcur + kring->nr_hwavail, len);
	// XXX reconsider long packets if we handle fragments
	if (len > NETMAP_BDG_BUF_SIZE(na->nm_mem)) { /* too long for us */
		D("%s from_host, drop packet size %d > %d", NM_IFPNAME(ifp),
			len, NETMAP_BDG_BUF_SIZE(na->nm_mem));
		goto done;
	}
	/* protect against other instances of netmap_transmit,
	 * and userspace invocations of rxsync().
	 */
	// XXX [Linux] there can be no other instances of netmap_transmit
	// on this same ring, but we still need this lock to protect
	// concurrent access from netmap_sw_to_nic() -gl
	mtx_lock(&kring->q_lock);
	if (kring->nr_hwavail >= lim) {
		if (netmap_verbose)
			D("stack ring %s full\n", NM_IFPNAME(ifp));
	} else {
		/* compute the insert position */
		i = nm_kr_rxpos(kring);
		slot = &kring->ring->slot[i];
		m_copydata(m, 0, (int)len, BDG_NMB(na, slot));
		slot->len = len;
		slot->flags = kring->nkr_slot_flags;
		kring->nr_hwavail++;
		if (netmap_verbose  & NM_VERB_HOST)
			D("wake up host ring %s %d", NM_IFPNAME(na->ifp), na->num_rx_rings);
		na->nm_notify(na, na->num_rx_rings, NR_RX, 0);
		error = 0;
	}
	mtx_unlock(&kring->q_lock);

done:
	// mtx_unlock(&na->core_lock);

	/* release the mbuf in either cases of success or failure. As an
	 * alternative, put the mbuf in a free list and free the list
	 * only when really necessary.
	 */
	m_freem(m);

	return (error);
}


/*
 * netmap_reset() is called by the driver routines when reinitializing
 * a ring. The driver is in charge of locking to protect the kring.
 * If native netmap mode is not set just return NULL.
 */
struct netmap_slot *
netmap_reset(struct netmap_adapter *na, enum txrx tx, u_int n,
	u_int new_cur)
{
	struct netmap_kring *kring;
	int new_hwofs, lim;

	if (na == NULL) {
		D("NULL na, should not happen");
		return NULL;	/* no netmap support here */
	}
	if (!(na->ifp->if_capenable & IFCAP_NETMAP)) {
		ND("interface not in netmap mode");
		return NULL;	/* nothing to reinitialize */
	}

	/* XXX note- in the new scheme, we are not guaranteed to be
	 * under lock (e.g. when called on a device reset).
	 * In this case, we should set a flag and do not trust too
	 * much the values. In practice: TODO
	 * - set a RESET flag somewhere in the kring
	 * - do the processing in a conservative way
	 * - let the *sync() fixup at the end.
	 */
	if (tx == NR_TX) {
		if (n >= na->num_tx_rings)
			return NULL;
		kring = na->tx_rings + n;
		new_hwofs = kring->nr_hwcur - new_cur;
	} else {
		if (n >= na->num_rx_rings)
			return NULL;
		kring = na->rx_rings + n;
		new_hwofs = kring->nr_hwcur + kring->nr_hwavail - new_cur;
	}
	lim = kring->nkr_num_slots - 1;
	if (new_hwofs > lim)
		new_hwofs -= lim + 1;

	/* Always set the new offset value and realign the ring. */
	D("%s hwofs %d -> %d, hwavail %d -> %d",
		tx == NR_TX ? "TX" : "RX",
		kring->nkr_hwofs, new_hwofs,
		kring->nr_hwavail,
		tx == NR_TX ? lim : kring->nr_hwavail);
	kring->nkr_hwofs = new_hwofs;
	if (tx == NR_TX)
		kring->nr_hwavail = lim;
	kring->nr_hwreserved = 0;

#if 0 // def linux
	/* XXX check that the mappings are correct */
	/* need ring_nr, adapter->pdev, direction */
	buffer_info->dma = dma_map_single(&pdev->dev, addr, adapter->rx_buffer_len, DMA_FROM_DEVICE);
	if (dma_mapping_error(&adapter->pdev->dev, buffer_info->dma)) {
		D("error mapping rx netmap buffer %d", i);
		// XXX fix error handling
	}

#endif /* linux */
	/*
	 * Wakeup on the individual and global selwait
	 * We do the wakeup here, but the ring is not yet reconfigured.
	 * However, we are under lock so there are no races.
	 */
	na->nm_notify(na, n, tx, NAF_GLOBAL_NOTIFY);
	return kring->ring->slot;
}


/*
 * Dispatch rx/tx interrupts to the netmap rings.
 *
 * "work_done" is non-null on the RX path, NULL for the TX path.
 * We rely on the OS to make sure that there is only one active
 * instance per queue, and that there is appropriate locking.
 *
 * The 'notify' routine depends on what the ring is attached to.
 * - for a netmap file descriptor, do a selwakeup on the individual
 *   waitqueue, plus one on the global one if needed
 * - for a switch, call the proper forwarding routine
 * - XXX more ?
 */
void
netmap_common_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring;

	q &= NETMAP_RING_MASK;

	if (netmap_verbose) {
	        RD(5, "received %s queue %d", work_done ? "RX" : "TX" , q);
	}

	if (work_done) { /* RX path */
		if (q >= na->num_rx_rings)
			return;	// not a physical queue
		kring = na->rx_rings + q;
		kring->nr_kflags |= NKR_PENDINTR;	// XXX atomic ?
		na->nm_notify(na, q, NR_RX,
			(na->num_rx_rings > 1 ? NAF_GLOBAL_NOTIFY : 0));
		*work_done = 1; /* do not fire napi again */
	} else { /* TX path */
		if (q >= na->num_tx_rings)
			return;	// not a physical queue
		kring = na->tx_rings + q;
		na->nm_notify(na, q, NR_TX,
			(na->num_tx_rings > 1 ? NAF_GLOBAL_NOTIFY : 0));
	}
}

/*
 * Default functions to handle rx/tx interrupts from a physical device.
 * "work_done" is non-null on the RX path, NULL for the TX path.
 *
 * If the card is not in netmap mode, simply return 0,
 * so that the caller proceeds with regular processing.
 * Otherwise call netmap_common_irq() and return 1.
 *
 * If the card is connected to a netmap file descriptor,
 * do a selwakeup on the individual queue, plus one on the global one
 * if needed (multiqueue card _and_ there are multiqueue listeners),
 * and return 1.
 *
 * Finally, if called on rx from an interface connected to a switch,
 * calls the proper forwarding routine, and return 1.
 */
int
netmap_rx_irq(struct ifnet *ifp, u_int q, u_int *work_done)
{
	// XXX could we check NAF_NATIVE_ON ?
	if (!(ifp->if_capenable & IFCAP_NETMAP))
		return 0;

	if (NA(ifp)->na_flags & NAF_SKIP_INTR) {
		ND("use regular interrupt");
		return 0;
	}

	netmap_common_irq(ifp, q, work_done);
	return 1;
}


/*
 * Module loader and unloader
 *
 * netmap_init() creates the /dev/netmap device and initializes
 * all global variables. Returns 0 on success, errno on failure
 * (but there is no chance)
 *
 * netmap_fini() destroys everything.
 */

static struct cdev *netmap_dev; /* /dev/netmap character device. */
extern struct cdevsw netmap_cdevsw;

void
netmap_fini(void)
{
	// XXX destroy_bridges() ?
	if (netmap_dev)
		destroy_dev(netmap_dev);
	netmap_mem_fini();
	NMG_LOCK_DESTROY();
	printf("netmap: unloaded module.\n");
}

int
netmap_init(void)
{
	int error;

	NMG_LOCK_INIT();

	error = netmap_mem_init();
	if (error != 0)
		goto fail;
	/* XXX could use make_dev_credv() to get error number */
	netmap_dev = make_dev(&netmap_cdevsw, 0, UID_ROOT, GID_WHEEL, 0660,
			      "netmap");
	if (!netmap_dev)
		goto fail;

	netmap_init_bridges();
	printf("netmap: loaded module\n");
	return (0);
fail:
	netmap_fini();
	return (EINVAL); /* may be incorrect */
}
