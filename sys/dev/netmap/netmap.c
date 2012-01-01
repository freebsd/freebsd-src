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
 * $Id: netmap.c 9795 2011-12-02 11:39:08Z luigi $
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
 */

#include <sys/cdefs.h> /* prerequisite */
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/param.h>	/* defines used in kernel.h */
#include <sys/jail.h>
#include <sys/kernel.h>	/* types used in module initialization */
#include <sys/conf.h>	/* cdevsw struct */
#include <sys/uio.h>	/* uio struct */
#include <sys/sockio.h>
#include <sys/socketvar.h>	/* struct socket */
#include <sys/malloc.h>
#include <sys/mman.h>	/* PROT_EXEC */
#include <sys/poll.h>
#include <sys/proc.h>
#include <vm/vm.h>	/* vtophys */
#include <vm/pmap.h>	/* vtophys */
#include <sys/socket.h> /* sockaddrs */
#include <machine/bus.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/bpf.h>		/* BIOCIMMEDIATE */
#include <net/vnet.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <machine/bus.h>	/* bus_dmamap_* */

MALLOC_DEFINE(M_NETMAP, "netmap", "Network memory map");

/*
 * lock and unlock for the netmap memory allocator
 */
#define NMA_LOCK()	mtx_lock(&netmap_mem_d->nm_mtx);
#define NMA_UNLOCK()	mtx_unlock(&netmap_mem_d->nm_mtx);

/*
 * Default amount of memory pre-allocated by the module.
 * We start with a large size and then shrink our demand
 * according to what is avalable when the module is loaded.
 * At the moment the block is contiguous, but we can easily
 * restrict our demand to smaller units (16..64k)
 */
#define NETMAP_MEMORY_SIZE (64 * 1024 * PAGE_SIZE)
static void * netmap_malloc(size_t size, const char *msg);
static void netmap_free(void *addr, const char *msg);

/*
 * Allocator for a pool of packet buffers. For each buffer we have
 * one entry in the bitmap to signal the state. Allocation scans
 * the bitmap, but since this is done only on attach, we are not
 * too worried about performance
 * XXX if we need to allocate small blocks, a translation
 * table is used both for kernel virtual address and physical
 * addresses.
 */
struct netmap_buf_pool {
	u_int total_buffers;	/* total buffers. */
	u_int free;
	u_int bufsize;
	char *base;		/* buffer base address */
	uint32_t *bitmap;	/* one bit per buffer, 1 means free */
};
struct netmap_buf_pool nm_buf_pool;
/* XXX move these two vars back into netmap_buf_pool */
u_int netmap_total_buffers;
char *netmap_buffer_base;

/* user-controlled variables */
int netmap_verbose;

static int no_timestamp; /* don't timestamp on rxsync */

SYSCTL_NODE(_dev, OID_AUTO, netmap, CTLFLAG_RW, 0, "Netmap args");
SYSCTL_INT(_dev_netmap, OID_AUTO, verbose,
    CTLFLAG_RW, &netmap_verbose, 0, "Verbose mode");
SYSCTL_INT(_dev_netmap, OID_AUTO, no_timestamp,
    CTLFLAG_RW, &no_timestamp, 0, "no_timestamp");
SYSCTL_INT(_dev_netmap, OID_AUTO, total_buffers,
    CTLFLAG_RD, &nm_buf_pool.total_buffers, 0, "total_buffers");
SYSCTL_INT(_dev_netmap, OID_AUTO, free_buffers,
    CTLFLAG_RD, &nm_buf_pool.free, 0, "free_buffers");

/*
 * Allocate n buffers from the ring, and fill the slot.
 * Buffer 0 is the 'junk' buffer.
 */
static void
netmap_new_bufs(struct netmap_buf_pool *p, struct netmap_slot *slot, u_int n)
{
	uint32_t bi = 0;		/* index in the bitmap */
	uint32_t mask, j, i = 0;	/* slot counter */

	if (n > p->free) {
		D("only %d out of %d buffers available", i, n);
		return;
	}
	/* termination is guaranteed by p->free */
	while (i < n && p->free > 0) {
		uint32_t cur = p->bitmap[bi];
		if (cur == 0) { /* bitmask is fully used */
			bi++;
			continue;
		}
		/* locate a slot */
		for (j = 0, mask = 1; (cur & mask) == 0; j++, mask <<= 1) ;
		p->bitmap[bi] &= ~mask;		/* slot in use */
		p->free--;
		slot[i].buf_idx = bi*32+j;
		slot[i].len = p->bufsize;
		slot[i].flags = NS_BUF_CHANGED;
		i++;
	}
	ND("allocated %d buffers, %d available", n, p->free);
}


static void
netmap_free_buf(struct netmap_buf_pool *p, uint32_t i)
{
	uint32_t pos, mask;
	if (i >= p->total_buffers) {
		D("invalid free index %d", i);
		return;
	}
	pos = i / 32;
	mask = 1 << (i % 32);
	if (p->bitmap[pos] & mask) {
		D("slot %d already free", i);
		return;
	}
	p->bitmap[pos] |= mask;
	p->free++;
}


/* Descriptor of the memory objects handled by our memory allocator. */
struct netmap_mem_obj {
	TAILQ_ENTRY(netmap_mem_obj) nmo_next; /* next object in the
						 chain. */
	int nmo_used; /* flag set on used memory objects. */
	size_t nmo_size; /* size of the memory area reserved for the
			    object. */
	void *nmo_data; /* pointer to the memory area. */
};

/* Wrap our memory objects to make them ``chainable``. */
TAILQ_HEAD(netmap_mem_obj_h, netmap_mem_obj);


/* Descriptor of our custom memory allocator. */
struct netmap_mem_d {
	struct mtx nm_mtx; /* lock used to handle the chain of memory
			      objects. */
	struct netmap_mem_obj_h nm_molist; /* list of memory objects */
	size_t nm_size; /* total amount of memory used for rings etc. */
	size_t nm_totalsize; /* total amount of allocated memory
		(the difference is used for buffers) */
	size_t nm_buf_start; /* offset of packet buffers.
			This is page-aligned. */
	size_t nm_buf_len; /* total memory for buffers */
	void *nm_buffer; /* pointer to the whole pre-allocated memory
			    area. */
};


/* Structure associated to each thread which registered an interface. */
struct netmap_priv_d {
	struct netmap_if *np_nifp;	/* netmap interface descriptor. */

	struct ifnet	*np_ifp;	/* device for which we hold a reference */
	int		np_ringid;	/* from the ioctl */
	u_int		np_qfirst, np_qlast;	/* range of rings to scan */
	uint16_t	np_txpoll;
};


static struct cdev *netmap_dev; /* /dev/netmap character device. */
static struct netmap_mem_d *netmap_mem_d; /* Our memory allocator. */


