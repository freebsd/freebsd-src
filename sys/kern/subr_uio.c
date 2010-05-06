/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)kern_subr.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_zero.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#ifdef ZERO_COPY_SOCKETS
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#endif

SYSCTL_INT(_kern, KERN_IOV_MAX, iov_max, CTLFLAG_RD, NULL, UIO_MAXIOV,
	"Maximum number of elements in an I/O vector; sysconf(_SC_IOV_MAX)");

#ifdef ZERO_COPY_SOCKETS
/* Declared in uipc_socket.c */
extern int so_zero_copy_receive;

/*
 * Identify the physical page mapped at the given kernel virtual
 * address.  Insert this physical page into the given address space at
 * the given virtual address, replacing the physical page, if any,
 * that already exists there.
 */
static int
vm_pgmoveco(vm_map_t mapa, vm_offset_t kaddr, vm_offset_t uaddr)
{
	vm_map_t map = mapa;
	vm_page_t kern_pg, user_pg;
	vm_object_t uobject;
	vm_map_entry_t entry;
	vm_pindex_t upindex;
	vm_prot_t prot;
	boolean_t wired;

	KASSERT((uaddr & PAGE_MASK) == 0,
	    ("vm_pgmoveco: uaddr is not page aligned"));

	/*
	 * Herein the physical page is validated and dirtied.  It is
	 * unwired in sf_buf_mext().
	 */
	kern_pg = PHYS_TO_VM_PAGE(vtophys(kaddr));
	kern_pg->valid = VM_PAGE_BITS_ALL;
	KASSERT(kern_pg->queue == PQ_NONE && kern_pg->wire_count == 1,
	    ("vm_pgmoveco: kern_pg is not correctly wired"));

	if ((vm_map_lookup(&map, uaddr,
			   VM_PROT_WRITE, &entry, &uobject,
			   &upindex, &prot, &wired)) != KERN_SUCCESS) {
		return(EFAULT);
	}
	VM_OBJECT_LOCK(uobject);
retry:
	if ((user_pg = vm_page_lookup(uobject, upindex)) != NULL) {
		if (vm_page_sleep_if_busy(user_pg, TRUE, "vm_pgmoveco"))
			goto retry;
		vm_page_lock(user_pg);
		vm_page_lock_queues();
		pmap_remove_all(user_pg);
		vm_page_free(user_pg);
		vm_page_unlock(user_pg);
	} else {
		/*
		 * Even if a physical page does not exist in the
		 * object chain's first object, a physical page from a
		 * backing object may be mapped read only.
		 */
		if (uobject->backing_object != NULL)
			pmap_remove(map->pmap, uaddr, uaddr + PAGE_SIZE);
		vm_page_lock_queues();
	}
	vm_page_insert(kern_pg, uobject, upindex);
	vm_page_dirty(kern_pg);
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(uobject);
	vm_map_lookup_done(map, entry);
	return(KERN_SUCCESS);
}
#endif /* ZERO_COPY_SOCKETS */

int
uiomove(void *cp, int n, struct uio *uio)
{
	struct thread *td = curthread;
	struct iovec *iov;
	u_int cnt;
	int error = 0;
	int save = 0;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomove proc"));
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "Calling uiomove()");

	save = td->td_pflags & TDP_DEADLKTREAT;
	td->td_pflags |= TDP_DEADLKTREAT;

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;

		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
			if (ticks - PCPU_GET(switchticks) >= hogticks)
				uio_yield();
			if (uio->uio_rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				goto out;
			break;

		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		case UIO_NOCOPY:
			break;
		}
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (char *)cp + cnt;
		n -= cnt;
	}
out:
	if (save == 0)
		td->td_pflags &= ~TDP_DEADLKTREAT;
	return (error);
}

/*
 * Wrapper for uiomove() that validates the arguments against a known-good
 * kernel buffer.  Currently, uiomove accepts a signed (n) argument, which
 * is almost definitely a bad thing, so we catch that here as well.  We
 * return a runtime failure, but it might be desirable to generate a runtime
 * assertion failure instead.
 */
int
uiomove_frombuf(void *buf, int buflen, struct uio *uio)
{
	unsigned int offset, n;

	if (uio->uio_offset < 0 || uio->uio_resid < 0 ||
	    (offset = uio->uio_offset) != uio->uio_offset)
		return (EINVAL);
	if (buflen <= 0 || offset >= buflen)
		return (0);
	if ((n = buflen - offset) > INT_MAX)
		return (EINVAL);
	return (uiomove((char *)buf + offset, n, uio));
}

#ifdef ZERO_COPY_SOCKETS
/*
 * Experimental support for zero-copy I/O
 */
static int
userspaceco(void *cp, u_int cnt, struct uio *uio, int disposable)
{
	struct iovec *iov;
	int error;

	iov = uio->uio_iov;
	if (uio->uio_rw == UIO_READ) {
		if ((so_zero_copy_receive != 0)
		 && ((cnt & PAGE_MASK) == 0)
		 && ((((intptr_t) iov->iov_base) & PAGE_MASK) == 0)
		 && ((uio->uio_offset & PAGE_MASK) == 0)
		 && ((((intptr_t) cp) & PAGE_MASK) == 0)
		 && (disposable != 0)) {
			/* SOCKET: use page-trading */
			/*
			 * We only want to call vm_pgmoveco() on
			 * disposeable pages, since it gives the
			 * kernel page to the userland process.
			 */
			error =	vm_pgmoveco(&curproc->p_vmspace->vm_map,
			    (vm_offset_t)cp, (vm_offset_t)iov->iov_base);

			/*
			 * If we get an error back, attempt
			 * to use copyout() instead.  The
			 * disposable page should be freed
			 * automatically if we weren't able to move
			 * it into userland.
			 */
			if (error != 0)
				error = copyout(cp, iov->iov_base, cnt);
		} else {
			error = copyout(cp, iov->iov_base, cnt);
		}
	} else {
		error = copyin(iov->iov_base, cp, cnt);
	}
	return (error);
}

