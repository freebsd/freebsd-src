/*
 * Copyright (C) 2013-2014 Universita` di Pisa. All rights reserved.
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

/* $FreeBSD$ */

#include <sys/types.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/poll.h>  /* POLLIN, POLLOUT */
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/conf.h>	/* DEV_MODULE */
#include <sys/endian.h>

#include <sys/rwlock.h>

#include <vm/vm.h>      /* vtophys */
#include <vm/pmap.h>    /* vtophys */
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>


#include <sys/malloc.h>
#include <sys/socket.h> /* sockaddrs */
#include <sys/selinfo.h>
#include <net/if.h>
#include <net/if_var.h>
#include <machine/bus.h>        /* bus_dmamap_* */
#include <netinet/in.h>		/* in6_cksum_pseudo() */
#include <machine/in_cksum.h>  /* in_pseudo(), in_cksum_hdr() */

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>


/* ======================== FREEBSD-SPECIFIC ROUTINES ================== */

rawsum_t
nm_csum_raw(uint8_t *data, size_t len, rawsum_t cur_sum)
{
	/* TODO XXX please use the FreeBSD implementation for this. */
	uint16_t *words = (uint16_t *)data;
	int nw = len / 2;
	int i;

	for (i = 0; i < nw; i++)
		cur_sum += be16toh(words[i]);

	if (len & 1)
		cur_sum += (data[len-1] << 8);

	return cur_sum;
}

/* Fold a raw checksum: 'cur_sum' is in host byte order, while the
 * return value is in network byte order.
 */
uint16_t
nm_csum_fold(rawsum_t cur_sum)
{
	/* TODO XXX please use the FreeBSD implementation for this. */
	while (cur_sum >> 16)
		cur_sum = (cur_sum & 0xFFFF) + (cur_sum >> 16);

	return htobe16((~cur_sum) & 0xFFFF);
}

uint16_t
nm_csum_ipv4(struct nm_iphdr *iph)
{
#if 0
	return in_cksum_hdr((void *)iph);
#else
	return nm_csum_fold(nm_csum_raw((uint8_t*)iph, sizeof(struct nm_iphdr), 0));
#endif
}

void
nm_csum_tcpudp_ipv4(struct nm_iphdr *iph, void *data,
					size_t datalen, uint16_t *check)
{
#ifdef INET
	uint16_t pseudolen = datalen + iph->protocol;

	/* Compute and insert the pseudo-header cheksum. */
	*check = in_pseudo(iph->saddr, iph->daddr,
				 htobe16(pseudolen));
	/* Compute the checksum on TCP/UDP header + payload
	 * (includes the pseudo-header).
	 */
	*check = nm_csum_fold(nm_csum_raw(data, datalen, 0));
#else
	static int notsupported = 0;
	if (!notsupported) {
		notsupported = 1;
		D("inet4 segmentation not supported");
	}
#endif
}

void
nm_csum_tcpudp_ipv6(struct nm_ipv6hdr *ip6h, void *data,
					size_t datalen, uint16_t *check)
{
#ifdef INET6
	*check = in6_cksum_pseudo((void*)ip6h, datalen, ip6h->nexthdr, 0);
	*check = nm_csum_fold(nm_csum_raw(data, datalen, 0));
#else
	static int notsupported = 0;
	if (!notsupported) {
		notsupported = 1;
		D("inet6 segmentation not supported");
	}
#endif
}


/*
 * Intercept the rx routine in the standard device driver.
 * Second argument is non-zero to intercept, 0 to restore
 */
int
netmap_catch_rx(struct netmap_adapter *na, int intercept)
{
	struct netmap_generic_adapter *gna =
		(struct netmap_generic_adapter *)na;
	struct ifnet *ifp = na->ifp;

	if (intercept) {
		if (gna->save_if_input) {
			D("cannot intercept again");
			return EINVAL; /* already set */
		}
		gna->save_if_input = ifp->if_input;
		ifp->if_input = generic_rx_handler;
	} else {
		if (!gna->save_if_input){
			D("cannot restore");
			return EINVAL;  /* not saved */
		}
		ifp->if_input = gna->save_if_input;
		gna->save_if_input = NULL;
	}

	return 0;
}


/*
 * Intercept the packet steering routine in the tx path,
 * so that we can decide which queue is used for an mbuf.
 * Second argument is non-zero to intercept, 0 to restore.
 * On freebsd we just intercept if_transmit.
 */
void
netmap_catch_tx(struct netmap_generic_adapter *gna, int enable)
{
	struct netmap_adapter *na = &gna->up.up;
	struct ifnet *ifp = na->ifp;

	if (enable) {
		na->if_transmit = ifp->if_transmit;
		ifp->if_transmit = netmap_transmit;
	} else {
		ifp->if_transmit = na->if_transmit;
	}
}