static d_mmap_t netmap_mmap;
static d_ioctl_t netmap_ioctl;
static d_poll_t netmap_poll;

#ifdef NETMAP_KEVENT
static d_kqfilter_t netmap_kqfilter;
#endif

static struct cdevsw netmap_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "netmap",
	.d_mmap = netmap_mmap,
	.d_ioctl = netmap_ioctl,
	.d_poll = netmap_poll,
#ifdef NETMAP_KEVENT
	.d_kqfilter = netmap_kqfilter,
#endif
};

#ifdef NETMAP_KEVENT
static int              netmap_kqread(struct knote *, long);
static int              netmap_kqwrite(struct knote *, long);
static void             netmap_kqdetach(struct knote *);

static struct filterops netmap_read_filterops = {
	.f_isfd =       1,
	.f_attach =     NULL,
	.f_detach =     netmap_kqdetach,
	.f_event =      netmap_kqread,
};
  
static struct filterops netmap_write_filterops = {
	.f_isfd =       1,
	.f_attach =     NULL,
	.f_detach =     netmap_kqdetach,
	.f_event =      netmap_kqwrite,
};

/*
 * support for the kevent() system call.
 *
 * This is the kevent filter, and is executed each time a new event
 * is triggered on the device. This function execute some operation
 * depending on the received filter.
 *
 * The implementation should test the filters and should implement
 * filter operations we are interested on (a full list in /sys/event.h).
 *
 * On a match we should:
 * - set kn->kn_fop
 * - set kn->kn_hook
 * - call knlist_add() to deliver the event to the application.
 *
 * Return 0 if the event should be delivered to the application.
 */
static int
netmap_kqfilter(struct cdev *dev, struct knote *kn)
{
	/* declare variables needed to read/write */

	switch(kn->kn_filter) {
	case EVFILT_READ:
		if (netmap_verbose)
			D("%s kqfilter: EVFILT_READ" ifp->if_xname);

		/* read operations */
		kn->kn_fop = &netmap_read_filterops;
		break;

	case EVFILT_WRITE:
		if (netmap_verbose)
			D("%s kqfilter: EVFILT_WRITE" ifp->if_xname);

		/* write operations */
		kn->kn_fop = &netmap_write_filterops;
		break;
  
	default:
		if (netmap_verbose)
			D("%s kqfilter: invalid filter" ifp->if_xname);
		return(EINVAL);
	}
  
	kn->kn_hook = 0;//
	knlist_add(&netmap_sc->tun_rsel.si_note, kn, 0);

	return (0);
}
#endif /* NETMAP_KEVENT */

/*
 * File descriptor's private data destructor.
 *
 * Call nm_register(ifp,0) to stop netmap mode on the interface and
 * revert to normal operation. We expect that np_ifp has not gone.
 */
static void
netmap_dtor(void *data)
{
	struct netmap_priv_d *priv = data;
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_if *nifp = priv->np_nifp;

	if (0)
	    printf("%s starting for %p ifp %p\n", __FUNCTION__, priv,
		priv ? priv->np_ifp : NULL);

	na->nm_lock(ifp->if_softc, NETMAP_CORE_LOCK, 0); 

	na->refcount--;
	if (na->refcount <= 0) {	/* last instance */
		u_int i;

		D("deleting last netmap instance for %s", ifp->if_xname);
		/*
		 * there is a race here with *_netmap_task() and
		 * netmap_poll(), which don't run under NETMAP_CORE_LOCK.
		 * na->refcount == 0 && na->ifp->if_capenable & IFCAP_NETMAP
		 * (aka NETMAP_DELETING(na)) are a unique marker that the
		 * device is dying.
		 * Before destroying stuff we sleep a bit, and then complete
		 * the job. NIOCREG should realize the condition and
		 * loop until they can continue; the other routines
		 * should check the condition at entry and quit if
		 * they cannot run.
		 */
		na->nm_lock(ifp->if_softc, NETMAP_CORE_UNLOCK, 0);
		tsleep(na, 0, "NIOCUNREG", 4);
		na->nm_lock(ifp->if_softc, NETMAP_CORE_LOCK, 0);
		na->nm_register(ifp, 0); /* off, clear IFCAP_NETMAP */
		/* Wake up any sleeping threads. netmap_poll will
		 * then return POLLERR
		 */
		for (i = 0; i < na->num_queues + 2; i++) {
			selwakeuppri(&na->tx_rings[i].si, PI_NET);
			selwakeuppri(&na->rx_rings[i].si, PI_NET);
		}
		/* release all buffers */
		NMA_LOCK();
		for (i = 0; i < na->num_queues + 1; i++) {
			int j, lim;
			struct netmap_ring *ring;

			ND("tx queue %d", i);
			ring = na->tx_rings[i].ring;
			lim = na->tx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(&nm_buf_pool,
					ring->slot[j].buf_idx);

			ND("rx queue %d", i);
			ring = na->rx_rings[i].ring;
			lim = na->rx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(&nm_buf_pool,
					ring->slot[j].buf_idx);
		}
		NMA_UNLOCK();
		netmap_free(na->tx_rings[0].ring, "shadow rings");
		wakeup(na);
	}
	netmap_free(nifp, "nifp");

	na->nm_lock(ifp->if_softc, NETMAP_CORE_UNLOCK, 0); 

	if_rele(ifp);

	bzero(priv, sizeof(*priv));	/* XXX for safety */
	free(priv, M_DEVBUF);
}



/*
 * Create and return a new ``netmap_if`` object, and possibly also
 * rings and packet buffors.
 *
 * Return NULL on failure.
 */
