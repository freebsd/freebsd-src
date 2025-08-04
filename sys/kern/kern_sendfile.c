/*-
 * Copyright (c) 2013-2015 Gleb Smirnoff <glebius@FreeBSD.org>
 * Copyright (c) 1998, David Greenman. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_kern_tls.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/inotify.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ktls.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/sf_buf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>

#include <net/vnet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_log_buf.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

static MALLOC_DEFINE(M_SENDFILE, "sendfile", "sendfile dynamic memory");

#define	EXT_FLAG_SYNC		EXT_FLAG_VENDOR1
#define	EXT_FLAG_NOCACHE	EXT_FLAG_VENDOR2
#define	EXT_FLAG_CACHE_LAST	EXT_FLAG_VENDOR3

/*
 * Structure describing a single sendfile(2) I/O, which may consist of
 * several underlying pager I/Os.
 *
 * The syscall context allocates the structure and initializes 'nios'
 * to 1.  As sendfile_swapin() runs through pages and starts asynchronous
 * paging operations, it increments 'nios'.
 *
 * Every I/O completion calls sendfile_iodone(), which decrements the 'nios',
 * and the syscall also calls sendfile_iodone() after allocating all mbufs,
 * linking them and sending to socket.  Whoever reaches zero 'nios' is
 * responsible to call pr_ready() on the socket, to notify it of readyness
 * of the data.
 */
struct sf_io {
	volatile u_int	nios;
	u_int		error;
	int		npages;
	struct socket	*so;
	struct mbuf	*m;
	vm_object_t	obj;
	vm_pindex_t	pindex0;
#ifdef KERN_TLS
	struct ktls_session *tls;
#endif
	vm_page_t	pa[];
};

/*
 * Structure used to track requests with SF_SYNC flag.
 */
struct sendfile_sync {
	struct mtx	mtx;
	struct cv	cv;
	unsigned	count;
	bool		waiting;
};

static void
sendfile_sync_destroy(struct sendfile_sync *sfs)
{
	KASSERT(sfs->count == 0, ("sendfile sync %p still busy", sfs));

	cv_destroy(&sfs->cv);
	mtx_destroy(&sfs->mtx);
	free(sfs, M_SENDFILE);
}

static void
sendfile_sync_signal(struct sendfile_sync *sfs)
{
	mtx_lock(&sfs->mtx);
	KASSERT(sfs->count > 0, ("sendfile sync %p not busy", sfs));
	if (--sfs->count == 0) {
		if (!sfs->waiting) {
			/* The sendfile() waiter was interrupted by a signal. */
			sendfile_sync_destroy(sfs);
			return;
		} else {
			cv_signal(&sfs->cv);
		}
	}
	mtx_unlock(&sfs->mtx);
}

counter_u64_t sfstat[sizeof(struct sfstat) / sizeof(uint64_t)];

static void
sfstat_init(const void *unused)
{

	COUNTER_ARRAY_ALLOC(sfstat, sizeof(struct sfstat) / sizeof(uint64_t),
	    M_WAITOK);
}
SYSINIT(sfstat, SI_SUB_MBUF, SI_ORDER_FIRST, sfstat_init, NULL);

static int
sfstat_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct sfstat s;

	COUNTER_ARRAY_COPY(sfstat, &s, sizeof(s) / sizeof(uint64_t));
	if (req->newptr)
		COUNTER_ARRAY_ZERO(sfstat, sizeof(s) / sizeof(uint64_t));
	return (SYSCTL_OUT(req, &s, sizeof(s)));
}
SYSCTL_PROC(_kern_ipc, OID_AUTO, sfstat,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
    sfstat_sysctl, "I",
    "sendfile statistics");

static void
sendfile_free_mext(struct mbuf *m)
{
	struct sf_buf *sf;
	vm_page_t pg;
	int flags;

	KASSERT(m->m_flags & M_EXT && m->m_ext.ext_type == EXT_SFBUF,
	    ("%s: m %p !M_EXT or !EXT_SFBUF", __func__, m));

	sf = m->m_ext.ext_arg1;
	pg = sf_buf_page(sf);
	flags = (m->m_ext.ext_flags & EXT_FLAG_NOCACHE) != 0 ? VPR_TRYFREE : 0;

	sf_buf_free(sf);
	vm_page_release(pg, flags);

	if (m->m_ext.ext_flags & EXT_FLAG_SYNC) {
		struct sendfile_sync *sfs = m->m_ext.ext_arg2;
		sendfile_sync_signal(sfs);
	}
}

static void
sendfile_free_mext_pg(struct mbuf *m)
{
	vm_page_t pg;
	int flags, i;
	bool cache_last;

	M_ASSERTEXTPG(m);

	cache_last = m->m_ext.ext_flags & EXT_FLAG_CACHE_LAST;
	flags = (m->m_ext.ext_flags & EXT_FLAG_NOCACHE) != 0 ? VPR_TRYFREE : 0;

	for (i = 0; i < m->m_epg_npgs; i++) {
		if (cache_last && i == m->m_epg_npgs - 1)
			flags = 0;
		pg = PHYS_TO_VM_PAGE(m->m_epg_pa[i]);
		vm_page_release(pg, flags);
	}

	if (m->m_ext.ext_flags & EXT_FLAG_SYNC) {
		struct sendfile_sync *sfs = m->m_ext.ext_arg1;
		sendfile_sync_signal(sfs);
	}
}

/*
 * Helper function to calculate how much data to put into page i of n.
 * Only first and last pages are special.
 */