/*
 * Transmit routine used by generic_netmap_txsync(). Returns 0 on success
 * and non-zero on error (which may be packet drops or other errors).
 * addr and len identify the netmap buffer, m is the (preallocated)
 * mbuf to use for transmissions.
 *
 * We should add a reference to the mbuf so the m_freem() at the end
 * of the transmission does not consume resources.
 *
 * On FreeBSD, and on multiqueue cards, we can force the queue using
 *      if ((m->m_flags & M_FLOWID) != 0)
 *              i = m->m_pkthdr.flowid % adapter->num_queues;
 *      else
 *              i = curcpu % adapter->num_queues;
 *
 */
int
generic_xmit_frame(struct ifnet *ifp, struct mbuf *m,
	void *addr, u_int len, u_int ring_nr)
{
	int ret;

	/*
	 * The mbuf should be a cluster from our special pool,
	 * so we do not need to do an m_copyback but just copy
	 * (and eventually, just reference the netmap buffer)
	 */

	if (*m->m_ext.ext_cnt != 1) {
		D("invalid refcnt %d for %p",
			*m->m_ext.ext_cnt, m);
		panic("in generic_xmit_frame");
	}
	// XXX the ext_size check is unnecessary if we link the netmap buf
	if (m->m_ext.ext_size < len) {
		RD(5, "size %d < len %d", m->m_ext.ext_size, len);
		len = m->m_ext.ext_size;
	}
	if (0) { /* XXX seems to have negligible benefits */
		m->m_ext.ext_buf = m->m_data = addr;
	} else {
		bcopy(addr, m->m_data, len);
	}
	m->m_len = m->m_pkthdr.len = len;
	// inc refcount. All ours, we could skip the atomic
	atomic_fetchadd_int(m->m_ext.ext_cnt, 1);
	m->m_flags |= M_FLOWID;
	m->m_pkthdr.flowid = ring_nr;
	m->m_pkthdr.rcvif = ifp; /* used for tx notification */
	ret = NA(ifp)->if_transmit(ifp, m);
	return ret;
}


#if __FreeBSD_version >= 1100005
struct netmap_adapter *
netmap_getna(if_t ifp)
{
	return (NA((struct ifnet *)ifp));
}
#endif /* __FreeBSD_version >= 1100005 */

/*
 * The following two functions are empty until we have a generic
 * way to extract the info from the ifp
 */
int
generic_find_num_desc(struct ifnet *ifp, unsigned int *tx, unsigned int *rx)
{
	D("called, in tx %d rx %d", *tx, *rx);
	return 0;
}


void
generic_find_num_queues(struct ifnet *ifp, u_int *txq, u_int *rxq)
{
	D("called, in txq %d rxq %d", *txq, *rxq);
	*txq = netmap_generic_rings;
	*rxq = netmap_generic_rings;
}


void
netmap_mitigation_init(struct nm_generic_mit *mit, struct netmap_adapter *na)
{
	ND("called");
	mit->mit_pending = 0;
	mit->mit_na = na;
}


void
netmap_mitigation_start(struct nm_generic_mit *mit)
{
	ND("called");
}


void
netmap_mitigation_restart(struct nm_generic_mit *mit)
{
	ND("called");
}


int
netmap_mitigation_active(struct nm_generic_mit *mit)
{
	ND("called");
	return 0;
}


void
netmap_mitigation_cleanup(struct nm_generic_mit *mit)
{
	ND("called");
}


/*
 * In order to track whether pages are still mapped, we hook into
 * the standard cdev_pager and intercept the constructor and
 * destructor.
 */

struct netmap_vm_handle_t {
	struct cdev 		*dev;
	struct netmap_priv_d	*priv;
};


static int
netmap_dev_pager_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct netmap_vm_handle_t *vmh = handle;

	if (netmap_verbose)
		D("handle %p size %jd prot %d foff %jd",
			handle, (intmax_t)size, prot, (intmax_t)foff);
	dev_ref(vmh->dev);
	return 0;
}


static void
netmap_dev_pager_dtor(void *handle)
{
	struct netmap_vm_handle_t *vmh = handle;
	struct cdev *dev = vmh->dev;
	struct netmap_priv_d *priv = vmh->priv;

	if (netmap_verbose)
		D("handle %p", handle);
	netmap_dtor(priv);
	free(vmh, M_DEVBUF);
	dev_rel(dev);
}


