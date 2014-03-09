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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_map.h>

SYSCTL_INT(_kern, KERN_IOV_MAX, iov_max, CTLFLAG_RD, NULL, UIO_MAXIOV,
	"Maximum number of elements in an I/O vector; sysconf(_SC_IOV_MAX)");

static int uiomove_faultflag(void *cp, int n, struct uio *uio, int nofault);

int
copyin_nofault(const void *udaddr, void *kaddr, size_t len)
{
	int error, save;

	save = vm_fault_disable_pagefaults();
	error = copyin(udaddr, kaddr, len);
	vm_fault_enable_pagefaults(save);
	return (error);
}

int
copyout_nofault(const void *kaddr, void *udaddr, size_t len)
{
	int error, save;

	save = vm_fault_disable_pagefaults();
	error = copyout(kaddr, udaddr, len);
	vm_fault_enable_pagefaults(save);
	return (error);
}

#define	PHYS_PAGE_COUNT(len)	(howmany(len, PAGE_SIZE) + 1)

int
physcopyin(void *src, vm_paddr_t dst, size_t len)
{
	vm_page_t m[PHYS_PAGE_COUNT(len)];
	struct iovec iov[1];
	struct uio uio;
	int i;

	iov[0].iov_base = src;
	iov[0].iov_len = len;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = len;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	for (i = 0; i < PHYS_PAGE_COUNT(len); i++, dst += PAGE_SIZE)
		m[i] = PHYS_TO_VM_PAGE(dst);
	return (uiomove_fromphys(m, dst & PAGE_MASK, len, &uio));
}

int
physcopyout(vm_paddr_t src, void *dst, size_t len)
{
	vm_page_t m[PHYS_PAGE_COUNT(len)];
	struct iovec iov[1];
	struct uio uio;
	int i;

	iov[0].iov_base = dst;
	iov[0].iov_len = len;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = len;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	for (i = 0; i < PHYS_PAGE_COUNT(len); i++, src += PAGE_SIZE)
		m[i] = PHYS_TO_VM_PAGE(src);
	return (uiomove_fromphys(m, src & PAGE_MASK, len, &uio));
}

#undef PHYS_PAGE_COUNT

int
uiomove(void *cp, int n, struct uio *uio)
{

	return (uiomove_faultflag(cp, n, uio, 0));
}

int
uiomove_nofault(void *cp, int n, struct uio *uio)
{

	return (uiomove_faultflag(cp, n, uio, 1));
}

static int
uiomove_faultflag(void *cp, int n, struct uio *uio, int nofault)
{
	struct thread *td;
	struct iovec *iov;
	size_t cnt;
	int error, newflags, save;

	td = curthread;
	error = 0;

	KASSERT(uio->uio_rw == UIO_READ || uio->uio_rw == UIO_WRITE,
	    ("uiomove: mode"));
	KASSERT(uio->uio_segflg != UIO_USERSPACE || uio->uio_td == td,
	    ("uiomove proc"));
	if (!nofault)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "Calling uiomove()");

	/* XXX does it make a sense to set TDP_DEADLKTREAT for UIO_SYSSPACE ? */
	newflags = TDP_DEADLKTREAT;
	if (uio->uio_segflg == UIO_USERSPACE && nofault) {
		/*
		 * Fail if a non-spurious page fault occurs.
		 */
		newflags |= TDP_NOFAULTING | TDP_RESETSPUR;
	}
	save = curthread_pflags_set(newflags);

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
			maybe_yield();
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
	curthread_pflags_restore(save);
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
	size_t offset, n;

	if (uio->uio_offset < 0 || uio->uio_resid < 0 ||
	    (offset = uio->uio_offset) != uio->uio_offset)
		return (EINVAL);
	if (buflen <= 0 || offset >= buflen)
		return (0);
	if ((n = buflen - offset) > IOSIZE_MAX)
		return (EINVAL);
	return (uiomove((char *)buf + offset, n, uio));
}

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
copyiniov(const struct iovec *iovp, u_int iovcnt, struct iovec **iov, int error)
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
copyinuio(const struct iovec *iovp, u_int iovcnt, struct uio **uiop)
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
		if (iov->iov_len > IOSIZE_MAX - uio->uio_resid) {
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

/*
 * Map some anonymous memory in user space of size sz, rounded up to the page
 * boundary.
 */
int
copyout_map(struct thread *td, vm_offset_t *addr, size_t sz)
{
	struct vmspace *vms;
	int error;
	vm_size_t size;

	vms = td->td_proc->p_vmspace;

	/*
	 * Map somewhere after heap in process memory.
	 */
	PROC_LOCK(td->td_proc);
	*addr = round_page((vm_offset_t)vms->vm_daddr +
	    lim_max(td->td_proc, RLIMIT_DATA));
	PROC_UNLOCK(td->td_proc);

	/* round size up to page boundry */
	size = (vm_size_t)round_page(sz);

	error = vm_mmap(&vms->vm_map, addr, size, PROT_READ | PROT_WRITE,
	    VM_PROT_ALL, MAP_PRIVATE | MAP_ANON, OBJT_DEFAULT, NULL, 0);

	return (error);
}

/*
 * Unmap memory in user space.
 */
int
copyout_unmap(struct thread *td, vm_offset_t addr, size_t sz)
{
	vm_map_t map;
	vm_size_t size;

	if (sz == 0)
		return (0);

	map = &td->td_proc->p_vmspace->vm_map;
	size = (vm_size_t)round_page(sz);

	if (vm_map_remove(map, addr, addr + size) != KERN_SUCCESS)
		return (EINVAL);

	return (0);
}