static inline off_t
xfsize(int i, int n, off_t off, off_t len)
{

	if (i == 0)
		return (omin(PAGE_SIZE - (off & PAGE_MASK), len));

	if (i == n - 1 && ((off + len) & PAGE_MASK) > 0)
		return ((off + len) & PAGE_MASK);

	return (PAGE_SIZE);
}

/*
 * Helper function to get offset within object for i page.
 */
static inline vm_ooffset_t
vmoff(int i, off_t off)
{

	if (i == 0)
		return ((vm_ooffset_t)off);

	return (trunc_page(off + i * PAGE_SIZE));
}

/*
 * Helper function used when allocation of a page or sf_buf failed.
 * Pretend as if we don't have enough space, subtract xfsize() of
 * all pages that failed.
 */
static inline void
fixspace(int old, int new, off_t off, int *space)
{

	KASSERT(old > new, ("%s: old %d new %d", __func__, old, new));

	/* Subtract last one. */
	*space -= xfsize(old - 1, old, off, *space);
	old--;

	if (new == old)
		/* There was only one page. */
		return;

	/* Subtract first one. */
	if (new == 0) {
		*space -= xfsize(0, old, off, *space);
		new++;
	}

	/* Rest of pages are full sized. */
	*space -= (old - new) * PAGE_SIZE;

	KASSERT(*space >= 0, ("%s: space went backwards", __func__));
}

/*
 * Wait for all in-flight ios to complete, we must not unwire pages
 * under them.
 */
static void
sendfile_iowait(struct sf_io *sfio, const char *wmesg)
{
	while (atomic_load_int(&sfio->nios) != 1)
		pause(wmesg, 1);
}

/*
 * I/O completion callback.
 *
 * When called via I/O path, the curvnet is not set and should be obtained
 * from the socket.  When called synchronously from vn_sendfile(), usually
 * to report error or just release the reference (all pages are valid), then
 * curvnet shall be already set.
 */
static void
sendfile_iodone(void *arg, vm_page_t *pa, int count, int error)
{
	struct sf_io *sfio = arg;
	struct socket *so;
	int i;

	if (error != 0)
		sfio->error = error;

	/*
	 * Restore the valid page pointers.  They are already
	 * unbusied, but still wired.
	 *
	 * XXXKIB since pages are only wired, and we do not
	 * own the object lock, other users might have
	 * invalidated them in meantime.  Similarly, after we
	 * unbusied the swapped-in pages, they can become
	 * invalid under us.
	 */
	MPASS(count == 0 || pa[0] != bogus_page);
	for (i = 0; i < count; i++) {
		if (pa[i] == bogus_page) {
			sfio->pa[(pa[0]->pindex - sfio->pindex0) + i] =
			    pa[i] = vm_page_relookup(sfio->obj,
			    pa[0]->pindex + i);
			KASSERT(pa[i] != NULL,
			    ("%s: page %p[%d] disappeared",
			    __func__, pa, i));
		} else {
			vm_page_xunbusy_unchecked(pa[i]);
		}
	}

	if (!refcount_release(&sfio->nios))
		return;

#ifdef INVARIANTS
	for (i = 1; i < sfio->npages; i++) {
		if (sfio->pa[i] == NULL)
			break;
		KASSERT(vm_page_wired(sfio->pa[i]),
		    ("sfio %p page %d %p not wired", sfio, i, sfio->pa[i]));
		if (i == 0)
			continue;
		KASSERT(sfio->pa[0]->object == sfio->pa[i]->object,
		    ("sfio %p page %d %p wrong owner %p %p", sfio, i,
		    sfio->pa[i], sfio->pa[0]->object, sfio->pa[i]->object));
		KASSERT(sfio->pa[0]->pindex + i == sfio->pa[i]->pindex,
		    ("sfio %p page %d %p wrong index %jx %jx", sfio, i,
		    sfio->pa[i], (uintmax_t)sfio->pa[0]->pindex,
		    (uintmax_t)sfio->pa[i]->pindex));
	}
#endif

	vm_object_pip_wakeup(sfio->obj);

	if (sfio->m == NULL) {
		/*
		 * Either I/O operation failed, or we failed to allocate
		 * buffers, or we bailed out on first busy page, or we
		 * succeeded filling the request without any I/Os. Anyway,
		 * pr_send() hadn't been executed - nothing had been sent
		 * to the socket yet.
		 */
		MPASS((curthread->td_pflags & TDP_KTHREAD) == 0);
		free(sfio, M_SENDFILE);
		return;
	}

#if defined(KERN_TLS) && defined(INVARIANTS)
	if ((sfio->m->m_flags & M_EXTPG) != 0)
		KASSERT(sfio->tls == sfio->m->m_epg_tls,
		    ("TLS session mismatch"));
	else
		KASSERT(sfio->tls == NULL,
		    ("non-ext_pgs mbuf with TLS session"));
#endif
	so = sfio->so;
	CURVNET_SET_QUIET(so->so_vnet);
	if (__predict_false(sfio->error)) {
		/*
		 * I/O operation failed.  The state of data in the socket
		 * is now inconsistent, and all what we can do is to tear
		 * it down. Protocol abort method would tear down protocol
		 * state, free all ready mbufs and detach not ready ones.
		 * We will free the mbufs corresponding to this I/O manually.
		 *
		 * The socket would be marked with EIO and made available
		 * for read, so that application receives EIO on next
		 * syscall and eventually closes the socket.
		 */
		so->so_proto->pr_abort(so);
		so->so_error = EIO;

		mb_free_notready(sfio->m, sfio->npages);
#ifdef KERN_TLS
	} else if (sfio->tls != NULL && sfio->tls->mode == TCP_TLS_MODE_SW) {
		/*
		 * I/O operation is complete, but we still need to
		 * encrypt.  We cannot do this in the interrupt thread
		 * of the disk controller, so forward the mbufs to a
		 * different thread.
		 *
		 * Donate the socket reference from sfio to rather
		 * than explicitly invoking soref().
		 */
		ktls_enqueue(sfio->m, so, sfio->npages);
		goto out_with_ref;
#endif
	} else
		(void)so->so_proto->pr_ready(so, sfio->m, sfio->npages);

	sorele(so);
#ifdef KERN_TLS
out_with_ref:
#endif
	CURVNET_RESTORE();
	free(sfio, M_SENDFILE);
}