static int
netmap_dev_pager_fault(vm_object_t object, vm_ooffset_t offset,
	int prot, vm_page_t *mres)
{
	struct netmap_vm_handle_t *vmh = object->handle;
	struct netmap_priv_d *priv = vmh->priv;
	vm_paddr_t paddr;
	vm_page_t page;
	vm_memattr_t memattr;
	vm_pindex_t pidx;

	ND("object %p offset %jd prot %d mres %p",
			object, (intmax_t)offset, prot, mres);
	memattr = object->memattr;
	pidx = OFF_TO_IDX(offset);
	paddr = netmap_mem_ofstophys(priv->np_mref, offset);
	if (paddr == 0)
		return VM_PAGER_FAIL;

	if (((*mres)->flags & PG_FICTITIOUS) != 0) {
		/*
		 * If the passed in result page is a fake page, update it with
		 * the new physical address.
		 */
		page = *mres;
		vm_page_updatefake(page, paddr, memattr);
	} else {
		/*
		 * Replace the passed in reqpage page with our own fake page and
		 * free up the all of the original pages.
		 */
#ifndef VM_OBJECT_WUNLOCK	/* FreeBSD < 10.x */
#define VM_OBJECT_WUNLOCK VM_OBJECT_UNLOCK
#define VM_OBJECT_WLOCK	VM_OBJECT_LOCK
#endif /* VM_OBJECT_WUNLOCK */

		VM_OBJECT_WUNLOCK(object);
		page = vm_page_getfake(paddr, memattr);
		VM_OBJECT_WLOCK(object);
		vm_page_lock(*mres);
		vm_page_free(*mres);
		vm_page_unlock(*mres);
		*mres = page;
		vm_page_insert(page, object, pidx);
	}
	page->valid = VM_PAGE_BITS_ALL;
	return (VM_PAGER_OK);
}


static struct cdev_pager_ops netmap_cdev_pager_ops = {
	.cdev_pg_ctor = netmap_dev_pager_ctor,
	.cdev_pg_dtor = netmap_dev_pager_dtor,
	.cdev_pg_fault = netmap_dev_pager_fault,
};


static int
netmap_mmap_single(struct cdev *cdev, vm_ooffset_t *foff,
	vm_size_t objsize,  vm_object_t *objp, int prot)
{
	int error;
	struct netmap_vm_handle_t *vmh;
	struct netmap_priv_d *priv;
	vm_object_t obj;

	if (netmap_verbose)
		D("cdev %p foff %jd size %jd objp %p prot %d", cdev,
		    (intmax_t )*foff, (intmax_t )objsize, objp, prot);

	vmh = malloc(sizeof(struct netmap_vm_handle_t), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
	if (vmh == NULL)
		return ENOMEM;
	vmh->dev = cdev;

	NMG_LOCK();
	error = devfs_get_cdevpriv((void**)&priv);
	if (error)
		goto err_unlock;
	vmh->priv = priv;
	priv->np_refcount++;
	NMG_UNLOCK();

	error = netmap_get_memory(priv);
	if (error)
		goto err_deref;

	obj = cdev_pager_allocate(vmh, OBJT_DEVICE,
		&netmap_cdev_pager_ops, objsize, prot,
		*foff, NULL);
	if (obj == NULL) {
		D("cdev_pager_allocate failed");
		error = EINVAL;
		goto err_deref;
	}

	*objp = obj;
	return 0;

err_deref:
	NMG_LOCK();
	priv->np_refcount--;
err_unlock:
	NMG_UNLOCK();
// err:
	free(vmh, M_DEVBUF);
	return error;
}


// XXX can we remove this ?
static int
netmap_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	if (netmap_verbose)
		D("dev %p fflag 0x%x devtype %d td %p",
			dev, fflag, devtype, td);
	return 0;
}


static int
netmap_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct netmap_priv_d *priv;
	int error;

	(void)dev;
	(void)oflags;
	(void)devtype;
	(void)td;

	// XXX wait or nowait ?
	priv = malloc(sizeof(struct netmap_priv_d), M_DEVBUF,
			      M_NOWAIT | M_ZERO);
	if (priv == NULL)
		return ENOMEM;

	error = devfs_set_cdevpriv(priv, netmap_dtor);
	if (error)
	        return error;

	priv->np_refcount = 1;

	return 0;
}

/******************** kqueue support ****************/

/*
 * The OS_selwakeup also needs to issue a KNOTE_UNLOCKED.
 * We use a non-zero argument to distinguish the call from the one
 * in kevent_scan() which instead also needs to run netmap_poll().
 * The knote uses a global mutex for the time being. We might
 * try to reuse the one in the si, but it is not allocated
 * permanently so it might be a bit tricky.
 *
 * The *kqfilter function registers one or another f_event
 * depending on read or write mode.
 * In the call to f_event() td_fpop is NULL so any child function
 * calling devfs_get_cdevpriv() would fail - and we need it in
 * netmap_poll(). As a workaround we store priv into kn->kn_hook
 * and pass it as first argument to netmap_poll(), which then
 * uses the failure to tell that we are called from f_event()
 * and do not need the selrecord().
 */