static void *
netmap_if_new(const char *ifname, struct netmap_adapter *na)
{
	struct netmap_if *nifp;
	struct netmap_ring *ring;
	char *buff;
	u_int i, len, ofs;
	u_int n = na->num_queues + 1; /* shorthand, include stack queue */

	/*
	 * the descriptor is followed inline by an array of offsets
	 * to the tx and rx rings in the shared memory region.
	 */
	len = sizeof(struct netmap_if) + 2 * n * sizeof(ssize_t);
	nifp = netmap_malloc(len, "nifp");
	if (nifp == NULL)
		return (NULL);

	/* initialize base fields */
	*(int *)(uintptr_t)&nifp->ni_num_queues = na->num_queues;
	strncpy(nifp->ni_name, ifname, IFNAMSIZ);

	(na->refcount)++;	/* XXX atomic ? we are under lock */
	if (na->refcount > 1)
		goto final;

	/*
	 * If this is the first instance, allocate the shadow rings and
	 * buffers for this card (one for each hw queue, one for the host).
	 * The rings are contiguous, but have variable size.
	 * The entire block is reachable at
	 *	na->tx_rings[0].ring
	 */

	len = n * (2 * sizeof(struct netmap_ring) +
		  (na->num_tx_desc + na->num_rx_desc) *
		   sizeof(struct netmap_slot) );
	buff = netmap_malloc(len, "shadow rings");
	if (buff == NULL) {
		D("failed to allocate %d bytes for %s shadow ring",
			len, ifname);
error:
		(na->refcount)--;
		netmap_free(nifp, "nifp, rings failed");
		return (NULL);
	}
	/* do we have the bufers ? we are in need of num_tx_desc buffers for
	 * each tx ring and num_tx_desc buffers for each rx ring. */
	len = n * (na->num_tx_desc + na->num_rx_desc);
	NMA_LOCK();
	if (nm_buf_pool.free < len) {
		NMA_UNLOCK();
		netmap_free(buff, "not enough bufs");
		goto error;
	}
	/*
	 * in the kring, store the pointers to the shared rings
	 * and initialize the rings. We are under NMA_LOCK().
	 */
	ofs = 0;
	for (i = 0; i < n; i++) {
		struct netmap_kring *kring;
		int numdesc;

		/* Transmit rings */
		kring = &na->tx_rings[i];
		numdesc = na->num_tx_desc;
		bzero(kring, sizeof(*kring));
		kring->na = na;

		ring = kring->ring = (struct netmap_ring *)(buff + ofs);
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
			nm_buf_pool.base - (char *)ring;
		ND("txring[%d] at %p ofs %d", i, ring, ring->buf_ofs);
		*(int *)(int *)(uintptr_t)&ring->num_slots =
			kring->nkr_num_slots = numdesc;

		/*
		 * IMPORTANT:
		 * Always keep one slot empty, so we can detect new
		 * transmissions comparing cur and nr_hwcur (they are
		 * the same only if there are no new transmissions).
		 */
		ring->avail = kring->nr_hwavail = numdesc - 1;
		ring->cur = kring->nr_hwcur = 0;
		netmap_new_bufs(&nm_buf_pool, ring->slot, numdesc);

		ofs += sizeof(struct netmap_ring) +
			numdesc * sizeof(struct netmap_slot);

		/* Receive rings */
		kring = &na->rx_rings[i];
		numdesc = na->num_rx_desc;
		bzero(kring, sizeof(*kring));
		kring->na = na;

		ring = kring->ring = (struct netmap_ring *)(buff + ofs);
		*(ssize_t *)(uintptr_t)&ring->buf_ofs =
			nm_buf_pool.base - (char *)ring;
		ND("rxring[%d] at %p offset %d", i, ring, ring->buf_ofs);
		*(int *)(int *)(uintptr_t)&ring->num_slots =
			kring->nkr_num_slots = numdesc;
		ring->cur = kring->nr_hwcur = 0;
		ring->avail = kring->nr_hwavail = 0; /* empty */
		netmap_new_bufs(&nm_buf_pool, ring->slot, numdesc);
		ofs += sizeof(struct netmap_ring) +
			numdesc * sizeof(struct netmap_slot);
	}
	NMA_UNLOCK();
	for (i = 0; i < n+1; i++) {
		// XXX initialize the selrecord structs.
	}
final:
	/*
	 * fill the slots for the rx and tx queues. They contain the offset
	 * between the ring and nifp, so the information is usable in
	 * userspace to reach the ring from the nifp.
	 */
	for (i = 0; i < n; i++) {
		char *base = (char *)nifp;
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i] =
			(char *)na->tx_rings[i].ring - base;
		*(ssize_t *)(uintptr_t)&nifp->ring_ofs[i+n] =
			(char *)na->rx_rings[i].ring - base;
	}
	return (nifp);
}


/*
 * mmap(2) support for the "netmap" device.
 *
 * Expose all the memory previously allocated by our custom memory
 * allocator: this way the user has only to issue a single mmap(2), and
 * can work on all the data structures flawlessly.
 *
 * Return 0 on success, -1 otherwise.
 */
static int
#if __FreeBSD_version < 900000
netmap_mmap(__unused struct cdev *dev, vm_offset_t offset, vm_paddr_t *paddr,
	    int nprot)
#else
netmap_mmap(__unused struct cdev *dev, vm_ooffset_t offset, vm_paddr_t *paddr,
	    int nprot, __unused vm_memattr_t *memattr)
#endif
{
	if (nprot & PROT_EXEC)
		return (-1);	// XXX -1 or EINVAL ?
	ND("request for offset 0x%x", (uint32_t)offset);
	*paddr = vtophys(netmap_mem_d->nm_buffer) + offset;

	return (0);
}


/*
 * Handlers for synchronization of the queues from/to the host.
 *
 * netmap_sync_to_host() passes packets up. We are called from a
 * system call in user process context, and the only contention
 * can be among multiple user threads erroneously calling
 * this routine concurrently. In principle we should not even
 * need to lock.
 */
static void
netmap_sync_to_host(struct netmap_adapter *na)
{
	struct netmap_kring *kring = &na->tx_rings[na->num_queues];
	struct netmap_ring *ring = kring->ring;
	struct mbuf *head = NULL, *tail = NULL, *m;
	u_int k, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim) {
		netmap_ring_reinit(kring);
		return;
	}
	// na->nm_lock(na->ifp->if_softc, NETMAP_CORE_LOCK, 0);

	/* Take packets from hwcur to cur and pass them up.
	 * In case of no buffers we give up. At the end of the loop,
	 * the queue is drained in all cases.
	 */
	for (n = kring->nr_hwcur; n != k;) {
		struct netmap_slot *slot = &ring->slot[n];

		n = (n == lim) ? 0 : n + 1;
		if (slot->len < 14 || slot->len > NETMAP_BUF_SIZE) {
			D("bad pkt at %d len %d", n, slot->len);
			continue;
		}
		m = m_devget(NMB(slot), slot->len, 0, na->ifp, NULL);

		if (m == NULL)
			break;
		if (tail)
			tail->m_nextpkt = m;
		else
			head = m;
		tail = m;
		m->m_nextpkt = NULL;
	}
	kring->nr_hwcur = k;
	kring->nr_hwavail = ring->avail = lim;
	// na->nm_lock(na->ifp->if_softc, NETMAP_CORE_UNLOCK, 0);

	/* send packets up, outside the lock */
	while ((m = head) != NULL) {
		head = head->m_nextpkt;
		m->m_nextpkt = NULL;
		m->m_pkthdr.rcvif = na->ifp;
		if (netmap_verbose & NM_VERB_HOST)
			D("sending up pkt %p size %d", m, m->m_pkthdr.len);
		(na->ifp->if_input)(na->ifp, m);
	}
}