/*
 * Iterate through pages vector and request paging for non-valid pages.
 */
static int
sendfile_swapin(vm_object_t obj, struct sf_io *sfio, int *nios, off_t off,
    off_t len, int rhpages, int flags)
{
	vm_page_t *pa;
	int a, count, count1, grabbed, i, j, npages, rv;

	pa = sfio->pa;
	npages = sfio->npages;
	*nios = 0;
	flags = (flags & SF_NODISKIO) ? VM_ALLOC_NOWAIT : 0;
	sfio->pindex0 = OFF_TO_IDX(off);

	/*
	 * First grab all the pages and wire them.  Note that we grab
	 * only required pages.  Readahead pages are dealt with later.
	 */
	grabbed = vm_page_grab_pages_unlocked(obj, OFF_TO_IDX(off),
	    VM_ALLOC_NORMAL | VM_ALLOC_WIRED | flags, pa, npages);
	if (grabbed < npages) {
		for (int i = grabbed; i < npages; i++)
			pa[i] = NULL;
		npages = grabbed;
		rhpages = 0;
	}

	for (i = 0; i < npages;) {
		/* Skip valid pages. */
		if (vm_page_is_valid(pa[i], vmoff(i, off) & PAGE_MASK,
		    xfsize(i, npages, off, len))) {
			vm_page_xunbusy(pa[i]);
			SFSTAT_INC(sf_pages_valid);
			i++;
			continue;
		}

		/*
		 * Next page is invalid.  Check if it belongs to pager.  It
		 * may not be there, which is a regular situation for shmem
		 * pager.  For vnode pager this happens only in case of
		 * a sparse file.
		 *
		 * Important feature of vm_pager_has_page() is the hint
		 * stored in 'a', about how many pages we can pagein after
		 * this page in a single I/O.
		 */
		VM_OBJECT_RLOCK(obj);
		if (!vm_pager_has_page(obj, OFF_TO_IDX(vmoff(i, off)), NULL,
		    &a)) {
			VM_OBJECT_RUNLOCK(obj);
			pmap_zero_page(pa[i]);
			vm_page_valid(pa[i]);
			MPASS(pa[i]->dirty == 0);
			vm_page_xunbusy(pa[i]);
			i++;
			continue;
		}
		VM_OBJECT_RUNLOCK(obj);

		/*
		 * We want to pagein as many pages as possible, limited only
		 * by the 'a' hint and actual request.
		 */
		count = min(a + 1, npages - i);

		/*
		 * We should not pagein into a valid page because
		 * there might be still unfinished write tracked by
		 * e.g. a buffer, thus we substitute any valid pages
		 * with the bogus one.
		 *
		 * We must not leave around xbusy pages which are not
		 * part of the run passed to vm_pager_getpages(),
		 * otherwise pager might deadlock waiting for the busy
		 * status of the page, e.g. if it constitues the
		 * buffer needed to validate other page.
		 *
		 * First trim the end of the run consisting of the
		 * valid pages, then replace the rest of the valid
		 * with bogus.
		 */
		count1 = count;
		for (j = i + count - 1; j > i; j--) {
			if (vm_page_is_valid(pa[j], vmoff(j, off) & PAGE_MASK,
			    xfsize(j, npages, off, len))) {
				vm_page_xunbusy(pa[j]);
				SFSTAT_INC(sf_pages_valid);
				count--;
			} else {
				break;
			}
		}

		/*
		 * The last page in the run pa[i + count - 1] is
		 * guaranteed to be invalid by the trim above, so it
		 * is not replaced with bogus, thus -1 in the loop end
		 * condition.
		 */
		MPASS(pa[i + count - 1]->valid != VM_PAGE_BITS_ALL);
		for (j = i + 1; j < i + count - 1; j++) {
			if (vm_page_is_valid(pa[j], vmoff(j, off) & PAGE_MASK,
			    xfsize(j, npages, off, len))) {
				vm_page_xunbusy(pa[j]);
				SFSTAT_INC(sf_pages_valid);
				SFSTAT_INC(sf_pages_bogus);
				pa[j] = bogus_page;
			}
		}

		refcount_acquire(&sfio->nios);
		rv = vm_pager_get_pages_async(obj, pa + i, count, NULL,
		    i + count == npages ? &rhpages : NULL,
		    &sendfile_iodone, sfio);
		if (__predict_false(rv != VM_PAGER_OK)) {
			sendfile_iowait(sfio, "sferrio");

			/*
			 * Do remaining pages recovery before returning EIO.
			 * Pages from 0 to npages are wired.
			 * Pages from (i + count1) to npages are busied.
			 */
			for (j = 0; j < npages; j++) {
				if (j >= i + count1)
					vm_page_xunbusy(pa[j]);
				KASSERT(pa[j] != NULL && pa[j] != bogus_page,
				    ("%s: page %p[%d] I/O recovery failure",
				    __func__, pa, j));
				vm_page_unwire(pa[j], PQ_INACTIVE);
				pa[j] = NULL;
			}
			return (EIO);
		}

		SFSTAT_INC(sf_iocnt);
		SFSTAT_ADD(sf_pages_read, count);
		if (i + count == npages)
			SFSTAT_ADD(sf_rhpages_read, rhpages);

		i += count1;
		(*nios)++;
	}

	if (*nios == 0 && npages != 0)
		SFSTAT_INC(sf_noiocnt);

	return (0);
}