int
uiomoveco(void *cp, int n, struct uio *uio, int disposable)
{
	struct iovec *iov;
	u_int cnt;
	int error;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomoveco: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomoveco proc"));

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = n;

		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
			if (ticks - PCPU_GET(switchticks) >= hogticks)
				uio_yield();

			error = userspaceco(cp, cnt, uio, disposable);

			if (error)
				return (error);
			break;

		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		case UIO_NOCOPY:
			break;
		}
		iov->iov_base = (char *)iov->iov_base + cnt;
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (char *)cp + cnt;
		n -= cnt;
	}
	return (0);
}
#endif /* ZERO_COPY_SOCKETS */

/*
 * Give next character to user as result of read.
 */
int
ureadc(int c, struct uio *uio)
{
	struct iovec *iov;
	char *iov_base;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "Calling ureadc()");

again:
	if (uio->uio_iovcnt == 0 || uio->uio_resid == 0)
		panic("ureadc");
	iov = uio->uio_iov;
	if (iov->iov_len == 0) {
		uio->uio_iovcnt--;
		uio->uio_iov++;
		goto again;
	}
	switch (uio->uio_segflg) {

	case UIO_USERSPACE:
		if (subyte(iov->iov_base, c) < 0)
			return (EFAULT);
		break;

	case UIO_SYSSPACE:
		iov_base = iov->iov_base;
		*iov_base = c;
		iov->iov_base = iov_base;
		break;

	case UIO_NOCOPY:
		break;
	}
	iov->iov_base = (char *)iov->iov_base + 1;
	iov->iov_len--;
	uio->uio_resid--;
	uio->uio_offset++;
	return (0);
}

void
uio_yield(void)
{
	struct thread *td;

	td = curthread;
	DROP_GIANT();
	thread_lock(td);
	sched_prio(td, td->td_user_pri);
	mi_switch(SW_INVOL | SWT_RELINQUISH, NULL);
	thread_unlock(td);
	PICKUP_GIANT();
}

int
copyinfrom(const void * __restrict src, void * __restrict dst, size_t len,
    int seg)
{
	int error = 0;

	switch (seg) {
	case UIO_USERSPACE:
		error = copyin(src, dst, len);
		break;
	case UIO_SYSSPACE:
		bcopy(src, dst, len);
		break;
	default:
		panic("copyinfrom: bad seg %d\n", seg);
	}
	return (error);
}

int
copyinstrfrom(const void * __restrict src, void * __restrict dst, size_t len,
    size_t * __restrict copied, int seg)
{
	int error = 0;

	switch (seg) {
	case UIO_USERSPACE:
		error = copyinstr(src, dst, len, copied);
		break;
	case UIO_SYSSPACE:
		error = copystr(src, dst, len, copied);
		break;
	default:
		panic("copyinstrfrom: bad seg %d\n", seg);
	}
	return (error);
}

int
copyiniov(struct iovec *iovp, u_int iovcnt, struct iovec **iov, int error)
{
	u_int iovlen;

	*iov = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (error);
	iovlen = iovcnt * sizeof (struct iovec);
	*iov = malloc(iovlen, M_IOV, M_WAITOK);
	error = copyin(iovp, *iov, iovlen);
	if (error) {
		free(*iov, M_IOV);
		*iov = NULL;
	}
	return (error);
}

int
copyinuio(struct iovec *iovp, u_int iovcnt, struct uio **uiop)
{
	struct iovec *iov;
	struct uio *uio;
	u_int iovlen;
	int error, i;

	*uiop = NULL;
	if (iovcnt > UIO_MAXIOV)
		return (EINVAL);
	iovlen = iovcnt * sizeof (struct iovec);
	uio = malloc(iovlen + sizeof *uio, M_IOV, M_WAITOK);
	iov = (struct iovec *)(uio + 1);
	error = copyin(iovp, iov, iovlen);
	if (error) {
		free(uio, M_IOV);
		return (error);
	}
	uio->uio_iov = iov;
	uio->uio_iovcnt = iovcnt;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_offset = -1;
	uio->uio_resid = 0;
	for (i = 0; i < iovcnt; i++) {
		if (iov->iov_len > INT_MAX - uio->uio_resid) {
			free(uio, M_IOV);
			return (EINVAL);
		}
		uio->uio_resid += iov->iov_len;
		iov++;
	}
	*uiop = uio;
	return (0);
}

struct uio *
cloneuio(struct uio *uiop)
{
	struct uio *uio;
	int iovlen;

	iovlen = uiop->uio_iovcnt * sizeof (struct iovec);
	uio = malloc(iovlen + sizeof *uio, M_IOV, M_WAITOK);
	*uio = *uiop;
	uio->uio_iov = (struct iovec *)(uio + 1);
	bcopy(uiop->uio_iov, uio->uio_iov, iovlen);
	return (uio);
}
