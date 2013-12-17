/*
 * Copyright (C) 2013 Universita` di Pisa. All rights reserved.
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
#include <sys/kernel.h> /* types used in module initialization */
#include <sys/conf.h>	/* DEV_MODULE */

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

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>
#include <dev/netmap/netmap_mem2.h>


/* ======================== FREEBSD-SPECIFIC ROUTINES ================== */

/*
 * Intercept the rx routine in the standard device driver.
 * Second argument is non-zero to intercept, 0 to restore
 */
int
netmap_catch_rx(struct netmap_adapter *na, int intercept)
{
	struct netmap_generic_adapter *gna = (struct netmap_generic_adapter *)na;
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
 *
 * XXX see if FreeBSD has such a mechanism
 */
void
netmap_catch_packet_steering(struct netmap_generic_adapter *na, int enable)
{
	if (enable) {
	} else {
	}
}

/* Transmit routine used by generic_netmap_txsync(). Returns 0 on success
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

	m->m_len = m->m_pkthdr.len = 0;

	// copy data to the mbuf
	m_copyback(m, 0, len, addr);

	// inc refcount. We are alone, so we can skip the atomic
	atomic_fetchadd_int(m->m_ext.ref_cnt, 1);
	m->m_flags |= M_FLOWID;
	m->m_pkthdr.flowid = ring_nr;
	m->m_pkthdr.rcvif = ifp; /* used for tx notification */
	ret = ifp->if_transmit(ifp, m);
	return ret;
}

/*
 * The following two functions are empty until we have a generic
 * way to extract the info from the ifp
 */
int
generic_find_num_desc(struct ifnet *ifp, unsigned int *tx, unsigned int *rx)
{
	D("called");
	return 0;
}

void
generic_find_num_queues(struct ifnet *ifp, u_int *txq, u_int *rxq)
{
	D("called");
	*txq = 1;
	*rxq = 1;
}

void netmap_mitigation_init(struct netmap_generic_adapter *na)
{
	ND("called");
	na->mit_pending = 0;
}


void netmap_mitigation_start(struct netmap_generic_adapter *na)
{
	ND("called");
}

void netmap_mitigation_restart(struct netmap_generic_adapter *na)
{
	ND("called");
}

int netmap_mitigation_active(struct netmap_generic_adapter *na)
{
	ND("called");
	return 0;
}

void netmap_mitigation_cleanup(struct netmap_generic_adapter *na)
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


struct cdevsw netmap_cdevsw = {
	.d_version = D_VERSION,
	.d_name = "netmap",
	.d_open = netmap_open,
	.d_mmap_single = netmap_mmap_single,
	.d_ioctl = netmap_ioctl,
	.d_poll = netmap_poll,
	.d_close = netmap_close,
};


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