static int
sendfile_getobj(struct thread *td, struct file *fp, vm_object_t *obj_res,
    struct vnode **vp_res, struct shmfd **shmfd_res, off_t *obj_size,
    int *bsize)
{
	vm_object_t obj;
	struct vnode *vp;
	struct shmfd *shmfd;
	int error;

	error = 0;
	vp = *vp_res = NULL;
	obj = NULL;
	shmfd = *shmfd_res = NULL;
	*bsize = 0;

	/*
	 * The file descriptor must be a regular file and have a
	 * backing VM object.
	 */
	if (fp->f_type == DTYPE_VNODE) {
		vp = fp->f_vnode;
		vn_lock(vp, LK_SHARED | LK_RETRY);
		if (vp->v_type != VREG) {
			error = EINVAL;
			goto out;
		}
		*bsize = vp->v_mount->mnt_stat.f_iosize;
		obj = vp->v_object;
		if (obj == NULL) {
			error = EINVAL;
			goto out;
		}

		/*
		 * Use the pager size when available to simplify synchronization
		 * with filesystems, which otherwise must atomically update both
		 * the vnode pager size and file size.
		 */
		if (obj->type == OBJT_VNODE) {
			VM_OBJECT_RLOCK(obj);
			*obj_size = obj->un_pager.vnp.vnp_size;
		} else {
			error = vn_getsize_locked(vp, obj_size, td->td_ucred);
			if (error != 0)
				goto out;
			VM_OBJECT_RLOCK(obj);
		}
	} else if (fp->f_type == DTYPE_SHM) {
		shmfd = fp->f_data;
		obj = shmfd->shm_object;
		VM_OBJECT_RLOCK(obj);
		*obj_size = shmfd->shm_size;
	} else {
		error = EINVAL;
		goto out;
	}

	if ((obj->flags & OBJ_DEAD) != 0) {
		VM_OBJECT_RUNLOCK(obj);
		error = EBADF;
		goto out;
	}

	/*
	 * Temporarily increase the backing VM object's reference
	 * count so that a forced reclamation of its vnode does not
	 * immediately destroy it.
	 */
	vm_object_reference_locked(obj);
	VM_OBJECT_RUNLOCK(obj);
	*obj_res = obj;
	*vp_res = vp;
	*shmfd_res = shmfd;

out:
	if (vp != NULL)
		VOP_UNLOCK(vp);
	return (error);
}

static int
sendfile_getsock(struct thread *td, int s, struct file **sock_fp,
    struct socket **so)
{
	int error;

	*sock_fp = NULL;
	*so = NULL;

	/*
	 * The socket must be a stream socket and connected.
	 */
	error = getsock(td, s, &cap_send_rights, sock_fp);
	if (error != 0)
		return (error);
	*so = (*sock_fp)->f_data;
	if ((*so)->so_type != SOCK_STREAM)
		return (EINVAL);
	/*
	 * SCTP one-to-one style sockets currently don't work with
	 * sendfile(). So indicate EINVAL for now.
	 */
	if ((*so)->so_proto->pr_protocol == IPPROTO_SCTP)
		return (EINVAL);
	return (0);
}

/*
 * Check socket state and wait (or EAGAIN) for needed amount of space.
 */
int
sendfile_wait_generic(struct socket *so, off_t need, int *space)
{
	int error;

	MPASS(need > 0);
	MPASS(space != NULL);

	/*
	 * XXXGL: the hack with sb_lowat originates from d99b0dd2c5297.  To
	 * achieve high performance sending with sendfile(2) a non-blocking
	 * socket needs a fairly high low watermark.  Otherwise, the socket
	 * will be reported as writable too early, and sendfile(2) will send
	 * just a few bytes each time.  It is important to understand that
	 * we are changing sb_lowat not for the current invocation of the
	 * syscall, but for the *next* syscall. So there is no way to
	 * workaround the problem, e.g. provide a special version of sbspace().
	 * Since this hack has been in the kernel for a long time, we
	 * anticipate that there is a lot of software that will suffer if we
	 * remove it.  See also b21104487324.
	 */
	error = 0;
	SOCK_SENDBUF_LOCK(so);
	if (so->so_snd.sb_flags & SB_AUTOLOWAT) {
		if (so->so_snd.sb_lowat < so->so_snd.sb_hiwat / 2)
			so->so_snd.sb_lowat = so->so_snd.sb_hiwat / 2;
		if (so->so_snd.sb_lowat < PAGE_SIZE &&
		    so->so_snd.sb_hiwat >= PAGE_SIZE)
			so->so_snd.sb_lowat = PAGE_SIZE;
	}
retry_space:
	if (so->so_snd.sb_state & SBS_CANTSENDMORE) {
		error = EPIPE;
		goto done;
	} else if (so->so_error) {
		error = so->so_error;
		so->so_error = 0;
		goto done;
	}
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto done;
	}

	*space = sbspace(&so->so_snd);
	if (*space < need && (*space <= 0 || *space < so->so_snd.sb_lowat)) {
		if (so->so_state & SS_NBIO) {
			error = EAGAIN;
			goto done;
		}
		/*
		 * sbwait() drops the lock while sleeping.  When we loop back
		 * to retry_space the state may have changed and we retest
		 * for it.
		 */
		error = sbwait(so, SO_SND);
		/*
		 * An error from sbwait() usually indicates that we've been
		 * interrupted by a signal.  If we've sent anything then return
		 * bytes sent, otherwise return the error.
		 */
		if (error != 0)
			goto done;
		goto retry_space;
	}
