/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD$
 */

#include "opt_zero.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#ifdef ZERO_COPY_SOCKETS
#include <vm/vm_param.h>
#endif
#if defined(ZERO_COPY_SOCKETS) || defined(ENABLE_VFS_IOOPT)
#include <vm/vm_object.h>
#endif

SYSCTL_INT(_kern, KERN_IOV_MAX, iov_max, CTLFLAG_RD, NULL, UIO_MAXIOV, 
	"Maximum number of elements in an I/O vector; sysconf(_SC_IOV_MAX)");

#if defined(ZERO_COPY_SOCKETS) || defined(ENABLE_VFS_IOOPT)
static int userspaceco(caddr_t cp, u_int cnt, struct uio *uio,
		       struct vm_object *obj, int disposable);
#endif

#ifdef ZERO_COPY_SOCKETS
/* Declared in uipc_socket.c */
extern int so_zero_copy_receive;

static int vm_pgmoveco(vm_map_t mapa, vm_object_t srcobj, vm_offset_t kaddr,
		       vm_offset_t uaddr);

static int
vm_pgmoveco(mapa, srcobj,  kaddr, uaddr)
        vm_map_t mapa;
	vm_object_t srcobj;
	vm_offset_t kaddr, uaddr;
{
	vm_map_t map = mapa;
	vm_page_t kern_pg, user_pg;
	vm_object_t uobject;
	vm_map_entry_t entry;
	vm_pindex_t upindex, kpindex;
	vm_prot_t prot;
	boolean_t wired;

	/*
	 * First lookup the kernel page.
	 */
	kern_pg = PHYS_TO_VM_PAGE(vtophys(kaddr));

	if ((vm_map_lookup(&map, uaddr,
			   VM_PROT_READ, &entry, &uobject,
			   &upindex, &prot, &wired)) != KERN_SUCCESS) {
		return(EFAULT);
	}
	if ((user_pg = vm_page_lookup(uobject, upindex)) != NULL) {
		do
			vm_page_lock_queues();
		while (vm_page_sleep_if_busy(user_pg, 1, "vm_pgmoveco"));
		vm_page_busy(user_pg);
		pmap_remove_all(user_pg);
		vm_page_free(user_pg);
	} else
		vm_page_lock_queues();
	if (kern_pg->busy || ((kern_pg->queue - kern_pg->pc) == PQ_FREE) ||
	    (kern_pg->hold_count != 0)|| (kern_pg->flags & PG_BUSY)) {
		printf("vm_pgmoveco: pindex(%lu), busy(%d), PG_BUSY(%d), "
		       "hold(%d) paddr(0x%lx)\n", (u_long)kern_pg->pindex,
			kern_pg->busy, (kern_pg->flags & PG_BUSY) ? 1 : 0,
			kern_pg->hold_count, (u_long)kern_pg->phys_addr);
		if ((kern_pg->queue - kern_pg->pc) == PQ_FREE)
			panic("vm_pgmoveco: renaming free page");
		else
			panic("vm_pgmoveco: renaming busy page");
	}
	kpindex = kern_pg->pindex;
	vm_page_busy(kern_pg);
	vm_page_unlock_queues();
	vm_page_rename(kern_pg, uobject, upindex);
	vm_page_lock_queues();
	vm_page_flag_clear(kern_pg, PG_BUSY);
	kern_pg->valid = VM_PAGE_BITS_ALL;
	vm_page_unlock_queues();
	
	vm_map_lookup_done(map, entry);
	return(KERN_SUCCESS);
}
#endif /* ZERO_COPY_SOCKETS */

int
uiomove(cp, n, uio)
	register caddr_t cp;
	register int n;
	register struct uio *uio;
{
	struct thread *td = curthread;
	register struct iovec *iov;
	u_int cnt;
	int error = 0;
	int save = 0;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == curthread,
	    ("uiomove proc"));

	if (td) {
		mtx_lock_spin(&sched_lock);
		save = td->td_flags & TDF_DEADLKTREAT;
		td->td_flags |= TDF_DEADLKTREAT;
		mtx_unlock_spin(&sched_lock);
	}

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
		cp += cnt;
		n -= cnt;
	}
out:
	if (td != curthread) printf("uiomove: IT CHANGED!");
	td = curthread;	/* Might things have changed in copyin/copyout? */
	if (td) {
		mtx_lock_spin(&sched_lock);
		td->td_flags = (td->td_flags & ~TDF_DEADLKTREAT) | save;
		mtx_unlock_spin(&sched_lock);
	}
	return (error);
}