/*
 * rxsync backend for packets coming from the host stack.
 * They have been put in the queue by netmap_start() so we
 * need to protect access to the kring using a lock.
 *
 * This routine also does the selrecord if called from the poll handler
 * (we know because td != NULL).
 */
static void
netmap_sync_from_host(struct netmap_adapter *na, struct thread *td)
{
	struct netmap_kring *kring = &na->rx_rings[na->num_queues];
	struct netmap_ring *ring = kring->ring;
	int error = 1, delta;
	u_int k = ring->cur, lim = kring->nkr_num_slots;

	na->nm_lock(na->ifp->if_softc, NETMAP_CORE_LOCK, 0);
	if (k >= lim) /* bad value */
		goto done;
	delta = k - kring->nr_hwcur;
	if (delta < 0)
		delta += lim;
	kring->nr_hwavail -= delta;
	if (kring->nr_hwavail < 0)	/* error */
		goto done;
	kring->nr_hwcur = k;
	error = 0;
	k = ring->avail = kring->nr_hwavail;
	if (k == 0 && td)
		selrecord(td, &kring->si);
	if (k && (netmap_verbose & NM_VERB_HOST))
		D("%d pkts from stack", k);
done:
	na->nm_lock(na->ifp->if_softc, NETMAP_CORE_UNLOCK, 0);
	if (error)
		netmap_ring_reinit(kring);
}


/*
 * get a refcounted reference to an interface.
 * Return ENXIO if the interface does not exist, EINVAL if netmap
 * is not supported by the interface.
 * If successful, hold a reference.
 */