done:
	SOCK_SENDBUF_UNLOCK(so);

	return (error);
}

int
vn_sendfile(struct file *fp, int sockfd, struct uio *hdr_uio,
    struct uio *trl_uio, off_t offset, size_t nbytes, off_t *sent, int flags,
    struct thread *td)
{
	struct file *sock_fp;
	struct vnode *vp;
	struct vm_object *obj;
	vm_page_t pga;
	struct socket *so;
	const struct protosw *pr;
#ifdef KERN_TLS
	struct ktls_session *tls;
#endif
	struct mbuf *m, *mh, *mhtail;
	struct sf_buf *sf;
	struct shmfd *shmfd;
	struct sendfile_sync *sfs;
	struct vattr va;
	off_t off, sbytes, rem, obj_size, nobj_size;
	int bsize, error, ext_pgs_idx, hdrlen, max_pgs, softerr;
#ifdef KERN_TLS
	int tls_enq_cnt;
#endif
	bool use_ext_pgs;

	obj = NULL;
	so = NULL;
	m = mh = NULL;
	sfs = NULL;
#ifdef KERN_TLS
	tls = NULL;
#endif
	hdrlen = sbytes = 0;
	softerr = 0;
	use_ext_pgs = false;

	error = sendfile_getobj(td, fp, &obj, &vp, &shmfd, &obj_size, &bsize);
	if (error != 0)
		return (error);

	error = sendfile_getsock(td, sockfd, &sock_fp, &so);
	if (error != 0)
		goto out;
	pr = so->so_proto;

#ifdef MAC
	error = mac_socket_check_send(td->td_ucred, so);
	if (error != 0)
		goto out;
#endif

	SFSTAT_INC(sf_syscalls);
	SFSTAT_ADD(sf_rhpages_requested, SF_READAHEAD(flags));

	if (__predict_false(flags & SF_SYNC)) {
		gone_in(16, "Warning! %s[%u] uses SF_SYNC sendfile(2) flag. "
		    "Please follow up to https://bugs.freebsd.org/"
		    "bugzilla/show_bug.cgi?id=287348. ",
		    td->td_proc->p_comm, td->td_proc->p_pid);
		sfs = malloc(sizeof(*sfs), M_SENDFILE, M_WAITOK | M_ZERO);
		mtx_init(&sfs->mtx, "sendfile", NULL, MTX_DEF);
		cv_init(&sfs->cv, "sendfile");
		sfs->waiting = true;
	}

	rem = nbytes ? omin(nbytes, obj_size - offset) : obj_size - offset;

	/*
	 * Protect against multiple writers to the socket.
	 *
	 * XXXRW: Historically this has assumed non-interruptibility, so now
	 * we implement that, but possibly shouldn't.
	 */
	error = SOCK_IO_SEND_LOCK(so, SBL_WAIT | SBL_NOINTR);
	if (error != 0)
		goto out;
	CURVNET_SET(so->so_vnet);
#ifdef KERN_TLS
	tls = ktls_hold(so->so_snd.sb_tls_info);
#endif