void freebsd_selwakeup(struct selinfo *si, int pri);

void
freebsd_selwakeup(struct selinfo *si, int pri)
{
	if (netmap_verbose)
		D("on knote %p", &si->si_note);
	selwakeuppri(si, pri);
	/* use a non-zero hint to tell the notification from the
	 * call done in kqueue_scan() which uses 0
	 */
	KNOTE_UNLOCKED(&si->si_note, 0x100 /* notification */);
}

static void
netmap_knrdetach(struct knote *kn)
{
	struct netmap_priv_d *priv = (struct netmap_priv_d *)kn->kn_hook;
	struct selinfo *si = priv->np_rxsi;

	D("remove selinfo %p", si);
	knlist_remove(&si->si_note, kn, 0);
}

static void
netmap_knwdetach(struct knote *kn)
{
	struct netmap_priv_d *priv = (struct netmap_priv_d *)kn->kn_hook;
	struct selinfo *si = priv->np_txsi;

	D("remove selinfo %p", si);
	knlist_remove(&si->si_note, kn, 0);
}

/*
 * callback from notifies (generated externally) and our
 * calls to kevent(). The former we just return 1 (ready)
 * since we do not know better.
 * In the latter we call netmap_poll and return 0/1 accordingly.
 */
static int
netmap_knrw(struct knote *kn, long hint, int events)
{
	struct netmap_priv_d *priv;
	int revents;

	if (hint != 0) {
		ND(5, "call from notify");
		return 1; /* assume we are ready */
	}
	priv = kn->kn_hook;
	/* the notification may come from an external thread,
	 * in which case we do not want to run the netmap_poll
	 * This should be filtered above, but check just in case.
	 */
	if (curthread != priv->np_td) { /* should not happen */
		RD(5, "curthread changed %p %p", curthread, priv->np_td);
		return 1;
	} else {
		revents = netmap_poll((void *)priv, events, curthread);
		return (events & revents) ? 1 : 0;
	}
}

static int
netmap_knread(struct knote *kn, long hint)
{
	return netmap_knrw(kn, hint, POLLIN);
}

static int
netmap_knwrite(struct knote *kn, long hint)
{
	return netmap_knrw(kn, hint, POLLOUT);
}

static struct filterops netmap_rfiltops = {
	.f_isfd = 1,
	.f_detach = netmap_knrdetach,
	.f_event = netmap_knread,
};

static struct filterops netmap_wfiltops = {
	.f_isfd = 1,
	.f_detach = netmap_knwdetach,
	.f_event = netmap_knwrite,
};


/*
 * This is called when a thread invokes kevent() to record
 * a change in the configuration of the kqueue().
 * The 'priv' should be the same as in the netmap device.
 */
static int
netmap_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct netmap_priv_d *priv;
	int error;
	struct netmap_adapter *na;
	struct selinfo *si;
	int ev = kn->kn_filter;

	if (ev != EVFILT_READ && ev != EVFILT_WRITE) {
		D("bad filter request %d", ev);
		return 1;
	}
	error = devfs_get_cdevpriv((void**)&priv);
	if (error) {
		D("device not yet setup");
		return 1;
	}
	na = priv->np_na;
	if (na == NULL) {
		D("no netmap adapter for this file descriptor");
		return 1;
	}
	/* the si is indicated in the priv */
	si = (ev == EVFILT_WRITE) ? priv->np_txsi : priv->np_rxsi;
	// XXX lock(priv) ?
	kn->kn_fop = (ev == EVFILT_WRITE) ?
		&netmap_wfiltops : &netmap_rfiltops;
	kn->kn_hook = priv;
	knlist_add(&si->si_note, kn, 1);
	// XXX unlock(priv)
	ND("register %p %s td %p priv %p kn %p np_nifp %p kn_fp/fpop %s",
		na, na->ifp->if_xname, curthread, priv, kn,
		priv->np_nifp,
		kn->kn_fp == curthread->td_fpop ? "match" : "MISMATCH");
	return 0;
}

struct cdevsw netmap_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "netmap",
	.d_open = netmap_open,
	.d_mmap_single = netmap_mmap_single,
	.d_ioctl = netmap_ioctl,
	.d_poll = netmap_poll,
	.d_kqfilter = netmap_kqfilter,
	.d_close = netmap_close,
};
/*--- end of kqueue support ----*/

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
