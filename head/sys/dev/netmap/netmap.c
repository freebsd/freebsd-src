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
#define NMA_LOCK()	mtx_lock(&nm_mem->nm_mtx);
#define NMA_UNLOCK()	mtx_unlock(&nm_mem->nm_mtx);
struct netmap_mem_d;
static struct netmap_mem_d *nm_mem;	/* Our memory allocator. */

u_int netmap_total_buffers;
char *netmap_buffer_base;	/* address of an invalid buffer */

/* user-controlled variables */
int netmap_verbose;

static int netmap_no_timestamp; /* don't timestamp on rxsync */

SYSCTL_NODE(_dev, OID_AUTO, netmap, CTLFLAG_RW, 0, "Netmap args");
SYSCTL_INT(_dev_netmap, OID_AUTO, verbose,
    CTLFLAG_RW, &netmap_verbose, 0, "Verbose mode");
SYSCTL_INT(_dev_netmap, OID_AUTO, no_timestamp,
    CTLFLAG_RW, &netmap_no_timestamp, 0, "no_timestamp");
int netmap_buf_size = 2048;
TUNABLE_INT("hw.netmap.buf_size", &netmap_buf_size);
SYSCTL_INT(_dev_netmap, OID_AUTO, buf_size,
    CTLFLAG_RD, &netmap_buf_size, 0, "Size of packet buffers");
int netmap_mitigate = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, mitigate, CTLFLAG_RW, &netmap_mitigate, 0, "");
int netmap_no_pendintr = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, no_pendintr,
    CTLFLAG_RW, &netmap_no_pendintr, 0, "Always look for new received packets.");


/*------------- memory allocator -----------------*/
#ifdef NETMAP_MEM2
#include "netmap_mem2.c"
#else /* !NETMAP_MEM2 */
#include "netmap_mem1.c"
#endif /* !NETMAP_MEM2 */
/*------------ end of memory allocator ----------*/

/* Structure associated to each thread which registered an interface. */
struct netmap_priv_d {
	struct netmap_if *np_nifp;	/* netmap interface descriptor. */

	struct ifnet	*np_ifp;	/* device for which we hold a reference */
	int		np_ringid;	/* from the ioctl */
	u_int		np_qfirst, np_qlast;	/* range of rings to scan */
	uint16_t	np_txpoll;
};


/*
 * File descriptor's private data destructor.
 *
 * Call nm_register(ifp,0) to stop netmap mode on the interface and
 * revert to normal operation. We expect that np_ifp has not gone.
 */
static void
netmap_dtor_locked(void *data)
{
	struct netmap_priv_d *priv = data;
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);
	struct netmap_if *nifp = priv->np_nifp;

	na->refcount--;
	if (na->refcount <= 0) {	/* last instance */
		u_int i, j, lim;

		D("deleting last netmap instance for %s", ifp->if_xname);
		/*
		 * there is a race here with *_netmap_task() and
		 * netmap_poll(), which don't run under NETMAP_REG_LOCK.
		 * na->refcount == 0 && na->ifp->if_capenable & IFCAP_NETMAP
		 * (aka NETMAP_DELETING(na)) are a unique marker that the
		 * device is dying.
		 * Before destroying stuff we sleep a bit, and then complete
		 * the job. NIOCREG should realize the condition and
		 * loop until they can continue; the other routines
		 * should check the condition at entry and quit if
		 * they cannot run.
		 */
		na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
		tsleep(na, 0, "NIOCUNREG", 4);
		na->nm_lock(ifp, NETMAP_REG_LOCK, 0);
		na->nm_register(ifp, 0); /* off, clear IFCAP_NETMAP */
		/* Wake up any sleeping threads. netmap_poll will
		 * then return POLLERR
		 */
		for (i = 0; i < na->num_tx_rings + 1; i++)
			selwakeuppri(&na->tx_rings[i].si, PI_NET);
		for (i = 0; i < na->num_rx_rings + 1; i++)
			selwakeuppri(&na->rx_rings[i].si, PI_NET);
		selwakeuppri(&na->tx_si, PI_NET);
		selwakeuppri(&na->rx_si, PI_NET);
		/* release all buffers */
		NMA_LOCK();
		for (i = 0; i < na->num_tx_rings + 1; i++) {
			struct netmap_ring *ring = na->tx_rings[i].ring;
			lim = na->tx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(nifp, ring->slot[j].buf_idx);
		}
		for (i = 0; i < na->num_rx_rings + 1; i++) {
			struct netmap_ring *ring = na->rx_rings[i].ring;
			lim = na->rx_rings[i].nkr_num_slots;
			for (j = 0; j < lim; j++)
				netmap_free_buf(nifp, ring->slot[j].buf_idx);
		}
		NMA_UNLOCK();
		netmap_free_rings(na);
		wakeup(na);
	}
	netmap_if_free(nifp);
}