	/*
	 * Loop through the pages of the file, starting with the requested
	 * offset. Get a file page (do I/O if necessary), map the file page
	 * into an sf_buf, attach an mbuf header to the sf_buf, and queue
	 * it on the socket.
	 * This is done in two loops.  The inner loop turns as many pages
	 * as it can, up to available socket buffer space, without blocking
	 * into mbufs to have it bulk delivered into the socket send buffer.
	 * The outer loop checks the state and available space of the socket
	 * and takes care of the overall progress.
	 */
	for (off = offset; rem > 0; ) {
		struct sf_io *sfio;
		vm_page_t *pa;
		struct mbuf *m0, *mtail;
		int nios, space, npages, rhpages;

		mtail = NULL;
		if ((error = pr->pr_sendfile_wait(so, rem, &space)) != 0)
			goto done;
		/*
		 * At the beginning of the first loop check if any headers
		 * are specified and copy them into mbufs.  Reduce space in
		 * the socket buffer by the size of the header mbuf chain.
		 * Clear hdr_uio here and hdrlen at the end of the first loop.
		 */
		if (hdr_uio != NULL && hdr_uio->uio_resid > 0) {
			hdr_uio->uio_td = td;
			hdr_uio->uio_rw = UIO_WRITE;
#ifdef KERN_TLS
			if (tls != NULL)
				mh = m_uiotombuf(hdr_uio, M_WAITOK, space,
				    tls->params.max_frame_len, M_EXTPG);
			else
#endif
				mh = m_uiotombuf(hdr_uio, M_WAITOK,
				    space, 0, 0);
			hdrlen = m_length(mh, &mhtail);
			space -= hdrlen;
			/*
			 * If header consumed all the socket buffer space,
			 * don't waste CPU cycles and jump to the end.
			 */
			if (space == 0) {
				sfio = NULL;
				nios = 0;
				goto prepend_header;
			}
			hdr_uio = NULL;
		}

		if (vp != NULL) {
			error = vn_lock(vp, LK_SHARED);
			if (error != 0)
				goto done;

			/*
			 * Check to see if the file size has changed.
			 */
			if (obj->type == OBJT_VNODE) {
				VM_OBJECT_RLOCK(obj);
				nobj_size = obj->un_pager.vnp.vnp_size;
				VM_OBJECT_RUNLOCK(obj);
			} else {
				error = VOP_GETATTR(vp, &va, td->td_ucred);
				if (error != 0) {
					VOP_UNLOCK(vp);
					goto done;
				}
				nobj_size = va.va_size;
			}
			if (off >= nobj_size) {
				VOP_UNLOCK(vp);
				goto done;
			}
			if (nobj_size != obj_size) {
				obj_size = nobj_size;
				rem = nbytes ? omin(nbytes + offset, obj_size) :
				    obj_size;
				rem -= off;
			}
		}

		if (space > rem)
			space = rem;
		else if (space > PAGE_SIZE) {
			/*
			 * Use page boundaries when possible for large
			 * requests.
			 */
			if (off & PAGE_MASK)
				space -= (PAGE_SIZE - (off & PAGE_MASK));
			space = trunc_page(space);
			if (off & PAGE_MASK)
				space += (PAGE_SIZE - (off & PAGE_MASK));
		}

		npages = howmany(space + (off & PAGE_MASK), PAGE_SIZE);

		/*
		 * Calculate maximum allowed number of pages for readahead
		 * at this iteration.  If SF_USER_READAHEAD was set, we don't
		 * do any heuristics and use exactly the value supplied by
		 * application.  Otherwise, we allow readahead up to "rem".
		 * If application wants more, let it be, but there is no
		 * reason to go above maxphys.  Also check against "obj_size",
		 * since vm_pager_has_page() can hint beyond EOF.
		 */
		if (flags & SF_USER_READAHEAD) {
			rhpages = SF_READAHEAD(flags);
		} else {
			rhpages = howmany(rem + (off & PAGE_MASK), PAGE_SIZE) -
			    npages;
			rhpages += SF_READAHEAD(flags);
		}
		rhpages = min(howmany(maxphys, PAGE_SIZE), rhpages);
		rhpages = min(howmany(obj_size - trunc_page(off), PAGE_SIZE) -
		    npages, rhpages);

		sfio = malloc(sizeof(struct sf_io) +
		    npages * sizeof(vm_page_t), M_SENDFILE, M_WAITOK);
		refcount_init(&sfio->nios, 1);
		sfio->obj = obj;
		sfio->error = 0;
		sfio->m = NULL;
		sfio->npages = npages;
#ifdef KERN_TLS
		/*
		 * This doesn't use ktls_hold() because sfio->m will
		 * also have a reference on 'tls' that will be valid
		 * for all of sfio's lifetime.
		 */
		sfio->tls = tls;
#endif
		vm_object_pip_add(obj, 1);
		error = sendfile_swapin(obj, sfio, &nios, off, space, rhpages,
		    flags);
		if (error != 0) {
			if (vp != NULL)
				VOP_UNLOCK(vp);
			sendfile_iodone(sfio, NULL, 0, error);
			goto done;
		}

		/*
		 * Loop and construct maximum sized mbuf chain to be bulk
		 * dumped into socket buffer.
		 */
		pa = sfio->pa;

		/*
		 * Use unmapped mbufs if enabled for TCP.  Unmapped
		 * bufs are restricted to TCP as that is what has been
		 * tested.  In particular, unmapped mbufs have not
		 * been tested with UNIX-domain sockets.
		 *
		 * TLS frames always require unmapped mbufs.
		 */
		if ((mb_use_ext_pgs && pr->pr_protocol == IPPROTO_TCP)
#ifdef KERN_TLS
		    || tls != NULL
#endif
		    ) {
			use_ext_pgs = true;
#ifdef KERN_TLS
			if (tls != NULL)
				max_pgs = num_pages(tls->params.max_frame_len);
			else
#endif
				max_pgs = MBUF_PEXT_MAX_PGS;

			/* Start at last index, to wrap on first use. */
			ext_pgs_idx = max_pgs - 1;
		}

		for (int i = 0; i < npages; i++) {
			/*
			 * If a page wasn't grabbed successfully, then
			 * trim the array. Can happen only with SF_NODISKIO.
			 */
			if (pa[i] == NULL) {
				SFSTAT_INC(sf_busy);
				fixspace(npages, i, off, &space);
				sfio->npages = i;
				softerr = EBUSY;
				break;
			}
			pga = pa[i];
			if (pga == bogus_page)
				pga = vm_page_relookup(obj, sfio->pindex0 + i);

			if (use_ext_pgs) {
				off_t xfs;

				ext_pgs_idx++;
				if (ext_pgs_idx == max_pgs) {
					m0 = mb_alloc_ext_pgs(M_WAITOK,
					    sendfile_free_mext_pg, M_RDONLY);

					if (flags & SF_NOCACHE) {
						m0->m_ext.ext_flags |=
						    EXT_FLAG_NOCACHE;

						/*
						 * See comment below regarding
						 * ignoring SF_NOCACHE for the
						 * last page.
						 */
						if ((npages - i <= max_pgs) &&
						    ((off + space) & PAGE_MASK) &&
						    (rem > space || rhpages > 0))
							m0->m_ext.ext_flags |=
							    EXT_FLAG_CACHE_LAST;
					}
					if (sfs != NULL) {
						m0->m_ext.ext_flags |=
						    EXT_FLAG_SYNC;
						m0->m_ext.ext_arg1 = sfs;
						mtx_lock(&sfs->mtx);
						sfs->count++;
						mtx_unlock(&sfs->mtx);
					}
					ext_pgs_idx = 0;

					/* Append to mbuf chain. */
					if (mtail != NULL)
						mtail->m_next = m0;
					else
						m = m0;
					mtail = m0;
					m0->m_epg_1st_off =
					    vmoff(i, off) & PAGE_MASK;
				}
				if (nios) {
					mtail->m_flags |= M_NOTREADY;
					m0->m_epg_nrdy++;
				}

				m0->m_epg_pa[ext_pgs_idx] = VM_PAGE_TO_PHYS(pga);
				m0->m_epg_npgs++;
				xfs = xfsize(i, npages, off, space);
				m0->m_epg_last_len = xfs;
				MBUF_EXT_PGS_ASSERT_SANITY(m0);
				mtail->m_len += xfs;
				mtail->m_ext.ext_size += PAGE_SIZE;
				continue;
			}

			/*
			 * Get a sendfile buf.  When allocating the
			 * first buffer for mbuf chain, we usually
			 * wait as long as necessary, but this wait
			 * can be interrupted.  For consequent
			 * buffers, do not sleep, since several
			 * threads might exhaust the buffers and then
			 * deadlock.
			 */
			sf = sf_buf_alloc(pga,
			    m != NULL ? SFB_NOWAIT : SFB_CATCH);
			if (sf == NULL) {
				SFSTAT_INC(sf_allocfail);
				sendfile_iowait(sfio, "sfnosf");
				for (int j = i; j < npages; j++) {
					vm_page_unwire(pa[j], PQ_INACTIVE);
					pa[j] = NULL;
				}
				if (m == NULL)
					softerr = ENOBUFS;
				fixspace(npages, i, off, &space);
				sfio->npages = i;
				break;
			}

			m0 = m_get(M_WAITOK, MT_DATA);
			m0->m_ext.ext_buf = (char *)sf_buf_kva(sf);
			m0->m_ext.ext_size = PAGE_SIZE;
			m0->m_ext.ext_arg1 = sf;
			m0->m_ext.ext_type = EXT_SFBUF;
			m0->m_ext.ext_flags = EXT_FLAG_EMBREF;
			m0->m_ext.ext_free = sendfile_free_mext;
			/*
			 * SF_NOCACHE sets the page as being freed upon send.
			 * However, we ignore it for the last page in 'space',
			 * if the page is truncated, and we got more data to
			 * send (rem > space), or if we have readahead
			 * configured (rhpages > 0).
			 */
			if ((flags & SF_NOCACHE) &&
			    (i != npages - 1 ||
			    !((off + space) & PAGE_MASK) ||
			    !(rem > space || rhpages > 0)))
				m0->m_ext.ext_flags |= EXT_FLAG_NOCACHE;
			if (sfs != NULL) {
				m0->m_ext.ext_flags |= EXT_FLAG_SYNC;
				m0->m_ext.ext_arg2 = sfs;
				mtx_lock(&sfs->mtx);
				sfs->count++;
				mtx_unlock(&sfs->mtx);
			}
			m0->m_ext.ext_count = 1;
			m0->m_flags |= (M_EXT | M_RDONLY);
			if (nios)
				m0->m_flags |= M_NOTREADY;
			m0->m_data = (char *)sf_buf_kva(sf) +
			    (vmoff(i, off) & PAGE_MASK);
			m0->m_len = xfsize(i, npages, off, space);

			/* Append to mbuf chain. */
			if (mtail != NULL)
				mtail->m_next = m0;
			else
				m = m0;
			mtail = m0;
		}

		if (vp != NULL)
			VOP_UNLOCK(vp);

		/* Keep track of bytes processed. */
		off += space;
		rem -= space;

		/*
		 * Prepend header, if any.  Save pointer to first mbuf
		 * with a page.
		 */
		if (hdrlen) {
prepend_header:
			m0 = mhtail->m_next = m;
			m = mh;
			mh = NULL;
		} else
			m0 = m;

		if (m == NULL) {
			KASSERT(softerr, ("%s: m NULL, no error", __func__));
			error = softerr;
			sendfile_iodone(sfio, NULL, 0, 0);
			goto done;
		}

		/* Add the buffer chain to the socket buffer. */
		KASSERT(m_length(m, NULL) == space + hdrlen,
		    ("%s: mlen %u space %d hdrlen %d",
		    __func__, m_length(m, NULL), space, hdrlen));

#ifdef KERN_TLS
		if (tls != NULL)
			ktls_frame(m, tls, &tls_enq_cnt, TLS_RLTYPE_APP);
#endif
		if (nios == 0) {
			/*
			 * If sendfile_swapin() didn't initiate any I/Os,
			 * which happens if all data is cached in VM, or if
			 * the header consumed all socket buffer space and
			 * sfio is NULL, then we can send data right now
			 * without the PRUS_NOTREADY flag.
			 */
			if (sfio != NULL)
				sendfile_iodone(sfio, NULL, 0, 0);
#ifdef KERN_TLS
			if (tls != NULL && tls->mode == TCP_TLS_MODE_SW) {
				error = pr->pr_send(so, PRUS_NOTREADY, m, NULL,
				    NULL, td);
				if (error != 0) {
					m_freem(m);
				} else {
					soref(so);
					ktls_enqueue(m, so, tls_enq_cnt);
				}
			} else
#endif
				error = pr->pr_send(so, 0, m, NULL, NULL, td);
		} else {
			sfio->so = so;
			sfio->m = m0;
			soref(so);
			error = pr->pr_send(so, PRUS_NOTREADY, m, NULL, NULL,
			    td);
			sendfile_iodone(sfio, NULL, 0, error);
		}
#ifdef TCP_REQUEST_TRK
		if (so->so_proto->pr_protocol == IPPROTO_TCP) {
			/* log the sendfile call to the TCP log, if enabled */
			tcp_log_sendfile(so, offset, nbytes, flags);
		}
#endif
		m = NULL;
		if (error)
			goto done;
		sbytes += space + hdrlen;
		if (hdrlen)
			hdrlen = 0;
		if (softerr) {
			error = softerr;
			goto done;
		}
	}