static int
get_ifp(const char *name, struct ifnet **ifp)
{
	*ifp = ifunit_ref(name);
	if (*ifp == NULL)
		return (ENXIO);
	/* can do this if the capability exists and if_pspare[0]
	 * points to the netmap descriptor.
	 */
	if ((*ifp)->if_capabilities & IFCAP_NETMAP && NA(*ifp))
		return 0;	/* valid pointer, we hold the refcount */
	if_rele(*ifp);
	return EINVAL;	// not NETMAP capable
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

	D("called for %s", kring->na->ifp->if_xname);
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
		} else if (len > NETMAP_BUF_SIZE) {
			ring->slot[i].len = 0;
			if (!errors++)
				D("bad len %d at slot %d idx %d",
					len, i, idx);
		}
	}
	if (errors) {
		int pos = kring - kring->na->tx_rings;
		int n = kring->na->num_queues + 2;

		D("total %d errors", errors);
		errors++;
		D("%s %s[%d] reinit, cur %d -> %d avail %d -> %d",
			kring->na->ifp->if_xname,
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
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);
	void *adapter = na->ifp->if_softc;	/* shorthand */
	u_int i = ringid & NETMAP_RING_MASK;
	/* first time we don't lock */
	int need_lock = (priv->np_qfirst != priv->np_qlast);

	if ( (ringid & NETMAP_HW_RING) && i >= na->num_queues) {
		D("invalid ring id %d", i);
		return (EINVAL);
	}
	if (need_lock)
		na->nm_lock(adapter, NETMAP_CORE_LOCK, 0);
	priv->np_ringid = ringid;
	if (ringid & NETMAP_SW_RING) {
		priv->np_qfirst = na->num_queues;
		priv->np_qlast = na->num_queues + 1;
	} else if (ringid & NETMAP_HW_RING) {
		priv->np_qfirst = i;
		priv->np_qlast = i + 1;
	} else {
		priv->np_qfirst = 0;
		priv->np_qlast = na->num_queues;
	}
	priv->np_txpoll = (ringid & NETMAP_NO_TX_POLL) ? 0 : 1;
	if (need_lock)
		na->nm_lock(adapter, NETMAP_CORE_UNLOCK, 0);
	if (ringid & NETMAP_SW_RING)
		D("ringid %s set to SW RING", ifp->if_xname);
	else if (ringid & NETMAP_HW_RING)
		D("ringid %s set to HW RING %d", ifp->if_xname,
			priv->np_qfirst);
	else
		D("ringid %s set to all %d HW RINGS", ifp->if_xname,
			priv->np_qlast);
	return 0;
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
static int
netmap_ioctl(__unused struct cdev *dev, u_long cmd, caddr_t data,
	__unused int fflag, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct ifnet *ifp;
	struct nmreq *nmr = (struct nmreq *) data;
	struct netmap_adapter *na;
	void *adapter;
	int error;
	u_int i;
	struct netmap_if *nifp;

	CURVNET_SET(TD_TO_VNET(td));

	error = devfs_get_cdevpriv((void **)&priv);
	if (error != ENOENT && error != 0) {
		CURVNET_RESTORE();
		return (error);
	}

	error = 0;	/* Could be ENOENT */
	switch (cmd) {
	case NIOCGINFO:		/* return capabilities etc */
		/* memsize is always valid */
		nmr->nr_memsize = netmap_mem_d->nm_totalsize;
		nmr->nr_offset = 0;
		nmr->nr_numrings = 0;
		nmr->nr_numslots = 0;
		if (nmr->nr_name[0] == '\0')	/* just get memory info */
			break;
		error = get_ifp(nmr->nr_name, &ifp); /* get a refcount */
		if (error)
			break;
		na = NA(ifp); /* retrieve netmap_adapter */
		nmr->nr_numrings = na->num_queues;
		nmr->nr_numslots = na->num_tx_desc;
		if_rele(ifp);	/* return the refcount */
		break;

	case NIOCREGIF:
		if (priv != NULL) {	/* thread already registered */
			error = netmap_set_ringid(priv, nmr->nr_ringid);
			break;
		}
		/* find the interface and a reference */
		error = get_ifp(nmr->nr_name, &ifp); /* keep reference */
		if (error)
			break;
		na = NA(ifp); /* retrieve netmap adapter */
		adapter = na->ifp->if_softc;	/* shorthand */
		/*
		 * Allocate the private per-thread structure.
		 * XXX perhaps we can use a blocking malloc ?
		 */
		priv = malloc(sizeof(struct netmap_priv_d), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
		if (priv == NULL) {
			error = ENOMEM;
			if_rele(ifp);   /* return the refcount */
			break;
		}


		for (i = 10; i > 0; i--) {
			na->nm_lock(adapter, NETMAP_CORE_LOCK, 0);
			if (!NETMAP_DELETING(na))
				break;
			na->nm_lock(adapter, NETMAP_CORE_UNLOCK, 0);
			tsleep(na, 0, "NIOCREGIF", hz/10);
		}
		if (i == 0) {
			D("too many NIOCREGIF attempts, give up");
			error = EINVAL;
			free(priv, M_DEVBUF);
			if_rele(ifp);	/* return the refcount */
			break;
		}

		priv->np_ifp = ifp;	/* store the reference */
		error = netmap_set_ringid(priv, nmr->nr_ringid);
		if (error)
			goto error;
		priv->np_nifp = nifp = netmap_if_new(nmr->nr_name, na);
		if (nifp == NULL) { /* allocation failed */
			error = ENOMEM;
		} else if (ifp->if_capenable & IFCAP_NETMAP) {
			/* was already set */
		} else {
			/* Otherwise set the card in netmap mode
			 * and make it use the shared buffers.
			 */
			error = na->nm_register(ifp, 1); /* mode on */
			if (error) {
				/*
				 * do something similar to netmap_dtor().
				 */
				netmap_free(na->tx_rings[0].ring, "rings, reg.failed");
				free(na->tx_rings, M_DEVBUF);
				na->tx_rings = na->rx_rings = NULL;
				na->refcount--;
				netmap_free(nifp, "nifp, rings failed");
				nifp = NULL;
			}
		}

		if (error) {	/* reg. failed, release priv and ref */
error:
			na->nm_lock(adapter, NETMAP_CORE_UNLOCK, 0);
			free(priv, M_DEVBUF);
			if_rele(ifp);	/* return the refcount */
			break;
		}

		na->nm_lock(adapter, NETMAP_CORE_UNLOCK, 0);
		error = devfs_set_cdevpriv(priv, netmap_dtor);

		if (error != 0) {
			/* could not assign the private storage for the
			 * thread, call the destructor explicitly.
			 */
			netmap_dtor(priv);
			break;
		}

		/* return the offset of the netmap_if object */
		nmr->nr_numrings = na->num_queues;
		nmr->nr_numslots = na->num_tx_desc;
		nmr->nr_memsize = netmap_mem_d->nm_totalsize;
		nmr->nr_offset =
			((char *) nifp - (char *) netmap_mem_d->nm_buffer);
		break;

	case NIOCUNREGIF:
		if (priv == NULL) {
			error = ENXIO;
			break;
		}

		/* the interface is unregistered inside the
		   destructor of the private data. */
		devfs_clear_cdevpriv();
		break;

	case NIOCTXSYNC:
        case NIOCRXSYNC:
		if (priv == NULL) {
			error = ENXIO;
			break;
		}
		ifp = priv->np_ifp;	/* we have a reference */
		na = NA(ifp); /* retrieve netmap adapter */
		adapter = ifp->if_softc;	/* shorthand */

		if (priv->np_qfirst == na->num_queues) {
			/* queues to/from host */
			if (cmd == NIOCTXSYNC)
				netmap_sync_to_host(na);
			else
				netmap_sync_from_host(na, NULL);
			break;
		}

		for (i = priv->np_qfirst; i < priv->np_qlast; i++) {
		    if (cmd == NIOCTXSYNC) {
			struct netmap_kring *kring = &na->tx_rings[i];
			if (netmap_verbose & NM_VERB_TXSYNC)
				D("sync tx ring %d cur %d hwcur %d",
					i, kring->ring->cur,
					kring->nr_hwcur);
                        na->nm_txsync(adapter, i, 1 /* do lock */);
			if (netmap_verbose & NM_VERB_TXSYNC)
				D("after sync tx ring %d cur %d hwcur %d",
					i, kring->ring->cur,
					kring->nr_hwcur);
		    } else {
			na->nm_rxsync(adapter, i, 1 /* do lock */);
			microtime(&na->rx_rings[i].ring->ts);
		    }
		}

                break;

	case BIOCIMMEDIATE:
	case BIOCGHDRCMPLT:
	case BIOCSHDRCMPLT:
	case BIOCSSEESENT:
		D("ignore BIOCIMMEDIATE/BIOCSHDRCMPLT/BIOCSHDRCMPLT/BIOCSSEESENT");
		break;

	default:
	    {
		/*
		 * allow device calls
		 */
		struct socket so;
		bzero(&so, sizeof(so));
		error = get_ifp(nmr->nr_name, &ifp); /* keep reference */
		if (error)
			break;
		so.so_vnet = ifp->if_vnet;
		// so->so_proto not null.
		error = ifioctl(&so, cmd, data, td);
		if_rele(ifp);
	    }
	}

	CURVNET_RESTORE();
	return (error);
}


/*
 * select(2) and poll(2) handlers for the "netmap" device.
 *
 * Can be called for one or more queues.
 * Return true the event mask corresponding to ready events.
 * If there are no ready events, do a selrecord on either individual
 * selfd or on the global one.
 * Device-dependent parts (locking and sync of tx/rx rings)
 * are done through callbacks.
 */
static int
netmap_poll(__unused struct cdev *dev, int events, struct thread *td)
{
	struct netmap_priv_d *priv = NULL;
	struct netmap_adapter *na;
	struct ifnet *ifp;
	struct netmap_kring *kring;
	u_int core_lock, i, check_all, want_tx, want_rx, revents = 0;
	void *adapter;

	if (devfs_get_cdevpriv((void **)&priv) != 0 || priv == NULL)
		return POLLERR;

	ifp = priv->np_ifp;
	// XXX check for deleting() ?
	if ( (ifp->if_capenable & IFCAP_NETMAP) == 0)
		return POLLERR;

	if (netmap_verbose & 0x8000)
		D("device %s events 0x%x", ifp->if_xname, events);
	want_tx = events & (POLLOUT | POLLWRNORM);
	want_rx = events & (POLLIN | POLLRDNORM);

	adapter = ifp->if_softc;
	na = NA(ifp); /* retrieve netmap adapter */

	/* how many queues we are scanning */
	i = priv->np_qfirst;
	if (i == na->num_queues) { /* from/to host */
		if (priv->np_txpoll || want_tx) {
			/* push any packets up, then we are always ready */
			kring = &na->tx_rings[i];
			netmap_sync_to_host(na);
			revents |= want_tx;
		}
		if (want_rx) {
			kring = &na->rx_rings[i];
			if (kring->ring->avail == 0)
				netmap_sync_from_host(na, td);
			if (kring->ring->avail > 0) {
				revents |= want_rx;
			}
		}
		return (revents);
	}

	/*
	 * check_all is set if the card has more than one queue and
	 * the client is polling all of them. If true, we sleep on
	 * the "global" selfd, otherwise we sleep on individual selfd
	 * (we can only sleep on one of them per direction).
	 * The interrupt routine in the driver should always wake on
	 * the individual selfd, and also on the global one if the card
	 * has more than one ring.
	 *
	 * If the card has only one lock, we just use that.
	 * If the card has separate ring locks, we just use those
	 * unless we are doing check_all, in which case the whole
	 * loop is wrapped by the global lock.
	 * We acquire locks only when necessary: if poll is called
	 * when buffers are available, we can just return without locks.
	 *
	 * rxsync() is only called if we run out of buffers on a POLLIN.
	 * txsync() is called if we run out of buffers on POLLOUT, or
	 * there are pending packets to send. The latter can be disabled
	 * passing NETMAP_NO_TX_POLL in the NIOCREG call.
	 */
	check_all = (i + 1 != priv->np_qlast);

	/*
	 * core_lock indicates what to do with the core lock.
	 * The core lock is used when either the card has no individual
	 * locks, or it has individual locks but we are cheking all
	 * rings so we need the core lock to avoid missing wakeup events.
	 *
	 * It has three possible states:
	 * NO_CL	we don't need to use the core lock, e.g.
	 *		because we are protected by individual locks.
	 * NEED_CL	we need the core lock. In this case, when we
	 *		call the lock routine, move to LOCKED_CL
	 *		to remember to release the lock once done.
	 * LOCKED_CL	core lock is set, so we need to release it.
	 */
	enum {NO_CL, NEED_CL, LOCKED_CL };
	core_lock = (check_all || !na->separate_locks) ?  NEED_CL:NO_CL;
	/*
	 * We start with a lock free round which is good if we have
	 * data available. If this fails, then lock and call the sync
	 * routines.
	 */
		for (i = priv->np_qfirst; want_rx && i < priv->np_qlast; i++) {
			kring = &na->rx_rings[i];
			if (kring->ring->avail > 0) {
				revents |= want_rx;
				want_rx = 0;	/* also breaks the loop */
			}
		}
		for (i = priv->np_qfirst; want_tx && i < priv->np_qlast; i++) {
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
	 */
	if (priv->np_txpoll || want_tx) {
		for (i = priv->np_qfirst; i < priv->np_qlast; i++) {
			kring = &na->tx_rings[i];
			if (!want_tx && kring->ring->cur == kring->nr_hwcur)
				continue;
			if (core_lock == NEED_CL) {
				na->nm_lock(adapter, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(adapter, NETMAP_TX_LOCK, i);
			if (netmap_verbose & NM_VERB_TXSYNC)
				D("send %d on %s %d",
					kring->ring->cur,
					ifp->if_xname, i);
			if (na->nm_txsync(adapter, i, 0 /* no lock */))
				revents |= POLLERR;

			if (want_tx) {
				if (kring->ring->avail > 0) {
					/* stop at the first ring. We don't risk
					 * starvation.
					 */
					revents |= want_tx;
					want_tx = 0;
				} else if (!check_all)
					selrecord(td, &kring->si);
			}
			if (na->separate_locks)
				na->nm_lock(adapter, NETMAP_TX_UNLOCK, i);
		}
	}

	/*
	 * now if want_rx is still set we need to lock and rxsync.
	 * Do it on all rings because otherwise we starve.
	 */
	if (want_rx) {
		for (i = priv->np_qfirst; i < priv->np_qlast; i++) {
			kring = &na->rx_rings[i];
			if (core_lock == NEED_CL) {
				na->nm_lock(adapter, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(adapter, NETMAP_RX_LOCK, i);

			if (na->nm_rxsync(adapter, i, 0 /* no lock */))
				revents |= POLLERR;
			if (no_timestamp == 0 ||
					kring->ring->flags & NR_TIMESTAMP)
				microtime(&kring->ring->ts);

			if (kring->ring->avail > 0)
				revents |= want_rx;
			else if (!check_all)
				selrecord(td, &kring->si);
			if (na->separate_locks)
				na->nm_lock(adapter, NETMAP_RX_UNLOCK, i);
		}
	}
	if (check_all && revents == 0) {
		i = na->num_queues + 1; /* the global queue */
		if (want_tx)
			selrecord(td, &na->tx_rings[i].si);
		if (want_rx)
			selrecord(td, &na->rx_rings[i].si);
	}
	if (core_lock == LOCKED_CL)
		na->nm_lock(adapter, NETMAP_CORE_UNLOCK, 0);

	return (revents);
}

/*------- driver support routines ------*/

/*
 * Initialize a ``netmap_adapter`` object created by driver on attach.
 * We allocate a block of memory with room for a struct netmap_adapter
 * plus two sets of N+2 struct netmap_kring (where N is the number
 * of hardware rings):
 * krings	0..N-1	are for the hardware queues.
 * kring	N	is for the host stack queue
 * kring	N+1	is only used for the selinfo for all queues.
 * Return 0 on success, ENOMEM otherwise.
 */
int
netmap_attach(struct netmap_adapter *na, int num_queues)
{
	int n = num_queues + 2;
	int size = sizeof(*na) + 2 * n * sizeof(struct netmap_kring);
	void *buf;
	struct ifnet *ifp = na->ifp;

	if (ifp == NULL) {
		D("ifp not set, giving up");
		return EINVAL;
	}
	na->refcount = 0;
	na->num_queues = num_queues;

	buf = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (buf) {
		WNA(ifp) = buf;
		na->tx_rings = (void *)((char *)buf + sizeof(*na));
		na->rx_rings = na->tx_rings + n;
		bcopy(na, buf, sizeof(*na));
		ifp->if_capabilities |= IFCAP_NETMAP;
	}
	D("%s for %s", buf ? "ok" : "failed", ifp->if_xname);

	return (buf ? 0 : ENOMEM);
}


/*
 * Free the allocated memory linked to the given ``netmap_adapter``
 * object.
 */
void
netmap_detach(struct ifnet *ifp)
{
	u_int i;
	struct netmap_adapter *na = NA(ifp);

	if (!na)
		return;

	for (i = 0; i < na->num_queues + 2; i++) {
		knlist_destroy(&na->tx_rings[i].si.si_note);
		knlist_destroy(&na->rx_rings[i].si.si_note);
	}
	bzero(na, sizeof(*na));
	WNA(ifp) = NULL;
	free(na, M_DEVBUF);
}


/*
 * Intercept packets from the network stack and pass them
 * to netmap as incoming packets on the 'software' ring.
 * We are not locked when called.
 */
int
netmap_start(struct ifnet *ifp, struct mbuf *m)
{
	struct netmap_adapter *na = NA(ifp);
	struct netmap_kring *kring = &na->rx_rings[na->num_queues];
	u_int i, len = m->m_pkthdr.len;
	int error = EBUSY, lim = kring->nkr_num_slots - 1;
	struct netmap_slot *slot;

	if (netmap_verbose & NM_VERB_HOST)
		D("%s packet %d len %d from the stack", ifp->if_xname,
			kring->nr_hwcur + kring->nr_hwavail, len);
	na->nm_lock(ifp->if_softc, NETMAP_CORE_LOCK, 0);
	if (kring->nr_hwavail >= lim) {
		D("stack ring %s full\n", ifp->if_xname);
		goto done;	/* no space */
	}
	if (len > na->buff_size) {
		D("drop packet size %d > %d", len, na->buff_size);
		goto done;	/* too long for us */
	}

	/* compute the insert position */
	i = kring->nr_hwcur + kring->nr_hwavail;
	if (i > lim)
		i -= lim + 1;
	slot = &kring->ring->slot[i];
	m_copydata(m, 0, len, NMB(slot));
	slot->len = len;
	kring->nr_hwavail++;
	if (netmap_verbose  & NM_VERB_HOST)
		D("wake up host ring %s %d", na->ifp->if_xname, na->num_queues);
	selwakeuppri(&kring->si, PI_NET);
	error = 0;
done:
	na->nm_lock(ifp->if_softc, NETMAP_CORE_UNLOCK, 0);

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
 * If netmap mode is not set just return NULL.
 */
struct netmap_slot *
netmap_reset(struct netmap_adapter *na, enum txrx tx, int n,
	u_int new_cur)
{
	struct netmap_kring *kring;
	struct netmap_ring *ring;
	int new_hwofs, lim;

	if (na == NULL)
		return NULL;	/* no netmap support here */
	if (!(na->ifp->if_capenable & IFCAP_NETMAP))
		return NULL;	/* nothing to reinitialize */
	kring = tx == NR_TX ?  na->tx_rings + n : na->rx_rings + n;
	ring = kring->ring;
	lim = kring->nkr_num_slots - 1;

	if (tx == NR_TX)
		new_hwofs = kring->nr_hwcur - new_cur;
	else
		new_hwofs = kring->nr_hwcur + kring->nr_hwavail - new_cur;
	if (new_hwofs > lim)
		new_hwofs -= lim + 1;

	/* Alwayws set the new offset value and realign the ring. */
	kring->nkr_hwofs = new_hwofs;
	if (tx == NR_TX)
		kring->nr_hwavail = kring->nkr_num_slots - 1;
	D("new hwofs %d on %s %s[%d]",
			kring->nkr_hwofs, na->ifp->if_xname,
			tx == NR_TX ? "TX" : "RX", n);

	/*
	 * We do the wakeup here, but the ring is not yet reconfigured.
	 * However, we are under lock so there are no races.
	 */
	selwakeuppri(&kring->si, PI_NET);
	selwakeuppri(&kring[na->num_queues + 1 - n].si, PI_NET);
	return kring->ring->slot;
}

static void
ns_dmamap_cb(__unused void *arg, __unused bus_dma_segment_t * segs,
	__unused int nseg, __unused int error)
{
}

/* unload a bus_dmamap and create a new one. Used when the
 * buffer in the slot is changed.
 * XXX buflen is probably not needed, buffers have constant size.
 */
void
netmap_reload_map(bus_dma_tag_t tag, bus_dmamap_t map,
	void *buf, bus_size_t buflen)
{
	bus_addr_t paddr;
	bus_dmamap_unload(tag, map);
	bus_dmamap_load(tag, map, buf, buflen, ns_dmamap_cb, &paddr,
				BUS_DMA_NOWAIT);
}

void
netmap_load_map(bus_dma_tag_t tag, bus_dmamap_t map,
	void *buf, bus_size_t buflen)
{
	bus_addr_t paddr;
	bus_dmamap_load(tag, map, buf, buflen, ns_dmamap_cb, &paddr,
				BUS_DMA_NOWAIT);
}

/*------ netmap memory allocator -------*/
/*
 * Request for a chunk of memory.
 *
 * Memory objects are arranged into a list, hence we need to walk this
 * list until we find an object with the needed amount of data free. 
 * This sounds like a completely inefficient implementation, but given
 * the fact that data allocation is done once, we can handle it
 * flawlessly.
 *
 * Return NULL on failure.
 */
static void *
netmap_malloc(size_t size, __unused const char *msg)
{
	struct netmap_mem_obj *mem_obj, *new_mem_obj;
	void *ret = NULL;

	NMA_LOCK();
	TAILQ_FOREACH(mem_obj, &netmap_mem_d->nm_molist, nmo_next) {
		if (mem_obj->nmo_used != 0 || mem_obj->nmo_size < size)
			continue;

		new_mem_obj = malloc(sizeof(struct netmap_mem_obj), M_NETMAP,
				     M_WAITOK | M_ZERO);
		TAILQ_INSERT_BEFORE(mem_obj, new_mem_obj, nmo_next);

		new_mem_obj->nmo_used = 1;
		new_mem_obj->nmo_size = size;
		new_mem_obj->nmo_data = mem_obj->nmo_data;
		memset(new_mem_obj->nmo_data, 0, new_mem_obj->nmo_size);

		mem_obj->nmo_size -= size;
		mem_obj->nmo_data = (char *) mem_obj->nmo_data + size;
		if (mem_obj->nmo_size == 0) {
			TAILQ_REMOVE(&netmap_mem_d->nm_molist, mem_obj,
				     nmo_next);
			free(mem_obj, M_NETMAP);
		}

		ret = new_mem_obj->nmo_data;

		break;
	}
	NMA_UNLOCK();
	ND("%s: %d bytes at %p", msg, size, ret);

	return (ret);
}

/*
 * Return the memory to the allocator.
 *
 * While freeing a memory object, we try to merge adjacent chunks in
 * order to reduce memory fragmentation.
 */
static void
netmap_free(void *addr, const char *msg)
{
	size_t size;
	struct netmap_mem_obj *cur, *prev, *next;

	if (addr == NULL) {
		D("NULL addr for %s", msg);
		return;
	}

	NMA_LOCK();
	TAILQ_FOREACH(cur, &netmap_mem_d->nm_molist, nmo_next) {
		if (cur->nmo_data == addr && cur->nmo_used)
			break;
	}
	if (cur == NULL) {
		NMA_UNLOCK();
		D("invalid addr %s %p", msg, addr);
		return;
	}

	size = cur->nmo_size;
	cur->nmo_used = 0;

	/* merge current chunk of memory with the previous one,
	   if present. */
	prev = TAILQ_PREV(cur, netmap_mem_obj_h, nmo_next);
	if (prev && prev->nmo_used == 0) {
		TAILQ_REMOVE(&netmap_mem_d->nm_molist, cur, nmo_next);
		prev->nmo_size += cur->nmo_size;
		free(cur, M_NETMAP);
		cur = prev;
	}

	/* merge with the next one */
	next = TAILQ_NEXT(cur, nmo_next);
	if (next && next->nmo_used == 0) {
		TAILQ_REMOVE(&netmap_mem_d->nm_molist, next, nmo_next);
		cur->nmo_size += next->nmo_size;
		free(next, M_NETMAP);
	}
	NMA_UNLOCK();
	ND("freed %s %d bytes at %p", msg, size, addr);
}


/*
 * Initialize the memory allocator.
 *
 * Create the descriptor for the memory , allocate the pool of memory
 * and initialize the list of memory objects with a single chunk
 * containing the whole pre-allocated memory marked as free.
 *
 * Start with a large size, then halve as needed if we fail to
 * allocate the block. While halving, always add one extra page
 * because buffers 0 and 1 are used for special purposes.
 * Return 0 on success, errno otherwise.
 */
static int
netmap_memory_init(void)
{
	struct netmap_mem_obj *mem_obj;
	void *buf = NULL;
	int i, n, sz = NETMAP_MEMORY_SIZE;
	int extra_sz = 0; // space for rings and two spare buffers

	for (; !buf && sz >= 1<<20; sz >>=1) {
		extra_sz = sz/200;
		extra_sz = (extra_sz + 2*PAGE_SIZE - 1) & ~(PAGE_SIZE-1);
	        buf = contigmalloc(sz + extra_sz,
			     M_NETMAP,
			     M_WAITOK | M_ZERO,
			     0, /* low address */
			     -1UL, /* high address */
			     PAGE_SIZE, /* alignment */
			     0 /* boundary */
			    );
	} 
	if (buf == NULL)
		return (ENOMEM);
	sz += extra_sz;
	netmap_mem_d = malloc(sizeof(struct netmap_mem_d), M_NETMAP,
			      M_WAITOK | M_ZERO);
	mtx_init(&netmap_mem_d->nm_mtx, "netmap memory allocator lock", NULL,
		 MTX_DEF);
	TAILQ_INIT(&netmap_mem_d->nm_molist);
	netmap_mem_d->nm_buffer = buf;
	netmap_mem_d->nm_totalsize = sz;

	/*
	 * A buffer takes 2k, a slot takes 8 bytes + ring overhead,
	 * so the ratio is 200:1. In other words, we can use 1/200 of
	 * the memory for the rings, and the rest for the buffers,
	 * and be sure we never run out.
	 */
	netmap_mem_d->nm_size = sz/200;
	netmap_mem_d->nm_buf_start =
		(netmap_mem_d->nm_size + PAGE_SIZE - 1) & ~(PAGE_SIZE-1);
	netmap_mem_d->nm_buf_len = sz - netmap_mem_d->nm_buf_start;

	nm_buf_pool.base = netmap_mem_d->nm_buffer;
	nm_buf_pool.base += netmap_mem_d->nm_buf_start;
	netmap_buffer_base = nm_buf_pool.base;
	D("netmap_buffer_base %p (offset %d)",
		netmap_buffer_base, (int)netmap_mem_d->nm_buf_start);
	/* number of buffers, they all start as free */

	netmap_total_buffers = nm_buf_pool.total_buffers =
		netmap_mem_d->nm_buf_len / NETMAP_BUF_SIZE;
	nm_buf_pool.bufsize = NETMAP_BUF_SIZE;

	D("Have %d MB, use %dKB for rings, %d buffers at %p",
		(sz >> 20), (int)(netmap_mem_d->nm_size >> 10),
		nm_buf_pool.total_buffers, nm_buf_pool.base);

	/* allocate and initialize the bitmap. Entry 0 is considered
	 * always busy (used as default when there are no buffers left).
	 */
	n = (nm_buf_pool.total_buffers + 31) / 32;
	nm_buf_pool.bitmap = malloc(sizeof(uint32_t) * n, M_NETMAP,
			 M_WAITOK | M_ZERO);
	nm_buf_pool.bitmap[0] = ~3; /* slot 0 and 1 always busy */
	for (i = 1; i < n; i++)
		nm_buf_pool.bitmap[i] = ~0;
	nm_buf_pool.free = nm_buf_pool.total_buffers - 2;
	
	mem_obj = malloc(sizeof(struct netmap_mem_obj), M_NETMAP,
			 M_WAITOK | M_ZERO);
	TAILQ_INSERT_HEAD(&netmap_mem_d->nm_molist, mem_obj, nmo_next);
	mem_obj->nmo_used = 0;
	mem_obj->nmo_size = netmap_mem_d->nm_size;
	mem_obj->nmo_data = netmap_mem_d->nm_buffer;

	return (0);
}


/*
 * Finalize the memory allocator.
 *
 * Free all the memory objects contained inside the list, and deallocate
 * the pool of memory; finally free the memory allocator descriptor.
 */
static void
netmap_memory_fini(void)
{
	struct netmap_mem_obj *mem_obj;

	while (!TAILQ_EMPTY(&netmap_mem_d->nm_molist)) {
		mem_obj = TAILQ_FIRST(&netmap_mem_d->nm_molist);
		TAILQ_REMOVE(&netmap_mem_d->nm_molist, mem_obj, nmo_next);
		if (mem_obj->nmo_used == 1) {
			printf("netmap: leaked %d bytes at %p\n",
			       (int)mem_obj->nmo_size,
			       mem_obj->nmo_data);
		}
		free(mem_obj, M_NETMAP);
	}
	contigfree(netmap_mem_d->nm_buffer, netmap_mem_d->nm_totalsize, M_NETMAP);
	// XXX mutex_destroy(nm_mtx);
	free(netmap_mem_d, M_NETMAP);
}


/*
 * Module loader.
 *
 * Create the /dev/netmap device and initialize all global
 * variables.
 *
 * Return 0 on success, errno on failure.
 */
static int
netmap_init(void)
{
	int error;


	error = netmap_memory_init();
	if (error != 0) {
		printf("netmap: unable to initialize the memory allocator.");
		return (error);
	}
	printf("netmap: loaded module with %d Mbytes\n",
		(int)(netmap_mem_d->nm_totalsize >> 20));

	netmap_dev = make_dev(&netmap_cdevsw, 0, UID_ROOT, GID_WHEEL, 0660,
			      "netmap");

	return (0);
}


/*
 * Module unloader.
 *
 * Free all the memory, and destroy the ``/dev/netmap`` device.
 */
static void
netmap_fini(void)
{
	destroy_dev(netmap_dev);

	netmap_memory_fini();

	printf("netmap: unloaded module.\n");
}


/*
 * Kernel entry point.
 *
 * Initialize/finalize the module and return.
 *
 * Return 0 on success, errno on failure.
 */
static int
netmap_loader(__unused struct module *module, int event, __unused void *arg)
{
	int error = 0;

	switch (event) {
	case MOD_LOAD:
		error = netmap_init();
		break;

	case MOD_UNLOAD:
		netmap_fini();
		break;

	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}


DEV_MODULE(netmap, netmap_loader, NULL);