static void
netmap_dtor(void *data)
{
	struct netmap_priv_d *priv = data;
	struct ifnet *ifp = priv->np_ifp;
	struct netmap_adapter *na = NA(ifp);

	na->nm_lock(ifp, NETMAP_REG_LOCK, 0);
	netmap_dtor_locked(data);
	na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);

	if_rele(ifp);
	bzero(priv, sizeof(*priv));	/* XXX for safety */
	free(priv, M_DEVBUF);
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
netmap_mmap(__unused struct cdev *dev,
#if __FreeBSD_version < 900000
		vm_offset_t offset, vm_paddr_t *paddr, int nprot
#else
		vm_ooffset_t offset, vm_paddr_t *paddr, int nprot,
		__unused vm_memattr_t *memattr
#endif
	)
{
	if (nprot & PROT_EXEC)
		return (-1);	// XXX -1 or EINVAL ?

	ND("request for offset 0x%x", (uint32_t)offset);
	*paddr = netmap_ofstophys(offset);

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
	struct netmap_kring *kring = &na->tx_rings[na->num_tx_rings];
	struct netmap_ring *ring = kring->ring;
	struct mbuf *head = NULL, *tail = NULL, *m;
	u_int k, n, lim = kring->nkr_num_slots - 1;

	k = ring->cur;
	if (k > lim) {
		netmap_ring_reinit(kring);
		return;
	}
	// na->nm_lock(na->ifp, NETMAP_CORE_LOCK, 0);

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
	// na->nm_lock(na->ifp, NETMAP_CORE_UNLOCK, 0);

	/* send packets up, outside the lock */
	while ((m = head) != NULL) {
		head = head->m_nextpkt;
		m->m_nextpkt = NULL;
		if (netmap_verbose & NM_VERB_HOST)
			D("sending up pkt %p size %d", m, MBUF_LEN(m));
		NM_SEND_UP(na->ifp, m);
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
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	struct netmap_ring *ring = kring->ring;
	u_int j, n, lim = kring->nkr_num_slots;
	u_int k = ring->cur, resvd = ring->reserved;

	na->nm_lock(na->ifp, NETMAP_CORE_LOCK, 0);
	if (k >= lim) {
		netmap_ring_reinit(kring);
		return;
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
	na->nm_lock(na->ifp, NETMAP_CORE_UNLOCK, 0);
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
		int n = kring->na->num_tx_rings + 1;

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
	u_int i = ringid & NETMAP_RING_MASK;
	/* initially (np_qfirst == np_qlast) we don't want to lock */
	int need_lock = (priv->np_qfirst != priv->np_qlast);
	int lim = na->num_rx_rings;

	if (na->num_tx_rings > lim)
		lim = na->num_tx_rings;
	if ( (ringid & NETMAP_HW_RING) && i >= lim) {
		D("invalid ring id %d", i);
		return (EINVAL);
	}
	if (need_lock)
		na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
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
	if (need_lock)
		na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);
	if (ringid & NETMAP_SW_RING)
		D("ringid %s set to SW RING", ifp->if_xname);
	else if (ringid & NETMAP_HW_RING)
		D("ringid %s set to HW RING %d", ifp->if_xname,
			priv->np_qfirst);
	else
		D("ringid %s set to all %d HW RINGS", ifp->if_xname, lim);
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
	int error;
	u_int i, lim;
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
		nmr->nr_memsize = nm_mem->nm_totalsize;
		nmr->nr_offset = 0;
		nmr->nr_rx_rings = nmr->nr_tx_rings = 0;
		nmr->nr_rx_slots = nmr->nr_tx_slots = 0;
		if (nmr->nr_version != NETMAP_API) {
			D("API mismatch got %d have %d",
				nmr->nr_version, NETMAP_API);
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		if (nmr->nr_name[0] == '\0')	/* just get memory info */
			break;
		error = get_ifp(nmr->nr_name, &ifp); /* get a refcount */
		if (error)
			break;
		na = NA(ifp); /* retrieve netmap_adapter */
		nmr->nr_rx_rings = na->num_rx_rings;
		nmr->nr_tx_rings = na->num_tx_rings;
		nmr->nr_rx_slots = na->num_rx_desc;
		nmr->nr_tx_slots = na->num_tx_desc;
		if_rele(ifp);	/* return the refcount */
		break;

	case NIOCREGIF:
		if (nmr->nr_version != NETMAP_API) {
			nmr->nr_version = NETMAP_API;
			error = EINVAL;
			break;
		}
		if (priv != NULL) {	/* thread already registered */
			error = netmap_set_ringid(priv, nmr->nr_ringid);
			break;
		}
		/* find the interface and a reference */
		error = get_ifp(nmr->nr_name, &ifp); /* keep reference */
		if (error)
			break;
		na = NA(ifp); /* retrieve netmap adapter */
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
			na->nm_lock(ifp, NETMAP_REG_LOCK, 0);
			if (!NETMAP_DELETING(na))
				break;
			na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
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
			if (error)
				netmap_dtor_locked(priv);
		}

		if (error) {	/* reg. failed, release priv and ref */
error:
			na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
			if_rele(ifp);	/* return the refcount */
			bzero(priv, sizeof(*priv));
			free(priv, M_DEVBUF);
			break;
		}

		na->nm_lock(ifp, NETMAP_REG_UNLOCK, 0);
		error = devfs_set_cdevpriv(priv, netmap_dtor);

		if (error != 0) {
			/* could not assign the private storage for the
			 * thread, call the destructor explicitly.
			 */
			netmap_dtor(priv);
			break;
		}

		/* return the offset of the netmap_if object */
		nmr->nr_rx_rings = na->num_rx_rings;
		nmr->nr_tx_rings = na->num_tx_rings;
		nmr->nr_rx_slots = na->num_rx_desc;
		nmr->nr_tx_slots = na->num_tx_desc;
		nmr->nr_memsize = nm_mem->nm_totalsize;
		nmr->nr_offset = netmap_if_offset(nifp);
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
		if (priv->np_qfirst == NETMAP_SW_RING) { /* host rings */
			if (cmd == NIOCTXSYNC)
				netmap_sync_to_host(na);
			else
				netmap_sync_from_host(na, NULL);
			break;
		}
		/* find the last ring to scan */
		lim = priv->np_qlast;
		if (lim == NETMAP_HW_RING)
			lim = (cmd == NIOCTXSYNC) ?
			    na->num_tx_rings : na->num_rx_rings;

		for (i = priv->np_qfirst; i < lim; i++) {
			if (cmd == NIOCTXSYNC) {
				struct netmap_kring *kring = &na->tx_rings[i];
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("pre txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
				na->nm_txsync(ifp, i, 1 /* do lock */);
				if (netmap_verbose & NM_VERB_TXSYNC)
					D("post txsync ring %d cur %d hwcur %d",
					    i, kring->ring->cur,
					    kring->nr_hwcur);
			} else {
				na->nm_rxsync(ifp, i, 1 /* do lock */);
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

	default:	/* allow device-specific ioctls */
	    {
		struct socket so;
		bzero(&so, sizeof(so));
		error = get_ifp(nmr->nr_name, &ifp); /* keep reference */
		if (error)
			break;
		so.so_vnet = ifp->if_vnet;
		// so->so_proto not null.
		error = ifioctl(&so, cmd, data, td);
		if_rele(ifp);
		break;
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
	u_int lim_tx, lim_rx;
	enum {NO_CL, NEED_CL, LOCKED_CL }; /* see below */

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

	na = NA(ifp); /* retrieve netmap adapter */

	lim_tx = na->num_tx_rings;
	lim_rx = na->num_rx_rings;
	/* how many queues we are scanning */
	if (priv->np_qfirst == NETMAP_SW_RING) {
		if (priv->np_txpoll || want_tx) {
			/* push any packets up, then we are always ready */
			kring = &na->tx_rings[lim_tx];
			netmap_sync_to_host(na);
			revents |= want_tx;
		}
		if (want_rx) {
			kring = &na->rx_rings[lim_rx];
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
	check_all = (priv->np_qlast == NETMAP_HW_RING) && (lim_tx > 1 || lim_rx > 1);

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
	core_lock = (check_all || !na->separate_locks) ? NEED_CL : NO_CL;
	if (priv->np_qlast != NETMAP_HW_RING) {
		lim_tx = lim_rx = priv->np_qlast;
	}

	/*
	 * We start with a lock free round which is good if we have
	 * data available. If this fails, then lock and call the sync
	 * routines.
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
	 */
	if (priv->np_txpoll || want_tx) {
		for (i = priv->np_qfirst; i < lim_tx; i++) {
			kring = &na->tx_rings[i];
			/*
			 * Skip the current ring if want_tx == 0
			 * (we have already done a successful sync on
			 * a previous ring) AND kring->cur == kring->hwcur
			 * (there are no pending transmissions for this ring).
			 */
			if (!want_tx && kring->ring->cur == kring->nr_hwcur)
				continue;
			if (core_lock == NEED_CL) {
				na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_TX_LOCK, i);
			if (netmap_verbose & NM_VERB_TXSYNC)
				D("send %d on %s %d",
					kring->ring->cur,
					ifp->if_xname, i);
			if (na->nm_txsync(ifp, i, 0 /* no lock */))
				revents |= POLLERR;

			/* Check avail/call selrecord only if called with POLLOUT */
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
				na->nm_lock(ifp, NETMAP_TX_UNLOCK, i);
		}
	}

	/*
	 * now if want_rx is still set we need to lock and rxsync.
	 * Do it on all rings because otherwise we starve.
	 */
	if (want_rx) {
		for (i = priv->np_qfirst; i < lim_rx; i++) {
			kring = &na->rx_rings[i];
			if (core_lock == NEED_CL) {
				na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
				core_lock = LOCKED_CL;
			}
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_RX_LOCK, i);

			if (na->nm_rxsync(ifp, i, 0 /* no lock */))
				revents |= POLLERR;
			if (netmap_no_timestamp == 0 ||
					kring->ring->flags & NR_TIMESTAMP) {
				microtime(&kring->ring->ts);
			}

			if (kring->ring->avail > 0)
				revents |= want_rx;
			else if (!check_all)
				selrecord(td, &kring->si);
			if (na->separate_locks)
				na->nm_lock(ifp, NETMAP_RX_UNLOCK, i);
		}
	}
	if (check_all && revents == 0) { /* signal on the global queue */
		if (want_tx)
			selrecord(td, &na->tx_si);
		if (want_rx)
			selrecord(td, &na->rx_si);
	}
	if (core_lock == LOCKED_CL)
		na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);

	return (revents);
}

/*------- driver support routines ------*/

/*
 * default lock wrapper.
 */
static void
netmap_lock_wrapper(struct ifnet *dev, int what, u_int queueid)
{
	struct netmap_adapter *na = NA(dev);

	switch (what) {
#ifdef linux	/* some system do not need lock on register */
	case NETMAP_REG_LOCK:
	case NETMAP_REG_UNLOCK:
		break;
#endif /* linux */

	case NETMAP_CORE_LOCK:
		mtx_lock(&na->core_lock);
		break;

	case NETMAP_CORE_UNLOCK:
		mtx_unlock(&na->core_lock);
		break;

	case NETMAP_TX_LOCK:
		mtx_lock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_TX_UNLOCK:
		mtx_unlock(&na->tx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_LOCK:
		mtx_lock(&na->rx_rings[queueid].q_lock);
		break;

	case NETMAP_RX_UNLOCK:
		mtx_unlock(&na->rx_rings[queueid].q_lock);
		break;
	}
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
 * na->num_tx_rings can be set for cards with different tx/rx setups
 */
int
netmap_attach(struct netmap_adapter *na, int num_queues)
{
	int i, n, size;
	void *buf;
	struct ifnet *ifp = na->ifp;

	if (ifp == NULL) {
		D("ifp not set, giving up");
		return EINVAL;
	}
	/* clear other fields ? */
	na->refcount = 0;
	if (na->num_tx_rings == 0)
		na->num_tx_rings = num_queues;
	na->num_rx_rings = num_queues;
	/* on each direction we have N+1 resources
	 * 0..n-1	are the hardware rings
	 * n		is the ring attached to the stack.
	 */
	n = na->num_rx_rings + na->num_tx_rings + 2;
	size = sizeof(*na) + n * sizeof(struct netmap_kring);

	buf = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (buf) {
		WNA(ifp) = buf;
		na->tx_rings = (void *)((char *)buf + sizeof(*na));
		na->rx_rings = na->tx_rings + na->num_tx_rings + 1;
		bcopy(na, buf, sizeof(*na));
		ifp->if_capabilities |= IFCAP_NETMAP;

		na = buf;
		if (na->nm_lock == NULL)
			na->nm_lock = netmap_lock_wrapper;
		mtx_init(&na->core_lock, "netmap core lock", NULL, MTX_DEF);
		for (i = 0 ; i < na->num_tx_rings + 1; i++)
			mtx_init(&na->tx_rings[i].q_lock, "netmap txq lock", NULL, MTX_DEF);
		for (i = 0 ; i < na->num_rx_rings + 1; i++)
			mtx_init(&na->rx_rings[i].q_lock, "netmap rxq lock", NULL, MTX_DEF);
	}
#ifdef linux
	D("netdev_ops %p", ifp->netdev_ops);
	/* prepare a clone of the netdev ops */
	na->nm_ndo = *ifp->netdev_ops;
	na->nm_ndo.ndo_start_xmit = netmap_start_linux;
#endif
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

	for (i = 0; i < na->num_tx_rings + 1; i++) {
		knlist_destroy(&na->tx_rings[i].si.si_note);
		mtx_destroy(&na->tx_rings[i].q_lock);
	}
	for (i = 0; i < na->num_rx_rings + 1; i++) {
		knlist_destroy(&na->rx_rings[i].si.si_note);
		mtx_destroy(&na->rx_rings[i].q_lock);
	}
	knlist_destroy(&na->tx_si.si_note);
	knlist_destroy(&na->rx_si.si_note);
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
	struct netmap_kring *kring = &na->rx_rings[na->num_rx_rings];
	u_int i, len = MBUF_LEN(m);
	int error = EBUSY, lim = kring->nkr_num_slots - 1;
	struct netmap_slot *slot;

	if (netmap_verbose & NM_VERB_HOST)
		D("%s packet %d len %d from the stack", ifp->if_xname,
			kring->nr_hwcur + kring->nr_hwavail, len);
	na->nm_lock(ifp, NETMAP_CORE_LOCK, 0);
	if (kring->nr_hwavail >= lim) {
		if (netmap_verbose)
			D("stack ring %s full\n", ifp->if_xname);
		goto done;	/* no space */
	}
	if (len > NETMAP_BUF_SIZE) {
		D("drop packet size %d > %d", len, NETMAP_BUF_SIZE);
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
		D("wake up host ring %s %d", na->ifp->if_xname, na->num_rx_rings);
	selwakeuppri(&kring->si, PI_NET);
	error = 0;
done:
	na->nm_lock(ifp, NETMAP_CORE_UNLOCK, 0);

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
	int new_hwofs, lim;

	if (na == NULL)
		return NULL;	/* no netmap support here */
	if (!(na->ifp->if_capenable & IFCAP_NETMAP))
		return NULL;	/* nothing to reinitialize */

	if (tx == NR_TX) {
		kring = na->tx_rings + n;
		new_hwofs = kring->nr_hwcur - new_cur;
	} else {
		kring = na->rx_rings + n;
		new_hwofs = kring->nr_hwcur + kring->nr_hwavail - new_cur;
	}
	lim = kring->nkr_num_slots - 1;
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
	 * Wakeup on the individual and global lock
	 * We do the wakeup here, but the ring is not yet reconfigured.
	 * However, we are under lock so there are no races.
	 */
	selwakeuppri(&kring->si, PI_NET);
	selwakeuppri(tx == NR_TX ? &na->tx_si : &na->rx_si, PI_NET);
	return kring->ring->slot;
}


/*
 * Default functions to handle rx/tx interrupts
 * we have 4 cases:
 * 1 ring, single lock:
 *	lock(core); wake(i=0); unlock(core)
 * N rings, single lock:
 *	lock(core); wake(i); wake(N+1) unlock(core)
 * 1 ring, separate locks: (i=0)
 *	lock(i); wake(i); unlock(i)
 * N rings, separate locks:
 *	lock(i); wake(i); unlock(i); lock(core) wake(N+1) unlock(core)
 * work_done is non-null on the RX path.
 */
int
netmap_rx_irq(struct ifnet *ifp, int q, int *work_done)
{
	struct netmap_adapter *na;
	struct netmap_kring *r;
	NM_SELINFO_T *main_wq;

	if (!(ifp->if_capenable & IFCAP_NETMAP))
		return 0;
	na = NA(ifp);
	if (work_done) { /* RX path */
		r = na->rx_rings + q;
		r->nr_kflags |= NKR_PENDINTR;
		main_wq = (na->num_rx_rings > 1) ? &na->rx_si : NULL;
	} else { /* tx path */
		r = na->tx_rings + q;
		main_wq = (na->num_tx_rings > 1) ? &na->tx_si : NULL;
		work_done = &q; /* dummy */
	}
	if (na->separate_locks) {
		mtx_lock(&r->q_lock);
		selwakeuppri(&r->si, PI_NET);
		mtx_unlock(&r->q_lock);
		if (main_wq) {
			mtx_lock(&na->core_lock);
			selwakeuppri(main_wq, PI_NET);
			mtx_unlock(&na->core_lock);
		}
	} else {
		mtx_lock(&na->core_lock);
		selwakeuppri(&r->si, PI_NET);
		if (main_wq)
			selwakeuppri(main_wq, PI_NET);
		mtx_unlock(&na->core_lock);
	}
	*work_done = 1; /* do not fire napi again */
	return 1;
}


static struct cdevsw netmap_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "netmap",
	.d_mmap = netmap_mmap,
	.d_ioctl = netmap_ioctl,
	.d_poll = netmap_poll,
};


static struct cdev *netmap_dev; /* /dev/netmap character device. */


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
		(int)(nm_mem->nm_totalsize >> 20));
	netmap_dev = make_dev(&netmap_cdevsw, 0, UID_ROOT, GID_WHEEL, 0660,
			      "netmap");
	return (error);
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