	/*
	 * Send trailers. Wimp out and use writev(2).
	 */
	if (trl_uio != NULL) {
		SOCK_IO_SEND_UNLOCK(so);
		CURVNET_RESTORE();
		error = kern_writev(td, sockfd, trl_uio);
		if (error == 0)
			sbytes += td->td_retval[0];
		goto out;
	}

done:
	SOCK_IO_SEND_UNLOCK(so);
	CURVNET_RESTORE();
out:
	/*
	 * If there was no error we have to clear td->td_retval[0]
	 * because it may have been set by writev.
	 */
	if (error == 0) {
		td->td_retval[0] = 0;
		if (sbytes > 0 && vp != NULL)
			INOTIFY(vp, IN_ACCESS);
	}
	if (sent != NULL) {
		(*sent) = sbytes;
	}
	if (obj != NULL)
		vm_object_deallocate(obj);
	if (so)
		fdrop(sock_fp, td);
	if (m)
		m_freem(m);
	if (mh)
		m_freem(mh);

	if (sfs != NULL) {
		mtx_lock(&sfs->mtx);
		if (sfs->count != 0)
			error = cv_wait_sig(&sfs->cv, &sfs->mtx);
		if (sfs->count == 0) {
			sendfile_sync_destroy(sfs);
		} else {
			sfs->waiting = false;
			mtx_unlock(&sfs->mtx);
		}
	}
#ifdef KERN_TLS
	if (tls != NULL)
		ktls_free(tls);
#endif
	if (error == ERESTART)
		error = EINTR;