#if defined(ENABLE_VFS_IOOPT) || defined(ZERO_COPY_SOCKETS)
/*
 * Experimental support for zero-copy I/O
 */
static int
userspaceco(cp, cnt, uio, obj, disposable)
	caddr_t cp;
	u_int cnt;
	struct uio *uio;
	struct vm_object *obj;
	int disposable;
{
	struct iovec *iov;
	int error;

	iov = uio->uio_iov;

#ifdef ZERO_COPY_SOCKETS

	if (uio->uio_rw == UIO_READ) {
		if ((so_zero_copy_receive != 0)
		 && (obj != NULL)
		 && ((cnt & PAGE_MASK) == 0)
		 && ((((intptr_t) iov->iov_base) & PAGE_MASK) == 0)
		 && ((uio->uio_offset & PAGE_MASK) == 0)
		 && ((((intptr_t) cp) & PAGE_MASK) == 0)
		 && (obj->type == OBJT_DEFAULT)
		 && (disposable != 0)) {
			/* SOCKET: use page-trading */
			/*
			 * We only want to call vm_pgmoveco() on
			 * disposeable pages, since it gives the
			 * kernel page to the userland process.
			 */
			error =	vm_pgmoveco(&curproc->p_vmspace->vm_map,
					    obj, (vm_offset_t)cp, 
					    (vm_offset_t)iov->iov_base);

			/*
			 * If we get an error back, attempt
			 * to use copyout() instead.  The
			 * disposable page should be freed
			 * automatically if we weren't able to move
			 * it into userland.
			 */
			if (error != 0)
				error = copyout(cp, iov->iov_base, cnt);
#ifdef ENABLE_VFS_IOOPT
		} else if ((vfs_ioopt != 0)
		 && ((cnt & PAGE_MASK) == 0)
		 && ((((intptr_t) iov->iov_base) & PAGE_MASK) == 0)
		 && ((uio->uio_offset & PAGE_MASK) == 0)
		 && ((((intptr_t) cp) & PAGE_MASK) == 0)) {
			error = vm_uiomove(&curproc->p_vmspace->vm_map, obj,
					   uio->uio_offset, cnt,
					   (vm_offset_t) iov->iov_base, NULL);
#endif /* ENABLE_VFS_IOOPT */
		} else {
			error = copyout(cp, iov->iov_base, cnt);
		}
	} else {
		error = copyin(iov->iov_base, cp, cnt);
	}
#else /* ZERO_COPY_SOCKETS */
	if (uio->uio_rw == UIO_READ) {
#ifdef ENABLE_VFS_IOOPT
		if ((vfs_ioopt != 0)
		 && ((cnt & PAGE_MASK) == 0)
		 && ((((intptr_t) iov->iov_base) & PAGE_MASK) == 0)
		 && ((uio->uio_offset & PAGE_MASK) == 0)
		 && ((((intptr_t) cp) & PAGE_MASK) == 0)) {
			error = vm_uiomove(&curproc->p_vmspace->vm_map, obj,
					   uio->uio_offset, cnt,
					   (vm_offset_t) iov->iov_base, NULL);
		} else
#endif /* ENABLE_VFS_IOOPT */
		{
			error = copyout(cp, iov->iov_base, cnt);
		}
	} else {
		error = copyin(iov->iov_base, cp, cnt);
	}
#endif /* ZERO_COPY_SOCKETS */

	return (error);
}

int
uiomoveco(cp, n, uio, obj, disposable)
	caddr_t cp;
	int n;
	struct uio *uio;
	struct vm_object *obj;
	int disposable;
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

			error = userspaceco(cp, cnt, uio, obj, disposable);

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
		cp += cnt;
		n -= cnt;
	}
	return (0);
}
#endif /* ENABLE_VFS_IOOPT || ZERO_COPY_SOCKETS */

#ifdef ENABLE_VFS_IOOPT

/*
 * Experimental support for zero-copy I/O
 */
int
uioread(n, uio, obj, nread)
	int n;
	struct uio *uio;
	struct vm_object *obj;
	int *nread;
{
	int npagesmoved;
	struct iovec *iov;
	u_int cnt, tcnt;
	int error;

	*nread = 0;
	if (vfs_ioopt < 2)
		return 0;

	error = 0;

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

		if ((uio->uio_segflg == UIO_USERSPACE) &&
			((((intptr_t) iov->iov_base) & PAGE_MASK) == 0) &&
				 ((uio->uio_offset & PAGE_MASK) == 0) ) {

			if (cnt < PAGE_SIZE)
				break;

			cnt &= ~PAGE_MASK;

			if (ticks - PCPU_GET(switchticks) >= hogticks)
				uio_yield();
			error = vm_uiomove(&curproc->p_vmspace->vm_map, obj,
						uio->uio_offset, cnt,
						(vm_offset_t) iov->iov_base, &npagesmoved);

			if (npagesmoved == 0)
				break;

			tcnt = npagesmoved * PAGE_SIZE;
			cnt = tcnt;

			if (error)
				break;

			iov->iov_base = (char *)iov->iov_base + cnt;
			iov->iov_len -= cnt;
			uio->uio_resid -= cnt;
			uio->uio_offset += cnt;
			*nread += cnt;
			n -= cnt;
		} else {
			break;
		}
	}
	return error;
}
#endif /* ENABLE_VFS_IOOPT */

/*
 * Give next character to user as result of read.
 */
int
ureadc(c, uio)
	register int c;
	register struct uio *uio;
{
	register struct iovec *iov;
	register char *iov_base;

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

/*
 * General routine to allocate a hash table.
 */
void *
hashinit(elements, type, hashmask)
	int elements;
	struct malloc_type *type;
	u_long *hashmask;
{
	long hashsize;
	LIST_HEAD(generic, generic) *hashtbl;
	int i;

	if (elements <= 0)
		panic("hashinit: bad elements");
	for (hashsize = 1; hashsize <= elements; hashsize <<= 1)
		continue;
	hashsize >>= 1;
	hashtbl = malloc((u_long)hashsize * sizeof(*hashtbl), type, M_WAITOK);
	for (i = 0; i < hashsize; i++)
		LIST_INIT(&hashtbl[i]);
	*hashmask = hashsize - 1;
	return (hashtbl);
}

void
hashdestroy(vhashtbl, type, hashmask)
	void *vhashtbl;
	struct malloc_type *type;
	u_long hashmask;
{
	LIST_HEAD(generic, generic) *hashtbl, *hp;

	hashtbl = vhashtbl;
	for (hp = hashtbl; hp <= &hashtbl[hashmask]; hp++)
		if (!LIST_EMPTY(hp))
			panic("hashdestroy: hash not empty");
	free(hashtbl, type);
}

static int primes[] = { 1, 13, 31, 61, 127, 251, 509, 761, 1021, 1531, 2039,
			2557, 3067, 3583, 4093, 4603, 5119, 5623, 6143, 6653,
			7159, 7673, 8191, 12281, 16381, 24571, 32749 };
#define NPRIMES (sizeof(primes) / sizeof(primes[0]))

/*
 * General routine to allocate a prime number sized hash table.
 */
void *
phashinit(elements, type, nentries)
	int elements;
	struct malloc_type *type;
	u_long *nentries;
{
	long hashsize;
	LIST_HEAD(generic, generic) *hashtbl;
	int i;

	if (elements <= 0)
		panic("phashinit: bad elements");
	for (i = 1, hashsize = primes[1]; hashsize <= elements;) {
		i++;
		if (i == NPRIMES)
			break;
		hashsize = primes[i];
	}
	hashsize = primes[i - 1];
	hashtbl = malloc((u_long)hashsize * sizeof(*hashtbl), type, M_WAITOK);
	for (i = 0; i < hashsize; i++)
		LIST_INIT(&hashtbl[i]);
	*nentries = hashsize;
	return (hashtbl);
}

void
uio_yield()
{
	struct thread *td;

	td = curthread;
	mtx_lock_spin(&sched_lock);
	DROP_GIANT();
	sched_prio(td, td->td_ksegrp->kg_user_pri); /* XXXKSE */
	td->td_proc->p_stats->p_ru.ru_nivcsw++;
	mi_switch();
	mtx_unlock_spin(&sched_lock);
	PICKUP_GIANT();
}

int
copyinfrom(const void *src, void *dst, size_t len, int seg)
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
copyinstrfrom(const void *src, void *dst, size_t len, size_t *copied, int seg)
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