	return (error);
}

static int
sendfile(struct thread *td, struct sendfile_args *uap, int compat)
{
	struct sf_hdtr hdtr;
	struct uio *hdr_uio, *trl_uio;
	struct file *fp;
	off_t sbytes;
	int error;

	/*
	 * File offset must be positive.  If it goes beyond EOF
	 * we send only the header/trailer and no payload data.
	 */
	if (uap->offset < 0)
		return (EINVAL);

	sbytes = 0;
	hdr_uio = trl_uio = NULL;

	if (uap->hdtr != NULL) {
		error = copyin(uap->hdtr, &hdtr, sizeof(hdtr));
		if (error != 0)
			goto out;
		if (hdtr.headers != NULL) {
			error = copyinuio(hdtr.headers, hdtr.hdr_cnt,
			    &hdr_uio);
			if (error != 0)
				goto out;
#ifdef COMPAT_FREEBSD4
			/*
			 * In FreeBSD < 5.0 the nbytes to send also included
			 * the header.  If compat is specified subtract the
			 * header size from nbytes.
			 */
			if (compat) {
				if (uap->nbytes > hdr_uio->uio_resid)
					uap->nbytes -= hdr_uio->uio_resid;
				else
					uap->nbytes = 0;
			}
#endif
		}
		if (hdtr.trailers != NULL) {
			error = copyinuio(hdtr.trailers, hdtr.trl_cnt,
			    &trl_uio);
			if (error != 0)
				goto out;
		}
	}

	AUDIT_ARG_FD(uap->fd);

	/*
	 * sendfile(2) can start at any offset within a file so we require
	 * CAP_READ+CAP_SEEK = CAP_PREAD.
	 */
	if ((error = fget_read(td, uap->fd, &cap_pread_rights, &fp)) != 0)
		goto out;

	error = fo_sendfile(fp, uap->s, hdr_uio, trl_uio, uap->offset,
	    uap->nbytes, &sbytes, uap->flags, td);
	fdrop(fp, td);

	if (uap->sbytes != NULL)
		(void)copyout(&sbytes, uap->sbytes, sizeof(off_t));

out:
	freeuio(hdr_uio);
	freeuio(trl_uio);
	return (error);
}

/*
 * sendfile(2)
 * 
 * int sendfile(int fd, int s, off_t offset, size_t nbytes,
 *       struct sf_hdtr *hdtr, off_t *sbytes, int flags)
 * 
 * Send a file specified by 'fd' and starting at 'offset' to a socket
 * specified by 's'. Send only 'nbytes' of the file or until EOF if nbytes ==
 * 0.  Optionally add a header and/or trailer to the socket output.  If
 * specified, write the total number of bytes sent into *sbytes.
 */
int
sys_sendfile(struct thread *td, struct sendfile_args *uap)
{

	return (sendfile(td, uap, 0));
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sendfile(struct thread *td, struct freebsd4_sendfile_args *uap)
{
	struct sendfile_args args;

	args.fd = uap->fd;
	args.s = uap->s;
	args.offset = uap->offset;
	args.nbytes = uap->nbytes;
	args.hdtr = uap->hdtr;
	args.sbytes = uap->sbytes;
	args.flags = uap->flags;

	return (sendfile(td, &args, 1));
}
#endif /* COMPAT_FREEBSD4 */
